#include <gtest/gtest.h>

#include "ReplicationMocks.h"
#include "Enterprise/Replication/CheckpointReceiver.h"

#include <string>

namespace arangodb {
namespace test {

using Receiver = CheckpointReceiverT<MockSequenceNumberTracker>;

// ========================================================================
// Valid Checkpoint Tests
// ========================================================================

TEST(CheckpointReceiver, Receive_ValidCheckpoint_UpdatesTracker) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  Receiver receiver(tracker);

  auto result = receiver.receive("shard-1", 75);

  EXPECT_TRUE(result.accepted);
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 75u);
}

TEST(CheckpointReceiver, Receive_ValidCheckpoint_ReturnsAccepted) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  Receiver receiver(tracker);

  auto result = receiver.receive("shard-1", 75);

  EXPECT_TRUE(result.accepted);
  EXPECT_TRUE(result.reason.empty());
}

TEST(CheckpointReceiver, Receive_MaxSequence_AcceptsExactMatch) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  Receiver receiver(tracker);

  // appliedSequence == lastGenerated is valid (fully caught up)
  auto result = receiver.receive("shard-1", 100);

  EXPECT_TRUE(result.accepted);
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 100u);
}

// ========================================================================
// Rejection: Sequence Behind Current
// ========================================================================

TEST(CheckpointReceiver, Receive_SequenceBehindCurrent_RejectsWithReason) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 75);
  Receiver receiver(tracker);

  auto result = receiver.receive("shard-1", 50);

  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("not ahead of current lastApplied"),
            std::string::npos);
  // Tracker should not have been modified
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 75u);
}

TEST(CheckpointReceiver, Receive_SequenceEqualToCurrent_RejectsWithReason) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 75);
  Receiver receiver(tracker);

  auto result = receiver.receive("shard-1", 75);

  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("not ahead of current lastApplied"),
            std::string::npos);
}

// ========================================================================
// Rejection: Sequence Ahead of Generated
// ========================================================================

TEST(CheckpointReceiver, Receive_SequenceAheadOfGenerated_RejectsWithReason) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  Receiver receiver(tracker);

  auto result = receiver.receive("shard-1", 150);

  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("exceeds lastGenerated"), std::string::npos);
  // Tracker should not have been modified
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 50u);
}

// ========================================================================
// Rejection: Unknown Shard
// ========================================================================

TEST(CheckpointReceiver, Receive_UnknownShard_RejectsWithReason) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  Receiver receiver(tracker);

  auto result = receiver.receive("unknown-shard", 10);

  EXPECT_FALSE(result.accepted);
  EXPECT_NE(result.reason.find("unknown shard"), std::string::npos);
}

// ========================================================================
// Sequential Checkpoints
// ========================================================================

TEST(CheckpointReceiver, Receive_SequentialCheckpoints_AllAccepted) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 0);
  Receiver receiver(tracker);

  auto r1 = receiver.receive("shard-1", 25);
  EXPECT_TRUE(r1.accepted);
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 25u);

  auto r2 = receiver.receive("shard-1", 50);
  EXPECT_TRUE(r2.accepted);
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 50u);

  auto r3 = receiver.receive("shard-1", 100);
  EXPECT_TRUE(r3.accepted);
  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 100u);
}

// ========================================================================
// Multiple Shards Independent
// ========================================================================

TEST(CheckpointReceiver, Receive_MultipleShards_IndependentTracking) {
  MockSequenceNumberTracker tracker;
  tracker.addShard("shard-1", 100, 50);
  tracker.addShard("shard-2", 200, 100);
  Receiver receiver(tracker);

  auto r1 = receiver.receive("shard-1", 75);
  EXPECT_TRUE(r1.accepted);

  auto r2 = receiver.receive("shard-2", 150);
  EXPECT_TRUE(r2.accepted);

  EXPECT_EQ(tracker.getState("shard-1").lastApplied, 75u);
  EXPECT_EQ(tracker.getState("shard-2").lastApplied, 150u);
}

}  // namespace test
}  // namespace arangodb
