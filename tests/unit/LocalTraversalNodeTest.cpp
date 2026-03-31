#include <gtest/gtest.h>
#include "Enterprise/Aql/LocalTraversalNode.h"
#include "Enterprise/Aql/LocalEnumeratePathsNode.h"
#include "Enterprise/Aql/LocalShortestPathNode.h"

using namespace arangodb::aql;

// --- LocalTraversalNode ---

TEST(LocalTraversalNode, IsLocalGraphNode_ReturnsTrue) {
  LocalTraversalNode node;
  EXPECT_TRUE(node.isLocalGraphNode());
}

TEST(LocalTraversalNode, GetType_ReturnsTraversal) {
  LocalTraversalNode node;
  EXPECT_EQ(node.getType(), GraphNodeType::TRAVERSAL);
}

TEST(LocalTraversalNode, ToData_WritesLocalFlag) {
  LocalTraversalNode node;
  node.setGraphName("myGraph");
  node.setSmart(true);
  node.setDepth(1, 3);

  auto data = node.toData();
  EXPECT_TRUE(data.isLocalGraphNode);
  EXPECT_EQ(data.nodeType, GraphNodeType::TRAVERSAL);
  EXPECT_EQ(data.graphName, "myGraph");
  EXPECT_TRUE(data.isSmart);
  EXPECT_EQ(data.minDepth, 1u);
  EXPECT_EQ(data.maxDepth, 3u);
}

TEST(LocalTraversalNode, FromData_RoundTrip) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::TRAVERSAL;
  data.isLocalGraphNode = true;
  data.isSmart = true;
  data.isDisjoint = true;
  data.graphName = "smartGraph";
  data.minDepth = 2;
  data.maxDepth = 5;

  auto node = LocalTraversalNode::fromData(data);
  EXPECT_TRUE(node.isLocalGraphNode());
  EXPECT_TRUE(node.isSmart());
  EXPECT_TRUE(node.isDisjoint());
  EXPECT_EQ(node.graphName(), "smartGraph");
  EXPECT_EQ(node.minDepth(), 2u);
  EXPECT_EQ(node.maxDepth(), 5u);
}

TEST(LocalTraversalNode, Clone_PreservesProperties) {
  LocalTraversalNode original;
  original.setGraphName("cloneTest");
  original.setSmart(true);
  original.setDisjoint(true);
  original.setUsedAsSatellite(true);
  original.setDepth(1, 10);
  original.addCollectionToShard("vertices", "shard1");

  auto cloned = original.clone();
  EXPECT_TRUE(cloned.isLocalGraphNode());
  EXPECT_EQ(cloned.graphName(), "cloneTest");
  EXPECT_TRUE(cloned.isSmart());
  EXPECT_TRUE(cloned.isDisjoint());
  EXPECT_TRUE(cloned.isUsedAsSatellite());
  EXPECT_EQ(cloned.minDepth(), 1u);
  EXPECT_EQ(cloned.maxDepth(), 10u);
  EXPECT_EQ(cloned.collectionToShard().at("vertices"), "shard1");
}

TEST(LocalTraversalNode, CollectionToShard_AddAndQuery) {
  LocalTraversalNode node;
  node.addCollectionToShard("users", "shard0");
  node.addCollectionToShard("edges", "shard1");

  auto const& map = node.collectionToShard();
  EXPECT_EQ(map.size(), 2u);
  EXPECT_EQ(map.at("users"), "shard0");
  EXPECT_EQ(map.at("edges"), "shard1");
}

TEST(LocalTraversalNode, MemoryUsage_NonZero) {
  LocalTraversalNode node;
  node.setGraphName("memTest");
  EXPECT_GT(node.getMemoryUsedBytes(), 0u);
}

// --- LocalEnumeratePathsNode ---

TEST(LocalEnumeratePathsNode, IsLocalGraphNode_ReturnsTrue) {
  LocalEnumeratePathsNode node;
  EXPECT_TRUE(node.isLocalGraphNode());
}

TEST(LocalEnumeratePathsNode, GetType_ReturnsEnumeratePaths) {
  LocalEnumeratePathsNode node;
  EXPECT_EQ(node.getType(), GraphNodeType::ENUMERATE_PATHS);
}

TEST(LocalEnumeratePathsNode, RoundTrip) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::ENUMERATE_PATHS;
  data.isLocalGraphNode = true;
  data.graphName = "pathGraph";
  data.isSmart = true;

  auto node = LocalEnumeratePathsNode::fromData(data);
  auto result = node.toData();
  EXPECT_TRUE(result.isLocalGraphNode);
  EXPECT_EQ(result.nodeType, GraphNodeType::ENUMERATE_PATHS);
  EXPECT_EQ(result.graphName, "pathGraph");
}

// --- LocalShortestPathNode ---

TEST(LocalShortestPathNode, IsLocalGraphNode_ReturnsTrue) {
  LocalShortestPathNode node;
  EXPECT_TRUE(node.isLocalGraphNode());
}

TEST(LocalShortestPathNode, GetType_ReturnsShortestPath) {
  LocalShortestPathNode node;
  EXPECT_EQ(node.getType(), GraphNodeType::SHORTEST_PATH);
}

TEST(LocalShortestPathNode, RoundTrip) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::SHORTEST_PATH;
  data.isLocalGraphNode = true;
  data.graphName = "shortGraph";

  auto node = LocalShortestPathNode::fromData(data);
  EXPECT_TRUE(node.isLocalGraphNode());
  EXPECT_EQ(node.graphName(), "shortGraph");
}

// --- createLocalGraphNode factory ---

TEST(CreateLocalGraphNode, TraversalType_ReturnsTraversal) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::TRAVERSAL;
  data.graphName = "factoryTest";

  auto result = createLocalGraphNode(data);
  EXPECT_EQ(result.type, CreateLocalGraphNodeResult::TraversalNode);
  EXPECT_TRUE(result.traversal.isLocalGraphNode());
  EXPECT_EQ(result.traversal.graphName(), "factoryTest");
}

TEST(CreateLocalGraphNode, EnumeratePathsType_ReturnsEnumeratePaths) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::ENUMERATE_PATHS;

  auto result = createLocalGraphNode(data);
  EXPECT_EQ(result.type, CreateLocalGraphNodeResult::EnumeratePathsNode);
  EXPECT_TRUE(result.enumeratePaths.isLocalGraphNode());
}

TEST(CreateLocalGraphNode, ShortestPathType_ReturnsShortestPath) {
  LocalGraphNodeData data;
  data.nodeType = GraphNodeType::SHORTEST_PATH;

  auto result = createLocalGraphNode(data);
  EXPECT_EQ(result.type, CreateLocalGraphNodeResult::ShortestPathNode);
  EXPECT_TRUE(result.shortestPath.isLocalGraphNode());
}

TEST(CreateLocalGraphNode, UnknownType_ReturnsError) {
  LocalGraphNodeData data;
  data.nodeType = static_cast<GraphNodeType>(999);

  auto result = createLocalGraphNode(data);
  EXPECT_EQ(result.type, CreateLocalGraphNodeResult::Error);
}
