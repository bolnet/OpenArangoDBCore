#pragma once

#include "Enterprise/Replication/ReplicationStatusBuilder.h"
#include "Enterprise/Replication/ReplicationFeedStream.h"
#include "Enterprise/Replication/CheckpointReceiver.h"

#include <chrono>
#include <sstream>

namespace arangodb {

template <typename TrackerT, typename TailerT>
RestReplicationHandlerT<TrackerT, TailerT>::RestReplicationHandlerT(
    TrackerT& tracker, TailerT& tailer)
    : _tracker(tracker), _tailer(tailer) {}

template <typename TrackerT, typename TailerT>
typename RestReplicationHandlerT<TrackerT, TailerT>::Route
RestReplicationHandlerT<TrackerT, TailerT>::parseRoute(
    std::string_view suffix) {
  if (suffix == "status") {
    return Route::STATUS;
  }
  if (suffix == "start") {
    return Route::START;
  }
  if (suffix == "stop") {
    return Route::STOP;
  }
  if (suffix == "reset") {
    return Route::RESET;
  }
  if (suffix == "feed") {
    return Route::FEED;
  }
  if (suffix == "checkpoint") {
    return Route::CHECKPOINT;
  }
  return Route::UNKNOWN;
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeStatus(
    std::string& response) {
  ReplicationStatusBuilderT<TrackerT, TailerT> builder(_tracker, _tailer);
  std::string statusJson = builder.build();
  response = formatSuccess(statusJson);
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeStart(
    std::string& response) {
  if (_tailer.isRunning()) {
    response = formatError(409, "replication is already running");
    return;
  }
  _tailer.start();
  auto now = std::chrono::system_clock::now();
  auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
                   now.time_since_epoch())
                   .count();
  std::ostringstream oss;
  oss << "{\"started\": true, \"timestamp\": " << epoch
      << ", \"previousState\": \"stopped\"}";
  response = formatSuccess(oss.str());
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeStop(
    std::string& response) {
  if (!_tailer.isRunning()) {
    response = formatError(409, "replication is not running");
    return;
  }
  _tailer.stop();
  auto now = std::chrono::system_clock::now();
  auto epoch = std::chrono::duration_cast<std::chrono::seconds>(
                   now.time_since_epoch())
                   .count();
  std::ostringstream oss;
  oss << "{\"stopped\": true, \"timestamp\": " << epoch
      << ", \"previousState\": \"running\"}";
  response = formatSuccess(oss.str());
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeReset(
    std::string& response) {
  auto states = _tracker.getAllStates();
  uint32_t count = static_cast<uint32_t>(states.size());
  _tracker.resetAll();
  std::ostringstream oss;
  oss << "{\"reset\": true, \"shardsReset\": " << count << "}";
  response = formatSuccess(oss.str());
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeFeed(
    std::string const& shardId, uint64_t afterSequence,
    uint32_t maxCount, std::string& response) {
  if (shardId.empty()) {
    response = formatError(400, "missing required parameter: shardId");
    return;
  }
  // Enforce a ceiling on maxCount to prevent memory spikes
  if (maxCount == 0) {
    maxCount = 1000;  // default batch size
  }
  if (maxCount > 10000) {
    maxCount = 10000;  // safety ceiling
  }
  ReplicationFeedStreamT<TailerT> feed(_tailer);
  std::string feedJson = feed.fetch(shardId, afterSequence, maxCount);
  response = formatSuccess(feedJson);
}

template <typename TrackerT, typename TailerT>
void RestReplicationHandlerT<TrackerT, TailerT>::executeCheckpoint(
    std::string const& shardId, uint64_t appliedSequence,
    std::string& response) {
  if (shardId.empty()) {
    response = formatError(400, "missing required field: shardId");
    return;
  }
  if (appliedSequence == 0) {
    response = formatError(400, "appliedSequence must be greater than 0");
    return;
  }
  CheckpointReceiverT<TrackerT> receiver(_tracker);
  auto result = receiver.receive(shardId, appliedSequence);
  if (!result.accepted) {
    response = formatError(409, result.reason);
    return;
  }
  std::ostringstream oss;
  oss << "{\"acknowledged\": true, \"shardId\": \"" << shardId
      << "\", \"appliedSequence\": " << appliedSequence << "}";
  response = formatSuccess(oss.str());
}

template <typename TrackerT, typename TailerT>
std::string RestReplicationHandlerT<TrackerT, TailerT>::formatSuccess(
    std::string const& resultJson) {
  return "{\n  \"error\": false,\n  \"code\": 200,\n  \"result\": " +
         resultJson + "\n}";
}

template <typename TrackerT, typename TailerT>
std::string RestReplicationHandlerT<TrackerT, TailerT>::formatError(
    int code, std::string const& message) {
  std::ostringstream oss;
  oss << "{\n  \"error\": true,\n  \"code\": " << code
      << ",\n  \"errorMessage\": \"" << message << "\"\n}";
  return oss.str();
}

}  // namespace arangodb
