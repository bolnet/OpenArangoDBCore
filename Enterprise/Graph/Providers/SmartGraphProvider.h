#pragma once
#ifndef ARANGODB_SMART_GRAPH_PROVIDER_H
#define ARANGODB_SMART_GRAPH_PROVIDER_H

#include "Enterprise/Sharding/ShardingStrategyEE.h"
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace arangodb::graph {

/// Identifies a vertex by collection and key.
struct VertexRef {
  std::string collection;
  std::string key;

  std::string id() const { return collection + "/" + key; }
};

/// An edge connecting two vertices.
struct EdgeRef {
  std::string collection;
  std::string key;
  std::string from;  // collection/key
  std::string to;    // collection/key
};

/// Result of expanding a vertex -- its neighboring edges and vertices.
struct ExpansionResult {
  std::vector<EdgeRef> edges;
  std::vector<VertexRef> neighbors;
};

/// SmartGraphProvider routes graph traversal operations to the correct shard
/// based on the smart graph attribute prefix in vertex keys.
///
/// In ArangoDB, this is a template-based provider (duck-typed, not virtual).
/// It determines whether a vertex is local to this DB-Server's shard and
/// routes traversal steps accordingly.
class SmartGraphProvider {
 public:
  /// @param collectionToShard mapping from collection name to local shard ID
  /// @param numberOfShards total number of shards in the graph
  explicit SmartGraphProvider(
      std::unordered_map<std::string, std::string> collectionToShard,
      uint32_t numberOfShards = 4);

  /// Check if a vertex is on a shard owned by this DB-Server.
  bool isResponsible(VertexRef const& vertex) const;

  /// Determine which shard a vertex belongs to based on its smart prefix.
  std::string getResponsibleShard(std::string_view collection,
                                  std::string_view key) const;

  /// Start a traversal from the given vertex.
  /// Returns true if the vertex is local and traversal can proceed.
  bool startVertex(VertexRef const& vertex) const;

  /// Check if this provider supports depth-specific index lookups.
  bool hasDepthSpecificLookup() const { return false; }

  /// Get the collection-to-shard mapping.
  std::unordered_map<std::string, std::string> const& collectionToShard()
      const {
    return _collectionToShard;
  }

  uint32_t numberOfShards() const { return _numberOfShards; }

 private:
  std::unordered_map<std::string, std::string> _collectionToShard;
  uint32_t _numberOfShards;
};

}  // namespace arangodb::graph

#endif  // ARANGODB_SMART_GRAPH_PROVIDER_H
