#include "Enterprise/Encryption/EncryptionFeature.h"

#include <filesystem>
#include <stdexcept>
#include <type_traits>

#include "Enterprise/RocksDBEngine/EncryptionProvider.h"
#include "Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h"
#if defined(ARANGODB_INTEGRATION_BUILD) || __has_include("ProgramOptions/Parameters.h")
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Parameters.h"
#else
#include "ProgramOptions/ProgramOptions.h"
#endif

static_assert(!std::is_abstract_v<arangodb::EncryptionFeature>,
              "EncryptionFeature must not be abstract");

namespace arangodb {

void EncryptionFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> opts) {
  if (opts) {
    opts->addOption("--rocksdb.encryption-keyfile",
                    "path to the encryption keyfile (32-byte raw key)",
                    new options::StringParameter(&_keyfilePath));
    opts->addOption("--rocksdb.encryption-keyfolder",
                    "path to folder with encryption keys for rotation",
                    new options::StringParameter(&_keyfolderPath));
    opts->addOption("--rocksdb.encryption-key-rotation",
                    "enable API-based key rotation",
                    new options::BooleanParameter(&_keyRotationEnabled));
  }
  _hasKeyfileOption = true;
  _hasKeyfolderOption = true;
}

void EncryptionFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*opts*/) {
  // If keyfile is set, verify the file exists and is readable.
  if (!_keyfilePath.empty()) {
    std::error_code ec;
    if (!std::filesystem::exists(_keyfilePath, ec)) {
      throw std::runtime_error(
          "Encryption keyfile does not exist: " + _keyfilePath);
    }
    if (!std::filesystem::is_regular_file(_keyfilePath, ec)) {
      throw std::runtime_error(
          "Encryption keyfile is not a regular file: " + _keyfilePath);
    }
  }

  // If keyfolder is set, verify the directory exists.
  if (!_keyfolderPath.empty()) {
    std::error_code ec;
    if (!std::filesystem::is_directory(_keyfolderPath, ec)) {
      throw std::runtime_error(
          "Encryption keyfolder is not a directory: " + _keyfolderPath);
    }
  }
}

void EncryptionFeature::prepare() {
  if (_keyfilePath.empty() && _keyfolderPath.empty()) {
    // Encryption not requested -- nothing to do.
    return;
  }

  // Load key from keyfile
  if (!_keyfilePath.empty()) {
    auto secret = enterprise::loadKeyFromFile(_keyfilePath);
    if (!secret.has_value()) {
      throw std::runtime_error(
          "Failed to load encryption key from: " + _keyfilePath +
          " (file must contain exactly 32 bytes)");
    }

    _provider = std::make_shared<enterprise::EncryptionProvider>(secret->key);
    _encryptionEnabled = true;
  }
}

void EncryptionFeature::start() {
  // Provider is already created in prepare().
  // In a full ArangoDB integration, this would call
  // RocksDBEngine::configureEnterpriseRocksDBOptions() to hook the
  // provider into rocksdb::NewEncryptedEnv().
}

void EncryptionFeature::stop() {
  // Nothing to do on stop -- cleanup happens in unprepare().
}

void EncryptionFeature::unprepare() {
  _provider.reset();
  _encryptionEnabled = false;
}

}  // namespace arangodb
