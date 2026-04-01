#include "Enterprise/Replication/IdempotencyChecker.h"

#include <gtest/gtest.h>

class IdempotencyCheckerTest : public ::testing::Test {
 protected:
  arangodb::SequenceNumberTracker tracker;
  arangodb::IdempotencyChecker checker{tracker};

  arangodb::DirectMQMessage makeMsg(std::string shard, uint64_t seq) {
    return arangodb::DirectMQMessage(
        std::move(shard), seq, arangodb::Operation::Insert, {});
  }
};

TEST_F(IdempotencyCheckerTest, Check_NewMessage_ReturnsAccept) {
  auto msg = makeMsg("s1", 1);
  EXPECT_EQ(checker.check(msg), arangodb::CheckResult::Accept);
}

TEST_F(IdempotencyCheckerTest, Check_DuplicateMessage_ReturnsReject) {
  auto msg = makeMsg("s1", 1);
  checker.accept(msg);
  EXPECT_EQ(checker.check(msg), arangodb::CheckResult::Reject);
}

TEST_F(IdempotencyCheckerTest, Check_OlderMessage_ReturnsReject) {
  auto msg5 = makeMsg("s1", 5);
  checker.accept(msg5);

  auto msg3 = makeMsg("s1", 3);
  EXPECT_EQ(checker.check(msg3), arangodb::CheckResult::Reject);
}

TEST_F(IdempotencyCheckerTest, Check_NextInOrder_ReturnsAccept) {
  auto msg1 = makeMsg("s1", 1);
  checker.accept(msg1);

  auto msg2 = makeMsg("s1", 2);
  EXPECT_EQ(checker.check(msg2), arangodb::CheckResult::Accept);
}

TEST_F(IdempotencyCheckerTest, Check_GapInSequence_ReturnsOutOfOrder) {
  auto msg1 = makeMsg("s1", 1);
  checker.accept(msg1);

  auto msg5 = makeMsg("s1", 5);
  EXPECT_EQ(checker.check(msg5), arangodb::CheckResult::OutOfOrder);
}

TEST_F(IdempotencyCheckerTest, Accept_UpdatesTracker) {
  auto msg = makeMsg("s1", 3);
  checker.accept(msg);
  EXPECT_EQ(tracker.lastAppliedSequence("s1"), 3u);
}

TEST_F(IdempotencyCheckerTest, Check_CrossShardIndependent) {
  auto msg10 = makeMsg("s1", 10);
  checker.accept(msg10);

  auto msg1s2 = makeMsg("s2", 1);
  EXPECT_EQ(checker.check(msg1s2), arangodb::CheckResult::Accept);
}

TEST_F(IdempotencyCheckerTest, Check_SequentialAcceptAndCheck) {
  // Accept 1, 2, 3 in order; verify each is Accept before accepting.
  for (uint64_t i = 1; i <= 3; ++i) {
    auto msg = makeMsg("s1", i);
    EXPECT_EQ(checker.check(msg), arangodb::CheckResult::Accept);
    checker.accept(msg);
  }
  // 4 is Accept, 3 is Reject, 10 is OutOfOrder.
  EXPECT_EQ(checker.check(makeMsg("s1", 4)), arangodb::CheckResult::Accept);
  EXPECT_EQ(checker.check(makeMsg("s1", 3)), arangodb::CheckResult::Reject);
  EXPECT_EQ(checker.check(makeMsg("s1", 10)), arangodb::CheckResult::OutOfOrder);
}
