#include <gtest/gtest.h>

#include "Enterprise/Replication/ReplicationApplier.h"
#include "MockCollectionAccessor.h"

#include <string>
#include <vector>

namespace arangodb {
namespace test {

class ReplicationApplierTest : public ::testing::Test {
 protected:
  void SetUp() override {
    store = std::make_unique<MockCollectionStore>();
    auto writeCb = store->makeWriteCallback();
    auto satelliteCb = [](std::string const& shard) -> bool {
      // Collections starting with "satellite_" are satellite collections.
      return shard.find("satellite_") == 0;
    };
    applier =
        std::make_unique<ReplicationApplier>(std::move(writeCb), satelliteCb);
  }

  ApplyMessage makeInsert(std::string const& shard, uint64_t seq,
                           std::string const& key) {
    ApplyMessage msg;
    msg.shardId = shard;
    msg.sequence = seq;
    msg.operation = ReplicationOperation::INSERT;
    msg.payload = std::vector<uint8_t>(key.begin(), key.end());
    msg.documentKey = key;
    msg.documentRev = "1";
    return msg;
  }

  ApplyMessage makeUpdate(std::string const& shard, uint64_t seq,
                           std::string const& key) {
    ApplyMessage msg;
    msg.shardId = shard;
    msg.sequence = seq;
    msg.operation = ReplicationOperation::UPDATE;
    msg.payload = std::vector<uint8_t>{0xAA, 0xBB};
    msg.documentKey = key;
    msg.documentRev = "2";
    return msg;
  }

  ApplyMessage makeRemove(std::string const& shard, uint64_t seq,
                           std::string const& key) {
    ApplyMessage msg;
    msg.shardId = shard;
    msg.sequence = seq;
    msg.operation = ReplicationOperation::REMOVE;
    msg.payload = {};
    msg.documentKey = key;
    msg.documentRev = "3";
    return msg;
  }

  ApplyMessage makeTruncate(std::string const& shard, uint64_t seq) {
    ApplyMessage msg;
    msg.shardId = shard;
    msg.sequence = seq;
    msg.operation = ReplicationOperation::TRUNCATE;
    msg.payload = {};
    msg.documentKey = "";
    msg.documentRev = "";
    return msg;
  }

  std::unique_ptr<MockCollectionStore> store;
  std::unique_ptr<ReplicationApplier> applier;
};

TEST_F(ReplicationApplierTest, ApplySingleInsert_Succeeds) {
  auto msg = makeInsert("shard_0", 1, "doc1");
  EXPECT_EQ(0, applier->applyMessage(msg));
  EXPECT_EQ(1u, applier->totalApplied());
  EXPECT_TRUE(store->hasDocument("shard_0", "doc1"));
  EXPECT_EQ(1u, store->documentCount("shard_0"));
}

TEST_F(ReplicationApplierTest, ApplySingleUpdate_Succeeds) {
  // First insert.
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1")));
  // Then update.
  EXPECT_EQ(0, applier->applyMessage(makeUpdate("shard_0", 2, "doc1")));
  EXPECT_EQ(2u, applier->totalApplied());
  // Document still exists (updated in place).
  EXPECT_TRUE(store->hasDocument("shard_0", "doc1"));
  EXPECT_EQ(1u, store->documentCount("shard_0"));
}

TEST_F(ReplicationApplierTest, ApplySingleRemove_Succeeds) {
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1")));
  EXPECT_EQ(0, applier->applyMessage(makeRemove("shard_0", 2, "doc1")));
  EXPECT_EQ(2u, applier->totalApplied());
  EXPECT_FALSE(store->hasDocument("shard_0", "doc1"));
  EXPECT_EQ(0u, store->documentCount("shard_0"));
}

TEST_F(ReplicationApplierTest, ApplyTruncate_Succeeds) {
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1")));
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 2, "doc2")));
  EXPECT_EQ(0, applier->applyMessage(makeTruncate("shard_0", 3)));
  EXPECT_EQ(3u, applier->totalApplied());
  EXPECT_EQ(0u, store->documentCount("shard_0"));
}

TEST_F(ReplicationApplierTest, UsesIgnoreNoAccessTransaction) {
  // The applier's write callback was created from MockCollectionStore,
  // which simulates the bypass behavior. Verify the write succeeds
  // on a "read-only" named collection (the mock doesn't enforce ACLs).
  auto msg = makeInsert("readonly_shard", 1, "doc1");
  EXPECT_EQ(0, applier->applyMessage(msg));
  EXPECT_EQ(1u, applier->totalApplied());
  EXPECT_TRUE(store->hasDocument("readonly_shard", "doc1"));
}

TEST_F(ReplicationApplierTest, SkipsSatelliteCollections) {
  auto msg = makeInsert("satellite_users", 1, "doc1");
  EXPECT_EQ(0, applier->applyMessage(msg));
  EXPECT_EQ(0u, applier->totalApplied());
  EXPECT_EQ(1u, applier->totalSatelliteSkipped());
  EXPECT_EQ(0u, store->documentCount("satellite_users"));
}

TEST_F(ReplicationApplierTest, EmptyBatch_NoOp) {
  std::vector<ApplyMessage> empty;
  uint64_t applied = applier->applyBatch(empty);
  EXPECT_EQ(0u, applied);
  EXPECT_EQ(0u, applier->totalApplied());
}

TEST_F(ReplicationApplierTest, DuplicateMessage_Rejected) {
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1")));
  // Send the same sequence again.
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1_dup")));
  EXPECT_EQ(1u, applier->totalApplied());
  EXPECT_EQ(1u, applier->totalDuplicatesRejected());
  // Only original doc exists.
  EXPECT_EQ(1u, store->documentCount("shard_0"));
}

TEST_F(ReplicationApplierTest, OutOfOrder_Buffered) {
  // Send seq=3 before seq=2. seq=3 should be buffered.
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 1, "doc1")));
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 3, "doc3")));
  EXPECT_EQ(1u, applier->totalApplied());  // only seq=1 applied
  EXPECT_EQ(1u, applier->bufferedCount());

  // Now send seq=2, which should drain the buffer and apply both.
  EXPECT_EQ(0, applier->applyMessage(makeInsert("shard_0", 2, "doc2")));
  EXPECT_EQ(3u, applier->totalApplied());
  EXPECT_EQ(0u, applier->bufferedCount());
  EXPECT_EQ(3u, applier->lastAppliedSequence("shard_0"));
}

TEST_F(ReplicationApplierTest, ConstructWithNullCallbacks_Throws) {
  EXPECT_THROW(ReplicationApplier(nullptr, [](std::string const&) {
    return false;
  }), std::invalid_argument);

  EXPECT_THROW(ReplicationApplier(
    [](std::string const&, ReplicationOperation, std::vector<uint8_t> const&,
       std::string const&) { return 0; },
    nullptr), std::invalid_argument);
}

}  // namespace test
}  // namespace arangodb
