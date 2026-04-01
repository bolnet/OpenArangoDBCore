# Phase 5 Research: DC-to-DC Replication

**Researched:** 2026-04-01
**Confidence:** HIGH (source inspection + architecture docs + Phase 2/3 dependency analysis)
**Scope:** Asynchronous cross-datacenter replication with mTLS, idempotent replay, arangosync integration

---

## 1. Current Stubs

- `Enterprise/Replication/DC2DCReplicator.h` — empty namespace stub
- `Enterprise/Replication/DC2DCReplicator.cpp` — empty namespace stub
- `Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h` — empty stub (cross-DC access bypass)
- `Enterprise/Transaction/IgnoreNoAccessMethods.h` — empty stub
- All registered in CMakeLists.txt

## 2. Architecture Overview

### Source-to-Target Topology

```
Source Datacenter              Target Datacenter
┌──────────────┐              ┌──────────────┐
│  ArangoDB    │   mTLS       │  ArangoDB    │
│  Cluster     │─────────────>│  Cluster     │
│  (writes)    │   DirectMQ   │  (apply)     │
└──────────────┘              └──────────────┘
       │                              │
  WAL tailing                  Sequence tracking
  (per-shard)                  (idempotent apply)
```

- **Unidirectional:** Source → Target only (no conflict resolution needed)
- **Asynchronous:** Writes acknowledged on source before propagation
- **Eventual consistency:** Target converges within configurable lag window
- **arangosync:** Separate Go-based coordinator (not C++ code in OpenArangoDBCore)

### Key Characteristics

| Requirement | Description |
|-------------|-------------|
| DC2DC-01 | Replication stream via WAL tailing + DirectMQ messaging |
| DC2DC-02 | Async with configurable lag tolerance (1-30s typical) |
| DC2DC-03 | All traffic over mTLS (Phase 2 SslServerFeatureEE) |
| DC2DC-04 | Idempotent replay via per-shard monotonic sequence numbers |
| DC2DC-05 | Integration with arangosync coordination protocol |

---

## 3. Replication Protocol

### WAL Tailing

- RocksDB WAL contains all writes (insert, update, delete)
- DC2DCReplicator spawns per-shard tail threads
- Each thread polls RocksDB iterator for new entries
- Filters by shard key (uses ShardingStrategyEE from Phase 3)
- Skips satellite collections (SatelliteDistribution::isSatellite)
- Batches changes into DirectMQ messages (100-1000 docs per batch)

### Sequence Number Protocol

```
Message format: (shard_id: string, sequence: uint64, operation: enum, payload: VPack)

Source generates monotonic sequence per shard:
  shard_0: seq=1000, 1001, 1002, ...
  shard_1: seq=2000, 2001, 2002, ...

Target tracks last_applied_seq per shard:
  Reject any message with seq <= last_applied_seq (idempotent)
  Apply messages in order within each shard
  Cross-shard order doesn't matter
```

### Idempotency Guarantee

- Each message tagged with monotonic sequence number (per-shard)
- Target tracks highest applied sequence per shard
- Duplicate delivery safely rejected (seq <= last_applied)
- Uses document `_rev` for additional deduplication
- Network retry cannot cause duplicate inserts or phantom deletes

---

## 4. mTLS Transport

### Phase 2 Integration (Complete)

SslServerFeatureEE provides:
- `--ssl.require-client-cert` for mutual TLS verification
- `--ssl.min-tls-version` enforcement (TLS 1.2+)
- `--ssl.enterprise-cipher-suites` allowlist
- Client certificate path: `--ssl.keyfile` / `--ssl.cafile`

### DirectMQ Connection

- DC2DCReplicator opens outbound TLS connection to target coordinator
- Client cert verified against target's CA
- Server cert verified against source's CA
- All replication messages encrypted in transit
- Reject plaintext connections (Pitfall 9)

---

## 5. Cluster Coordination

### Phase 3 Dependencies (All Complete)

| Component | Role in DC2DC |
|-----------|---------------|
| ShardingStrategyEE | Shard routing for WAL entry filtering |
| SatelliteDistribution | Skip satellite collections (already replicated) |
| ReadFromFollower | Tail from any replica (reduce leader load) |

### Multi-Shard Coordination

- Source cluster has N shards per collection
- Each shard has independent sequence counter
- Per-shard tail threads run independently
- Target applies shards in any order (idempotent by design)

---

## 6. Transaction Handling

### IgnoreNoAccessAqlTransaction

- Used when replication applier applies changes with system privileges
- Bypasses read-only collection checks on target
- Ensures target reaches exact source state regardless of access control
- Required for cross-DC replication where target may have different permissions

### IgnoreNoAccessMethods

- Transaction method overrides for replication context
- Allows write operations on collections that would normally be read-only
- Scoped to replication applier only (not general user access)

---

## 7. Configuration Parameters

```
--replication.dc2dc.enabled              Boolean (default false)
--replication.dc2dc.target-cluster       URL (https://target:8529)
--replication.dc2dc.lag-tolerance        Max lag seconds (default 30)
--replication.dc2dc.batch-size           Docs per message (default 1000)
--replication.dc2dc.thread-count         Shard tail threads (default 4)
--replication.dc2dc.max-retries          Network retries (default 5)
--replication.dc2dc.checkpoint-interval  Checkpoint frequency seconds (default 60)

Inherited from Phase 2:
--ssl.keyfile, --ssl.cafile, --ssl.require-client-cert, --ssl.min-tls-version
```

---

## 8. REST API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/_admin/replication/status` | GET | Lag, shard status, message counts |
| `/_admin/replication/start` | POST | Start replication |
| `/_admin/replication/stop` | POST | Graceful stop |
| `/_admin/replication/reset` | POST | Reset sequence tracking (recovery) |
| `/_admin/replication/feed` | GET | Streaming feed for arangosync |
| `/_admin/replication/checkpoint` | POST | arangosync reports progress |

---

## 9. Plan Decomposition

| Plan | Scope | Est. Tests |
|------|-------|-----------|
| 05-01 | Sequence Numbers + Idempotency Protocol | ~25 |
| 05-02 | WAL Tailing + Message Generation | ~20 |
| 05-03 | DirectMQ Client + mTLS Transport | ~20 |
| 05-04 | REST API + arangosync Integration | ~25 |
| 05-05 | Transaction Bypass + Network Partition Tests | ~20 |

**Total estimated tests:** ~110

### Dependencies Between Plans

- 05-01 (Sequence Numbers) must come first — foundational protocol
- 05-02 (WAL Tailing) depends on 05-01 (uses sequence numbers)
- 05-03 (mTLS Transport) independent of 05-01/05-02
- 05-04 (REST API) depends on 05-01 and 05-02 (exposes their state)
- 05-05 (Transaction Bypass + Integration) depends on all prior plans
- Execution order: [05-01 || 05-03] → 05-02 → 05-04 → 05-05

---

## 10. Blockers & Risks

| Risk | Severity | Mitigation |
|------|----------|-----------|
| Idempotency protocol correctness | HIGH | Design sequence numbers first (05-01); replay tests in 05-05 |
| arangosync protocol undocumented | MEDIUM | Define REST contract; mock arangosync in tests |
| WAL iterator API varies by RocksDB version | LOW | Use stable rocksdb::TransactionLogIterator API |
| Network partition data loss | HIGH | Idempotent replay + checkpoint persistence |
| mTLS cert configuration complexity | LOW | Reuse Phase 2 SslServerFeatureEE (complete) |
