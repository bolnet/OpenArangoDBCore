#include "Enterprise/Replication/DC2DCReplicator.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(DC2DCReplicatorTest, Construction_DefaultDisabled) {
  arangodb::DC2DCReplicator replicator;
  EXPECT_FALSE(replicator.options().enabled);
  EXPECT_FALSE(replicator.isPrepared());
}

TEST(DC2DCReplicatorTest, CollectOptions_DoesNotThrow) {
  arangodb::DC2DCReplicator replicator;
  EXPECT_NO_THROW(replicator.collectOptions());
}

TEST(DC2DCReplicatorTest, ValidateOptions_DisabledNoValidation) {
  arangodb::DC2DCReplicator replicator;
  // Default: enabled=false, targetCluster empty.
  EXPECT_TRUE(replicator.validateOptions());
}

TEST(DC2DCReplicatorTest, ValidateOptions_EnabledRequiresTarget) {
  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().enabled = true;
  // targetCluster is empty -> validation should fail.
  EXPECT_FALSE(replicator.validateOptions());
}

TEST(DC2DCReplicatorTest, ValidateOptions_EnabledWithTarget_Succeeds) {
  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().enabled = true;
  replicator.mutableOptions().targetCluster = "https://dc2:8529";
  EXPECT_TRUE(replicator.validateOptions());
}

TEST(DC2DCReplicatorTest, ValidateOptions_ZeroBatchSize_Fails) {
  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().enabled = true;
  replicator.mutableOptions().targetCluster = "https://dc2:8529";
  replicator.mutableOptions().batchSize = 0;
  EXPECT_FALSE(replicator.validateOptions());
}

TEST(DC2DCReplicatorTest, Prepare_InitializesComponents) {
  arangodb::DC2DCReplicator replicator;
  replicator.prepare();
  EXPECT_TRUE(replicator.isPrepared());
  EXPECT_NE(replicator.generator(), nullptr);
  EXPECT_NE(replicator.tracker(), nullptr);
  EXPECT_NE(replicator.checker(), nullptr);
  EXPECT_NE(replicator.checkpoint(), nullptr);
}

TEST(DC2DCReplicatorTest, Start_LoadsCheckpoint) {
  // Use a temp directory for checkpoint.
  auto tmpDir = std::filesystem::temp_directory_path().string() +
                "/dc2dc_test_start";
  std::filesystem::create_directories(tmpDir);
  std::string ckptPath = tmpDir + "/ckpt.json";

  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().checkpointPath = ckptPath;
  replicator.prepare();

  // start() should succeed (no checkpoint file = fresh start).
  EXPECT_NO_THROW(replicator.start());

  std::error_code ec;
  std::filesystem::remove_all(tmpDir, ec);
}

TEST(DC2DCReplicatorTest, Stop_SavesCheckpoint) {
  auto tmpDir = std::filesystem::temp_directory_path().string() +
                "/dc2dc_test_stop";
  std::filesystem::create_directories(tmpDir);
  std::string ckptPath = tmpDir + "/ckpt.json";

  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().checkpointPath = ckptPath;
  replicator.prepare();
  replicator.start();

  // Apply some state via the tracker.
  replicator.tracker()->markApplied("s1", 42);

  // stop() should save checkpoint to disk.
  EXPECT_NO_THROW(replicator.stop());
  EXPECT_TRUE(std::filesystem::exists(ckptPath));

  std::error_code ec;
  std::filesystem::remove_all(tmpDir, ec);
}

TEST(DC2DCReplicatorTest, StartWithoutPrepare_Throws) {
  arangodb::DC2DCReplicator replicator;
  EXPECT_THROW(replicator.start(), std::runtime_error);
}

TEST(DC2DCReplicatorTest, StopWithoutPrepare_NoThrow) {
  arangodb::DC2DCReplicator replicator;
  EXPECT_NO_THROW(replicator.stop());
}

TEST(DC2DCReplicatorTest, FullLifecycle) {
  auto tmpDir = std::filesystem::temp_directory_path().string() +
                "/dc2dc_test_lifecycle";
  std::filesystem::create_directories(tmpDir);
  std::string ckptPath = tmpDir + "/ckpt.json";

  arangodb::DC2DCReplicator replicator;
  replicator.mutableOptions().checkpointPath = ckptPath;
  replicator.collectOptions();
  EXPECT_TRUE(replicator.validateOptions());
  replicator.prepare();
  replicator.start();

  // Generate sequences and track them.
  auto seq1 = replicator.generator()->nextSequence("s1");
  EXPECT_EQ(seq1, 1u);
  replicator.tracker()->markApplied("s1", seq1);

  replicator.stop();

  // Verify checkpoint was persisted by loading into new replicator.
  arangodb::DC2DCReplicator replicator2;
  replicator2.mutableOptions().checkpointPath = ckptPath;
  replicator2.prepare();
  replicator2.start();
  EXPECT_EQ(replicator2.tracker()->lastAppliedSequence("s1"), 1u);
  replicator2.stop();

  std::error_code ec;
  std::filesystem::remove_all(tmpDir, ec);
}
