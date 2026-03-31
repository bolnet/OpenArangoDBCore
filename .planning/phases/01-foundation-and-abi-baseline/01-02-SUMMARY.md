---
phase: 01-foundation-and-abi-baseline
plan: 02
subsystem: infra
tags: [cmake, cpp, application-feature, enterprise, license, ssl, rclone, audit, encryption]

requires:
  - phase: 01-foundation-and-abi-baseline plan 01
    provides: Enterprise/ directory layout with arangodb namespace stubs

provides:
  - CMakeLists.txt with all Enterprise/ source paths and arango_rclone separate library target
  - LicenseFeature: full ApplicationFeature subclass, onlySuperUser()=false, isEnterprise()=true
  - AuditFeature, EncryptionFeature, RCloneFeature, HotBackupFeature: minimal ApplicationFeature stubs
  - SslServerFeatureEE: inherits SslServerFeature with prepare()/unprepare() final respected
  - static_assert(!is_abstract_v<T>) guards in all .cpp files

affects:
  - phase 01-03 (test harness will link openarangodb_enterprise and arango_rclone)
  - phase 02-encryption (EncryptionFeature stub will be expanded)
  - phase 03-graph (Graph/Cluster stubs included in openarangodb_enterprise)

tech-stack:
  added: []
  patterns:
    - "ApplicationFeature subclass: static constexpr name(), constructor calls ApplicationFeature(server, *this), all 7 lifecycle methods stubbed"
    - "static_assert(!std::is_abstract_v<T>) in every .cpp to catch vtable ODR violations at compile time"
    - "arango_rclone: separate STATIC library for RCloneFeature so arangod/RocksDBEngine can link against it independently"
    - "SslServerFeatureEE inherits SslServerFeature, not ApplicationFeature — respects prepare()/unprepare() final"

key-files:
  created:
    - "Enterprise/License/LicenseFeature.h — full ApplicationFeature subclass with onlySuperUser() and isEnterprise()"
    - "Enterprise/License/LicenseFeature.cpp — 7 lifecycle methods + static_assert"
  modified:
    - "CMakeLists.txt — Enterprise/ paths, arango_rclone target, -Wall -Woverloaded-virtual -Werror"
    - "Enterprise/Audit/AuditFeature.h — full ApplicationFeature stub with name() and lifecycle"
    - "Enterprise/Encryption/EncryptionFeature.h — full ApplicationFeature stub"
    - "Enterprise/RClone/RCloneFeature.h — full ApplicationFeature stub"
    - "Enterprise/RClone/RCloneFeature.cpp — constructor defined for arango_rclone target"
    - "Enterprise/Ssl/SslServerFeatureEE.h — SslServerFeature subclass (not ApplicationFeature directly)"
    - "Enterprise/Ssl/SslServerFeatureEE.cpp — constructor + static_assert"
    - "Enterprise/StorageEngine/HotBackupFeature.h — full ApplicationFeature stub"

key-decisions:
  - "arango_rclone is a separate STATIC library target (not part of openarangodb_enterprise) — mirrors ArangoDB's CMake layout where arango_rocksdb links arango_rclone directly"
  - "LicenseFeature::onlySuperUser() returns false and isEnterprise() returns true — open-source build unlocks all enterprise features without license check"
  - "SslServerFeatureEE inherits SslServerFeature, not ApplicationFeature — SslServerFeature::prepare() and unprepare() are marked final, so EE cannot re-override them"
  - "-Wall -Woverloaded-virtual -Werror added to both CMake targets to catch vtable ODR violations at compile time (Research Pitfall 4)"

patterns-established:
  - "Pattern: ApplicationFeature stub — static constexpr name(), constructor calls ApplicationFeature(server, *this), all lifecycle methods have empty bodies"
  - "Pattern: static_assert in .cpp — every feature .cpp file includes static_assert(!std::is_abstract_v<ClassName>) as compile-time vtable check"

requirements-completed: [FOUND-02, FOUND-03, FOUND-04]

duration: 3min
completed: 2026-03-31
---

# Phase 1 Plan 02: CMake Enterprise Paths and LicenseFeature Summary

**CMakeLists.txt rewritten with Enterprise/ source paths and arango_rclone STATIC target; LicenseFeature implemented as full ApplicationFeature subclass with onlySuperUser()=false and isEnterprise()=true**

## Performance

- **Duration:** 3 min
- **Started:** 2026-03-31T03:40:06Z
- **Completed:** 2026-03-31T03:42:45Z
- **Tasks:** 2
- **Files modified:** 11

## Accomplishments

- CMakeLists.txt now references all 21 Enterprise/ .cpp files and defines both `openarangodb_enterprise` and `arango_rclone` STATIC library targets with -Wall -Woverloaded-virtual -Werror
- LicenseFeature fully implements the ApplicationFeature interface: constructor, all 7 lifecycle methods (collectOptions, validateOptions, prepare, start, beginShutdown, stop, unprepare), onlySuperUser()=false, isEnterprise()=true
- All 5 other arangod_includes.h features (Audit, Encryption, RClone, HotBackup, SslServerFeatureEE) updated with correct ApplicationFeature inheritance, static constexpr name(), and static_assert vtable guards
- SslServerFeatureEE correctly inherits from SslServerFeature (not ApplicationFeature directly), respecting base class final methods

## Task Commits

1. **Task 1: Rewrite CMakeLists.txt with Enterprise/ paths and arango_rclone target** - `2abc62f` (feat)
2. **Task 2: Implement LicenseFeature and ensure all 6 arangod_includes.h features compile** - `476dc6b` (feat)

**Plan metadata:** (docs commit follows)

## Files Created/Modified

- `CMakeLists.txt` - Enterprise/ source paths, arango_rclone STATIC target, compile flags
- `Enterprise/License/LicenseFeature.h` - Full ApplicationFeature subclass with onlySuperUser/isEnterprise
- `Enterprise/License/LicenseFeature.cpp` - 7 lifecycle methods, static_assert non-abstract
- `Enterprise/Audit/AuditFeature.h` - Minimal ApplicationFeature stub with name() and lifecycle
- `Enterprise/Audit/AuditFeature.cpp` - static_assert non-abstract
- `Enterprise/Encryption/EncryptionFeature.h` - Minimal ApplicationFeature stub
- `Enterprise/Encryption/EncryptionFeature.cpp` - static_assert non-abstract
- `Enterprise/RClone/RCloneFeature.h` - Minimal ApplicationFeature stub (arango_rclone target)
- `Enterprise/RClone/RCloneFeature.cpp` - Constructor defined for arango_rclone target, static_assert
- `Enterprise/Ssl/SslServerFeatureEE.h` - Inherits SslServerFeature, name()="SslServer"
- `Enterprise/Ssl/SslServerFeatureEE.cpp` - Constructor, static_assert non-abstract
- `Enterprise/StorageEngine/HotBackupFeature.h` - Full ApplicationFeature stub

## Decisions Made

- arango_rclone defined as a separate STATIC library target, not folded into openarangodb_enterprise. This mirrors ArangoDB's own CMake layout where `arangod/RocksDBEngine/CMakeLists.txt` does `target_link_libraries(arango_rocksdb PRIVATE arango_rclone)`.
- LicenseFeature::onlySuperUser() returns false and isEnterprise() returns true. The open-source build should unlock all enterprise features without a license check — this is the project's core purpose.
- SslServerFeatureEE inherits SslServerFeature directly. SslServerFeature::prepare() and unprepare() are `override final`, so re-overriding them in EE would be a compile error.
- -Wall -Woverloaded-virtual -Werror added as compile flags. This catches vtable ODR violations (Research Pitfall 4) at build time rather than getting silent link-time undefined behavior.

## Deviations from Plan

None — plan executed exactly as written.

## Issues Encountered

Minor: The automated verify command `! grep -q "arangod/" CMakeLists.txt` initially failed because two comment lines in the new CMakeLists.txt contained the string "arangod/" in explanatory text. Fixed by rephrasing comments to not contain that literal string.

## User Setup Required

None — no external service configuration required.

## Next Phase Readiness

- CMake build system is ready: both targets defined, all source paths correct
- LicenseFeature is the enterprise capability gate — returns true for all capability checks
- Plan 03 can now add test infrastructure that links against openarangodb_enterprise and arango_rclone
- No blockers for Phase 1 Plan 03

---
*Phase: 01-foundation-and-abi-baseline*
*Completed: 2026-03-31*
