#pragma once
/// Mock Result for standalone builds.
/// Matches the real ArangoDB Basics/Result.h API surface.

#include "Basics/ErrorCode.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace arangodb {

namespace result {

class Error final {
 public:
  Error() : _errorNumber(TRI_ERROR_NO_ERROR) {}
  explicit Error(ErrorCode errorNumber) noexcept
      : _errorNumber(errorNumber) {}
  Error(ErrorCode errorNumber, std::string_view errorMessage)
      : _errorNumber(errorNumber), _errorMessage(errorMessage) {}

  [[nodiscard]] auto errorNumber() const noexcept -> ErrorCode {
    return _errorNumber;
  }
  [[nodiscard]] auto errorMessage() const& noexcept -> std::string_view {
    return _errorMessage;
  }
  [[nodiscard]] auto errorMessage() && noexcept -> std::string {
    return std::move(_errorMessage);
  }

 private:
  ErrorCode _errorNumber;
  std::string _errorMessage;
};

}  // namespace result

class Result final {
 public:
  // Prevent accidental bool/int conversion
  Result(bool) = delete;
  Result(int) = delete;

  // Default = OK
  Result() noexcept = default;

  // Error from code
  /* implicit */ Result(ErrorCode errorNumber)  // NOLINT
      : _error(std::make_unique<result::Error>(errorNumber)) {}

  Result(ErrorCode errorNumber, std::string_view errorMessage)
      : _error(
            std::make_unique<result::Error>(errorNumber, errorMessage)) {}

  Result(ErrorCode errorNumber, std::string const& errorMessage)
      : Result(errorNumber, std::string_view(errorMessage)) {}

  Result(ErrorCode errorNumber, std::string&& errorMessage)
      : _error(std::make_unique<result::Error>(
            errorNumber, std::string_view(errorMessage))) {}

  Result(result::Error error)
      : _error(std::make_unique<result::Error>(std::move(error))) {}

  // Copy
  Result(Result const& other) {
    if (other._error) {
      _error = std::make_unique<result::Error>(*other._error);
    }
  }
  auto operator=(Result const& other) -> Result& {
    if (this != &other) {
      if (other._error) {
        _error = std::make_unique<result::Error>(*other._error);
      } else {
        _error.reset();
      }
    }
    return *this;
  }

  // Move
  Result(Result&& other) noexcept = default;
  auto operator=(Result&& other) noexcept -> Result& = default;

  ~Result() = default;

  // Convenience factory (matches our existing code)
  static Result success() { return Result{}; }

  // Status queries
  [[nodiscard]] auto ok() const noexcept -> bool {
    return _error == nullptr;
  }
  [[nodiscard]] auto fail() const noexcept -> bool { return !ok(); }
  [[nodiscard]] auto errorNumber() const noexcept -> ErrorCode {
    return _error ? _error->errorNumber() : TRI_ERROR_NO_ERROR;
  }
  [[nodiscard]] auto is(ErrorCode errorNumber) const noexcept -> bool {
    return this->errorNumber() == errorNumber;
  }
  [[nodiscard]] auto isNot(ErrorCode errorNumber) const noexcept -> bool {
    return !is(errorNumber);
  }

  // Error message access
  [[nodiscard]] auto errorMessage() const& noexcept -> std::string_view {
    if (_error) {
      return _error->errorMessage();
    }
    return {};
  }
  [[nodiscard]] auto errorMessage() && noexcept -> std::string {
    if (_error) {
      return std::move(*_error).errorMessage();
    }
    return {};
  }

  // Reset
  auto reset() noexcept -> Result& {
    _error.reset();
    return *this;
  }
  auto reset(ErrorCode errorNumber) -> Result& {
    _error = std::make_unique<result::Error>(errorNumber);
    return *this;
  }
  auto reset(ErrorCode errorNumber, std::string_view errorMessage) -> Result& {
    _error = std::make_unique<result::Error>(errorNumber, errorMessage);
    return *this;
  }

 private:
  std::unique_ptr<result::Error> _error = nullptr;
};

}  // namespace arangodb
