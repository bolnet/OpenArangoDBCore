#include "LocalTraversalNode.h"

namespace arangodb::aql {

// --- LocalTraversalNode ---

LocalGraphNodeData LocalTraversalNode::toData() const {
  auto data = _data;
  data.nodeType = GraphNodeType::TRAVERSAL;
  data.isLocalGraphNode = true;
  data.isUsedAsSatellite = _isUsedAsSatellite;
  data.clusterOneShardRuleEnabled = _clusterOneShardRuleEnabled;
  return data;
}

LocalTraversalNode LocalTraversalNode::fromData(
    LocalGraphNodeData const& data) {
  LocalTraversalNode node(data);
  node.setUsedAsSatellite(data.isUsedAsSatellite);
  node.enableClusterOneShardRule(data.clusterOneShardRuleEnabled);
  return node;
}

LocalTraversalNode LocalTraversalNode::clone() const {
  return fromData(toData());
}

size_t LocalTraversalNode::getMemoryUsedBytes() const {
  size_t bytes = sizeof(*this);
  bytes += _data.graphName.capacity();
  for (auto const& [k, v] : _data.collectionToShard) {
    bytes += k.capacity() + v.capacity();
  }
  return bytes;
}

// --- LocalEnumeratePathsNode ---

LocalGraphNodeData LocalEnumeratePathsNode::toData() const {
  auto data = _data;
  data.nodeType = GraphNodeType::ENUMERATE_PATHS;
  data.isLocalGraphNode = true;
  data.isUsedAsSatellite = _isUsedAsSatellite;
  data.clusterOneShardRuleEnabled = _clusterOneShardRuleEnabled;
  return data;
}

LocalEnumeratePathsNode LocalEnumeratePathsNode::fromData(
    LocalGraphNodeData const& data) {
  LocalEnumeratePathsNode node(data);
  node.setUsedAsSatellite(data.isUsedAsSatellite);
  node.enableClusterOneShardRule(data.clusterOneShardRuleEnabled);
  return node;
}

LocalEnumeratePathsNode LocalEnumeratePathsNode::clone() const {
  return fromData(toData());
}

size_t LocalEnumeratePathsNode::getMemoryUsedBytes() const {
  size_t bytes = sizeof(*this);
  bytes += _data.graphName.capacity();
  return bytes;
}

// --- LocalShortestPathNode ---

LocalGraphNodeData LocalShortestPathNode::toData() const {
  auto data = _data;
  data.nodeType = GraphNodeType::SHORTEST_PATH;
  data.isLocalGraphNode = true;
  data.isUsedAsSatellite = _isUsedAsSatellite;
  data.clusterOneShardRuleEnabled = _clusterOneShardRuleEnabled;
  return data;
}

LocalShortestPathNode LocalShortestPathNode::fromData(
    LocalGraphNodeData const& data) {
  LocalShortestPathNode node(data);
  node.setUsedAsSatellite(data.isUsedAsSatellite);
  node.enableClusterOneShardRule(data.clusterOneShardRuleEnabled);
  return node;
}

LocalShortestPathNode LocalShortestPathNode::clone() const {
  return fromData(toData());
}

size_t LocalShortestPathNode::getMemoryUsedBytes() const {
  size_t bytes = sizeof(*this);
  bytes += _data.graphName.capacity();
  return bytes;
}

// --- Factory ---

LocalGraphNodeVariant createLocalGraphNode(LocalGraphNodeData const& data) {
  LocalGraphNodeVariant result;

  switch (data.nodeType) {
    case GraphNodeType::TRAVERSAL:
      result.type = CreateLocalGraphNodeResult::TraversalNode;
      result.traversal = LocalTraversalNode::fromData(data);
      break;
    case GraphNodeType::ENUMERATE_PATHS:
      result.type = CreateLocalGraphNodeResult::EnumeratePathsNode;
      result.enumeratePaths = LocalEnumeratePathsNode::fromData(data);
      break;
    case GraphNodeType::SHORTEST_PATH:
      result.type = CreateLocalGraphNodeResult::ShortestPathNode;
      result.shortestPath = LocalShortestPathNode::fromData(data);
      break;
    default:
      result.type = CreateLocalGraphNodeResult::Error;
      break;
  }

  return result;
}

}  // namespace arangodb::aql
