#pragma once
#ifndef ARANGODB_ROCKSDB_ENCRYPTION_UTILS_H
#define ARANGODB_ROCKSDB_ENCRYPTION_UTILS_H

#include <optional>
#include <string>
#include <vector>

namespace arangodb {
namespace enterprise {

/// Holds an encryption key and its identifier (for key rotation).
struct EncryptionSecret {
  std::string key;  // Raw key bytes (must be exactly 32 bytes for AES-256)
  std::string id;   // Key identifier (e.g., filename or version tag)
};

/// Load a single 32-byte encryption key from a file.
/// Returns nullopt if the file cannot be read or is not exactly 32 bytes.
std::optional<EncryptionSecret> loadKeyFromFile(std::string const& path);

/// Load all key files from a directory for key rotation support.
/// Each file in the directory should contain a 32-byte raw key.
/// Files that are not exactly 32 bytes are skipped.
std::vector<EncryptionSecret> loadKeysFromFolder(std::string const& path);

}  // namespace enterprise
}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_ENCRYPTION_UTILS_H
