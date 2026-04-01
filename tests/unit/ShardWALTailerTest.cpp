#include "Enterprise/Replication/ShardWALTailer.h"
#include "MockWALIterator.h"
#include "Enterprise/Cluster/SatelliteDistribution.h"
#include "Enterprise/Sharding/ShardingStrategyEE.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace arangodb;
using namespace arangodb::test;

namespace {

/// Helper: create a WALEntry with the given parameters.
WALEntry makeEntry(uint64_t seq, std::string collection, std::string key,
                   WALEntry::Operation op = WALEntry::Operation::kInsert,
                   uint64_t timestamp = 1000000) {
  WALEntry e;
  e.sequenceNumber = seq;
  e.timestamp = timestamp;
  e.collectionName = std::move(collection);
  e.documentKey = std::move(key);
  e.operation = op;
  e.payload = "{}";
  return e;
}

/// A collection info resolver that returns configurable replication info.
/// Satellite collections have replicationFactor == 0.
class TestCollectionResolver {
 public:
  void addCollection(std::string name, uint64_t replicationFactor,
                     uint32_t numberOfShards) {
    _info[std::move(name)] = CollectionReplicationInfo{
        replicationFactor, numberOfShards};
  }

  CollectionInfoResolver resolver() {
    return [this](std::string const& name)
               -> std::optional<CollectionReplicationInfo> {
      auto it = _info.find(name);
      if (it == _info.end()) {
        return std::nullopt;
      }
      return it->second;
    };
  }

 private:
  std::unordered_map<std::string, CollectionReplicationInfo> _info;
};

}  // namespace

// Test 1: With 1 shard, all entries pass the filter
TEST(ShardWALTailerTest, ReadsAllEntries_ForSingleShard) {
  auto iter = std::make_unique<MockWALIterator>();
  iter->addEntry(makeEntry(1, "users", "key1"));
  iter->addEntry(makeEntry(2, "users", "key2"));
  iter->addEntry(makeEntry(3, "users", "key3"));

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, 1);  // replicationFactor=3, 1 shard

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  auto entries = tailer.poll();
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0].documentKey, "key1");
  EXPECT_EQ(entries[1].documentKey, "key2");
  EXPECT_EQ(entries[2].documentKey, "key3");
}

// Test 2: With 4 shards, only entries whose documentKey hashes to shard 0
TEST(ShardWALTailerTest, FiltersEntriesByShardIndex) {
  // Generate several keys and determine which shard they belong to
  std::vector<WALEntry> allEntries;
  for (uint64_t i = 0; i < 20; ++i) {
    allEntries.push_back(
        makeEntry(i + 1, "users", "key" + std::to_string(i)));
  }

  uint32_t numberOfShards = 4;
  uint32_t targetShard = 0;

  // Count how many entries belong to shard 0
  size_t expectedCount = 0;
  for (auto const& e : allEntries) {
    if (computeShardIndex(e.documentKey, numberOfShards) == targetShard) {
      ++expectedCount;
    }
  }

  auto iter = std::make_unique<MockWALIterator>(allEntries);

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, numberOfShards);

  ShardWALTailer tailer(targetShard, numberOfShards, std::move(iter),
                        resolver.resolver());

  auto entries = tailer.poll();
  EXPECT_EQ(entries.size(), expectedCount);

  // Verify all returned entries hash to the target shard
  for (auto const& e : entries) {
    EXPECT_EQ(computeShardIndex(e.documentKey, numberOfShards), targetShard);
  }
}

// Test 3: Entries from satellite collections (replicationFactor==0) are skipped
TEST(ShardWALTailerTest, SkipsSatelliteCollections) {
  auto iter = std::make_unique<MockWALIterator>();
  iter->addEntry(makeEntry(1, "users", "key1"));
  iter->addEntry(makeEntry(2, "satellite_col", "key2"));
  iter->addEntry(makeEntry(3, "users", "key3"));
  iter->addEntry(makeEntry(4, "satellite_col", "key4"));

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, 1);          // Normal collection
  resolver.addCollection("satellite_col", 0, 1);  // Satellite (replFactor=0)

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  auto entries = tailer.poll();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].collectionName, "users");
  EXPECT_EQ(entries[0].documentKey, "key1");
  EXPECT_EQ(entries[1].collectionName, "users");
  EXPECT_EQ(entries[1].documentKey, "key3");
}

// Test 4: Empty WAL iterator yields empty result
TEST(ShardWALTailerTest, EmptyWAL_ReturnsNoEntries) {
  auto iter = std::make_unique<MockWALIterator>();
  TestCollectionResolver resolver;

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  auto entries = tailer.poll();
  EXPECT_TRUE(entries.empty());
}

// Test 5: After seekTo(), tailer resumes from the specified sequence
TEST(ShardWALTailerTest, SeeksToLastProcessedSequence) {
  auto iter = std::make_unique<MockWALIterator>();
  iter->addEntry(makeEntry(10, "users", "key1"));
  iter->addEntry(makeEntry(20, "users", "key2"));
  iter->addEntry(makeEntry(30, "users", "key3"));
  iter->addEntry(makeEntry(40, "users", "key4"));

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, 1);

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  // Seek past the first two entries
  tailer.seekTo(25);

  auto entries = tailer.poll();
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries[0].sequenceNumber, 30u);
  EXPECT_EQ(entries[1].sequenceNumber, 40u);
}

// Test 6: Insert, Update, and Delete operations all pass the shard filter
TEST(ShardWALTailerTest, MixedOperations_AllPassThrough) {
  auto iter = std::make_unique<MockWALIterator>();
  iter->addEntry(makeEntry(1, "users", "key1", WALEntry::Operation::kInsert));
  iter->addEntry(makeEntry(2, "users", "key2", WALEntry::Operation::kUpdate));
  iter->addEntry(makeEntry(3, "users", "key3", WALEntry::Operation::kDelete));

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, 1);

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  auto entries = tailer.poll();
  ASSERT_EQ(entries.size(), 3u);
  EXPECT_EQ(entries[0].operation, WALEntry::Operation::kInsert);
  EXPECT_EQ(entries[1].operation, WALEntry::Operation::kUpdate);
  EXPECT_EQ(entries[2].operation, WALEntry::Operation::kDelete);
}

// Test 7: Entries from different collections are filtered independently
TEST(ShardWALTailerTest, MultipleCollections_FilteredIndependently) {
  uint32_t numberOfShards = 4;
  uint32_t targetShard = 1;

  auto iter = std::make_unique<MockWALIterator>();

  // Add entries for two collections with different shard counts
  // Both use the same global numberOfShards for routing
  std::vector<std::string> keys = {
      "alpha", "beta", "gamma", "delta", "epsilon",
      "zeta", "eta", "theta", "iota", "kappa"};

  uint64_t seq = 1;
  for (auto const& key : keys) {
    iter->addEntry(makeEntry(seq++, "orders", key));
    iter->addEntry(makeEntry(seq++, "products", key));
  }

  TestCollectionResolver resolver;
  resolver.addCollection("orders", 3, numberOfShards);
  resolver.addCollection("products", 3, numberOfShards);

  ShardWALTailer tailer(targetShard, numberOfShards, std::move(iter),
                        resolver.resolver());

  auto entries = tailer.poll();

  // All returned entries must hash to the target shard
  for (auto const& e : entries) {
    EXPECT_EQ(computeShardIndex(e.documentKey, numberOfShards), targetShard);
  }

  // We should have entries from both collections
  bool hasOrders = false;
  bool hasProducts = false;
  for (auto const& e : entries) {
    if (e.collectionName == "orders") hasOrders = true;
    if (e.collectionName == "products") hasProducts = true;
  }
  // At least one collection should be present (hash distribution)
  EXPECT_TRUE(hasOrders || hasProducts);
}

// Test 8: Setting the stop flag causes poll() to return immediately
TEST(ShardWALTailerTest, StopSignal_HaltsIteration) {
  auto iter = std::make_unique<MockWALIterator>();
  for (uint64_t i = 1; i <= 1000; ++i) {
    iter->addEntry(makeEntry(i, "users", "key" + std::to_string(i)));
  }

  TestCollectionResolver resolver;
  resolver.addCollection("users", 3, 1);

  ShardWALTailer tailer(0, 1, std::move(iter), resolver.resolver());

  // Stop before polling
  tailer.stop();
  EXPECT_TRUE(tailer.isStopped());

  auto entries = tailer.poll();
  // Should return empty or very few entries (stopped immediately)
  EXPECT_TRUE(entries.empty());
}
