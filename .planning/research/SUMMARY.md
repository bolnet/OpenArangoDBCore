# Project Research Summary

**Project:** OpenArangoDBCore
**Domain:** C++ Enterprise database plugin — drop-in replacement for ArangoDB's proprietary Enterprise module
**Researched:** 2026-03-29
**Confidence:** MEDIUM-HIGH

## Executive Summary

OpenArangoDBCore is a static C++ library (`openarangodb_enterprise`) that replaces ArangoDB's closed-source `enterprise/` directory by providing open implementations of all 19 Enterprise modules. It is not a standalone application — it is compiled as a subdirectory inside ArangoDB's own build when `-DUSE_ENTERPRISE=1` is set. This architecture fundamentally shapes every decision: the library must match ArangoDB's internal ABI exactly, use only bundled dependencies (RocksDB 7.x, VelocyPack) rather than independently-installed versions, and declare all types in ArangoDB's own namespaces (`arangodb`, `arangodb::audit`, etc.) rather than a custom namespace. The 19 modules span security (encryption, audit, LDAP auth), graph/cluster (SmartGraphs, sharding strategies, satellite collections), search (MinHash, TopK/WAND), backup/ops (hot backup, cloud backup, parallel index building), and replication (DC2DC). All 4 researchers independently identified the same single most critical requirement: **LicenseFeature must always return `true`** for all capability checks, because every other module is gated behind it at runtime.

The recommended approach is to work through a strict dependency-ordered build: LicenseFeature first (unlocks all `#ifdef USE_ENTERPRISE` paths), then security fundamentals (encryption, audit, LDAP), then graph and cluster infrastructure (SmartGraph sharding, satellite distribution), then search and backup, and finally DC2DC replication. Before writing any implementation code, every module's exact virtual interface signatures and namespace expectations must be extracted from the ArangoDB source tree (already available locally at `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/`). The ArangoDB source checkout is the highest-confidence reference — documentation frequently lags the real signatures. All 4 research files agree: read the headers first, then implement.

The highest risk in the project is not functional correctness but **ABI compatibility**. Three C++ landmines stand above all others: (1) the wrong namespace (`openarangodb` vs `arangodb`) causes silent link failures; (2) vtable ODR violations from mismatched `virtual` signatures cause crashes at runtime, not compile time; and (3) duplicate symbol definitions between OpenArangoDBCore and ArangoDB community code cause silent undefined behavior. All three must be addressed in Phase 1 before any module work begins. The second-highest risk is cryptographic correctness in the encryption module — IV reuse in AES-256-CTR, non-sequential file access with `EVP_CIPHER_CTX`, and failure to validate `RAND_bytes()` return values are concrete ways to ship code that appears to work but is cryptographically broken. DC2DC replication is the least-understood module and should be treated as a separate phase with its own deeper research.

---

## Key Findings

### Recommended Stack

OpenArangoDBCore inherits almost its entire dependency stack from ArangoDB's build system. The language standard is C++20 (confirmed in ArangoDB's own `VERSIONS` file: `"C++ standard": "20"`), targeting GCC 13.2.0 and Clang 19.1.7. RocksDB 7.2.x and VelocyPack are already bundled with ArangoDB — adding independent installations of either would cause ABI conflicts and must be avoided. The only truly external dependencies are OpenSSL 3.5.x (system-level, for AES-256-CTR encryption and TLS), OpenLDAP libldap 2.6.x (for LDAPHandler), and rclone 1.65.2 (for cloud backup — use subprocess execution, not the librclone C shim). For the standalone testing stack, GoogleTest 1.17.0 via CMake `FetchContent` is the correct choice because GMock is essential for mocking ArangoDB's abstract interfaces (`ApplicationFeature`, `ShardingStrategy`, etc.).

**Core technologies:**
- C++20 / CMake 3.20+: implementation language and build system — ArangoDB's own minimum, already correct in current CMakeLists.txt
- RocksDB 7.2.x (bundled at `${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/`): storage engine hooks for encryption, hot backup, index building — do NOT install separately
- VelocyPack (bundled at `${CMAKE_SOURCE_DIR}/3rdParty/velocypack/`): binary serialization — required by sharding, graph, and cluster modules — do NOT install separately
- OpenSSL 3.5.x (system): AES-256-CTR encryption via EVP API and enhanced TLS — use `EVP_CIPHER_CTX`, not deprecated `EVP_MD_CTX_create()`
- OpenLDAP libldap 2.6.x (system, `libldap-dev`): LDAP bind/search for authentication — use C API directly; all C++ wrappers are unmaintained since 2019-2021
- rclone 1.65.2 (subprocess): cloud backup — subprocess exec is simpler and matches how ArangoDB Enterprise itself uses it; librclone requires Go toolchain
- GoogleTest 1.17.0 (FetchContent, tests only): unit/mock testing — GMock is critical for mocking ArangoDB interfaces

See `.planning/research/STACK.md` for the full per-module dependency table and version compatibility matrix.

### Expected Features

All 19 modules are required for full ArangoDB Enterprise compatibility. They fall into a clear priority hierarchy. See `.planning/research/FEATURES.md` for complete behavioral specifications per module.

**Must have (table stakes) — missing any of these = incomplete product:**
- License Feature — the gate; must return `true` for all capability checks before anything else works
- Encryption at Rest (AES-256-CTR) — regulatory requirement (GDPR, HIPAA, SOC2); RocksDB EncryptionProvider
- Audit Logging (8 topics) — compliance baseline; file/syslog output per `ApplicationFeature`
- LDAP Authentication + External RBAC — enterprise identity management; two auth modes, TLS, group-to-role mapping
- Hot Backup (RocksDB snapshots) — production ops; global write lock + hard-link checkpoint
- SmartGraphs — core reason enterprises choose ArangoDB; shard co-location for graph traversal
- Parallel Index Building — ops quality for large collections; background, non-locking index construction
- ReadFromFollower — horizontal read scaling in clustered deployments
- Data Masking / Anonymization — GDPR export anonymization for non-prod environments
- SatelliteGraphs / Satellite Collections — full data locality for small reference datasets

**Should have (differentiators) — demonstrate technical depth:**
- Disjoint SmartGraphs — 40-120x query speedup via full query push-down to a single DB-Server
- Shard-local Graph Execution (`LocalTraversalNode`) — zero network hops for SmartGraph traversals
- DC-to-DC Replication — disaster recovery / geographic HA
- TopK Query Optimization (WAND) — fast ranked ArangoSearch queries with early termination
- MinHash Similarity Functions — approximate Jaccard similarity at scale for entity resolution
- Cloud Backup (RClone) — one-command backup to S3/Azure/GCS
- SmartToSat Edge Validation — mixed smart+satellite graph semantics
- Enhanced SSL/TLS (SNI, hot reload)
- External RBAC (already part of LDAPHandler; test independently)

**Defer (v2+) — out of scope per project goals:**
- Bidirectional DC2DC replication — ArangoDB itself does not support it; conflict resolution is unsolved
- Per-document field-level encryption — massive performance cost, breaks AQL indexes
- GUI / dashboard for audit logs — use Kibana/Grafana/Loki instead
- Live data masking in AQL queries — the Enterprise edition does not have this; dump-time only

### Architecture Approach

OpenArangoDBCore is composed of 19 modules organized into 6 subsystem layers mirroring ArangoDB's own `arangod/` directory layout exactly (the directory structure is not optional — `#include` paths must match what ArangoDB's community code expects). Server-level features (`AuditFeature`, `EncryptionFeature`, `LicenseFeature`, `SslServerFeatureEE`, `RCloneFeature`) all extend ArangoDB's `ApplicationFeature` base class and participate in the server's 7-phase lifecycle (collectOptions → validateOptions → prepare → start → beginShutdown → stop → unprepare). Storage features hook into RocksDB via three free functions (`collectEnterpriseOptions`, `validateEnterpriseOptions`, `configureEnterpriseRocksDBOptions`) called from `#ifdef USE_ENTERPRISE` blocks in `RocksDBEngine.cpp`. Graph and cluster features are registered via strategy patterns and factory registries — they do not activate through raw `#ifdef` calls but through explicit registration with `ShardingFeature`, `AqlFunctionFeature`, and the AQL optimizer rule registry. The build dependency order defines implementation order: LicenseFeature (no deps) → security + sharding foundations → graph traversal + cluster → search + backup → DC2DC.

**Major components and responsibilities:**
1. `LicenseFeature` (Layer 0) — always returns valid; without this, no other module activates regardless of correct implementation
2. `EncryptionProvider` + `EncryptionFeature` + `RocksDBEncryptionUtils` (Layer 1) — wraps all RocksDB SST/WAL writes in AES-256-CTR; hooks into RocksDB via `configureEnterpriseRocksDBOptions()`
3. `AuditFeature` (Layer 1) — intercepts request pipeline; writes structured records to file/syslog across 8 topic channels
4. `LDAPHandler` (Layer 1) — hooked from `Auth::AuthInfo::checkAuthentication()`; two bind modes + role mapping
5. `ShardingStrategyEE` + `SmartGraphSchema` (Layer 1) — consistent hash of `smartGraphAttribute`; enforces `{smartValue}:{localKey}` key format; immutable after collection creation
6. `SmartGraphProvider` + `SmartGraphStep` + `LocalTraversalNode` (Layer 2) — AQL execution push-down to DB-Server; zero cross-shard hops for local neighborhoods
7. `SatelliteDistribution` + `ReadFromFollower` (Layer 2) — replication factor = all DB-Servers for satellite; dirty-read routing for read scaling
8. `RocksDBHotBackup` (Layer 3) — per-DB-Server checkpoint creation; global lock coordination happens at Coordinator level
9. `RCloneFeature` (Layer 3) — subprocess rclone with strict timeout; streams upload/download progress
10. `DC2DCReplicator` (Layer 4) — ArangoDB-side replication stream API; must handle idempotent message application with sequence numbers

See `.planning/research/ARCHITECTURE.md` for data flow diagrams, integration point tables, and the complete build dependency graph.

### Critical Pitfalls

All 10 pitfalls are documented in `.planning/research/PITFALLS.md` with recovery strategies and phase assignments. The top 5 requiring action before or early in implementation:

1. **Wrong namespace (`openarangodb` vs `arangodb`)** — All current stubs use `namespace openarangodb`, which will not match ArangoDB's `#include` expectations. Run `grep -r "USE_ENTERPRISE" arangodb-core/` to extract every expected class name, namespace, and signature before writing a single line of implementation. Fix all namespaces in Phase 1 before any other work.

2. **Vtable ODR violation from incomplete or mismatched `virtual` overrides** — Use `override` on every virtual in every subclass; add `static_assert(!std::is_abstract_v<T>)` to every concrete class. Compile Enterprise headers together with ArangoDB headers from the start. A method with wrong `const` or wrong parameter type silently fails to override, causing crashes at runtime.

3. **ODR violation — duplicate symbol between OpenArangoDBCore and ArangoDB community** — Never redefine any type already present in ArangoDB's headers. Enable AddressSanitizer with `detect_odr_violation=2` in CI from day one. Use `target_include_directories` so OpenArangoDBCore always sees the canonical definitions.

4. **IV reuse in AES-256-CTR (catastrophic cryptographic failure)** — Always use `RAND_bytes(iv, 16)` for every new IV; assert its return value. Never use `rand()`, time-based seeds, or counter-based IVs. Additionally, `EVP_CIPHER_CTX` must be re-initialized for each seek offset — RocksDB reads SST files non-sequentially and a stateful context produces garbage for non-sequential reads.

5. **CMake static library link order — symbol starvation** — On Linux with GNU ld, static libraries are scanned once. If `openarangodb_enterprise` is listed before the object files that reference it, its symbols are discarded. Use `target_link_libraries(arangod PRIVATE openarangodb_enterprise)`; write a minimal link test (one symbol per module) in Phase 1 CI.

---

## Implications for Roadmap

Based on combined research, the dependency graph mandates a 5-phase structure. The ordering is not a preference — it is dictated by hard technical dependencies: LicenseFeature gates all other modules; encryption and auth have no cross-dependencies and can proceed in parallel; graph features depend on sharding foundations; DC2DC depends on the full cluster being operational.

### Phase 1: Foundation and ABI Baseline

**Rationale:** Without this phase being correct, no subsequent work is valid. Namespace mismatches, vtable ODR violations, and CMake link order issues are Phase 1 bugs that corrupt all later phases. LicenseFeature must be the first module that compiles and links correctly against the real ArangoDB headers — it is the gateway for all Enterprise feature activation.

**Delivers:** Correct namespace hierarchy for all 19 modules (based on ArangoDB source grep); working CMake structure that links against ArangoDB as a subdirectory; LicenseFeature returning `true` for all capability checks; CI pipeline with ASan (`detect_odr_violation=2`) and a minimal link test; project-wide `-Wall -Woverloaded-virtual -Werror` enforced.

**Addresses features from FEATURES.md:** License Feature (complete), foundational build structure for all 19 modules.

**Avoids pitfalls from PITFALLS.md:** Pitfall 1 (namespace mismatch), Pitfall 2 (vtable ODR), Pitfall 3 (ODR duplicate symbol), Pitfall 8 (CMake link order).

**Key action:** Before writing any class body, run `grep -r "USE_ENTERPRISE" /Users/aarjay/Documents/OpenArangoDB/arangodb-core/` and produce a `symbols_expected.txt` listing every required class, namespace, and virtual signature. This artifact drives all subsequent phases.

### Phase 2: Security Foundations

**Rationale:** Security modules (encryption, audit, LDAP) have no cross-dependencies on graph or cluster modules and are the highest-value features for enterprise adoption. Encryption at Rest is the most technically dangerous module (cryptographic correctness) and should be tackled while the codebase is small and review bandwidth is high. LDAP thread safety must be designed in from the start, not retrofitted.

**Delivers:** Working AES-256-CTR encryption at rest (all RocksDB SST/WAL files encrypted on disk, verified by `xxd` inspection); audit logging across all 8 topics to file/syslog; LDAP authentication in both simple and search modes with TLS; per-request `LDAP*` handle architecture preventing thread-safety issues; Enhanced SSL/TLS with SNI and hot reload; Data masking for arangodump.

**Uses from STACK.md:** OpenSSL 3.5.x EVP API, `libldap` 2.6.x C API, RocksDB `env_encryption.h`, `ApplicationFeature` base class.

**Implements from ARCHITECTURE.md:** `EncryptionProvider`, `EncryptionFeature`, `RocksDBEncryptionUtils`, `AuditFeature`, `LDAPHandler`, `SslServerFeatureEE`, `AttributeMasking`.

**Avoids from PITFALLS.md:** Pitfall 4 (IV reuse), Pitfall 7 (LDAP shared handle), Pitfall 9 (AES-CTR counter offset for random access).

**Research flag:** Encryption module needs careful review of ArangoDB's `RocksDBEngine.cpp` hooks — the exact signature of `configureEnterpriseRocksDBOptions` and how `EncryptionProvider` is registered in the `rocksdb::ObjectRegistry` must be confirmed from source before implementation.

### Phase 3: Graph and Cluster

**Rationale:** SmartGraph is the primary reason enterprises choose ArangoDB. The sharding foundation (`ShardingStrategyEE`, `SmartGraphSchema`) must be correct before the graph traversal layer (`SmartGraphProvider`, `SmartGraphStep`, `LocalTraversalNode`) is built on top of it. The `_key` format invariant (`{smartValue}:{localKey}`) is immutable after collection creation — getting it wrong here invalidates every traversal built on it.

**Delivers:** SmartGraph and Disjoint SmartGraph with correct `smartGraphAttribute`-based shard co-location; `_key` format validation and immutability enforcement; shard-local traversal execution via `LocalTraversalNode` (zero cross-shard hops for co-located neighborhoods); Satellite Collections and SatelliteGraphs (replication factor = all DB-Servers); ReadFromFollower (dirty-read header routing); SmartToSat edge validation.

**Uses from STACK.md:** VelocyPack (`Slice`, `Builder`), ArangoDB `ShardingStrategy` base, `ClusterInfo`, `ServerState` headers.

**Implements from ARCHITECTURE.md:** `ShardingStrategyEE`, `SmartGraphSchema`, `SmartGraphProvider`, `SmartGraphStep`, `LocalTraversalNode`, `SatelliteDistribution`, `ReadFromFollower`.

**Avoids from PITFALLS.md:** Pitfall 6 (SmartGraph immutable sharding attribute — enforce `canModifyShardingAttribute() = false` and validate `_key` format on every insert).

**Research flag:** `LocalTraversalNode` requires understanding ArangoDB's AQL optimizer rule registration API and the exact `ExecutionNode` serialization protocol for cross-node execution plan distribution. Browse `arangod/Aql/ExecutionNode.h` and the optimizer rule registration in `arangod/Aql/OptimizerRules.cpp` before starting.

### Phase 4: Search and Backup Operations

**Rationale:** MinHash, TopK/WAND, hot backup, parallel index building, and cloud backup are operationally independent of the graph/cluster layer. Hot backup depends on RocksDB being initialized (Layer 3 in the dependency graph) but not on SmartGraph correctness. The global write lock deadlock in hot backup is a design-time problem — the lock acquisition order must be specified before the implementation starts.

**Delivers:** `MINHASH`, `MINHASH_MATCH`, `JACCARD` AQL functions registered with `AqlFunctionFeature`; TopK/WAND optimization for ArangoSearch `SORT LIMIT` patterns; Hot Backup via RocksDB Checkpoint API (WAL flush → hard-link directory → lock release sequence); Parallel index building with configurable thread pool (background non-locking builds); Cloud backup via rclone subprocess with credentials, timeout, and incremental transfer.

**Uses from STACK.md:** RocksDB `utilities/checkpoint.h`, rclone 1.65.2 subprocess, xxHash for MinHash permutations, IResearch optimizer API.

**Implements from ARCHITECTURE.md:** `MinHashFunctions`, `IResearchOptimizeTopK`, `RocksDBHotBackup`, `RocksDBBuilderIndexEE`, `RCloneFeature`.

**Avoids from PITFALLS.md:** Pitfall 5 (hot backup global write lock deadlock — drain in-flight transactions before acquiring lock; never acquire per-shard locks individually).

**Research flag:** `IResearchOptimizeTopK` requires understanding IResearch's optimizer rule registration and WAND iterator interface — both are internal ArangoDB APIs with minimal public documentation. Inspect `arangod/IResearch/IResearchFeature.h` and the iresearch library at `3rdParty/iresearch/` before starting.

### Phase 5: DC2DC Replication

**Rationale:** DC2DC is the most complex module, the least documented, and the most dependent on the full system being operational (Layer 4 in the dependency graph). It involves an external Go-based arangosync process, a DirectMQ message queue protocol, and must handle network partitions with idempotency guarantees. Treating it as an isolated final phase is the correct risk-management decision — it should not block delivery of the other 18 modules.

**Delivers:** `DC2DCReplicator` implementing the ArangoDB-side replication stream API (WAL/change feed reader at source, write applier at target); monotonic sequence numbers in every replication message for idempotency; mTLS for all inter-datacenter connections; network partition handling (reconnect + replay without duplicates or data loss); integration with arangosync worker process lifecycle.

**Uses from STACK.md:** ArangoDB `Replication/ReplicationApplierConfiguration.h`, fuerte HTTP client (bundled), Auth headers.

**Implements from ARCHITECTURE.md:** `DC2DCReplicator` at Layer 4.

**Avoids from PITFALLS.md:** Pitfall 10 (DC2DC async delivery without idempotency guards — sequence numbers must be in the protocol design from day one, not retrofitted; replay test required before shipping).

**Research flag:** This phase needs `/gsd:research-phase` before planning. The C++ interface ArangoDB exposes for the replication stream feed, and how `DC2DCReplicator` interacts with the arangosync binary, is not documented publicly. The ArangoDB source at `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/Replication/` is the primary reference. Also review `arangod/RestServer/ArangodServer.cpp` for how the replication feature is registered.

### Phase Ordering Rationale

- Phase 1 must come first because namespace and ABI correctness are preconditions for every other phase. Code written with the wrong namespace compiles but links incorrectly — discovering this after 5 modules are implemented is a HIGH-cost recovery (all 5 headers must be renamed).
- Phases 2 and 3 can partially overlap after Phase 1 completes. Encryption and LDAP (Phase 2) share no dependencies with SmartGraph sharding (Phase 3). However, SmartGraphSchema depends on VocBase headers, and it is safer to verify the full security module set works end-to-end before expanding surface area.
- Phase 4 (backup/search) depends only on RocksDB being initialized (always true), not on Phase 3 correctness. However, keeping Phase 3 before Phase 4 preserves narrative momentum: the graph+cluster feature set delivers the most enterprise value and should be demonstrated first.
- Phase 5 (DC2DC) must be last. It depends on Auth (Phase 2), Cluster (Phase 3), and Replication subsystems, and introduces external process dependencies (arangosync) that require a fully operational cluster to test.

### Research Flags

Phases requiring deeper research during planning (run `/gsd:research-phase` before task decomposition):
- **Phase 1:** Namespace mapping requires systematic `grep` of ArangoDB source tree — automate `symbols_expected.txt` generation before writing a single class.
- **Phase 2 (Encryption):** Exact `rocksdb::ObjectRegistry` registration pattern for `EncryptionProvider` — needs source inspection of `RocksDBEngine.cpp`.
- **Phase 3 (Graph):** AQL optimizer rule registration API and `ExecutionNode` serialization for `LocalTraversalNode` — needs source inspection of `arangod/Aql/`.
- **Phase 4 (IResearch):** WAND iterator interface in IResearch internals — `3rdParty/iresearch/` inspection required.
- **Phase 5 (DC2DC):** Entire module — replication stream API, DirectMQ protocol, arangosync integration. This is the least-understood module across all 4 research files.

Phases with well-established patterns (can skip research-phase):
- **Phase 2 (Audit, LDAP, SSL):** `ApplicationFeature` lifecycle is well-documented; OpenLDAP C API is stable and well-documented. Standard patterns apply.
- **Phase 4 (Hot Backup):** RocksDB Checkpoint API is thoroughly documented; the implementation sequence (flush WAL → checkpoint → release) is confirmed.

---

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Core stack (C++20, CMake, RocksDB, VelocyPack, OpenSSL) verified against ArangoDB VERSIONS file and GitHub source. OpenLDAP version not pinned in VERSIONS — use system 2.6.x. rclone version 1.65.2 confirmed. |
| Features | MEDIUM-HIGH | All 19 module behaviors documented from official ArangoDB docs. Some behavioral nuances (exact MinHash Analyzer registration, SmartToSat validation details) inferred from docs rather than verified from source. |
| Architecture | MEDIUM | `ApplicationFeature` lifecycle, RocksDB hooks, and AQL function registration confirmed from ArangoDB source. Graph Provider/Steps base classes and `LocalTraversalNode` interface: MEDIUM — paths provided but exact signatures not fully verified. DC2DC architecture: MEDIUM — high-level understanding only. |
| Pitfalls | HIGH | ABI/ODR pitfalls are from authoritative C++ specifications and post-mortems. Crypto pitfalls are from RocksDB source and OpenSSL docs. LDAP thread safety from libldap documentation. CMake link order from GNU ld specification. |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **Exact namespace hierarchy:** The current stubs use `namespace openarangodb`. The correct namespaces (`arangodb`, `arangodb::audit`, `arangodb::encryption`, etc.) must be extracted from `grep -r "USE_ENTERPRISE" arangodb-core/` before Phase 1 begins. This is the single highest-priority discovery task.
- **`LocalTraversalNode` serialization protocol:** The `ExecutionNode` subclass must serialize/deserialize correctly for cross-node AQL plan distribution. The exact `toVelocyPackHelper` and `fromVelocyPack` signatures need verification from `arangod/Aql/ExecutionNode.h`.
- **DC2DC replication stream API:** No public documentation exists for the C++ interface between ArangoDB and arangosync. The `arangod/Replication/` directory in the local source checkout is the only reference. Phase 5 cannot be planned without reading these headers.
- **IResearch WAND iterator interface:** The iresearch library at `3rdParty/iresearch/` must be inspected for the optimizer rule hook and WAND-capable iterator API before `IResearchOptimizeTopK` can be implemented.
- **RocksDB `ObjectRegistry` pattern for EncryptionProvider:** The exact registration call differs between RocksDB versions. Verify against the bundled `3rdParty/rocksdb/` version before implementing.

---

## Sources

### Primary (HIGH confidence)
- ArangoDB devel VERSIONS file (GitHub) — C++20 standard, GCC 13.2.0, Clang 19.1.7, OpenSSL 3.5.5, rclone 1.65.2
- `ApplicationFeatures/ApplicationServer.h` (ArangoDB GitHub raw) — feature lifecycle: collectOptions, prepare, start, stop, unprepare
- `arangod/RocksDBEngine/RocksDBEngine.cpp` (ArangoDB GitHub raw) — `collectEnterpriseOptions`, `validateEnterpriseOptions`, `configureEnterpriseRocksDBOptions` hook signatures
- `arangod/Aql/AqlFunctionFeature.cpp` (ArangoDB GitHub raw) — Enterprise function registration pattern, `USE_ENTERPRISE` guard for `SELECT_SMART_DISTRIBUTE_GRAPH_INPUT`
- RocksDB `env_encryption.h` — `EncryptionProvider` virtual interface
- RocksDB `utilities/checkpoint.h` and Checkpoints wiki — `Checkpoint::Create()` API
- ArangoDB Audit Logging docs — 8 audit topics confirmed
- ArangoDB SmartGraphs docs (3.12/3.13) — smartGraphAttribute, shard co-location, disjoint constraint, 40-120x speedup claim
- ArangoDB Hot Backup docs and blog post — global write lock protocol, hard-link mechanism
- ArangoDB LDAP configuration docs — bind modes, search filter, role mapping
- SEI CERT DCL60-CPP, KDE Binary Compatibility guide — ODR violation specifications
- OpenSSL EVP Wiki — AES-256-CTR EVP API, IV handling
- Eli Bendersky: "Library Order in Static Linking" — CMake link order on GNU ld

### Secondary (MEDIUM confidence)
- ArangoDB DC2DC Replication and ArangoSync docs — high-level architecture (DirectMQ, arangosync master/worker); C++ interface not documented
- ArangoDB IResearch TopK/WAND performance docs — WAND optimization description; internal API not exposed
- ArangoDB rclone fork (GitHub) — cloud backup integration approach
- RocksDB AES-CTR PR #7240 and CockroachDB Encryption RFC — EncryptionProvider implementation patterns
- ArangoSync Reliability Blog post — idempotency requirements for DC2DC

### Tertiary (LOW confidence — needs validation from local source)
- `arangod/Graph/Providers/` and `arangod/Graph/Steps/` base class signatures — paths provided, exact virtual signatures not confirmed
- `DC2DCReplicator` base class in `arangod/Replication/` — existence inferred; content unknown without source inspection
- IResearch optimizer rule registration interface — inferred from docs; exact API requires `3rdParty/iresearch/` inspection

---
*Research completed: 2026-03-29*
*Ready for roadmap: yes*
