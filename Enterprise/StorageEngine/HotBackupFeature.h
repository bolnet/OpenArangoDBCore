#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Enterprise/RocksDBEngine/RocksDBHotBackup.h"

namespace arangodb {

class RocksDBHotBackup;  // forward declaration

class HotBackupFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "HotBackup"; }

  explicit HotBackupFeature(ArangodServer& server)
      : ArangodFeature(server, *this),
        _enabled(true),
        _maxBackups(0) {}

  ~HotBackupFeature() = default;

  void collectOptions(std::shared_ptr<options::ProgramOptions> opts) override {
    opts->addOption("--rocksdb.backup-path",
                    "directory for hot backup storage", _backupPath);
    opts->addOption("--hot-backup.enabled",
                    "enable hot backup feature", _enabled);
    opts->addOption("--hot-backup.max-backups",
                    "maximum number of backups to retain (0 = unlimited)",
                    _maxBackups);
  }

  void validateOptions(std::shared_ptr<options::ProgramOptions>) override {
    // If enabled, backup path must not be empty
    if (_enabled && _backupPath.empty()) {
      _backupPath = "/tmp/arangodb-backups";  // default path
    }
  }

  void prepare() override {
    if (!_enabled) {
      return;
    }
    _hotBackup = std::make_unique<RocksDBHotBackup>(_backupPath);
  }

  void start() override {
    // Hot backup subsystem is now available for requests
  }

  void beginShutdown() override {}

  void stop() override {
    _hotBackup.reset();
  }

  void unprepare() override {
    _hotBackup.reset();
  }

  /// Access the backup engine. Throws if feature is disabled.
  RocksDBHotBackup& hotBackup() {
    if (!_hotBackup) {
      throw std::runtime_error(
          "HotBackupFeature is not enabled or not yet prepared");
    }
    return *_hotBackup;
  }

  bool isEnabled() const { return _enabled; }

 private:
  std::unique_ptr<RocksDBHotBackup> _hotBackup;
  std::string _backupPath;
  bool _enabled;
  uint64_t _maxBackups;
};

}  // namespace arangodb
