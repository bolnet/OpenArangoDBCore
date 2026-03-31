#pragma once
#ifndef ARANGODB_ROCKSDB_ENGINE_EE_H
#define ARANGODB_ROCKSDB_ENGINE_EE_H

#include <memory>

#include <rocksdb/env_encryption.h>

namespace arangodb {
namespace enterprise {

/// Enterprise-specific data attached to the RocksDB engine.
///
/// Stores the encryption provider using both a shared_ptr (for ownership)
/// and a raw pointer (for the fast accessor path).  This follows ArangoDB's
/// pattern where the engine holds the shared_ptr and passes the raw pointer
/// to NewEncryptedEnv().
struct RocksDBEngineEEData {
  /// Owning reference to the encryption provider (keeps it alive).
  std::shared_ptr<rocksdb::EncryptionProvider> _provider;

  /// Non-owning accessor (passed to rocksdb::NewEncryptedEnv).
  rocksdb::EncryptionProvider* _encryptionProvider = nullptr;
};

}  // namespace enterprise
}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_ENGINE_EE_H
