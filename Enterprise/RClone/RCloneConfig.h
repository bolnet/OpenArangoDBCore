#pragma once

#include <cstdint>
#include <string>

#include "Enterprise/RClone/CloudProvider.h"

namespace arangodb {

/// Immutable configuration for a cloud backup upload.
/// Constructed once from CLI options, never mutated.
struct RCloneConfig {
  CloudProvider provider;
  std::string endpoint;           // e.g. "s3.amazonaws.com" or custom
  std::string bucket;             // bucket or container name
  std::string pathPrefix;         // sub-path within bucket
  std::string region;             // provider region (S3/GCS)
  std::string rcloneBinaryPath;   // path to rclone executable
  uint64_t timeoutSeconds = 300;  // per-transfer IO timeout
  uint32_t transfers = 4;         // parallel transfer count
  uint32_t retries = 3;           // retry count on failure

  // Credential fields -- stored in memory, injected as env vars, never
  // serialized to disk or passed as CLI arguments.
  std::string accessKeyId;            // S3
  std::string secretAccessKey;        // S3
  std::string azureAccount;           // Azure
  std::string azureKey;               // Azure
  std::string gcsServiceAccountFile;  // GCS

  /// Validate that required fields are populated for the chosen provider.
  /// Returns empty string on success, error description on failure.
  [[nodiscard]] std::string validate() const;
};

}  // namespace arangodb
