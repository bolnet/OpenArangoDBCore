#include "ReadFromFollower.h"

#include <algorithm>
#include <cctype>

namespace arangodb {

std::atomic<uint64_t> ReadFromFollower::_roundRobinCounter{0};

bool ReadFromFollower::isDirtyReadAllowed(
    std::unordered_map<std::string, std::string> const& headers) {
  auto it = headers.find(kAllowDirtyRead);
  if (it == headers.end()) {
    return false;
  }
  // Case-insensitive comparison of value to "true"
  auto const& val = it->second;
  if (val.size() != 4) return false;
  return (std::tolower(static_cast<unsigned char>(val[0])) == 't' &&
          std::tolower(static_cast<unsigned char>(val[1])) == 'r' &&
          std::tolower(static_cast<unsigned char>(val[2])) == 'u' &&
          std::tolower(static_cast<unsigned char>(val[3])) == 'e');
}

ReplicaSelection ReadFromFollower::chooseReplica(
    std::string const& leader,
    std::vector<std::string> const& followers) {
  if (followers.empty()) {
    return {leader, false};
  }

  // Round-robin across leader (index 0) + followers (index 1..N)
  uint64_t const totalReplicas = 1 + followers.size();
  uint64_t const index = _roundRobinCounter.fetch_add(1, std::memory_order_relaxed) % totalReplicas;

  if (index == 0) {
    return {leader, false};
  }
  return {followers[index - 1], true};
}

void ReadFromFollower::getResponsibleServersReadFromFollower(
    std::unordered_set<std::string> const& shards,
    std::unordered_map<std::string, std::string>& result,
    std::unordered_map<std::string, std::string> const& shardLeaderMap,
    std::unordered_map<std::string, std::vector<std::string>> const& shardFollowersMap) {
  result.clear();
  for (auto const& shard : shards) {
    auto leaderIt = shardLeaderMap.find(shard);
    if (leaderIt == shardLeaderMap.end()) {
      continue;  // Unknown shard, skip
    }

    auto followersIt = shardFollowersMap.find(shard);
    std::vector<std::string> const emptyFollowers;
    auto const& followers = (followersIt != shardFollowersMap.end())
        ? followersIt->second
        : emptyFollowers;

    auto selection = chooseReplica(leaderIt->second, followers);
    result[shard] = std::move(selection.serverId);
  }
}

bool ReadFromFollower::isFollowerEligible(
    std::chrono::milliseconds reportedLag,
    StalenessConfig const& config) {
  if (config.mode == StalenessConfig::Mode::Eventual) {
    return true;
  }
  // Bounded mode: check lag against threshold
  // If maxStaleness is 0 in bounded mode, no follower is eligible
  if (config.maxStaleness.count() == 0) {
    return false;
  }
  return reportedLag <= config.maxStaleness;
}

void ReadFromFollower::setDirtyReadResponseHeader(
    std::unordered_map<std::string, std::string>& responseHeaders,
    bool followerUsed) {
  if (followerUsed) {
    responseHeaders[kPotentialDirtyRead] = "true";
  }
}

}  // namespace arangodb
