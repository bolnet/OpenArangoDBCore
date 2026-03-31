# Requirements: OpenArangoDBCore

**Defined:** 2026-03-30
**Core Value:** Every Enterprise-gated feature compiles and links correctly against ArangoDB, providing functionally equivalent open-source alternatives.

## v1.0 Requirements

Requirements for full Enterprise parity. Each maps to roadmap phases.

### Foundation

- [x] **FOUND-01**: All source files use `namespace arangodb` (matching ArangoDB's expected symbols)
- [x] **FOUND-02**: CMakeLists.txt integrates correctly with ArangoDB's build system when placed as `enterprise/` directory
- [x] **FOUND-03**: Static library links without unresolved symbols when built with `-DUSE_ENTERPRISE=1`
- [x] **FOUND-04**: LicenseFeature returns true for all enterprise capability checks (always-enabled)
- [x] **FOUND-05**: CI pipeline builds against ArangoDB source with AddressSanitizer enabled

### Audit Logging

- [x] **AUDIT-01**: AuditFeature registers as ApplicationFeature with correct lifecycle hooks
- [x] **AUDIT-02**: Audit events captured for all 8 topics (authentication, authorization, collection, database, document, view, service, hotbackup)
- [x] **AUDIT-03**: Audit log output configurable (file, syslog)
- [x] **AUDIT-04**: Audit logging is non-blocking (does not degrade server performance)

### Encryption at Rest

- [ ] **ENCR-01**: EncryptionProvider implements RocksDB encryption interface (AES-256-CTR)
- [ ] **ENCR-02**: Encryption key management supports keyfile and key rotation
- [ ] **ENCR-03**: IV generation uses cryptographically secure random (RAND_bytes)
- [ ] **ENCR-04**: Random-access read/write works correctly with counter-mode encryption
- [ ] **ENCR-05**: EncryptionFeature registers as ApplicationFeature and hooks into RocksDB engine startup

### LDAP Authentication & External RBAC

- [x] **AUTH-01**: LDAPHandler authenticates users against external LDAP directory
- [x] **AUTH-02**: LDAP connections use per-request handles (thread-safe under concurrent load)
- [x] **AUTH-03**: LDAP role mapping translates directory groups to ArangoDB roles
- [x] **AUTH-04**: External RBAC policy evaluation for fine-grained access control
- [x] **AUTH-05**: LDAP connection supports TLS (LDAPS and StartTLS)

### Data Masking

- [ ] **MASK-01**: AttributeMasking applies field-level masking rules to query results
- [ ] **MASK-02**: Masking rules configurable per collection and per user role
- [ ] **MASK-03**: Built-in masking strategies (redact, hash, partial, randomize)

### Enhanced SSL/TLS

- [ ] **SSL-01**: SslServerFeatureEE extends base SSL with enterprise TLS options
- [ ] **SSL-02**: Support for client certificate authentication (mTLS)
- [ ] **SSL-03**: TLS version and cipher suite restrictions configurable

### SmartGraphs

- [ ] **SMART-01**: ShardingStrategyEE implements smart graph sharding based on partition key prefix in `_key`
- [ ] **SMART-02**: SmartGraphSchema validates `_key` format (`prefix:suffix`) on document insert/update
- [ ] **SMART-03**: SmartGraphProvider routes graph traversal queries to correct shards
- [ ] **SMART-04**: SmartGraphStep executes traversal steps within shard boundaries
- [ ] **SMART-05**: Disjoint SmartGraph variant enforces vertex uniqueness per partition

### Satellite Collections & Graphs

- [ ] **SAT-01**: SatelliteDistribution replicates designated collections to all DB servers
- [ ] **SAT-02**: Satellite collection joins execute locally (no network hop)
- [ ] **SAT-03**: SmartToSat edge validation ensures edges between smart and satellite collections are valid

### Shard-local Graph Execution

- [ ] **SHARD-01**: LocalTraversalNode executes graph traversals entirely within a single shard
- [ ] **SHARD-02**: AQL optimizer recognizes shard-local traversal opportunities and rewrites query plan

### ReadFromFollower

- [ ] **RFF-01**: ReadFromFollower routes read queries to follower replicas when configured
- [ ] **RFF-02**: Staleness guarantees configurable (eventual vs bounded)

### MinHash Similarity

- [ ] **MHASH-01**: MinHash AQL functions registered and callable (`MINHASH`, `MINHASH_MATCH`)
- [ ] **MHASH-02**: MinHash signature generation produces correct locality-sensitive hashes
- [ ] **MHASH-03**: Jaccard similarity estimation via MinHash within configurable error bounds

### TopK Query Optimization

- [ ] **TOPK-01**: IResearchOptimizeTopK implements WAND algorithm for top-K scoring
- [ ] **TOPK-02**: AQL optimizer rule triggers TopK optimization when ORDER BY + LIMIT detected on search views
- [ ] **TOPK-03**: TopK skips non-competitive documents (measurable speedup vs full scan)

### Hot Backup

- [ ] **HBAK-01**: RocksDBHotBackup creates consistent point-in-time snapshots via RocksDB Checkpoint
- [ ] **HBAK-02**: Global write lock acquired before checkpoint to ensure consistency
- [ ] **HBAK-03**: Backup metadata (timestamp, version, collections) stored with snapshot
- [ ] **HBAK-04**: Restore from hot backup snapshot supported

### Cloud Backup (RClone)

- [ ] **CBAK-01**: RCloneFeature uploads hot backup snapshots to cloud storage (S3, Azure Blob, GCS)
- [ ] **CBAK-02**: RClone integration via subprocess execution (not librclone)
- [ ] **CBAK-03**: Cloud backup configurable (endpoint, credentials, bucket/container)

### Parallel Index Building

- [ ] **PIDX-01**: RocksDBBuilderIndexEE builds indexes using multiple threads
- [ ] **PIDX-02**: Parallel index building configurable (thread count, memory budget)
- [ ] **PIDX-03**: Index building does not block read queries during construction

### DC-to-DC Replication

- [ ] **DC2DC-01**: DC2DCReplicator implements replication stream between datacenters
- [ ] **DC2DC-02**: Replication is asynchronous with configurable lag tolerance
- [ ] **DC2DC-03**: Replication uses mTLS for inter-datacenter communication
- [ ] **DC2DC-04**: Idempotent replay with sequence numbers (handles network interruptions)
- [ ] **DC2DC-05**: Integration with arangosync protocol for replication coordination

## v2 Requirements

Deferred to future milestones. Tracked but not in current roadmap.

### Performance & Benchmarking

- **PERF-01**: Performance benchmarks for each module vs ArangoDB Enterprise
- **PERF-02**: Load testing under concurrent multi-tenant workloads
- **PERF-03**: Memory profiling and optimization pass

### Extended Features

- **EXT-01**: GUI admin panel integration for enterprise features
- **EXT-02**: Prometheus/Grafana metrics export for enterprise modules
- **EXT-03**: Configuration hot-reload without server restart

## Out of Scope

| Feature | Reason |
|---------|--------|
| ArangoDB core modifications | We provide enterprise/ directory only, not a fork |
| Web UI changes | C++ core modules only |
| Python OpenArangoDB integration | Separate project with its own roadmap |
| Proprietary license compatibility | Apache 2.0 only; no Enterprise license emulation |
| ArangoDB version pinning | Must work with latest stable; no backports |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| FOUND-01 | Phase 1 | Complete |
| FOUND-02 | Phase 1 | Complete |
| FOUND-03 | Phase 1 | Complete |
| FOUND-04 | Phase 1 | Complete |
| FOUND-05 | Phase 1 | Complete |
| AUDIT-01 | Phase 2 | Complete |
| AUDIT-02 | Phase 2 | Complete |
| AUDIT-03 | Phase 2 | Complete |
| AUDIT-04 | Phase 2 | Complete |
| ENCR-01 | Phase 2 | Pending |
| ENCR-02 | Phase 2 | Pending |
| ENCR-03 | Phase 2 | Pending |
| ENCR-04 | Phase 2 | Pending |
| ENCR-05 | Phase 2 | Pending |
| AUTH-01 | Phase 2 | Complete |
| AUTH-02 | Phase 2 | Complete |
| AUTH-03 | Phase 2 | Complete |
| AUTH-04 | Phase 2 | Complete |
| AUTH-05 | Phase 2 | Complete |
| MASK-01 | Phase 2 | Pending |
| MASK-02 | Phase 2 | Pending |
| MASK-03 | Phase 2 | Pending |
| SSL-01 | Phase 2 | Pending |
| SSL-02 | Phase 2 | Pending |
| SSL-03 | Phase 2 | Pending |
| SMART-01 | Phase 3 | Pending |
| SMART-02 | Phase 3 | Pending |
| SMART-03 | Phase 3 | Pending |
| SMART-04 | Phase 3 | Pending |
| SMART-05 | Phase 3 | Pending |
| SAT-01 | Phase 3 | Pending |
| SAT-02 | Phase 3 | Pending |
| SAT-03 | Phase 3 | Pending |
| SHARD-01 | Phase 3 | Pending |
| SHARD-02 | Phase 3 | Pending |
| RFF-01 | Phase 3 | Pending |
| RFF-02 | Phase 3 | Pending |
| MHASH-01 | Phase 4 | Pending |
| MHASH-02 | Phase 4 | Pending |
| MHASH-03 | Phase 4 | Pending |
| TOPK-01 | Phase 4 | Pending |
| TOPK-02 | Phase 4 | Pending |
| TOPK-03 | Phase 4 | Pending |
| HBAK-01 | Phase 4 | Pending |
| HBAK-02 | Phase 4 | Pending |
| HBAK-03 | Phase 4 | Pending |
| HBAK-04 | Phase 4 | Pending |
| CBAK-01 | Phase 4 | Pending |
| CBAK-02 | Phase 4 | Pending |
| CBAK-03 | Phase 4 | Pending |
| PIDX-01 | Phase 4 | Pending |
| PIDX-02 | Phase 4 | Pending |
| PIDX-03 | Phase 4 | Pending |
| DC2DC-01 | Phase 5 | Pending |
| DC2DC-02 | Phase 5 | Pending |
| DC2DC-03 | Phase 5 | Pending |
| DC2DC-04 | Phase 5 | Pending |
| DC2DC-05 | Phase 5 | Pending |

**Coverage:**
- v1.0 requirements: 53 total
- Mapped to phases: 53
- Unmapped: 0

---
*Requirements defined: 2026-03-30*
*Last updated: 2026-03-30 after roadmap creation — all 53 requirements mapped*
