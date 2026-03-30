# Stack Research

**Domain:** C++ Enterprise module replacement for ArangoDB (drop-in `enterprise/` directory)
**Researched:** 2026-03-29
**Confidence:** MEDIUM-HIGH (core dependencies verified against ArangoDB devel branch VERSIONS file and official docs; ABI surface confirmed via GitHub source inspection)

---

## Context

OpenArangoDBCore is a static library (`openarangodb_enterprise`) that replaces ArangoDB's proprietary `enterprise/` directory. It is NOT a standalone application — it is compiled as a subdirectory inside ArangoDB's own build when `-DUSE_ENTERPRISE=1` is set. This shapes every stack decision:

- No package manager (apt/vcpkg/conan) installs — dependencies come from ArangoDB's build system
- RocksDB, VelocyPack, and ArangoDB internal headers are resolved via `${CMAKE_SOURCE_DIR}` (ArangoDB root)
- Only genuinely external deps (OpenSSL, OpenLDAP, librclone) need system-level resolution
- Testing stack is the one area that runs standalone (unit tests outside ArangoDB full build)

---

## Recommended Stack

### Core Technologies

| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++20 | ISO/IEC 14882:2020 | Implementation language | ArangoDB devel requires C++20; confirmed in VERSIONS file (`"C++ standard": "20"`). GCC 13.2+ and Clang 19.1+ are the tested compilers. |
| CMake | 3.20+ | Build system | ArangoDB's own minimum; `add_subdirectory(enterprise)` is how OpenArangoDBCore is consumed. Use `cmake_minimum_required(VERSION 3.20)` — already correct in current CMakeLists.txt. |
| RocksDB | 7.x (bundled) | Storage engine integration for encryption, hot backup, index building | ArangoDB bundles RocksDB; 3.12.x ships with RocksDB 7.2.0. Headers are available at `${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/include/`. Do NOT add a separate RocksDB dependency — link against ArangoDB's. |
| VelocyPack (velocypack) | Bundled with ArangoDB | Binary serialization format — used in ShardingStrategy::getResponsibleShard, all cluster/graph APIs | ArangoDB's own format. Headers at `${CMAKE_SOURCE_DIR}/3rdParty/velocypack/include/`. Written in C++20. All ArangoDB APIs pass `velocypack::Slice` and `velocypack::Builder`. Required for ShardingStrategyEE, SmartGraphSchema, SatelliteDistribution, ReadFromFollower. |
| OpenSSL | 3.5.x (system) | AES-256-CTR encryption for EncryptionFeature, EncryptionProvider, SSL/TLS enhancements | ArangoDB VERSIONS file pins `OPENSSL_LINUX: "3.5.5"`. Use EVP API (`EVP_aes_256_ctr()`, `EVP_CIPHER_CTX`). Provides hardware-accelerated CTR mode. Required by: EncryptionFeature, EncryptionProvider, RocksDBEncryptionUtils, SslServerFeatureEE. |

### Per-Module External Dependencies

| Module | Files | Dependency | Source |
|--------|-------|------------|--------|
| Encryption at Rest | EncryptionFeature, EncryptionProvider, RocksDBEncryptionUtils | OpenSSL 3.x (EVP), RocksDB `env_encryption.h` | System OpenSSL; ArangoDB-bundled RocksDB |
| Hot Backup | RocksDBHotBackup | RocksDB `utilities/checkpoint.h` (`Checkpoint::Create`, `CreateCheckpoint`) | ArangoDB-bundled RocksDB |
| Parallel Index Building | RocksDBBuilderIndexEE | RocksDB column family APIs, ArangoDB `RocksDBIndex` headers | ArangoDB source |
| LDAP Auth + External RBAC | LDAPHandler | OpenLDAP `libldap` (C API: `ldap_initialize`, `ldap_bind_s`, `ldap_search_ext_s`) | System `libldap-dev` / `openldap-devel` |
| Cloud Backup | RCloneFeature | `librclone` C shim (`RcloneRPC`, Go-built shared/static lib) or subprocess exec of `rclone` binary | System rclone 1.65.2 (per ArangoDB VERSIONS); librclone C bindings |
| SmartGraph Sharding | ShardingStrategyEE | ArangoDB `ShardingStrategy` base class (pure virtuals: `name()`, `usesDefaultShardKeys()`, `getResponsibleShard()`), `ResultT<ShardID>`, VelocyPack | ArangoDB source |
| SmartGraph Schema | SmartGraphSchema | ArangoDB `VocBase` headers, VelocyPack | ArangoDB source |
| Graph Provider/Steps | SmartGraphProvider, SmartGraphStep | ArangoDB `Graph/Providers/` and `Graph/Steps/` base classes | ArangoDB source |
| Cluster/Satellite | SatelliteDistribution, ReadFromFollower | ArangoDB `Cluster/` headers, `ServerState`, VelocyPack | ArangoDB source |
| AQL Extensions | LocalTraversalNode, MinHashFunctions | ArangoDB `Aql/` node/function registration APIs | ArangoDB source |
| MinHash Similarity | MinHashFunctions | Custom implementation (MurmurHash3 or xxHash for hash function); no external LSH library needed — ArangoDB expects AQL function registration interface | ArangoDB `Aql/Functions.h` for registration |
| TopK Optimization | IResearchOptimizeTopK | ArangoDB IResearch optimizer rule registration API; iresearch library bundled with ArangoDB | ArangoDB source, `3rdParty/iresearch/` |
| DC2DC Replication | DC2DCReplicator | ArangoDB `Replication/` base classes, HTTP client (fuerte, bundled) | ArangoDB source |
| Audit Logging | AuditFeature | `ApplicationFeature` base class (lifecycle: `prepare()`, `start()`, `stop()`); spdlog or ArangoDB's own logging (`Logger.h`) | ArangoDB source |
| License | LicenseFeature | `ApplicationFeature` base; constant returns, no external deps | ArangoDB source |
| Data Masking | AttributeMasking | ArangoDB VocBase/attribute APIs, VelocyPack | ArangoDB source |
| Enhanced SSL/TLS | SslServerFeatureEE | OpenSSL 3.x, ArangoDB `Ssl/SslFeature` base | OpenSSL system lib + ArangoDB source |

### Supporting Libraries (External, Not Bundled)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| OpenLDAP (`libldap`) | 2.6.x | LDAP bind, search, attribute extraction for LDAPHandler | Only for LDAP Auth and External RBAC modules. Install: `libldap-dev` (Debian/Ubuntu) or `openldap-devel` (RHEL/Fedora). Use the C API directly — no C++ wrapper needed at this scale. |
| librclone / rclone binary | 1.65.2 | Cloud backup (S3, Azure GCS) via RCloneFeature | ArangoDB VERSIONS pins rclone 1.65.2. Two integration strategies: (a) subprocess execution of the `rclone` binary via `popen()`/`exec()` — simpler, matches how ArangoDB Enterprise actually calls it; (b) `librclone` C shim compiled from Go source — complex, avoids subprocess. Use strategy (a) first. |
| xxHash | 0.8.x | Fast non-cryptographic hashing for MinHash permutations | Only for MinHashFunctions. Already bundled with many modern systems; check if ArangoDB bundles it at `3rdParty/xxhash/` before adding. |

### Testing Stack

| Tool | Version | Purpose | Notes |
|------|---------|---------|-------|
| GoogleTest (GTest + GMock) | 1.17.0 | Unit and mock-based testing for all 19 modules | Latest stable as of 2025. Requires C++17+; works with C++20. Integrate via CMake `FetchContent` in a separate `tests/CMakeLists.txt`. Do NOT build tests as part of ArangoDB's enterprise subdirectory — build them as a standalone test target. |
| CMake CTest | 3.20+ | Test runner, integrates with CI | Already provided by CMake. Use `enable_testing()` and `add_test()` or `gtest_discover_tests()`. |
| clang-tidy | 19.x (match compiler) | Static analysis: modernize-*, cppcoreguidelines-*, bugprone-* checks | Enable via `CMAKE_CXX_CLANG_TIDY` or `.clang-tidy` config file. Run in CI, not mandatory in local builds. Key checks: `bugprone-use-after-move`, `cppcoreguidelines-pro-type-cstyle-cast`, `modernize-use-override`. |
| clang-format | 19.x | Code style enforcement | ArangoDB has its own `.clang-format` at root; mirror it or reference it. |
| AddressSanitizer (ASan) | Built into Clang/GCC | Memory safety testing | Enable with `-fsanitize=address` in test builds only. Typical 2x slowdown acceptable for CI. |
| ThreadSanitizer (TSan) | Built into Clang/GCC | Data race detection in cluster/replication modules | Use specifically for DC2DCReplicator, ReadFromFollower, SatelliteDistribution tests. 5-15x slowdown — run in separate CI job. |

---

## ArangoDB Internal API Surface (What We Implement Against)

These are the ArangoDB headers OpenArangoDBCore must match. They are not external libraries — they are ArangoDB source paths resolved via `${CMAKE_SOURCE_DIR}`.

| Header Path (relative to ArangoDB root) | Used By Our Module |
|------------------------------------------|--------------------|
| `lib/ApplicationFeatures/ApplicationFeature.h` | AuditFeature, LicenseFeature, EncryptionFeature, RCloneFeature, SslServerFeatureEE |
| `arangod/Sharding/ShardingStrategy.h` | ShardingStrategyEE |
| `arangod/RocksDBEngine/RocksDBIndex.h` | RocksDBBuilderIndexEE |
| `3rdParty/rocksdb/include/rocksdb/env_encryption.h` | EncryptionProvider, RocksDBEncryptionUtils |
| `3rdParty/rocksdb/include/rocksdb/utilities/checkpoint.h` | RocksDBHotBackup |
| `3rdParty/velocypack/include/velocypack/` | ShardingStrategyEE, SmartGraphSchema, SatelliteDistribution, ReadFromFollower |
| `arangod/Aql/AstNode.h`, `ExecutionNode.h` | LocalTraversalNode |
| `arangod/Aql/Functions.h` | MinHashFunctions |
| `arangod/IResearch/IResearchFeature.h` | IResearchOptimizeTopK |
| `arangod/Graph/Providers/BaseProviderOptions.h` | SmartGraphProvider |
| `arangod/Cluster/ServerState.h`, `ClusterInfo.h` | SatelliteDistribution, ReadFromFollower |
| `arangod/Replication/ReplicationApplierConfiguration.h` | DC2DCReplicator |
| `arangod/Auth/AuthInfo.h` | LDAPHandler |
| `arangod/VocBase/LogicalCollection.h` | SmartGraphSchema, AttributeMasking |
| `arangod/Ssl/SslFeature.h` | SslServerFeatureEE |

---

## Installation

The testing stack is the only piece needing explicit installation:

```bash
# System dependencies (Debian/Ubuntu)
sudo apt-get install -y libldap-dev libssl-dev

# System dependencies (RHEL/Fedora)
sudo dnf install -y openldap-devel openssl-devel

# GoogleTest (integrated via CMake FetchContent — no manual install)
# Add to tests/CMakeLists.txt:
# include(FetchContent)
# FetchContent_Declare(googletest
#   URL https://github.com/google/googletest/archive/v1.17.0.tar.gz
# )
# FetchContent_MakeAvailable(googletest)

# rclone binary (for RCloneFeature integration tests)
# Use ArangoDB's pinned version: 1.65.2
curl -O https://downloads.rclone.org/v1.65.2/rclone-v1.65.2-linux-amd64.zip
```

---

## CMake Structure Recommendation

The current `CMakeLists.txt` needs augmentation for testing. The library target itself stays minimal:

```cmake
# In CMakeLists.txt (enterprise/ build — runs inside ArangoDB)
cmake_minimum_required(VERSION 3.20)
project(OpenArangoDBCore VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find external system deps (OpenSSL, LDAP)
find_package(OpenSSL 3.0 REQUIRED)
find_package(LDAP)  # optional guard: if LDAP_FOUND

# Static library target (unchanged)
add_library(openarangodb_enterprise STATIC ...)
target_include_directories(openarangodb_enterprise PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
)
target_link_libraries(openarangodb_enterprise
  OpenSSL::SSL OpenSSL::Crypto
  $<$<BOOL:${LDAP_FOUND}>:ldap>
)
target_compile_definitions(openarangodb_enterprise PUBLIC USE_ENTERPRISE=1)

# Tests (standalone, only when building outside ArangoDB)
if(CMAKE_PROJECT_NAME STREQUAL "OpenArangoDBCore")
  enable_testing()
  add_subdirectory(tests)
endif()
```

---

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| GoogleTest 1.17.0 | Catch2 v3 | Catch2 is excellent but GoogleTest's `GMock` is critical for mocking ArangoDB internal interfaces (ApplicationFeature, ShardingStrategy); GMock saves significant stub-writing effort |
| GoogleTest 1.17.0 | Boost.Test | Boost adds heavy dependency footprint; not warranted for test-only use |
| OpenLDAP C API directly | ldap-cpp wrapper | Wrappers are unmaintained (last commit 2019-2021); C API is stable, well-documented, and what ArangoDB Enterprise itself uses |
| Subprocess exec for rclone | librclone C shim | librclone requires Go toolchain at build time, adds complexity; ArangoDB Enterprise uses subprocess approach per architecture docs |
| Custom MinHash (xxHash + permutation) | LSHBOX / LSHKIT | These are full LSH libraries; ArangoDB needs a specific AQL function registration — a custom 200-line implementation is cleaner than pulling an external library for 2 functions |
| clang-tidy + clang-format | cppcheck | clang-tidy integrates with CMake natively via `CMAKE_CXX_CLANG_TIDY`; has richer C++20-aware checks |

---

## What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Separate RocksDB installation / vcpkg/conan RocksDB | ArangoDB bundles a patched RocksDB; linking against a different version will cause ABI mismatches and linker symbol conflicts | Use `${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/` headers and ArangoDB's `rocksdb` target |
| Standalone VelocyPack installation | Same reason — ArangoDB's bundled copy may diverge from the upstream repo | Use `${CMAKE_SOURCE_DIR}/3rdParty/velocypack/` |
| C++ LDAP wrapper libraries (`ldap-cpp`, `libldap--`) | All are unmaintained (2019 or earlier); insufficient TLS/STARTTLS support | OpenLDAP C API (`ldap.h`) directly — stable, well-tested, what ArangoDB Enterprise itself uses |
| Boost (any component) | ArangoDB does not use Boost in its modern codebase; adds massive compile-time overhead | C++20 standard library covers ranges, chrono, optional, variant, filesystem |
| fmt library / spdlog (standalone) | ArangoDB has its own Logger (`lib/Logger/`) — use it for audit logging to ensure output goes to ArangoDB's logging infrastructure, not a separate sink | `arangodb::Logger` via `#include "Logger/LogMacros.h"` |
| CMake < 3.20 | `FetchContent` improvements, `cmake_path()`, better `target_sources()` are needed | CMake 3.20+ (already specified) |
| GoogleTest < 1.15 | Older versions don't build cleanly with C++20 `-Werror` flags; deprecation warnings on TYPED_TEST | GoogleTest 1.17.0 |

---

## Version Compatibility Matrix

| Package | Compatible With | Notes |
|---------|-----------------|-------|
| GCC 13.2.0 | C++20, GoogleTest 1.17, OpenSSL 3.x, OpenLDAP 2.6 | ArangoDB's pinned Linux compiler |
| Clang 19.1.7 | C++20, clang-tidy 19.x, ASan/TSan | ArangoDB's pinned Clang; use matching clang-tidy version |
| OpenSSL 3.5.x | AES-256-CTR EVP API stable since 3.0; TLS 1.3 default | Breaking change from 1.x: `EVP_MD_CTX_new()` replaces `EVP_MD_CTX_create()`; use 3.x API throughout |
| RocksDB 7.2.x | `env_encryption.h` with `EncryptionProvider` as `Customizable` subclass; `Checkpoint::Create()` in `utilities/checkpoint.h` | ArangoDB 3.12 bundled version; API stable since 6.14 |
| OpenLDAP 2.6.x | TLS via `ldap_start_tls_s()`; SASL via `ldap_sasl_bind_s()` | OpenLDAP 2.7 (roadmap: Fall 2025) adds functional enhancements but 2.6 API is fully compatible |
| GoogleTest 1.17.0 | C++17 minimum (works with C++20); GMock included | Last tested: v1.17.0 (2025). Requires `cmake 3.20+` for `gtest_discover_tests()`. |
| rclone | 1.65.2 | ArangoDB VERSIONS file pins this exact version. Go 1.25.8 for building librclone C shim if needed. |

---

## Sources

- [ArangoDB devel VERSIONS file](https://github.com/arangodb/arangodb/blob/devel/VERSIONS) — Compiler versions (GCC 13.2.0, Clang 19.1.7), OpenSSL 3.5.5, rclone 1.65.2. HIGH confidence.
- [ArangoDB ApplicationFeature.h](https://github.com/arangodb/arangodb/blob/devel/lib/ApplicationFeatures/ApplicationFeature.h) — Feature lifecycle hooks (prepare/start/stop), constructor pattern. HIGH confidence.
- [ArangoDB ShardingStrategy.h](https://github.com/arangodb/arangodb/blob/devel/arangod/Sharding/ShardingStrategy.h) — Pure virtual interface for sharding strategies. HIGH confidence.
- [RocksDB env_encryption.h](https://github.com/facebook/rocksdb/blob/main/include/rocksdb/env_encryption.h) — EncryptionProvider virtual interface. HIGH confidence.
- [RocksDB Checkpoints wiki](https://github.com/facebook/rocksdb/wiki/Checkpoints) — `Checkpoint::Create()` and `CreateCheckpoint()` API. HIGH confidence.
- [GoogleTest v1.17.0 release](https://github.com/google/googletest/releases/tag/v1.17.0) — Latest stable, C++17+ required. HIGH confidence.
- [VelocyPack GitHub](https://github.com/arangodb/velocypack) — C++20 library, Builder/Slice API. HIGH confidence.
- [OpenSSL EVP Symmetric Encryption](https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption) — EVP AES-256-CTR API. HIGH confidence.
- [OpenLDAP C++ API](https://www.openldap.org/) — libldap 2.6 stable, C API. MEDIUM confidence (no version file found in ArangoDB VERSIONS for LDAP).
- [rclone librclone README](https://github.com/rclone/rclone/blob/master/librclone/README.md) — C shim approach. MEDIUM confidence.
- [ArangoDB DC2DC architecture](https://www.arangodb.com/docs/stable/architecture-deployment-modes-dc2-dc-introduction.html) — DirectMQ / subprocess architecture. MEDIUM confidence (C++ interface not in public docs).
- [clang-tidy CMake integration](https://www.studyplan.dev/cmake/cmake-clang-tidy) — `CMAKE_CXX_CLANG_TIDY` setup. HIGH confidence.

---

*Stack research for: OpenArangoDBCore — ArangoDB Enterprise C++ module replacement*
*Researched: 2026-03-29*
