#include "Enterprise/Replication/DirectMQMessage.h"
#include "Enterprise/Replication/SequenceNumberGenerator.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <thread>
#include <vector>

// ===================================================================
// DirectMQMessage tests
// ===================================================================

TEST(DirectMQMessageTest, DefaultConstruction_ZeroSequence) {
  arangodb::DirectMQMessage msg;
  EXPECT_TRUE(msg.shard_id.empty());
  EXPECT_EQ(msg.sequence, 0u);
  EXPECT_EQ(msg.operation, arangodb::Operation::Insert);
  EXPECT_TRUE(msg.payload.empty());
}

TEST(DirectMQMessageTest, FullConstruction_AllFieldsSet) {
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03};
  arangodb::DirectMQMessage msg("s10001", 42, arangodb::Operation::Delete,
                                 payload);
  EXPECT_EQ(msg.shard_id, "s10001");
  EXPECT_EQ(msg.sequence, 42u);
  EXPECT_EQ(msg.operation, arangodb::Operation::Delete);
  EXPECT_EQ(msg.payload.size(), 3u);
  EXPECT_EQ(msg.payload[0], 0x01);
}

TEST(DirectMQMessageTest, OperationEnum_ThreeValues) {
  EXPECT_EQ(static_cast<uint8_t>(arangodb::Operation::Insert), 0);
  EXPECT_EQ(static_cast<uint8_t>(arangodb::Operation::Update), 1);
  EXPECT_EQ(static_cast<uint8_t>(arangodb::Operation::Delete), 2);
}

// ===================================================================
// SequenceNumberGenerator tests
// ===================================================================

TEST(SequenceNumberGeneratorTest, NextForShard_StartsAtOne) {
  arangodb::SequenceNumberGenerator gen;
  EXPECT_EQ(gen.nextSequence("s1"), 1u);
}

TEST(SequenceNumberGeneratorTest, NextForShard_Monotonic) {
  arangodb::SequenceNumberGenerator gen;
  EXPECT_EQ(gen.nextSequence("s1"), 1u);
  EXPECT_EQ(gen.nextSequence("s1"), 2u);
  EXPECT_EQ(gen.nextSequence("s1"), 3u);
  EXPECT_EQ(gen.nextSequence("s1"), 4u);
  EXPECT_EQ(gen.nextSequence("s1"), 5u);
}

TEST(SequenceNumberGeneratorTest, IndependentShards_IndependentCounters) {
  arangodb::SequenceNumberGenerator gen;
  EXPECT_EQ(gen.nextSequence("s1"), 1u);
  EXPECT_EQ(gen.nextSequence("s2"), 1u);
  EXPECT_EQ(gen.nextSequence("s1"), 2u);
  EXPECT_EQ(gen.nextSequence("s2"), 2u);
}

TEST(SequenceNumberGeneratorTest, ThreadSafety_NoDuplicates) {
  arangodb::SequenceNumberGenerator gen;
  constexpr int kThreads = 8;
  constexpr int kIterations = 1000;

  std::vector<std::vector<uint64_t>> results(kThreads);
  std::vector<std::thread> threads;

  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&gen, &results, t]() {
      results[t].reserve(kIterations);
      for (int i = 0; i < kIterations; ++i) {
        results[t].push_back(gen.nextSequence("s1"));
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  // Collect all values and verify uniqueness.
  std::set<uint64_t> allValues;
  for (auto const& vec : results) {
    for (auto v : vec) {
      EXPECT_TRUE(allValues.insert(v).second)
          << "Duplicate sequence number: " << v;
    }
  }
  EXPECT_EQ(allValues.size(), static_cast<size_t>(kThreads * kIterations));

  // Verify all values are in range [1, 8000].
  EXPECT_EQ(*allValues.begin(), 1u);
  EXPECT_EQ(*allValues.rbegin(), static_cast<uint64_t>(kThreads * kIterations));
}

TEST(SequenceNumberGeneratorTest, CurrentSequence_ReturnsLastGenerated) {
  arangodb::SequenceNumberGenerator gen;
  for (int i = 0; i < 5; ++i) {
    gen.nextSequence("s1");
  }
  EXPECT_EQ(gen.currentSequence("s1"), 5u);
}

TEST(SequenceNumberGeneratorTest, CurrentSequence_UnknownShard_ReturnsZero) {
  arangodb::SequenceNumberGenerator gen;
  EXPECT_EQ(gen.currentSequence("unknown"), 0u);
}

TEST(SequenceNumberGeneratorTest, Reset_ClearsAllCounters) {
  arangodb::SequenceNumberGenerator gen;
  gen.nextSequence("s1");
  gen.nextSequence("s2");
  gen.reset();
  EXPECT_EQ(gen.currentSequence("s1"), 0u);
  EXPECT_EQ(gen.currentSequence("s2"), 0u);
  // After reset, next call starts fresh at 1.
  EXPECT_EQ(gen.nextSequence("s1"), 1u);
}
