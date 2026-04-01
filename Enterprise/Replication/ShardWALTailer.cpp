#include "ShardWALTailer.h"
#include "Enterprise/Cluster/SatelliteDistribution.h"
#include "Enterprise/Sharding/ShardingStrategyEE.h"

#include <optional>

namespace arangodb {

ShardWALTailer::ShardWALTailer(uint32_t shardIndex,
                               uint32_t numberOfShards,
                               std::unique_ptr<IWALIterator> iterator,
                               CollectionInfoResolver resolver)
    : _shardIndex(shardIndex),
      _numberOfShards(numberOfShards),
      _iterator(std::move(iterator)),
      _resolver(std::move(resolver)) {}

std::vector<WALEntry> ShardWALTailer::poll(uint32_t maxEntries) {
  std::vector<WALEntry> result;
  result.reserve(std::min(maxEntries, uint32_t(256)));

  while (_iterator->valid() && !_stopped.load(std::memory_order_relaxed) &&
         result.size() < maxEntries) {
    WALEntry const& current = _iterator->entry();

    if (!isSatelliteEntry(current) && belongsToShard(current)) {
      _lastProcessedSeq = current.sequenceNumber;
      result.push_back(current);  // Copy; caller owns the result
    } else if (!isSatelliteEntry(current)) {
      // Entry belongs to a different shard; still advance the sequence tracker
    }

    // Always advance past entries we've examined
    if (current.sequenceNumber > _lastProcessedSeq) {
      _lastProcessedSeq = current.sequenceNumber;
    }

    _iterator->next();
  }

  return result;
}

void ShardWALTailer::seekTo(uint64_t sequenceNumber) {
  _iterator->seek(sequenceNumber);
  _lastProcessedSeq = sequenceNumber > 0 ? sequenceNumber - 1 : 0;
}

void ShardWALTailer::stop() {
  _stopped.store(true, std::memory_order_relaxed);
}

bool ShardWALTailer::isStopped() const {
  return _stopped.load(std::memory_order_relaxed);
}

uint64_t ShardWALTailer::lastProcessedSequence() const {
  return _lastProcessedSeq;
}

bool ShardWALTailer::belongsToShard(WALEntry const& entry) const {
  if (_numberOfShards <= 1) {
    return true;  // Single shard: everything belongs here
  }
  uint32_t idx = computeShardIndex(entry.documentKey, _numberOfShards);
  return idx == _shardIndex;
}

bool ShardWALTailer::isSatelliteEntry(WALEntry const& entry) const {
  // Check cache first
  auto it = _infoCache.find(entry.collectionName);
  if (it != _infoCache.end()) {
    return SatelliteDistribution::isSatellite(it->second.replicationFactor);
  }

  // Resolve and cache
  auto info = _resolver(entry.collectionName);
  if (!info.has_value()) {
    return false;  // Unknown collection: do not skip
  }

  _infoCache[entry.collectionName] = info.value();
  return SatelliteDistribution::isSatellite(info->replicationFactor);
}

}  // namespace arangodb
