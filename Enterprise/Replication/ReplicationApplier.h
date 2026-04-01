#pragma once
#ifndef ARANGODB_REPLICATION_APPLIER_H
#define ARANGODB_REPLICATION_APPLIER_H

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

/// Operation types matching DirectMQMessage::Operation.
enum class ReplicationOperation : uint8_t {
  INSERT = 0,
  UPDATE = 1,
  REMOVE = 2,
  TRUNCATE = 3
};

/// A single replication message to apply on the target.
struct ApplyMessage {
  std::string shardId;
  uint64_t sequence;
  ReplicationOperation operation;
  std::vector<uint8_t> payload;
  std::string documentKey;
  std::string documentRev;
};

/// Callback for executing a write operation on a collection.
/// Returns 0 on success, error code on failure.
using WriteCallback = std::function<int(std::string const& collection,
                                         ReplicationOperation op,
                                         std::vector<uint8_t> const& payload,
                                         std::string const& docKey)>;

/// Callback to check if a collection is a satellite.
using SatelliteCheckCallback =
    std::function<bool(std::string const& collection)>;

/// ReplicationApplier consumes batches of DirectMQ messages and
/// applies them to local collections using IgnoreNoAccess transactions.
///
/// Guarantees:
/// - Idempotent: duplicate messages (seq <= last_applied) are rejected
/// - Ordered: within a shard, messages are applied in sequence order
/// - Out-of-order buffering: messages arriving ahead of sequence are held
/// - Satellite skip: satellite collection messages are silently dropped
class ReplicationApplier {
 public:
  /// Construct with write and satellite-check callbacks.
  ReplicationApplier(WriteCallback writeCb,
                     SatelliteCheckCallback satelliteCb);
  ~ReplicationApplier();

  // Non-copyable
  ReplicationApplier(ReplicationApplier const&) = delete;
  ReplicationApplier& operator=(ReplicationApplier const&) = delete;

  /// Apply a batch of messages. Returns count of successfully applied messages.
  /// Duplicates and satellite messages are not counted.
  uint64_t applyBatch(std::vector<ApplyMessage> const& messages);

  /// Apply a single message. Returns 0 on success, error code on failure.
  /// Returns 0 (no-op) for duplicates and satellite messages.
  int applyMessage(ApplyMessage const& message);

  /// Get the last applied sequence for a shard.
  uint64_t lastAppliedSequence(std::string const& shardId) const;

  /// Get count of messages currently buffered (out-of-order).
  uint64_t bufferedCount() const;

  /// Get total messages applied since construction.
  uint64_t totalApplied() const noexcept { return _totalApplied; }

  /// Get total duplicates rejected since construction.
  uint64_t totalDuplicatesRejected() const noexcept {
    return _totalDuplicates;
  }

  /// Get total satellite messages skipped since construction.
  uint64_t totalSatelliteSkipped() const noexcept {
    return _totalSatelliteSkipped;
  }

  /// Drain any buffered out-of-order messages that can now be applied.
  /// Called internally after each apply, but can be called externally.
  /// NOTE: caller must hold _mutex when calling externally, or use
  /// drainBufferedExternal() which acquires the lock.
  uint64_t drainBuffered(std::string const& shardId);

 private:
  WriteCallback _writeCb;
  SatelliteCheckCallback _satelliteCb;

  mutable std::mutex _mutex;

  /// Per-shard last applied sequence number.
  std::unordered_map<std::string, uint64_t> _lastApplied;

  /// Per-shard buffer for out-of-order messages: shardId -> (seq -> message).
  std::unordered_map<std::string,
                     std::unordered_map<uint64_t, ApplyMessage>>
      _buffer;

  uint64_t _totalApplied{0};
  uint64_t _totalDuplicates{0};
  uint64_t _totalSatelliteSkipped{0};

  /// Internal: apply a single message assuming lock is held.
  int applyLocked(ApplyMessage const& message);
};

}  // namespace arangodb

#endif  // ARANGODB_REPLICATION_APPLIER_H
