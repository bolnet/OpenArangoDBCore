---
phase: 1
slug: foundation-and-abi-baseline
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-03-30
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | CMake CTest + custom shell scripts |
| **Config file** | `CMakeLists.txt` (top-level) |
| **Quick run command** | `cmake --build build --target openarangodb_enterprise 2>&1 | tail -20` |
| **Full suite command** | `cd build && ctest --output-on-failure` |
| **Estimated runtime** | ~30 seconds (compile) + ~5 seconds (link tests) |

---

## Sampling Rate

- **After every task commit:** Run `cmake --build build --target openarangodb_enterprise`
- **After every plan wave:** Run full CTest suite
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 35 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 1-01-01 | 01 | 1 | FOUND-01 | build | `grep -r "namespace openarangodb" arangod/` (must return empty) | ❌ W0 | ⬜ pending |
| 1-01-02 | 01 | 1 | FOUND-02 | build | `cmake -DUSE_ENTERPRISE=1 ..` succeeds | ❌ W0 | ⬜ pending |
| 1-02-01 | 02 | 1 | FOUND-03 | link | `cmake --build build` completes with no unresolved symbols | ❌ W0 | ⬜ pending |
| 1-02-02 | 02 | 1 | FOUND-04 | build | `LicenseFeature::isEnterprise()` returns true | ❌ W0 | ⬜ pending |
| 1-03-01 | 03 | 2 | FOUND-05 | CI | ASan build exits clean | ❌ W0 | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

- [ ] `tests/CMakeLists.txt` — CTest configuration for link tests
- [ ] `tests/test_link_symbols.cpp` — per-module symbol link test
- [ ] `scripts/check_namespace.sh` — namespace audit script (grep for wrong namespace)
- [ ] CMake build directory configured against ArangoDB source

*Wave 0 establishes the build+test loop that all subsequent tasks validate against.*

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| ArangoDB binary starts with enterprise features | FOUND-04 | Requires running ArangoDB binary | Build, start `arangod`, query `/_admin/license` |
| ASan CI pipeline green | FOUND-05 | Requires CI environment (GitHub Actions) | Push to branch, check Actions tab |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 35s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
