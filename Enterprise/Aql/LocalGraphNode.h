#pragma once
#ifndef ARANGODB_LOCAL_GRAPH_NODE_H
#define ARANGODB_LOCAL_GRAPH_NODE_H

#include <string>

namespace arangodb::aql {

/// Mixin providing enterprise local graph node capabilities.
/// In ArangoDB, GraphNode has enterprise-gated methods that return false
/// in community builds. This class provides the enterprise implementations.
///
/// Local graph nodes execute graph traversals entirely within a single shard,
/// eliminating cross-shard network hops for SmartGraph queries.
class LocalGraphNodeMixin {
 public:
  bool isLocalGraphNode() const { return _isLocalGraphNode; }
  bool isUsedAsSatellite() const { return _isUsedAsSatellite; }
  bool isClusterOneShardRuleEnabled() const {
    return _clusterOneShardRuleEnabled;
  }

  void setLocalGraphNode(bool v) { _isLocalGraphNode = v; }
  void setUsedAsSatellite(bool v) { _isUsedAsSatellite = v; }
  void enableClusterOneShardRule(bool v) { _clusterOneShardRuleEnabled = v; }

 protected:
  bool _isLocalGraphNode = true;
  bool _isUsedAsSatellite = false;
  bool _clusterOneShardRuleEnabled = false;
};

}  // namespace arangodb::aql

#endif  // ARANGODB_LOCAL_GRAPH_NODE_H
