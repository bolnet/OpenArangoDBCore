#pragma once
#ifndef ARANGODB_ROCKSDB_MOCKS_H
#define ARANGODB_ROCKSDB_MOCKS_H

/// Mock RocksDB types for testing encryption without a real RocksDB instance.
///
/// Provides minimal stubs for:
/// - rocksdb::Slice
/// - rocksdb::Status
/// - rocksdb::EnvOptions
/// - rocksdb::Customizable
/// - rocksdb::EncryptionProvider (abstract base)
/// - rocksdb::BlockAccessCipherStream (abstract base)
///
/// These allow EncryptionProvider tests to compile and run standalone.

#include <cstddef>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace rocksdb {

// ---- Status ----
class Status {
 public:
  Status() : code_(kOk) {}
  static Status OK() { return Status(); }
  static Status NotSupported(std::string const& msg = "") {
    return Status(kNotSupported, msg);
  }
  static Status InvalidArgument(std::string const& msg = "") {
    return Status(kInvalidArgument, msg);
  }
  static Status IOError(std::string const& msg = "") {
    return Status(kIOError, msg);
  }

  bool ok() const { return code_ == kOk; }
  bool IsNotSupported() const { return code_ == kNotSupported; }
  bool IsInvalidArgument() const { return code_ == kInvalidArgument; }
  bool IsIOError() const { return code_ == kIOError; }
  std::string ToString() const { return msg_; }

 private:
  enum Code { kOk, kNotSupported, kInvalidArgument, kIOError };
  Code code_;
  std::string msg_;
  Status(Code c, std::string msg = "") : code_(c), msg_(std::move(msg)) {}
};

// ---- Slice ----
class Slice {
 public:
  Slice() : data_(""), size_(0) {}
  Slice(char const* d, size_t n) : data_(d), size_(n) {}
  Slice(char const* s) : data_(s), size_(std::strlen(s)) {}
  Slice(std::string const& s) : data_(s.data()), size_(s.size()) {}

  char const* data() const { return data_; }
  size_t size() const { return size_; }
  std::string ToString() const { return std::string(data_, size_); }

  void clear() { data_ = ""; size_ = 0; }

 private:
  char const* data_;
  size_t size_;
};

// ---- EnvOptions ----
struct EnvOptions {
  bool use_mmap_reads = false;
  bool use_mmap_writes = false;
};

// ---- Customizable ----
class Customizable {
 public:
  virtual ~Customizable() = default;
  virtual char const* Name() const { return "Customizable"; }
};

// ---- BlockAccessCipherStream ----
class BlockAccessCipherStream {
 public:
  virtual ~BlockAccessCipherStream() = default;

  // Encrypt one or more (typically block-aligned) bytes at fileOffset.
  virtual Status Encrypt(uint64_t fileOffset, char* data,
                         size_t dataSize) = 0;

  // Decrypt one or more (typically block-aligned) bytes at fileOffset.
  virtual Status Decrypt(uint64_t fileOffset, char* data,
                         size_t dataSize) = 0;

  // Block size for the cipher (0 = stream cipher).
  virtual size_t BlockSize() { return 0; }
};

// ---- EncryptionProvider ----
class EncryptionProvider : public Customizable {
 public:
  ~EncryptionProvider() override = default;

  char const* Name() const override { return "EncryptionProvider"; }

  // Returns the length of the prefix that is stored at the beginning of
  // each encrypted file.  Typically the IV length.
  virtual size_t GetPrefixLength() const = 0;

  // Generate prefix (IV) for a new file.
  virtual Status CreateNewPrefix(std::string const& fname, char* prefix,
                                 size_t prefixLength) const = 0;

  // Add a cipher key.
  virtual Status AddCipher(std::string const& descriptor, char const* cipher,
                           size_t len, bool for_write) = 0;

  // Create a cipher stream for reading/writing a file with the given prefix.
  virtual Status CreateCipherStream(
      std::string const& fname, EnvOptions const& options, Slice& prefix,
      std::unique_ptr<BlockAccessCipherStream>* result) = 0;
};

}  // namespace rocksdb

#endif  // ARANGODB_ROCKSDB_MOCKS_H
