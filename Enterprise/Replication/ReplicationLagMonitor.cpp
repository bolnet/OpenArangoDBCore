#include "ReplicationLagMonitor.h"

#include <chrono>

namespace arangodb {

ReplicationLagMonitor::ReplicationLagMonitor(
    double lagThresholdSeconds, ClockProvider clock)
    : _lagThresholdSeconds(lagThresholdSeconds),
      _clock(std::move(clock)) {}

void ReplicationLagMonitor::recordEntry(
    std::string const& shardId, uint64_t entryTimestampMicros) {
  std::lock_guard<std::mutex> lock(_mutex);
  auto& ts = _lastTimestamps[shardId];
  if (entryTimestampMicros > ts) {
    ts = entryTimestampMicros;
  }
}

ShardLagInfo ReplicationLagMonitor::getLag(std::string const& shardId) const {
  std::lock_guard<std::mutex> lock(_mutex);

  ShardLagInfo info;
  info.shardId = shardId;
  info.currentTimestamp = now();

  auto it = _lastTimestamps.find(shardId);
  if (it == _lastTimestamps.end()) {
    info.lastEntryTimestamp = 0;
    info.lagSeconds = 0.0;
    info.exceedsThreshold = false;
    return info;
  }

  info.lastEntryTimestamp = it->second;
  double deltaMicros = static_cast<double>(info.currentTimestamp) -
                       static_cast<double>(info.lastEntryTimestamp);
  info.lagSeconds = deltaMicros / 1'000'000.0;
  info.exceedsThreshold = info.lagSeconds > _lagThresholdSeconds;
  return info;
}

std::unordered_map<std::string, ShardLagInfo>
ReplicationLagMonitor::getAllLags() const {
  std::lock_guard<std::mutex> lock(_mutex);

  std::unordered_map<std::string, ShardLagInfo> result;
  uint64_t currentTs = now();

  for (auto const& [shardId, lastTs] : _lastTimestamps) {
    ShardLagInfo info;
    info.shardId = shardId;
    info.lastEntryTimestamp = lastTs;
    info.currentTimestamp = currentTs;
    double deltaMicros = static_cast<double>(currentTs) -
                         static_cast<double>(lastTs);
    info.lagSeconds = deltaMicros / 1'000'000.0;
    info.exceedsThreshold = info.lagSeconds > _lagThresholdSeconds;
    result[shardId] = std::move(info);
  }

  return result;
}

bool ReplicationLagMonitor::anyShardExceedsThreshold() const {
  std::lock_guard<std::mutex> lock(_mutex);
  uint64_t currentTs = now();

  for (auto const& [shardId, lastTs] : _lastTimestamps) {
    double deltaMicros = static_cast<double>(currentTs) -
                         static_cast<double>(lastTs);
    double lagSec = deltaMicros / 1'000'000.0;
    if (lagSec > _lagThresholdSeconds) {
      return true;
    }
  }
  return false;
}

uint64_t ReplicationLagMonitor::now() const {
  if (_clock) {
    return _clock();
  }
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace arangodb
