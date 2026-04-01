#include "IgnoreNoAccessAqlTransaction.h"

namespace arangodb {

IgnoreNoAccessAqlTransaction::IgnoreNoAccessAqlTransaction() = default;
IgnoreNoAccessAqlTransaction::~IgnoreNoAccessAqlTransaction() {
  // If still active on destruction, abort to prevent resource leaks.
  if (_active) {
    abort();
  }
}

IgnoreNoAccessAqlTransaction::IgnoreNoAccessAqlTransaction(
    IgnoreNoAccessAqlTransaction&& other) noexcept
    : _active(other._active) {
  other._active = false;
}

IgnoreNoAccessAqlTransaction& IgnoreNoAccessAqlTransaction::operator=(
    IgnoreNoAccessAqlTransaction&& other) noexcept {
  if (this != &other) {
    if (_active) {
      abort();
    }
    _active = other._active;
    other._active = false;
  }
  return *this;
}

int IgnoreNoAccessAqlTransaction::checkAccess(
    std::string const& /*collectionName*/, int /*accessMode*/) const {
  // Replication transactions bypass all access checks.
  // TRI_ERROR_NO_ERROR = 0
  return 0;
}

int IgnoreNoAccessAqlTransaction::begin() {
  if (_active) {
    return 1;  // already active
  }
  _active = true;
  return 0;
}

int IgnoreNoAccessAqlTransaction::commit() {
  if (!_active) {
    return 1;  // not active
  }
  _active = false;
  return 0;
}

int IgnoreNoAccessAqlTransaction::abort() {
  _active = false;
  return 0;
}

}  // namespace arangodb
