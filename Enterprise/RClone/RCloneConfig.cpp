#include "Enterprise/RClone/RCloneConfig.h"

namespace arangodb {

std::string RCloneConfig::validate() const {
  if (rcloneBinaryPath.empty()) {
    return "rclone binary path must not be empty "
           "(set --rclone-executable)";
  }
  if (bucket.empty()) {
    return "bucket/container name must not be empty "
           "(set --rclone-bucket)";
  }

  switch (provider) {
    case CloudProvider::kS3:
      if (accessKeyId.empty()) {
        return "S3 access key ID must not be empty "
               "(set ARANGO_RCLONE_S3_ACCESS_KEY_ID)";
      }
      if (secretAccessKey.empty()) {
        return "S3 secret access key must not be empty "
               "(set ARANGO_RCLONE_S3_SECRET_ACCESS_KEY)";
      }
      break;

    case CloudProvider::kAzureBlob:
      if (azureAccount.empty()) {
        return "Azure account must not be empty "
               "(set ARANGO_RCLONE_AZURE_ACCOUNT)";
      }
      if (azureKey.empty()) {
        return "Azure key must not be empty "
               "(set ARANGO_RCLONE_AZURE_KEY)";
      }
      break;

    case CloudProvider::kGCS:
      if (gcsServiceAccountFile.empty()) {
        return "GCS service account file must not be empty "
               "(set ARANGO_RCLONE_GCS_SERVICE_ACCOUNT_FILE)";
      }
      break;
  }

  return "";
}

}  // namespace arangodb
