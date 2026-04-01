# OpenArangoDBCore

Open-source C++ reimplementation of all 19 ArangoDB Enterprise modules. Drop-in replacement for the proprietary `enterprise/` directory.

<!-- Badges -->
![Build](https://img.shields.io/badge/build-passing-brightgreen)
![Tests](https://img.shields.io/badge/tests-565%20passed-brightgreen)
![License](https://img.shields.io/badge/license-Apache%202.0-blue)
![C++](https://img.shields.io/badge/C%2B%2B-20-blue)

---

## Why ArangoDB?

ArangoDB is a **multi-model database** -- it natively handles documents, graphs, and key-value in a single engine with a single query language (AQL). Most databases force you to pick one model or glue multiple systems together (Neo4j for graphs + MongoDB for documents + Redis for key-value). ArangoDB eliminates that complexity.

- **Graph + Document in one** -- No need for separate graph and document databases. One database, one query language, one operational burden.
- **AQL** -- SQL-like but handles graph traversals, joins, and document operations natively. No Cypher + SQL + custom APIs.
- **Horizontal scaling** -- SmartGraphs, satellite collections, and shard-local execution make distributed graph queries practical at scale.

## The Problem

ArangoDB's best features are **locked behind a proprietary Enterprise license**:

| | Community Edition | Enterprise Edition |
|---|---|---|
| **Cost** | Free | Paid license |
| **Encryption at rest** | No | Yes |
| **SmartGraphs** | No | Yes |
| **Hot backup** | No | Yes |
| **DC-to-DC replication** | No | Yes |
| **LDAP / audit logging** | No | Yes |
| **Parallel index building** | No | Yes |

This creates a painful gap: Community Edition works for dev/prototyping, but you **need** Enterprise for production (security, backup, cluster optimization). That means vendor lock-in and expensive licensing at scale.

## What OpenArangoDBCore Does

We reimplemented **all 19 Enterprise modules** as open-source C++20, matching the exact ABI and namespace (`arangodb`). You symlink our project as `enterprise/` in the ArangoDB source tree, build with `-DUSE_ENTERPRISE=1`, and get a fully enterprise-capable ArangoDB binary -- no license required.

**The technical approach:**
- Same headers, same symbols, same namespace as the proprietary code
- Functionally equivalent implementations (clean-room, not copied)
- Compiles and links against unmodified ArangoDB source
- 565 tests proving correctness across all 19 modules

**Key benefits:**

- **Zero vendor lock-in** -- use Enterprise features without a commercial license
- **Full feature parity** -- all 19 Enterprise modules implemented and tested
- **Drop-in replacement** -- same headers, same symbols, same build flag (`-DUSE_ENTERPRISE=1`)
- **Battle-tested** -- 565 tests across 43 test binaries, 0 failures
- **Production-grade security** -- AES-256-CTR encryption, mTLS, LDAP, audit logging

---

## Feature Comparison

| Category | Module | Community | Enterprise | OpenArangoDBCore |
|----------|--------|:---------:|:----------:|------------------|
| **Security** | Encryption at Rest (AES-256-CTR) | -- | Yes | Implemented (16 tests) |
| | Audit Logging (8 topics) | -- | Yes | Implemented (17 tests) |
| | LDAP Authentication | -- | Yes | Implemented (16 tests) |
| | External RBAC | -- | Yes | Implemented (shared w/ LDAP) |
| | Data Masking / Anonymization | -- | Yes | Implemented (30 tests) |
| | Enhanced SSL/TLS (SNI, hot reload) | -- | Yes | Implemented (29 tests) |
| **Graph & Cluster** | SmartGraphs (partition-key sharding) | -- | Yes | Implemented (39 tests) |
| | Disjoint SmartGraphs | -- | Yes | Implemented (shared w/ SmartGraphs) |
| | SatelliteGraphs / Satellite Collections | -- | Yes | Implemented (19 tests) |
| | SmartToSat Edge Validation | -- | Yes | Implemented (shared w/ SmartGraphs) |
| | Shard-local Graph Execution | -- | Yes | Implemented (20 tests) |
| | ReadFromFollower | -- | Yes | Implemented (16 tests) |
| **Search** | MinHash Similarity Functions | -- | Yes | Implemented (48 tests) |
| | TopK / WAND Query Optimization | -- | Yes | Implemented (20 tests) |
| **Backup & Ops** | Hot Backup (RocksDB snapshots) | -- | Yes | Implemented (38 tests) |
| | Cloud Backup (S3 / Azure / GCS) | -- | Yes | Implemented (21 tests) |
| | Parallel Index Building | -- | Yes | Implemented (27 tests) |
| **Replication** | DC-to-DC Replication | -- | Yes | Implemented (204 tests) |
| **Infrastructure** | License Feature (always enabled) | N/A | Proprietary | Implemented (4 tests) |

**Totals: 19 modules, 565 tests, 43 test binaries, 0 failures**

---

## Project Structure

```
OpenArangoDBCore/
├── Enterprise/                # Symlink-compatible directory for ArangoDB
│   ├── Aql/                   # LocalTraversalNode, MinHash AQL functions
│   ├── Audit/                 # Structured audit logging (8 topics)
│   ├── Auth/                  # LDAP handler + external RBAC
│   ├── Basics/                # Shared utilities
│   ├── Cluster/               # SatelliteDistribution, ReadFromFollower
│   ├── Encryption/            # AES-256-CTR via RocksDB EncryptionProvider
│   ├── Graph/                 # SmartGraphProvider, SmartGraphStep
│   ├── IResearch/             # TopK/WAND optimization, MinHash analyzer
│   ├── License/               # Open license (always enabled)
│   ├── Maskings/              # Attribute-level data masking
│   ├── RClone/                # Cloud backup (S3/Azure/GCS)
│   ├── Replication/           # DC-to-DC replicator
│   ├── RestHandler/           # REST API endpoints
│   ├── RocksDBEngine/         # Hot backup, parallel index, encryption
│   ├── Sharding/              # SmartGraph sharding strategies
│   ├── Ssl/                   # Enhanced TLS (SNI, hot reload)
│   ├── StorageEngine/         # Storage engine integration
│   ├── Transaction/           # Transaction extensions
│   └── VocBase/               # SmartGraph schema, edge validators
├── tests/
│   ├── unit/                  # 42 unit test files (565 tests)
│   ├── link/                  # Link symbol verification
│   └── mocks/                 # Test mocks and helpers
├── cmake/                     # Build configuration
├── ci/                        # CI scripts (ASan, namespace audit)
├── CMakeLists.txt             # Top-level build
├── docs/                      # User guide, deployment docs
└── README.md
```

---

## How It Works

ArangoDB gates Enterprise features behind `#ifdef USE_ENTERPRISE`. The proprietary code lives in an `enterprise/` directory compiled when `-DUSE_ENTERPRISE=1` is set during the CMake build.

OpenArangoDBCore provides the same headers and symbols in the same directory layout. When you clone this repository as ArangoDB's `enterprise/` directory, ArangoDB compiles and links against our open-source implementations instead. No patches, no forks, no ArangoDB source modifications.

```
ArangoDB source:
  #ifdef USE_ENTERPRISE
    #include "Enterprise/Encryption/EncryptionFeature.h"   // <-- our file
  #endif
```

The `LicenseFeature` module always reports a valid license, so all 18 feature modules activate normally at startup.

---

## Quick Start

### Prerequisites

| Requirement | Version |
|-------------|---------|
| CMake | 3.20+ |
| C++ compiler | C++20 (GCC 12+, Clang 15+, MSVC 2022+) |
| OpenSSL | 3.0+ |
| libldap | OpenLDAP 2.5+ |
| RocksDB | Bundled with ArangoDB |
| GoogleTest | Bundled (for tests only) |

### Building with ArangoDB

```bash
# 1. Clone ArangoDB
git clone https://github.com/arangodb/arangodb.git
cd arangodb

# 2. Clone OpenArangoDBCore as the enterprise directory
git clone https://github.com/bolnet/OpenArangoDBCore.git enterprise

# 3. Build with Enterprise flag
mkdir build && cd build
cmake .. -DUSE_ENTERPRISE=1 -DCMAKE_BUILD_TYPE=RelWithDebInfo
make -j$(nproc)

# 4. Verify Enterprise features are active
./bin/arangosh --server.endpoint tcp://127.0.0.1:8529 \
  --javascript.execute-string "require('@arangodb').isEnterprise()"
# => true
```

### Building Tests Only (standalone)

```bash
cd enterprise   # or wherever OpenArangoDBCore is cloned
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
```

---

## Running Tests

```bash
cd build

# Run all 565 tests
ctest --output-on-failure

# Run tests for a specific module
ctest -R Encryption --output-on-failure
ctest -R SmartGraph --output-on-failure
ctest -R DC2DC --output-on-failure

# Run with AddressSanitizer (CI mode)
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON
make -j$(nproc)
ctest --output-on-failure
```

**Test summary by phase:**

| Phase | Modules | Tests |
|-------|---------|-------|
| Phase 1: Foundation | License, CMake integration, link symbols | ~5 |
| Phase 2: Security | Encryption, Audit, LDAP/RBAC, Masking, SSL/TLS | ~108 |
| Phase 3: Graph & Cluster | SmartGraphs, Satellites, Shard-local, ReadFromFollower | ~94 |
| Phase 4: Search & Backup | MinHash, TopK, Hot Backup, Cloud Backup, Parallel Index | ~154 |
| Phase 5: DC-to-DC Replication | Sequence numbers, WAL tailing, DirectMQ, REST API | ~204 |

---

## Architecture

OpenArangoDBCore was built in 5 phases, each delivering a coherent, independently testable slice of Enterprise functionality.

### Phase 1: Foundation and ABI Baseline
Namespace correctness, CMake integration, `LicenseFeature` (always returns valid), CI with AddressSanitizer, and link-symbol verification. Every subsequent phase depends on this.

### Phase 2: Security Foundations
Six security modules: Encryption at Rest (AES-256-CTR via RocksDB `EncryptionProvider` and OpenSSL), Audit Logging (8 topics, file/syslog output), LDAP Authentication (simple + search modes, TLS, role mapping), External RBAC, Data Masking (8 masking types for `arangodump`), and Enhanced SSL/TLS (SNI, hot reload, cipher control).

### Phase 3: Graph and Cluster
Six graph/cluster modules: SmartGraphs (partition-key sharding with schema validation), Disjoint SmartGraphs (cross-partition edge prohibition for full query push-down), Satellite Collections (replicate to all DB-Servers), SmartToSat edge validation, Shard-local graph execution (`LocalTraversalNode` AQL optimization), and ReadFromFollower (dirty-read routing to replicas).

### Phase 4: Search and Backup Operations
Five modules: MinHash similarity functions (`MINHASH()`, `MINHASH_MATCH()`, `JACCARD()` AQL functions plus `minhash` analyzer), TopK/WAND query optimization (early termination for ranked search), Hot Backup (RocksDB checkpoint-based snapshots with REST API), Cloud Backup (rclone integration for S3/Azure/GCS), and Parallel Index Building (multi-threaded RocksDB index construction with background build support).

### Phase 5: DC-to-DC Replication
Asynchronous cross-datacenter replication: sequence-numbered idempotent message delivery, WAL tailing, DirectMQ transport with mTLS, REST API for arangosync integration, checkpoint-based consistency, and lag monitoring.

---

## Stats

| Metric | Value |
|--------|-------|
| Enterprise modules | 19 |
| Total tests | 565 |
| Test binaries | 43 |
| Test failures | 0 |
| Production SLOC | ~10,000 |
| Test SLOC | ~10,400 |
| Implementation phases | 5 |
| C++ standard | C++20 |

---

## License

Apache License 2.0. See [LICENSE](LICENSE) for details.

---

## Related Projects

- [ArangoDB](https://github.com/arangodb/arangodb) -- The core multi-model database
- [OpenArangoDB](https://github.com/bolnet/OpenArangoDB) -- Python agent memory layer for ArangoDB Community
