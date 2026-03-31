#include <gtest/gtest.h>

#include <set>
#include <string_view>

#include "Enterprise/Sharding/ShardingStrategyEE.h"
#include "ShardingMocks.h"

using namespace arangodb;
using namespace arangodb::test;

// --- Strategy name tests ---

TEST(ShardingStrategyEE, EnterpriseHashSmartEdge_Name) {
  EnterpriseHashSmartEdgeShardingStrategy strategy;
  EXPECT_EQ(strategy.name(), "enterprise-hash-smart-edge");
}

TEST(ShardingStrategyEE, EnterpriseHexSmartVertex_Name) {
  EnterpriseHexSmartVertexShardingStrategy strategy;
  EXPECT_EQ(strategy.name(), "enterprise-hex-smart-vertex");
}

TEST(ShardingStrategyEE, EnterpriseSmartEdgeCompat_Name) {
  EnterpriseSmartEdgeCompatShardingStrategy strategy;
  EXPECT_EQ(strategy.name(), "enterprise-smart-edge-compat");
}

// --- usesDefaultShardKeys tests ---

TEST(ShardingStrategyEE, AllStrategies_UsesDefaultShardKeys_ReturnsFalse) {
  EnterpriseHashSmartEdgeShardingStrategy edge;
  EnterpriseHexSmartVertexShardingStrategy vertex;
  EnterpriseSmartEdgeCompatShardingStrategy compat;

  EXPECT_FALSE(edge.usesDefaultShardKeys());
  EXPECT_FALSE(vertex.usesDefaultShardKeys());
  EXPECT_FALSE(compat.usesDefaultShardKeys());
}

// --- extractSmartPrefix tests ---

TEST(ShardingStrategyEE, ExtractSmartPrefix_WithColon) {
  auto prefix = extractSmartPrefix("eu:12345");
  EXPECT_EQ(prefix, "eu");
}

TEST(ShardingStrategyEE, ExtractSmartPrefix_WithoutColon) {
  auto prefix = extractSmartPrefix("12345");
  EXPECT_EQ(prefix, "12345");
}

TEST(ShardingStrategyEE, ExtractSmartPrefix_MultipleColons) {
  auto prefix = extractSmartPrefix("eu:us:12345");
  EXPECT_EQ(prefix, "eu");
}

// --- computeShardIndex tests ---

TEST(ShardingStrategyEE, ComputeShardIndex_Deterministic) {
  uint32_t shards = 8;
  auto shard1 = computeShardIndex("eu", shards);
  auto shard2 = computeShardIndex("eu", shards);
  EXPECT_EQ(shard1, shard2);
  EXPECT_LT(shard1, shards);
}

TEST(ShardingStrategyEE, ComputeShardIndex_Distribution) {
  uint32_t shards = 16;
  std::set<uint32_t> seen;
  // Hash a bunch of different prefixes and expect at least some distribution
  for (int i = 0; i < 100; ++i) {
    auto idx = computeShardIndex("prefix" + std::to_string(i), shards);
    EXPECT_LT(idx, shards);
    seen.insert(idx);
  }
  // With 100 inputs across 16 shards, we should hit many of them
  EXPECT_GT(seen.size(), 8u);
}

// --- getResponsibleShard tests ---

TEST(ShardingStrategyEE, EnterpriseHashSmartEdge_SameFromPrefix_SameShard) {
  EnterpriseHashSmartEdgeShardingStrategy strategy;
  uint32_t shards = 8;
  // Both edge keys have "eu" as the first prefix
  auto shard1 = strategy.getResponsibleShard("eu:us:1", shards);
  auto shard2 = strategy.getResponsibleShard("eu:de:2", shards);
  EXPECT_EQ(shard1, shard2);
  EXPECT_LT(shard1, shards);
}

TEST(ShardingStrategyEE, EnterpriseHexSmartVertex_SamePrefix_SameShard) {
  EnterpriseHexSmartVertexShardingStrategy strategy;
  uint32_t shards = 8;
  auto shard1 = strategy.getResponsibleShard("eu:abc", shards);
  auto shard2 = strategy.getResponsibleShard("eu:xyz", shards);
  EXPECT_EQ(shard1, shard2);
  EXPECT_LT(shard1, shards);
}
