#pragma once
#ifndef ARANGODB_READ_FROM_FOLLOWER_H
#define ARANGODB_READ_FROM_FOLLOWER_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace arangodb {

/// Configuration for staleness guarantees when reading from followers.
struct StalenessConfig {
  enum class Mode {
    Eventual,   // Accept any follower regardless of lag
    Bounded     // Only accept followers within maxStaleness
  };

  Mode mode = Mode::Eventual;
  std::chrono::milliseconds maxStaleness{0};  // 0 = eventual
};

/// Result of choosing a replica for a read operation.
struct ReplicaSelection {
  std::string serverId;
  bool isFollower;  // true if selected server is not the leader
};

/// ReadFromFollower enables horizontal read scaling by routing read queries
/// to follower shard replicas when clients opt in via the
/// X-Arango-Allow-Dirty-Read HTTP header.
///
/// Integration point in ArangoDB:
///   ClusterInfo::getResponsibleServersReadFromFollower() — enterprise override
///   TransactionState::chooseReplicasNolock() — calls enterprise routing
class ReadFromFollower {
 public:
  /// Check if dirty read is allowed based on request headers.
  /// Looks for "X-Arango-Allow-Dirty-Read: true" (case-insensitive value).
  static bool isDirtyReadAllowed(
      std::unordered_map<std::string, std::string> const& headers);

  /// Choose a replica (leader or follower) for a given shard using round-robin.
  /// Returns the selected server and whether it's a follower.
  /// If followers is empty, always returns the leader.
  static ReplicaSelection chooseReplica(
      std::string const& leader,
      std::vector<std::string> const& followers);

  /// Map each shard to a selected server, distributing reads across replicas.
  /// When dirty reads are enabled, uses round-robin across leader+followers.
  /// When disabled, always maps to leader.
  static void getResponsibleServersReadFromFollower(
      std::unordered_set<std::string> const& shards,
      std::unordered_map<std::string, std::string>& result,
      std::unordered_map<std::string, std::string> const& shardLeaderMap,
      std::unordered_map<std::string, std::vector<std::string>> const& shardFollowersMap);

  /// Check if a follower is eligible given staleness configuration.
  /// In Eventual mode: always returns true.
  /// In Bounded mode: checks reportedLag against config.maxStaleness.
  static bool isFollowerEligible(
      std::chrono::milliseconds reportedLag,
      StalenessConfig const& config);

  /// Set the dirty-read response header if a follower was used.
  /// Adds "X-Arango-Potential-Dirty-Read: true" to response headers.
  static void setDirtyReadResponseHeader(
      std::unordered_map<std::string, std::string>& responseHeaders,
      bool followerUsed);

  /// HTTP header constants
  static constexpr char const* kAllowDirtyRead = "x-arango-allow-dirty-read";
  static constexpr char const* kPotentialDirtyRead = "x-arango-potential-dirty-read";

 private:
  static std::atomic<uint64_t> _roundRobinCounter;
};

}  // namespace arangodb

#endif  // ARANGODB_READ_FROM_FOLLOWER_H
