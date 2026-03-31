---
phase: 2
slug: security-foundations
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-31
---

# Phase 2 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | GoogleTest 1.17.0 (via FetchContent, established in Phase 1) |
| **Config file** | tests/CMakeLists.txt |
| **Quick run command** | `cd build && ctest --test-dir tests -R "Phase2" -V` |
| **Full suite command** | `cd build && ctest --test-dir tests -V` |
| **Estimated runtime** | ~15 seconds |

---

## Sampling Rate

- **After every task commit:** Run `cd build && ctest --test-dir tests -R "Phase2" -V`
- **After every plan wave:** Run `cd build && ctest --test-dir tests -V`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 15 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | ENCR-01 | unit | `ctest -R EncryptionProvider` | ❌ W0 | ⬜ pending |
| 02-01-02 | 01 | 1 | ENCR-02 | unit | `ctest -R EncryptionKeyRotation` | ❌ W0 | ⬜ pending |
| 02-02-01 | 02 | 1 | AUDIT-01 | unit | `ctest -R AuditFeature` | ❌ W0 | ⬜ pending |
| 02-02-02 | 02 | 1 | AUDIT-02 | unit | `ctest -R AuditTopics` | ❌ W0 | ⬜ pending |
| 02-03-01 | 03 | 2 | AUTH-01 | unit | `ctest -R LDAPHandler` | ❌ W0 | ⬜ pending |
| 02-03-02 | 03 | 2 | AUTH-02 | unit | `ctest -R LDAPRoleMapping` | ❌ W0 | ⬜ pending |
| 02-04-01 | 04 | 2 | MASK-01 | unit | `ctest -R AttributeMasking` | ❌ W0 | ⬜ pending |
| 02-05-01 | 05 | 2 | SSL-01 | unit | `ctest -R SslServerFeatureEE` | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/unit/EncryptionProviderTest.cpp` — stubs for ENCR-01 through ENCR-05
- [ ] `tests/unit/AuditFeatureTest.cpp` — stubs for AUDIT-01 through AUDIT-04
- [ ] `tests/unit/LDAPHandlerTest.cpp` — stubs for AUTH-01 through AUTH-05
- [ ] `tests/unit/AttributeMaskingTest.cpp` — stubs for MASK-01 through MASK-03
- [ ] `tests/unit/SslServerFeatureEETest.cpp` — stubs for SSL-01 through SSL-03
- [ ] `tests/mocks/GeneralServer/SslServerFeature.h` — mock for SslServerFeature base class

*Existing GoogleTest infrastructure from Phase 1 covers framework setup.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| RocksDB SST/WAL no plaintext | ENCR-01 | Requires running ArangoDB with data | Create test DB, write data, stop, xxd SST files |
| LDAP auth against real server | AUTH-01 | Requires LDAP server | Stand up OpenLDAP in Docker, configure ArangoDB |
| mTLS client cert validation | SSL-02 | Requires TLS handshake | Use openssl s_client with valid/invalid certs |
| Audit syslog output | AUDIT-03 | Requires syslog daemon | Configure syslog output, check /var/log |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 15s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
