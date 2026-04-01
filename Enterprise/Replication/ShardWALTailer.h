#pragma once
#ifndef ARANGODB_SHARD_WAL_TAILER_H
#define ARANGODB_SHARD_WAL_TAILER_H

#include "IWALIterator.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

/// Metadata needed to determine if a collection is satellite.
struct CollectionReplicationInfo {
  uint64_t replicationFactor;   // 0 = satellite
  uint32_t numberOfShards;
};

/// Callback to resolve collection replication info by name.
/// Returns nullopt if the collection is unknown.
using CollectionInfoResolver =
    std::function<std::optional<CollectionReplicationInfo>(
        std::string const& collectionName)>;

/// ShardWALTailer reads a RocksDB WAL stream and yields only entries
/// belonging to a specific shard index, skipping satellite collections.
///
/// Thread-safety: one tailer per thread. Use stop() from another thread
/// to signal graceful shutdown.
class ShardWALTailer {
 public:
  /// @param shardIndex      The shard index this tailer is responsible for
  /// @param numberOfShards  Total number of shards in the collection
  /// @param iterator        WAL iterator (owned; injected for testability)
  /// @param resolver        Callback to look up collection replication info
  ShardWALTailer(uint32_t shardIndex,
                 uint32_t numberOfShards,
                 std::unique_ptr<IWALIterator> iterator,
                 CollectionInfoResolver resolver);

  /// Poll the WAL for the next batch of entries belonging to this shard.
  /// Returns entries read since the last poll (up to maxEntries).
  /// Returns an empty vector if no new entries or stopped.
  std::vector<WALEntry> poll(uint32_t maxEntries = 1000);

  /// Seek to a specific WAL sequence number (e.g., on restart).
  void seekTo(uint64_t sequenceNumber);

  /// Signal the tailer to stop. Safe to call from another thread.
  void stop();

  /// Check if the tailer has been stopped.
  bool isStopped() const;

  /// Return the sequence number of the last entry yielded by poll().
  uint64_t lastProcessedSequence() const;

 private:
  /// Check if a WAL entry belongs to this shard.
  bool belongsToShard(WALEntry const& entry) const;

  /// Check if a WAL entry is from a satellite collection (should be skipped).
  bool isSatelliteEntry(WALEntry const& entry) const;

  uint32_t _shardIndex;
  uint32_t _numberOfShards;
  std::unique_ptr<IWALIterator> _iterator;
  CollectionInfoResolver _resolver;
  std::atomic<bool> _stopped{false};
  uint64_t _lastProcessedSeq = 0;

  /// Cache for resolved collection info to avoid repeated lookups.
  mutable std::unordered_map<std::string, CollectionReplicationInfo> _infoCache;
};

}  // namespace arangodb

#endif  // ARANGODB_SHARD_WAL_TAILER_H
