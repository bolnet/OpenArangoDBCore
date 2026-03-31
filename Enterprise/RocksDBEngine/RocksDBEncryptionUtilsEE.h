#pragma once
#ifndef ARANGODB_ROCKSDB_ENCRYPTION_UTILS_EE_H
#define ARANGODB_ROCKSDB_ENCRYPTION_UTILS_EE_H

#include "Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h"

namespace arangodb {
namespace enterprise {

/// Enterprise-specific key rotation helpers.
///
/// These extend the base key loading utilities with rotation-aware
/// operations (e.g., re-encrypting SST files with a new key).
/// Currently delegates to loadKeysFromFolder() from RocksDBEncryptionUtils.

/// Check if key rotation is needed (new key available in keyfolder).
inline bool hasNewRotationKey(std::string const& keyfolderPath,
                              std::string const& currentKeyId) {
  auto keys = loadKeysFromFolder(keyfolderPath);
  for (auto const& k : keys) {
    if (k.id != currentKeyId) {
      return true;
    }
  }
  return false;
}

}  // namespace enterprise
}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_ENCRYPTION_UTILS_EE_H
