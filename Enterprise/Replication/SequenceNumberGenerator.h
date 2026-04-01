#pragma once
#ifndef ARANGODB_SEQUENCE_NUMBER_GENERATOR_H
#define ARANGODB_SEQUENCE_NUMBER_GENERATOR_H

#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace arangodb {

/// Generates monotonically increasing sequence numbers per shard.
///
/// Thread-safe: multiple WAL tail threads can call nextSequence()
/// concurrently for the same or different shards without data races.
///
/// Sequence numbers start at 1 for each shard (0 is reserved as
/// the "no sequence" sentinel).
class SequenceNumberGenerator {
 public:
  SequenceNumberGenerator() = default;

  // Non-copyable, non-movable (contains atomics behind shared_ptr)
  SequenceNumberGenerator(SequenceNumberGenerator const&) = delete;
  SequenceNumberGenerator& operator=(SequenceNumberGenerator const&) = delete;

  /// Generate the next sequence number for the given shard.
  /// Returns a monotonically increasing uint64 (1, 2, 3, ...).
  /// Thread-safe via atomic increment.
  uint64_t nextSequence(std::string const& shardId);

  /// Return the current (last generated) sequence for a shard.
  /// Returns 0 if the shard has never been seen.
  uint64_t currentSequence(std::string const& shardId) const;

  /// Reset all counters (used in testing and full resync).
  void reset();

 private:
  /// Per-shard atomic counters. Wrapped in shared_ptr so the map
  /// value is stable after insertion (atomics are non-movable).
  mutable std::shared_mutex _mutex;
  std::unordered_map<std::string, std::shared_ptr<std::atomic<uint64_t>>> _counters;

  /// Get or create the atomic counter for a shard.
  std::shared_ptr<std::atomic<uint64_t>> getOrCreate(std::string const& shardId);
};

}  // namespace arangodb

#endif  // ARANGODB_SEQUENCE_NUMBER_GENERATOR_H
