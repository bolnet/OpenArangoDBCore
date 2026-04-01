#pragma once
#ifndef ARANGODB_ROCKSDB_HOT_BACKUP_H
#define ARANGODB_ROCKSDB_HOT_BACKUP_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {

/// Result type for hot backup operations.
/// Wraps success/failure with an optional error message.
struct HotBackupResult {
  bool ok() const { return _ok; }
  std::string const& errorMessage() const { return _message; }
  int errorNumber() const { return _errorNumber; }

  static HotBackupResult success() { return HotBackupResult{true, 0, ""}; }
  static HotBackupResult error(int code, std::string msg) {
    return HotBackupResult{false, code, std::move(msg)};
  }
  static HotBackupResult notFound(std::string msg) {
    return HotBackupResult{false, 404, std::move(msg)};
  }
  static HotBackupResult badRequest(std::string msg) {
    return HotBackupResult{false, 400, std::move(msg)};
  }

 private:
  bool _ok = true;
  int _errorNumber = 0;
  std::string _message;

  HotBackupResult(bool ok, int code, std::string msg)
      : _ok(ok), _errorNumber(code), _message(std::move(msg)) {}
};

/// Backup manifest: metadata for a point-in-time snapshot.
struct BackupManifest {
  std::string backupId;       // e.g. "2026-03-31T14.30.00.123456"
  std::string timestamp;      // ISO-8601 creation time
  std::string version;        // ArangoDB version string
  uint64_t sequenceNumber = 0;  // RocksDB sequence number at snapshot
  std::vector<std::string> collections;  // Collection names in backup
  std::string path;           // Absolute path to backup directory
  bool isConsistent = false;  // Whether WAL was flushed before checkpoint

  /// Serialize manifest to a JSON string.
  std::string toJson() const;

  /// Parse manifest from a JSON string. Returns error on invalid input.
  static HotBackupResult fromJson(std::string const& json, BackupManifest& out);

  /// Save manifest to a JSON file on disk.
  static HotBackupResult saveToFile(std::string const& manifestPath,
                                    BackupManifest const& manifest);

  /// Load manifest from a JSON file on disk.
  static HotBackupResult loadFromFile(std::string const& manifestPath,
                                      BackupManifest& out);
};

/// RAII wrapper for a global write lock (blocks writes during checkpoint).
class GlobalWriteLock {
 public:
  explicit GlobalWriteLock(std::mutex& mutex);
  ~GlobalWriteLock();

  GlobalWriteLock(GlobalWriteLock const&) = delete;
  GlobalWriteLock& operator=(GlobalWriteLock const&) = delete;
  GlobalWriteLock(GlobalWriteLock&& other) noexcept;
  GlobalWriteLock& operator=(GlobalWriteLock&&) = delete;

  bool isLocked() const;

 private:
  std::unique_lock<std::mutex> _lock;
};

/// Interface for the RocksDB database operations needed by hot backup.
/// This allows mocking the database in tests.
class IRocksDBForBackup {
 public:
  virtual ~IRocksDBForBackup() = default;

  /// Flush the write-ahead log to disk.
  virtual bool flushWAL() = 0;

  /// Create a checkpoint (hard-link-based snapshot) at the given path.
  virtual bool createCheckpoint(std::string const& path) = 0;

  /// Get the latest sequence number.
  virtual uint64_t getLatestSequenceNumber() const = 0;

  /// Collect collection names from the database.
  virtual std::vector<std::string> getCollectionNames() const = 0;
};

/// Core hot backup engine: checkpoint creation, listing, deletion, restoration.
class RocksDBHotBackup {
 public:
  struct CreateResult {
    BackupManifest manifest;
    double durationSeconds = 0.0;
  };

  /// Construct with a base path for storing backups and an optional DB interface.
  explicit RocksDBHotBackup(std::string backupBasePath,
                            IRocksDBForBackup* db = nullptr);

  /// Create a new hot backup via RocksDB checkpoint.
  HotBackupResult create(std::string const& label, CreateResult& out);

  /// List all available backups, sorted by timestamp ascending.
  HotBackupResult list(std::vector<BackupManifest>& out) const;

  /// Delete a specific backup by ID.
  HotBackupResult deleteBackup(std::string const& backupId);

  /// Restore a backup (validates manifest integrity).
  HotBackupResult restore(std::string const& backupId);

  /// Generate a unique backup ID based on current timestamp.
  static std::string generateBackupId();

  /// Get the backup directory path for a given ID.
  std::string backupPath(std::string const& backupId) const;

 private:
  std::string _backupBasePath;
  IRocksDBForBackup* _db;
  mutable std::mutex _mutex;

  /// Validate that a backup directory is intact.
  static HotBackupResult validateBackupDirectory(std::string const& backupDir);

  /// Recursively remove a directory.
  static HotBackupResult removeDirectory(std::string const& path);
};

}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_HOT_BACKUP_H
