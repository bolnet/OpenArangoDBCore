#include "Enterprise/Replication/SequenceNumberTracker.h"

#include <gtest/gtest.h>

TEST(SequenceNumberTrackerTest, IsAlreadyApplied_FirstMessage_ReturnsFalse) {
  arangodb::SequenceNumberTracker tracker;
  EXPECT_FALSE(tracker.isAlreadyApplied("s1", 1));
}

TEST(SequenceNumberTrackerTest, IsAlreadyApplied_SameSequence_ReturnsTrue) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);
  EXPECT_TRUE(tracker.isAlreadyApplied("s1", 5));
}

TEST(SequenceNumberTrackerTest, IsAlreadyApplied_OlderSequence_ReturnsTrue) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 10);
  EXPECT_TRUE(tracker.isAlreadyApplied("s1", 7));
}

TEST(SequenceNumberTrackerTest, IsAlreadyApplied_NewerSequence_ReturnsFalse) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);
  EXPECT_FALSE(tracker.isAlreadyApplied("s1", 6));
}

TEST(SequenceNumberTrackerTest, MarkApplied_UpdatesLastApplied) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 3);
  EXPECT_EQ(tracker.lastAppliedSequence("s1"), 3u);
}

TEST(SequenceNumberTrackerTest, IndependentShards_IndependentTracking) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 10);
  EXPECT_FALSE(tracker.isAlreadyApplied("s2", 1));
}

TEST(SequenceNumberTrackerTest, LastAppliedSequence_UnknownShard_ReturnsZero) {
  arangodb::SequenceNumberTracker tracker;
  EXPECT_EQ(tracker.lastAppliedSequence("unknown"), 0u);
}

TEST(SequenceNumberTrackerTest, GetState_ReturnsAllShards) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);
  tracker.markApplied("s2", 10);

  auto state = tracker.getState();
  EXPECT_EQ(state.size(), 2u);
  EXPECT_EQ(state["s1"], 5u);
  EXPECT_EQ(state["s2"], 10u);
}

TEST(SequenceNumberTrackerTest, RestoreState_RestoresAllShards) {
  arangodb::SequenceNumberTracker tracker;
  std::unordered_map<std::string, uint64_t> state = {
      {"s1", 42}, {"s2", 99}};
  tracker.restoreState(state);
  EXPECT_EQ(tracker.lastAppliedSequence("s1"), 42u);
  EXPECT_EQ(tracker.lastAppliedSequence("s2"), 99u);
}

TEST(SequenceNumberTrackerTest, MarkApplied_UsesMaxSemantics) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 10);
  tracker.markApplied("s1", 5);  // Should not regress.
  EXPECT_EQ(tracker.lastAppliedSequence("s1"), 10u);
}

TEST(SequenceNumberTrackerTest, Reset_ClearsAllState) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);
  tracker.reset();
  EXPECT_EQ(tracker.lastAppliedSequence("s1"), 0u);
  EXPECT_FALSE(tracker.isAlreadyApplied("s1", 1));
}
