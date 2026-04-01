#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace arangodb {

enum class CloudProvider {
  kS3,
  kAzureBlob,
  kGCS
};

/// Convert CloudProvider to rclone remote type string.
constexpr std::string_view cloudProviderToRCloneType(CloudProvider p) noexcept {
  switch (p) {
    case CloudProvider::kS3:        return "s3";
    case CloudProvider::kAzureBlob: return "azureblob";
    case CloudProvider::kGCS:       return "google cloud storage";
  }
  return "unknown";
}

/// Parse a user-supplied string ("s3", "azure", "gcs") into CloudProvider.
/// Returns nullopt on unrecognized input.
inline std::optional<CloudProvider> parseCloudProvider(
    std::string_view input) noexcept {
  if (input == "s3" || input == "S3") {
    return CloudProvider::kS3;
  }
  if (input == "azure" || input == "azureblob" || input == "Azure") {
    return CloudProvider::kAzureBlob;
  }
  if (input == "gcs" || input == "GCS") {
    return CloudProvider::kGCS;
  }
  return std::nullopt;
}

/// Human-readable name for error messages.
constexpr std::string_view cloudProviderName(CloudProvider p) noexcept {
  switch (p) {
    case CloudProvider::kS3:        return "Amazon S3";
    case CloudProvider::kAzureBlob: return "Azure Blob Storage";
    case CloudProvider::kGCS:       return "Google Cloud Storage";
  }
  return "Unknown";
}

}  // namespace arangodb
