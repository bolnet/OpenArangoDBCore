#include "ConnectionRetryPolicy.h"

#include <algorithm>
#include <random>

namespace arangodb {

ConnectionRetryPolicy::ConnectionRetryPolicy(uint32_t maxRetries,
                                               uint32_t baseDelayMs,
                                               uint32_t maxDelayMs)
    : _maxRetries(maxRetries),
      _baseDelayMs(baseDelayMs),
      _maxDelayMs(maxDelayMs) {}

bool ConnectionRetryPolicy::shouldRetry() const noexcept {
  return _attempt < _maxRetries;
}

std::chrono::milliseconds ConnectionRetryPolicy::rawDelay() const {
  // Compute raw exponential delay: baseDelay * 2^attempt
  // Use uint64_t to avoid overflow when attempt > 31
  uint64_t raw = static_cast<uint64_t>(_baseDelayMs) << _attempt;
  uint64_t clamped = std::min(raw, static_cast<uint64_t>(_maxDelayMs));
  return std::chrono::milliseconds(clamped);
}

std::chrono::milliseconds ConnectionRetryPolicy::nextDelay() {
  uint64_t clamped = static_cast<uint64_t>(rawDelay().count());

  // Apply jitter: uniform in [0.5, 1.5] of clamped delay
  thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> jitter(0.5, 1.5);
  auto delayMs =
      static_cast<uint64_t>(static_cast<double>(clamped) * jitter(rng));

  ++_attempt;
  return std::chrono::milliseconds(delayMs);
}

void ConnectionRetryPolicy::reset() noexcept { _attempt = 0; }

}  // namespace arangodb
