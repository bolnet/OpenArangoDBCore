#include "Enterprise/RocksDBEngine/EncryptionProvider.h"

#include <openssl/evp.h>
#include <openssl/rand.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace arangodb {
namespace enterprise {

// ============================================================
// AESCTRCipherStream
// ============================================================

AESCTRCipherStream::AESCTRCipherStream(std::string const& key,
                                       std::string const& iv)
    : _key(key), _iv(iv) {
  if (_key.size() != 32) {
    throw std::invalid_argument("AES-256-CTR requires a 32-byte key");
  }
  if (_iv.size() != 16) {
    throw std::invalid_argument("AES-256-CTR requires a 16-byte IV");
  }
}

AESCTRCipherStream::~AESCTRCipherStream() {
  // Securely zero the key material
  volatile char* p = _key.data();
  for (size_t i = 0; i < _key.size(); ++i) {
    p[i] = 0;
  }
}

rocksdb::Status AESCTRCipherStream::Encrypt(uint64_t fileOffset, char* data,
                                            size_t dataSize) {
  return ctrTransform(fileOffset, data, dataSize);
}

rocksdb::Status AESCTRCipherStream::Decrypt(uint64_t fileOffset, char* data,
                                            size_t dataSize) {
  // CTR mode: encryption and decryption are the same operation
  return ctrTransform(fileOffset, data, dataSize);
}

rocksdb::Status AESCTRCipherStream::ctrTransform(uint64_t fileOffset,
                                                 char* data,
                                                 size_t dataSize) {
  if (dataSize == 0) {
    return rocksdb::Status::OK();
  }

  // Compute the counter value for the given file offset.
  // In CTR mode, the counter = IV + (offset / blockSize).
  // AES block size is 16 bytes.
  constexpr size_t kBlockSize = 16;
  uint64_t blockIndex = fileOffset / kBlockSize;
  size_t blockOffset = fileOffset % kBlockSize;

  // Build the initial counter for this offset by adding blockIndex to the IV.
  // The IV is treated as a 128-bit big-endian integer.
  unsigned char counter[kBlockSize];
  std::memcpy(counter, _iv.data(), kBlockSize);

  // Add blockIndex to the counter (big-endian addition)
  uint64_t carry = blockIndex;
  for (int i = static_cast<int>(kBlockSize) - 1; i >= 0 && carry > 0; --i) {
    uint64_t sum = static_cast<uint64_t>(counter[i]) + (carry & 0xFF);
    counter[i] = static_cast<unsigned char>(sum & 0xFF);
    carry = (carry >> 8) + (sum >> 8);
  }

  // If we start mid-block, we need to generate the keystream for the
  // partial first block and skip the initial bytes.
  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return rocksdb::Status::IOError("Failed to create EVP context");
  }

  int rc = EVP_EncryptInit_ex(
      ctx, EVP_aes_256_ctr(), nullptr,
      reinterpret_cast<unsigned char const*>(_key.data()), counter);
  if (rc != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return rocksdb::Status::IOError("EVP_EncryptInit_ex failed");
  }

  // If starting mid-block, consume the partial block prefix to advance
  // the keystream to the correct position.
  if (blockOffset > 0) {
    unsigned char scratch[kBlockSize];
    std::memset(scratch, 0, kBlockSize);
    int outLen = 0;
    rc = EVP_EncryptUpdate(ctx, scratch, &outLen, scratch,
                           static_cast<int>(blockOffset));
    if (rc != 1) {
      EVP_CIPHER_CTX_free(ctx);
      return rocksdb::Status::IOError("EVP_EncryptUpdate (skip) failed");
    }
  }

  // XOR the actual data with the keystream
  int outLen = 0;
  rc = EVP_EncryptUpdate(ctx, reinterpret_cast<unsigned char*>(data), &outLen,
                         reinterpret_cast<unsigned char const*>(data),
                         static_cast<int>(dataSize));
  if (rc != 1) {
    EVP_CIPHER_CTX_free(ctx);
    return rocksdb::Status::IOError("EVP_EncryptUpdate failed");
  }

  EVP_CIPHER_CTX_free(ctx);
  return rocksdb::Status::OK();
}

// ============================================================
// EncryptionProvider
// ============================================================

EncryptionProvider::EncryptionProvider(std::string const& key) : _key(key) {
  if (_key.size() != 32) {
    throw std::invalid_argument(
        "EncryptionProvider requires a 32-byte AES-256 key");
  }
}

size_t EncryptionProvider::GetPrefixLength() const {
  return 16;  // AES block size = IV length
}

rocksdb::Status EncryptionProvider::CreateNewPrefix(
    std::string const& /*fname*/, char* prefix, size_t prefixLength) const {
  if (prefixLength < 16) {
    return rocksdb::Status::InvalidArgument("Prefix too short for IV");
  }

  // Generate cryptographically secure random IV via OpenSSL RAND_bytes.
  // NEVER use rand(), std::random_device, or other non-cryptographic RNGs.
  int rc = RAND_bytes(reinterpret_cast<unsigned char*>(prefix),
                      static_cast<int>(prefixLength));
  if (rc != 1) {
    return rocksdb::Status::IOError("RAND_bytes failed to generate IV");
  }

  return rocksdb::Status::OK();
}

rocksdb::Status EncryptionProvider::AddCipher(std::string const& /*descriptor*/,
                                              char const* /*cipher*/,
                                              size_t /*len*/,
                                              bool /*for_write*/) {
  // Key rotation support: in a full implementation this would store
  // multiple cipher descriptors.  For now, the single key passed at
  // construction is used for all operations.
  return rocksdb::Status::OK();
}

rocksdb::Status EncryptionProvider::CreateCipherStream(
    std::string const& /*fname*/, rocksdb::EnvOptions const& /*options*/,
    rocksdb::Slice& prefix,
    std::unique_ptr<rocksdb::BlockAccessCipherStream>* result) {
  if (prefix.size() < 16) {
    return rocksdb::Status::InvalidArgument("Prefix too short for IV");
  }

  std::string iv(prefix.data(), 16);
  *result = std::make_unique<AESCTRCipherStream>(_key, iv);
  return rocksdb::Status::OK();
}

}  // namespace enterprise
}  // namespace arangodb
