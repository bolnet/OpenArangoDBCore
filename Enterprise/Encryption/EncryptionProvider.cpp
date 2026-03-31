#include "Enterprise/Encryption/EncryptionProvider.h"

// Application-level provider delegates entirely to the RocksDB-level
// EncryptionProvider.  No additional implementation needed here.
// See Enterprise/RocksDBEngine/EncryptionProvider.cpp for the full
// AES-256-CTR implementation.
