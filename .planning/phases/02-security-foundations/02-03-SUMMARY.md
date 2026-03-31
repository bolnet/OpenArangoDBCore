---
phase: 02-security-foundations
plan: 03
subsystem: auth
tags: [ldap, libldap, tls, rbac, thread-safety, c++20]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: Enterprise/ directory layout, CMake build system, namespace correction
provides:
  - LDAPHandler with per-request LDAP* handles for thread-safe authentication
  - LDAPConfig struct with typed --ldap.* option storage and validation
  - Simple and Search authentication modes against LDAP directory
  - Role mapping via attribute read and group search
  - TLS support (StartTLS and LDAPS) with CA certificate verification
  - Mock libldap framework (LDAPMocks.h) for testing without real LDAP server
  - Function pointer indirection pattern for C library testability
affects: [02-05-ssl-ee, phase-5-dc2dc]

# Tech tracking
tech-stack:
  added: [libldap (optional, runtime), LDAPMocks (test-only)]
  patterns: [function-pointer-indirection-for-c-libs, per-request-handle-pattern, conditional-compilation-via-define]

key-files:
  created:
    - Enterprise/Auth/LDAPConfig.h
    - Enterprise/Auth/LDAPConfig.cpp
    - tests/mocks/LDAPMocks.h
    - tests/unit/LDAPHandlerTest.cpp
  modified:
    - Enterprise/Auth/LDAPHandler.h
    - Enterprise/Auth/LDAPHandler.cpp
    - CMakeLists.txt
    - tests/CMakeLists.txt

key-decisions:
  - "Function pointer indirection instead of link-time substitution for mocking libldap -- enables per-test mock configuration without separate link targets"
  - "No shared LDAP* member in LDAPHandler -- each authenticate() creates and destroys its own handle for thread safety"
  - "Conditional compilation: stub mode when ARANGODB_HAVE_LDAP not defined, mock mode for tests, real libldap for production"

patterns-established:
  - "Function pointer indirection: LDAPFunctions struct wraps C library calls, swappable in constructor for testing"
  - "Per-request resource pattern: ephemeral LDAP* handles with RAII-style create/destroy in each authenticate() call"
  - "Mock header pattern: LDAPMocks.h provides types, constants, and mock implementations for standalone test compilation"

requirements-completed: [AUTH-01, AUTH-02, AUTH-03, AUTH-04, AUTH-05]

# Metrics
duration: 27min
completed: 2026-03-31
---

# Phase 2 Plan 3: LDAP Authentication Summary

**Thread-safe LDAPHandler with per-request handles, simple/search auth modes, attribute/search role mapping, and TLS support via function-pointer-indirected libldap**

## Performance

- **Duration:** 27 min
- **Started:** 2026-03-31T20:21:00Z
- **Completed:** 2026-03-31T20:48:57Z
- **Tasks:** 1 (TDD: RED + GREEN)
- **Files modified:** 8

## Accomplishments
- LDAPHandler authenticates via simple mode (prefix+username+suffix DN construction) and search mode (admin bind + search + user bind)
- Per-request LDAP* handles with no shared handle member ensure thread safety under 8-thread concurrent load
- Role mapping works via both attribute read (rolesAttribute) and group search (rolesSearch with {userDn} placeholder)
- TLS support: StartTLS on port 389, LDAPS (ldaps://) on port 636, CA certificate verification
- Conditional compilation: builds without libldap as a stub that always returns false
- All 16 unit tests pass with mocked libldap (no real LDAP server required)

## Task Commits

Each task was committed atomically:

1. **Task 1 (RED): LDAP handler failing tests** - `5adc180` (test)
2. **Task 1 (GREEN): LDAP handler implementation** - `98bac0b` (feat)

_TDD task: RED committed tests, GREEN committed implementation that passes all tests._

## Files Created/Modified
- `Enterprise/Auth/LDAPConfig.h` - Typed config struct with all --ldap.* options and validation
- `Enterprise/Auth/LDAPConfig.cpp` - Config validation (server, port, mode requirements, TLS conflicts)
- `Enterprise/Auth/LDAPHandler.h` - LDAPHandler class with LDAPFunctions indirection table, no shared LDAP* member
- `Enterprise/Auth/LDAPHandler.cpp` - Full implementation: createHandle, destroyHandle, bindUser, authenticate (simple/search), fetchRoles (attribute/search)
- `tests/mocks/LDAPMocks.h` - Mock libldap types (MockLDAPHandle, MockLDAPMessage, MockBerval) and mock function implementations
- `tests/unit/LDAPHandlerTest.cpp` - 16 GoogleTest tests covering AUTH-01 through AUTH-05
- `CMakeLists.txt` - Added LDAPConfig.cpp to enterprise library sources
- `tests/CMakeLists.txt` - Added ldap_handler_test target with ARANGODB_LDAP_MOCK_MODE, fixed GoogleTest SHA256 hash

## Decisions Made
- **Function pointer indirection over link-time substitution:** LDAPFunctions struct holds std::function wrappers for all libldap calls. This allows per-test mock configuration via constructor injection, avoiding complex CMake link-time mocking setups.
- **No connection pooling (yet):** Per-request handle create/destroy is simpler and guaranteed thread-safe. Pooling can be added later as an optimization if LDAP auth latency becomes a concern.
- **Conditional compilation via three modes:** ARANGODB_LDAP_MOCK_MODE for tests, ARANGODB_HAVE_LDAP for production with real libldap, and stub mode (neither defined) that compiles but always returns false.

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 3 - Blocking] Fixed GoogleTest SHA256 hash mismatch in tests/CMakeLists.txt**
- **Found during:** Task 1 (build phase)
- **Issue:** SHA256 hash for GoogleTest v1.17.0 tarball was incorrect, preventing CMake configuration
- **Fix:** Updated hash from `65fab70b01e38b24b55e1c5c65e15a674d3b2f3b6f7b7b5e5a9d8e7c7e6a8f2d` to `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c`
- **Files modified:** tests/CMakeLists.txt
- **Verification:** CMake configure succeeds
- **Committed in:** 5adc180 (part of RED commit)

---

**Total deviations:** 1 auto-fixed (1 blocking)
**Impact on plan:** SHA256 fix was necessary for build to work. No scope creep.

## Issues Encountered
- cmake not on PATH despite being installed via Homebrew. Resolved by using explicit path `/opt/homebrew/opt/cmake/bin/cmake`.

## User Setup Required
None - no external service configuration required. LDAP tests use mocked libldap and do not require a real LDAP server.

## Next Phase Readiness
- LDAP authentication subsystem complete, ready for integration with ArangoDB's authentication framework
- SslServerFeatureEE (Plan 05) can leverage TLS patterns established here
- Phase 5 (DC2DC) can use LDAP role mapping for replication access control

## Self-Check: PASSED

All 6 created/modified source files verified on disk. Both commits (5adc180, 98bac0b) verified in git history.

---
*Phase: 02-security-foundations*
*Completed: 2026-03-31*
