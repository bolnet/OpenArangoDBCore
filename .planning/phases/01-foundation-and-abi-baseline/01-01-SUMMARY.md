---
phase: 01-foundation-and-abi-baseline
plan: 01
subsystem: infra
tags: [cpp, cmake, namespace, enterprise, abi, rocksdb, iresearch]

requires: []
provides:
  - "Complete Enterprise/ directory tree (65 files) matching ArangoDB #include Enterprise/ directives"
  - "ARANGODB_ENTERPRISE_VERSION macro defined in Enterprise/Basics/Version.h"
  - "arangodb::enterprise::RocksDBEngineEEData struct in Enterprise/RocksDBEngine/RocksDBEngineEE.h"
  - "arangodb::enterprise::EncryptionProvider base class in Enterprise/RocksDBEngine/EncryptionProvider.h"
  - "HotBackupFeature ApplicationFeature subclass in Enterprise/StorageEngine/HotBackupFeature.h"
  - "Correct arangodb namespace in all 65 files (zero openarangodb occurrences)"
  - "Old arangod/ directory removed"
affects:
  - 01-02
  - 01-03
  - 02-encryption-and-hotbackup
  - 03-graph-and-cluster
  - 04-iresearch-and-maskings
  - 05-dc2dc-replication

tech-stack:
  added: []
  patterns:
    - "Enterprise headers live under Enterprise/ (not arangod/), matching ArangoDB #include Enterprise/Foo/Bar.h"
    - "All enterprise code uses namespace arangodb (not openarangodb)"
    - "Enterprise-specific subcomponents use namespace arangodb::enterprise"
    - "Include guards use ARANGODB_ prefix (not OPENARANGODB_)"
    - "Critical struct/class stubs use namespace arangodb::enterprise for RocksDB-level types"

key-files:
  created:
    - Enterprise/Basics/Version.h
    - Enterprise/RocksDBEngine/RocksDBEngineEE.h
    - Enterprise/StorageEngine/HotBackupFeature.h
    - Enterprise/RocksDBEngine/EncryptionProvider.h
    - Enterprise/RocksDBEngine/RocksDBEncryptionUtilsEE.h
    - Enterprise/RocksDBEngine/RocksDBHotBackup/RocksDBHotBackup.h
    - Enterprise/RestHandler/RestHotBackupHandler.h
    - Enterprise/VocBase/VirtualClusterSmartEdgeCollection.h
    - Enterprise/Aql/LocalEnumeratePathsNode.h
    - Enterprise/Aql/LocalGraphNode.h
    - Enterprise/Aql/LocalShortestPathNode.h
    - Enterprise/Graph/Providers/SmartGraphRPCCommunicator.h
    - Enterprise/Graph/Providers/SingleServerProviderEE.tpp
    - Enterprise/Graph/Enumerators/OneSidedEnumeratorEE.tpp
    - Enterprise/Graph/PathValidatorEE.cpp
    - Enterprise/IResearch/GeoAnalyzerEE.h
    - Enterprise/IResearch/IResearchAnalyzerFeature.h
    - Enterprise/IResearch/IResearchDataStoreEE.hpp
    - Enterprise/IResearch/IResearchDocumentEE.h
    - Enterprise/IResearch/IResearchDocumentEE.hpp
    - Enterprise/Maskings/AttributeMaskingEE.h
    - Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h
    - Enterprise/Transaction/IgnoreNoAccessMethods.h
  modified:
    - "All 21 files relocated from arangod/ to Enterprise/ with namespace/guard fixes"

key-decisions:
  - "openarangodb namespace replaced with arangodb throughout — ArangoDB resolves enterprise includes against namespace arangodb"
  - "Enterprise-specific low-level types (e.g., RocksDBEngineEEData, EncryptionProvider) placed under arangodb::enterprise sub-namespace"
  - "SslServerFeatureEE.h includes and inherits from SslServerFeature per ArangoDB include pattern"
  - "HotBackupFeature inherits from ApplicationFeature and provides static name() constexpr per ArangoDB feature registration pattern"
  - "arangod/ directory fully removed — no backward compatibility required since it was wrong from the start"

patterns-established:
  - "Namespace rule: top-level enterprise features use namespace arangodb; RocksDB/storage-layer enterprise types use namespace arangodb::enterprise"
  - "Include guard naming: ARANGODB_<MODULE>_<FILENAME>_H (e.g., ARANGODB_ROCKSDB_ENGINE_EE_H)"
  - "Stub files: each contains #pragma once, correct include guard, correct namespace, and TODO comment pointing to phase"

requirements-completed: [FOUND-01]

duration: 8min
completed: 2026-03-31
---

# Phase 1 Plan 01: ABI Baseline — Repository Layout and Namespace Correction Summary

**65 Enterprise/ stub files in correct ArangoDB include paths, all using namespace arangodb, with ARANGODB_ENTERPRISE_VERSION macro and RocksDBEngineEEData struct for Phase 1 compilation**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-03-31T03:31:05Z
- **Completed:** 2026-03-31T03:39:00Z
- **Tasks:** 2
- **Files modified:** 65 (42 moved/fixed from arangod/, 23 new stubs)

## Accomplishments
- Relocated all 21 source files from wrong `arangod/` layout to correct `Enterprise/` layout matching ArangoDB's `#include "Enterprise/Foo/Bar.h"` include directives
- Fixed all namespaces from `openarangodb` to `arangodb` (or `arangodb::enterprise` for storage-layer types); zero occurrences of `openarangodb` remain
- Created 23 missing stub files: 5 critical headers with required content (Version.h, RocksDBEngineEE.h, HotBackupFeature.h, EncryptionProvider.h, SslServerFeatureEE.h) plus 18 minimal stubs for Phase 3/4/5 features
- Deleted `arangod/` directory entirely

## Task Commits

1. **Task 1: Move stubs from arangod/ to Enterprise/ and fix namespaces** - `6cf0357` (feat)
2. **Task 2: Create 23 missing stub files required by ArangoDB include directives** - `5971c37` (feat)

**Plan metadata:** _(pending final docs commit)_

## Files Created/Modified

- `Enterprise/Basics/Version.h` - Defines ARANGODB_ENTERPRISE_VERSION macro (hard requirement — ArangoDB emits #error if absent)
- `Enterprise/RocksDBEngine/RocksDBEngineEE.h` - Defines arangodb::enterprise::RocksDBEngineEEData struct (member in RocksDBEngine)
- `Enterprise/StorageEngine/HotBackupFeature.h` - ApplicationFeature subclass registered via addFeature<HotBackupFeature>()
- `Enterprise/RocksDBEngine/EncryptionProvider.h` - Base class for Phase 2 RocksDB encryption
- `Enterprise/Ssl/SslServerFeatureEE.h` - Inherits from SslServerFeature (correct ArangoDB include pattern)
- `Enterprise/License/LicenseFeature.{h,cpp}` - Namespace fixed (openarangodb -> arangodb)
- `Enterprise/Audit/AuditFeature.{h,cpp}` - Namespace fixed
- `Enterprise/Encryption/Encryption{Feature,Provider}.{h,cpp}` - Namespace fixed
- `Enterprise/RocksDBEngine/RocksDB{EncryptionUtils,HotBackup,BuilderIndexEE}.{h,cpp}` - Namespace fixed
- `Enterprise/Graph/Providers/Single ServerProviderEE.tpp` - Empty template implementation stub
- `Enterprise/Graph/Enumerators/OneSidedEnumeratorEE.tpp` - Empty template implementation stub
- (plus 42 more files — see task commits for full diff)

## Decisions Made

- `openarangodb` namespace entirely replaced with `arangodb` — ArangoDB's include system resolves enterprise headers in the `arangodb` namespace, not a project-specific one
- RocksDB/storage-layer enterprise types placed in `arangodb::enterprise` sub-namespace per ArangoDB convention (RocksDBEngineEEData, EncryptionProvider)
- `SslServerFeatureEE.h` updated to inherit from `SslServerFeature` (was bare stub without inheritance)
- `HotBackupFeature` given proper `ApplicationFeature` inheritance and `name()` constexpr per ArangoDB feature registration pattern

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] SslServerFeatureEE.h given proper SslServerFeature inheritance**
- **Found during:** Task 1 (moving Ssl files)
- **Issue:** Original stub had no inheritance; Task 2 spec explicitly required inheritance from SslServerFeature
- **Fix:** Applied the inheritance pattern from Task 2 spec directly during Task 1 file creation
- **Files modified:** Enterprise/Ssl/SslServerFeatureEE.h
- **Verification:** File contains `SslServerFeature` base class include and inheritance
- **Committed in:** 6cf0357 (Task 1 commit)

---

**Total deviations:** 1 auto-fixed (missing critical inheritance)
**Impact on plan:** Essential for correct compilation. No scope creep.

## Issues Encountered

None - files followed uniform openarangodb pattern, making bulk namespace substitution straightforward.

## User Setup Required

None - no external service configuration required.

## Next Phase Readiness

- All 65 Enterprise/ files exist at paths matching ArangoDB `#include "Enterprise/"` directives
- Enterprise/Basics/Version.h ready to satisfy `#error ARANGODB_ENTERPRISE_VERSION` checks
- Enterprise/RocksDBEngine/RocksDBEngineEE.h ready for Phase 2 to add EncryptionProvider* member
- Enterprise/RocksDBEngine/EncryptionProvider.h ready for Phase 2 rocksdb::ObjectRegistry registration
- Blockers from STATE.md cleared: namespace mismatch (openarangodb) is fully resolved

---
*Phase: 01-foundation-and-abi-baseline*
*Completed: 2026-03-31*

## Self-Check: PASSED

- FOUND: Enterprise/Basics/Version.h
- FOUND: Enterprise/RocksDBEngine/RocksDBEngineEE.h
- FOUND: Enterprise/StorageEngine/HotBackupFeature.h
- FOUND: Enterprise/License/LicenseFeature.h
- FOUND: .planning/phases/01-foundation-and-abi-baseline/01-01-SUMMARY.md
- FOUND commit: 6cf0357 (Task 1)
- FOUND commit: 5971c37 (Task 2)
