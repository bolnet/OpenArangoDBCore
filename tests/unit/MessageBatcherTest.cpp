#include "Enterprise/Replication/MessageBatcher.h"

#include <gtest/gtest.h>

#include <atomic>
#include <string>
#include <vector>

using namespace arangodb;

namespace {

/// Helper: create a simple WALEntry.
WALEntry makeEntry(uint64_t seq, std::string key = "doc1",
                   WALEntry::Operation op = WALEntry::Operation::kInsert) {
  WALEntry e;
  e.sequenceNumber = seq;
  e.timestamp = 1000000 + seq;
  e.collectionName = "test_col";
  e.documentKey = std::move(key);
  e.operation = op;
  e.payload = "{}";
  return e;
}

/// Simple sequence provider: returns incrementing values starting at 1.
class TestSequenceProvider {
 public:
  SequenceProvider provider() {
    return [this]() -> uint64_t {
      return ++_counter;
    };
  }

  uint64_t current() const { return _counter; }

 private:
  uint64_t _counter = 0;
};

}  // namespace

// Test 9: Adding entries below threshold does not produce a message
TEST(MessageBatcherTest, AccumulatesUpToBatchSize) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 5, seqProvider.provider());

  // Add 4 entries (batch size = 5), none should trigger a batch
  for (uint64_t i = 1; i <= 4; ++i) {
    auto result = batcher.add(makeEntry(i));
    EXPECT_FALSE(result.has_value());
  }

  EXPECT_EQ(batcher.pendingCount(), 4u);
}

// Test 10: Adding the Nth entry (N = batchSize) triggers a complete batch
TEST(MessageBatcherTest, FlushesAtBatchSize) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 3, seqProvider.provider());

  auto r1 = batcher.add(makeEntry(1));
  EXPECT_FALSE(r1.has_value());

  auto r2 = batcher.add(makeEntry(2));
  EXPECT_FALSE(r2.has_value());

  // Third entry should trigger the batch
  auto r3 = batcher.add(makeEntry(3));
  ASSERT_TRUE(r3.has_value());
  EXPECT_EQ(r3->entries.size(), 3u);
  EXPECT_EQ(batcher.pendingCount(), 0u);
}

// Test 11: flush() returns a message with all pending entries
TEST(MessageBatcherTest, ManualFlush_ReturnsPendingEntries) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 100, seqProvider.provider());

  batcher.add(makeEntry(1, "a"));
  batcher.add(makeEntry(2, "b"));

  auto result = batcher.flush();
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->entries.size(), 2u);
  EXPECT_EQ(result->entries[0].documentKey, "a");
  EXPECT_EQ(result->entries[1].documentKey, "b");
  EXPECT_EQ(batcher.pendingCount(), 0u);
}

// Test 12: flush() on empty batcher returns nullopt
TEST(MessageBatcherTest, ManualFlush_EmptyBatcher_ReturnsNullopt) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 100, seqProvider.provider());

  auto result = batcher.flush();
  EXPECT_FALSE(result.has_value());
}

// Test 13: Output MessageBatch has the configured shard ID
TEST(MessageBatcherTest, BatchContainsCorrectShardId) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_42", 2, seqProvider.provider());

  batcher.add(makeEntry(1));
  auto result = batcher.add(makeEntry(2));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->shardId, "shard_42");
}

// Test 14: Each batch receives a monotonic sequence number
TEST(MessageBatcherTest, BatchTaggedWithSequenceNumber) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 1, seqProvider.provider());

  auto result = batcher.add(makeEntry(1));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->sequenceNumber, 1u);
}

// Test 15: Successive batches have strictly increasing sequence numbers
TEST(MessageBatcherTest, MultipleBatches_IncreasingSequences) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 2, seqProvider.provider());

  batcher.add(makeEntry(1));
  auto batch1 = batcher.add(makeEntry(2));

  batcher.add(makeEntry(3));
  auto batch2 = batcher.add(makeEntry(4));

  batcher.add(makeEntry(5));
  auto batch3 = batcher.add(makeEntry(6));

  ASSERT_TRUE(batch1.has_value());
  ASSERT_TRUE(batch2.has_value());
  ASSERT_TRUE(batch3.has_value());

  EXPECT_LT(batch1->sequenceNumber, batch2->sequenceNumber);
  EXPECT_LT(batch2->sequenceNumber, batch3->sequenceNumber);
}

// Test 16: After reset(), pending entries are discarded
TEST(MessageBatcherTest, Reset_ClearsPendingEntries) {
  TestSequenceProvider seqProvider;
  MessageBatcher batcher("shard_0", 100, seqProvider.provider());

  batcher.add(makeEntry(1));
  batcher.add(makeEntry(2));
  EXPECT_EQ(batcher.pendingCount(), 2u);

  batcher.reset();
  EXPECT_EQ(batcher.pendingCount(), 0u);

  // flush after reset should return nullopt
  auto result = batcher.flush();
  EXPECT_FALSE(result.has_value());
}
