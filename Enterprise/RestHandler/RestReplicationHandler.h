#pragma once
#ifndef ARANGODB_REST_REPLICATION_HANDLER_H
#define ARANGODB_REST_REPLICATION_HANDLER_H

#include <cstdint>
#include <string>
#include <string_view>

namespace arangodb {

/// REST handler for /_admin/replication/* endpoints.
/// Routes to status, start, stop, reset, feed, and checkpoint operations.
/// Follows the same envelope pattern as RestHotBackupHandler.
///
/// Templated on tracker and tailer types for testability.
template <typename TrackerT, typename TailerT>
class RestReplicationHandlerT {
 public:
  enum class Route {
    STATUS,
    START,
    STOP,
    RESET,
    FEED,
    CHECKPOINT,
    UNKNOWN
  };

  RestReplicationHandlerT(TrackerT& tracker, TailerT& tailer);

  /// Parse route from REST path suffix.
  static Route parseRoute(std::string_view suffix);

  /// GET /_admin/replication/status
  /// Returns aggregated replication status as JSON.
  void executeStatus(std::string& response);

  /// POST /_admin/replication/start
  /// Starts replication. Returns 409 if already running.
  void executeStart(std::string& response);

  /// POST /_admin/replication/stop
  /// Stops replication gracefully. Returns 409 if already stopped.
  void executeStop(std::string& response);

  /// POST /_admin/replication/reset
  /// Resets all per-shard sequence tracking. Returns count of shards reset.
  void executeReset(std::string& response);

  /// GET /_admin/replication/feed?shardId=X&afterSequence=N&maxCount=M
  /// Streaming feed for arangosync to consume DirectMQ messages.
  void executeFeed(std::string const& shardId,
                   uint64_t afterSequence,
                   uint32_t maxCount,
                   std::string& response);

  /// POST /_admin/replication/checkpoint
  /// Body: {"shardId": "...", "appliedSequence": N}
  /// arangosync reports applied progress.
  void executeCheckpoint(std::string const& shardId,
                         uint64_t appliedSequence,
                         std::string& response);

  /// Format a successful response envelope.
  static std::string formatSuccess(std::string const& resultJson);

  /// Format an error response envelope.
  static std::string formatError(int code, std::string const& message);

 private:
  TrackerT& _tracker;
  TailerT& _tailer;
};

}  // namespace arangodb

#include "Enterprise/RestHandler/RestReplicationHandler.ipp"

#endif  // ARANGODB_REST_REPLICATION_HANDLER_H
