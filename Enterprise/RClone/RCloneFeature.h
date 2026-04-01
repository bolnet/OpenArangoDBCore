#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Enterprise/RClone/RCloneConfig.h"
#include "Enterprise/RClone/RCloneProcess.h"

namespace arangodb {

namespace options {
class ProgramOptions;
}

class RCloneFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "RClone"; }

  explicit RCloneFeature(ArangodServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override;
  void stop() override;
  void unprepare() override;

  /// Upload a local backup directory to the configured cloud destination.
  /// Returns RCloneResult with exit code and captured output.
  /// progressCallback is invoked with percentage (0-100) during upload.
  RCloneResult uploadBackup(
      std::string const& localBackupPath,
      RCloneProgressCallback progressCallback = nullptr) const;

  /// Returns true if cloud backup is configured and ready.
  bool isConfigured() const noexcept;

  /// Access the current configuration (immutable after validateOptions).
  RCloneConfig const& config() const noexcept;

  /// Returns the validation error message (empty if none).
  std::string const& validationError() const noexcept;

 private:
  // CLI option targets (populated by collectOptions/validateOptions)
  std::string _rcloneExecutable = "rclone";
  std::string _providerString;
  std::string _endpoint;
  std::string _bucket;
  std::string _pathPrefix;
  std::string _region;
  uint64_t _timeoutSeconds = 300;
  uint64_t _transfers = 4;
  uint64_t _retries = 3;

  // Built during validateOptions, immutable thereafter
  std::optional<RCloneConfig> _config;
  bool _configured = false;
  std::string _validationError;
};

}  // namespace arangodb
