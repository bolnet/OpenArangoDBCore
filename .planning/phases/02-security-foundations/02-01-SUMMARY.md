---
phase: 02-security-foundations
plan: 01
subsystem: encryption
tags: [aes-256-ctr, openssl, rocksdb, encryption-at-rest, evp, rand-bytes]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: Enterprise/ directory layout, mock headers, CMake build system
provides:
  - RocksDB EncryptionProvider with AES-256-CTR via OpenSSL EVP
  - Key management utilities (keyfile loading, keyfolder rotation)
  - EncryptionFeature ApplicationFeature with option parsing and lifecycle
  - RocksDBEngineEEData with shared_ptr provider ownership pattern
  - Mock RocksDB headers for standalone test compilation
affects: [02-security-foundations, 05-dc2dc-replication]

# Tech tracking
tech-stack:
  added: [OpenSSL::Crypto]
  patterns: [EVP cipher API, RAND_bytes IV generation, CTR mode random access]

key-files:
  created:
    - Enterprise/RocksDBEngine/EncryptionProvider.cpp
    - tests/unit/EncryptionProviderTest.cpp
    - tests/unit/EncryptionFeatureTest.cpp
    - tests/mocks/RocksDBMocks.h
    - tests/mocks/rocksdb/env_encryption.h
  modified:
    - Enterprise/RocksDBEngine/EncryptionProvider.h
    - Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h
    - Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp
    - Enterprise/RocksDBEngine/RocksDBEncryptionUtilsEE.h
    - Enterprise/RocksDBEngine/RocksDBEngineEE.h
    - Enterprise/Encryption/EncryptionFeature.h
    - Enterprise/Encryption/EncryptionFeature.cpp
    - Enterprise/Encryption/EncryptionProvider.h
    - Enterprise/Encryption/EncryptionProvider.cpp
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Single EncryptionProvider class serves both RocksDB and application level -- no separate wrapper needed"
  - "Tests compile encryption sources directly rather than linking full enterprise library to avoid header dependency cascade"
  - "CTR counter uses big-endian addition to IV for block offset calculation, matching NIST standard"
  - "Key material is securely zeroed in AESCTRCipherStream destructor"

patterns-established:
  - "OpenSSL EVP pattern: EVP_CIPHER_CTX_new -> EVP_EncryptInit_ex -> EVP_EncryptUpdate -> EVP_CIPHER_CTX_free"
  - "RocksDB provider ownership: shared_ptr in feature + raw pointer in EEData for NewEncryptedEnv"
  - "Standalone test compilation: compile specific .cpp files instead of linking full enterprise library"

requirements-completed: [ENCR-01, ENCR-02, ENCR-03, ENCR-04, ENCR-05]

# Metrics
duration: 33min
completed: 2026-03-31
---

# Phase 2 Plan 1: RocksDB Encryption at Rest Summary

**AES-256-CTR encryption provider with OpenSSL EVP, per-file RAND_bytes IV generation, keyfile loading with 32-byte validation, and EncryptionFeature lifecycle hooks**

## Performance

- **Duration:** 33 min
- **Started:** 2026-03-31T20:19:57Z
- **Completed:** 2026-03-31T20:52:40Z
- **Tasks:** 2
- **Files modified:** 15

## Accomplishments
- Full rocksdb::EncryptionProvider implementation with AES-256-CTR using OpenSSL EVP API
- Random-access encrypt/decrypt at arbitrary file offsets (CTR mode property verified)
- Per-file IV generation via RAND_bytes with uniqueness verification across 100 iterations
- NIST SP 800-38A test vector validation for AES-256-CTR correctness
- Key management: loadKeyFromFile validates 32-byte length, loadKeysFromFolder for rotation
- EncryptionFeature registers command-line options, loads keyfile in prepare(), cleans up in unprepare()
- 16 total tests (10 provider + 6 feature), all passing

## Task Commits

Each task was committed atomically:

1. **Task 1: RocksDB EncryptionProvider with AES-256-CTR** (TDD)
   - `864c3e1` (test: add failing tests for encryption provider - RED)
   - `dec28bd` (feat: implement EncryptionProvider with AES-256-CTR - GREEN)

2. **Task 2: EncryptionFeature ApplicationFeature lifecycle** (TDD)
   - `2df3e56` (test: add failing tests for EncryptionFeature lifecycle - RED)
   - `a52bb71` (feat: implement EncryptionFeature lifecycle with keyfile loading - GREEN)

## Files Created/Modified
- `Enterprise/RocksDBEngine/EncryptionProvider.h` - rocksdb::EncryptionProvider subclass with AESCTRCipherStream
- `Enterprise/RocksDBEngine/EncryptionProvider.cpp` - Full AES-256-CTR implementation using OpenSSL EVP
- `Enterprise/RocksDBEngine/RocksDBEncryptionUtils.h` - EncryptionSecret struct, loadKeyFromFile, loadKeysFromFolder
- `Enterprise/RocksDBEngine/RocksDBEncryptionUtils.cpp` - Key file I/O with 32-byte validation
- `Enterprise/RocksDBEngine/RocksDBEncryptionUtilsEE.h` - Key rotation helper (hasNewRotationKey)
- `Enterprise/RocksDBEngine/RocksDBEngineEE.h` - shared_ptr + raw pointer for provider ownership
- `Enterprise/Encryption/EncryptionFeature.h` - Full feature with option parsing, accessors, setters
- `Enterprise/Encryption/EncryptionFeature.cpp` - Lifecycle: collectOptions, validateOptions, prepare, unprepare
- `Enterprise/Encryption/EncryptionProvider.h` - Re-export of RocksDB-level provider
- `tests/unit/EncryptionProviderTest.cpp` - 10 tests including NIST vector
- `tests/unit/EncryptionFeatureTest.cpp` - 6 tests for feature lifecycle
- `tests/mocks/RocksDBMocks.h` - Mock rocksdb::Status, Slice, EncryptionProvider, etc.
- `tests/mocks/rocksdb/env_encryption.h` - Redirect to RocksDBMocks.h
- `CMakeLists.txt` - Added find_package(OpenSSL), linked OpenSSL::Crypto
- `tests/CMakeLists.txt` - Added encryption_provider_test and encryption_feature_test targets

## Decisions Made
- Single EncryptionProvider class serves both RocksDB and application level -- the plan's Enterprise/Encryption/EncryptionProvider.h just re-exports the RocksDB-level one since no separate wrapper logic was needed
- Tests compile encryption sources directly rather than linking the full openarangodb_enterprise library, avoiding a cascade of missing headers from unrelated modules
- CTR counter uses big-endian addition matching NIST standard behavior for counter block incrementing
- Key material is securely zeroed in the AESCTRCipherStream destructor via volatile pointer writes

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Created mock GeneralServer/SslServerFeature.h**
- **Found during:** Task 1 (build phase)
- **Issue:** Enterprise library compilation requires SslServerFeature.h which is not available in standalone builds
- **Fix:** Created minimal mock at tests/mocks/GeneralServer/SslServerFeature.h
- **Files modified:** tests/mocks/GeneralServer/SslServerFeature.h
- **Verification:** Build succeeds with mock headers

**2. [Rule 3 - Blocking] Fixed unused function warning in AttributeMasking.cpp**
- **Found during:** Task 1 (build phase)
- **Issue:** Pre-existing -Werror flagged unused `trim()` function in AttributeMasking.cpp
- **Fix:** Added [[maybe_unused]] attribute
- **Files modified:** Enterprise/Maskings/AttributeMasking.cpp
- **Verification:** Build compiles without warnings

**3. [Rule 3 - Blocking] Changed test compilation strategy from library linking to source compilation**
- **Found during:** Task 1 (build phase)
- **Issue:** Linking openarangodb_enterprise pulled in all enterprise modules requiring many missing mock headers
- **Fix:** Compile only the encryption .cpp files directly in the test target
- **Files modified:** tests/CMakeLists.txt
- **Verification:** Both test targets build and all tests pass

---

**Total deviations:** 3 auto-fixed (all Rule 3 - blocking issues)
**Impact on plan:** All fixes were necessary to unblock compilation. No scope creep.

## Issues Encountered
None beyond the blocking issues documented above.

## User Setup Required
None - no external service configuration required.

## Next Phase Readiness
- Encryption provider is ready for integration with RocksDB engine via NewEncryptedEnv()
- EncryptionFeature can be registered as ApplicationFeature via addFeature<EncryptionFeature>()
- Key rotation infrastructure (keyfolder loading) is in place for future API-based rotation
- RocksDBEngineEEData provides the shared_ptr + raw pointer pattern needed by the engine

---
*Phase: 02-security-foundations*
*Completed: 2026-03-31*
