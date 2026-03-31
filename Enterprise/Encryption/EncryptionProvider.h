#pragma once
#ifndef ARANGODB_ENCRYPTION_PROVIDER_H
#define ARANGODB_ENCRYPTION_PROVIDER_H

// Application-level encryption provider.
//
// This header re-exports the RocksDB-level EncryptionProvider from
// Enterprise/RocksDBEngine/EncryptionProvider.h.  EncryptionFeature
// creates the RocksDB-level provider directly -- no separate
// application-level wrapper is needed since the RocksDB provider
// already handles the full lifecycle.

#include "Enterprise/RocksDBEngine/EncryptionProvider.h"

#endif  // ARANGODB_ENCRYPTION_PROVIDER_H
