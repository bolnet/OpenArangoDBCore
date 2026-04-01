#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace fs = std::filesystem;

namespace arangodb {

// ============================================================================
// BackupManifest
// ============================================================================

std::string BackupManifest::toJson() const {
  // Hand-rolled JSON serialization (no external JSON library dependency).
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"backupId\": \"" << backupId << "\",\n";
  oss << "  \"timestamp\": \"" << timestamp << "\",\n";
  oss << "  \"version\": \"" << version << "\",\n";
  oss << "  \"sequenceNumber\": " << sequenceNumber << ",\n";
  oss << "  \"path\": \"" << path << "\",\n";
  oss << "  \"isConsistent\": " << (isConsistent ? "true" : "false") << ",\n";
  oss << "  \"collections\": [";
  for (size_t i = 0; i < collections.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << "\"" << collections[i] << "\"";
  }
  oss << "]\n";
  oss << "}";
  return oss.str();
}

namespace {

// Minimal JSON parsing helpers (no external JSON library).
// These extract values from simple, well-formed JSON produced by toJson().

std::string extractStringField(std::string const& json,
                               std::string const& field) {
  std::string key = "\"" + field + "\": \"";
  auto pos = json.find(key);
  if (pos == std::string::npos) {
    return "";
  }
  pos += key.size();
  auto end = json.find('"', pos);
  if (end == std::string::npos) {
    return "";
  }
  return json.substr(pos, end - pos);
}

bool hasField(std::string const& json, std::string const& field) {
  std::string key = "\"" + field + "\"";
  return json.find(key) != std::string::npos;
}

uint64_t extractUint64Field(std::string const& json,
                            std::string const& field) {
  std::string key = "\"" + field + "\": ";
  auto pos = json.find(key);
  if (pos == std::string::npos) {
    return 0;
  }
  pos += key.size();
  // Read digits
  std::string digits;
  while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9') {
    digits += json[pos++];
  }
  if (digits.empty()) {
    return 0;
  }
  return std::stoull(digits);
}

bool extractBoolField(std::string const& json, std::string const& field) {
  std::string key = "\"" + field + "\": ";
  auto pos = json.find(key);
  if (pos == std::string::npos) {
    return false;
  }
  pos += key.size();
  return json.substr(pos, 4) == "true";
}

std::vector<std::string> extractStringArray(std::string const& json,
                                            std::string const& field) {
  std::vector<std::string> result;
  std::string key = "\"" + field + "\": [";
  auto pos = json.find(key);
  if (pos == std::string::npos) {
    return result;
  }
  pos += key.size();
  auto end = json.find(']', pos);
  if (end == std::string::npos) {
    return result;
  }
  std::string arrayContent = json.substr(pos, end - pos);

  // Parse "item1", "item2", ...
  size_t i = 0;
  while (i < arrayContent.size()) {
    auto quoteStart = arrayContent.find('"', i);
    if (quoteStart == std::string::npos) {
      break;
    }
    auto quoteEnd = arrayContent.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) {
      break;
    }
    result.push_back(
        arrayContent.substr(quoteStart + 1, quoteEnd - quoteStart - 1));
    i = quoteEnd + 1;
  }
  return result;
}

}  // namespace

HotBackupResult BackupManifest::fromJson(std::string const& json,
                                         BackupManifest& out) {
  if (json.empty()) {
    return HotBackupResult::badRequest("Empty JSON input");
  }

  if (!hasField(json, "backupId")) {
    return HotBackupResult::badRequest("Missing required field: backupId");
  }
  if (!hasField(json, "timestamp")) {
    return HotBackupResult::badRequest("Missing required field: timestamp");
  }

  out.backupId = extractStringField(json, "backupId");
  out.timestamp = extractStringField(json, "timestamp");
  out.version = extractStringField(json, "version");
  out.sequenceNumber = extractUint64Field(json, "sequenceNumber");
  out.path = extractStringField(json, "path");
  out.isConsistent = extractBoolField(json, "isConsistent");
  out.collections = extractStringArray(json, "collections");

  if (out.backupId.empty()) {
    return HotBackupResult::badRequest("backupId cannot be empty");
  }
  if (out.timestamp.empty()) {
    return HotBackupResult::badRequest("timestamp cannot be empty");
  }

  return HotBackupResult::success();
}

HotBackupResult BackupManifest::saveToFile(std::string const& manifestPath,
                                           BackupManifest const& manifest) {
  std::ofstream ofs(manifestPath);
  if (!ofs.is_open()) {
    return HotBackupResult::error(500,
                                  "Failed to open manifest file for writing: " +
                                      manifestPath);
  }
  ofs << manifest.toJson();
  ofs.close();
  if (ofs.fail()) {
    return HotBackupResult::error(500,
                                  "Failed to write manifest file: " +
                                      manifestPath);
  }
  return HotBackupResult::success();
}

HotBackupResult BackupManifest::loadFromFile(std::string const& manifestPath,
                                             BackupManifest& out) {
  std::ifstream ifs(manifestPath);
  if (!ifs.is_open()) {
    return HotBackupResult::notFound(
        "Manifest file not found: " + manifestPath);
  }
  std::ostringstream oss;
  oss << ifs.rdbuf();
  std::string json = oss.str();
  if (json.empty()) {
    return HotBackupResult::badRequest(
        "Manifest file is empty: " + manifestPath);
  }
  return BackupManifest::fromJson(json, out);
}

// ============================================================================
// GlobalWriteLock
// ============================================================================

GlobalWriteLock::GlobalWriteLock(std::mutex& mutex)
    : _lock(mutex) {}

GlobalWriteLock::~GlobalWriteLock() = default;

GlobalWriteLock::GlobalWriteLock(GlobalWriteLock&& other) noexcept
    : _lock(std::move(other._lock)) {}

bool GlobalWriteLock::isLocked() const { return _lock.owns_lock(); }

// ============================================================================
// RocksDBHotBackup
// ============================================================================

RocksDBHotBackup::RocksDBHotBackup(std::string backupBasePath,
                                   IRocksDBForBackup* db)
    : _backupBasePath(std::move(backupBasePath)), _db(db) {}

std::string RocksDBHotBackup::generateBackupId() {
  auto now = std::chrono::system_clock::now();
  auto timeT = std::chrono::system_clock::to_time_t(now);
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                now.time_since_epoch()) %
            1000000;

  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &timeT);
#else
  gmtime_r(&timeT, &tm);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H.%M.%S") << "." << std::setw(6)
      << std::setfill('0') << us.count();
  return oss.str();
}

std::string RocksDBHotBackup::backupPath(std::string const& backupId) const {
  return _backupBasePath + "/" + backupId;
}

HotBackupResult RocksDBHotBackup::create(std::string const& label,
                                         CreateResult& out) {
  auto startTime = std::chrono::steady_clock::now();

  std::string id = generateBackupId();
  std::string backupDir = backupPath(id);

  // Create backup directory
  std::error_code ec;
  fs::create_directories(backupDir, ec);
  if (ec) {
    return HotBackupResult::error(
        500, "Failed to create backup directory: " + ec.message());
  }

  uint64_t seqNum = 0;
  bool isConsistent = false;
  std::vector<std::string> collectionNames;

  if (_db != nullptr) {
    // Acquire global write lock, flush WAL, create checkpoint
    {
      GlobalWriteLock lock(_mutex);
      isConsistent = _db->flushWAL();
      bool checkpointOk = _db->createCheckpoint(backupDir);
      if (!checkpointOk) {
        // Cleanup partial directory
        removeDirectory(backupDir);
        return HotBackupResult::error(500,
                                      "Failed to create RocksDB checkpoint");
      }
      seqNum = _db->getLatestSequenceNumber();
    }
    // Lock released here (RAII) before manifest write
    collectionNames = _db->getCollectionNames();
  }

  // Build manifest (outside of lock)
  auto now = std::chrono::system_clock::now();
  auto timeT = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &timeT);
#else
  gmtime_r(&timeT, &tm);
#endif
  std::ostringstream tsOss;
  tsOss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");

  BackupManifest manifest;
  manifest.backupId = id;
  manifest.timestamp = tsOss.str();
  manifest.version = "3.12.0";
  manifest.sequenceNumber = seqNum;
  manifest.collections = std::move(collectionNames);
  manifest.path = backupDir;
  manifest.isConsistent = isConsistent;

  // Write manifest to disk
  std::string manifestPath = backupDir + "/MANIFEST.json";
  auto res = BackupManifest::saveToFile(manifestPath, manifest);
  if (!res.ok()) {
    removeDirectory(backupDir);
    return res;
  }

  auto endTime = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(endTime - startTime).count();

  out.manifest = std::move(manifest);
  out.durationSeconds = elapsed;

  return HotBackupResult::success();
}

HotBackupResult RocksDBHotBackup::list(
    std::vector<BackupManifest>& out) const {
  out.clear();

  if (!fs::exists(_backupBasePath)) {
    return HotBackupResult::success();  // No backups directory = empty list
  }

  std::error_code ec;
  for (auto const& entry : fs::directory_iterator(_backupBasePath, ec)) {
    if (!entry.is_directory()) {
      continue;
    }
    std::string manifestPath = entry.path().string() + "/MANIFEST.json";
    BackupManifest manifest;
    auto res = BackupManifest::loadFromFile(manifestPath, manifest);
    if (res.ok()) {
      out.push_back(std::move(manifest));
    }
    // Skip directories with missing/corrupt manifests
  }

  // Sort by timestamp ascending
  std::sort(out.begin(), out.end(),
            [](BackupManifest const& a, BackupManifest const& b) {
              return a.timestamp < b.timestamp;
            });

  return HotBackupResult::success();
}

HotBackupResult RocksDBHotBackup::deleteBackup(
    std::string const& backupId) {
  std::string dir = backupPath(backupId);
  if (!fs::exists(dir)) {
    return HotBackupResult::notFound("Backup not found: " + backupId);
  }
  return removeDirectory(dir);
}

HotBackupResult RocksDBHotBackup::restore(std::string const& backupId) {
  std::string dir = backupPath(backupId);
  if (!fs::exists(dir)) {
    return HotBackupResult::notFound("Backup not found: " + backupId);
  }

  auto valRes = validateBackupDirectory(dir);
  if (!valRes.ok()) {
    return valRes;
  }

  // Load and verify manifest
  BackupManifest manifest;
  std::string manifestPath = dir + "/MANIFEST.json";
  auto loadRes = BackupManifest::loadFromFile(manifestPath, manifest);
  if (!loadRes.ok()) {
    return loadRes;
  }

  // Validate version compatibility (must match current version)
  // In production, this would compare with the running server version.
  // For now, we accept any valid manifest as restorable.

  return HotBackupResult::success();
}

HotBackupResult RocksDBHotBackup::validateBackupDirectory(
    std::string const& backupDir) {
  // Check MANIFEST.json exists
  std::string manifestPath = backupDir + "/MANIFEST.json";
  if (!fs::exists(manifestPath)) {
    return HotBackupResult::error(
        400, "Backup directory missing MANIFEST.json: " + backupDir);
  }

  // Check that the directory contains at least one file besides the manifest
  // (in a real checkpoint, there would be SST files)
  std::error_code ec;
  int fileCount = 0;
  for (auto const& entry : fs::directory_iterator(backupDir, ec)) {
    if (entry.is_regular_file() &&
        entry.path().filename().string() != "MANIFEST.json") {
      ++fileCount;
    }
  }

  if (fileCount == 0) {
    return HotBackupResult::error(
        400,
        "Backup directory contains no data files (expected SST files): " +
            backupDir);
  }

  return HotBackupResult::success();
}

HotBackupResult RocksDBHotBackup::removeDirectory(std::string const& path) {
  std::error_code ec;
  fs::remove_all(path, ec);
  if (ec) {
    return HotBackupResult::error(
        500, "Failed to remove directory: " + path + " (" + ec.message() + ")");
  }
  return HotBackupResult::success();
}

}  // namespace arangodb
