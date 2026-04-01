#pragma once
#ifndef ARANGODB_SEQUENCE_NUMBER_TRACKER_H
#define ARANGODB_SEQUENCE_NUMBER_TRACKER_H

#include <cstdint>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace arangodb {

/// Tracks the last applied sequence number per shard on the target side.
///
/// Used to implement idempotent replay: any message with
/// seq <= last_applied_seq for that shard is a duplicate and
/// must be rejected.
///
/// Thread-safe for concurrent shard application.
class SequenceNumberTracker {
 public:
  SequenceNumberTracker() = default;

  /// Check if a sequence has already been applied for this shard.
  /// Returns true if seq <= last_applied_seq (duplicate/stale message).
  bool isAlreadyApplied(std::string const& shardId, uint64_t seq) const;

  /// Record that a message with the given sequence was applied.
  /// Updates last_applied_seq to max(current, seq).
  void markApplied(std::string const& shardId, uint64_t seq);

  /// Get the last applied sequence for a shard (0 if unseen).
  uint64_t lastAppliedSequence(std::string const& shardId) const;

  /// Get a snapshot of all shard -> last_applied_seq mappings.
  /// Used by ReplicationCheckpoint for persistence.
  std::unordered_map<std::string, uint64_t> getState() const;

  /// Restore state from a previously saved snapshot.
  /// Used by ReplicationCheckpoint on startup.
  void restoreState(std::unordered_map<std::string, uint64_t> const& state);

  /// Reset all tracking state.
  void reset();

 private:
  mutable std::shared_mutex _mutex;
  std::unordered_map<std::string, uint64_t> _lastApplied;
};

}  // namespace arangodb

#endif  // ARANGODB_SEQUENCE_NUMBER_TRACKER_H
