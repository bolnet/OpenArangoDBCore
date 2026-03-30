# Pitfalls Research

**Domain:** C++ Enterprise module drop-in replacement for ArangoDB (ABI/link compatibility, crypto, cluster, replication)
**Researched:** 2026-03-29
**Confidence:** HIGH (ABI/ODR/vtable: official specs + community post-mortems), MEDIUM (RocksDB encryption/backup: official docs + real-world implementations), MEDIUM (LDAP/cluster/DC2DC: official docs + ArangoDB issue tracker)

---

## Critical Pitfalls

### Pitfall 1: Header Symbol Mismatch — Wrong Namespace or Class Name

**What goes wrong:**
ArangoDB's `#ifdef USE_ENTERPRISE` blocks include Enterprise headers by path and expect specific class names, function signatures, and namespaces. If OpenArangoDBCore declares its types in `namespace openarangodb` instead of matching ArangoDB's expected namespace (e.g., `arangodb` or a sub-namespace like `arangodb::enterprise`), the include will succeed but the linker will fail with "undefined reference" for every Enterprise symbol. The compiler silently compiles each file; the error surfaces only at link time, after hours of compilation.

All current stubs use `namespace openarangodb`, which will not match ArangoDB's `#include` expectations.

**Why it happens:**
The stub files were created with a new namespace before reverse-engineering the exact names ArangoDB expects. Developers assume the directory structure alone is sufficient for the drop-in, but C++ uses both path and name for symbol resolution.

**How to avoid:**
1. Before writing any implementation, `grep -r "USE_ENTERPRISE" arangodb/` to find every `#ifdef USE_ENTERPRISE` block in the ArangoDB source tree.
2. Extract the exact header path, class name, namespace, and virtual function signatures from each block.
3. Replicate the namespace hierarchy exactly — if ArangoDB includes `#include "Enterprise/Encryption/EncryptionProvider.h"` and expects `class EncryptionProvider : public rocksdb::EncryptionProvider` in `namespace arangodb`, that is what OpenArangoDBCore must provide.
4. Create a `symbols_expected.txt` artifact in the build that lists every required symbol and verify it at CI time using `nm` or `objdump`.

**Warning signs:**
- Any "undefined reference to `arangodb::`" link error where the referenced symbol does not contain `openarangodb`
- `grep -r "openarangodb" arangodb/` returns nothing (ArangoDB's source does not know about this namespace)
- Build succeeds with an empty stub but crashes at runtime — vtable or symbol was resolved to a no-op from a wrong translation unit

**Phase to address:** Phase 1 (Foundation) — before any implementation work begins. Every module's header namespace must be locked before code is written.

---

### Pitfall 2: Vtable ODR Violation — Incomplete Virtual Override

**What goes wrong:**
ArangoDB's Enterprise interfaces define abstract base classes with pure virtual methods (e.g., `rocksdb::EncryptionProvider`, `arangodb::ShardingStrategy`). If OpenArangoDBCore's concrete subclass does not override every pure virtual method, the class is abstract and cannot be instantiated — but this surfaces as a link error rather than a compile error when the construction happens in a different translation unit. Worse: if a virtual is overridden with a slightly different signature (e.g., missing `const`, wrong `noexcept`, wrong parameter type), the compiler silently defines a new, non-overriding virtual, leaving the base version as a pure virtual, causing a crash or link error.

**Why it happens:**
Stub files are written without the real base class in scope. Developers copy method signatures from documentation instead of from the actual ArangoDB header, introducing subtle signature mismatches. C++ does not warn when a `virtual` function in a derived class shadows but does not override a base virtual.

**How to avoid:**
1. Always use `override` on every virtual method in every Enterprise subclass. This makes the compiler reject signature mismatches as errors.
2. Use `= 0` markers in the base and `static_assert` that subclasses are not abstract: `static_assert(!std::is_abstract_v<MyConcreteProvider>)`.
3. Compile all Enterprise headers together with ArangoDB's headers from the start (even for stubs returning `{}`) to validate that vtable layouts match.
4. Use `objdump -C` or `llvm-nm --demangle` to inspect the vtable entries in compiled object files and compare against ArangoDB's enterprise `.a` symbols if available.

**Warning signs:**
- Compiler warning: "overriding virtual function has different type" (Clang) or "candidate function not viable" (GCC)
- Link error: "undefined reference to `vtable for ...`" — this always indicates a pure virtual remains unimplemented
- Runtime SIGABRT with "pure virtual method called" — vtable mismatch was not caught at compile time

**Phase to address:** Phase 1 (Foundation) and again at start of each module phase. Add `-Wall -Woverloaded-virtual` to CMake flags project-wide.

---

### Pitfall 3: ODR Violation — Duplicate Symbol Between OpenArangoDBCore and ArangoDB Community

**What goes wrong:**
ArangoDB Community already defines many utility classes, feature registries, and base infrastructure that OpenArangoDBCore references. If OpenArangoDBCore redefines any of these (e.g., provides its own version of a logging utility, a base Feature class, or a common struct that differs from Community's definition), the linker will silently pick one definition. In the best case this is a link error; in the worst case it is undefined behavior at runtime — memory layouts diverge and writes to one definition corrupt the other's data.

This is especially likely for: header-only utility templates, inline function definitions, and `constexpr` values that differ between Community and Enterprise compilation units.

**Why it happens:**
The ODR is a "shall" rule in the C++ standard with no required diagnostic for cross-translation-unit violations. Silent UB is the default outcome.

**How to avoid:**
1. Never define any type in OpenArangoDBCore that already exists in ArangoDB's core headers. Forward-declare and include instead.
2. Use `target_include_directories` to include the ArangoDB source root so OpenArangoDBCore always sees the canonical definitions.
3. Compile with `-Wl,--warn-common` on Linux to surface duplicate symbols in the link step.
4. Use `address-sanitizer` (ASan) with `detect_odr_violation=2` in CI to catch silent ODR violations at runtime.

**Warning signs:**
- Link warning: "multiple definition of" (non-fatal but indicates the problem)
- ASan output: `==ERROR: AddressSanitizer: odr-violation`
- Segfault in code that "couldn't possibly crash" — e.g., a simple feature flag access

**Phase to address:** Phase 1 (CMake integration) — configure ASan in CI from day one, not retroactively.

---

### Pitfall 4: RocksDB EncryptionProvider — IV Reuse Causing Catastrophic Key Stream Collision

**What goes wrong:**
AES-256-CTR encryption used in `arangod/Encryption/EncryptionProvider.cpp` requires that the (key, IV) pair is unique for every encrypted file. RocksDB prepends a 4KB header block containing the randomly generated IV to each SST file. If the IV generation is not truly random (e.g., seeded with time, reuses a counter, or copies the same IV across files), two files encrypted with the same key share a key stream. XOR-ing the two ciphertexts immediately reveals the XOR of the plaintexts — every byte of both database files is exposed without the key.

**Why it happens:**
Developers use `rand()` or `std::rand()` for quick IV generation during stub implementation and forget to replace it with `RAND_bytes()` (OpenSSL CSPRNG). Or the IV buffer is zero-initialized and only partially filled, leaving a deterministic suffix.

**How to avoid:**
1. Use `RAND_bytes(iv, 16)` (OpenSSL) for every new IV. Never use `rand()`, `random()`, or time-seeded generators.
2. Assert `RAND_bytes` return value == 1 (failure means OpenSSL entropy pool is depleted — abort rather than continue).
3. Store IV in the per-file 4KB header block as RocksDB's `EncryptedEnv` protocol requires (first `kHeaderSize` bytes of each file).
4. Write a property-based test: create 1000 file headers and verify all IVs are distinct.
5. Log IV generation as DEBUG audit events so testing can confirm uniqueness.

**Warning signs:**
- Two database SST files share identical first 16 bytes in their headers
- `RAND_bytes` failures are silently ignored (check return value handling in code review)
- IV is constructed from `file_id % N` or similar counter-based scheme

**Phase to address:** Phase 2 (Encryption at Rest) — before any data is written through the encryption layer. Treat IV generation as a security-critical code path requiring explicit review.

---

### Pitfall 5: RocksDB Hot Backup — Global Write Lock Deadlock

**What goes wrong:**
ArangoDB's Hot Backup requires acquiring a global write lock across all DB servers in the cluster before calling `rocksdb::Checkpoint::Create()`. If any long-running transaction holds a shard lock when the backup coordinator broadcasts the lock request, the coordinator waits. If the long-running transaction also waits for a resource that the backup lock would eventually release, the system deadlocks and the backup times out. In cluster mode with high write throughput, this can make hot backup effectively unusable.

**Why it happens:**
Implementations naively call `rocksdb::Checkpoint::Create()` without first flushing the WAL and without coordinating with ArangoDB's transaction manager to drain in-flight transactions. The RocksDB checkpoint API itself is safe, but ArangoDB's logical consistency layer requires additional coordination.

**How to avoid:**
1. Implement the backup sequence in strict order: (a) broadcast "stop accepting new writes" to all DB servers, (b) wait for in-flight transactions to complete up to `timeout` seconds, (c) flush WAL (`db->FlushWAL(true)`), (d) create checkpoint with hard links, (e) re-enable writes.
2. Respect ArangoDB's `allowInconsistent` flag — if the timeout expires and `allowInconsistent=false`, return HTTP 408 rather than creating a potentially inconsistent backup.
3. Never acquire per-shard locks one by one; acquire all locks atomically to prevent partial-lock deadlocks.
4. Test backup under sustained write load (not just quiescent) — deadlocks only appear under contention.

**Warning signs:**
- Hot backup requests consistently time out at exactly the `timeout` value under load
- `rocksdb::DB::GetProperty("rocksdb.num-running-flushes")` always returns non-zero during backup
- Backup succeeds in tests (low load) but fails in staging (real load)

**Phase to address:** Phase 5 (Backup & Ops) — the global lock protocol must be designed before implementation, not discovered during testing.

---

### Pitfall 6: SmartGraph Sharding — Immutable smartGraphAttribute After Creation

**What goes wrong:**
SmartGraph sharding partitions vertices by the `smartGraphAttribute` value embedded in the `_key` field (format: `{smartValue}:{localKey}`). If the implementation allows the `smartGraphAttribute` to be changed after collection creation, or if it does not enforce this format on key insertion, vertex documents will be silently assigned to the wrong shard. Cross-shard graph traversals will then produce wrong results without any error — edges will reference vertices that appear to not exist on the expected shard.

**Why it happens:**
The sharding key format is ArangoDB-specific and not self-evident from the SmartGraph API. Developers implement the general sharding strategy but miss the `_key` format requirement, or they allow schema modification because the base class's `canModifyShardingAttribute()` returns true by default.

**How to avoid:**
1. Override `canModifyShardingAttribute()` to return `false` for all SmartGraph collections — enforce immutability at the strategy level.
2. In key insertion validation, parse the `_key` for the `:` separator and verify the prefix matches the document's `smartGraphAttribute` value before writing.
3. Write a test: create a SmartGraph with `smartGraphAttribute: "region"`, insert a vertex with `region: "eu"`, and assert the `_key` starts with `eu:`.
4. Test the disjoint variant separately — disjoint SmartGraphs prohibit edges between different `smartGraphAttribute` partitions and require separate validation logic.

**Warning signs:**
- Traversal queries return empty results for edges that visibly exist in the collection
- `_key` values in SmartGraph collections don't follow the `{prefix}:{localKey}` pattern
- SmartGraph collections accept key modification without error

**Phase to address:** Phase 3 (Graph & Cluster) — validate the key format invariant in unit tests before any traversal logic is written.

---

### Pitfall 7: LDAP Authentication — Shared LDAP* Handle Across Threads

**What goes wrong:**
`libldap` is thread-safe for separate `LDAP*` handles (one per thread), but sharing a single `LDAP*` handle between threads without external locking corrupts the handle's internal state. In an ArangoDB authentication handler, multiple request threads can simultaneously attempt LDAP authentication. If they share one connection handle, `ldap_simple_bind_s()` calls from two threads will interleave, causing authentication to return success for the wrong credentials or SIGSEGV inside libldap.

**Why it happens:**
Connection pooling is the natural optimization. Developers create one `LDAP*` at startup and reuse it, correctly identifying this as a "connection pool of size 1." They do not realize that libldap's thread safety guarantee only applies to handles that are not shared, not to handles protected by an external mutex (which blocks the whole authentication thread).

**How to avoid:**
1. Use per-thread `LDAP*` handles, or implement a connection pool where each `LDAP*` is checked out exclusively per authentication request and returned after completion.
2. If reusing connections across requests is required for performance, use `ldap_dup()` to create a copy of the handle for concurrent operations (requires linking against `libldap_r`, the thread-safe variant).
3. Set `ldap_set_option(NULL, LDAP_OPT_THREAD_SAFE, 1)` at startup to enable global thread safety in libldap.
4. Test concurrent authentication: fire 50 simultaneous auth requests and verify all return correct results.

**Warning signs:**
- Intermittent authentication failures under concurrent load (passes in sequential tests)
- SIGSEGV or SIGABRT inside libldap call stack during load testing
- `ldap_result()` returns unexpected error codes when called from multiple threads

**Phase to address:** Phase 2 (Security — Auth) — thread safety design must be part of the initial architecture, not a later fix.

---

### Pitfall 8: CMake Link Order — Static Library Symbol Starvation

**What goes wrong:**
The `openarangodb_enterprise` static library is linked into ArangoDB's main binary. On Linux with GNU ld, the linker processes static libraries exactly once, in order. If ArangoDB's `CMakeLists.txt` lists `openarangodb_enterprise` before the object files that reference its symbols, those symbols are "not yet needed" when the library is scanned and are discarded. The result is "undefined reference" errors for Enterprise symbols, even though they are correctly compiled and present in the `.a` file.

**Why it happens:**
CMake's `target_link_libraries` with PRIVATE linking conceals dependency direction from the final link command. When ArangoDB's top-level `CMakeLists.txt` links `openarangodb_enterprise`, the order in the generated Makefile or Ninja file may not place it after all referencing object files.

**How to avoid:**
1. Use CMake's `target_link_libraries(arangod PRIVATE openarangodb_enterprise)` — CMake handles dependency ordering for targets it knows about.
2. For GNU ld, if link order cannot be guaranteed, wrap the library: `-Wl,--whole-archive openarangodb_enterprise.a -Wl,--no-whole-archive`. This forces all symbols to be included regardless of reference order. Use sparingly — it increases binary size.
3. Add a CI link test: a minimal `main.cpp` that calls one function from each Enterprise module, to catch symbol starvation early.
4. On macOS, use `-force_load` equivalent: `target_link_options(arangod PRIVATE -Wl,-force_load,$<TARGET_FILE:openarangodb_enterprise>)`.

**Warning signs:**
- "Undefined reference" errors that disappear when library order is changed in the link command
- Symbols are visible in `nm -g openarangodb_enterprise.a` but absent from `nm -g arangod`
- Build succeeds on macOS (ld64 is more lenient) but fails on Linux

**Phase to address:** Phase 1 (CMake integration) — write the minimal link test before any real implementation.

---

### Pitfall 9: AES-256-CTR Counter Overflow on Large Files

**What goes wrong:**
In CTR mode, the 128-bit counter block is incremented for each 16-byte AES block. For a file larger than 2^128 × 16 bytes, the counter wraps around and repeats. While this threshold (≈ 5.2 × 10^39 bytes) is not reachable in practice, a related issue is more dangerous: if the IV is stored as a 16-byte nonce with no dedicated counter field, and the EncryptionProvider implementation uses the nonce as the initial counter value without enforcing that different file offsets produce unique counter values, seeking to an offset and re-encrypting the same block produces the same keystream — exposing both plaintexts.

More practically: `EVP_EncryptUpdate` with OpenSSL maintains an internal counter. If the `EVP_CIPHER_CTX` is reused across seek operations without being reset to the correct counter state for the file offset, decryption produces garbage for non-sequential reads. RocksDB reads SST files non-sequentially.

**Why it happens:**
Developers test encryption with sequential writes and sequential reads only. RocksDB's block cache reads at arbitrary file offsets. The encryption layer must compute the correct counter value for any byte offset, not just for sequential access.

**How to avoid:**
1. For each read/write at file offset `N`, compute the counter as: `initial_counter + (N / 16)` and set the counter explicitly before each `EVP_EncryptUpdate` / `EVP_DecryptUpdate` call.
2. Use `EVP_EncryptInit_ex` to re-initialize the context with the computed counter for every seek, rather than maintaining a stateful context across seeks.
3. Test random-access decryption: write a 10MB file, then read and verify 100 random 4KB blocks using the same EncryptionProvider instance.

**Warning signs:**
- Decrypted data is correct at file start but corrupt at large offsets
- Sequential reads pass tests; random-access reads fail
- `EVP_CIPHER_CTX` is stored as a member variable and reused across `Read()` calls

**Phase to address:** Phase 2 (Encryption at Rest) — write the random-access test before marking encryption complete.

---

### Pitfall 10: DC2DC Replication — Asynchronous Delivery Without Idempotency Guards

**What goes wrong:**
ArangoDB's DC2DC replication (ArangoSync) is explicitly asynchronous — writes acknowledged in the source datacenter may not yet be present in the destination. The `DC2DCReplicator` implementation must handle the scenario where a replication message is delivered more than once (network retry) or out of order (reconnection after partition). Without idempotency guards (e.g., sequence numbers, version vectors, or operation IDs), duplicate application of a write operation corrupts the destination dataset — a document insertion applied twice creates a conflict, and a deletion applied twice after re-insertion removes data that should exist.

**Why it happens:**
Initial implementations focus on "happy path" — write arrives once, in order. Network retry logic and reconnection handling are added later, by which point idempotency was not designed in and retrofitting requires changing the protocol format.

**How to avoid:**
1. From day one, include a monotonic sequence number (per-database, per-collection) in every replication message.
2. At the destination, track the highest applied sequence number and reject (with ACK) any message with a sequence number less than or equal to the last applied.
3. For the DirectMQ message queue protocol, model messages as `(source_shard_id, sequence_number, operation, payload)` tuples.
4. Test: replay the same 100-message log twice against a fresh destination and verify the destination state is identical to applying it once.

**Warning signs:**
- Document counts drift between source and destination over time
- Replication catches up after reconnection but data is corrupt
- "Duplicate key" errors appear in destination cluster logs during reconnection

**Phase to address:** Phase 4 (Replication) — idempotency is a protocol-level concern; it cannot be added to a running replication stream without a version bump.

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Stub all 19 modules with empty implementations | ArangoDB compiles against OpenArangoDBCore immediately | Stubs silently replace Enterprise behavior with no-ops; security features appear present but do nothing | Only acceptable as a verified compile-test scaffold; stubs must be gated behind a feature flag or compile-time assert |
| Copy Enterprise header signatures from documentation rather than ArangoDB source | Faster to research | Documentation lags the source; signature mismatch is a silent ODR or vtable bug | Never — always read from ArangoDB source directly |
| Use `rand()` for IV/nonce generation to unblock encryption tests | Tests pass immediately | Deterministic IVs destroy all encryption security guarantees | Never for production code; only acceptable in unit test harnesses that mock the RNG |
| Single global LDAP connection without pooling | Simplest to implement | SIGSEGV or wrong-user auth under concurrent load | Never — design for concurrent auth from the start |
| `--whole-archive` link flag to avoid link order debugging | Immediately fixes undefined symbols | Bloats binary by including all object code from all translation units, including unused stubs | Acceptable as a temporary CI fix while proper CMake dependency order is established |
| Hardcode `smartGraphAttribute = "_key"` in sharding strategy | Works for single-server simulation | Breaks all multi-shard SmartGraph traversals; wrong shard routing silently returns empty results | Never |
| Skip WAL flush before RocksDB checkpoint in hot backup | Faster backup initiation | Produces logically inconsistent backup — crash recovery required even for "clean" backups | Never |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| RocksDB `EncryptedEnv` | Calling `NewEncryptedEnv()` with a custom EncryptionProvider but not registering it in `rocksdb::ObjectRegistry` — provider is ignored at DB open time | Register via `rocksdb::ObjectRegistry::NewInstance()->AddManagedObject()` before opening the DB; verify with `rocksdb::GetEncryptionProvider()` |
| OpenSSL EVP AES-CTR | Calling `EVP_EncryptInit_ex` once at provider creation and reusing `EVP_CIPHER_CTX` across file operations | Create a fresh `EVP_CIPHER_CTX` per file, or explicitly re-initialize with the correct counter for each seek offset |
| libldap TLS (LDAPS) | Calling `ldap_start_tls_s()` without setting `LDAP_OPT_X_TLS_REQUIRE_CERT` — silently accepts any certificate including expired/self-signed | Set `ldap_set_option(NULL, LDAP_OPT_X_TLS_REQUIRE_CERT, &val)` where `val = LDAP_OPT_X_TLS_DEMAND` before any connection |
| ArangoDB Feature registration | Implementing an `arangodb::application_features::ApplicationFeature` subclass but not adding it to the correct feature dependency graph — feature starts in wrong order or not at all | Mirror the dependency list from ArangoDB's Enterprise feature registration in `arangod/RestServer/ArangodServer.cpp` |
| RocksDB parallel index building | Using `rocksdb::DB::CreateColumnFamily()` and building indexes concurrently across column families without coordinating flushes — compaction interferes with in-progress index builds | Use `rocksdb::Options::max_background_jobs` to bound concurrency; hold a `rocksdb::SstFileWriter` open per index build and finalize atomically |
| DC2DC DirectMQ | Opening a raw TCP socket for replication without TLS and assuming network security — plaintext replication stream includes full document payloads | All DC2DC connections must use mTLS from the first handshake; generate per-deployment certs at setup time |
| AQL MinHash functions | Registering AQL function under a name that conflicts with a Community AQL function — silently shadows the Community implementation | Query ArangoDB's `arangodb::aql::Functions::getFunctionList()` at build time and assert no name collisions |
| RClone subprocess | Spawning `rclone` as a child process without a strict timeout or stdin/stdout pipe drain — `rclone` blocks on credential prompt if auth is misconfigured, hanging the backup operation indefinitely | Use `boost::process` (or equivalent) with a strict timeout, close stdin, and read stdout/stderr in a separate thread |

---

## Performance Traps

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Synchronous audit log writes on every request | API latency spikes on high write workload; `p99` latency becomes I/O bound | Use a lock-free ring buffer (one per audit topic) drained by a dedicated writer thread; never block the request thread | Above ~500 writes/second on rotating disk |
| Per-request LDAP bind (new connection per authentication) | Auth latency becomes the bottleneck for all API requests | Maintain a connection pool with keep-alive; re-bind only on connection failure | Above ~100 concurrent auth requests |
| RocksDB encryption: per-block `EVP_CIPHER_CTX` allocation | High CPU overhead from EVP context initialization on every 4KB block | Allocate `EVP_CIPHER_CTX` once per file handle, not per block; use `EVP_CIPHER_CTX_reset()` to reuse | Visible above 50 MB/s write throughput |
| SmartGraph shard assignment scan on every vertex insert | Insert throughput collapses as vertex count grows | Cache the shard mapping; the `smartGraphAttribute`-to-shard mapping is deterministic and needs no DB round-trip | Above ~100K vertex insertions/second |
| MinHash: computing all `k` hash functions sequentially | MinHash computation is `O(k * |set|)` — very slow for large sets | Use the linear hash family `h(x) = (a*x + b) mod p` where `a` and `b` are precomputed per function; enables SIMD vectorization | Above signature size `k=128` on sets > 10K elements |
| Hot backup: holding RocksDB checkpoint open indefinitely | Storage grows unbounded as RocksDB cannot compact old SST files | Implement a TTL-based backup lifecycle manager; delete old backups when new ones are verified | After 3-5 hot backups without cleanup |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| IV/nonce reuse in AES-256-CTR | Two ciphertexts encrypted with same (key, IV) reveal XOR of plaintexts — full plaintext recovery without the key | Always use `RAND_bytes(iv, 16)` per file; assert return value == 1; add property-based test for IV uniqueness across 10K file headers |
| LDAP bind credentials stored in plaintext configuration | Any user with file read access to `arangod.conf` recovers LDAP service account password | Store LDAP bind password in ArangoDB's secret store or as an environment variable; never in the config file |
| Accepting self-signed TLS certificates in LDAPS | LDAP credential traffic including user passwords is exposed to MITM | Set `LDAP_OPT_X_TLS_REQUIRE_CERT = LDAP_OPT_X_TLS_DEMAND`; provide the CA bundle path via `LDAP_OPT_X_TLS_CACERTFILE` |
| DC2DC replication over plaintext TCP | Full database replication stream (all document data) transmitted in cleartext across datacenter network | Require mTLS for all DC2DC connections; reject connections without a valid client certificate |
| Data masking: reversible masking applied to fields marked "anonymize" | "Anonymized" data can be de-anonymized by anyone with the masking key — violates GDPR irreversibility requirements | Distinguish between `MASK` (reversible, format-preserving) and `ANONYMIZE` (irreversible, hash/null) in the masking strategy; enforce in the `AttributeMasking` policy loader |
| Hot backup: including encryption key material in backup archive | Backup restored on a different machine exposes key and all encrypted data | Backup archives must NOT include the master encryption key; key must be managed separately (key rotation documented) |
| Audit log: logging full document payloads including PII | Audit log becomes a secondary exfiltration vector for PII that is masked in the main DB | Define per-topic payload scrubbing rules; for write operations, log only document `_key` and operation type, not field values |

---

## "Looks Done But Isn't" Checklist

- [ ] **Encryption at Rest:** Compiles and ArangoDB accepts it — verify that files written to disk are actually encrypted by reading raw bytes with `xxd` and confirming they are not plaintext JSON. The no-op stub produces plaintext "encrypted" files.
- [ ] **LDAP Authentication:** Returns success for valid credentials in a test — verify it also rejects invalid credentials, handles LDAP server timeout, and fails closed (denies access) rather than open on error.
- [ ] **SmartGraph Sharding:** Graph traversal works on single-server test — verify shard assignment produces the same result as ArangoDB's native Enterprise edition by comparing `_key` formats on the same dataset.
- [ ] **Hot Backup:** `Checkpoint::Create()` completes — verify the backup is actually consistent by starting ArangoDB from the backup directory and running a read query, not just checking that the checkpoint directory exists.
- [ ] **RBAC (External):** Role assignment compiles — verify that every AQL query actually enforces the roles by testing a restricted user performing a forbidden operation.
- [ ] **Audit Logging (8 topics):** One topic logs correctly — verify all 8 topics (authentication, authorization, document-create, document-read, document-update, document-delete, collection-drop, server-config-change) fire their audit events under the correct trigger conditions.
- [ ] **DC2DC Replication:** Data replicates during normal operation — verify replication resumes correctly after a simulated network partition (disconnect for 10 seconds, reconnect, verify no data loss and no duplicates).
- [ ] **ReadFromFollower:** Reads succeed from follower — verify that reads from the follower are eventually consistent (not always returning stale data from before the last write) and that the follower correctly refuses reads it cannot serve.
- [ ] **MinHash Functions:** Similarity scores are computed — verify that the Jaccard similarity estimate produced by MinHash is within the expected error bound (±0.05 for k=128) against the ground-truth Jaccard similarity on test sets.
- [ ] **License Feature:** Always returns "enabled" — verify that every Enterprise feature guard that checks the license (`isEnterprise()`, `hasFeature()`) returns true, and that the LicenseFeature registers itself before any Enterprise features attempt their license check.

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Header namespace mismatch discovered after 5 modules implemented | HIGH | Rename all `namespace openarangodb` to the correct `arangodb` sub-namespace; update all forward declarations; rebuild from scratch to verify no remaining references |
| Vtable mismatch causing runtime crash | MEDIUM | Add `-Wall -Woverloaded-virtual` compile flag to surface all override mismatches; fix each one; add `override` keyword project-wide using `clang-tidy --fix-errors` with `cppcoreguidelines-explicit-virtual-functions` check |
| IV reuse discovered after encryption shipped | VERY HIGH | All data encrypted with the affected provider is compromised; must re-encrypt entire database with a new key and corrected provider; requires coordinated key rotation and downtime |
| LDAP shared handle SIGSEGV in production | HIGH | Hotfix: add a global mutex around all `ldap_*` calls as a stopgap; then redesign with per-request handles and remove the mutex |
| CMake link order breaks on new platform | LOW | Add `--whole-archive` wrapper as a CI workaround; then fix by explicitly expressing `target_link_libraries` dependencies in CMake |
| Hot backup deadlock under load | MEDIUM | Implement exponential backoff retry with `allowInconsistent=true` as a fallback; then fix the lock acquisition order to drain transactions before acquiring backup lock |
| ODR violation causing silent data corruption | HIGH | Binary search with ASan + `detect_odr_violation=2` to identify the offending translation unit; remove the duplicate definition from OpenArangoDBCore; verify with sanitizers |
| DC2DC replication producing duplicates after reconnect | MEDIUM | Replay destination log to find and remove duplicates using document `_rev` field as deduplication key; then add sequence-number idempotency to the protocol |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Header symbol/namespace mismatch | Phase 1: Foundation & CMake | `grep -r "openarangodb::" arangodb/` returns nothing; link test binary succeeds |
| Vtable ODR — incomplete virtual override | Phase 1: Foundation; repeated at each module start | All subclasses compile with `-Werror=overloaded-virtual`; `static_assert(!std::is_abstract_v<T>)` passes |
| ODR violation — duplicate symbol | Phase 1: CMake integration | ASan CI with `detect_odr_violation=2` passes; `nm -C` shows no duplicate symbols |
| IV reuse in AES-256-CTR | Phase 2: Encryption at Rest | Property-based test: 10K generated headers have unique IVs; raw SST bytes are not plaintext |
| Hot backup global write lock deadlock | Phase 5: Backup & Ops | Load test: backup under 1000 writes/second completes within timeout |
| SmartGraph immutable sharding attribute | Phase 3: Graph & Cluster | Unit test: `_key` format validation; attempt to modify `smartGraphAttribute` returns error |
| LDAP shared handle thread safety | Phase 2: Security — Auth | Concurrent auth test: 50 simultaneous requests all return correct results |
| CMake static library link order | Phase 1: CMake integration | Minimal link test (one symbol per module) passes on Linux, macOS, and CI |
| AES-CTR counter offset for random access | Phase 2: Encryption at Rest | Random-access decryption test: 100 random 4KB reads from a 10MB encrypted file all decrypt correctly |
| DC2DC idempotency | Phase 4: Replication | Replay test: applying the same 100-message log twice produces identical destination state |
| Audit log blocking request thread | Phase 2: Security — Audit | Benchmark: audit logging adds < 1ms p99 overhead per request at 1000 req/s |
| LDAP certificate validation | Phase 2: Security — Auth | Integration test with self-signed cert: connection is rejected |

---

## Sources

- ABI/ODR/vtable pitfalls: [Comprehensive C++ ABI Guide (GitHub Gist)](https://gist.github.com/MangaD/506a0f3273724ef3af26b8c085accdcb), [KDE Binary Compatibility Issues with C++](https://community.kde.org/Policies/Binary_Compatibility_Issues_With_C++), [C++ ABI Breaking Change — Lei Mao](https://leimao.github.io/blog/CPP-ABI-Breaking-Change/), [LLD Missing Key Function](https://lld.llvm.org/missingkeyfunction.html)
- ODR violations: [SEI CERT DCL60-CPP](https://wiki.sei.cmu.edu/confluence/display/cplusplus/DCL60-CPP.+Obey+the+one-definition+rule), [Oops I Violated ODR Again](https://gieseanw.wordpress.com/2018/10/30/oops-i-violated-odr-again/)
- RocksDB EncryptionProvider: [RocksDB AES-CTR PR #7240](https://github.com/facebook/rocksdb/pull/7240), [RocksDB env_encryption.h](https://github.com/facebook/rocksdb/blob/master/include/rocksdb/env_encryption.h), [CockroachDB Encryption RFC](https://github.com/cockroachdb/cockroach/blob/master/docs/RFCS/20171220_encryption_at_rest.md), [Intel ippcp-plugin-rocksdb](https://github.com/intel/ippcp-plugin-rocksdb)
- AES-CTR OpenSSL pitfalls: [OpenSSL EVP AES-CTR Discussion #23637](https://github.com/openssl/openssl/discussions/23637), [OpenSSL EVP Wiki](https://wiki.openssl.org/index.php/EVP_Symmetric_Encryption_and_Decryption)
- ArangoDB Hot Backup: [Hot Backup Implementation Blog](https://arangodb.com/2019/10/arangodb-hot-backup-creating-consistent-cluster-wide-snapshots/), [ArangoDB Backup Documentation](https://www.arangodb.com/docs/stable/backup-restore.html)
- ArangoDB SmartGraphs: [SmartGraphs Documentation 3.13](https://docs.arangodb.com/3.13/graphs/smartgraphs/), [SmartGraph Management](https://docs.arangodb.com/3.11/graphs/smartgraphs/management/)
- LDAP thread safety: [libldap thread safety discussion](https://openldap.org/lists/openldap-software/200606/msg00241.html), [ldap_dup man page](https://man7.org/linux/man-pages/man3/ldap_dup.3.html), [OpenLDAP Threading Support (DeepWiki)](https://deepwiki.com/openldap/openldap/5.3-threading-support)
- CMake link order: [Library Order in Static Linking (Eli Bendersky)](https://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking), [CMake target_link_libraries docs](https://cmake.org/cmake/help/latest/command/target_link_libraries.html)
- DC2DC Replication: [ArangoSync Reliability Blog](https://arangodb.com/2021/08/arangosync-a-recipe-for-reliability/), [DC2DC Replication Docs](https://docs.arangodb.com/3.10/deploy/arangosync/), [DC2DC Troubleshooting](https://www.arangodb.com/docs/stable/troubleshooting-dc2-dc.html)
- MinHash correctness: [MinHash LSH at Scale (DEV Community)](https://dev.to/schiffer_kate_18420bf9766/my-battle-against-training-data-duplicates-implementing-minhash-lsh-at-scale-3nab), [MinHash Wikipedia](https://en.wikipedia.org/wiki/MinHash)
- RocksDB parallel index/threading: [RocksDB Iterator Wiki](https://github.com/facebook/rocksdb/wiki/Iterator), [Reducing Lock Contention in RocksDB](https://rocksdb.org/blog/2014/05/14/lock.html)

---
*Pitfalls research for: C++ Enterprise replacement module for ArangoDB (ABI compatibility, RocksDB encryption, cluster, replication, crypto)*
*Researched: 2026-03-29*
