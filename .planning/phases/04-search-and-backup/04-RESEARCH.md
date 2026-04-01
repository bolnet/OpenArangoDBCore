# Phase 4 Research: Search and Backup Operations

**Researched:** 2026-03-31
**Confidence:** HIGH (direct source code inspection + architecture docs)
**Scope:** MinHash AQL Functions, TopK/WAND Optimization, Hot Backup, Cloud Backup, Parallel Index Building

---

## 1. MinHash AQL Functions

### Current Stubs

- `Enterprise/Aql/MinHashFunctions.h` — empty, TODO comment
- `Enterprise/Aql/MinHashFunctions.cpp` — empty, TODO comment
- `Enterprise/IResearch/IResearchAnalyzerFeature.h` — empty, for MinHash analyzer
- All registered in CMakeLists.txt (line 36)

### Required AQL Functions

| Function | Signature | Flags | Returns |
|----------|-----------|-------|---------|
| `MINHASH(set, numHashes)` | `".,."` | Deterministic, Cacheable, CanRunOnDBServerCluster | Array of k hash values |
| `MINHASH_MATCH(sig1, sig2, threshold)` | `".,.,."` | Deterministic, Cacheable, CanRunOnDBServerCluster | Boolean (similarity >= threshold) |

Registration pattern: `AqlFunctionFeature::addEnterpriseFunctions()` with `#ifdef USE_ENTERPRISE` guard.

### Algorithm

- Locality-sensitive hashing (LSH) for approximate Jaccard similarity
- k independent hash functions (k=128 default): `h(x) = (a*x + b) mod p`
- a, b are precomputed permutation seeds from a large prime field
- Error bound: ±0.05 for Jaccard estimate with k=128
- Hashing: xxHash 0.8.x (check `3rdParty/xxhash/`) or MurmurHash3

### IResearch Analyzer Integration

Separate from AQL functions — `minhash` analyzer in IResearch subsystem:
- Tokenizes field into a set
- Computes k min-hash signatures
- Indexes signatures for `MINHASH_MATCH` queries
- Registered via `IResearchAnalyzerFeature`

### Pitfalls

- Sequential hash computation on sets >10K elements is O(k*|set|) — use precomputed permutation matrices
- Must use `namespace arangodb`, verify against actual AqlFunctionFeature API
- Use RAND_bytes() for cryptographic PRNG if randomizing permutations

---

## 2. TopK/WAND Optimizer Rule

### Current Stubs

- `Enterprise/IResearch/IResearchOptimizeTopK.h` — empty, namespace declared only
- `Enterprise/IResearch/IResearchOptimizeTopK.cpp` — empty, TODO comment

### Optimizer Rule Interface

Rules register with AQL Optimizer Rule Registry (`arangod/Aql/OptimizerRules.cpp`):
- Callback receives `Optimizer` instance + `ExecutionNode` tree
- Rule traverses plan tree, detects patterns, rewrites nodes

### Pattern Detection

Detects: `EnumerateViewNode` → `SortNode` (BM25 desc) → `LimitNode` (k)

**Before optimization:**
```
EnumerateViewNode (all documents)
  └─> SortNode (BM25 descending)
      └─> LimitNode (k records)
```

**After optimization:**
```
EnumerateViewNode (same node, _enableWand=true, limit=k)
  └─> SortNode
      └─> LimitNode
```

No new node types created — existing `EnumerateViewNode` annotated with `_enableWand=true`.

### WAND Algorithm

At execution time, IResearch iterator implements Weak AND (WAND):
- Maintains running threshold = k-th best score seen
- Processes posting lists in score-descending order
- Skips documents whose max possible score < threshold
- Returns exact top-k results (not approximate)
- Exploits BM25 score bounds on index segments

### Dependencies on IResearch

- `3rdParty/iresearch/core/search/iterator.hpp` — Iterator base class
- `3rdParty/iresearch/core/search/wand.hpp` — WAND implementation (if exposed)
- Iterator accepts `wand_config` struct with threshold callback
- **Critical gap:** Exact IResearch WAND iterator interface requires `3rdParty/iresearch/` inspection

---

## 3. Hot Backup

### Current Stubs

- `Enterprise/StorageEngine/HotBackupFeature.h` — complete ApplicationFeature stub (all lifecycle no-ops)
- `Enterprise/RocksDBEngine/RocksDBHotBackup.h` — empty namespace stub
- `Enterprise/RocksDBEngine/RocksDBHotBackup.cpp` — empty, TODO
- `Enterprise/RestHandler/RestHotBackupHandler.h` — empty class stub

### RocksDB Checkpoint Mechanism

Uses RocksDB native Checkpoint API: `rocksdb::Checkpoint::Create()`
- WAL flush → hard-link directory creation → lock release
- Hard-links SST files (no copy of large files)
- Point-in-time consistent snapshot

### REST API Endpoints

| Endpoint | Method | Purpose |
|----------|--------|---------|
| `/_admin/backup/create` | POST | Create hot backup, returns backup ID + timestamp |
| `/_admin/backup/list` | GET | List available backups with metadata |
| `/_admin/backup/{id}` | DELETE | Remove backup directory |
| `/_admin/backup/restore/{id}` | POST | Restore from snapshot |

### Global Write Lock & Consistency

- Global write lock acquired before checkpoint for consistency
- Lock ordering critical in cluster mode (deadlock risk)
- Per-DB-Server checkpoint; Coordinator orchestrates multi-node backups
- Metadata: timestamp, ArangoDB version, collection list, backup ID

### CMake Integration

- `RocksDBHotBackup.cpp` in `openarangodb_enterprise` target (line 51)
- `RCloneFeature.cpp` in separate `arango_rclone` static library (line 94)
- Link: `target_link_libraries(arango_rocksdb PRIVATE arango_rclone)`

---

## 4. Cloud Backup (RClone)

### Current Stubs

- `Enterprise/RClone/RCloneFeature.h` — ApplicationFeature stub
- `Enterprise/RClone/RCloneFeature.cpp` — empty implementation

### Architecture

- Subprocess execution of rclone binary (not librclone C shim)
- Supports S3, Azure Blob Storage, Google Cloud Storage
- Configuration: endpoint, credentials, bucket/container path
- Credential injection via environment variables (not CLI args for security)

### ApplicationFeature Lifecycle

- `collectOptions()` — Register `--rclone-*` CLI arguments
- `validateOptions()` — Verify credentials, endpoint validity
- `prepare()` — Initialize rclone subprocess handle
- `start()` — Begin background upload thread
- `stop()`/`unprepare()` — Clean shutdown with timeout

---

## 5. Parallel Index Building

### Current Stubs

- `Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.h` — empty header
- `Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.cpp` — empty implementation

### Enterprise vs Community

| Aspect | Community | Enterprise |
|--------|-----------|-----------|
| Thread parallelism | Single-threaded | Multi-threaded (configurable, default 4) |
| Partitioning | N/A | Key space partitioned across threads, segments merged |
| Collection availability | Exclusive lock blocks all reads/writes | Background mode allows reads during build |
| Memory usage | Minimal | Higher (changelog buffer for concurrent writes) |

### Background Index Build Architecture

1. Create changelog buffer (ring buffer/vector of document operations)
2. Each thread scans a key-space partition at snapshot isolation
3. Concurrent writes append to changelog (thread-safe queue)
4. After all threads complete, merge segments and apply changelog
5. Atomic swap to publish index (no lock contention)

### RocksDB Integration

- Subclasses/wraps Community `RocksDBIndex::fillIndex()` for parallelism
- Per-thread RocksDB iterators at same snapshot version
- May use `rocksdb::SstFileWriter` for concurrent SST segment building
- Must coordinate with RocksDB background compaction via `max_background_jobs`

### Configuration Parameters

```
--rocksdb.index-builder-threads <int>       # Default: 4
--rocksdb.index-builder-memory-budget <mb>  # Default: 256
```

### Pitfalls

- Using `CreateColumnFamily()` + concurrent builds without flush coordination causes compaction interference
- Changelog must track deletes using document `_rev` field
- All threads must read at same MVCC snapshot version to avoid skew
- Final merge is single-threaded (bottleneck for very large collections)

---

## 6. Plan Decomposition

Based on the research, Phase 4 decomposes into 5 plans:

| Plan | Scope | Est. Tests |
|------|-------|-----------|
| 04-01 | MinHash AQL Functions + IResearch Analyzer | ~25 |
| 04-02 | TopK/WAND Optimizer Rule | ~20 |
| 04-03 | Hot Backup (RocksDB Checkpoint + REST API) | ~25 |
| 04-04 | Cloud Backup (RClone Integration) | ~15 |
| 04-05 | Parallel Index Building (RocksDBBuilderIndexEE) | ~20 |

**Total estimated tests:** ~105

### Dependencies Between Plans

- 04-01 (MinHash) and 04-02 (TopK) are independent
- 04-03 (Hot Backup) must complete before 04-04 (Cloud Backup) — RClone uploads backup dirs
- 04-05 (Parallel Index) is independent of all others
- Execution order: [04-01 || 04-02 || 04-05] → 04-03 → 04-04

---

## 7. Blockers & Gaps

| Gap | Severity | Mitigation |
|-----|----------|-----------|
| IResearch WAND iterator interface unknown | MEDIUM | Inspect `3rdParty/iresearch/` before 04-02 implementation |
| xxHash availability in bundled deps | LOW | Check `3rdParty/xxhash/`, fallback to MurmurHash3 |
| RocksDB Checkpoint API exact signatures | LOW | Available in public RocksDB headers |
| Optimizer rule registration interface | MEDIUM | Inspect `arangod/Aql/OptimizerRules.h` before 04-02 |
| rclone binary availability at runtime | LOW | Document as runtime dependency, test with mock subprocess |
