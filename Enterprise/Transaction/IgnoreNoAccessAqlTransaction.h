#pragma once
#ifndef ARANGODB_IGNORE_NO_ACCESS_AQL_TRANSACTION_H
#define ARANGODB_IGNORE_NO_ACCESS_AQL_TRANSACTION_H

#include <string>

namespace arangodb {

/// Transaction subclass that bypasses access-control checks.
/// Used exclusively by the DC-to-DC ReplicationApplier to write
/// incoming replicated data to local collections regardless of
/// the target cluster's permission configuration.
///
/// SECURITY: This class must ONLY be instantiated by ReplicationApplier.
/// It grants unrestricted write access to all collections.
class IgnoreNoAccessAqlTransaction {
 public:
  IgnoreNoAccessAqlTransaction();
  ~IgnoreNoAccessAqlTransaction();

  // Non-copyable, movable
  IgnoreNoAccessAqlTransaction(IgnoreNoAccessAqlTransaction const&) = delete;
  IgnoreNoAccessAqlTransaction& operator=(IgnoreNoAccessAqlTransaction const&) =
      delete;
  IgnoreNoAccessAqlTransaction(IgnoreNoAccessAqlTransaction&&) noexcept;
  IgnoreNoAccessAqlTransaction& operator=(
      IgnoreNoAccessAqlTransaction&&) noexcept;

  /// Always returns TRI_ERROR_NO_ERROR (0) regardless of collection or mode.
  /// Overrides the base transaction's access check.
  int checkAccess(std::string const& collectionName, int accessMode) const;

  /// Returns true to indicate this transaction bypasses access control.
  bool isReplicationTransaction() const noexcept { return true; }

  /// Begin the transaction context.
  int begin();

  /// Commit all applied operations.
  int commit();

  /// Abort and roll back.
  int abort();

  /// Whether the transaction is currently active.
  bool isActive() const noexcept { return _active; }

 private:
  bool _active{false};
};

}  // namespace arangodb

#endif  // ARANGODB_IGNORE_NO_ACCESS_AQL_TRANSACTION_H
