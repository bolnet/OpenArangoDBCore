#pragma once
#ifndef ARANGODB_REST_HOT_BACKUP_HANDLER_H
#define ARANGODB_REST_HOT_BACKUP_HANDLER_H

#include <string>
#include <string_view>

#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"

namespace arangodb {

class RocksDBHotBackup;  // forward declaration

/// REST handler routing for /_admin/backup/* endpoints.
/// Delegates to RocksDBHotBackup for create/list/delete/restore operations.
class RestHotBackupHandler {
 public:
  enum class Operation {
    CREATE,
    LIST,
    DELETE,
    RESTORE,
    UNKNOWN
  };

  explicit RestHotBackupHandler(RocksDBHotBackup& backup);

  /// Parse operation from REST path suffix (e.g. "create", "list").
  static Operation parseOperation(std::string_view suffix);

  /// Execute a backup create operation.
  /// Populates response with backupId, timestamp, durationSeconds.
  HotBackupResult executeCreate(std::string const& label,
                                std::string& response);

  /// Execute a backup list operation.
  /// Populates response with JSON array of manifests.
  HotBackupResult executeList(std::string& response);

  /// Execute a backup delete operation.
  /// Populates response with confirmation.
  HotBackupResult executeDelete(std::string const& backupId,
                                std::string& response);

  /// Execute a backup restore operation.
  /// Populates response with confirmation.
  HotBackupResult executeRestore(std::string const& backupId,
                                 std::string& response);

 private:
  RocksDBHotBackup& _backup;

  /// Format a successful response envelope.
  static std::string formatSuccess(std::string const& resultJson);

  /// Format an error response envelope.
  static std::string formatError(int code, std::string const& message);
};

}  // namespace arangodb

#endif  // ARANGODB_REST_HOT_BACKUP_HANDLER_H
