#pragma once
#ifndef ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H
#define ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H

namespace arangodb {
namespace enterprise {

class EncryptionProvider {
 public:
  EncryptionProvider() = default;
  virtual ~EncryptionProvider() = default;
};

}  // namespace enterprise
}  // namespace arangodb

#endif  // ARANGODB_ROCKSDB_ENCRYPTION_PROVIDER_H
