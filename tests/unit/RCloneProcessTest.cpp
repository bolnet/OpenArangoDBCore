#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Enterprise/RClone/RCloneProcess.h"
#include "RCloneMocks.h"

using namespace arangodb;
using namespace arangodb::testing;
using ::testing::HasSubstr;

// ============================================================================
// buildCommand tests
// ============================================================================

TEST(RCloneProcess, BuildCommand_S3_CorrectArgs) {
  auto cfg = makeTestS3Config();
  auto cmd = RCloneProcess::buildCommand(cfg, "/tmp/backup-2026");

  ASSERT_GE(cmd.size(), 4u);
  EXPECT_EQ(cmd[0], "/usr/bin/rclone");
  EXPECT_EQ(cmd[1], "copy");
  EXPECT_EQ(cmd[2], "/tmp/backup-2026");
  EXPECT_EQ(cmd[3], "remote:my-backup-bucket/backups/daily");

  // Check flags are present.
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--progress"), cmd.end());
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--stats"), cmd.end());
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--transfers"), cmd.end());
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--retries"), cmd.end());
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--timeout"), cmd.end());
  EXPECT_NE(std::find(cmd.begin(), cmd.end(), "--log-level"), cmd.end());
}

TEST(RCloneProcess, BuildCommand_Azure_CorrectArgs) {
  auto cfg = makeTestAzureConfig();
  auto cmd = RCloneProcess::buildCommand(cfg, "/data/backup-latest");

  ASSERT_GE(cmd.size(), 4u);
  EXPECT_EQ(cmd[0], "/usr/bin/rclone");
  EXPECT_EQ(cmd[1], "copy");
  EXPECT_EQ(cmd[2], "/data/backup-latest");
  EXPECT_EQ(cmd[3], "remote:my-container/backups");
}

TEST(RCloneProcess, BuildCommand_GCS_CorrectArgs) {
  auto cfg = makeTestGCSConfig();
  auto cmd = RCloneProcess::buildCommand(cfg, "/data/backup");

  ASSERT_GE(cmd.size(), 4u);
  EXPECT_EQ(cmd[0], "/usr/bin/rclone");
  EXPECT_EQ(cmd[1], "copy");
  EXPECT_EQ(cmd[2], "/data/backup");
  EXPECT_EQ(cmd[3], "remote:my-gcs-bucket/backups");
}

// ============================================================================
// buildEnvironment tests
// ============================================================================

TEST(RCloneProcess, BuildEnvironment_S3_SetsCorrectVars) {
  auto cfg = makeTestS3Config();
  auto env = RCloneProcess::buildEnvironment(cfg);

  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_TYPE"], "s3");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_ACCESS_KEY_ID"],
            "AKIAIOSFODNN7EXAMPLE");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_SECRET_ACCESS_KEY"],
            "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_ENDPOINT"], "s3.amazonaws.com");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_REGION"], "us-east-1");
}

TEST(RCloneProcess, BuildEnvironment_Azure_SetsCorrectVars) {
  auto cfg = makeTestAzureConfig();
  auto env = RCloneProcess::buildEnvironment(cfg);

  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_TYPE"], "azureblob");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_ACCOUNT"], "myaccount");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_KEY"], "base64encodedkey==");
}

TEST(RCloneProcess, BuildEnvironment_GCS_SetsCorrectVars) {
  auto cfg = makeTestGCSConfig();
  auto env = RCloneProcess::buildEnvironment(cfg);

  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_TYPE"], "google cloud storage");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_SERVICE_ACCOUNT_FILE"],
            "/etc/gcs-key.json");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_BUCKET_POLICY_ONLY"], "true");
}

TEST(RCloneProcess, BuildEnvironment_NoCredentialsInArgs) {
  // Verify command args never contain credential values.
  auto cfg = makeTestS3Config();
  auto cmd = RCloneProcess::buildCommand(cfg, "/tmp/backup");

  for (auto const& arg : cmd) {
    EXPECT_NE(arg, cfg.accessKeyId)
        << "access key ID found in command args (security violation)";
    EXPECT_NE(arg, cfg.secretAccessKey)
        << "secret access key found in command args (security violation)";
  }

  // Also check Azure.
  auto azureCfg = makeTestAzureConfig();
  auto azureCmd = RCloneProcess::buildCommand(azureCfg, "/tmp/backup");
  for (auto const& arg : azureCmd) {
    EXPECT_NE(arg, azureCfg.azureAccount)
        << "azure account found in command args";
    EXPECT_NE(arg, azureCfg.azureKey)
        << "azure key found in command args";
  }

  // Also check GCS.
  auto gcsCfg = makeTestGCSConfig();
  auto gcsCmd = RCloneProcess::buildCommand(gcsCfg, "/tmp/backup");
  for (auto const& arg : gcsCmd) {
    EXPECT_NE(arg, gcsCfg.gcsServiceAccountFile)
        << "GCS service account file found in command args";
  }
}

// ============================================================================
// parseProgressLine tests
// ============================================================================

TEST(RCloneProcess, ParseProgress_ValidLine_ExtractsPercentage) {
  auto result = RCloneProcess::parseProgressLine(
      "Transferred:   1.234 GiB / 5.678 GiB, 22%, 100.000 MiB/s, ETA 45s");
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 22u);

  // Test 100%.
  auto result2 = RCloneProcess::parseProgressLine(
      "Transferred:   5.678 GiB / 5.678 GiB, 100%, 200.000 MiB/s, ETA 0s");
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(result2.value(), 100u);

  // Test 0%.
  auto result3 = RCloneProcess::parseProgressLine(
      "Transferred:   0 B / 1.000 GiB, 0%, 0 B/s, ETA -");
  ASSERT_TRUE(result3.has_value());
  EXPECT_EQ(result3.value(), 0u);
}

TEST(RCloneProcess, ParseProgress_InvalidLine_ReturnsNullopt) {
  // Not a progress line at all.
  auto r1 = RCloneProcess::parseProgressLine("Errors: 0");
  EXPECT_FALSE(r1.has_value());

  // Garbage.
  auto r2 = RCloneProcess::parseProgressLine("hello world");
  EXPECT_FALSE(r2.has_value());

  // Empty.
  auto r3 = RCloneProcess::parseProgressLine("");
  EXPECT_FALSE(r3.has_value());

  // Count-based transferred line (not bytes).
  auto r4 = RCloneProcess::parseProgressLine(
      "Transferred:   50 / 200, 25%");
  EXPECT_FALSE(r4.has_value());
}
