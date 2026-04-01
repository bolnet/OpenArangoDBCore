#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include "Enterprise/RocksDBEngine/KeySpacePartitioner.h"

using arangodb::KeyRange;
using arangodb::KeySpacePartitioner;

// --- Partition_SingleThread_ReturnsFullRange ---
TEST(KeySpacePartitionerTest, Partition_SingleThread_ReturnsFullRange) {
  auto result = KeySpacePartitioner::partition("aaa", "zzz", 1);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("aaa", result[0].lowerBound);
  EXPECT_EQ("zzz", result[0].upperBound);
}

// --- Partition_FourThreads_NonOverlapping ---
TEST(KeySpacePartitionerTest, Partition_FourThreads_NonOverlapping) {
  auto result = KeySpacePartitioner::partition("a", "z", 4);
  ASSERT_EQ(4u, result.size());

  // First partition starts at lower bound
  EXPECT_EQ("a", result[0].lowerBound);

  // Last partition ends at upper bound
  EXPECT_EQ("z", result[3].upperBound);

  // Partitions are contiguous: each upper == next lower
  for (size_t i = 0; i + 1 < result.size(); ++i) {
    EXPECT_EQ(result[i].upperBound, result[i + 1].lowerBound)
        << "Gap between partition " << i << " and " << (i + 1);
  }

  // Partitions are non-overlapping: each lower < upper
  for (size_t i = 0; i < result.size(); ++i) {
    if (!result[i].upperBound.empty()) {
      EXPECT_LT(result[i].lowerBound, result[i].upperBound)
          << "Partition " << i << " has lower >= upper";
    }
  }
}

// --- Partition_EmptyRange_ReturnsEmpty ---
TEST(KeySpacePartitionerTest, Partition_EmptyRange_ReturnsEmpty) {
  // Lower >= upper means empty range
  auto result = KeySpacePartitioner::partition("zzz", "aaa", 4);
  EXPECT_TRUE(result.empty());
}

// --- Partition_ZeroPartitions_ReturnsEmpty ---
TEST(KeySpacePartitionerTest, Partition_ZeroPartitions_ReturnsEmpty) {
  auto result = KeySpacePartitioner::partition("a", "z", 0);
  EXPECT_TRUE(result.empty());
}

// --- Partition_Boundaries_AreInclusive ---
TEST(KeySpacePartitionerTest, Partition_Boundaries_AreInclusive) {
  auto result = KeySpacePartitioner::partition("abc", "xyz", 3);
  ASSERT_EQ(3u, result.size());

  // First partition starts at the exact lower bound
  EXPECT_EQ("abc", result[0].lowerBound);

  // Last partition ends at the exact upper bound
  EXPECT_EQ("xyz", result[2].upperBound);
}

// --- Partition_UnboundedUpper_ProducesPartitions ---
TEST(KeySpacePartitionerTest, Partition_UnboundedUpper_ProducesPartitions) {
  auto result = KeySpacePartitioner::partition("a", "", 3);
  ASSERT_EQ(3u, result.size());

  EXPECT_EQ("a", result[0].lowerBound);
  // Last partition has empty upper bound (unbounded)
  EXPECT_EQ("", result[2].upperBound);

  // Contiguous
  for (size_t i = 0; i + 1 < result.size(); ++i) {
    EXPECT_EQ(result[i].upperBound, result[i + 1].lowerBound);
  }
}

// --- ComputeSplitPoints_BasicSampling ---
TEST(KeySpacePartitionerTest, ComputeSplitPoints_BasicSampling) {
  std::vector<std::string> keys = {"a", "b", "c", "d", "e", "f", "g", "h"};
  auto splits = KeySpacePartitioner::computeSplitPoints(keys, 4);
  ASSERT_EQ(3u, splits.size());

  // Split points should be in sorted order
  for (size_t i = 0; i + 1 < splits.size(); ++i) {
    EXPECT_LE(splits[i], splits[i + 1]);
  }
}

// --- ComputeSplitPoints_SinglePartition_ReturnsEmpty ---
TEST(KeySpacePartitionerTest, ComputeSplitPoints_SinglePartition_ReturnsEmpty) {
  std::vector<std::string> keys = {"a", "b", "c"};
  auto splits = KeySpacePartitioner::computeSplitPoints(keys, 1);
  EXPECT_TRUE(splits.empty());
}

// --- ComputeSplitPoints_EmptyKeys_ReturnsEmpty ---
TEST(KeySpacePartitionerTest, ComputeSplitPoints_EmptyKeys_ReturnsEmpty) {
  std::vector<std::string> keys;
  auto splits = KeySpacePartitioner::computeSplitPoints(keys, 4);
  EXPECT_TRUE(splits.empty());
}
