#pragma once
#ifndef ARANGODB_CHECKPOINT_RECEIVER_H
#define ARANGODB_CHECKPOINT_RECEIVER_H

#include <cstdint>
#include <string>

namespace arangodb {

/// Result of a checkpoint receive operation.
struct CheckpointResult {
  bool accepted;
  std::string reason;  // non-empty when accepted is false
};

/// Accepts checkpoint progress reports from arangosync via
/// POST /_admin/replication/checkpoint.
///
/// Validates that the reported appliedSequence is within the valid range
/// (lastApplied < appliedSequence <= lastGenerated) before updating the
/// tracker.
///
/// Templated on the tracker type to support both production
/// (SequenceNumberTracker) and test mocks (MockSequenceNumberTracker).
template <typename TrackerT>
class CheckpointReceiverT {
 public:
  explicit CheckpointReceiverT(TrackerT& tracker) : _tracker(tracker) {}

  /// Process a checkpoint report.
  /// Returns accepted=true if the sequence was valid and tracker was updated.
  CheckpointResult receive(std::string const& shardId,
                           uint64_t appliedSequence);

 private:
  TrackerT& _tracker;
};

}  // namespace arangodb

#include "Enterprise/Replication/CheckpointReceiver.ipp"

#endif  // ARANGODB_CHECKPOINT_RECEIVER_H
