#pragma once
#ifndef ARANGODB_SMART_GRAPH_RPC_COMMUNICATOR_H
#define ARANGODB_SMART_GRAPH_RPC_COMMUNICATOR_H

#include <string>
#include <vector>

namespace arangodb::graph {

/// SmartGraphRPCCommunicator handles remote shard communication for
/// SmartGraph traversals that cross shard boundaries.
///
/// In a full ArangoDB cluster, this communicates with TraverserEngines
/// on remote DB-Servers via the cluster's internal RPC mechanism.
/// For standalone compilation, this provides the interface declaration.
class SmartGraphRPCCommunicator {
 public:
  virtual ~SmartGraphRPCCommunicator() = default;

  /// Send a traversal request to a remote shard.
  /// Returns true if the request was successfully queued.
  virtual bool sendTraversalRequest(std::string const& serverId,
                                    std::string const& shardId,
                                    std::string const& startVertexId,
                                    uint64_t depth) {
    return false;
  }

  /// Check if there are pending remote results.
  virtual bool hasPendingResults() const { return false; }
};

}  // namespace arangodb::graph

#endif  // ARANGODB_SMART_GRAPH_RPC_COMMUNICATOR_H
