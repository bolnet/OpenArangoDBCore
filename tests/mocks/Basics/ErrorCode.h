#pragma once
/// Mock ErrorCode for standalone builds.
/// Matches the real ArangoDB Basics/ErrorCode.h API surface.

#include <cstdint>

namespace arangodb {

class ErrorCode {
 public:
  using ValueType = int;

  ErrorCode() = delete;
  constexpr explicit ErrorCode(ValueType value) noexcept : _value(value) {}

  ErrorCode(ErrorCode const&) = default;
  ErrorCode& operator=(ErrorCode const&) = default;
  ErrorCode(ErrorCode&&) = default;
  ErrorCode& operator=(ErrorCode&&) = default;

  [[nodiscard]] constexpr ValueType value() const noexcept { return _value; }
  [[nodiscard]] constexpr explicit operator ValueType() const noexcept {
    return _value;
  }

  [[nodiscard]] constexpr bool operator==(ErrorCode other) const noexcept {
    return _value == other._value;
  }
  [[nodiscard]] constexpr bool operator!=(ErrorCode other) const noexcept {
    return _value != other._value;
  }

 private:
  ValueType _value;
};

// Standard error codes used in Enterprise code
constexpr ErrorCode TRI_ERROR_NO_ERROR{0};
constexpr ErrorCode TRI_ERROR_INTERNAL{4};
constexpr ErrorCode TRI_ERROR_BAD_PARAMETER{10};
constexpr ErrorCode TRI_ERROR_FORBIDDEN{11};
constexpr ErrorCode TRI_ERROR_NOT_FOUND{1203};

}  // namespace arangodb
