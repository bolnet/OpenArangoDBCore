#pragma once
#ifndef ARANGODB_LOCAL_TRAVERSAL_NODE_H
#define ARANGODB_LOCAL_TRAVERSAL_NODE_H

#include "Enterprise/Aql/LocalGraphNode.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {

/// NodeType enum values matching ArangoDB's ExecutionNode::NodeType.
/// Local nodes reuse the same values as their standard counterparts.
enum class GraphNodeType : int {
  TRAVERSAL = 22,
  SHORTEST_PATH = 24,
  ENUMERATE_PATHS = 25
};

/// Serialized representation of a local graph node for VelocyPack round-trip.
struct LocalGraphNodeData {
  GraphNodeType nodeType;
  bool isLocalGraphNode = true;
  bool isUsedAsSatellite = false;
  bool clusterOneShardRuleEnabled = false;
  bool isSmart = false;
  bool isDisjoint = false;
  std::string graphName;
  std::unordered_map<std::string, std::string> collectionToShard;

  // Traversal-specific
  uint64_t minDepth = 1;
  uint64_t maxDepth = 1;
};

/// LocalTraversalNode executes graph traversals entirely within a single shard.
/// It replaces the standard TraversalNode when the AQL optimizer detects that
/// a SmartGraph traversal can be executed locally.
///
/// In ArangoDB, this node is created by the enterprise optimizer rule that
/// converts TraversalNode to LocalTraversalNode for SmartGraph queries.
class LocalTraversalNode : public LocalGraphNodeMixin {
 public:
  LocalTraversalNode() = default;

  explicit LocalTraversalNode(LocalGraphNodeData data)
      : _data(std::move(data)) {
    _isLocalGraphNode = true;
  }

  /// Returns TRAVERSAL (same as standard TraversalNode -- local nodes don't
  /// have separate type values).
  GraphNodeType getType() const { return GraphNodeType::TRAVERSAL; }

  /// Serialize to a data struct (simulates VelocyPack serialization).
  LocalGraphNodeData toData() const;

  /// Deserialize from a data struct.
  static LocalTraversalNode fromData(LocalGraphNodeData const& data);

  /// Clone this node.
  LocalTraversalNode clone() const;

  /// Estimated memory usage.
  size_t getMemoryUsedBytes() const;

  // Traversal parameters
  uint64_t minDepth() const { return _data.minDepth; }
  uint64_t maxDepth() const { return _data.maxDepth; }
  void setDepth(uint64_t min, uint64_t max) {
    _data.minDepth = min;
    _data.maxDepth = max;
  }

  std::string const& graphName() const { return _data.graphName; }
  void setGraphName(std::string name) { _data.graphName = std::move(name); }

  bool isSmart() const { return _data.isSmart; }
  void setSmart(bool v) { _data.isSmart = v; }

  bool isDisjoint() const { return _data.isDisjoint; }
  void setDisjoint(bool v) { _data.isDisjoint = v; }

  // Collection-to-shard mapping
  void addCollectionToShard(std::string collection, std::string shard) {
    _data.collectionToShard[std::move(collection)] = std::move(shard);
  }

  std::unordered_map<std::string, std::string> const& collectionToShard()
      const {
    return _data.collectionToShard;
  }

 private:
  LocalGraphNodeData _data;
};

/// LocalEnumeratePathsNode for shard-local path enumeration.
class LocalEnumeratePathsNode : public LocalGraphNodeMixin {
 public:
  LocalEnumeratePathsNode() = default;
  explicit LocalEnumeratePathsNode(LocalGraphNodeData data)
      : _data(std::move(data)) {
    _isLocalGraphNode = true;
  }

  GraphNodeType getType() const { return GraphNodeType::ENUMERATE_PATHS; }
  LocalGraphNodeData toData() const;
  static LocalEnumeratePathsNode fromData(LocalGraphNodeData const& data);
  LocalEnumeratePathsNode clone() const;
  size_t getMemoryUsedBytes() const;

  std::string const& graphName() const { return _data.graphName; }
  void setGraphName(std::string name) { _data.graphName = std::move(name); }
  bool isSmart() const { return _data.isSmart; }
  bool isDisjoint() const { return _data.isDisjoint; }

 private:
  LocalGraphNodeData _data;
};

/// LocalShortestPathNode for shard-local shortest path queries.
class LocalShortestPathNode : public LocalGraphNodeMixin {
 public:
  LocalShortestPathNode() = default;
  explicit LocalShortestPathNode(LocalGraphNodeData data)
      : _data(std::move(data)) {
    _isLocalGraphNode = true;
  }

  GraphNodeType getType() const { return GraphNodeType::SHORTEST_PATH; }
  LocalGraphNodeData toData() const;
  static LocalShortestPathNode fromData(LocalGraphNodeData const& data);
  LocalShortestPathNode clone() const;
  size_t getMemoryUsedBytes() const;

  std::string const& graphName() const { return _data.graphName; }
  void setGraphName(std::string name) { _data.graphName = std::move(name); }
  bool isSmart() const { return _data.isSmart; }
  bool isDisjoint() const { return _data.isDisjoint; }

 private:
  LocalGraphNodeData _data;
};

/// Factory function result types.
enum class CreateLocalGraphNodeResult {
  TraversalNode,
  EnumeratePathsNode,
  ShortestPathNode,
  Error
};

struct LocalGraphNodeVariant {
  CreateLocalGraphNodeResult type = CreateLocalGraphNodeResult::Error;
  LocalTraversalNode traversal;
  LocalEnumeratePathsNode enumeratePaths;
  LocalShortestPathNode shortestPath;
};

/// Factory function that creates the appropriate local graph node
/// from serialized data. This replaces the community stub that throws
/// TRI_ERROR_NOT_IMPLEMENTED.
LocalGraphNodeVariant createLocalGraphNode(LocalGraphNodeData const& data);

}  // namespace arangodb::aql

#endif  // ARANGODB_LOCAL_TRAVERSAL_NODE_H
