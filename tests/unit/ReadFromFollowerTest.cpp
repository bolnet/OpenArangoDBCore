#include <gtest/gtest.h>
#include "Enterprise/Cluster/ReadFromFollower.h"
#include <set>
#include <algorithm>

using namespace arangodb;

// --- isDirtyReadAllowed ---

TEST(ReadFromFollower, IsDirtyReadAllowed_HeaderPresent_ReturnsTrue) {
  std::unordered_map<std::string, std::string> headers = {
    {"x-arango-allow-dirty-read", "true"}
  };
  EXPECT_TRUE(ReadFromFollower::isDirtyReadAllowed(headers));
}

TEST(ReadFromFollower, IsDirtyReadAllowed_HeaderMissing_ReturnsFalse) {
  std::unordered_map<std::string, std::string> headers = {
    {"content-type", "application/json"}
  };
  EXPECT_FALSE(ReadFromFollower::isDirtyReadAllowed(headers));
}

TEST(ReadFromFollower, IsDirtyReadAllowed_HeaderFalse_ReturnsFalse) {
  std::unordered_map<std::string, std::string> headers = {
    {"x-arango-allow-dirty-read", "false"}
  };
  EXPECT_FALSE(ReadFromFollower::isDirtyReadAllowed(headers));
}

TEST(ReadFromFollower, IsDirtyReadAllowed_CaseInsensitiveValue) {
  std::unordered_map<std::string, std::string> headers = {
    {"x-arango-allow-dirty-read", "True"}
  };
  EXPECT_TRUE(ReadFromFollower::isDirtyReadAllowed(headers));
}

// --- chooseReplica ---

TEST(ReadFromFollower, ChooseReplica_NoFollowers_ReturnsLeader) {
  auto selection = ReadFromFollower::chooseReplica("leader1", {});
  EXPECT_EQ(selection.serverId, "leader1");
  EXPECT_FALSE(selection.isFollower);
}

TEST(ReadFromFollower, ChooseReplica_WithFollowers_RoundRobin) {
  std::vector<std::string> followers = {"follower1", "follower2"};
  std::set<std::string> seen;

  // Call enough times to see all replicas (leader + 2 followers = 3)
  for (int i = 0; i < 30; ++i) {
    auto selection = ReadFromFollower::chooseReplica("leader1", followers);
    seen.insert(selection.serverId);
  }

  // Should have seen leader and both followers
  EXPECT_TRUE(seen.count("leader1") > 0);
  EXPECT_TRUE(seen.count("follower1") > 0);
  EXPECT_TRUE(seen.count("follower2") > 0);
}

TEST(ReadFromFollower, ChooseReplica_SingleFollower_Distributes) {
  std::vector<std::string> followers = {"follower1"};
  int leaderCount = 0;
  int followerCount = 0;

  for (int i = 0; i < 100; ++i) {
    auto selection = ReadFromFollower::chooseReplica("leader1", followers);
    if (selection.isFollower) {
      ++followerCount;
    } else {
      ++leaderCount;
    }
  }

  // Should be roughly 50/50
  EXPECT_GT(leaderCount, 30);
  EXPECT_GT(followerCount, 30);
}

// --- getResponsibleServersReadFromFollower ---

TEST(ReadFromFollower, GetServers_DistributesAcrossReplicas) {
  std::unordered_set<std::string> shards = {"shard1", "shard2", "shard3"};
  std::unordered_map<std::string, std::string> leaders = {
    {"shard1", "server1"}, {"shard2", "server2"}, {"shard3", "server3"}
  };
  std::unordered_map<std::string, std::vector<std::string>> followers = {
    {"shard1", {"server2", "server3"}},
    {"shard2", {"server1", "server3"}},
    {"shard3", {"server1", "server2"}}
  };

  std::unordered_map<std::string, std::string> result;
  ReadFromFollower::getResponsibleServersReadFromFollower(
      shards, result, leaders, followers);

  EXPECT_EQ(result.size(), 3u);
  // Each shard should have a server assigned
  EXPECT_TRUE(result.count("shard1") > 0);
  EXPECT_TRUE(result.count("shard2") > 0);
  EXPECT_TRUE(result.count("shard3") > 0);
}

TEST(ReadFromFollower, GetServers_UnknownShard_Skipped) {
  std::unordered_set<std::string> shards = {"shard1", "unknownShard"};
  std::unordered_map<std::string, std::string> leaders = {{"shard1", "server1"}};
  std::unordered_map<std::string, std::vector<std::string>> followers = {
    {"shard1", {"server2"}}
  };

  std::unordered_map<std::string, std::string> result;
  ReadFromFollower::getResponsibleServersReadFromFollower(
      shards, result, leaders, followers);

  EXPECT_EQ(result.size(), 1u);
  EXPECT_TRUE(result.count("shard1") > 0);
  EXPECT_EQ(result.count("unknownShard"), 0u);
}

// --- isFollowerEligible ---

TEST(ReadFromFollower, IsFollowerEligible_EventualMode_AlwaysTrue) {
  StalenessConfig config{StalenessConfig::Mode::Eventual, std::chrono::milliseconds{0}};
  EXPECT_TRUE(ReadFromFollower::isFollowerEligible(std::chrono::milliseconds{999999}, config));
}

TEST(ReadFromFollower, IsFollowerEligible_BoundedMode_WithinThreshold) {
  StalenessConfig config{StalenessConfig::Mode::Bounded, std::chrono::milliseconds{1000}};
  EXPECT_TRUE(ReadFromFollower::isFollowerEligible(std::chrono::milliseconds{500}, config));
}

TEST(ReadFromFollower, IsFollowerEligible_BoundedMode_ExceedsThreshold) {
  StalenessConfig config{StalenessConfig::Mode::Bounded, std::chrono::milliseconds{1000}};
  EXPECT_FALSE(ReadFromFollower::isFollowerEligible(std::chrono::milliseconds{1500}, config));
}

TEST(ReadFromFollower, IsFollowerEligible_BoundedMode_ZeroMaxStaleness) {
  StalenessConfig config{StalenessConfig::Mode::Bounded, std::chrono::milliseconds{0}};
  EXPECT_FALSE(ReadFromFollower::isFollowerEligible(std::chrono::milliseconds{0}, config));
}

TEST(ReadFromFollower, IsFollowerEligible_BoundedMode_ExactThreshold) {
  StalenessConfig config{StalenessConfig::Mode::Bounded, std::chrono::milliseconds{1000}};
  EXPECT_TRUE(ReadFromFollower::isFollowerEligible(std::chrono::milliseconds{1000}, config));
}

// --- setDirtyReadResponseHeader ---

TEST(ReadFromFollower, SetResponseHeader_FollowerUsed_SetsHeader) {
  std::unordered_map<std::string, std::string> headers;
  ReadFromFollower::setDirtyReadResponseHeader(headers, true);
  EXPECT_EQ(headers["x-arango-potential-dirty-read"], "true");
}

TEST(ReadFromFollower, SetResponseHeader_LeaderUsed_NoHeader) {
  std::unordered_map<std::string, std::string> headers;
  ReadFromFollower::setDirtyReadResponseHeader(headers, false);
  EXPECT_TRUE(headers.empty());
}
