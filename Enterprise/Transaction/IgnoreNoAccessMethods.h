#pragma once
#ifndef ARANGODB_IGNORE_NO_ACCESS_METHODS_H
#define ARANGODB_IGNORE_NO_ACCESS_METHODS_H

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

/// Method overrides for replication context.
/// Wraps collection write operations to bypass read-only enforcement.
/// Works in conjunction with IgnoreNoAccessAqlTransaction.
///
/// All write methods skip the isReadOnly() check that would normally
/// reject mutations on collections configured as read-only on the target.
class IgnoreNoAccessMethods {
 public:
  IgnoreNoAccessMethods();
  ~IgnoreNoAccessMethods();

  // Non-copyable, movable
  IgnoreNoAccessMethods(IgnoreNoAccessMethods const&) = delete;
  IgnoreNoAccessMethods& operator=(IgnoreNoAccessMethods const&) = delete;
  IgnoreNoAccessMethods(IgnoreNoAccessMethods&&) noexcept;
  IgnoreNoAccessMethods& operator=(IgnoreNoAccessMethods&&) noexcept;

  /// Insert a document, bypassing read-only checks.
  /// @param collectionName target collection
  /// @param documentData serialized VPack document bytes
  /// @return 0 on success, error code otherwise
  int insert(std::string const& collectionName,
             std::vector<uint8_t> const& documentData);

  /// Update a document, bypassing read-only checks.
  int update(std::string const& collectionName,
             std::vector<uint8_t> const& documentData);

  /// Remove a document by key, bypassing read-only checks.
  int remove(std::string const& collectionName,
             std::string const& documentKey);

  /// Truncate a collection, bypassing read-only checks.
  int truncate(std::string const& collectionName);

  /// Returns true -- this instance operates in replication context.
  bool isReplicationContext() const noexcept { return true; }

  /// Returns total number of operations executed through this instance.
  uint64_t operationCount() const noexcept { return _operationCount; }

 private:
  /// Internal: execute a write operation without access checks.
  int executeBypass(std::string const& collectionName,
                    std::string const& operationType,
                    std::vector<uint8_t> const& data);

  uint64_t _operationCount{0};
};

}  // namespace arangodb

#endif  // ARANGODB_IGNORE_NO_ACCESS_METHODS_H
