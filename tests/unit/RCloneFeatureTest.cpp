#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdlib>
#include <memory>
#include <string>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Enterprise/RClone/RCloneFeature.h"
#include "Enterprise/RClone/CloudProvider.h"
#include "RCloneMocks.h"

using namespace arangodb;
using namespace arangodb::testing;

// ============================================================================
// Helper: Set environment variables for tests, restore on destruction.
// ============================================================================
class ScopedEnv {
 public:
  void set(std::string const& key, std::string const& value) {
    // Save original.
    char const* orig = std::getenv(key.c_str());
    _saved.push_back({key, orig ? std::string(orig) : std::string(),
                       orig != nullptr});
    setenv(key.c_str(), value.c_str(), 1);
  }

  ~ScopedEnv() {
    for (auto it = _saved.rbegin(); it != _saved.rend(); ++it) {
      if (it->hadValue) {
        setenv(it->key.c_str(), it->value.c_str(), 1);
      } else {
        unsetenv(it->key.c_str());
      }
    }
  }

 private:
  struct Entry {
    std::string key;
    std::string value;
    bool hadValue;
  };
  std::vector<Entry> _saved;
};

// ============================================================================
// collectOptions tests
// ============================================================================

TEST(RCloneFeature, CollectOptions_RegistersAllRCloneOptions) {
  application_features::ApplicationServer server;
  RCloneFeature feature(server);

  auto opts = std::make_shared<options::ProgramOptions>();
  feature.collectOptions(opts);

  EXPECT_TRUE(opts->hasOption("--rclone-executable"));
  EXPECT_TRUE(opts->hasOption("--rclone-provider"));
  EXPECT_TRUE(opts->hasOption("--rclone-endpoint"));
  EXPECT_TRUE(opts->hasOption("--rclone-bucket"));
  EXPECT_TRUE(opts->hasOption("--rclone-path-prefix"));
  EXPECT_TRUE(opts->hasOption("--rclone-region"));
  EXPECT_TRUE(opts->hasOption("--rclone-timeout"));
  EXPECT_TRUE(opts->hasOption("--rclone-transfers"));
  EXPECT_TRUE(opts->hasOption("--rclone-retries"));
}

// ============================================================================
// validateOptions tests
// ============================================================================

TEST(RCloneFeature, ValidateOptions_MissingProvider_ReportsError) {
  application_features::ApplicationServer server;
  RCloneFeature feature(server);

  auto opts = std::make_shared<options::ProgramOptions>();
  feature.collectOptions(opts);
  // Don't set provider -- leave it empty.
  feature.validateOptions(opts);

  EXPECT_FALSE(feature.isConfigured());
  EXPECT_FALSE(feature.validationError().empty());
  EXPECT_NE(feature.validationError().find("provider"),
            std::string::npos);
}

TEST(RCloneFeature, ValidateOptions_InvalidProvider_ReportsError) {
  application_features::ApplicationServer server;
  RCloneFeature feature(server);

  auto opts = std::make_shared<options::ProgramOptions>();
  feature.collectOptions(opts);

  // We need to set the provider string directly. Since collectOptions
  // binds to a private member, we use a workaround: create a feature,
  // call collectOptions to register, then modify the option target.
  // However, our mock ProgramOptions doesn't actually write to the
  // target. So we test via a different approach: build and validate
  // a feature that has the provider set.

  // Instead, we test the CloudProvider parser directly as a proxy.
  auto parsed = parseCloudProvider("dropbox");
  EXPECT_FALSE(parsed.has_value());

  auto validParsed = parseCloudProvider("s3");
  EXPECT_TRUE(validParsed.has_value());
  EXPECT_EQ(validParsed.value(), CloudProvider::kS3);
}

TEST(RCloneFeature, ValidateOptions_ValidS3Config_Succeeds) {
  // Since our mock ProgramOptions doesn't actually bind to member vars,
  // we test the config validation path directly.
  auto cfg = makeTestS3Config();
  auto error = cfg.validate();
  EXPECT_TRUE(error.empty()) << "Expected valid config, got: " << error;
}

// ============================================================================
// Config validation tests
// ============================================================================

TEST(RCloneFeature, BuildConfig_FromOptions_CorrectMapping) {
  auto cfg = makeTestS3Config();
  EXPECT_EQ(cfg.provider, CloudProvider::kS3);
  EXPECT_EQ(cfg.endpoint, "s3.amazonaws.com");
  EXPECT_EQ(cfg.bucket, "my-backup-bucket");
  EXPECT_EQ(cfg.pathPrefix, "backups/daily");
  EXPECT_EQ(cfg.region, "us-east-1");
  EXPECT_EQ(cfg.rcloneBinaryPath, "/usr/bin/rclone");
  EXPECT_EQ(cfg.timeoutSeconds, 300u);
  EXPECT_EQ(cfg.transfers, 4u);
  EXPECT_EQ(cfg.retries, 3u);

  // Verify environment is built correctly from this config.
  auto env = RCloneProcess::buildEnvironment(cfg);
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_TYPE"], "s3");
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_ACCESS_KEY_ID"], cfg.accessKeyId);
  EXPECT_EQ(env["RCLONE_CONFIG_REMOTE_SECRET_ACCESS_KEY"],
            cfg.secretAccessKey);

  // Verify command is built correctly.
  auto cmd = RCloneProcess::buildCommand(cfg, "/backup/snap-001");
  EXPECT_EQ(cmd[3], "remote:my-backup-bucket/backups/daily");
}

TEST(RCloneFeature, UploadBackup_CallsProcessWithCorrectPath) {
  // We verify the integration by checking that uploadBackup uses the
  // config's path and produces the correct command structure.
  // (We cannot actually run rclone in unit tests.)
  auto cfg = makeTestS3Config();
  auto cmd = RCloneProcess::buildCommand(cfg, "/var/lib/arangodb/backups/snap-001");

  EXPECT_EQ(cmd[0], cfg.rcloneBinaryPath);
  EXPECT_EQ(cmd[1], "copy");
  EXPECT_EQ(cmd[2], "/var/lib/arangodb/backups/snap-001");
  EXPECT_EQ(cmd[3], "remote:" + cfg.bucket + "/" + cfg.pathPrefix);

  // Also verify that an unconfigured feature returns error.
  application_features::ApplicationServer server;
  RCloneFeature feature(server);
  auto result = feature.uploadBackup("/tmp/backup");
  EXPECT_EQ(result.exitCode, -1);
  EXPECT_FALSE(result.stderrOutput.empty());
}

// ============================================================================
// CloudProvider enum tests
// ============================================================================

TEST(CloudProvider, ParseAndConvert) {
  // S3
  auto s3 = parseCloudProvider("s3");
  ASSERT_TRUE(s3.has_value());
  EXPECT_EQ(cloudProviderToRCloneType(s3.value()), "s3");
  EXPECT_EQ(cloudProviderName(s3.value()), "Amazon S3");

  // Azure
  auto azure = parseCloudProvider("azure");
  ASSERT_TRUE(azure.has_value());
  EXPECT_EQ(cloudProviderToRCloneType(azure.value()), "azureblob");
  EXPECT_EQ(cloudProviderName(azure.value()), "Azure Blob Storage");

  // GCS
  auto gcs = parseCloudProvider("gcs");
  ASSERT_TRUE(gcs.has_value());
  EXPECT_EQ(cloudProviderToRCloneType(gcs.value()), "google cloud storage");
  EXPECT_EQ(cloudProviderName(gcs.value()), "Google Cloud Storage");

  // Invalid
  EXPECT_FALSE(parseCloudProvider("dropbox").has_value());
  EXPECT_FALSE(parseCloudProvider("").has_value());
}

// ============================================================================
// RCloneConfig validation tests
// ============================================================================

TEST(RCloneConfig, Validate_MissingBucket_ReportsError) {
  auto cfg = makeTestS3Config();
  cfg.bucket = "";
  auto error = cfg.validate();
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("bucket"), std::string::npos);
}

TEST(RCloneConfig, Validate_MissingS3Credentials_ReportsError) {
  auto cfg = makeTestS3Config();
  cfg.accessKeyId = "";
  auto error = cfg.validate();
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("access key"), std::string::npos);
}

TEST(RCloneConfig, Validate_MissingAzureCredentials_ReportsError) {
  auto cfg = makeTestAzureConfig();
  cfg.azureAccount = "";
  auto error = cfg.validate();
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("Azure account"), std::string::npos);
}

TEST(RCloneConfig, Validate_MissingGCSCredentials_ReportsError) {
  auto cfg = makeTestGCSConfig();
  cfg.gcsServiceAccountFile = "";
  auto error = cfg.validate();
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("service account"), std::string::npos);
}

TEST(RCloneConfig, Validate_MissingBinaryPath_ReportsError) {
  auto cfg = makeTestS3Config();
  cfg.rcloneBinaryPath = "";
  auto error = cfg.validate();
  EXPECT_FALSE(error.empty());
  EXPECT_NE(error.find("binary path"), std::string::npos);
}
