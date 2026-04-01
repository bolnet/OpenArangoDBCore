#pragma once
#ifndef ARANGODB_MESSAGE_BATCHER_H
#define ARANGODB_MESSAGE_BATCHER_H

#include "IWALIterator.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace arangodb {

/// A sealed batch of WAL entries ready for transmission via DirectMQ.
struct MessageBatch {
  std::string shardId;
  uint64_t sequenceNumber;
  std::vector<WALEntry> entries;
  uint64_t batchTimestamp;  // Microseconds since epoch when batch was sealed
};

/// Callback to obtain the next sequence number for a batch.
/// Wraps SequenceNumberGenerator::nextSequence() for dependency injection.
using SequenceProvider = std::function<uint64_t()>;

/// MessageBatcher accumulates WAL entries into MessageBatch payloads.
///
/// When the number of pending entries reaches batchSize, the batch is
/// automatically sealed and returned. Callers may also manually flush
/// a partial batch (e.g., on a timer or shutdown).
class MessageBatcher {
 public:
  /// @param shardId         Shard identifier for this batcher
  /// @param batchSize       Number of entries per batch (default 1000)
  /// @param sequenceProvider  Callback returning next monotonic sequence
  MessageBatcher(std::string shardId,
                 uint32_t batchSize,
                 SequenceProvider sequenceProvider);

  /// Add a WAL entry to the pending batch.
  /// Returns a sealed MessageBatch if the batch is full, otherwise nullopt.
  std::optional<MessageBatch> add(WALEntry entry);

  /// Flush any pending entries into a MessageBatch.
  /// Returns nullopt if no entries are pending.
  std::optional<MessageBatch> flush();

  /// Discard all pending entries without producing a message.
  void reset();

  /// Number of entries currently pending (not yet batched).
  uint32_t pendingCount() const;

 private:
  MessageBatch seal();

  std::string _shardId;
  uint32_t _batchSize;
  SequenceProvider _sequenceProvider;
  std::vector<WALEntry> _pending;
};

}  // namespace arangodb

#endif  // ARANGODB_MESSAGE_BATCHER_H
