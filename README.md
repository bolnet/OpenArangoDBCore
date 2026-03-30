# OpenArangoDBCore

Open-source C++ implementation of ArangoDB Enterprise features. Drop-in replacement for the proprietary `Enterprise/` module.

Build ArangoDB with `-DUSE_ENTERPRISE=1` pointing to this code instead of the closed-source Enterprise directory.

## Features (19 Enterprise-equivalent modules)

### Security
| Module | Directory | Status |
|--------|-----------|--------|
| Audit Logging (8 topics) | `arangod/Audit/` | Planned |
| Encryption at Rest (AES-256-CTR) | `arangod/Encryption/` | Planned |
| LDAP Authentication | `arangod/Auth/` | Planned |
| External RBAC | `arangod/Auth/` | Planned |
| Data Masking / Anonymization | `arangod/Maskings/` | Planned |
| Enhanced SSL/TLS | `arangod/Ssl/` | Planned |

### Graph & Cluster
| Module | Directory | Status |
|--------|-----------|--------|
| SmartGraphs (partition-key sharding) | `arangod/Sharding/` | Planned |
| Disjoint SmartGraphs | `arangod/Sharding/` | Planned |
| SatelliteGraphs / Satellite Collections | `arangod/Cluster/` | Planned |
| SmartToSat Edge Validation | `arangod/VocBase/` | Planned |
| Shard-local Graph Execution | `arangod/Aql/` | Planned |
| ReadFromFollower | `arangod/Cluster/` | Planned |

### Search
| Module | Directory | Status |
|--------|-----------|--------|
| MinHash Similarity Functions | `arangod/Aql/` | Planned |
| TopK Query Optimization | `arangod/IResearch/` | Planned |

### Replication
| Module | Directory | Status |
|--------|-----------|--------|
| DC-to-DC Replication | `arangod/Replication/` | Planned |

### Backup & Ops
| Module | Directory | Status |
|--------|-----------|--------|
| Hot Backup (RocksDB snapshots) | `arangod/RocksDBEngine/` | Planned |
| Cloud Backup (RClone: S3/Azure/GCS) | `arangod/RClone/` | Planned |
| Parallel Index Building | `arangod/RocksDBEngine/` | Planned |

## Project Structure

```
OpenArangoDBCore/
├── arangod/
│   ├── Audit/              # Structured audit logging (8 topics)
│   ├── Encryption/         # AES-256-CTR via RocksDB EncryptionProvider
│   ├── Sharding/           # SmartGraph + disjoint sharding strategies
│   ├── Graph/
│   │   ├── Providers/      # SmartGraphProvider + RPC communicator
│   │   └── Steps/          # SmartGraphStep for traversals
│   ├── Aql/                # LocalTraversalNode, MinHash functions
│   ├── Auth/               # LDAP handler + external RBAC
│   ├── Maskings/           # Attribute-level data masking
│   ├── IResearch/          # TopK optimization, enhanced analyzers
│   ├── RocksDBEngine/      # Hot backup, parallel index, encryption provider
│   ├── Cluster/            # SatelliteDistribution, ReadFromFollower
│   ├── Replication/        # DC-to-DC replicator
│   ├── RClone/             # Cloud backup (S3/Azure/GCS)
│   ├── Ssl/                # Enhanced TLS
│   ├── License/            # Open license feature (always enabled)
│   └── VocBase/            # SmartGraph schema, edge validators
├── cmake/                  # Build configuration
├── tests/                  # Unit and integration tests
├── CMakeLists.txt          # Top-level build
└── README.md
```

## How It Works

ArangoDB's Enterprise features are gated behind `#ifdef USE_ENTERPRISE`. The proprietary code lives in an `enterprise/` directory that gets compiled when `-DUSE_ENTERPRISE=1` is set.

OpenArangoDBCore provides the same headers and symbols, so ArangoDB compiles and links against our open-source implementations instead.

## Building

```bash
# Clone ArangoDB
git clone https://github.com/arangodb/arangodb.git
cd arangodb

# Clone OpenArangoDBCore as the enterprise directory
git clone https://github.com/bolnet/OpenArangoDBCore.git enterprise

# Build with Enterprise flag
mkdir build && cd build
cmake .. -DUSE_ENTERPRISE=1
make -j$(nproc)
```

## Requirements

- ArangoDB source (latest stable)
- CMake 3.20+
- C++20 compiler (GCC 12+, Clang 15+, MSVC 2022+)
- RocksDB (bundled with ArangoDB)
- OpenSSL 3.0+ (for encryption)
- ldap library (for LDAP auth)

## License

Apache 2.0

## Related

- [OpenArangoDB](https://github.com/bolnet/OpenArangoDB) — Python agent memory layer for ArangoDB Community
- [ArangoDB](https://github.com/arangodb/arangodb) — The core database
