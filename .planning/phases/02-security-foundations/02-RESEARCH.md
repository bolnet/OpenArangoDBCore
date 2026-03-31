# Phase 2: Security Foundations - Research

**Researched:** 2026-03-31
**Domain:** C++ enterprise security modules -- RocksDB encryption at rest, audit logging, LDAP authentication, data masking, enhanced SSL/TLS
**Confidence:** HIGH (verified against ArangoDB source at `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/` and RocksDB official headers)

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| AUDIT-01 | AuditFeature registers as ApplicationFeature with correct lifecycle hooks | Confirmed: `arangod.cpp:207` calls `addFeature<AuditFeature>()`, `BasicFeaturePhaseServer.cpp` registers phase dependency. Current stub is already an ApplicationFeature subclass -- needs lifecycle implementation |
| AUDIT-02 | Audit events captured for all 8 topics (authentication, authorization, collection, database, document, view, service, hotbackup) | Confirmed: ArangoDB docs list exactly these 8 topics. Format: `timestamp | server | topic | username | database | client-ip | authentication | text...` |
| AUDIT-03 | Audit log output configurable (file, syslog) | Confirmed: `--audit.output` accepts `file://<path>` and `syslog://<facility>`, repeatable for multiple destinations |
| AUDIT-04 | Audit logging is non-blocking (does not degrade server performance) | Architecture pattern: async ring buffer with background writer thread. Must not block request path |
| ENCR-01 | EncryptionProvider implements RocksDB encryption interface (AES-256-CTR) | Confirmed: `rocksdb::EncryptionProvider` is abstract with methods `GetPrefixLength()`, `CreateNewPrefix()`, `AddCipher()`, `CreateCipherStream()`. ArangoDB uses `NewEncryptedEnv()` with shared_ptr to provider |
| ENCR-02 | Encryption key management supports keyfile and key rotation | Confirmed: `--rocksdb.encryption-keyfile` (32-byte key), `--rocksdb.encryption-keyfolder` for rotation secrets, `--rocksdb.encryption-key-rotation` to enable API-based rotation |
| ENCR-03 | IV generation uses cryptographically secure random (RAND_bytes) | Standard OpenSSL pattern for AES-256-CTR -- each file prefix stores the IV, generated via `RAND_bytes()` |
| ENCR-04 | Random-access read/write works correctly with counter-mode encryption | RocksDB `BlockAccessCipherStream` handles block-aligned encryption with `EncryptBlock()`/`DecryptBlock()` at specified block indices -- CTR mode natively supports random access |
| ENCR-05 | EncryptionFeature registers as ApplicationFeature and hooks into RocksDB engine startup | Confirmed: `arangod.cpp:211` calls `addFeature<EncryptionFeature>()`, `BasicFeaturePhaseServer` orders it. `RocksDBEngine::configureEnterpriseRocksDBOptions()` is the hook point |
| AUTH-01 | LDAPHandler authenticates users against external LDAP directory | ArangoDB supports "simple" (prefix+suffix DN construction) and "search" (admin bind + search) authentication modes. Entirely enterprise-only code |
| AUTH-02 | LDAP connections use per-request handles (thread-safe under concurrent load) | Must use per-thread or per-request LDAP* handles -- libldap handles are NOT thread-safe. Pool pattern with mutex or thread-local storage required |
| AUTH-03 | LDAP role mapping translates directory groups to ArangoDB roles | Two methods: "roles attribute" (read from user's LDAP attributes) and "roles search" (separate LDAP search for group membership). Maps to ArangoDB internal roles |
| AUTH-04 | External RBAC policy evaluation for fine-grained access control | LDAP-mapped roles are evaluated against ArangoDB's permission system. Roles must be pre-created in ArangoDB; LDAP provides the membership |
| AUTH-05 | LDAP connection supports TLS (LDAPS and StartTLS) | `--ldap.tls` for StartTLS on port 389, or `ldaps://` URL for port 636. Certificate verification via `--ldap.tls-cacert-file` |
| MASK-01 | AttributeMasking applies field-level masking rules to query results | Confirmed: `lib/Maskings/` provides base framework. `AttributeMasking::installMasking()` registers named strategies. Enterprise extends via `InstallMaskingsEE()` called in `arangodump.cpp` |
| MASK-02 | Masking rules configurable per collection and per user role | Base `Maskings` class supports `_collections` map with `CollectionSelection`. JSON config file passed via `--maskings` option to arangodump |
| MASK-03 | Built-in masking strategies (redact, hash, partial, randomize) | Base provides `randomString` and `random`. Enterprise adds: `xifyFront` (hash-based prefix replacement), email masking, and potentially others via `InstallMaskingsEE()` |
| SSL-01 | SslServerFeatureEE extends base SSL with enterprise TLS options | Confirmed: `SslServerFeatureEE` inherits `SslServerFeature`. Virtual methods available for override: `verifySslOptions()`, `createSslContexts()`, `dumpTLSData()`. `prepare()` and `unprepare()` are `final` |
| SSL-02 | Support for client certificate authentication (mTLS) | EE overrides `createSslContexts()` to configure `SSL_CTX_set_verify(SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT)` and load CA for client cert verification |
| SSL-03 | TLS version and cipher suite restrictions configurable | Base `SslServerOptions` already has `cipherList` and `sslProtocol`. EE adds enterprise-specific options: minimum TLS version enforcement, cipher suite allowlist/blocklist |
</phase_requirements>

---

## Summary

Phase 2 implements five security subsystems that are entirely contained in the Enterprise module. Each subsystem is independent of the others (audit does not depend on encryption, LDAP does not depend on SSL-EE), which allows parallel implementation across multiple plans. The subsystems connect to ArangoDB through well-defined integration points: ApplicationFeature lifecycle hooks for audit and encryption, the `rocksdb::EncryptionProvider` abstract class for encryption at rest, the `InstallMaskingsEE()` function for data masking, and virtual method overrides on `SslServerFeature` for enhanced TLS.

The most complex subsystem is Encryption at Rest, which requires implementing the full `rocksdb::EncryptionProvider` interface with AES-256-CTR block cipher, key management (keyfile + rotation), and integration with RocksDB's `NewEncryptedEnv()`. The LDAP subsystem is the riskiest because it depends on the external `libldap` library and requires thread-safe handle management. Audit logging is structurally simple but wide (8 topics, 2 output backends, non-blocking requirement). Data masking extends an existing base framework. SSL-EE is the smallest -- three virtual method overrides on an already-functional base class.

**Primary recommendation:** Split into 4-5 plans: (1) Encryption at Rest (EncryptionProvider + EncryptionFeature + RocksDBEngineEEData), (2) Audit Logging (AuditFeature with all 8 topics and file/syslog output), (3) LDAP Authentication (LDAPHandler + role mapping + TLS), (4) Data Masking (AttributeMaskingEE + enterprise strategies), (5) Enhanced SSL/TLS (SslServerFeatureEE overrides for mTLS and cipher restriction). Plans 1-5 have no cross-dependencies and can be developed in any order.

---

## Standard Stack

### Core (inherited from ArangoDB build)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| OpenSSL | 3.0+ | AES-256-CTR encryption, RAND_bytes, SSL/TLS context | ArangoDB depends on OpenSSL; used for both encryption at rest and TLS |
| libldap (OpenLDAP) | 2.6+ | LDAP client library | Standard LDAP client; ArangoDB links against it when enterprise LDAP is enabled |
| RocksDB | 7.2.x (bundled) | `rocksdb::EncryptionProvider`, `BlockAccessCipherStream`, `NewEncryptedEnv` | Encryption at rest hooks into RocksDB's encrypted environment layer |
| VelocyPack | bundled | Audit log structured data, masking value serialization | All ArangoDB data flows through VelocyPack |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| syslog (POSIX) | system | Audit log syslog output | When `--audit.output syslog://` is configured |
| GoogleTest + GMock | 1.17.0 | Unit tests for all security modules | All test files |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| OpenSSL EVP for AES | libsodium | OpenSSL is already linked; adding libsodium adds a dependency for no benefit |
| libldap | ldap++ (C++ wrapper) | Adds dependency; raw libldap is what ArangoDB's enterprise code uses |
| Custom async logging | spdlog | ArangoDB already has Logger infrastructure; use it for consistency |

---

## Architecture Patterns

### Phase 2 Module Structure

```
Enterprise/
├── Audit/
│   ├── AuditFeature.h          # ApplicationFeature subclass - lifecycle + options
│   ├── AuditFeature.cpp         # Implement all 8 topic handlers + output backends
│   ├── AuditLogger.h            # NEW: async ring-buffer logger (non-blocking)
│   └── AuditLogger.cpp          # NEW: background writer thread for file/syslog
├── Auth/
│   ├── LDAPHandler.h            # LDAP bind/search/role-mapping
│   ├── LDAPHandler.cpp          # libldap integration with per-request handles
│   ├── LDAPConfig.h             # NEW: typed config from --ldap.* options
│   └── LDAPConfig.cpp
├── Encryption/
│   ├── EncryptionFeature.h      # ApplicationFeature - key loading, option parsing
│   ├── EncryptionFeature.cpp    # Lifecycle: load keyfile in prepare(), hook RocksDB
│   ├── EncryptionProvider.h     # Application-level provider (wraps RocksDB provider)
│   └── EncryptionProvider.cpp
├── Maskings/
│   ├── AttributeMasking.h       # Enterprise masking strategies
│   ├── AttributeMasking.cpp
│   └── AttributeMaskingEE.h     # InstallMaskingsEE() - registers enterprise strategies
├── RocksDBEngine/
│   ├── EncryptionProvider.h     # rocksdb::EncryptionProvider subclass (AES-256-CTR)
│   ├── EncryptionProvider.cpp   # NEW: full implementation of RocksDB encryption
│   ├── RocksDBEncryptionUtils.h # Key generation, keyfile I/O, key rotation helpers
│   ├── RocksDBEncryptionUtils.cpp
│   ├── RocksDBEncryptionUtilsEE.h # Enterprise-specific encryption utils
│   └── RocksDBEngineEE.h       # UPDATE: add _encryptionProvider to RocksDBEngineEEData
└── Ssl/
    ├── SslServerFeatureEE.h     # Override verifySslOptions(), createSslContexts()
    └── SslServerFeatureEE.cpp   # mTLS, TLS version enforcement, cipher restrictions
```

### Pattern 1: RocksDB EncryptionProvider Implementation

**What:** Implement `rocksdb::EncryptionProvider` abstract class with AES-256-CTR.
**When to use:** ENCR-01 through ENCR-05.

```cpp
// Enterprise/RocksDBEngine/EncryptionProvider.h
// Source: RocksDB env_encryption.h (https://github.com/facebook/rocksdb/blob/main/include/rocksdb/env_encryption.h)
#pragma once
#include <rocksdb/env_encryption.h>
#include <openssl/evp.h>
#include <string>
#include <memory>

namespace arangodb {
namespace enterprise {

class AESCTRBlockCipher : public rocksdb::BlockCipher {
 public:
  AESCTRBlockCipher(std::string const& key, bool allowHWAcceleration);
  const char* Name() const override { return "AES-CTR-256"; }
  size_t BlockSize() override { return 16; }  // AES block size
  rocksdb::Status Encrypt(char* data) override;
  rocksdb::Status Decrypt(char* data) override;
 private:
  std::string _key;  // 32 bytes (256-bit)
  bool _allowHW;
};

class EncryptionProvider : public rocksdb::EncryptionProvider {
 public:
  EncryptionProvider(std::string const& key, bool allowHWAcceleration);
  const char* Name() const override { return "ArangoDB-AES-CTR"; }
  size_t GetPrefixLength() const override;
  rocksdb::Status CreateNewPrefix(const std::string& fname, char* prefix,
                                  size_t prefixLength) const override;
  rocksdb::Status AddCipher(const std::string& descriptor, const char* cipher,
                            size_t len, bool for_write) override;
  rocksdb::Status CreateCipherStream(
      const std::string& fname, const rocksdb::EnvOptions& options,
      rocksdb::Slice& prefix,
      std::unique_ptr<rocksdb::BlockAccessCipherStream>* result) override;
 private:
  std::shared_ptr<AESCTRBlockCipher> _cipher;
};

}  // namespace enterprise
}  // namespace arangodb
```

### Pattern 2: Non-Blocking Audit Logger

**What:** Async ring buffer that audit event producers write to without blocking; a background thread drains to file/syslog.
**When to use:** AUDIT-04.

```cpp
// Enterprise/Audit/AuditLogger.h
// Pattern: lock-free SPMC ring buffer with background drain thread
#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace arangodb {

struct AuditEvent {
  std::string timestamp;   // GMT formatted
  std::string server;      // --audit.hostname or system hostname
  std::string topic;       // one of 8 topic strings
  std::string username;    // authenticated user or "-"
  std::string database;    // database name
  std::string clientIp;    // source IP:port
  std::string authMethod;  // "http basic", "jwt", etc.
  std::string text;        // action-specific details
};

class AuditLogger {
 public:
  enum class OutputType { File, Syslog };

  void addOutput(OutputType type, std::string const& target);
  void log(AuditEvent event);  // non-blocking enqueue
  void start();
  void stop();

 private:
  void drainLoop();
  std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<AuditEvent> _buffer;
  std::atomic<bool> _running{false};
  std::thread _writerThread;
  // output backends
  std::vector<std::pair<OutputType, std::string>> _outputs;
};

}  // namespace arangodb
```

### Pattern 3: Thread-Safe LDAP Handler

**What:** Per-request LDAP handle acquisition with connection pooling.
**When to use:** AUTH-01, AUTH-02.

```cpp
// Enterprise/Auth/LDAPHandler.h
// CRITICAL: libldap LDAP* handles are NOT thread-safe.
// Each concurrent authentication must use its own handle.
#pragma once
#include <ldap.h>
#include <mutex>
#include <string>
#include <vector>

namespace arangodb {

struct LDAPConfig {
  std::string server;           // --ldap.server
  int port = 389;               // --ldap.port
  std::string basedn;           // --ldap.basedn
  std::string binddn;           // --ldap.binddn (for search mode)
  std::string bindpasswd;       // --ldap.bindpasswd
  std::string prefix;           // --ldap.prefix (for simple mode)
  std::string suffix;           // --ldap.suffix (for simple mode)
  bool useTLS = false;          // --ldap.tls (StartTLS)
  bool useLDAPS = false;        // ldaps:// scheme
  std::string tlsCACertFile;    // --ldap.tls-cacert-file
  std::string rolesAttribute;   // --ldap.roles-attribute-name
  std::string rolesSearch;      // --ldap.roles-search
};

class LDAPHandler {
 public:
  explicit LDAPHandler(LDAPConfig config);
  ~LDAPHandler();

  // Authenticate user, returns true + populates roles on success
  bool authenticate(std::string const& username,
                    std::string const& password,
                    std::vector<std::string>& outRoles);

 private:
  LDAP* createHandle();  // creates fresh LDAP* per request
  void destroyHandle(LDAP* handle);
  bool bindUser(LDAP* handle, std::string const& dn,
                std::string const& password);
  std::vector<std::string> fetchRoles(LDAP* handle,
                                      std::string const& userDn);
  LDAPConfig _config;
};

}  // namespace arangodb
```

### Pattern 4: SslServerFeatureEE Virtual Overrides

**What:** Override the three non-final virtual methods to add mTLS and cipher restrictions.
**When to use:** SSL-01, SSL-02, SSL-03.

```cpp
// Enterprise/Ssl/SslServerFeatureEE.h
// IMPORTANT: prepare() and unprepare() are `override final` in base class.
// Only these virtuals are available: verifySslOptions(), createSslContexts(), dumpTLSData()
class SslServerFeatureEE final : public SslServerFeature {
 public:
  static constexpr std::string_view name() noexcept { return "SslServer"; }
  explicit SslServerFeatureEE(application_features::ApplicationServer& server);

  // Override to add enterprise validation (minimum TLS version, required ciphers)
  void verifySslOptions() override;

  // Override to configure mTLS (client certificate verification)
  SslContextList createSslContexts() override;

  // Override to include enterprise TLS details in dump
  Result dumpTLSData(VPackBuilder& builder) const override;

  // Enterprise-specific options
  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;

 private:
  bool _requireClientCert = false;     // mTLS enforcement
  uint64_t _minTlsVersion = 0;        // minimum TLS version (e.g., TLS 1.2)
  std::string _allowedCipherSuites;    // enterprise cipher allowlist
};
```

### Pattern 5: Enterprise Masking Strategy Registration

**What:** Register enterprise-specific masking functions via `InstallMaskingsEE()`.
**When to use:** MASK-01, MASK-03.

```cpp
// Enterprise/Maskings/AttributeMaskingEE.h
// Called from arangodump.cpp after InstallMaskings()
#pragma once

namespace arangodb::maskings {

// Registers enterprise masking strategies:
//   - "xifyFront": replaces characters with 'x', appends hash
//   - "email": hashes email into AAAA.BBBB@CCCC.invalid format
//   - "creditCard": masks all but last 4 digits
//   - "phone": masks all but last 4 digits
void InstallMaskingsEE();

}  // namespace arangodb::maskings
```

### Anti-Patterns to Avoid

- **Sharing LDAP handles across threads:** libldap `LDAP*` handles are NOT thread-safe. Creating a single handle and sharing it with a mutex will cause deadlocks under concurrent authentication. Use per-request handles.
- **Blocking audit writes in the request path:** Writing to file/syslog in the request handler thread will add latency. Use an async queue with a background writer.
- **Using ECB mode for encryption:** AES-ECB leaks patterns. AES-256-CTR (counter mode) is mandatory for RocksDB encryption -- it supports random access and is semantically secure.
- **Storing encryption keys in plaintext config files:** Keys must be in a separate keyfile with restricted permissions, not embedded in `arangod.conf`.
- **Re-overriding `prepare()` or `unprepare()` in SslServerFeatureEE:** These are `override final` in `SslServerFeature`. Attempting to override them will cause a compile error.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| AES-256-CTR encryption | Custom AES implementation | OpenSSL `EVP_EncryptInit_ex()` with `EVP_aes_256_ctr()` | Side-channel attacks, constant-time requirements, hardware acceleration |
| Cryptographic random for IVs | `rand()` or `std::random_device` | `RAND_bytes()` from OpenSSL | Only OpenSSL RAND is cryptographically secure and FIPS-compliant |
| LDAP protocol handling | Raw TCP socket LDAP | `libldap` (`ldap_initialize()`, `ldap_simple_bind_s()`, `ldap_search_ext_s()`) | LDAP protocol is complex; libldap handles TLS upgrade, referrals, paging |
| Syslog output | Custom UDP/TCP syslog | POSIX `syslog()` / `openlog()` | System syslog handles buffering, rotation, facility codes |
| TLS context configuration | Raw `SSL_CTX_*` calls | Boost.Asio `ssl::context` (already used by ArangoDB) | ArangoDB uses `asio_ns::ssl::context`; must be compatible |
| Masking strategy framework | Custom plugin system | Extend existing `AttributeMasking::installMasking()` registry | Base framework already provides parse/match/apply pipeline |

**Key insight:** All cryptographic operations MUST use OpenSSL. Custom crypto is a security vulnerability. All LDAP operations MUST use libldap. The masking framework already exists in `lib/Maskings/` -- enterprise code only adds new strategies to the existing registry.

---

## Common Pitfalls

### Pitfall 1: RocksDBEngineEEData Struct Incomplete

**What goes wrong:** `RocksDBEngine::encryptionProvider()` accesses `_eeData._encryptionProvider` (line 489). If `RocksDBEngineEEData` does not have this member, compilation fails.
**Why it happens:** The current stub struct is empty. Phase 2 must add `rocksdb::EncryptionProvider* _encryptionProvider = nullptr;`.
**How to avoid:** First task in the encryption plan must update `Enterprise/RocksDBEngine/RocksDBEngineEE.h` to add the pointer member.
**Warning signs:** Linker errors referencing `_encryptionProvider` or `encryptionProvider()`.

### Pitfall 2: EncryptionProvider Shared Pointer vs Raw Pointer Mismatch

**What goes wrong:** `NewEncryptedEnv()` takes `std::shared_ptr<EncryptionProvider>`, but `RocksDBEngine::encryptionProvider()` returns a raw `rocksdb::EncryptionProvider*`.
**Why it happens:** RocksDB stores the provider as shared_ptr internally; the raw pointer accessor is for convenience. The `_eeData._encryptionProvider` field is a raw pointer into the shared_ptr's managed object.
**How to avoid:** Store the `shared_ptr` in `RocksDBEngineEEData` and expose the raw pointer via the accessor. Pattern: `std::shared_ptr<rocksdb::EncryptionProvider> _provider; rocksdb::EncryptionProvider* _encryptionProvider = nullptr;`
**Warning signs:** Use-after-free if shared_ptr is destroyed before raw pointer is dereferenced.

### Pitfall 3: LDAP Handle Lifetime and Thread Safety

**What goes wrong:** `libldap` `LDAP*` handles are not thread-safe. Sharing one handle across threads causes corruption, segfaults, or auth failures.
**Why it happens:** libldap maintains per-handle state (pending results, connection state) that is not protected by internal mutexes.
**How to avoid:** Create a fresh `LDAP*` handle per authentication request. `ldap_initialize()` + `ldap_simple_bind_s()` + operation + `ldap_unbind_ext()` per request. If performance is a concern, use a pool of pre-initialized handles protected by a mutex, but each handle is used by only one thread at a time.
**Warning signs:** Intermittent auth failures under concurrent load, SIGSEGV in `ldap_result()`.

### Pitfall 4: AES-CTR Nonce/IV Reuse

**What goes wrong:** Reusing the same IV+key combination for two different files leaks plaintext via XOR.
**Why it happens:** CTR mode encrypts a counter sequence; if two plaintexts are encrypted with the same counter start, XOR of ciphertexts = XOR of plaintexts.
**How to avoid:** Generate a fresh IV per file using `RAND_bytes()`. Store the IV in the file prefix (RocksDB `CreateNewPrefix()` method). Never reuse an IV.
**Warning signs:** `xxd` inspection showing repeated byte patterns across SST files.

### Pitfall 5: Audit Logger Losing Events on Shutdown

**What goes wrong:** If the server shuts down while audit events are in the ring buffer, compliance-critical events are lost.
**Why it happens:** Background thread is killed before draining remaining events.
**How to avoid:** In `AuditFeature::stop()` or `unprepare()`, flush the ring buffer synchronously before joining the writer thread. Set a reasonable drain timeout.
**Warning signs:** Missing audit entries for the last few operations before shutdown.

### Pitfall 6: SslServerFeatureEE collectOptions/validateOptions Override Chain

**What goes wrong:** `SslServerFeatureEE` overrides `collectOptions()` and `validateOptions()`, but forgets to call the base class versions, losing all base SSL options.
**Why it happens:** These are not `final` in the base class, so overriding is allowed but the base implementations register all standard SSL options.
**How to avoid:** Always call `SslServerFeature::collectOptions(opts)` and `SslServerFeature::validateOptions(opts)` as the first line of the EE overrides.
**Warning signs:** `--ssl.keyfile` and `--ssl.cafile` options disappear when enterprise is enabled.

---

## Code Examples

### RocksDB Encryption Integration Point

```cpp
// Source: arangod/RocksDBEngine/RocksDBEngine.h lines 477-493
// This is how ArangoDB calls into enterprise encryption:

// In RocksDBEngine (defined in enterprise code):
void RocksDBEngine::configureEnterpriseRocksDBOptions(
    rocksdb::DBOptions& options, bool createdEngineDir) {
  // Load keyfile, create EncryptionProvider, call NewEncryptedEnv
  auto provider = std::make_shared<enterprise::EncryptionProvider>(
      encryptionKey, _allowHWAcceleration);
  _eeData._provider = provider;
  _eeData._encryptionProvider = provider.get();

  auto* encEnv = rocksdb::NewEncryptedEnv(options.env, provider);
  options.env = encEnv;
}
```

### Audit Feature Integration Point

```cpp
// Source: arangod/RestServer/arangod.cpp line 207
// Source: arangod/FeaturePhases/BasicFeaturePhaseServer.cpp lines 79-80
// AuditFeature starts in BasicFeaturePhase:
//   if (server.hasFeature<AuditFeature>()) {
//     startsAfter<AuditFeature>();
//   }
//
// Options parsed in collectOptions(), output initialized in prepare(),
// background thread started in start(), flushed in stop().
```

### LDAP Per-Request Handle Pattern

```cpp
// Pattern for thread-safe LDAP authentication
bool LDAPHandler::authenticate(std::string const& username,
                               std::string const& password,
                               std::vector<std::string>& outRoles) {
  LDAP* handle = createHandle();  // ldap_initialize() per request
  if (!handle) return false;

  // Configure TLS if needed
  if (_config.useTLS) {
    ldap_start_tls_s(handle, nullptr, nullptr);
  }

  // Construct DN and bind
  std::string dn = _config.prefix + username + _config.suffix;
  int rc = ldap_simple_bind_s(handle, dn.c_str(), password.c_str());
  if (rc != LDAP_SUCCESS) {
    destroyHandle(handle);
    return false;
  }

  outRoles = fetchRoles(handle, dn);
  destroyHandle(handle);  // ldap_unbind_ext() per request
  return true;
}
```

### Enterprise Masking Registration

```cpp
// Source: client-tools/Dump/arangodump.cpp lines 66-68
// Enterprise maskings are installed AFTER base maskings:
//   maskings::InstallMaskings();      // base: randomString, random
//   maskings::InstallMaskingsEE();    // enterprise: xifyFront, email, etc.

// Enterprise/Maskings/AttributeMaskingEE.h implementation:
void InstallMaskingsEE() {
  AttributeMasking::installMasking("xifyFront", XifyFrontMask::create);
  AttributeMasking::installMasking("email", EmailMask::create);
  // additional enterprise strategies...
}
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `rocksdb::EncryptionProvider` raw ptr | `shared_ptr<EncryptionProvider>` to `NewEncryptedEnv()` | RocksDB 6.x+ | Must store shared_ptr, not just raw pointer |
| `rocksdb::EncryptionProvider` standalone | `rocksdb::EncryptionProvider` extends `Customizable` | RocksDB 7.x+ | `CreateFromString()` factory available but not required |
| LDAP `ldap_simple_bind()` | `ldap_simple_bind_s()` (synchronous) or `ldap_sasl_bind()` | OpenLDAP 2.4+ | `ldap_simple_bind()` deprecated; use `ldap_simple_bind_s()` or `ldap_sasl_bind_s()` |
| OpenSSL `EVP_EncryptInit()` | `EVP_EncryptInit_ex()` with `EVP_aes_256_ctr()` | OpenSSL 3.0 | Low-level cipher APIs deprecated; use EVP exclusively |
| TLS 1.0/1.1 support | TLS 1.2 minimum, TLS 1.3 preferred | Industry standard | Enterprise SSL should enforce TLS 1.2+ minimum by default |

**Deprecated/outdated:**
- `ldap_simple_bind()` (non-_s suffix): deprecated in OpenLDAP 2.4+, use `ldap_simple_bind_s()` or `ldap_sasl_bind_s()`
- OpenSSL `DES_*`, `BF_*` low-level APIs: use `EVP_*` exclusively in OpenSSL 3.0+
- RocksDB `ROT13Cipher`: test-only cipher, never use for production encryption

---

## Open Questions

1. **RocksDBEngineEEData exact fields needed**
   - What we know: `_encryptionProvider` (raw `rocksdb::EncryptionProvider*`) is accessed. `EncryptionSecret` is referenced in method signatures. A `shared_ptr` must keep the provider alive.
   - What is unclear: Exact field list for key rotation state, keystore path, etc. May need `std::string _encryptionKey`, `std::string _keystorePath`, `std::vector<EncryptionSecret> _secrets`.
   - Recommendation: Start with the minimum (`_encryptionProvider` pointer + owning `shared_ptr`), expand fields as the `configureEnterpriseRocksDBOptions()` / `prepareEnterprise()` / `collectEnterpriseOptions()` implementations reveal requirements.

2. **EncryptionSecret struct definition**
   - What we know: `RocksDBEngine` references `enterprise::EncryptionSecret` in method signatures for key rotation.
   - What is unclear: Exact struct layout (no public documentation, enterprise-only type).
   - Recommendation: Define as `struct EncryptionSecret { std::string key; std::string id; }` initially, refine during implementation.

3. **LDAP library availability on all platforms**
   - What we know: macOS has OpenLDAP client headers via Homebrew (`brew install openldap`). Linux has `libldap-dev`.
   - What is unclear: Whether the CI environment has libldap available.
   - Recommendation: Make LDAP support conditional (`#ifdef ARANGODB_HAVE_LDAP` or CMake `find_package(LDAP)`). If unavailable, LDAPHandler compiles as a stub that always returns authentication failure.

4. **Exact audit output format for hotbackup topic**
   - What we know: 7 of 8 topics are well documented. `audit-hotbackup` is mentioned but details are sparse.
   - What is unclear: Exact event text format for hotbackup create/restore operations.
   - Recommendation: Implement as `"hotbackup create '<id>'" | "ok"` and `"hotbackup restore '<id>'" | "ok"` following the pattern of other topics.

---

## Validation Architecture

> `workflow.nyquist_validation` not set in config.json -- treating as enabled.

### Test Framework

| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 + GMock (via FetchContent) |
| Config file | `tests/CMakeLists.txt` (exists from Phase 1) |
| Quick run command | `cd build && ctest --test-dir tests -R <test_name> --output-on-failure` |
| Full suite command | `cd build && ctest --output-on-failure` |

### Phase Requirements to Test Map

| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| ENCR-01 | EncryptionProvider implements RocksDB interface, encrypts/decrypts | unit | `ctest -R encryption_provider_test -x` | Wave 0 |
| ENCR-02 | Keyfile loading and key rotation | unit | `ctest -R encryption_key_test -x` | Wave 0 |
| ENCR-03 | IV generation uses RAND_bytes | unit | `ctest -R encryption_iv_test -x` | Wave 0 |
| ENCR-04 | Random-access read/write with CTR | unit | `ctest -R encryption_random_access_test -x` | Wave 0 |
| ENCR-05 | EncryptionFeature registers as ApplicationFeature | unit | `ctest -R encryption_feature_test -x` | Wave 0 |
| AUDIT-01 | AuditFeature lifecycle hooks | unit | `ctest -R audit_feature_test -x` | Wave 0 |
| AUDIT-02 | All 8 topics produce events | unit | `ctest -R audit_topics_test -x` | Wave 0 |
| AUDIT-03 | File and syslog output | unit+integration | `ctest -R audit_output_test -x` | Wave 0 |
| AUDIT-04 | Non-blocking logging | unit | `ctest -R audit_async_test -x` | Wave 0 |
| AUTH-01 | LDAP authentication | unit (mock libldap) | `ctest -R ldap_handler_test -x` | Wave 0 |
| AUTH-02 | Thread-safe concurrent handles | unit | `ctest -R ldap_concurrent_test -x` | Wave 0 |
| AUTH-03 | Role mapping from LDAP groups | unit | `ctest -R ldap_roles_test -x` | Wave 0 |
| AUTH-04 | RBAC policy evaluation | unit | `ctest -R ldap_rbac_test -x` | Wave 0 |
| AUTH-05 | LDAP TLS support | unit (mock) | `ctest -R ldap_tls_test -x` | Wave 0 |
| MASK-01 | AttributeMasking field-level apply | unit | `ctest -R masking_apply_test -x` | Wave 0 |
| MASK-02 | Per-collection per-role config | unit | `ctest -R masking_config_test -x` | Wave 0 |
| MASK-03 | Built-in strategies (redact, hash, partial, randomize) | unit | `ctest -R masking_strategies_test -x` | Wave 0 |
| SSL-01 | SslServerFeatureEE extends base | unit | `ctest -R ssl_ee_feature_test -x` | Wave 0 |
| SSL-02 | mTLS client certificate | unit (mock SSL context) | `ctest -R ssl_mtls_test -x` | Wave 0 |
| SSL-03 | TLS version and cipher restriction | unit | `ctest -R ssl_cipher_test -x` | Wave 0 |

### Sampling Rate

- **Per task commit:** `ctest -R <module>_test --output-on-failure`
- **Per wave merge:** `ctest --output-on-failure`
- **Phase gate:** Full suite green before `/gsd:verify-work`

### Wave 0 Gaps

- [ ] `tests/unit/EncryptionProviderTest.cpp` -- covers ENCR-01 through ENCR-04
- [ ] `tests/unit/EncryptionFeatureTest.cpp` -- covers ENCR-05
- [ ] `tests/unit/AuditFeatureTest.cpp` -- covers AUDIT-01 through AUDIT-04
- [ ] `tests/unit/LDAPHandlerTest.cpp` -- covers AUTH-01 through AUTH-05
- [ ] `tests/unit/MaskingStrategiesTest.cpp` -- covers MASK-01 through MASK-03
- [ ] `tests/unit/SslServerFeatureEETest.cpp` -- covers SSL-01 through SSL-03
- [ ] `tests/mocks/RocksDBMocks.h` -- mock `rocksdb::Env`, `rocksdb::DBOptions` for encryption tests
- [ ] `tests/mocks/LDAPMocks.h` -- mock libldap functions for LDAP tests without real server
- [ ] `tests/mocks/SslMocks.h` -- mock `asio_ns::ssl::context` for SSL tests
- [ ] CMake `find_package(OpenSSL)` integration for test compilation

---

## Sources

### Primary (HIGH confidence)

- ArangoDB source tree at `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/` -- verified all include paths, class declarations, feature registration patterns, RocksDBEngine::_eeData usage, SslServerFeature virtual methods, masking framework in `lib/Maskings/`
- [RocksDB env_encryption.h](https://github.com/facebook/rocksdb/blob/main/include/rocksdb/env_encryption.h) -- `EncryptionProvider`, `BlockCipher`, `BlockAccessCipherStream`, `NewEncryptedEnv()` API verified
- ArangoDB `lib/Maskings/` source -- `AttributeMasking`, `MaskingFunction`, `Maskings`, `Collection`, `RandomMask`, `RandomStringMask` class hierarchies verified

### Secondary (MEDIUM confidence)

- [ArangoDB Audit Logging Docs](https://docs.arangodb.com/3.10/operations/security/audit-logging/) -- 8 topics, log format, `--audit.output` options
- [ArangoDB Encryption at Rest Docs](https://docs.arangodb.com/3.10/operations/security/encryption-at-rest/) -- AES-256-CTR, 32-byte keyfile, key rotation
- [ArangoDB LDAP Options](https://docs.arangodb.com/3.11/components/arangodb-server/ldap/) -- `--ldap.*` options, simple/search auth modes, roles attribute/search
- [ArangoDB Masking Docs](https://docs.arangodb.com/3.11/components/tools/arangodump/maskings/) -- `--maskings` option, JSON config, xifyFront/email strategies
- [ArangoDB Auditing v3.3 docs (Huihoo mirror)](https://docs.huihoo.com/arangodb/3.3/Manual/Administration/Auditing/) -- audit format, configuration, event types

### Tertiary (LOW confidence)

- LDAP thread safety claims based on general OpenLDAP documentation and known behavior -- should validate with specific OpenLDAP version used in CI

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH - verified against ArangoDB source includes and linked libraries
- Architecture: HIGH - patterns derived from actual ArangoDB source code integration points
- Encryption at Rest: HIGH - RocksDB `EncryptionProvider` API verified from official GitHub, ArangoDB usage confirmed in source
- Audit Logging: HIGH - format and topics confirmed from documentation and source registration
- LDAP Authentication: MEDIUM - ArangoDB LDAP is entirely enterprise code (not visible in source tree); patterns based on documented options and libldap best practices
- Data Masking: HIGH - base framework fully inspected in `lib/Maskings/`, enterprise extension point (`InstallMaskingsEE`) confirmed in `arangodump.cpp`
- Enhanced SSL/TLS: HIGH - `SslServerFeature` header fully inspected, virtual methods and `final` constraints verified

**Research date:** 2026-03-31
**Valid until:** 2026-04-30 (stable domain -- RocksDB and OpenSSL APIs change slowly)
