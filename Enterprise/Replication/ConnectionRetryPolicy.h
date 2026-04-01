#pragma once
#ifndef ARANGODB_CONNECTION_RETRY_POLICY_H
#define ARANGODB_CONNECTION_RETRY_POLICY_H

#include <chrono>
#include <cstdint>

namespace arangodb {

/// Exponential backoff retry policy with jitter for transient network failures.
///
/// delay(attempt) = min(baseDelay * 2^attempt, maxDelay) * jitter
/// where jitter is uniformly distributed in [0.5, 1.5].
///
/// Immutable configuration; mutable attempt counter.
class ConnectionRetryPolicy {
 public:
  /// Construct a retry policy.
  /// @param maxRetries  Maximum number of retry attempts (0 = no retries)
  /// @param baseDelayMs Base delay in milliseconds for first retry
  /// @param maxDelayMs  Maximum delay cap in milliseconds
  ConnectionRetryPolicy(uint32_t maxRetries = 5,
                         uint32_t baseDelayMs = 100,
                         uint32_t maxDelayMs = 30000);

  /// Whether another retry attempt is allowed.
  bool shouldRetry() const noexcept;

  /// Compute the delay for the current attempt, then advance the counter.
  /// Returns delay in milliseconds with jitter applied.
  std::chrono::milliseconds nextDelay();

  /// Compute the delay for the current attempt WITHOUT jitter (for testing).
  /// Does NOT advance the counter.
  std::chrono::milliseconds rawDelay() const;

  /// Reset attempt counter to zero (e.g. after a successful connection).
  void reset() noexcept;

  /// Current attempt number (0-based).
  uint32_t attempt() const noexcept { return _attempt; }

  /// Maximum configured retries.
  uint32_t maxRetries() const noexcept { return _maxRetries; }

 private:
  uint32_t _maxRetries;
  uint32_t _baseDelayMs;
  uint32_t _maxDelayMs;
  uint32_t _attempt = 0;
};

}  // namespace arangodb

#endif  // ARANGODB_CONNECTION_RETRY_POLICY_H
