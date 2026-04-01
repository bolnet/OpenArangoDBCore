#pragma once
#ifndef ARANGODB_REPLICATION_STATUS_BUILDER_H
#define ARANGODB_REPLICATION_STATUS_BUILDER_H

#include <cstdint>
#include <string>

namespace arangodb {

/// Aggregates per-shard replication state into a JSON status object.
/// Used by RestReplicationHandler::executeStatus().
///
/// Templated on tracker and tailer types to support both production
/// (SequenceNumberTracker, ShardWALTailer) and test mocks.
///
/// Output format:
/// {
///   "isRunning": bool,
///   "lagSeconds": double,
///   "totalMessagesSent": uint64,
///   "totalMessagesAcked": uint64,
///   "totalPending": uint64,
///   "shardStates": [
///     {"shardId": "...", "lastGenerated": N, "lastApplied": N, "pending": N},
///     ...
///   ]
/// }
template <typename TrackerT, typename TailerT>
class ReplicationStatusBuilderT {
 public:
  ReplicationStatusBuilderT(TrackerT const& tracker, TailerT const& tailer)
      : _tracker(tracker), _tailer(tailer) {}

  /// Build the JSON status string (inner object, not wrapped in envelope).
  std::string build() const;

  /// Estimate lag in seconds from maximum pending message count.
  /// Uses a simple heuristic: lagSeconds = maxPending / estimatedThroughput.
  static double estimateLagSeconds(uint64_t maxPending,
                                   uint64_t messagesSent,
                                   uint64_t messagesAcked);

 private:
  TrackerT const& _tracker;
  TailerT const& _tailer;
};

}  // namespace arangodb

// Template implementation is in the .ipp file
#include "Enterprise/Replication/ReplicationStatusBuilder.ipp"

#endif  // ARANGODB_REPLICATION_STATUS_BUILDER_H
