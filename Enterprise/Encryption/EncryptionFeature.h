#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "ApplicationFeatures/ApplicationFeature.h"

// Forward declare the RocksDB-level provider
namespace rocksdb {
class EncryptionProvider;
}

namespace arangodb {

namespace options {
class ProgramOptions;
}

/// Enterprise Encryption ApplicationFeature.
///
/// Manages encryption-at-rest lifecycle:
/// - Parses --rocksdb.encryption-keyfile and --rocksdb.encryption-keyfolder
/// - Loads keys and creates the RocksDB EncryptionProvider in prepare()
/// - Cleans up provider in unprepare()
class EncryptionFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Encryption"; }

  explicit EncryptionFeature(ArangodServer& server)
      : ArangodFeature(server, *this) {}

  void collectOptions(std::shared_ptr<options::ProgramOptions> opts) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions> opts) override;
  void prepare() override;
  void start() override;
  void beginShutdown() override {}
  void stop() override;
  void unprepare() override;

  // ---- Accessors ----

  /// Whether encryption is currently enabled (key loaded, provider created).
  bool isEncryptionEnabled() const noexcept { return _encryptionEnabled; }

  /// The active EncryptionProvider (null if encryption is not enabled).
  rocksdb::EncryptionProvider* provider() const noexcept {
    return _provider.get();
  }

  /// Whether collectOptions registered the keyfile option.
  bool hasKeyfileOption() const noexcept { return _hasKeyfileOption; }

  /// Whether collectOptions registered the keyfolder option.
  bool hasKeyfolderOption() const noexcept { return _hasKeyfolderOption; }

  // ---- Setters (for testing) ----

  void setKeyfilePath(std::string const& path) { _keyfilePath = path; }
  void setKeyfolderPath(std::string const& path) { _keyfolderPath = path; }

 private:
  std::string _keyfilePath;
  std::string _keyfolderPath;
  bool _encryptionEnabled = false;
  [[maybe_unused]] bool _keyRotationEnabled = false;
  bool _hasKeyfileOption = false;
  bool _hasKeyfolderOption = false;
  std::shared_ptr<rocksdb::EncryptionProvider> _provider;
};

}  // namespace arangodb
