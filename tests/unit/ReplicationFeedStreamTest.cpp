#include <gtest/gtest.h>

#include "ReplicationMocks.h"
#include "Enterprise/Replication/ReplicationFeedStream.h"

#include <string>

namespace arangodb {
namespace test {

using FeedStream = ReplicationFeedStreamT<MockShardWALTailer>;

// ========================================================================
// Empty Batch Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_EmptyBatch_ReturnsEmptyArray) {
  MockShardWALTailer tailer;
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 0, 100);

  EXPECT_NE(json.find("\"messages\": []"), std::string::npos);
  EXPECT_NE(json.find("\"hasMore\": false"), std::string::npos);
  EXPECT_NE(json.find("\"lastSequence\": 0"), std::string::npos);
}

// ========================================================================
// Single Message Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_SingleMessage_ReturnsOneElement) {
  MockShardWALTailer tailer;
  tailer.addMessage({"shard-1", 1, "insert", "doc1"});
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 0, 100);

  EXPECT_NE(json.find("\"shardId\": \"shard-1\""), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"operation\": \"insert\""), std::string::npos);
  EXPECT_NE(json.find("\"payload\": \"doc1\""), std::string::npos);
  EXPECT_NE(json.find("\"hasMore\": false"), std::string::npos);
  EXPECT_NE(json.find("\"lastSequence\": 1"), std::string::npos);
}

// ========================================================================
// Batch Limit Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_BatchLimit_RespectsMaxCount) {
  MockShardWALTailer tailer;
  for (uint64_t i = 1; i <= 10; ++i) {
    tailer.addMessage({"shard-1", i, "insert", "doc" + std::to_string(i)});
  }
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 0, 3);

  EXPECT_NE(json.find("\"hasMore\": true"), std::string::npos);
  // Should contain exactly 3 messages (sequences 1, 2, 3)
  EXPECT_NE(json.find("\"sequence\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 2"), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 3"), std::string::npos);
  EXPECT_EQ(json.find("\"sequence\": 4"), std::string::npos);
  EXPECT_NE(json.find("\"lastSequence\": 3"), std::string::npos);
}

// ========================================================================
// AfterSequence Filtering Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_AfterSequence_FiltersOlderMessages) {
  MockShardWALTailer tailer;
  for (uint64_t i = 1; i <= 5; ++i) {
    tailer.addMessage({"shard-1", i, "update", "doc" + std::to_string(i)});
  }
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 3, 100);

  // Only sequences 4 and 5 should be returned
  EXPECT_EQ(json.find("\"sequence\": 1"), std::string::npos);
  EXPECT_EQ(json.find("\"sequence\": 2"), std::string::npos);
  EXPECT_EQ(json.find("\"sequence\": 3,"), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 4"), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 5"), std::string::npos);
  EXPECT_NE(json.find("\"hasMore\": false"), std::string::npos);
  EXPECT_NE(json.find("\"lastSequence\": 5"), std::string::npos);
}

// ========================================================================
// Message Format Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_MessageFormat_ContainsAllFields) {
  MockShardWALTailer tailer;
  tailer.addMessage({"shard-X", 42, "delete", "payload-data"});
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-X", 0, 10);

  EXPECT_NE(json.find("\"shardId\": \"shard-X\""), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 42"), std::string::npos);
  EXPECT_NE(json.find("\"operation\": \"delete\""), std::string::npos);
  EXPECT_NE(json.find("\"payload\": \"payload-data\""), std::string::npos);
}

// ========================================================================
// Cross-Shard Filtering Tests
// ========================================================================

TEST(ReplicationFeedStream, Fetch_OnlyReturnMessagesForRequestedShard) {
  MockShardWALTailer tailer;
  tailer.addMessage({"shard-1", 1, "insert", "doc-a"});
  tailer.addMessage({"shard-2", 2, "insert", "doc-b"});
  tailer.addMessage({"shard-1", 3, "insert", "doc-c"});
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 0, 100);

  EXPECT_NE(json.find("\"sequence\": 1"), std::string::npos);
  EXPECT_NE(json.find("\"sequence\": 3"), std::string::npos);
  EXPECT_EQ(json.find("\"shard-2\""), std::string::npos);
}

// ========================================================================
// HasMore Edge Case: Exact Count
// ========================================================================

TEST(ReplicationFeedStream, Fetch_ExactCount_HasMoreIsFalse) {
  MockShardWALTailer tailer;
  for (uint64_t i = 1; i <= 5; ++i) {
    tailer.addMessage({"shard-1", i, "insert", "doc"});
  }
  FeedStream feed(tailer);

  std::string json = feed.fetch("shard-1", 0, 5);

  EXPECT_NE(json.find("\"hasMore\": false"), std::string::npos);
}

}  // namespace test
}  // namespace arangodb
