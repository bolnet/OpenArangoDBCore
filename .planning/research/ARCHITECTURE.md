# Architecture Research

**Domain:** C++ Enterprise database plugin — ArangoDB Enterprise drop-in replacement
**Researched:** 2026-03-29
**Confidence:** MEDIUM (ArangoDB source not publicly browsable; integration patterns from official docs + limited raw GitHub access)

---

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ArangoDB Binary                              │
│  (compiled with -DUSE_ENTERPRISE=1)                                 │
├─────────────────────────────────────────────────────────────────────┤
│  ApplicationServer                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │ AuditFeature │  │EncryptionFtr │  │ LicenseFeature│  (Features) │
│  └──────────────┘  └──────────────┘  └──────────────┘              │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐              │
│  │ SslFtrEE     │  │ RCloneFtr    │  │  AuthInfo/   │              │
│  └──────────────┘  └──────────────┘  │  LDAPHandler │              │
│                                      └──────────────┘              │
├─────────────────────────────────────────────────────────────────────┤
│  AQL Engine                                                         │
│  ┌─────────────────────┐  ┌──────────────────────────┐             │
│  │ AqlFunctionFeature  │  │ LocalTraversalNode       │             │
│  │  MinHash functions  │  │ (Shard-local traversal)  │             │
│  └─────────────────────┘  └──────────────────────────┘             │
├─────────────────────────────────────────────────────────────────────┤
│  Storage Engine (RocksDB)                                           │
│  ┌──────────────────┐  ┌────────────────────┐  ┌─────────────────┐ │
│  │EncryptionProvider│  │ RocksDBHotBackup   │  │ RocksDBBuilder  │ │
│  │ AES-256-CTR      │  │ (Checkpoint API)   │  │ IndexEE         │ │
│  └──────────────────┘  └────────────────────┘  └─────────────────┘ │
├─────────────────────────────────────────────────────────────────────┤
│  Cluster / VocBase                                                  │
│  ┌───────────────────┐  ┌──────────────────┐  ┌──────────────────┐ │
│  │ShardingStrategyEE │  │SatelliteDistrib. │  │ReadFromFollower  │ │
│  │SmartGraphSchema   │  │(replicationFactor│  │(follower reads)  │ │
│  └───────────────────┘  │ = "satellite")   │  └──────────────────┘ │
│  ┌───────────────────┐  └──────────────────┘                       │
│  │ SmartGraphProvider│                                              │
│  │ SmartGraphStep    │                                              │
│  └───────────────────┘                                             │
├─────────────────────────────────────────────────────────────────────┤
│  IResearch (ArangoSearch)                                           │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  IResearchOptimizeTopK (WAND optimization)                  │   │
│  └─────────────────────────────────────────────────────────────┘   │
├─────────────────────────────────────────────────────────────────────┤
│  Replication                                                        │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │  DC2DCReplicator (ArangoSync/DirectMQ integration)          │   │
│  └─────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
              ↑
  openarangodb_enterprise (static lib — this project)
```

### Component Responsibilities

| Component | Responsibility | Integration Layer |
|-----------|----------------|-------------------|
| AuditFeature | Structured audit logging to file/syslog for 8 topics | ApplicationServer Feature |
| EncryptionFeature | Key management, startup/lifecycle for encryption | ApplicationServer Feature |
| EncryptionProvider | AES-256-CTR RocksDB environment wrapper | RocksDB EncryptionProvider |
| RocksDBEncryptionUtils | Key derivation, IV management helpers | RocksDB internal |
| LicenseFeature | Always-enabled license gate (open source stub) | ApplicationServer Feature |
| SslServerFeatureEE | Enhanced TLS cipher suites, cert validation | ApplicationServer Feature |
| LDAPHandler | Bind/search/group-mapping against LDAP server | Auth::AuthInfo hook |
| AttributeMasking | Field-level masking during arangodump | arangodump masking config |
| ShardingStrategyEE | Smart/disjoint shard distribution by smartGraphAttribute | VocBase/Cluster sharding |
| SmartGraphSchema | Validates SmartGraph collection properties | VocBase schema validation |
| SmartGraphProvider | Graph traversal provider routing to local shard | Graph traversal engine |
| SmartGraphStep | Per-step routing for SmartGraph traversals | AQL execution engine |
| LocalTraversalNode | AQL node for shard-local graph execution | AQL optimizer/execution |
| SatelliteDistribution | replicationFactor="satellite" distribution logic | Cluster distribution layer |
| ReadFromFollower | Route single-shard reads to follower DBServer | Cluster coordinator routing |
| MinHashFunctions | MINHASH, MINHASH_MATCH, etc. AQL functions | AqlFunctionFeature registry |
| IResearchOptimizeTopK | WAND/TopK optimization for ArangoSearch views | IResearch query optimizer |
| RocksDBHotBackup | RocksDB Checkpoint API, WAL flush, hard-link dir | RocksDBEngine backup hooks |
| RocksDBBuilderIndexEE | Parallel index construction (multi-threaded) | RocksDB index builder |
| DC2DCReplicator | ArangoSync replication coordinator/interface | Replication subsystem |
| RCloneFeature | Cloud backup upload/download via embedded rclone | ApplicationServer Feature |

---

## Recommended Project Structure

```
OpenArangoDBCore/
├── arangod/
│   ├── Audit/
│   │   ├── AuditFeature.h      # Extends ApplicationFeature
│   │   └── AuditFeature.cpp    # collectOptions, prepare, start, stop
│   ├── Encryption/
│   │   ├── EncryptionFeature.h   # Extends ApplicationFeature
│   │   ├── EncryptionFeature.cpp
│   │   ├── EncryptionProvider.h  # Extends rocksdb::EncryptionProvider
│   │   └── EncryptionProvider.cpp
│   ├── RocksDBEngine/
│   │   ├── RocksDBHotBackup.h    # Checkpoint API wrapper
│   │   ├── RocksDBHotBackup.cpp
│   │   ├── RocksDBBuilderIndexEE.h   # Parallel index builder
│   │   ├── RocksDBBuilderIndexEE.cpp
│   │   ├── RocksDBEncryptionUtils.h
│   │   └── RocksDBEncryptionUtils.cpp
│   ├── Sharding/
│   │   ├── ShardingStrategyEE.h    # Extends ShardingStrategy
│   │   └── ShardingStrategyEE.cpp
│   ├── Graph/
│   │   ├── Providers/
│   │   │   ├── SmartGraphProvider.h
│   │   │   └── SmartGraphProvider.cpp
│   │   └── Steps/
│   │       ├── SmartGraphStep.h
│   │       └── SmartGraphStep.cpp
│   ├── Aql/
│   │   ├── LocalTraversalNode.h    # Extends AQL ExecutionNode
│   │   ├── LocalTraversalNode.cpp
│   │   ├── MinHashFunctions.h
│   │   └── MinHashFunctions.cpp
│   ├── Auth/
│   │   ├── LDAPHandler.h
│   │   └── LDAPHandler.cpp
│   ├── Maskings/
│   │   ├── AttributeMasking.h
│   │   └── AttributeMasking.cpp
│   ├── IResearch/
│   │   ├── IResearchOptimizeTopK.h
│   │   └── IResearchOptimizeTopK.cpp
│   ├── Cluster/
│   │   ├── SatelliteDistribution.h
│   │   ├── SatelliteDistribution.cpp
│   │   ├── ReadFromFollower.h
│   │   └── ReadFromFollower.cpp
│   ├── Replication/
│   │   ├── DC2DCReplicator.h
│   │   └── DC2DCReplicator.cpp
│   ├── RClone/
│   │   ├── RCloneFeature.h
│   │   └── RCloneFeature.cpp
│   ├── Ssl/
│   │   ├── SslServerFeatureEE.h
│   │   └── SslServerFeatureEE.cpp
│   ├── License/
│   │   ├── LicenseFeature.h
│   │   └── LicenseFeature.cpp
│   └── VocBase/
│       ├── SmartGraphSchema.h
│       └── SmartGraphSchema.cpp
├── cmake/
├── tests/
│   ├── unit/
│   └── integration/
└── CMakeLists.txt
```

### Structure Rationale

- **Mirrors ArangoDB's arangod/ layout exactly:** Headers and symbols must match the paths ArangoDB's `#include` directives expect. Any deviation breaks the drop-in.
- **One directory per subsystem:** Matches ArangoDB's own module boundaries. Crossing subsystems goes through well-defined interfaces.
- **No enterprise/ sub-prefix inside the repo:** ArangoDB clones this repo AS the enterprise/ directory. Internal paths should not re-add enterprise/ indirection.

---

## Architectural Patterns

### Pattern 1: ApplicationFeature Lifecycle

**What:** Every server-level enterprise feature extends `ApplicationFeature` (from `lib/ApplicationFeatures/`). The server calls lifecycle methods in order at startup and shutdown.

**When to use:** AuditFeature, EncryptionFeature, LicenseFeature, SslServerFeatureEE, RCloneFeature all follow this pattern.

**Trade-offs:** Clean separation of concerns, but requires careful ordering via `startsAfter()`. Features that need RocksDB must start after the RocksDB engine feature.

**Lifecycle methods to implement:**
```cpp
class AuditFeature : public arangodb::application_features::ApplicationFeature {
 public:
  explicit AuditFeature(Server& server);

  // 1. Declare --audit.* options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;

  // 2. Validate option values
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

  // 3. Open log file, connect syslog — no threads yet
  void prepare() override;

  // 4. Start background flush thread if needed
  void start() override;

  // 5. Signal shutdown
  void beginShutdown() override;

  // 6. Flush and close output
  void stop() override;

  // 7. Release resources
  void unprepare() override;
};
```

### Pattern 2: RocksDB Engine Hook

**What:** Enterprise storage features inject into RocksDB via three hook functions called from `RocksDBEngine.cpp` inside `#ifdef USE_ENTERPRISE` blocks.

**When to use:** EncryptionProvider, hot backup, parallel index building.

**Trade-offs:** Tight coupling to RocksDB internals; ABI stability is guaranteed by ArangoDB bundling RocksDB.

**Hook functions that must be implemented:**
```cpp
// Called from RocksDBEngine::collectOptions()
void collectEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);

// Called from RocksDBEngine::validateOptions()
void validateEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);

// Called from RocksDBEngine::start() before DB::Open()
void configureEnterpriseRocksDBOptions(rocksdb::Options& opts,
                                        rocksdb::TransactionDBOptions& txOpts);
```

**Encryption provider registration (inside configureEnterpriseRocksDBOptions):**
```cpp
// Set a custom rocksdb::Env that wraps all SST file I/O with AES-256-CTR
auto encryptionEnv = std::make_shared<EncryptionProvider>(key);
opts.env = encryptionEnv.get();
opts.create_sha_files = true;  // for integrity verification
```

### Pattern 3: AQL Function Registration

**What:** Enterprise AQL functions are registered by calling `AqlFunctionFeature::add()` inside a `#ifdef USE_ENTERPRISE` block during server startup.

**When to use:** MinHashFunctions, SELECT_SMART_DISTRIBUTE_GRAPH_INPUT.

**Trade-offs:** Simple registration; function implementations must be stateless (deterministic, cacheable, or explicitly non-deterministic).

**Registration pattern:**
```cpp
// Inside AqlFunctionFeature::addEnterpriseFunctions() or equivalent
#ifdef USE_ENTERPRISE
  add({"MINHASH", ".,.", makeFlags(FF::Deterministic, FF::Cacheable,
                                    FF::CanRunOnDBServerCluster),
       &functions::MinHash});
  add({"MINHASH_MATCH", ".,.,.", makeFlags(FF::Deterministic, FF::Cacheable,
                                             FF::CanRunOnDBServerCluster),
       &functions::MinHashMatch});
#endif
```

**Note (MEDIUM confidence):** Actual MinHash functions may be registered unconditionally in some ArangoDB versions (observed in AqlFunctionFeature.cpp analysis). The `#ifdef USE_ENTERPRISE` guard is confirmed only for `SELECT_SMART_DISTRIBUTE_GRAPH_INPUT`.

### Pattern 4: Sharding Strategy Plugin

**What:** `ShardingStrategyEE` extends the abstract `ShardingStrategy` base class and is registered in `ShardingFeature` when `USE_ENTERPRISE=1`.

**When to use:** SmartGraph and Disjoint SmartGraph sharding.

**Core responsibility:** Given a document's `_key` or `smartGraphAttribute`, compute which shard it belongs to. Smart partitioning ensures that all vertices with the same smartGraphAttribute land on the same shard, and all connecting edges are co-located with those vertices.

**Shard key derivation:**
```
smartGraphAttribute value → hash → shard index
_key format: "<smartValue>:<localKey>"
Edge _from/_to: must share smartGraphAttribute prefix
```

### Pattern 5: Cluster Distribution Override

**What:** `SatelliteDistribution` sets `replicationFactor = "satellite"` which causes the cluster to synchronously replicate the collection to ALL DB-Servers. This is validated at collection creation time in VocBase.

**Integration point:** Collection properties validation in `VocBase::createCollection()` and cluster plan propagation in `ClusterInfo`.

---

## Integration Points Per Module Category

### Security Modules

| Module | Integration Point | What to Implement |
|--------|------------------|-------------------|
| AuditFeature | `ApplicationServer::addFeature<AuditFeature>()` inside `#ifdef USE_ENTERPRISE` in ArangoServer startup | Feature lifecycle + topic-based log emission hooks |
| EncryptionFeature | ApplicationServer Feature + `configureEnterpriseRocksDBOptions()` hook | Key loading, RocksDB env injection |
| EncryptionProvider | `rocksdb::Env` subclass, AES-256-CTR via OpenSSL 3.0+ EVP | Wrap SST file read/write with CTR stream |
| RocksDBEncryptionUtils | Internal helpers for key derivation and IV management | Called by EncryptionProvider |
| LDAPHandler | `Auth::AuthInfo::checkAuthentication()` calls `LDAPHandler::authenticate()` inside `#ifdef USE_ENTERPRISE` | OpenLDAP `ldap_bind`, `ldap_search_ext` |
| AttributeMasking | arangodump reads masking config, calls masking functions per attribute | JSON config parsing + masking type implementations |
| SslServerFeatureEE | Extends community `SslServerFeature`, overrides cipher list and cert validation | OpenSSL context configuration |

### Graph and Cluster Modules

| Module | Integration Point | What to Implement |
|--------|------------------|-------------------|
| ShardingStrategyEE | Registered in `ShardingFeature` when `USE_ENTERPRISE=1` | Consistent hash of smartGraphAttribute value |
| SmartGraphSchema | Called from `VocBase::createCollection()` to validate SmartGraph properties | Schema validation: smartGraphAttribute present, _key format |
| SmartGraphProvider | Plugged into graph traversal engine as provider for smart graphs | Remote/local step routing by shard |
| SmartGraphStep | Used by graph traversal execution to execute one traversal step | Classify step as local or remote, execute accordingly |
| LocalTraversalNode | AQL `ExecutionNode` subclass registered with `#ifdef USE_ENTERPRISE` | Serialize/deserialize plan node, execute locally |
| SatelliteDistribution | `ClusterInfo::createCollectionInCoordinator()` checks distribution type | Return `replicationFactor = "satellite"` plan |
| ReadFromFollower | Coordinator routes single-document reads to follower DB-Server | Read routing policy; respect eventual consistency contract |

### Search Modules

| Module | Integration Point | What to Implement |
|--------|------------------|-------------------|
| MinHashFunctions | `AqlFunctionFeature::add()` during startup | LSH minhash signature computation; similarity threshold matching |
| IResearchOptimizeTopK | IResearch query optimizer hook; WAND algorithm | Top-K heap with early termination; integrate with IResearch sort optimization |

### Storage and Backup Modules

| Module | Integration Point | What to Implement |
|--------|------------------|-------------------|
| RocksDBHotBackup | REST endpoint `/_admin/backup/create` → RocksDB Checkpoint API | `rocksdb::Checkpoint::Create()`, WAL flush, hard-link directory |
| RocksDBBuilderIndexEE | `RocksDBIndex::fillIndex()` parallelism hook | Thread pool for parallel SST ingestion during index build |
| DC2DCReplicator | Replication subsystem; wraps ArangoSync protocol | Async replication state machine; DirectMQ message handling |
| RCloneFeature | ApplicationServer Feature; uploads hot backup directory to cloud | Shell out to bundled rclone binary with provider config |

---

## Data Flow

### Feature Startup Flow

```
ArangoServer::buildApplicationServer()
    |
    +-- #ifdef USE_ENTERPRISE
    |   addFeature<LicenseFeature>()
    |   addFeature<AuditFeature>()
    |   addFeature<EncryptionFeature>()
    |   addFeature<SslServerFeatureEE>()
    |   addFeature<RCloneFeature>()
    +-- #endif
    |
ApplicationServer::run()
    |
    +--> collectOptions  (all features declare CLI options)
    +--> validateOptions (all features validate)
    +--> prepare         (open files, connect external services)
    +--> start           (start threads, RocksDB opens with EE env)
    +--> wait            (server running)
    +--> stop            (reverse order)
    +--> unprepare
```

### RocksDB Encryption Data Flow

```
ArangoDB write path
    |
    v
rocksdb::WriteBatch
    |
    v
RocksDB WAL (plaintext in memory)
    |
    v
SST file flush
    |
    v
EncryptionProvider::NewWritableFile()
    |
    v
AES-256-CTR stream encrypt (OpenSSL EVP_EncryptUpdate)
    |
    v
Encrypted SST file on disk
```

### SmartGraph Query Flow

```
AQL query: FOR v, e IN 1..3 OUTBOUND @start GRAPH 'smart'
    |
    v
AQL optimizer: LocalTraversalNode injection (#ifdef USE_ENTERPRISE)
    |
    v
SmartGraphProvider determines shard for start vertex
    |
    +--> vertex shard is LOCAL to this DB-Server?
    |       YES → execute SmartGraphStep locally (no network hop)
    |       NO  → route to remote DB-Server via cluster communication
    |
    v
SmartGraphStep: traverse edges co-located with vertex shard
    (40-120x speedup from reduced cross-shard hops)
```

### Hot Backup Flow

```
REST POST /_admin/backup/create
    |
    v
RocksDBHotBackup::createBackup()
    |
    +--> Stop all writes to instance
    +--> Flush WAL (flushColumnFamilies=true)
    +--> rocksdb::Checkpoint::Create(backup_dir)
    |    (creates hard links to all SST files)
    +--> Resume writes
    |
    v
Backup directory with hard-linked SST files

Optional: RCloneFeature uploads backup_dir to S3/Azure/GCS
```

### LDAP Authentication Flow

```
HTTP request with credentials
    |
    v
Auth::AuthInfo::checkAuthentication()
    |
    #ifdef USE_ENTERPRISE
    v
LDAPHandler::authenticate(username, password)
    |
    +--> ldap_bind(username, password)  — simple bind or SASL
    +--> ldap_search_ext(base_dn, filter, username)
    +--> extract group/role memberships
    +--> map LDAP roles to ArangoDB database permissions
    |
    #endif
    v
Return authentication result + permissions
```

---

## Build Order (Dependency Graph)

Build these module groups in order — later groups depend on earlier ones:

```
Layer 0 — No dependencies (pure C++ utilities):
  LicenseFeature     (empty gate, always-on)
  RocksDBEncryptionUtils  (OpenSSL AES utilities only)
  MinHashFunctions   (pure math, no ArangoDB dependencies)
  AttributeMasking   (VelocyPack + config parsing only)

Layer 1 — Depend only on ArangoDB base types:
  EncryptionProvider      (depends on RocksDBEncryptionUtils + RocksDB)
  EncryptionFeature       (depends on EncryptionProvider + ApplicationFeature)
  LDAPHandler             (depends on OpenLDAP + Auth base types)
  SslServerFeatureEE      (depends on OpenSSL + community SslServerFeature)
  ShardingStrategyEE      (depends on ShardingStrategy base)
  SmartGraphSchema        (depends on VocBase types)
  AuditFeature            (depends on ApplicationFeature + Logger)
  RCloneFeature           (depends on ApplicationFeature)

Layer 2 — Depend on Layer 1 + Cluster:
  SatelliteDistribution   (depends on ShardingStrategyEE + ClusterInfo)
  ReadFromFollower        (depends on Cluster + Replication base)
  SmartGraphProvider      (depends on SmartGraphSchema + Graph base)
  SmartGraphStep          (depends on SmartGraphProvider)
  LocalTraversalNode      (depends on SmartGraphStep + AQL ExecutionNode)
  IResearchOptimizeTopK   (depends on IResearch + AQL optimizer)

Layer 3 — Depend on RocksDB engine being initialized:
  RocksDBHotBackup        (depends on RocksDB Checkpoint API)
  RocksDBBuilderIndexEE   (depends on RocksDB IndexBuilder)

Layer 4 — Depend on full cluster being operational:
  DC2DCReplicator         (depends on Replication, Auth, Cluster)
```

**Recommended implementation order (week-by-week prioritization):**

1. `LicenseFeature` — trivial, unlocks all USE_ENTERPRISE ifdef paths
2. `AuditFeature` — file/syslog output, 8 topics; high compliance value
3. `EncryptionProvider` + `EncryptionFeature` + `RocksDBEncryptionUtils` — core security
4. `LDAPHandler` — auth integration is critical for enterprise
5. `ShardingStrategyEE` + `SmartGraphSchema` — foundation for all graph features
6. `SmartGraphProvider` + `SmartGraphStep` + `LocalTraversalNode` — graph traversal
7. `SatelliteDistribution` + `ReadFromFollower` — cluster operations
8. `MinHashFunctions` + `IResearchOptimizeTopK` — search features
9. `RocksDBHotBackup` + `RocksDBBuilderIndexEE` — backup and ops
10. `AttributeMasking` + `SslServerFeatureEE` — security hardening
11. `RCloneFeature` — cloud backup upload
12. `DC2DCReplicator` — most complex, external ArangoSync dependency

---

## Anti-Patterns

### Anti-Pattern 1: Wrong Namespace

**What people do:** Define classes in `namespace openarangodb` (as the current stubs do).

**Why it's wrong:** ArangoDB's community code uses `namespace arangodb`. Enterprise code that must be linked as drop-in replacements must live in `namespace arangodb` (or the specific sub-namespace used by each ArangoDB module — e.g., `arangodb::audit`, `arangodb::encryption`).

**Do this instead:** Match the exact namespace ArangoDB expects. Check ArangoDB's community headers for each module to find the correct namespace. The current stub files with `namespace openarangodb` must be changed.

### Anti-Pattern 2: Implementing in Isolation Without Header Contracts

**What people do:** Write implementations before knowing the exact virtual method signatures ArangoDB calls.

**Why it's wrong:** ABI compatibility is the entire point. A method with a wrong signature will silently fail to override the base class virtual, resulting in undefined behavior or link errors.

**Do this instead:** For each module, locate ArangoDB's community header that declares the base class or hook function signature. Implement the exact signature. Where ArangoDB headers are not available, use the official documentation + source browsing to confirm signatures before committing.

### Anti-Pattern 3: Monolithic Implementation File

**What people do:** Put all 19 features into a single .cpp or large files.

**Why it's wrong:** ArangoDB has module-level compilation units. Large files slow incremental compilation and make it impossible to disable individual features at link time.

**Do this instead:** Keep one .h/.cpp pair per class, matching the file layout above. The current CMakeLists.txt already does this correctly — maintain it.

### Anti-Pattern 4: Assuming USE_ENTERPRISE Guards Are Sufficient

**What people do:** Add `#ifdef USE_ENTERPRISE` around stubs and assume ArangoDB will call them.

**Why it's wrong:** Many enterprise hooks are called through virtual dispatch, registration functions, or factory registries — not raw ifdef calls. A stub that compiles but doesn't register itself with the correct factory will never be called.

**Do this instead:** For each module, trace the exact registration path: ApplicationServer::addFeature(), ShardingStrategyFeature::registerStrategy(), AqlFunctionFeature::add(), etc.

### Anti-Pattern 5: Skipping LicenseFeature

**What people do:** Leave LicenseFeature as an empty stub.

**Why it's wrong:** ArangoDB's community code checks `LicenseFeature::isEnterprise()` (or equivalent) at runtime before enabling enterprise code paths. If this returns false, even correctly implemented features won't be activated.

**Do this instead:** Implement LicenseFeature to return `true` for all enterprise capability checks. This is the "Open License" goal of the project.

---

## Scaling Considerations

This is a C++ library embedded in ArangoDB — scaling is ArangoDB's concern, not ours. However, implementation choices affect ArangoDB's scalability:

| Scale | Architecture Adjustments |
|-------|--------------------------|
| Single node | All modules work; DC2DC and cluster modules degrade gracefully (no-ops) |
| Small cluster (3-9 nodes) | SmartGraph + SatelliteGraph most impactful; ReadFromFollower adds read throughput |
| Large cluster (10+ nodes) | Parallel index building critical; encryption overhead should use AES-NI hardware acceleration |
| Multi-datacenter | DC2DCReplicator becomes critical path; must handle network partitions gracefully |

### Scaling Priorities

1. **First bottleneck:** Encryption at rest — AES-256-CTR must use OpenSSL hardware acceleration (AES-NI). Software-only encryption is 3-5x slower. Use `EVP_CIPHER_CTX` with `EVP_aes_256_ctr()`, not a hand-rolled cipher.
2. **Second bottleneck:** SmartGraph traversals — performance depends on correctness of shard co-location. Wrong shard key derivation leads to cross-shard hops, eliminating the 40-120x gain.

---

## Key ArangoDB Internal APIs to Map

These are the specific ArangoDB base classes and interfaces each module must extend or implement:

| Our Module | ArangoDB Base/Interface | Header Path (in ArangoDB) |
|------------|------------------------|--------------------------|
| AuditFeature | `application_features::ApplicationFeature` | `lib/ApplicationFeatures/ApplicationFeature.h` |
| EncryptionFeature | `application_features::ApplicationFeature` | same |
| EncryptionProvider | `rocksdb::EncryptionProvider` or `rocksdb::Env` | RocksDB headers (bundled) |
| LicenseFeature | `application_features::ApplicationFeature` | same |
| SslServerFeatureEE | community `SslServerFeature` | `arangod/Ssl/SslServerFeature.h` |
| LDAPHandler | called from `Auth::AuthInfo` | `arangod/Auth/AuthInfo.h` |
| ShardingStrategyEE | `ShardingStrategy` | `arangod/Sharding/ShardingStrategy.h` |
| SmartGraphProvider | graph traversal provider base | `arangod/Graph/Providers/` |
| SmartGraphStep | graph traversal step base | `arangod/Graph/Steps/` |
| LocalTraversalNode | `aql::ExecutionNode` | `arangod/Aql/ExecutionNode.h` |
| IResearchOptimizeTopK | IResearch optimizer rule | `arangod/IResearch/` |
| RocksDBHotBackup | free functions called from RocksDB engine hooks | `arangod/RocksDBEngine/RocksDBEngine.h` |
| RocksDBBuilderIndexEE | `RocksDBIndexBuilderWorker` or equivalent | `arangod/RocksDBEngine/` |
| DC2DCReplicator | Replication base | `arangod/Replication/` |
| RCloneFeature | `application_features::ApplicationFeature` | `lib/ApplicationFeatures/ApplicationFeature.h` |
| AttributeMasking | Called from arangodump masking config | `client-tools/Maskings/` |
| SatelliteDistribution | ClusterInfo collection distribution | `arangod/Cluster/ClusterInfo.h` |
| ReadFromFollower | Coordinator shard routing | `arangod/Cluster/` |

**IMPORTANT:** The exact base class names and header paths listed above are MEDIUM confidence — derived from ArangoDB docs and partial source access. Before implementing each module, verify the exact signatures by browsing the ArangoDB source at `github.com/arangodb/arangodb/tree/devel`.

---

## Sources

- [ArangoDB GitHub Repository](https://github.com/arangodb/arangodb) — Source structure reference (MEDIUM confidence — browsable but not all files accessible as raw)
- [ArangoDB CMakeLists.txt (devel)](https://raw.githubusercontent.com/arangodb/arangodb/devel/CMakeLists.txt) — Confirmed `add_subdirectory(enterprise)` pattern under `USE_ENTERPRISE` (HIGH confidence)
- [ApplicationFeatures/ApplicationServer.h](https://raw.githubusercontent.com/arangodb/arangodb/devel/lib/ApplicationFeatures/ApplicationServer.h) — Feature lifecycle: collectOptions, prepare, start, stop, unprepare (HIGH confidence)
- [AqlFunctionFeature.cpp](https://raw.githubusercontent.com/arangodb/arangodb/devel/arangod/Aql/AqlFunctionFeature.cpp) — Enterprise function registration pattern, USE_ENTERPRISE guard for `SELECT_SMART_DISTRIBUTE_GRAPH_INPUT` (HIGH confidence)
- [RocksDBEngine.cpp](https://raw.githubusercontent.com/arangodb/arangodb/devel/arangod/RocksDBEngine/RocksDBEngine.cpp) — `collectEnterpriseOptions`, `validateEnterpriseOptions`, `configureEnterpriseRocksDBOptions` hooks (HIGH confidence)
- [Encryption at Rest Documentation](https://docs.arangodb.com/3.10/operations/security/encryption-at-rest/) — AES-256-CTR, RocksDB EncryptionProvider interface (MEDIUM confidence)
- [Audit Logging Documentation](https://docs.arangodb.com/3.10/operations/security/audit-logging/) — 8 audit topics, output formats (HIGH confidence)
- [SmartGraphs Documentation](https://docs.arangodb.com/3.12/graphs/smartgraphs/) — smartGraphAttribute, shard co-location (HIGH confidence)
- [SatelliteCollections Documentation](https://docs.arangodb.com/3.12/develop/satellitecollections/) — replicationFactor="satellite" (HIGH confidence)
- [IResearch TopK/WAND Optimization](https://docs.arangodb.com/3.11/index-and-search/arangosearch/performance/) — WAND optimization, enterprise-only until 3.12.5 (MEDIUM confidence)
- [Hot Backup Documentation](https://www.arangodb.com/docs/stable/http/hot-backup.html) — RocksDB Checkpoint API, WAL flush pattern (HIGH confidence)
- [DC2DC Replication Documentation](https://www.arangodb.com/docs/stable/architecture-deployment-modes-dc2-dc.html) — ArangoSync, DirectMQ, async replication (HIGH confidence)
- [ArangoDB rclone fork](https://github.com/arangodb/rclone-update) — Cloud backup integration (MEDIUM confidence)
- [LDAP Configuration](https://www.arangodb.com/docs/stable/programs-arangod-ldap.html) — Bind, search filter, roles mapping (HIGH confidence)

---

*Architecture research for: OpenArangoDBCore — ArangoDB Enterprise C++ drop-in replacement*
*Researched: 2026-03-29*
