#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

// Include mock ProgramOptions before EncryptionFeature so
// std::make_shared<ProgramOptions>() has a complete type.
#include "ProgramOptions/ProgramOptions.h"
#include "Enterprise/Encryption/EncryptionFeature.h"

// ---- Static assertion: EncryptionFeature is non-abstract ----
static_assert(!std::is_abstract_v<arangodb::EncryptionFeature>,
              "EncryptionFeature must be a concrete class");

// ---- Fixture ----
class EncryptionFeatureTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _feature = std::make_unique<arangodb::EncryptionFeature>(_server);
  }

  arangodb::application_features::ApplicationServer _server;
  std::unique_ptr<arangodb::EncryptionFeature> _feature;
};

// ---- name() ----
TEST_F(EncryptionFeatureTest, NameReturnsEncryption) {
  EXPECT_EQ("Encryption", arangodb::EncryptionFeature::name());
}

// ---- collectOptions registers expected options ----
TEST_F(EncryptionFeatureTest, CollectOptionsRegistersKeyfileOption) {
  // collectOptions should not throw.  The mock ProgramOptions is nullptr
  // in this test, so we just verify the call doesn't crash when
  // collectOptions is a no-op or handles nullptr gracefully.
  // A more thorough test would check registered option names.
  auto opts = std::make_shared<arangodb::options::ProgramOptions>();
  EXPECT_NO_THROW(_feature->collectOptions(opts));
  EXPECT_TRUE(_feature->hasKeyfileOption())
      << "collectOptions should register --rocksdb.encryption-keyfile";
  EXPECT_TRUE(_feature->hasKeyfolderOption())
      << "collectOptions should register --rocksdb.encryption-keyfolder";
}

// ---- validateOptions ----
TEST_F(EncryptionFeatureTest, ValidateOptionsAcceptsEmptyPaths) {
  // When neither keyfile nor keyfolder is set, validation should pass
  // (encryption is simply not enabled).
  auto opts = std::make_shared<arangodb::options::ProgramOptions>();
  EXPECT_NO_THROW(_feature->validateOptions(opts));
}

// ---- prepare() with keyfile ----
class EncryptionFeaturePrepareTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpKeyFile = "/tmp/test_enc_feature_key_" +
                  std::to_string(reinterpret_cast<uintptr_t>(this));

    // Write a 32-byte key
    std::string key(32, '\0');
    for (int i = 0; i < 32; ++i) {
      key[i] = static_cast<char>(i + 0x30);
    }
    std::ofstream ofs(_tmpKeyFile, std::ios::binary);
    ofs.write(key.data(), key.size());
    ofs.close();

    _feature = std::make_unique<arangodb::EncryptionFeature>(_server);
  }

  void TearDown() override {
    std::remove(_tmpKeyFile.c_str());
  }

  arangodb::application_features::ApplicationServer _server;
  std::unique_ptr<arangodb::EncryptionFeature> _feature;
  std::string _tmpKeyFile;
};

TEST_F(EncryptionFeaturePrepareTest, PrepareLoadsKeyAndCreatesProvider) {
  _feature->setKeyfilePath(_tmpKeyFile);
  EXPECT_NO_THROW(_feature->prepare());
  EXPECT_TRUE(_feature->isEncryptionEnabled())
      << "After prepare() with a valid keyfile, encryption should be enabled";
  EXPECT_NE(nullptr, _feature->provider())
      << "After prepare(), provider should be non-null";
}

TEST_F(EncryptionFeaturePrepareTest, UnprepareNullsOutProvider) {
  _feature->setKeyfilePath(_tmpKeyFile);
  _feature->prepare();
  ASSERT_NE(nullptr, _feature->provider());

  _feature->unprepare();
  EXPECT_EQ(nullptr, _feature->provider())
      << "After unprepare(), provider should be null";
  EXPECT_FALSE(_feature->isEncryptionEnabled());
}

// ---- EncryptionFeature is constructible ----
TEST(EncryptionFeatureStaticTest, IsNonAbstract) {
  EXPECT_FALSE(std::is_abstract_v<arangodb::EncryptionFeature>);
}
