# Feature Research

**Domain:** ArangoDB Enterprise Edition — C++ drop-in replacement (OpenArangoDBCore)
**Researched:** 2026-03-29
**Confidence:** MEDIUM (official ArangoDB docs redirect to gated domain; findings cross-referenced from multiple public sources)

---

## Module Behavior Reference

This section documents the expected behavior of all 19 Enterprise modules so implementers know exactly what each one must do.

### Security Group

#### 1. Audit Logging (8 topics)
**Directory:** `arangod/Audit/` — `AuditFeature`

ArangoDB Enterprise emits structured audit records to a dedicated log file (separate from the regular server log). Every event belongs to one of **8 topic channels** that can be individually enabled or disabled at runtime via `--audit.topics`:

| Topic | What it captures |
|-------|-----------------|
| `audit-authentication` | Login attempts (success, failure, unknown auth method) |
| `audit-authorization` | Permission checks when accessing databases or collections |
| `audit-collection` | Collection create/drop/modify operations |
| `audit-database` | Database create/drop operations |
| `audit-document` | Document read/write/delete operations (can be very verbose) |
| `audit-service` | Foxx service install/uninstall/replace operations |
| `audit-view` | View create/drop operations |
| `audit-hotbackup` | Hot backup create/upload/download/restore/delete events |

Each audit record is a single-line JSON or structured log entry containing: timestamp, server ID, client IP, username, database name, operation type, and result code. The output file is configurable via `--audit.output` (file path or `syslog`). The feature integrates as an `ApplicationFeature` (ArangoDB's server lifecycle hook system) and intercepts operations at the request-handler layer — not the storage engine — so it captures the intent even when storage operations fail.

**Implementation note:** `AuditFeature` must register as an ArangoDB `ApplicationFeature`, hook into the server's request pipeline, and write records through the configured appender. Topics map to bitfield flags checked at emit sites.

---

#### 2. Encryption at Rest (AES-256-CTR)
**Directory:** `arangod/Encryption/` — `EncryptionFeature`, `EncryptionProvider`; `arangod/RocksDBEngine/` — `RocksDBEncryptionUtils`

ArangoDB uses RocksDB's `EncryptionProvider` plugin API. The Enterprise module provides a concrete implementation that wraps **AES-256-CTR** (Counter Mode, parallelizable, no padding) using OpenSSL. Hardware acceleration (AES-NI instruction set) is used automatically when available.

Key management:
- A 32-byte master key is loaded from a file specified by `--rocksdb.encryption-key-file` (or `--rocksdb.encryption-keyfolder` for key rotation)
- ArangoDB implements **key indirection**: the data encryption key (DEK) is stored encrypted by the master key, enabling key rotation without re-encrypting all data
- Key rotation API: `PUT /_admin/encryption` (only enabled if `--rocksdb.encryption-key-rotation true`)

Scope: encrypts all SST files, WAL, MANIFEST — everything RocksDB writes to disk. Does not encrypt in-memory data or network traffic (that is TLS's job). Each server in a cluster should use a distinct key.

**Implementation note:** `EncryptionProvider` must implement RocksDB's `rocksdb::EncryptionProvider` abstract class. `EncryptionFeature` registers it with the RocksDB options at startup. `RocksDBEncryptionUtils` provides the AES-CTR block cipher, IV generation, and key-derivation helpers.

---

#### 3. LDAP Authentication
**Directory:** `arangod/Auth/` — `LDAPHandler`

ArangoDB delegates user authentication to an external LDAP/Active Directory server. Two authentication flows:

- **Simple mode:** ArangoDB constructs the DN directly from the username using a configurable prefix/suffix, then performs an LDAP bind with the user's password.
- **Search mode (two-phase):** A read-only service account first searches for the user's DN, then ArangoDB re-binds as that user to verify the password.

Authorization (role mapping) after authentication:
- **Roles attribute:** Fetches a multi-value attribute (e.g., `memberOf`) from the user's LDAP entry; each value is treated as a role name
- **Roles search:** Searches LDAP for objects representing roles the user belongs to

Roles map to ArangoDB database/collection permissions configured inside ArangoDB. Users effectively get the union of all role permissions. Authorization cache refreshes every `--ldap.refresh-rate` seconds (default 300s).

**Implementation note:** `LDAPHandler` must link against `libldap` (OpenLDAP), wrap bind/search operations, handle TLS (LDAP over SSL / StartTLS), and populate ArangoDB's internal user permission cache.

---

#### 4. External RBAC
**Directory:** `arangod/Auth/` — same files as LDAP

This is the role-mapping layer of LDAP Auth treated as a separable concern. External RBAC means ArangoDB's permission model (rw/ro/none per database and collection) is driven by external role memberships rather than roles stored in `_system/_users`. The LDAP roles-attribute and roles-search methods described above are the concrete implementations. In the codebase, this is the same `LDAPHandler`; the separation into a distinct "module" reflects that RBAC policy enforcement (the permission lookup path) and authentication (the bind path) have distinct code paths and can be tested independently.

**Implementation note:** Implement the permission-refresh path and role-to-permission resolution in `LDAPHandler`, verified independently of the bind/authentication path.

---

#### 5. Data Masking / Anonymization
**Directory:** `arangod/Maskings/` — `AttributeMasking`

Data masking is applied during **arangodump** to produce anonymized exports for dev/test environments or GDPR-compliant data sharing. It is a pre-export transformation, not a live query filter.

Configuration: a JSON masking spec file passed to `arangodump --maskings <file>` defines per-collection rules. Each rule specifies a `path` (attribute name or dotted nested path) and a `type`:

| Masking Type | Behavior |
|-------------|----------|
| `xifyFront` | Replaces leading alphanumeric chars with `x`, preserves structure |
| `randomString` | Replaces with random-length random string (Community Edition also) |
| `creditCard` | Emits a structurally valid random credit card number |
| `phoneNumber` | Emits a random phone number |
| `email` | Hashes the value → `AAAA.BBBB@CCCC.invalid` format |
| `null` | Replaces attribute value with JSON `null` |
| `zipcode` | Replaces with a random US-format ZIP code |
| `identity` | No masking (explicit pass-through) |

When the attribute value is an array, each element is masked individually. Objects are not masked in aggregate — you must specify each leaf field path.

**Implementation note:** `AttributeMasking` reads the masking config, registers a per-collection transformation hook in the dump pipeline, and dispatches to masking type implementations. Must handle nested paths, array values, and type coercion cleanly.

---

#### 6. Enhanced SSL/TLS
**Directory:** `arangod/Ssl/` — `SslServerFeatureEE`

Community Edition provides basic TLS. The Enterprise `SslServerFeatureEE` adds:

- **SNI (Server Name Indication) support:** Multiple TLS certificates can be loaded for different hostnames from a single server. Incoming connections trigger SNI callbacks that select the appropriate key/cert pair.
- **TLS hot reload:** The `POST /_admin/ssl/reload` API triggers a reload of all TLS data (certificates and keys) without restart, returning a summary of the new TLS configuration. This endpoint is superuser-only.
- **Extended cipher suite control:** Allows configuration of TLS 1.3 ciphersuites beyond what the Community Edition exposes.

**Implementation note:** `SslServerFeatureEE` subclasses or wraps `SslServerFeature`, extends the SNI callback via `SSL_CTX_set_tlsext_servername_callback`, and implements the reload HTTP endpoint. Uses OpenSSL 3.0+ APIs.

---

### Graph & Cluster Group

#### 7. SmartGraphs (partition-key sharding)
**Directory:** `arangod/Sharding/` — `ShardingStrategyEE`; `arangod/VocBase/` — `SmartGraphSchema`; `arangod/Graph/Providers/` and `arangod/Graph/Steps/`

SmartGraphs shard vertices and edges by a **smartGraphAttribute** value. Every vertex with the same attribute value lands on the same DB-Server shard. Edge documents store the smartGraphAttribute value of both endpoints as a compound shard key (`_from`'s attribute + `_to`'s attribute), ensuring that edges between co-located vertices are also local.

Graph traversals from a starting vertex that shares its shard with its neighbors execute with **zero network hops** for that neighborhood. The AQL optimizer detects SmartGraph traversals with a constant start vertex and pushes execution down to the responsible DB-Server (`TraversalNode` → `LocalTraversalNode` optimization).

Creation: `graph._create(name, edgeDefs, orphans, {smartGraphAttribute: "region", numberOfShards: 4})`

The `SmartGraphSchema` validator enforces that every vertex document inserted into a SmartGraph collection **contains** the smartGraphAttribute and that the shard key value is consistent with the document's `_key` (which encodes the attribute value as a prefix separated by `:`).

**Implementation note:** `ShardingStrategyEE` implements the ArangoDB `ShardingStrategy` interface for smart key distribution. `SmartGraphSchema` is a vocbase-level document validator. The Graph Providers/Steps handle the AQL execution layer push-down.

---

#### 8. Disjoint SmartGraphs
**Directory:** `arangod/Sharding/` — `ShardingStrategyEE`; `arangod/VocBase/` — `SmartGraphSchema`

An extension of SmartGraphs. In a regular SmartGraph, edges between vertices on **different** shards are permitted (cross-shard edges are stored on the source vertex's shard). In a **Disjoint** SmartGraph, cross-shard edges are **prohibited** — all edges must connect vertices with the same smartGraphAttribute value.

This prohibition enables a stronger AQL optimization: because the entire graph is partitioned into isolated subgraphs, the AQL optimizer can push **the complete query** (traversal, shortest path, k-shortest-paths, pattern matching) down to a single DB-Server without coordinating across shards. Performance gains measured at 40–120x in ArangoDB's internal tests.

Creation: same as SmartGraphs with an additional `{isDisjoint: true}` option.

Enforcement: `SmartGraphSchema` must reject edge documents that violate the disjoint constraint (i.e., `_from` and `_to` have different smartGraphAttribute values).

**Implementation note:** Mostly reuses the SmartGraph sharding strategy. The key addition is the constraint validation in `SmartGraphSchema` and the `isDisjoint` flag surfaced in the graph metadata so the AQL optimizer can apply the full push-down rule.

---

#### 9. SatelliteGraphs / Satellite Collections
**Directory:** `arangod/Cluster/` — `SatelliteDistribution`

SatelliteCollections are replicated **synchronously to every DB-Server** in the cluster (replication factor = number of DB-Servers). They exist to co-locate small reference datasets (lookup tables, configuration) with every shard of a large collection, eliminating cross-shard joins.

**SatelliteGraphs** are named graphs where all vertex and edge collections are SatelliteCollections. Because every DB-Server has the full graph, any traversal query executes entirely locally on whichever DB-Server the coordinator routes to — no network hops, full data locality.

The `SatelliteDistribution` component implements the shard assignment logic that places all shards of a SatelliteCollection on all current DB-Servers and keeps them in sync as the cluster scales.

**Implementation note:** `SatelliteDistribution` must implement ArangoDB's collection distribution interface to assign shards to all DB-Servers and set the replication factor accordingly. Integration with the cluster's agency (etcd-like consensus store) is needed for membership changes.

---

#### 10. SmartToSat Edge Validation
**Directory:** `arangod/VocBase/` — `SmartGraphSchema`

When a SmartGraph uses SatelliteCollections as vertex collections (the "SmartGraphs using SatelliteCollections" feature), edge collections connecting a SmartCollection to a SatelliteCollection require special validation. The edge `_to` or `_from` may reference a SatelliteCollection document, which has no shard constraint, but the edge itself must still be stored on the correct shard (determined by the SmartCollection side).

`SmartToSat` edge validation enforces:
1. The edge's shard key is derived from the SmartCollection vertex's smartGraphAttribute value (not the Satellite side)
2. The `_from`/`_to` referencing a SatelliteCollection is permitted without a matching smartGraphAttribute

**Implementation note:** This is a validation hook inside `SmartGraphSchema`. It inspects edge documents during insert/update to determine which side is the smart vertex and which is the satellite vertex, and routes shard key extraction accordingly.

---

#### 11. Shard-local Graph Execution
**Directory:** `arangod/Aql/` — `LocalTraversalNode`

This is the AQL query plan node that enables SmartGraph and Disjoint SmartGraph traversals to execute entirely on the DB-Server responsible for the start vertex, rather than being coordinated by a Coordinator node.

Standard cluster graph traversal: Coordinator receives traversal request → dispatches to DB-Server holding start vertex → DB-Server returns neighbors → Coordinator fetches edges for each neighbor (potentially cross-shard network calls).

Shard-local execution: AQL optimizer replaces a standard `TraversalNode` with a `LocalTraversalNode` when it detects a SmartGraph traversal with a deterministic start vertex. The `LocalTraversalNode` is serialized into the DB-Server's sub-execution plan and runs the entire traversal loop locally — neighbors are fetched from local RocksDB only.

For Disjoint SmartGraphs, the full query (including FILTER, SORT, LIMIT) can be pushed to the DB-Server.

**Implementation note:** `LocalTraversalNode` must implement ArangoDB's `ExecutionNode` interface. The optimizer rule (an enterprise-only rule registered in the AQL optimizer) detects the pattern and swaps the node. The DB-Server execution engine must handle the `LocalTraversalNode` type during deserialization of remote execution plans.

---

#### 12. ReadFromFollower
**Directory:** `arangod/Cluster/` — `ReadFromFollower`

In a standard ArangoDB cluster, **all reads go to the shard leader**. This ensures strong consistency but means read throughput does not scale with replication factor.

`ReadFromFollower` (Enterprise only) allows Coordinators to route read requests to **follower replicas** (shard copies on non-leader DB-Servers). This enables horizontal read scaling at the cost of potential staleness (follower may lag behind the leader by a few milliseconds in normal operation).

Behavior:
- Clients opt in per-request via the `X-Arango-Allow-Dirty-Read: true` HTTP header
- Responses carry `X-Arango-Potential-Dirty-Read: true` so clients can detect when a follower was used
- Enterprise: the Coordinator actively routes to followers, not just allows it; can also do round-robin across replicas for load distribution

**Implementation note:** `ReadFromFollower` hooks into the Coordinator's request routing layer to override the default leader-only routing when the dirty-read header is present and the collection has replication factor > 1.

---

### Search Group

#### 13. MinHash Similarity Functions
**Directory:** `arangod/Aql/` — `MinHashFunctions`

MinHash is a locality-sensitive hashing technique for approximating **Jaccard similarity** between sets. ArangoDB exposes this as:

1. A `minhash` **Analyzer** (ArangoSearch) — tokenizes a field into a set, computes k min-hash signatures, and indexes them
2. `MINHASH_MATCH(doc.field, targetValue, threshold, analyzer)` — AQL function that uses the minhash index to find candidates with approximate Jaccard similarity ≥ threshold
3. `JACCARD(array1, array2)` — AQL function that computes exact Jaccard similarity between two arrays

Use case: entity resolution (finding near-duplicate records), plagiarism detection, recommendation systems. MinHash approximation is O(k) vs O(n) for exact Jaccard.

**Implementation note:** `MinHashFunctions` registers the AQL functions with ArangoDB's function registry. The `minhash` Analyzer is a separate registration in the IResearch/ArangoSearch subsystem. The AQL functions must integrate with ArangoSearch Views that use the minhash Analyzer.

---

#### 14. TopK Query Optimization (WAND)
**Directory:** `arangod/IResearch/` — `IResearchOptimizeTopK`

ArangoSearch's TopK optimization (also called **WAND optimization** after the Weak AND algorithm) accelerates `SEARCH ... SORT BM25(doc) DESC LIMIT k` queries by pruning low-scoring documents early during index traversal, without scanning all index entries.

How it works:
1. The View definition (or inverted index) specifies `optimizeTopK` as a list of SORT expressions to accelerate
2. At query time, when AQL contains `SEARCH` + `SORT <expression> DESC` + `LIMIT k` where `<expression>` matches an optimized slot, the IResearch iterator uses the WAND algorithm: maintains a running threshold of the k-th best score seen so far, and skips posting list entries whose maximum possible score falls below this threshold
3. Documents are still ranked correctly — WAND is an exact top-k algorithm, not an approximation

**Implementation note:** `IResearchOptimizeTopK` registers the optimizer rule and the WAND-capable iterator variant. The rule detects `SORT LIMIT` patterns on ArangoSearch nodes and rewrites the execution plan to use the WAND iterator. IResearch (the underlying library) must support the necessary iterator interface.

---

### Replication Group

#### 15. DC-to-DC Replication
**Directory:** `arangod/Replication/` — `DC2DCReplicator`

Datacenter-to-datacenter replication (DC2DC / ArangoSync) enables **asynchronous, one-directional** replication of an entire ArangoDB cluster to a second cluster in another datacenter, for disaster recovery and geographic redundancy.

Architecture:
- **arangosync master** processes (at least 2 per datacenter for HA) manage synchronization tasks
- **arangosync worker** processes pull data from the source cluster and push to the target
- **DirectMQ**: a custom Go message queue for inter-datacenter communication
- Replication is async: writes to the source are acknowledged before replication completes; lag is typically seconds

What replicates: all databases, collections, indexes, views, users, Foxx applications. Structural changes (add/remove collections) replicate automatically without reconfiguration.

Limitation: unidirectional only — no active-active multi-master between datacenters.

**Implementation note:** `DC2DCReplicator` must implement the ArangoDB replication stream reader (reading WAL/change feed from source) and a writer that applies changes to the target cluster. The full arangosync binary is a Go service; the C++ module here is the ArangoDB-side integration point (e.g., the API endpoint that exposes the replication feed and accepts sync commands).

---

### Backup & Ops Group

#### 16. Hot Backup (RocksDB snapshots)
**Directory:** `arangod/RocksDBEngine/` — `RocksDBHotBackup`

Hot Backup creates **near-instantaneous, consistent, zero-downtime snapshots** of an entire ArangoDB deployment (single server or cluster). Unlike arangodump (logical dump), Hot Backup is a physical snapshot at the RocksDB storage layer.

Mechanism:
1. Coordinator acquires a **global write transaction lock** across all DB-Servers simultaneously (briefly pauses new writes; in-flight transactions drain first)
2. Each DB-Server creates a new directory `<data-dir>/backups/<timestamp>_<label>/` and hard-links all active RocksDB files into it — this is near-instantaneous since hard links are metadata-only
3. Lock is released; writes resume
4. The backup directory is self-contained (can be used with `--database.directory` to restore)

Incremental uploads: when uploading to cloud storage, arangobackup compares files with a previously uploaded backup and only transfers changed files (content-addressed by hash).

HTTP API: `POST /_admin/backup/create`, `GET /_admin/backup/list`, `DELETE /_admin/backup/{id}`, `POST /_admin/backup/restore`

**Implementation note:** `RocksDBHotBackup` handles the per-DB-Server backup creation (hard links, manifest files) and restore operations. The global lock coordination is handled by the Coordinator; the DB-Server module just needs to implement the local create/restore/delete/list operations and expose them via internal HTTP endpoints.

---

#### 17. Cloud Backup (RClone: S3/Azure/GCS)
**Directory:** `arangod/RClone/` — `RCloneFeature`

Extends Hot Backup with upload/download to/from cloud object storage. ArangoDB bundles **rclone** (a Go binary for inter-site sync supporting S3, Azure Blob, Google Cloud Storage, Dropbox, WebDAV, and 40+ other backends).

Usage:
- `arangobackup upload --rclone-config-file remote.json --remote-path s3://my-bucket/backups --backup-id <id>`
- `arangobackup download --rclone-config-file remote.json --remote-path s3://my-bucket/backups/<id>`

The remote.json is an rclone-format JSON config with credentials (access key, secret, region, etc.). Every rclone config option can be passed as JSON key-value pairs.

Incremental: upload compares local file hashes against already-uploaded files at the remote path; only changed files are transferred.

**Implementation note:** `RCloneFeature` is the ArangoDB C++ integration layer that invokes the bundled rclone binary (or links rclone as a library) with appropriate arguments, streaming upload/download progress back to the arangobackup client via the HTTP API. Must handle subprocess lifecycle, credential passing, and error mapping.

---

#### 18. Parallel Index Building
**Directory:** `arangod/RocksDBEngine/` — `RocksDBBuilderIndexEE`

Community Edition builds indexes single-threaded. The Enterprise `RocksDBBuilderIndexEE` extends the index builder to use **multiple parallel threads** for non-unique index construction.

Two orthogonal capabilities:
1. **Parallel threads per index:** Uses 2+ threads (configurable) to partition the collection's key space and build index segments concurrently, then merge
2. **Background index creation:** Index builds do not hold an exclusive collection lock for the entire duration; the collection remains available for CRUD operations during build. RocksDB only. Enabled by setting `inBackground: true` on the createIndex call.

Tradeoff: Background builds are slower than foreground builds and use more RAM (must track removed documents to handle concurrent deletes during build).

**Implementation note:** `RocksDBBuilderIndexEE` subclasses or wraps the Community index builder, adds a thread pool, partitions the RocksDB key-space scan across threads, and merges results. Must handle concurrent writes (via a change-log buffer) to produce a correct index even when documents change during build.

---

### Infrastructure Group

#### 19. License Feature (always enabled)
**Directory:** `arangod/License/` — `LicenseFeature`

ArangoDB's Enterprise binary checks a license key at startup and periodically during operation. Without a valid license, Enterprise features are either disabled or the server refuses to start (depending on version and enforcement policy).

For OpenArangoDBCore, the `LicenseFeature` is an **open implementation that always reports a valid, perpetual license** for all Enterprise features. This is the core enabling mechanism — without it, even correct implementations of the other 18 modules would be gated off.

Expected behavior:
- `LicenseFeature::isEnterprise()` → always `true`
- `LicenseFeature::checkLicense(feature)` → always `LicenseResult::Valid`
- Startup check → passes unconditionally
- HTTP endpoint `GET /_admin/license` → returns a synthetic license object indicating all features are active

**Implementation note:** `LicenseFeature` must implement ArangoDB's `ApplicationFeature` interface, register with the feature registry at startup, and provide the same public API as the proprietary license checker. All other modules call into this to gate their activation — if the API matches, they activate normally.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that any serious Enterprise replacement must have. Missing any of these = the product is incomplete for the target use case.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| License Feature | Without it, nothing else works | LOW | Pure stub; always returns valid |
| Encryption at Rest | Regulatory requirement (GDPR, HIPAA, SOC2) | HIGH | RocksDB EncryptionProvider + OpenSSL AES-256-CTR |
| Audit Logging | Compliance and forensics baseline | MEDIUM | 8 topics, file/syslog appender, ApplicationFeature integration |
| LDAP Authentication | Enterprise identity management standard | HIGH | libldap dependency, two auth modes, TLS, cache |
| Hot Backup | Production ops requirement; no downtime backup | HIGH | Global write lock, hard-link mechanism, RocksDB integration |
| SmartGraphs | Core reason enterprises choose ArangoDB over competitors | HIGH | Sharding strategy, schema validation, AQL optimizer rule |
| Parallel Index Building | Ops quality; large collections need fast index builds | MEDIUM | Thread-parallel RocksDB scan, background build support |
| ReadFromFollower | Read scalability in clustered deployments | MEDIUM | Coordinator routing override, dirty-read header handling |
| Data Masking | GDPR data anonymization for non-prod environments | MEDIUM | arangodump integration, 7+ masking type implementations |
| SatelliteGraphs | Full data locality for small reference graphs | HIGH | Replication factor = all DB-Servers, shard distribution logic |

### Differentiators (Competitive Advantage)

Features that distinguish this implementation from "yet another ArangoDB wrapper" and demonstrate technical depth.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Disjoint SmartGraphs | Full query push-down; 40–120x speedup over regular cluster queries | HIGH | Extends SmartGraph sharding + disjoint constraint enforcement |
| Shard-local Graph Execution | Zero network hops for SmartGraph traversals | HIGH | LocalTraversalNode AQL execution node, optimizer rule |
| DC-to-DC Replication | Disaster recovery / geographic HA | VERY HIGH | Requires replication stream API, possibly arangosync process |
| TopK Query Optimization (WAND) | Fast ranked search with LIMIT without scanning all docs | HIGH | IResearch WAND iterator, optimizer rule in AQL |
| MinHash Similarity | Approximate Jaccard similarity at scale | MEDIUM | AQL function registration + minhash Analyzer |
| Cloud Backup (RClone) | One-command backup to S3/Azure/GCS | MEDIUM | rclone integration, subprocess management |
| SmartToSat Edge Validation | Enables mixed smart+satellite graphs with correct semantics | MEDIUM | Schema validator extension of SmartGraph |
| Enhanced SSL/TLS | Multi-cert SNI, hot reload without restart | MEDIUM | OpenSSL SNI callback, reload HTTP endpoint |
| External RBAC | Roles driven entirely from LDAP groups; no ArangoDB-side user management | MEDIUM | Part of LDAPHandler; needs independent testing |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Bidirectional DC2DC replication | Active-active across datacenters sounds appealing | Conflict resolution is unsolved; ArangoDB itself does not support it | Implement unidirectional per spec; document the limitation |
| Per-document encryption | Fine-grained security at field level | Massive performance cost; breaks AQL indexes and query patterns | Use data masking at export time; use collection-level encryption |
| GUI / web UI for audit logs | Users want a dashboard | Out of scope per PROJECT.md; adds frontend complexity | Direct users to Kibana/Grafana/Loki for log analysis |
| Live data masking in queries | Mask sensitive fields in AQL SELECT results | ArangoDB's Enterprise edition does not have this; it is a dump-time feature | Document the intended use: export masking only |
| ArangoDB fork maintenance | Some want a patched ArangoDB binary | Means owning the main ArangoDB codebase — huge maintenance burden | OpenArangoDBCore is an Enterprise directory only; ArangoDB is untouched |

---

## Feature Dependencies

```
License Feature
    └──enables──> ALL other 18 modules (gating check)

Encryption at Rest
    └──requires──> RocksDB storage engine (always true for ArangoDB)
    └──requires──> OpenSSL 3.0+
    └──enhances──> Hot Backup (encrypted backups)
    └──enhances──> Cloud Backup (encrypted at rest before upload)

LDAP Authentication
    └──requires──> libldap
    └──enables──> External RBAC (same module, role-mapping path)

External RBAC
    └──requires──> LDAP Authentication (shares LDAPHandler)

Hot Backup
    └──requires──> RocksDB storage engine
    └──enables──> Cloud Backup (hot backup is the local snapshot; cloud backup uploads it)

Cloud Backup (RClone)
    └──requires──> Hot Backup (creates the local snapshot first)
    └──requires──> rclone binary bundled with ArangoDB

SmartGraphs
    └──requires──> ShardingStrategyEE (partition key logic)
    └──requires──> SmartGraphSchema (document validator)
    └──enables──> Disjoint SmartGraphs (extension with stronger constraint)
    └──enables──> Shard-local Graph Execution (optimizer only triggers for SmartGraphs)
    └──enables──> SmartToSat Edge Validation (when Satellite vertex collections added)

Disjoint SmartGraphs
    └──requires──> SmartGraphs (extends sharding + schema)

SatelliteGraphs
    └──requires──> SatelliteDistribution (replication to all DB-Servers)
    └──enhances──> SmartGraphs (SmartGraphs using SatelliteCollections)

SmartToSat Edge Validation
    └──requires──> SmartGraphs
    └──requires──> SatelliteGraphs (satellite vertex collections must exist)

Shard-local Graph Execution
    └──requires──> SmartGraphs (optimizer rule only fires on SmartGraph queries)
    └──requires──> LocalTraversalNode (the AQL execution node)

TopK Query Optimization
    └──requires──> ArangoSearch / IResearch View with optimizeTopK configured
    └──requires──> AQL optimizer rule registration

MinHash Similarity
    └──requires──> ArangoSearch minhash Analyzer (separate registration)
    └──requires──> AQL function registration

Parallel Index Building
    └──requires──> RocksDB storage engine (background build is RocksDB-only)

ReadFromFollower
    └──requires──> Cluster deployment (replication factor ≥ 2)

DC-to-DC Replication
    └──requires──> Cluster deployment (source and target)
    └──independent──> other modules (no hard dep, but pairs well with Hot Backup for point-in-time recovery)

Audit Logging
    └──independent──> (no deps; hooks into request pipeline)

Data Masking
    └──independent──> (arangodump integration; no server-side deps)

Enhanced SSL/TLS
    └──requires──> OpenSSL 3.0+ (SNI callbacks)
    └──independent──> other modules
```

### Dependency Notes

- **License Feature must be built first:** Every other module calls `LicenseFeature::checkLicense()` at startup. If the symbol is missing, nothing links.
- **Hot Backup is a prerequisite for Cloud Backup:** Cloud backup is an upload layer on top of local snapshots. Implement and test Hot Backup standalone before adding RClone.
- **SmartGraphs is the dependency hub for graph modules:** Disjoint SmartGraphs, SmartToSat Edge Validation, and Shard-local Graph Execution all extend the SmartGraph machinery. Build SmartGraphs first and use it as the base.
- **LDAP Auth and External RBAC share a file:** They are the same `LDAPHandler` component. Do not treat as two separate implementations; test the authentication path and the authorization/role-mapping path as a unit.
- **Encryption at Rest enhances backup but is independent:** Hot Backup works without encryption; encrypted backups are the intersection of both features being active.

---

## MVP Definition

This is a complete-parity Enterprise replacement, so all 19 modules are required for v1.0. However, the implementation order below reflects build dependencies and risk:

### Phase 1: Foundation (Build First)

These unblock everything else.

- [ ] **License Feature** — gates all other modules; must be correct or nothing else can activate
- [ ] **Encryption at Rest** — foundational security; needed early to validate RocksDB EncryptionProvider integration
- [ ] **Audit Logging** — validates the ApplicationFeature registration pattern used by most modules

### Phase 2: Security Cluster

- [ ] **LDAP Authentication + External RBAC** — shared module; implement together
- [ ] **Data Masking** — arangodump integration; relatively self-contained
- [ ] **Enhanced SSL/TLS** — OpenSSL SNI extension; needed for secure cluster setups

### Phase 3: Backup & Ops Cluster

- [ ] **Hot Backup** — high-value, high-risk; RocksDB hard-link mechanism needs careful testing
- [ ] **Parallel Index Building** — extends RocksDB builder; lower risk than Hot Backup
- [ ] **Cloud Backup (RClone)** — depends on Hot Backup being solid

### Phase 4: Graph & Cluster Cluster

- [ ] **SmartGraphs** — core graph feature; must be correct before extending
- [ ] **Disjoint SmartGraphs** — extends SmartGraphs with disjoint constraint
- [ ] **SatelliteGraphs** — independent of SmartGraphs but uses same graph framework
- [ ] **SmartToSat Edge Validation** — requires both SmartGraphs and SatelliteGraphs
- [ ] **Shard-local Graph Execution** — AQL optimizer layer on top of SmartGraphs
- [ ] **ReadFromFollower** — cluster routing change; independent of graph modules

### Phase 5: Search & Replication

- [ ] **MinHash Similarity** — AQL function + Analyzer registration
- [ ] **TopK Query Optimization** — IResearch optimizer rule
- [ ] **DC-to-DC Replication** — highest complexity; deferred to last

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| License Feature | HIGH (enables everything) | LOW | P1 |
| Encryption at Rest | HIGH | HIGH | P1 |
| Audit Logging | HIGH | MEDIUM | P1 |
| LDAP Authentication | HIGH | HIGH | P1 |
| External RBAC | HIGH | MEDIUM (shares LDAP) | P1 |
| Hot Backup | HIGH | HIGH | P1 |
| SmartGraphs | HIGH | HIGH | P1 |
| SatelliteGraphs | HIGH | HIGH | P1 |
| Disjoint SmartGraphs | HIGH | MEDIUM (extends SmartGraphs) | P1 |
| Shard-local Graph Execution | HIGH | HIGH | P1 |
| ReadFromFollower | MEDIUM | MEDIUM | P1 |
| Data Masking | MEDIUM | MEDIUM | P2 |
| Enhanced SSL/TLS | MEDIUM | MEDIUM | P2 |
| Parallel Index Building | MEDIUM | MEDIUM | P2 |
| Cloud Backup (RClone) | MEDIUM | MEDIUM (depends on Hot Backup) | P2 |
| MinHash Similarity | MEDIUM | MEDIUM | P2 |
| TopK Query Optimization | MEDIUM | HIGH | P2 |
| SmartToSat Edge Validation | MEDIUM | MEDIUM (extends SmartGraphs) | P2 |
| DC-to-DC Replication | HIGH | VERY HIGH | P3 |

**Priority key:**
- P1: Must have for v1.0 launch (core Enterprise parity)
- P2: Should have for v1.0, but can slip to v1.1 if blocked
- P3: Architecturally complex; may require its own milestone

---

## Module Complexity Assessment

| Module | Complexity | Why |
|--------|------------|-----|
| License Feature | LOW | Pure stub returning valid; no logic |
| Audit Logging | MEDIUM | ApplicationFeature integration, topic routing, appender I/O |
| Data Masking | MEDIUM | Masking type dispatching, arangodump hook |
| Enhanced SSL/TLS | MEDIUM | OpenSSL SNI callback, reload endpoint |
| External RBAC | MEDIUM | Role-mapping path in LDAPHandler |
| ReadFromFollower | MEDIUM | Coordinator routing override, header handling |
| MinHash Similarity | MEDIUM | AQL function + Analyzer registration |
| SmartToSat Edge Validation | MEDIUM | Schema validator extension |
| Parallel Index Building | MEDIUM | Thread pool, RocksDB scan partitioning |
| Cloud Backup (RClone) | MEDIUM | rclone subprocess/library integration |
| Encryption at Rest | HIGH | RocksDB EncryptionProvider ABI, key rotation |
| LDAP Authentication | HIGH | libldap, two auth modes, TLS, cache refresh |
| SatelliteGraphs | HIGH | Replication-to-all-nodes distribution logic, agency integration |
| SmartGraphs | HIGH | Sharding strategy, schema validator, optimizer rule |
| Hot Backup | HIGH | Global write lock, hard-link mechanism, cluster coordination |
| Shard-local Graph Execution | HIGH | LocalTraversalNode AQL node, optimizer rule, plan serialization |
| TopK Query Optimization | HIGH | IResearch WAND iterator, optimizer rule |
| Disjoint SmartGraphs | HIGH | SmartGraph base + disjoint constraint enforcement + full push-down |
| DC-to-DC Replication | VERY HIGH | Replication stream API, async replication protocol, arangosync interaction |

---

## Sources

- [ArangoDB Enterprise Edition Features — docs.arangodb.com](https://docs.arangodb.com/3.12/about-arangodb/features/enterprise-edition/)
- [SmartGraphs — docs.arangodb.com 3.12](https://docs.arangodb.com/3.12/graphs/smartgraphs/)
- [SatelliteGraphs — docs.arangodb.com 3.10](https://docs.arangodb.com/3.10/graphs/satellitegraphs/)
- [Audit Logging — docs.arangodb.com 3.11](https://docs.arangodb.com/3.11/operations/security/audit-logging/)
- [Encryption at Rest — docs.arangodb.com](https://docs.arangodb.com/3.10/operations/security/encryption-at-rest/)
- [Data Masking — docs.arangodb.com 3.11](https://docs.arangodb.com/3.11/components/tools/arangodump/maskings/)
- [ArangoDB Data Masking Tutorial](https://arangodb.com/learn/development/data-masking-tutorial/)
- [LDAP Configuration — docs.arangodb.com](https://www.arangodb.com/docs/stable/programs-arangod-ldap.html)
- [DC2DC Replication Architecture](https://www.arangodb.com/docs/stable/architecture-deployment-modes-dc2-dc-introduction.html)
- [Hot Backup — ArangoDB Blog](https://arangodb.com/2019/10/arangodb-hot-backup-creating-consistent-cluster-wide-snapshots/)
- [Hot Backup HTTP API](https://www.arangodb.com/docs/stable/http/hot-backup.html)
- [Backup and Restore — docs.arangodb.com 3.11](https://docs.arangodb.com/3.11/operations/backup-and-restore/)
- [ArangoSearch WAND Optimization — docs.arangodb.com 3.11](https://docs.arangodb.com/3.11/index-and-search/arangosearch/performance/)
- [ArangoSearch Functions AQL — docs.arango.ai](https://docs.arango.ai/arangodb/3.12/aql/functions/arangosearch/)
- [ArangoSync Architecture](https://medium.com/arangodb/arangosync-a-recipe-for-reliability-bf07f9d8128d)
- [ArangoDB SmartGraphs Enterprise Page](https://arangodb.com/enterprise-server/smartgraphs/)
- [ArangoDB Security Enterprise Page](https://www.arangodb.com/why-arangodb/arangodb-enterprise/arangodb-enterprise-security/)

---

*Feature research for: OpenArangoDBCore — ArangoDB Enterprise Edition C++ replacement*
*Researched: 2026-03-29*
