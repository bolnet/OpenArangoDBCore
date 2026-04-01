#include <gtest/gtest.h>

#include "ReplicationMocks.h"
#include "Enterprise/Replication/ReplicationStatusBuilder.h"

#include <string>

namespace arangodb {
namespace test {

using Builder =
    ReplicationStatusBuilderT<MockSequenceNumberTracker, MockShardWALTailer>;

// ========================================================================
// Empty State Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_NoShards_ReturnsEmptyStatus) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Builder builder(tracker, tailer);

  std::string json = builder.build();

  EXPECT_NE(json.find("\"isRunning\": false"), std::string::npos);
  EXPECT_NE(json.find("\"shardStates\": []"), std::string::npos);
  EXPECT_NE(json.find("\"totalPending\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"totalMessagesSent\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"totalMessagesAcked\": 0"), std::string::npos);
  EXPECT_NE(json.find("\"lagSeconds\": 0"), std::string::npos);
}

// ========================================================================
// Single Shard Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_SingleShard_IncludesShardState) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-A", 100, 80);  // 20 pending
  MockShardWALTailer tailer;
  tailer.setSent(50);
  tailer.setAcked(40);
  Builder builder(tracker, tailer);

  std::string json = builder.build();

  EXPECT_NE(json.find("\"shardId\": \"shard-A\""), std::string::npos);
  EXPECT_NE(json.find("\"lastGenerated\": 100"), std::string::npos);
  EXPECT_NE(json.find("\"lastApplied\": 80"), std::string::npos);
  EXPECT_NE(json.find("\"pending\": 20"), std::string::npos);
  EXPECT_NE(json.find("\"totalPending\": 20"), std::string::npos);
}

// ========================================================================
// Multiple Shard Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_MultipleShards_AggregatesAll) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 90);  // 10 pending
  tracker.addShard("shard-2", 200, 150); // 50 pending
  tracker.addShard("shard-3", 300, 280); // 20 pending
  MockShardWALTailer tailer;
  tailer.setSent(100);
  tailer.setAcked(80);
  Builder builder(tracker, tailer);

  std::string json = builder.build();

  // Total pending = 10 + 50 + 20 = 80
  EXPECT_NE(json.find("\"totalPending\": 80"), std::string::npos);
  // All three shards should appear
  EXPECT_NE(json.find("\"shardId\": \"shard-1\""), std::string::npos);
  EXPECT_NE(json.find("\"shardId\": \"shard-2\""), std::string::npos);
  EXPECT_NE(json.find("\"shardId\": \"shard-3\""), std::string::npos);
}

// ========================================================================
// Lag Estimation Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_LagSeconds_ComputedFromMaxPending) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 90);   // 10 pending
  tracker.addShard("shard-2", 200, 100);  // 100 pending (max)
  MockShardWALTailer tailer;
  tailer.setSent(50);
  tailer.setAcked(50);
  Builder builder(tracker, tailer);

  std::string json = builder.build();

  // With sent=50, acked=50, ratio=1.0, lag = maxPending * ratio = 100
  EXPECT_NE(json.find("\"lagSeconds\": 100"), std::string::npos);
}

TEST(ReplicationStatusBuilder,
     EstimateLagSeconds_ZeroPending_ReturnsZero) {
  EXPECT_DOUBLE_EQ(Builder::estimateLagSeconds(0, 100, 50), 0.0);
}

TEST(ReplicationStatusBuilder,
     EstimateLagSeconds_NoMessagesSent_ReturnsMaxPending) {
  EXPECT_DOUBLE_EQ(Builder::estimateLagSeconds(42, 0, 0), 42.0);
}

TEST(ReplicationStatusBuilder,
     EstimateLagSeconds_NoMessagesAcked_ReturnsMaxPending) {
  EXPECT_DOUBLE_EQ(Builder::estimateLagSeconds(42, 100, 0), 42.0);
}

TEST(ReplicationStatusBuilder,
     EstimateLagSeconds_NormalRatio_ComputesCorrectly) {
  // sent=100, acked=50, ratio=2.0, lag=20*2.0=40
  EXPECT_DOUBLE_EQ(Builder::estimateLagSeconds(20, 100, 50), 40.0);
}

// ========================================================================
// Running State Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_IsRunning_ReflectsTailerState_Stopped) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  Builder builder(tracker, tailer);

  std::string json = builder.build();
  EXPECT_NE(json.find("\"isRunning\": false"), std::string::npos);
}

TEST(ReplicationStatusBuilder, Build_IsRunning_ReflectsTailerState_Running) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  tailer.start();
  Builder builder(tracker, tailer);

  std::string json = builder.build();
  EXPECT_NE(json.find("\"isRunning\": true"), std::string::npos);
}

// ========================================================================
// Message Count Tests
// ========================================================================

TEST(ReplicationStatusBuilder, Build_MessageCounts_FromTailer) {
  MockSequenceNumberTracker tracker;
  MockShardWALTailer tailer;
  tailer.setSent(1234);
  tailer.setAcked(1000);
  Builder builder(tracker, tailer);

  std::string json = builder.build();
  EXPECT_NE(json.find("\"totalMessagesSent\": 1234"), std::string::npos);
  EXPECT_NE(json.find("\"totalMessagesAcked\": 1000"), std::string::npos);
}

}  // namespace test
}  // namespace arangodb
