# Roadmap: OpenArangoDBCore

## Overview

OpenArangoDBCore implements all 19 ArangoDB Enterprise modules as an open-source drop-in replacement. The build order is dictated by hard technical dependencies: the ABI/namespace foundation must be correct before any module compiles against ArangoDB's headers; security modules share no cross-dependencies with graph modules and can follow immediately; graph and cluster infrastructure enables the traversal and sharding features enterprises depend on most; search and backup operations stand independently on top of the cluster layer; DC2DC replication is the most complex module and requires the full system to be operational before it can be tested. Every phase delivers a coherent, verifiable slice of the 53 v1.0 requirements.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation and ABI Baseline** - Namespace correctness, CMake integration, LicenseFeature, and CI with AddressSanitizer — everything downstream depends on this
- [ ] **Phase 2: Security Foundations** - Encryption at Rest, Audit Logging, LDAP Authentication, Data Masking, and Enhanced SSL/TLS
- [ ] **Phase 3: Graph and Cluster** - SmartGraphs, Disjoint SmartGraphs, Satellite Collections, Shard-local Execution, and ReadFromFollower
- [ ] **Phase 4: Search and Backup Operations** - MinHash similarity, TopK/WAND optimization, Hot Backup, Cloud Backup, and Parallel Index Building
- [ ] **Phase 5: DC-to-DC Replication** - Asynchronous cross-datacenter replication with mTLS, idempotent replay, and arangosync integration

## Phase Details

### Phase 1: Foundation and ABI Baseline
**Goal**: ArangoDB builds with `-DUSE_ENTERPRISE=1` against OpenArangoDBCore with no link errors, no namespace mismatches, and no ODR violations — every subsequent phase depends on this being correct
**Depends on**: Nothing (first phase)
**Requirements**: FOUND-01, FOUND-02, FOUND-03, FOUND-04, FOUND-05
**Success Criteria** (what must be TRUE):
  1. Running `cmake` with `-DUSE_ENTERPRISE=1` pointing at OpenArangoDBCore as `enterprise/` produces a complete ArangoDB binary without unresolved symbol errors
  2. Every source file in OpenArangoDBCore declares symbols in the `arangodb` namespace (matching ArangoDB's expected symbols), verified by a `grep` audit producing a clean `symbols_expected.txt`
  3. `LicenseFeature::isEnterprise()` and all capability check methods return `true` in a running ArangoDB instance, verified by querying `/_admin/license`
  4. CI runs the full build with AddressSanitizer (`detect_odr_violation=2`) and exits clean — no ODR violations, no undefined behavior from duplicate symbols
  5. A minimal per-module link test (one symbol per module) passes, confirming static library link order is correct on GNU ld
**Plans:** 1/3 plans executed

Plans:
- [ ] 01-01-PLAN.md — Restructure arangod/ to Enterprise/, fix namespaces, create 14 missing stubs
- [ ] 01-02-PLAN.md — Rewrite CMakeLists.txt, implement LicenseFeature, declare all 6 core features
- [ ] 01-03-PLAN.md — CI scripts (ASan, namespace audit), test infrastructure (GoogleTest, link tests)

### Phase 2: Security Foundations
**Goal**: ArangoDB with OpenArangoDBCore encrypts all data at rest, records compliance audit events, authenticates users against LDAP, applies data masking rules, and enforces enterprise TLS settings
**Depends on**: Phase 1
**Requirements**: AUDIT-01, AUDIT-02, AUDIT-03, AUDIT-04, ENCR-01, ENCR-02, ENCR-03, ENCR-04, ENCR-05, AUTH-01, AUTH-02, AUTH-03, AUTH-04, AUTH-05, MASK-01, MASK-02, MASK-03, SSL-01, SSL-02, SSL-03
**Success Criteria** (what must be TRUE):
  1. RocksDB SST and WAL files on disk contain no plaintext when encryption is enabled, verified by `xxd` inspection showing no human-readable ArangoDB metadata
  2. Authenticating as an LDAP-managed user succeeds and maps to the correct ArangoDB role; concurrent requests from multiple threads succeed without handle collisions
  3. All 8 audit topics (authentication, authorization, collection, database, document, view, service, hotbackup) produce structured records in the configured file or syslog output without measurable request latency increase
  4. A dump via `arangodump` with masking rules active produces output where configured fields are redacted/hashed, not the original values
  5. ArangoDB accepts client certificates for mTLS and rejects connections using TLS versions or cipher suites below the configured minimum
**Plans**: TBD

### Phase 3: Graph and Cluster
**Goal**: SmartGraph traversals execute with correct shard co-location, satellite collections replicate to all DB-Servers for local joins, shard-local traversal rewrites eliminate cross-shard hops, and read queries route to follower replicas when configured
**Depends on**: Phase 1
**Requirements**: SMART-01, SMART-02, SMART-03, SMART-04, SMART-05, SAT-01, SAT-02, SAT-03, SHARD-01, SHARD-02, RFF-01, RFF-02
**Success Criteria** (what must be TRUE):
  1. Inserting a document into a SmartGraph collection with a malformed `_key` (missing `prefix:suffix` format) is rejected with a validation error; a correctly formatted `_key` is accepted and routed to the correct shard
  2. A SmartGraph traversal query executes entirely within the owning DB-Server's shard — the AQL query plan shows `LocalTraversalNode` and no cross-shard scatter/gather steps
  3. Disjoint SmartGraph enforces vertex uniqueness per partition — an edge connecting vertices from different partitions is rejected
  4. A query joining a satellite collection executes without a network hop to another DB-Server, confirmed by the AQL explain plan showing no `RemoteNode` for the satellite side
  5. With ReadFromFollower enabled, read queries return results from follower replicas; the configured staleness bound is respected
**Plans**: TBD

### Phase 4: Search and Backup Operations
**Goal**: MinHash AQL functions return correct Jaccard similarity estimates, TopK searches on ArangoSearch views apply WAND early termination, hot backup creates restorable point-in-time snapshots, cloud backup uploads snapshots to object storage, and large collection indexes build in parallel without blocking reads
**Depends on**: Phase 1
**Requirements**: MHASH-01, MHASH-02, MHASH-03, TOPK-01, TOPK-02, TOPK-03, HBAK-01, HBAK-02, HBAK-03, HBAK-04, CBAK-01, CBAK-02, CBAK-03, PIDX-01, PIDX-02, PIDX-03
**Success Criteria** (what must be TRUE):
  1. `MINHASH()` and `MINHASH_MATCH()` are callable in AQL and return Jaccard similarity estimates within the documented error bounds for known test sets
  2. An AQL query with `SORT BM25(doc) LIMIT N` against an ArangoSearch view produces an explain plan showing the TopK/WAND optimizer rule applied; documents below the competitive score threshold are not scored
  3. A hot backup snapshot can be created while the database is live, the server continues serving reads during backup, and the database restores correctly from the snapshot to the exact state at backup time
  4. A hot backup snapshot uploads successfully to a configured S3/Azure/GCS bucket via rclone subprocess; the upload can be configured with credentials and bucket path
  5. An index build on a large collection completes using multiple threads and read queries return correct results throughout the entire build duration
**Plans**: TBD

### Phase 5: DC-to-DC Replication
**Goal**: ArangoDB instances in two separate datacenters maintain asynchronous replication with mTLS transport, sequence-numbered idempotent message delivery, and correct integration with the arangosync coordination process
**Depends on**: Phase 2, Phase 3
**Requirements**: DC2DC-01, DC2DC-02, DC2DC-03, DC2DC-04, DC2DC-05
**Success Criteria** (what must be TRUE):
  1. Writes committed on the source datacenter appear on the target datacenter within the configured lag tolerance window
  2. Simulating a network partition (dropping replication traffic) and then restoring connectivity results in the target catching up to the source state with no data loss or duplicate documents
  3. All inter-datacenter replication traffic uses mTLS; a connection attempt with an invalid certificate is rejected
  4. The `DC2DCReplicator` integrates with the arangosync worker process lifecycle — replication starts, stops, and reports status correctly through the arangosync coordination protocol
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation and ABI Baseline | 1/3 | In Progress|  |
| 2. Security Foundations | 0/TBD | Not started | - |
| 3. Graph and Cluster | 0/TBD | Not started | - |
| 4. Search and Backup Operations | 0/TBD | Not started | - |
| 5. DC-to-DC Replication | 0/TBD | Not started | - |
