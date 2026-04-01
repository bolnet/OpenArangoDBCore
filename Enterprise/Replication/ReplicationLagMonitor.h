#pragma once
#ifndef ARANGODB_REPLICATION_LAG_MONITOR_H
#define ARANGODB_REPLICATION_LAG_MONITOR_H

#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace arangodb {

/// Per-shard lag snapshot.
struct ShardLagInfo {
  std::string shardId;
  uint64_t lastEntryTimestamp = 0;  // Microseconds since epoch
  uint64_t currentTimestamp = 0;    // Microseconds since epoch
  double lagSeconds = 0.0;         // (current - lastEntry) in seconds
  bool exceedsThreshold = false;
};

/// Callback to obtain current time (injectable for testing).
using ClockProvider = std::function<uint64_t()>;

/// ReplicationLagMonitor tracks the time delta between WAL entry
/// timestamps and the current wall clock for each shard.
class ReplicationLagMonitor {
 public:
  /// @param lagThresholdSeconds  Alert threshold (default 30s from config)
  /// @param clock                Injectable clock (defaults to system clock)
  explicit ReplicationLagMonitor(
      double lagThresholdSeconds = 30.0,
      ClockProvider clock = nullptr);

  /// Record that a WAL entry with the given timestamp was processed for a shard.
  void recordEntry(std::string const& shardId, uint64_t entryTimestampMicros);

  /// Get the current lag info for a specific shard.
  ShardLagInfo getLag(std::string const& shardId) const;

  /// Get lag info for all tracked shards.
  std::unordered_map<std::string, ShardLagInfo> getAllLags() const;

  /// Check if any shard exceeds the lag threshold.
  bool anyShardExceedsThreshold() const;

 private:
  uint64_t now() const;

  double _lagThresholdSeconds;
  ClockProvider _clock;
  mutable std::mutex _mutex;
  std::unordered_map<std::string, uint64_t> _lastTimestamps;  // per-shard
};

}  // namespace arangodb

#endif  // ARANGODB_REPLICATION_LAG_MONITOR_H
