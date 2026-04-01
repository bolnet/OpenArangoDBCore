#include "Enterprise/RestHandler/RestHotBackupHandler.h"

#include <sstream>

namespace arangodb {

RestHotBackupHandler::RestHotBackupHandler(RocksDBHotBackup& backup)
    : _backup(backup) {}

RestHotBackupHandler::Operation RestHotBackupHandler::parseOperation(
    std::string_view suffix) {
  if (suffix == "create") {
    return Operation::CREATE;
  }
  if (suffix == "list") {
    return Operation::LIST;
  }
  if (suffix == "delete") {
    return Operation::DELETE;
  }
  if (suffix == "restore") {
    return Operation::RESTORE;
  }
  return Operation::UNKNOWN;
}

HotBackupResult RestHotBackupHandler::executeCreate(
    std::string const& label, std::string& response) {
  RocksDBHotBackup::CreateResult result;
  auto res = _backup.create(label, result);
  if (!res.ok()) {
    response = formatError(res.errorNumber(), res.errorMessage());
    return res;
  }

  // Build result JSON
  std::ostringstream oss;
  oss << "{\n";
  oss << "  \"id\": \"" << result.manifest.backupId << "\",\n";
  oss << "  \"datetime\": \"" << result.manifest.timestamp << "\",\n";
  oss << "  \"version\": \"" << result.manifest.version << "\",\n";
  oss << "  \"durationSeconds\": " << result.manifest.sequenceNumber << ",\n";
  oss << "  \"collections\": [";
  for (size_t i = 0; i < result.manifest.collections.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    oss << "\"" << result.manifest.collections[i] << "\"";
  }
  oss << "],\n";
  oss << "  \"isConsistent\": "
      << (result.manifest.isConsistent ? "true" : "false") << "\n";
  oss << "}";

  response = formatSuccess(oss.str());
  return HotBackupResult::success();
}

HotBackupResult RestHotBackupHandler::executeList(std::string& response) {
  std::vector<BackupManifest> manifests;
  auto res = _backup.list(manifests);
  if (!res.ok()) {
    response = formatError(res.errorNumber(), res.errorMessage());
    return res;
  }

  std::ostringstream oss;
  oss << "[";
  for (size_t i = 0; i < manifests.size(); ++i) {
    if (i > 0) {
      oss << ", ";
    }
    auto const& m = manifests[i];
    oss << "{\n";
    oss << "  \"id\": \"" << m.backupId << "\",\n";
    oss << "  \"datetime\": \"" << m.timestamp << "\",\n";
    oss << "  \"version\": \"" << m.version << "\",\n";
    oss << "  \"collections\": [";
    for (size_t j = 0; j < m.collections.size(); ++j) {
      if (j > 0) {
        oss << ", ";
      }
      oss << "\"" << m.collections[j] << "\"";
    }
    oss << "],\n";
    oss << "  \"isConsistent\": " << (m.isConsistent ? "true" : "false")
        << "\n";
    oss << "}";
  }
  oss << "]";

  response = formatSuccess(oss.str());
  return HotBackupResult::success();
}

HotBackupResult RestHotBackupHandler::executeDelete(
    std::string const& backupId, std::string& response) {
  auto res = _backup.deleteBackup(backupId);
  if (!res.ok()) {
    response = formatError(res.errorNumber(), res.errorMessage());
    return res;
  }

  response = formatSuccess("{\"deleted\": true, \"id\": \"" + backupId + "\"}");
  return HotBackupResult::success();
}

HotBackupResult RestHotBackupHandler::executeRestore(
    std::string const& backupId, std::string& response) {
  auto res = _backup.restore(backupId);
  if (!res.ok()) {
    response = formatError(res.errorNumber(), res.errorMessage());
    return res;
  }

  response =
      formatSuccess("{\"restored\": true, \"id\": \"" + backupId + "\"}");
  return HotBackupResult::success();
}

std::string RestHotBackupHandler::formatSuccess(
    std::string const& resultJson) {
  return "{\n  \"error\": false,\n  \"code\": 200,\n  \"result\": " +
         resultJson + "\n}";
}

std::string RestHotBackupHandler::formatError(int code,
                                              std::string const& message) {
  std::ostringstream oss;
  oss << "{\n  \"error\": true,\n  \"code\": " << code
      << ",\n  \"errorMessage\": \"" << message << "\"\n}";
  return oss.str();
}

}  // namespace arangodb
