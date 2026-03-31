#include <gtest/gtest.h>
#include "Enterprise/Cluster/SatelliteDistribution.h"

using namespace arangodb;

// --- SatelliteDistribution static methods ---

TEST(SatelliteDistribution, IsSatellite_ReplicationFactorZero_ReturnsTrue) {
  EXPECT_TRUE(SatelliteDistribution::isSatellite(0));
}

TEST(SatelliteDistribution, IsSatellite_ReplicationFactorNonZero_ReturnsFalse) {
  EXPECT_FALSE(SatelliteDistribution::isSatellite(1));
  EXPECT_FALSE(SatelliteDistribution::isSatellite(3));
  EXPECT_FALSE(SatelliteDistribution::isSatellite(100));
}

TEST(SatelliteDistribution, SatelliteReplicationFactor_ReturnsZero) {
  EXPECT_EQ(SatelliteDistribution::satelliteReplicationFactor(), 0u);
}

TEST(SatelliteDistribution, SatelliteNumberOfShards_ReturnsOne) {
  EXPECT_EQ(SatelliteDistribution::satelliteNumberOfShards(), 1u);
}

TEST(SatelliteDistribution, EffectiveWriteConcern_EqualsServerCount) {
  EXPECT_EQ(SatelliteDistribution::effectiveWriteConcern(1), 1u);
  EXPECT_EQ(SatelliteDistribution::effectiveWriteConcern(3), 3u);
  EXPECT_EQ(SatelliteDistribution::effectiveWriteConcern(5), 5u);
}

TEST(SatelliteDistribution, AssignShards_AllServersIncluded) {
  std::vector<std::string> servers = {"srv1", "srv2", "srv3", "srv4", "srv5"};
  auto assignment = SatelliteDistribution::assignShards(servers, "s", 42);

  EXPECT_EQ(assignment.shardId, "s42");
  EXPECT_EQ(assignment.leader, "srv1");
  EXPECT_EQ(assignment.followers.size(), 4u);
  EXPECT_EQ(assignment.followers[0], "srv2");
  EXPECT_EQ(assignment.followers[1], "srv3");
  EXPECT_EQ(assignment.followers[2], "srv4");
  EXPECT_EQ(assignment.followers[3], "srv5");
}

TEST(SatelliteDistribution, AssignShards_SingleServer) {
  std::vector<std::string> servers = {"srv1"};
  auto assignment = SatelliteDistribution::assignShards(servers);

  EXPECT_EQ(assignment.leader, "srv1");
  EXPECT_TRUE(assignment.followers.empty());
}

TEST(SatelliteDistribution, AssignShards_EmptyServers) {
  std::vector<std::string> servers;
  auto assignment = SatelliteDistribution::assignShards(servers);

  EXPECT_TRUE(assignment.leader.empty());
  EXPECT_TRUE(assignment.followers.empty());
}

TEST(SatelliteDistribution, ExtractCollectionName_WithSlash) {
  EXPECT_EQ(SatelliteDistribution::extractCollectionName("users/12345"), "users");
}

TEST(SatelliteDistribution, ExtractCollectionName_WithoutSlash) {
  EXPECT_EQ(SatelliteDistribution::extractCollectionName("users"), "users");
}

TEST(SatelliteDistribution, ExtractCollectionName_MultipleSlashes) {
  EXPECT_EQ(SatelliteDistribution::extractCollectionName("users/sub/key"), "users");
}

// --- SmartToSat edge detection ---

TEST(SatelliteDistribution, IsSmartToSatEdge_OneSideSatellite_ReturnsTrue) {
  SatelliteCollectionRegistry registry;
  registry.registerSatellite("lookup");

  EXPECT_TRUE(SatelliteDistribution::isSmartToSatEdge("smartColl", "lookup", registry));
  EXPECT_TRUE(SatelliteDistribution::isSmartToSatEdge("lookup", "smartColl", registry));
}

TEST(SatelliteDistribution, IsSmartToSatEdge_BothSmart_ReturnsFalse) {
  SatelliteCollectionRegistry registry;
  EXPECT_FALSE(SatelliteDistribution::isSmartToSatEdge("smart1", "smart2", registry));
}

TEST(SatelliteDistribution, IsSmartToSatEdge_BothSatellite_ReturnsFalse) {
  SatelliteCollectionRegistry registry;
  registry.registerSatellite("sat1");
  registry.registerSatellite("sat2");
  EXPECT_FALSE(SatelliteDistribution::isSmartToSatEdge("sat1", "sat2", registry));
}

// --- SatelliteCollectionRegistry ---

TEST(SatelliteCollectionRegistry, RegisterAndQuery) {
  SatelliteCollectionRegistry registry;
  EXPECT_FALSE(registry.isSatelliteCollection("lookup"));

  registry.registerSatellite("lookup");
  EXPECT_TRUE(registry.isSatelliteCollection("lookup"));
  EXPECT_FALSE(registry.isSatelliteCollection("other"));
}

TEST(SatelliteCollectionRegistry, UnregisterRemoves) {
  SatelliteCollectionRegistry registry;
  registry.registerSatellite("lookup");
  EXPECT_TRUE(registry.isSatelliteCollection("lookup"));

  registry.unregisterSatellite("lookup");
  EXPECT_FALSE(registry.isSatelliteCollection("lookup"));
}

TEST(SatelliteCollectionRegistry, All_ReturnsAllRegistered) {
  SatelliteCollectionRegistry registry;
  registry.registerSatellite("a");
  registry.registerSatellite("b");
  registry.registerSatellite("c");

  auto const& all = registry.all();
  EXPECT_EQ(all.size(), 3u);
  EXPECT_TRUE(all.count("a") > 0);
  EXPECT_TRUE(all.count("b") > 0);
  EXPECT_TRUE(all.count("c") > 0);
}

TEST(SatelliteCollectionRegistry, Clear_RemovesAll) {
  SatelliteCollectionRegistry registry;
  registry.registerSatellite("a");
  registry.registerSatellite("b");
  registry.clear();

  EXPECT_TRUE(registry.all().empty());
}

// --- Constants ---

TEST(SatelliteDistribution, Constants_AreCorrect) {
  EXPECT_EQ(SatelliteDistribution::kSatelliteReplicationFactor, 0u);
  EXPECT_EQ(SatelliteDistribution::kSatelliteNumberOfShards, 1u);
  EXPECT_EQ(SatelliteDistribution::kSatelliteWriteConcern, 0u);
}
