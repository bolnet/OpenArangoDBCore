#include "Enterprise/Replication/ReplicationCheckpoint.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

class ReplicationCheckpointTest : public ::testing::Test {
 protected:
  std::string testDir;
  std::string checkpointPath;

  void SetUp() override {
    testDir = std::filesystem::temp_directory_path().string() +
              "/repl_ckpt_test_" + std::to_string(::testing::UnitTest::GetInstance()
                  ->current_test_info()->line());
    std::filesystem::create_directories(testDir);
    checkpointPath = testDir + "/checkpoint.json";
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
  }
};

TEST_F(ReplicationCheckpointTest, Save_CreatesJsonFile) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);

  arangodb::ReplicationCheckpoint ckpt(checkpointPath, tracker);
  auto result = ckpt.save();
  EXPECT_TRUE(result.success) << result.error;
  EXPECT_TRUE(std::filesystem::exists(checkpointPath));

  // Verify it contains valid JSON-ish content.
  std::ifstream in(checkpointPath);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("\"version\""), std::string::npos);
  EXPECT_NE(content.find("\"shards\""), std::string::npos);
}

TEST_F(ReplicationCheckpointTest, Save_ContainsAllShards) {
  arangodb::SequenceNumberTracker tracker;
  tracker.markApplied("s1", 5);
  tracker.markApplied("s2", 10);

  arangodb::ReplicationCheckpoint ckpt(checkpointPath, tracker);
  auto result = ckpt.save();
  EXPECT_TRUE(result.success) << result.error;

  std::ifstream in(checkpointPath);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("\"s1\""), std::string::npos);
  EXPECT_NE(content.find("\"s2\""), std::string::npos);
}

TEST_F(ReplicationCheckpointTest, Load_RestoresState) {
  // Save state with one tracker.
  arangodb::SequenceNumberTracker tracker1;
  tracker1.markApplied("s1", 5);
  tracker1.markApplied("s2", 10);

  arangodb::ReplicationCheckpoint ckpt1(checkpointPath, tracker1);
  auto saveResult = ckpt1.save();
  ASSERT_TRUE(saveResult.success) << saveResult.error;

  // Load into a fresh tracker.
  arangodb::SequenceNumberTracker tracker2;
  arangodb::ReplicationCheckpoint ckpt2(checkpointPath, tracker2);
  auto loadResult = ckpt2.load();
  ASSERT_TRUE(loadResult.success) << loadResult.error;

  EXPECT_EQ(tracker2.lastAppliedSequence("s1"), 5u);
  EXPECT_EQ(tracker2.lastAppliedSequence("s2"), 10u);
}

TEST_F(ReplicationCheckpointTest, Load_NonexistentFile_ReturnsEmpty) {
  arangodb::SequenceNumberTracker tracker;
  std::string missingPath = testDir + "/nonexistent.json";

  arangodb::ReplicationCheckpoint ckpt(missingPath, tracker);
  auto result = ckpt.load();
  EXPECT_TRUE(result.success) << result.error;
  EXPECT_EQ(tracker.getState().size(), 0u);
}

TEST_F(ReplicationCheckpointTest, Load_CorruptedFile_ReturnsError) {
  // Write garbage to the checkpoint file.
  {
    std::ofstream out(checkpointPath);
    out << "THIS IS NOT JSON AT ALL";
  }

  arangodb::SequenceNumberTracker tracker;
  arangodb::ReplicationCheckpoint ckpt(checkpointPath, tracker);
  auto result = ckpt.load();
  EXPECT_FALSE(result.success);
  EXPECT_FALSE(result.error.empty());
}

TEST_F(ReplicationCheckpointTest, RoundTrip_PreservesState) {
  // Save 100 shards.
  arangodb::SequenceNumberTracker tracker1;
  for (int i = 0; i < 100; ++i) {
    tracker1.markApplied("shard_" + std::to_string(i),
                          static_cast<uint64_t>(i * 100 + 42));
  }

  arangodb::ReplicationCheckpoint ckpt1(checkpointPath, tracker1);
  auto saveResult = ckpt1.save();
  ASSERT_TRUE(saveResult.success) << saveResult.error;

  // Load into fresh tracker.
  arangodb::SequenceNumberTracker tracker2;
  arangodb::ReplicationCheckpoint ckpt2(checkpointPath, tracker2);
  auto loadResult = ckpt2.load();
  ASSERT_TRUE(loadResult.success) << loadResult.error;

  // Verify all 100 shards match.
  auto state1 = tracker1.getState();
  auto state2 = tracker2.getState();
  EXPECT_EQ(state1.size(), state2.size());
  for (auto const& [shard, seq] : state1) {
    auto it = state2.find(shard);
    ASSERT_NE(it, state2.end()) << "Missing shard: " << shard;
    EXPECT_EQ(it->second, seq) << "Mismatch for shard: " << shard;
  }
}
