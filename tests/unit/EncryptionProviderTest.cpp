#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "Enterprise/RocksDBEngine/EncryptionProvider.h"
#include "Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h"

// ---- Fixture ----
class EncryptionProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // AES-256 requires a 32-byte key
    _testKey.resize(32);
    for (int i = 0; i < 32; ++i) {
      _testKey[i] = static_cast<char>(i);
    }
    _provider = std::make_unique<arangodb::enterprise::EncryptionProvider>(
        _testKey);
  }

  std::string _testKey;
  std::unique_ptr<arangodb::enterprise::EncryptionProvider> _provider;
};

// ---- GetPrefixLength ----
TEST_F(EncryptionProviderTest, GetPrefixLengthReturns16) {
  // IV for AES-256-CTR is 16 bytes
  EXPECT_EQ(16u, _provider->GetPrefixLength());
}

// ---- CreateNewPrefix ----
TEST_F(EncryptionProviderTest, CreateNewPrefixGenerates16ByteIV) {
  constexpr size_t kPrefixLen = 16;
  char prefix[kPrefixLen];
  std::memset(prefix, 0, kPrefixLen);

  auto s = _provider->CreateNewPrefix("test_file.sst", prefix, kPrefixLen);
  ASSERT_TRUE(s.ok()) << s.ToString();

  // Should not remain all zeros (RAND_bytes fills with random data)
  bool allZero = true;
  for (size_t i = 0; i < kPrefixLen; ++i) {
    if (prefix[i] != 0) { allZero = false; break; }
  }
  EXPECT_FALSE(allZero) << "Prefix should contain random IV, not all zeros";
}

TEST_F(EncryptionProviderTest, CreateNewPrefixProducesUniqueIVs) {
  constexpr size_t kPrefixLen = 16;
  constexpr int kIterations = 100;
  std::set<std::string> seen;

  for (int i = 0; i < kIterations; ++i) {
    char prefix[kPrefixLen];
    auto s = _provider->CreateNewPrefix("file_" + std::to_string(i),
                                        prefix, kPrefixLen);
    ASSERT_TRUE(s.ok()) << s.ToString();
    seen.insert(std::string(prefix, kPrefixLen));
  }

  // All IVs should be unique
  EXPECT_EQ(static_cast<size_t>(kIterations), seen.size())
      << "RAND_bytes should produce unique IVs across 100 calls";
}

// ---- Encrypt then Decrypt roundtrip ----
TEST_F(EncryptionProviderTest, EncryptDecryptRoundtrip4096Bytes) {
  constexpr size_t kPrefixLen = 16;
  constexpr size_t kBlockSize = 4096;

  // Generate prefix (IV)
  char prefix[kPrefixLen];
  auto s = _provider->CreateNewPrefix("roundtrip.sst", prefix, kPrefixLen);
  ASSERT_TRUE(s.ok());

  // Create cipher stream
  rocksdb::Slice prefixSlice(prefix, kPrefixLen);
  rocksdb::EnvOptions envOpts;
  std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
  s = _provider->CreateCipherStream("roundtrip.sst", envOpts, prefixSlice,
                                    &stream);
  ASSERT_TRUE(s.ok()) << s.ToString();
  ASSERT_NE(nullptr, stream.get());

  // Original plaintext
  std::vector<char> original(kBlockSize);
  for (size_t i = 0; i < kBlockSize; ++i) {
    original[i] = static_cast<char>(i % 256);
  }

  // Encrypt
  std::vector<char> encrypted(original);
  s = stream->Encrypt(0, encrypted.data(), kBlockSize);
  ASSERT_TRUE(s.ok()) << s.ToString();

  // Encrypted data should differ from original
  EXPECT_NE(original, encrypted) << "Encryption should change the data";

  // Decrypt - create a new stream from same prefix for decryption
  rocksdb::Slice prefixSlice2(prefix, kPrefixLen);
  std::unique_ptr<rocksdb::BlockAccessCipherStream> decStream;
  s = _provider->CreateCipherStream("roundtrip.sst", envOpts, prefixSlice2,
                                    &decStream);
  ASSERT_TRUE(s.ok());

  s = decStream->Decrypt(0, encrypted.data(), kBlockSize);
  ASSERT_TRUE(s.ok()) << s.ToString();

  EXPECT_EQ(original, encrypted)
      << "Decrypt(Encrypt(plaintext)) should return original plaintext";
}

// ---- Random-access CTR mode ----
TEST_F(EncryptionProviderTest, RandomAccessDecryptAtArbitraryOffset) {
  constexpr size_t kPrefixLen = 16;
  constexpr size_t kTotalSize = 8192;
  constexpr uint64_t kOffset = 4096;  // Start at second block

  char prefix[kPrefixLen];
  auto s = _provider->CreateNewPrefix("random_access.sst", prefix, kPrefixLen);
  ASSERT_TRUE(s.ok());

  rocksdb::EnvOptions envOpts;

  // Create full plaintext
  std::vector<char> fullPlaintext(kTotalSize);
  for (size_t i = 0; i < kTotalSize; ++i) {
    fullPlaintext[i] = static_cast<char>((i * 7 + 13) % 256);
  }

  // Encrypt the full thing
  std::vector<char> fullEncrypted(fullPlaintext);
  {
    rocksdb::Slice ps(prefix, kPrefixLen);
    std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
    s = _provider->CreateCipherStream("random_access.sst", envOpts, ps,
                                      &stream);
    ASSERT_TRUE(s.ok());
    s = stream->Encrypt(0, fullEncrypted.data(), kTotalSize);
    ASSERT_TRUE(s.ok());
  }

  // Now decrypt only the second half (at offset 4096)
  std::vector<char> partialDecrypt(fullEncrypted.begin() + kOffset,
                                   fullEncrypted.end());
  {
    rocksdb::Slice ps(prefix, kPrefixLen);
    std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
    s = _provider->CreateCipherStream("random_access.sst", envOpts, ps,
                                      &stream);
    ASSERT_TRUE(s.ok());
    s = stream->Decrypt(kOffset, partialDecrypt.data(),
                        kTotalSize - kOffset);
    ASSERT_TRUE(s.ok());
  }

  // Verify partial decryption matches original data at that offset
  std::vector<char> expected(fullPlaintext.begin() + kOffset,
                             fullPlaintext.end());
  EXPECT_EQ(expected, partialDecrypt)
      << "CTR mode random-access decrypt should produce correct plaintext";
}

// ---- Key loading tests ----
class KeyLoadingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    _tmpKeyFile = "/tmp/test_encryption_key_" +
                  std::to_string(reinterpret_cast<uintptr_t>(this));
  }

  void TearDown() override {
    std::remove(_tmpKeyFile.c_str());
  }

  void writeKeyFile(std::string const& content) {
    std::ofstream ofs(_tmpKeyFile, std::ios::binary);
    ofs.write(content.data(), content.size());
    ofs.close();
  }

  std::string _tmpKeyFile;
};

TEST_F(KeyLoadingTest, LoadKeyFromFile32Bytes) {
  std::string key(32, '\0');
  for (int i = 0; i < 32; ++i) {
    key[i] = static_cast<char>(i + 0x41);
  }
  writeKeyFile(key);

  auto result = arangodb::enterprise::loadKeyFromFile(_tmpKeyFile);
  ASSERT_TRUE(result.has_value()) << "Should load a 32-byte key successfully";
  EXPECT_EQ(32u, result->key.size());
  EXPECT_EQ(key, result->key);
}

TEST_F(KeyLoadingTest, RejectsKeyFileNot32Bytes) {
  // Write a 16-byte file (too short for AES-256)
  std::string shortKey(16, 'A');
  writeKeyFile(shortKey);

  auto result = arangodb::enterprise::loadKeyFromFile(_tmpKeyFile);
  EXPECT_FALSE(result.has_value())
      << "Should reject key file that is not exactly 32 bytes";
}

TEST_F(KeyLoadingTest, RejectsNonexistentKeyFile) {
  auto result = arangodb::enterprise::loadKeyFromFile(
      "/tmp/nonexistent_key_file_xyz");
  EXPECT_FALSE(result.has_value())
      << "Should fail gracefully for non-existent file";
}

// ---- EncryptionSecret ----
TEST(EncryptionSecretTest, StoresKeyAndId) {
  arangodb::enterprise::EncryptionSecret secret;
  secret.key = std::string(32, 'K');
  secret.id = "key-001";

  EXPECT_EQ(32u, secret.key.size());
  EXPECT_EQ("key-001", secret.id);
}

// ---- NIST test vector for AES-256-CTR ----
TEST_F(EncryptionProviderTest, AES256CTRKnownTestVector) {
  // NIST SP 800-38A F.5.5 -- AES-256-CTR Encrypt
  // Key: 603deb1015ca71be2b73aef0857d7781 1f352c073b6108d72d9810a30914dff4
  // IV:  f0f1f2f3f4f5f6f7f8f9fafbfcfdfeff
  // Plaintext block 1: 6bc1bee22e409f96e93d7e117393172a
  // Ciphertext block 1: 601ec313775789a5b7a7f504bbf3d228

  unsigned char key[32] = {
    0x60, 0x3d, 0xeb, 0x10, 0x15, 0xca, 0x71, 0xbe,
    0x2b, 0x73, 0xae, 0xf0, 0x85, 0x7d, 0x77, 0x81,
    0x1f, 0x35, 0x2c, 0x07, 0x3b, 0x61, 0x08, 0xd7,
    0x2d, 0x98, 0x10, 0xa3, 0x09, 0x14, 0xdf, 0xf4
  };
  unsigned char iv[16] = {
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
    0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
  };
  unsigned char plaintext[16] = {
    0x6b, 0xc1, 0xbe, 0xe2, 0x2e, 0x40, 0x9f, 0x96,
    0xe9, 0x3d, 0x7e, 0x11, 0x73, 0x93, 0x17, 0x2a
  };
  unsigned char expectedCiphertext[16] = {
    0x60, 0x1e, 0xc3, 0x13, 0x77, 0x57, 0x89, 0xa5,
    0xb7, 0xa7, 0xf5, 0x04, 0xbb, 0xf3, 0xd2, 0x28
  };

  std::string keyStr(reinterpret_cast<char*>(key), 32);
  auto provider = std::make_unique<arangodb::enterprise::EncryptionProvider>(
      keyStr);

  // Use the known IV as prefix
  rocksdb::Slice prefixSlice(reinterpret_cast<char*>(iv), 16);
  rocksdb::EnvOptions envOpts;
  std::unique_ptr<rocksdb::BlockAccessCipherStream> stream;
  auto s = provider->CreateCipherStream("nist_test", envOpts, prefixSlice,
                                        &stream);
  ASSERT_TRUE(s.ok()) << s.ToString();

  // Encrypt
  char data[16];
  std::memcpy(data, plaintext, 16);
  s = stream->Encrypt(0, data, 16);
  ASSERT_TRUE(s.ok()) << s.ToString();

  // Verify against known ciphertext
  EXPECT_EQ(0, std::memcmp(data, expectedCiphertext, 16))
      << "AES-256-CTR output should match NIST SP 800-38A test vector";
}
