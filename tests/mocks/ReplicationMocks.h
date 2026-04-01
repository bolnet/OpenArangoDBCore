#pragma once
#ifndef ARANGODB_TEST_REPLICATION_MOCKS_H
#define ARANGODB_TEST_REPLICATION_MOCKS_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace arangodb {

/// Per-shard sequence state used by REST handler and status builder.
/// This struct is shared between mocks and production code.
struct ShardSequenceState {
  std::string shardId;
  uint64_t lastGenerated = 0;
  uint64_t lastApplied = 0;
  uint64_t pendingCount = 0;
};

/// Replication message exposed via the feed stream endpoint.
struct ReplicationMessage {
  std::string shardId;
  uint64_t sequence = 0;
  std::string operation;
  std::string payload;
};

/// Mock SequenceNumberTracker for REST handler, status builder,
/// and checkpoint receiver unit tests.
class MockSequenceNumberTracker {
 public:
  void addShard(std::string const& shardId, uint64_t generated,
                uint64_t applied) {
    _states[shardId] = ShardSequenceState{
        shardId, generated, applied,
        (generated >= applied) ? (generated - applied) : 0};
  }

  ShardSequenceState getState(std::string const& shardId) const {
    auto it = _states.find(shardId);
    if (it != _states.end()) {
      return it->second;
    }
    return ShardSequenceState{"", 0, 0, 0};
  }

  std::vector<ShardSequenceState> getAllStates() const {
    std::vector<ShardSequenceState> result;
    result.reserve(_states.size());
    for (auto const& [k, v] : _states) {
      result.push_back(v);
    }
    return result;
  }

  void updateApplied(std::string const& shardId, uint64_t sequence) {
    auto it = _states.find(shardId);
    if (it != _states.end()) {
      it->second.lastApplied = sequence;
      it->second.pendingCount =
          (it->second.lastGenerated >= sequence)
              ? (it->second.lastGenerated - sequence)
              : 0;
    }
  }

  void resetAll() { _states.clear(); }

  uint64_t totalPending() const {
    uint64_t total = 0;
    for (auto const& [k, v] : _states) {
      total += v.pendingCount;
    }
    return total;
  }

 private:
  std::unordered_map<std::string, ShardSequenceState> _states;
};

/// Mock ShardWALTailer for REST handler and feed stream tests.
class MockShardWALTailer {
 public:
  bool isRunning() const { return _running; }
  void start() { _running = true; }
  void stop() { _running = false; }

  void addMessage(ReplicationMessage msg) {
    _messages.push_back(std::move(msg));
    _sent++;
  }

  std::vector<ReplicationMessage> nextBatch(
      std::string const& shardId, uint64_t afterSequence,
      uint32_t maxCount) const {
    std::vector<ReplicationMessage> result;
    for (auto const& m : _messages) {
      if (m.shardId == shardId && m.sequence > afterSequence) {
        result.push_back(m);
        if (result.size() >= maxCount) {
          break;
        }
      }
    }
    return result;
  }

  uint64_t messagesSent() const { return _sent; }
  uint64_t messagesAcked() const { return _acked; }
  void setAcked(uint64_t n) { _acked = n; }
  void setSent(uint64_t n) { _sent = n; }

 private:
  bool _running = false;
  std::vector<ReplicationMessage> _messages;
  uint64_t _sent = 0;
  uint64_t _acked = 0;
};

}  // namespace arangodb

#endif  // ARANGODB_TEST_REPLICATION_MOCKS_H
