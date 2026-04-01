#pragma once
#ifndef ARANGODB_REPLICATION_FEED_STREAM_H
#define ARANGODB_REPLICATION_FEED_STREAM_H

#include <cstdint>
#include <string>

namespace arangodb {

/// Provides a streaming feed interface for arangosync to consume
/// DirectMQ replication messages via GET /_admin/replication/feed.
///
/// Templated on the tailer type to support both production
/// (ShardWALTailer) and test mocks (MockShardWALTailer).
///
/// Output format:
/// {
///   "messages": [
///     {"shardId": "...", "sequence": N, "operation": "...", "payload": "..."},
///     ...
///   ],
///   "hasMore": bool,
///   "lastSequence": uint64
/// }
template <typename TailerT>
class ReplicationFeedStreamT {
 public:
  explicit ReplicationFeedStreamT(TailerT const& tailer)
      : _tailer(tailer) {}

  /// Fetch a batch of messages for the given shard.
  /// Returns JSON object with messages after afterSequence, up to maxCount.
  std::string fetch(std::string const& shardId,
                    uint64_t afterSequence,
                    uint32_t maxCount) const;

 private:
  TailerT const& _tailer;
};

}  // namespace arangodb

#include "Enterprise/Replication/ReplicationFeedStream.ipp"

#endif  // ARANGODB_REPLICATION_FEED_STREAM_H
