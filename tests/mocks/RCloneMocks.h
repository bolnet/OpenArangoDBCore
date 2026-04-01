#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Enterprise/RClone/RCloneConfig.h"
#include "Enterprise/RClone/RCloneProcess.h"

namespace arangodb::testing {

/// Mock that captures RCloneProcess::execute calls without forking.
/// Tests set expectedResult and verify calls were made with correct args.
struct MockRCloneExecutor {
  RCloneResult expectedResult{0, "", "", false};
  std::vector<std::string> lastCommand;
  std::unordered_map<std::string, std::string> lastEnvironment;
  bool wasCalled = false;
};

/// Helper to create a valid S3 RCloneConfig for testing.
inline RCloneConfig makeTestS3Config() {
  RCloneConfig cfg;
  cfg.provider = CloudProvider::kS3;
  cfg.endpoint = "s3.amazonaws.com";
  cfg.bucket = "my-backup-bucket";
  cfg.pathPrefix = "backups/daily";
  cfg.region = "us-east-1";
  cfg.rcloneBinaryPath = "/usr/bin/rclone";
  cfg.timeoutSeconds = 300;
  cfg.transfers = 4;
  cfg.retries = 3;
  cfg.accessKeyId = "AKIAIOSFODNN7EXAMPLE";
  cfg.secretAccessKey = "wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY";
  return cfg;
}

/// Helper to create a valid Azure RCloneConfig for testing.
inline RCloneConfig makeTestAzureConfig() {
  RCloneConfig cfg;
  cfg.provider = CloudProvider::kAzureBlob;
  cfg.endpoint = "";
  cfg.bucket = "my-container";
  cfg.pathPrefix = "backups";
  cfg.region = "";
  cfg.rcloneBinaryPath = "/usr/bin/rclone";
  cfg.timeoutSeconds = 300;
  cfg.transfers = 4;
  cfg.retries = 3;
  cfg.azureAccount = "myaccount";
  cfg.azureKey = "base64encodedkey==";
  return cfg;
}

/// Helper to create a valid GCS RCloneConfig for testing.
inline RCloneConfig makeTestGCSConfig() {
  RCloneConfig cfg;
  cfg.provider = CloudProvider::kGCS;
  cfg.endpoint = "";
  cfg.bucket = "my-gcs-bucket";
  cfg.pathPrefix = "backups";
  cfg.region = "us-central1";
  cfg.rcloneBinaryPath = "/usr/bin/rclone";
  cfg.timeoutSeconds = 300;
  cfg.transfers = 4;
  cfg.retries = 3;
  cfg.gcsServiceAccountFile = "/etc/gcs-key.json";
  return cfg;
}

}  // namespace arangodb::testing
