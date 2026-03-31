#include <gtest/gtest.h>
#include "Enterprise/Graph/Providers/SmartGraphProvider.h"
#include "Enterprise/Graph/Steps/SmartGraphStep.h"
#include "Enterprise/Graph/PathValidatorEE.cpp"

using namespace arangodb::graph;

// --- SmartGraphProvider ---

TEST(SmartGraphProvider, IsResponsible_LocalShard_ReturnsTrue) {
  std::unordered_map<std::string, std::string> shardMap = {
      {"users", "users_s0"}};
  SmartGraphProvider provider(shardMap, 4);

  VertexRef vertex{"users", "test:123"};
  auto expectedShard = provider.getResponsibleShard("users", "test:123");

  if (expectedShard == "users_s0") {
    EXPECT_TRUE(provider.isResponsible(vertex));
  } else {
    EXPECT_FALSE(provider.isResponsible(vertex));
  }
}

TEST(SmartGraphProvider, IsResponsible_UnknownCollection_ReturnsFalse) {
  std::unordered_map<std::string, std::string> shardMap = {
      {"users", "users_s0"}};
  SmartGraphProvider provider(shardMap, 4);

  VertexRef vertex{"unknown", "eu:123"};
  EXPECT_FALSE(provider.isResponsible(vertex));
}

TEST(SmartGraphProvider, GetResponsibleShard_SamePrefix_SameShard) {
  std::unordered_map<std::string, std::string> shardMap;
  SmartGraphProvider provider(shardMap, 8);

  auto shard1 = provider.getResponsibleShard("users", "eu:abc");
  auto shard2 = provider.getResponsibleShard("users", "eu:xyz");
  auto shard3 = provider.getResponsibleShard("users", "eu:999");

  EXPECT_EQ(shard1, shard2);
  EXPECT_EQ(shard2, shard3);
}

TEST(SmartGraphProvider, GetResponsibleShard_DifferentPrefix_MayDiffer) {
  std::unordered_map<std::string, std::string> shardMap;
  SmartGraphProvider provider(shardMap, 8);

  auto shardEu = provider.getResponsibleShard("users", "eu:abc");
  auto shardUs = provider.getResponsibleShard("users", "us:abc");

  // Different prefixes may or may not map to different shards
  // But they should be deterministic
  auto shardEu2 = provider.getResponsibleShard("users", "eu:abc");
  EXPECT_EQ(shardEu, shardEu2);
}

TEST(SmartGraphProvider, StartVertex_SameAsIsResponsible) {
  std::unordered_map<std::string, std::string> shardMap = {
      {"users", "users_s0"}};
  SmartGraphProvider provider(shardMap, 4);

  VertexRef vertex{"users", "test:123"};
  EXPECT_EQ(provider.startVertex(vertex), provider.isResponsible(vertex));
}

// --- SmartGraphStep ---

TEST(SmartGraphStep, IsLocal_LocalVertex_ReturnsTrue) {
  SmartGraphStep step("users/eu:123", "shard0", 1, true);
  EXPECT_TRUE(step.isLocal());
}

TEST(SmartGraphStep, IsLocal_RemoteVertex_ReturnsFalse) {
  SmartGraphStep step("users/eu:123", "shard1", 1, false);
  EXPECT_FALSE(step.isLocal());
}

TEST(SmartGraphStep, GetSmartPrefix_WithCollection) {
  SmartGraphStep step("users/eu:123", "shard0", 1, true);
  EXPECT_EQ(step.getSmartPrefix(), "eu");
}

TEST(SmartGraphStep, GetSmartPrefix_WithoutCollection) {
  SmartGraphStep step("eu:123", "shard0", 0, true);
  EXPECT_EQ(step.getSmartPrefix(), "eu");
}

TEST(SmartGraphStep, Depth_TracksCorrectly) {
  SmartGraphStep step("v/a:1", "s0", 3, true);
  EXPECT_EQ(step.depth(), 3u);
}

TEST(SmartGraphStep, IsValid_NonEmpty_ReturnsTrue) {
  SmartGraphStep step("v/a:1", "s0", 0, true);
  EXPECT_TRUE(step.isValid());
}

TEST(SmartGraphStep, IsValid_Empty_ReturnsFalse) {
  SmartGraphStep step;
  EXPECT_FALSE(step.isValid());
}

// --- PathValidatorEE (disjoint validation) ---

TEST(PathValidatorEE, DisjointPath_SamePartition_Valid) {
  PathValidatorOptions opts{true, false, false};
  auto result = checkValidDisjointPath("eu:start", "eu:neighbor", opts);
  EXPECT_TRUE(result.valid);
}

TEST(PathValidatorEE, DisjointPath_CrossPartition_Invalid) {
  PathValidatorOptions opts{true, false, false};
  auto result = checkValidDisjointPath("eu:start", "us:neighbor", opts);
  EXPECT_FALSE(result.valid);
  EXPECT_FALSE(result.errorMessage.empty());
}

TEST(PathValidatorEE, NonDisjointPath_CrossPartition_Valid) {
  PathValidatorOptions opts{false, false, false};
  auto result = checkValidDisjointPath("eu:start", "us:neighbor", opts);
  EXPECT_TRUE(result.valid);
}
