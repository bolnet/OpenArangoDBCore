#include "SmartGraphProvider.h"
#include "Enterprise/Sharding/ShardingStrategyEE.h"

namespace arangodb::graph {

SmartGraphProvider::SmartGraphProvider(
    std::unordered_map<std::string, std::string> collectionToShard,
    uint32_t numberOfShards)
    : _collectionToShard(std::move(collectionToShard)),
      _numberOfShards(numberOfShards) {}

bool SmartGraphProvider::isResponsible(VertexRef const& vertex) const {
  auto expectedShard = getResponsibleShard(vertex.collection, vertex.key);
  auto it = _collectionToShard.find(vertex.collection);
  if (it == _collectionToShard.end()) {
    return false;
  }
  return it->second == expectedShard;
}

std::string SmartGraphProvider::getResponsibleShard(
    std::string_view collection, std::string_view key) const {
  auto prefix = arangodb::extractSmartPrefix(key);
  uint32_t shardIndex = arangodb::computeShardIndex(prefix, _numberOfShards);
  return std::string(collection) + "_s" + std::to_string(shardIndex);
}

bool SmartGraphProvider::startVertex(VertexRef const& vertex) const {
  return isResponsible(vertex);
}

}  // namespace arangodb::graph
