#include "Enterprise/RClone/RCloneFeature.h"

#include <cstdlib>
#include <filesystem>
#include <type_traits>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Enterprise/RClone/CloudProvider.h"

static_assert(!std::is_abstract_v<arangodb::RCloneFeature>,
              "RCloneFeature must not be abstract");

namespace arangodb {

RCloneFeature::RCloneFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, *this) {}

void RCloneFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addOption("--rclone-executable",
                     "path to rclone binary", _rcloneExecutable);
  options->addOption("--rclone-provider",
                     "cloud provider: s3, azure, gcs", _providerString);
  options->addOption("--rclone-endpoint",
                     "provider endpoint URL", _endpoint);
  options->addOption("--rclone-bucket",
                     "bucket or container name", _bucket);
  options->addOption("--rclone-path-prefix",
                     "sub-path within bucket", _pathPrefix);
  options->addOption("--rclone-region",
                     "provider region", _region);
  options->addOption("--rclone-timeout",
                     "IO timeout in seconds", _timeoutSeconds);
  options->addOption("--rclone-transfers",
                     "parallel transfer count", _transfers);
  options->addOption("--rclone-retries",
                     "retry count on failure", _retries);
}

void RCloneFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  // If no provider is set, cloud backup is simply not configured.
  if (_providerString.empty()) {
    _validationError = "cloud provider not specified (set --rclone-provider)";
    _configured = false;
    return;
  }

  auto provider = parseCloudProvider(_providerString);
  if (!provider.has_value()) {
    _validationError = "unsupported cloud provider '" + _providerString +
                       "' (valid: s3, azure, gcs)";
    _configured = false;
    return;
  }

  // Build config from CLI options and environment variables.
  RCloneConfig cfg;
  cfg.provider = provider.value();
  cfg.endpoint = _endpoint;
  cfg.bucket = _bucket;
  cfg.pathPrefix = _pathPrefix;
  cfg.region = _region;
  cfg.rcloneBinaryPath = _rcloneExecutable;
  cfg.timeoutSeconds = _timeoutSeconds;
  cfg.transfers = static_cast<uint32_t>(_transfers);
  cfg.retries = static_cast<uint32_t>(_retries);

  // Read credentials from environment variables (never from CLI args).
  auto readEnv = [](char const* name) -> std::string {
    char const* val = std::getenv(name);
    return val != nullptr ? std::string(val) : std::string();
  };

  switch (cfg.provider) {
    case CloudProvider::kS3:
      cfg.accessKeyId = readEnv("ARANGO_RCLONE_S3_ACCESS_KEY_ID");
      cfg.secretAccessKey = readEnv("ARANGO_RCLONE_S3_SECRET_ACCESS_KEY");
      break;
    case CloudProvider::kAzureBlob:
      cfg.azureAccount = readEnv("ARANGO_RCLONE_AZURE_ACCOUNT");
      cfg.azureKey = readEnv("ARANGO_RCLONE_AZURE_KEY");
      break;
    case CloudProvider::kGCS:
      cfg.gcsServiceAccountFile =
          readEnv("ARANGO_RCLONE_GCS_SERVICE_ACCOUNT_FILE");
      break;
  }

  auto error = cfg.validate();
  if (!error.empty()) {
    _validationError = std::move(error);
    _configured = false;
    return;
  }

  _config = std::move(cfg);
  _configured = true;
  _validationError.clear();
}

void RCloneFeature::prepare() {
  if (!_configured) {
    return;
  }

  // Verify the rclone binary exists.
  namespace fs = std::filesystem;
  std::error_code ec;
  if (!fs::exists(_config->rcloneBinaryPath, ec)) {
    _validationError = "rclone binary not found at '" +
                       _config->rcloneBinaryPath +
                       "' (set --rclone-executable)";
    _configured = false;
  }
}

void RCloneFeature::start() {
  // Log readiness (in real ArangoDB, this would use LOG_TOPIC).
  // For now, this is a no-op; the feature is ready for uploadBackup calls.
}

void RCloneFeature::beginShutdown() {
  // Uploads are synchronous per-call, nothing to shut down.
}

void RCloneFeature::stop() {
  // No-op.
}

void RCloneFeature::unprepare() {
  // No-op.
}

RCloneResult RCloneFeature::uploadBackup(
    std::string const& localBackupPath,
    RCloneProgressCallback progressCallback) const {
  if (!_configured || !_config.has_value()) {
    return RCloneResult{-1, "", "RClone feature is not configured", false};
  }

  return RCloneProcess::execute(
      _config.value(), localBackupPath, std::move(progressCallback));
}

bool RCloneFeature::isConfigured() const noexcept {
  return _configured;
}

RCloneConfig const& RCloneFeature::config() const noexcept {
  return _config.value();
}

std::string const& RCloneFeature::validationError() const noexcept {
  return _validationError;
}

}  // namespace arangodb
