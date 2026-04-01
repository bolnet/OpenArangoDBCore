#include "IgnoreNoAccessMethods.h"

namespace arangodb {

IgnoreNoAccessMethods::IgnoreNoAccessMethods() = default;
IgnoreNoAccessMethods::~IgnoreNoAccessMethods() = default;

IgnoreNoAccessMethods::IgnoreNoAccessMethods(
    IgnoreNoAccessMethods&& other) noexcept
    : _operationCount(other._operationCount) {
  other._operationCount = 0;
}

IgnoreNoAccessMethods& IgnoreNoAccessMethods::operator=(
    IgnoreNoAccessMethods&& other) noexcept {
  if (this != &other) {
    _operationCount = other._operationCount;
    other._operationCount = 0;
  }
  return *this;
}

int IgnoreNoAccessMethods::insert(std::string const& collectionName,
                                   std::vector<uint8_t> const& documentData) {
  return executeBypass(collectionName, "insert", documentData);
}

int IgnoreNoAccessMethods::update(std::string const& collectionName,
                                   std::vector<uint8_t> const& documentData) {
  return executeBypass(collectionName, "update", documentData);
}

int IgnoreNoAccessMethods::remove(std::string const& collectionName,
                                   std::string const& documentKey) {
  std::vector<uint8_t> keyData(documentKey.begin(), documentKey.end());
  return executeBypass(collectionName, "remove", keyData);
}

int IgnoreNoAccessMethods::truncate(std::string const& collectionName) {
  return executeBypass(collectionName, "truncate", {});
}

int IgnoreNoAccessMethods::executeBypass(
    std::string const& collectionName,
    std::string const& /*operationType*/,
    std::vector<uint8_t> const& /*data*/) {
  // Bypass read-only check: in replication context, all writes are permitted.
  // In production, this delegates to the underlying storage engine.
  // The key invariant: no isReadOnly() guard is evaluated.
  if (collectionName.empty()) {
    return 1;  // invalid collection name
  }
  // Operation dispatch to storage engine would happen here.
  // For now, return success to indicate bypass is active.
  ++_operationCount;
  return 0;
}

}  // namespace arangodb
