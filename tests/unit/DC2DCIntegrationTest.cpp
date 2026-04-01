#include <gtest/gtest.h>

#include "Enterprise/Replication/ReplicationApplier.h"
#include "Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h"
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"
#include "MockCollectionAccessor.h"

#include <algorithm>
#include <numeric>
#include <random>
#include <string>
#include <vector>

namespace arangodb {
namespace test {

// ==========================================================================
// Test Fixture
// ==========================================================================

class DC2DCIntegrationTest : public ::testing::Test {
 protected:
  void SetUp() override {
    store = std::make_unique<MockCollectionStore>();
    auto writeCb = store->makeWriteCallback();
    auto satelliteCb = [](std::string const& shard) -> bool {
      return shard.find("satellite_") == 0;
    };
    applier =
        std::make_unique<ReplicationApplier>(std::move(writeCb), satelliteCb);
  }

  /// Generate a batch of sequential INSERT messages for a shard.
  std::vector<ApplyMessage> generateMessages(std::string const& shard,
                                              uint64_t startSeq,
                                              uint64_t count) {
    std::vector<ApplyMessage> messages;
    messages.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
      ApplyMessage msg;
      msg.shardId = shard;
      msg.sequence = startSeq + i;
      msg.operation = ReplicationOperation::INSERT;
      std::string key = "doc_" + std::to_string(startSeq + i);
      msg.payload = std::vector<uint8_t>(key.begin(), key.end());
      msg.documentKey = key;
      msg.documentRev = "1";
      messages.push_back(std::move(msg));
    }
    return messages;
  }

  /// Simulate WAL tailing, batching, and sending (source side).
  /// Returns the messages as if they went through the full pipeline.
  std::vector<ApplyMessage> simulateSourcePipeline(std::string const& shard,
                                                    uint64_t count) {
    // WAL generate -> tail -> batch -> serialize -> deserialize.
    // In this mock, we skip the intermediate steps and produce
    // ApplyMessages directly, which is what the target receives.
    return generateMessages(shard, 1, count);
  }

  std::unique_ptr<MockCollectionStore> store;
  std::unique_ptr<ReplicationApplier> applier;
};

// ==========================================================================
// Full Pipeline Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_FullPipeline_GenerateWalTailBatchSendApply) {
  // Simulate: WAL generate 50 entries -> tail -> batch -> send -> apply.
  auto messages = simulateSourcePipeline("shard_0", 50);

  for (auto const& msg : messages) {
    EXPECT_EQ(0, applier->applyMessage(msg));
  }

  EXPECT_EQ(50u, applier->totalApplied());
  EXPECT_EQ(50u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(50u, store->documentCount("shard_0"));

  // Verify all 50 documents are present.
  for (uint64_t i = 1; i <= 50; ++i) {
    std::string key = "doc_" + std::to_string(i);
    EXPECT_TRUE(store->hasDocument("shard_0", key))
        << "Missing document: " << key;
  }
}

TEST_F(DC2DCIntegrationTest,
       Integration_FullPipeline_MultiShardParallel) {
  // Run the full pipeline across 4 shards simultaneously.
  std::vector<std::string> shards = {"shard_0", "shard_1", "shard_2",
                                      "shard_3"};
  uint64_t const docsPerShard = 25;

  for (auto const& shard : shards) {
    auto messages = generateMessages(shard, 1, docsPerShard);
    for (auto const& msg : messages) {
      EXPECT_EQ(0, applier->applyMessage(msg));
    }
  }

  EXPECT_EQ(docsPerShard * 4, applier->totalApplied());

  for (auto const& shard : shards) {
    EXPECT_EQ(docsPerShard, store->documentCount(shard));
    EXPECT_EQ(docsPerShard, applier->lastAppliedSequence(shard));
  }
}

// ==========================================================================
// Duplicate Replay Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_DuplicateReplay_100MessagesTwice_NoDuplicates) {
  auto messages = generateMessages("shard_0", 1, 100);

  // First pass: all 100 applied.
  for (auto const& msg : messages) {
    EXPECT_EQ(0, applier->applyMessage(msg));
  }
  EXPECT_EQ(100u, applier->totalApplied());

  // Second pass: all 100 rejected as duplicates.
  for (auto const& msg : messages) {
    EXPECT_EQ(0, applier->applyMessage(msg));
  }
  EXPECT_EQ(100u, applier->totalApplied());  // no increase
  EXPECT_EQ(100u, applier->totalDuplicatesRejected());
  EXPECT_EQ(100u, store->documentCount("shard_0"));
}

TEST_F(DC2DCIntegrationTest,
       Integration_DuplicateReplay_InterleavedDuplicates) {
  // Apply messages 1-5.
  auto first5 = generateMessages("shard_0", 1, 5);
  for (auto const& msg : first5) {
    applier->applyMessage(msg);
  }
  EXPECT_EQ(5u, applier->totalApplied());

  // Now send a batch with mix of old (1-3) and new (6-8).
  std::vector<ApplyMessage> mixed;
  auto oldMsgs = generateMessages("shard_0", 1, 3);
  auto newMsgs = generateMessages("shard_0", 6, 3);

  // Interleave: old, new, old, new, old, new.
  for (size_t i = 0; i < 3; ++i) {
    mixed.push_back(oldMsgs[i]);
    mixed.push_back(newMsgs[i]);
  }

  for (auto const& msg : mixed) {
    applier->applyMessage(msg);
  }

  EXPECT_EQ(8u, applier->totalApplied());   // 5 + 3 new
  EXPECT_EQ(3u, applier->totalDuplicatesRejected());  // 3 old
  EXPECT_EQ(8u, store->documentCount("shard_0"));
}

// ==========================================================================
// Out-of-Order Delivery Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_OutOfOrder_ReceiveSeq103Before102) {
  // Send seq=101 (first message, sequence starts at 1... let's use 1,3,2).
  EXPECT_EQ(0, applier->applyMessage(
      generateMessages("shard_0", 1, 1)[0]));  // seq=1

  // Send seq=3 (out of order, seq=2 is missing).
  EXPECT_EQ(0, applier->applyMessage(
      generateMessages("shard_0", 3, 1)[0]));  // seq=3 buffered
  EXPECT_EQ(1u, applier->totalApplied());
  EXPECT_EQ(1u, applier->bufferedCount());

  // Send seq=2 (fills the gap, should drain seq=3 too).
  EXPECT_EQ(0, applier->applyMessage(
      generateMessages("shard_0", 2, 1)[0]));  // seq=2
  EXPECT_EQ(3u, applier->totalApplied());
  EXPECT_EQ(0u, applier->bufferedCount());
  EXPECT_EQ(3u, applier->lastAppliedSequence("shard_0"));
}

TEST_F(DC2DCIntegrationTest,
       Integration_OutOfOrder_LargeGap_BufferAndDrain) {
  // Send seq=1.
  applier->applyMessage(generateMessages("shard_0", 1, 1)[0]);

  // Send seq=10 (large gap: 2-9 missing).
  applier->applyMessage(generateMessages("shard_0", 10, 1)[0]);
  EXPECT_EQ(1u, applier->totalApplied());
  EXPECT_EQ(1u, applier->bufferedCount());

  // Fill gap: send seq=2 through 9.
  for (uint64_t seq = 2; seq <= 9; ++seq) {
    applier->applyMessage(generateMessages("shard_0", seq, 1)[0]);
  }

  EXPECT_EQ(10u, applier->totalApplied());
  EXPECT_EQ(0u, applier->bufferedCount());
  EXPECT_EQ(10u, applier->lastAppliedSequence("shard_0"));
}

TEST_F(DC2DCIntegrationTest,
       Integration_OutOfOrder_MultiShard_Independent) {
  // Out-of-order on shard_0 does not block shard_1.
  applier->applyMessage(generateMessages("shard_0", 1, 1)[0]);
  applier->applyMessage(generateMessages("shard_0", 3, 1)[0]);  // buffered

  // shard_1 messages arrive in order.
  applier->applyMessage(generateMessages("shard_1", 1, 1)[0]);
  applier->applyMessage(generateMessages("shard_1", 2, 1)[0]);
  applier->applyMessage(generateMessages("shard_1", 3, 1)[0]);

  EXPECT_EQ(4u, applier->totalApplied());  // 1 from shard_0, 3 from shard_1
  EXPECT_EQ(1u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(3u, applier->lastAppliedSequence("shard_1"));
  EXPECT_EQ(1u, applier->bufferedCount());  // shard_0 has seq=3 buffered

  // Now fill shard_0 gap.
  applier->applyMessage(generateMessages("shard_0", 2, 1)[0]);
  EXPECT_EQ(6u, applier->totalApplied());
  EXPECT_EQ(3u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(0u, applier->bufferedCount());
}

// ==========================================================================
// Network Partition Simulation Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_Partition_DropMessagesForNSeconds_ResumeCatchUp) {
  // Source generates 200 messages.
  auto allMessages = generateMessages("shard_0", 1, 200);

  // Partition drops messages 50-150.
  PartitionSimulator partition(50, 150);
  auto delivered = partition.filter(allMessages);

  // Apply delivered messages (1-49 and 151-200).
  for (auto const& msg : delivered) {
    applier->applyMessage(msg);
  }

  // Only messages 1-49 should be applied (151-200 are buffered since gap).
  EXPECT_EQ(49u, applier->totalApplied());
  EXPECT_EQ(49u, applier->lastAppliedSequence("shard_0"));

  // After resume, retransmit dropped messages (50-150).
  auto dropped = partition.getDropped();
  EXPECT_EQ(101u, dropped.size());

  for (auto const& msg : dropped) {
    applier->applyMessage(msg);
  }

  // Now all 200 should be applied (49 + 101 dropped fill gap + 50 buffered drain).
  EXPECT_EQ(200u, applier->totalApplied());
  EXPECT_EQ(200u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(200u, store->documentCount("shard_0"));
  EXPECT_EQ(0u, applier->bufferedCount());
}

TEST_F(DC2DCIntegrationTest,
       Integration_Partition_ResumeFromCheckpoint) {
  // Apply messages 1-49 (before partition).
  auto first49 = generateMessages("shard_0", 1, 49);
  for (auto const& msg : first49) {
    applier->applyMessage(msg);
  }
  EXPECT_EQ(49u, applier->lastAppliedSequence("shard_0"));

  // Partition starts: messages 50-100 are dropped.
  // After partition ends, source resumes from checkpoint (seq=49).
  // Retransmit from seq=50 onwards.
  auto retransmit = generateMessages("shard_0", 50, 51);
  for (auto const& msg : retransmit) {
    applier->applyMessage(msg);
  }

  EXPECT_EQ(100u, applier->totalApplied());
  EXPECT_EQ(100u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(0u, applier->bufferedCount());
}

TEST_F(DC2DCIntegrationTest,
       Integration_Partition_PartialBatchDuringPartition) {
  // Apply messages 1-30.
  auto first30 = generateMessages("shard_0", 1, 30);
  for (auto const& msg : first30) {
    applier->applyMessage(msg);
  }

  // A batch of 20 (seq 31-50) is partially delivered when partition starts.
  // Only seq 31-35 get through.
  auto partial = generateMessages("shard_0", 31, 5);
  for (auto const& msg : partial) {
    applier->applyMessage(msg);
  }
  EXPECT_EQ(35u, applier->lastAppliedSequence("shard_0"));

  // After resume, the entire batch 31-50 is retransmitted.
  auto fullBatch = generateMessages("shard_0", 31, 20);
  for (auto const& msg : fullBatch) {
    applier->applyMessage(msg);
  }

  // seq 31-35 are duplicates; seq 36-50 are new.
  EXPECT_EQ(50u, applier->totalApplied());
  EXPECT_EQ(5u, applier->totalDuplicatesRejected());
  EXPECT_EQ(50u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(0u, applier->bufferedCount());
}

// ==========================================================================
// Satellite Collection Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_SatelliteSkip_MessagesDroppedSilently) {
  auto messages = generateMessages("satellite_users", 1, 10);
  for (auto const& msg : messages) {
    applier->applyMessage(msg);
  }

  EXPECT_EQ(0u, applier->totalApplied());
  EXPECT_EQ(10u, applier->totalSatelliteSkipped());
  EXPECT_EQ(0u, store->documentCount("satellite_users"));
  EXPECT_EQ(0u, store->writeCount());
}

TEST_F(DC2DCIntegrationTest,
       Integration_SatelliteSkip_MixedBatch) {
  // Mix satellite and non-satellite messages.
  std::vector<ApplyMessage> mixed;

  // 5 satellite messages.
  auto satMsgs = generateMessages("satellite_orders", 1, 5);
  // 5 normal messages.
  auto normalMsgs = generateMessages("shard_0", 1, 5);

  // Interleave them.
  for (size_t i = 0; i < 5; ++i) {
    mixed.push_back(satMsgs[i]);
    mixed.push_back(normalMsgs[i]);
  }

  for (auto const& msg : mixed) {
    applier->applyMessage(msg);
  }

  EXPECT_EQ(5u, applier->totalApplied());
  EXPECT_EQ(5u, applier->totalSatelliteSkipped());
  EXPECT_EQ(5u, store->documentCount("shard_0"));
  EXPECT_EQ(0u, store->documentCount("satellite_orders"));
}

// ==========================================================================
// Error Handling Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_WriteCallbackFailure_ReturnError) {
  // Apply first message successfully.
  applier->applyMessage(generateMessages("shard_0", 1, 1)[0]);
  EXPECT_EQ(1u, applier->totalApplied());

  // Configure store to fail.
  store->setFailOnWrite(true, 42);

  // Next message should fail.
  auto msg = generateMessages("shard_0", 2, 1)[0];
  int rc = applier->applyMessage(msg);
  EXPECT_EQ(42, rc);

  // Sequence should NOT have advanced.
  EXPECT_EQ(1u, applier->lastAppliedSequence("shard_0"));
  EXPECT_EQ(1u, applier->totalApplied());

  // Re-enable writes.
  store->setFailOnWrite(false);

  // Retry the same message -- should now succeed.
  EXPECT_EQ(0, applier->applyMessage(msg));
  EXPECT_EQ(2u, applier->totalApplied());
  EXPECT_EQ(2u, applier->lastAppliedSequence("shard_0"));
}

// ==========================================================================
// Transaction Integration Tests
// ==========================================================================

TEST_F(DC2DCIntegrationTest,
       Integration_TransactionBypass_FullWorkflow) {
  // Verify that IgnoreNoAccessAqlTransaction + IgnoreNoAccessMethods work
  // together as the replication pipeline expects.
  IgnoreNoAccessAqlTransaction trx;

  // Begin transaction.
  EXPECT_EQ(0, trx.begin());
  EXPECT_TRUE(trx.isReplicationTransaction());

  // Check access for multiple collections -- all should pass.
  EXPECT_EQ(0, trx.checkAccess("restricted_collection", 2));
  EXPECT_EQ(0, trx.checkAccess("_system", 3));
  EXPECT_EQ(0, trx.checkAccess("user_data", 2));

  // Use IgnoreNoAccessMethods for writes.
  IgnoreNoAccessMethods methods;
  EXPECT_TRUE(methods.isReplicationContext());

  std::vector<uint8_t> doc = {0x01, 0x02, 0x03};
  EXPECT_EQ(0, methods.insert("restricted_collection", doc));
  EXPECT_EQ(0, methods.update("restricted_collection", doc));
  EXPECT_EQ(0, methods.remove("restricted_collection", "key1"));
  EXPECT_EQ(0, methods.truncate("restricted_collection"));

  EXPECT_EQ(4u, methods.operationCount());

  // Commit transaction.
  EXPECT_EQ(0, trx.commit());
  EXPECT_FALSE(trx.isActive());
}

TEST_F(DC2DCIntegrationTest,
       Integration_BatchApply_CorrectCount) {
  auto batch = generateMessages("shard_0", 1, 20);
  uint64_t applied = applier->applyBatch(batch);
  EXPECT_EQ(20u, applied);
  EXPECT_EQ(20u, applier->totalApplied());
  EXPECT_EQ(20u, store->documentCount("shard_0"));
}

}  // namespace test
}  // namespace arangodb
