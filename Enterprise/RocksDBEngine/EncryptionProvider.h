#pragma once
#ifndef ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H
#define ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include <rocksdb/env_encryption.h>

namespace arangodb {
namespace enterprise {

/// AES-256-CTR block cipher wrapper using OpenSSL EVP.
///
/// Provides block-level encrypt/decrypt operations for RocksDB's
/// BlockAccessCipherStream interface.  CTR mode enables random-access
/// reads and writes at arbitrary file offsets.
class AESCTRCipherStream final : public rocksdb::BlockAccessCipherStream {
 public:
  /// Construct a cipher stream from a 32-byte key and 16-byte IV.
  AESCTRCipherStream(std::string const& key, std::string const& iv);
  ~AESCTRCipherStream() override;

  AESCTRCipherStream(AESCTRCipherStream const&) = delete;
  AESCTRCipherStream& operator=(AESCTRCipherStream const&) = delete;

  /// Encrypt data in-place at the given file offset.
  rocksdb::Status Encrypt(uint64_t fileOffset, char* data,
                          size_t dataSize) override;

  /// Decrypt data in-place at the given file offset.
  rocksdb::Status Decrypt(uint64_t fileOffset, char* data,
                          size_t dataSize) override;

  size_t BlockSize() override { return 0; }  // stream cipher

  /// Required by real RocksDB BlockAccessCipherStream interface.
  /// For CTR mode (BlockSize()==0), these are not called by RocksDB,
  /// but must be implemented to satisfy the pure virtual contract.
  void AllocateScratch(std::string& scratch) override {
    scratch.clear();
  }
  rocksdb::Status EncryptBlock(uint64_t /*blockIndex*/, char* /*data*/,
                               char* /*scratch*/) override {
    return rocksdb::Status::NotSupported("CTR mode uses Encrypt() directly");
  }
  rocksdb::Status DecryptBlock(uint64_t /*blockIndex*/, char* /*data*/,
                               char* /*scratch*/) override {
    return rocksdb::Status::NotSupported("CTR mode uses Decrypt() directly");
  }

 private:
  /// Perform CTR-mode XOR at arbitrary offset.
  rocksdb::Status ctrTransform(uint64_t fileOffset, char* data,
                               size_t dataSize);

  std::string _key;  // 32 bytes
  std::string _iv;   // 16 bytes (initial counter)
};

/// RocksDB EncryptionProvider implementation using AES-256-CTR.
///
/// Each encrypted file stores a 16-byte IV prefix.  New IVs are generated
/// via OpenSSL RAND_bytes for cryptographic uniqueness.
class EncryptionProvider final : public rocksdb::EncryptionProvider {
 public:
  /// Construct with a 32-byte AES-256 key.
  explicit EncryptionProvider(std::string const& key);
  ~EncryptionProvider() override = default;

  char const* Name() const override { return "AES256CTREncryptionProvider"; }

  /// IV length: 16 bytes (AES block size).
  size_t GetPrefixLength() const override;

  /// Generate a cryptographically random IV prefix via RAND_bytes.
  rocksdb::Status CreateNewPrefix(std::string const& fname, char* prefix,
                                  size_t prefixLength) const override;

  /// Store a cipher descriptor (for key rotation support).
  rocksdb::Status AddCipher(std::string const& descriptor,
                            char const* cipher, size_t len,
                            bool for_write) override;

  /// Create a CTR cipher stream from a file's IV prefix.
  rocksdb::Status CreateCipherStream(
      std::string const& fname, rocksdb::EnvOptions const& options,
      rocksdb::Slice& prefix,
      std::unique_ptr<rocksdb::BlockAccessCipherStream>* result) override;

 private:
  std::string _key;  // 32-byte AES-256 key
};

}  // namespace enterprise
}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H
