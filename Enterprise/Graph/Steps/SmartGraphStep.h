#pragma once
#ifndef ARANGODB_SMART_GRAPH_STEP_H
#define ARANGODB_SMART_GRAPH_STEP_H

#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb::graph {

/// SmartGraphStep represents a single step in a SmartGraph traversal.
/// Each step tracks whether it can be executed locally on this shard
/// or requires a remote call to another DB-Server.
class SmartGraphStep {
 public:
  SmartGraphStep() = default;

  SmartGraphStep(std::string vertexId, std::string shardId, uint64_t depth,
                 bool isLocal)
      : _vertexId(std::move(vertexId)),
        _shardId(std::move(shardId)),
        _depth(depth),
        _isLocal(isLocal) {}

  /// The vertex this step leads to.
  std::string const& vertexId() const { return _vertexId; }

  /// The shard containing this vertex.
  std::string const& shardId() const { return _shardId; }

  /// Current traversal depth.
  uint64_t depth() const { return _depth; }

  /// Whether this step's vertex is on a local shard.
  bool isLocal() const { return _isLocal; }

  /// Extract the smart prefix from this step's vertex key.
  std::string_view getSmartPrefix() const;

  /// Check if this step is valid (has a vertex ID).
  bool isValid() const { return !_vertexId.empty(); }

 private:
  std::string _vertexId;
  std::string _shardId;
  uint64_t _depth = 0;
  bool _isLocal = false;
};

}  // namespace arangodb::graph

#endif  // ARANGODB_SMART_GRAPH_STEP_H
