# Phase 1: Foundation and ABI Baseline - Research

**Researched:** 2026-03-30
**Domain:** C++ enterprise plugin drop-in for ArangoDB — directory structure, namespace hierarchy, CMake integration, LicenseFeature interface, ODR/ABI discipline
**Confidence:** HIGH (all critical findings verified against ArangoDB source at `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/`)

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| FOUND-01 | All source files use `namespace arangodb` (matching ArangoDB's expected symbols) | Confirmed: ArangoDB accesses `arangodb::LicenseFeature`, `arangodb::EncryptionFeature`, `arangodb::enterprise::EncryptionProvider` — see Namespace Hierarchy section |
| FOUND-02 | CMakeLists.txt integrates correctly with ArangoDB's build system when placed as `enterprise/` directory | Confirmed: `ENTERPRISE_INCLUDE_DIR = "enterprise"` + `add_subdirectory(enterprise)` — see CMake Integration section |
| FOUND-03 | Static library links without unresolved symbols when built with `-DUSE_ENTERPRISE=1` | Confirmed: file structure must be `Enterprise/` prefix; see Directory Structure section |
| FOUND-04 | LicenseFeature returns true for all enterprise capability checks (always-enabled) | Confirmed: `arangodb::LicenseFeature` with `onlySuperUser()` method; see LicenseFeature Interface section |
| FOUND-05 | CI pipeline builds against ArangoDB source with AddressSanitizer enabled | Standard ASan configuration; see Validation Architecture section |
</phase_requirements>

---

## Summary

Phase 1 is a pure structural and ABI correctness phase. No feature logic is implemented — only the skeleton that makes all subsequent phases possible. Three critical structural problems in the current codebase must be fixed before a single line of feature code is written.

**Problem 1 — Wrong directory layout (HIGH SEVERITY):** The current stubs live at `arangod/License/LicenseFeature.h`. ArangoDB includes enterprise headers as `Enterprise/License/LicenseFeature.h` (confirmed in `arangod/RestServer/arangod_includes.h` line 168). The `ENTERPRISE_INCLUDE_DIR` is set to `"enterprise"` (root `CMakeLists.txt` line 200), and `${PROJECT_SOURCE_DIR}/enterprise` is added to every target's include path. Therefore the OpenArangoDBCore repository, when placed as `enterprise/` inside ArangoDB, must contain files at `Enterprise/License/LicenseFeature.h` — not `arangod/License/LicenseFeature.h`. Every header file in the repo must be moved from `arangod/` to `Enterprise/`.

**Problem 2 — Wrong namespace (HIGH SEVERITY):** Every stub currently uses `namespace openarangodb`. ArangoDB's code uses `arangodb::LicenseFeature`, `arangodb::EncryptionFeature`, `arangodb::HotBackupFeature`, `arangodb::AuditFeature`, `arangodb::RCloneFeature`, and `arangodb::enterprise::EncryptionProvider`. All namespaces must be changed before any linking attempt.

**Problem 3 — Incomplete file inventory (HIGH SEVERITY):** ArangoDB requires 35 Enterprise header files (confirmed by scanning all `#include "Enterprise/"` directives in the source tree). The current repo provides 21. The 14 missing files — especially `Enterprise/Basics/Version.h` (which defines `ARANGODB_ENTERPRISE_VERSION` required at compile time), `Enterprise/RocksDBEngine/RocksDBEngineEE.h` (included unconditionally in `RocksDBEngine.h`), and multiple Transaction/Graph template files — must be created as stubs before the build can complete.

**Primary recommendation:** In Phase 1, rename all `arangod/` paths to `Enterprise/`, fix all namespaces to `arangodb` (or `arangodb::enterprise` where the source confirms it), create the 14 missing stub files, implement `LicenseFeature` with `onlySuperUser()` returning `false`, configure the CMakeLists.txt to export `arango_rclone` as an empty library, and set up CI with ASan `detect_odr_violation=2`.

---

## Standard Stack

### Core (inherited from ArangoDB build — do NOT install separately)

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| C++20 | ISO/IEC 14882:2020 | Implementation language | ArangoDB VERSIONS file pins `"C++ standard": "20"`; GCC 13.2.0 and Clang 19.1.7 are the tested compilers |
| CMake | 3.20+ | Build system | ArangoDB's minimum; current `cmake_minimum_required(VERSION 3.20)` is correct |
| RocksDB 7.2.x | bundled at `3rdParty/rocksdb/` | Storage engine headers | ArangoDB bundles RocksDB; adding a separate installation causes ABI conflicts |
| VelocyPack | bundled at `3rdParty/velocypack/` | Binary serialization | All ArangoDB cluster/graph APIs pass `velocypack::Slice` and `Builder` |
| ArangoDB internal headers | `${CMAKE_SOURCE_DIR}/arangod/` and `lib/` | Base classes: `ApplicationFeature`, `SslServerFeature`, `ShardingStrategy` | These are the contracts OpenArangoDBCore must implement |

### Testing Stack (standalone, outside ArangoDB full build)

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| GoogleTest + GMock | 1.17.0 | Unit tests and interface mocking | All tests; GMock is critical for mocking `ApplicationFeature` and `ShardingStrategy` |
| CMake CTest | 3.20+ | Test runner | Integrated with CMake; `gtest_discover_tests()` for automatic discovery |
| AddressSanitizer | GCC/Clang built-in | ODR violation and memory safety detection | CI build flag: `-fsanitize=address`, env: `ASAN_OPTIONS=detect_odr_violation=2` |

**Installation (testing stack only):**
```bash
# GoogleTest via CMake FetchContent (no manual install)
# Add to tests/CMakeLists.txt:
# include(FetchContent)
# FetchContent_Declare(googletest URL https://github.com/google/googletest/archive/v1.17.0.tar.gz)
# FetchContent_MakeAvailable(googletest)
```

---

## Architecture Patterns

### Critical: Correct Directory Structure

ArangoDB sets `ENTERPRISE_INCLUDE_DIR = "enterprise"` (root `CMakeLists.txt` line 200) and adds `${PROJECT_SOURCE_DIR}/enterprise` as a private include path to every library target (confirmed in `arangod/CMakeLists.txt` line 65, `arangod/RocksDBEngine/CMakeLists.txt` line 138, `arangod/Graph/CMakeLists.txt` line 25, and 15+ other `CMakeLists.txt` files).

ArangoDB's headers then include enterprise code as:
```cpp
#include "Enterprise/License/LicenseFeature.h"   // resolves to: enterprise/Enterprise/License/LicenseFeature.h
#include "Enterprise/Encryption/EncryptionFeature.h"
#include "Enterprise/RocksDBEngine/RocksDBEngineEE.h"
```

**Required repository layout (OpenArangoDBCore is placed as `enterprise/` inside ArangoDB):**

```
enterprise/                               ← OpenArangoDBCore repo root (= ${PROJECT_SOURCE_DIR}/enterprise)
└── Enterprise/                           ← REQUIRED: must be named "Enterprise" exactly
    ├── Aql/
    │   ├── LocalEnumeratePathsNode.h     ← stub (Phase 3)
    │   ├── LocalGraphNode.h              ← stub (Phase 3)
    │   ├── LocalShortestPathNode.h       ← stub (Phase 3)
    │   └── LocalTraversalNode.h         ← stub (Phase 3)
    ├── Audit/
    │   └── AuditFeature.h               ← implement (Phase 2)
    ├── Basics/
    │   └── Version.h                    ← MUST define ARANGODB_ENTERPRISE_VERSION (Phase 1)
    ├── Encryption/
    │   └── EncryptionFeature.h          ← stub (Phase 2)
    ├── Graph/
    │   ├── Enumerators/
    │   │   └── OneSidedEnumeratorEE.tpp ← stub (Phase 3)
    │   ├── Providers/
    │   │   ├── SingleServerProviderEE.tpp ← stub (Phase 3)
    │   │   ├── SmartGraphProvider.h     ← stub (Phase 3)
    │   │   └── SmartGraphRPCCommunicator.h ← stub (Phase 3)
    │   ├── Steps/
    │   │   └── SmartGraphStep.h         ← stub (Phase 3)
    │   └── PathValidatorEE.cpp          ← stub (Phase 3)
    ├── IResearch/
    │   ├── GeoAnalyzerEE.h              ← stub (Phase 4)
    │   ├── IResearchAnalyzerFeature.h   ← stub (Phase 4)
    │   ├── IResearchDataStoreEE.hpp     ← stub (Phase 4)
    │   ├── IResearchDocumentEE.h        ← stub (Phase 4)
    │   ├── IResearchDocumentEE.hpp      ← stub (Phase 4)
    │   └── IResearchOptimizeTopK.h      ← stub (Phase 4)
    ├── License/
    │   └── LicenseFeature.h             ← IMPLEMENT fully (Phase 1)
    ├── Maskings/
    │   └── AttributeMaskingEE.h         ← stub (Phase 2)
    ├── RClone/
    │   └── RCloneFeature.h              ← stub (Phase 2)
    ├── RestHandler/
    │   └── RestHotBackupHandler.h       ← stub (Phase 4)
    ├── RocksDBEngine/
    │   ├── EncryptionProvider.h         ← stub (Phase 2)
    │   ├── RocksDBBuilderIndexEE.h      ← stub (Phase 4)
    │   ├── RocksDBEncryptionUtilsEE.h   ← stub (Phase 2)
    │   ├── RocksDBEngineEE.h            ← MUST define RocksDBEngineEEData struct (Phase 1)
    │   └── RocksDBHotBackup/
    │       └── RocksDBHotBackup.h       ← stub (Phase 4)
    ├── Sharding/
    │   └── ShardingStrategyEE.h         ← stub (Phase 3)
    ├── Ssl/
    │   └── SslServerFeatureEE.h         ← stub (Phase 2)
    ├── StorageEngine/
    │   └── HotBackupFeature.h           ← stub (Phase 4)
    ├── Transaction/
    │   ├── IgnoreNoAccessAqlTransaction.h ← stub (Phase 2)
    │   └── IgnoreNoAccessMethods.h      ← stub (Phase 2)
    └── VocBase/
        ├── SmartGraphSchema.h           ← stub (Phase 3)
        └── VirtualClusterSmartEdgeCollection.h ← stub (Phase 3)
```

### Pattern 1: ApplicationFeature Subclass

All server-level enterprise features extend `arangodb::application_features::ApplicationFeature`. The constructor pattern uses the CRTP template overload — the subclass must provide a `static constexpr std::string_view name()` method.

```cpp
// Source: lib/ApplicationFeatures/ApplicationFeature.h (lines 163-168)
// The template constructor resolves to ApplicationFeature(server, typeid(Impl), Impl::name())
template<typename Server, typename Impl>
ApplicationFeature(Server& server, const Impl&)
    : ApplicationFeature{server, typeid(Impl), Impl::name()} {}
```

The concrete LicenseFeature must follow this exact pattern:

```cpp
// Enterprise/License/LicenseFeature.h
#pragma once
#include "ApplicationFeatures/ApplicationFeature.h"
#include <string_view>

namespace arangodb {

namespace options { class ProgramOptions; }

class LicenseFeature final
    : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "License"; }

  explicit LicenseFeature(application_features::ApplicationServer& server);

  // Called from RestLicenseHandler to determine if JWT-only super-user access
  // is required for license operations. Return false for open-source behavior.
  bool onlySuperUser() const noexcept { return false; }

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override;
  void prepare() override;
  void start() override;
  void stop() override;
  void unprepare() override;
};

}  // namespace arangodb
```

### Pattern 2: Critical Phase 1 Stub — RocksDBEngineEE.h

`RocksDBEngine.h` unconditionally includes `Enterprise/RocksDBEngine/RocksDBEngineEE.h` inside `#ifdef USE_ENTERPRISE` (line 49) and declares a member `enterprise::RocksDBEngineEEData _eeData` (line 593). This stub MUST exist and MUST define the `enterprise::RocksDBEngineEEData` struct:

```cpp
// Enterprise/RocksDBEngine/RocksDBEngineEE.h
#pragma once
#include <rocksdb/env_encryption.h>

namespace arangodb {
namespace enterprise {

struct RocksDBEngineEEData {
  rocksdb::EncryptionProvider* _encryptionProvider = nullptr;
  // Additional fields will be added in Phase 2 (Encryption)
};

}  // namespace enterprise
}  // namespace arangodb
```

### Pattern 3: Critical Phase 1 Stub — Enterprise/Basics/Version.h

`lib/Rest/Version.h` includes `Enterprise/Basics/Version.h` inside `#ifdef USE_ENTERPRISE` and then immediately checks `#ifndef ARANGODB_ENTERPRISE_VERSION` with an `#error`. This file MUST define the macro:

```cpp
// Enterprise/Basics/Version.h
#pragma once

// OpenArangoDBCore — open-source Enterprise-compatible build
#define ARANGODB_ENTERPRISE_VERSION "OpenArangoDBCore 0.1.0"
```

### Pattern 4: arango_rclone Library Target

`arangod/RocksDBEngine/CMakeLists.txt` (line 100) links against `arango_rclone` when `USE_ENTERPRISE=1`. The enterprise CMakeLists.txt must define this target (even as empty):

```cmake
# In enterprise/CMakeLists.txt
add_library(arango_rclone STATIC
  Enterprise/RClone/RCloneFeature.cpp  # stub for now
)
target_include_directories(arango_rclone PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}
)
```

### Pattern 5: SslServerFeatureEE Registration

ArangoDB registers SslServerFeatureEE using a two-template form (confirmed in `arangod/RestServer/arangod.cpp` line 212):

```cpp
addFeature<SslServerFeature, SslServerFeatureEE>();
```

This means `SslServerFeatureEE` must be a subclass of `SslServerFeature` (found at `arangod/GeneralServer/SslServerFeature.h`). The Phase 1 stub must inherit from it and override `verifySslOptions()` and `createSslContexts()`:

```cpp
// Enterprise/Ssl/SslServerFeatureEE.h
#pragma once
#include "GeneralServer/SslServerFeature.h"

namespace arangodb {

class SslServerFeatureEE final : public SslServerFeature {
 public:
  static constexpr std::string_view name() noexcept { return "SslServer"; }
  explicit SslServerFeatureEE(application_features::ApplicationServer& server);
  void verifySslOptions() override;
  SslContextList createSslContexts() override;
};

}  // namespace arangodb
```

### Anti-Patterns to Avoid

- **Wrong directory prefix:** Using `arangod/License/` instead of `Enterprise/License/` — the include path resolution will fail silently until link time.
- **`namespace openarangodb`:** Every existing stub uses this. It must be replaced with the correct namespace before any build attempt.
- **Missing `static constexpr name()`:** Without this, the `ApplicationFeature` template constructor cannot compile.
- **Defining types that already exist in ArangoDB headers:** Never redefine `ApplicationServer`, `ProgramOptions`, or any ArangoDB community type. Forward-declare and include.
- **Separate RocksDB/VelocyPack installations:** Use `${CMAKE_SOURCE_DIR}/3rdParty/` headers exclusively.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| ODR violation detection | Custom duplicate-symbol checks | ASan with `detect_odr_violation=2` | Built into compiler; catches cases nm/objdump miss |
| Namespace verification | Manual symbol inspection | `nm -C` + grep in CI | Single command produces the full symbol table |
| Missing include errors | Guessing what headers ArangoDB needs | `grep -rh "#include.*Enterprise/" ${ARANGO_SRC}` | Produces authoritative list from source; already done in this research |
| ApplicationFeature lifecycle | Custom base class | `arangodb::application_features::ApplicationFeature` | ArangoDB's server calls lifecycle methods in defined order; diverging breaks server startup |
| Feature name strings | `std::string` with runtime value | `static constexpr std::string_view name()` | Template constraint: `ApplicationFeature(Server& server, const Impl&)` calls `Impl::name()` at compile time |

**Key insight:** The entire phase is about matching ArangoDB's existing contracts. Do not invent any interface — extract every signature from the ArangoDB source tree first.

---

## Common Pitfalls

### Pitfall 1: Directory Structure Mismatch (CRITICAL — blocks all other work)

**What goes wrong:** Placing headers at `arangod/License/LicenseFeature.h` instead of `Enterprise/License/LicenseFeature.h`. ArangoDB includes `"Enterprise/License/LicenseFeature.h"` and resolves it against `${PROJECT_SOURCE_DIR}/enterprise`. If the file is at `enterprise/arangod/License/` the include fails.

**Why it happens:** The current stub was generated with a mirror of ArangoDB's own directory layout. ArangoDB's community code lives at `arangod/`. Enterprise code must live at `enterprise/Enterprise/` to satisfy the `Enterprise/` prefix in include directives.

**How to avoid:** Run the authoritative verification:
```bash
grep -rh '#include.*Enterprise/' ${ARANGO_SRC}/ --include="*.h" --include="*.cpp" \
  | sed 's/.*#include "//' | sed 's/".*//' | grep '^Enterprise/' | sort -u
```
This produces the canonical file list. All 35 files must exist at those exact paths relative to the `enterprise/` repo root.

**Warning signs:** Any `fatal error: Enterprise/License/LicenseFeature.h: No such file or directory` during compilation.

### Pitfall 2: Namespace `openarangodb` (CRITICAL — produces link errors)

**What goes wrong:** Every current stub uses `namespace openarangodb`. ArangoDB expects `namespace arangodb`. The linker will emit `undefined reference to arangodb::LicenseFeature::LicenseFeature(...)` while the object file provides `openarangodb::LicenseFeature::LicenseFeature(...)`.

**How to avoid:** Before renaming, produce the authoritative namespace map by checking how ArangoDB calls each feature:
- `server().getFeature<arangodb::LicenseFeature>()` — namespace `arangodb`
- `arangodb::enterprise::EncryptionProvider` — namespace `arangodb::enterprise`
- `addFeature<LicenseFeature>()` within `using namespace arangodb` context — namespace `arangodb`

**Confirmed namespaces from source inspection:**
| Class | Namespace |
|-------|-----------|
| `LicenseFeature` | `arangodb` |
| `EncryptionFeature` | `arangodb` |
| `AuditFeature` | `arangodb` |
| `HotBackupFeature` | `arangodb` |
| `RCloneFeature` | `arangodb` |
| `SslServerFeatureEE` | `arangodb` |
| `EncryptionProvider` | `arangodb::enterprise` |
| `RocksDBEngineEEData` | `arangodb::enterprise` |
| `EncryptionSecret` | `arangodb::enterprise` |

**Warning signs:** Link errors with `arangodb::` prefix in the unresolved symbol name.

### Pitfall 3: Missing `ARANGODB_ENTERPRISE_VERSION` Macro

**What goes wrong:** `lib/Rest/Version.h` does `#include "Enterprise/Basics/Version.h"` and immediately checks `#ifndef ARANGODB_ENTERPRISE_VERSION` with `#error "Enterprise Edition version number is not defined"`. If `Enterprise/Basics/Version.h` does not define this macro, compilation fails with a hard error.

**How to avoid:** Create `Enterprise/Basics/Version.h` as the very first file — it is the simplest stub and must exist before any other compilation unit can proceed.

### Pitfall 4: Vtable ODR Violation from Incorrect `override`

**What goes wrong:** If `LicenseFeature::collectOptions` has the wrong signature (e.g., wrong `const`, different parameter type), the compiler creates a new virtual instead of overriding the base's virtual. The base's pure virtual remains unimplemented, causing a link error or runtime abort with "pure virtual method called".

**How to avoid:**
- Always use `override` on every virtual. The compiler will reject a mismatched signature as an error.
- Add `static_assert(!std::is_abstract_v<LicenseFeature>)` in the .cpp file.
- Compile enterprise headers together with ArangoDB's headers from the first compilation.
- Add `-Wall -Woverloaded-virtual -Werror` to CMakeLists.txt.

### Pitfall 5: CMake Link Order (GNU ld symbol starvation)

**What goes wrong:** On Linux with GNU ld, static libraries are scanned once, left to right. If `openarangodb_enterprise` appears before the translation units that reference its symbols, those symbols are not included in the final binary.

**How to avoid:** In ArangoDB's enterprise CMakeLists.txt, use `target_sources` to add enterprise sources directly to existing ArangoDB library targets, or ensure the enterprise library target is listed after the referencing libraries:
```cmake
target_link_libraries(arangoserver PRIVATE openarangodb_enterprise)
```
Or add a minimal link test (one symbol per module) to Phase 1 CI to catch this before Phase 2 adds more symbols.

### Pitfall 6: ODR Violation from Duplicate Symbol Definitions

**What goes wrong:** If OpenArangoDBCore redefines any class, struct, or inline function already present in ArangoDB's community headers, the linker silently picks one definition. Memory layouts diverge causing undefined behavior with no error message.

**How to avoid:**
- Enable `ASAN_OPTIONS=detect_odr_violation=2` in CI from Phase 1 — not retroactively.
- Enable `-Wl,--warn-common` on Linux in the CMake flags.
- Never copy-paste class definitions from ArangoDB headers. Forward-declare and include instead.

---

## Code Examples

Verified patterns from ArangoDB source inspection:

### Feature Registration in arangod.cpp (source: arangod/RestServer/arangod.cpp)
```cpp
// Lines 206-215
#ifdef USE_ENTERPRISE
  addFeature<AuditFeature>();
  addFeature<LicenseFeature>();
  addFeature<RCloneFeature>();
  addFeature<HotBackupFeature>();
  addFeature<EncryptionFeature>();
  addFeature<SslServerFeature, SslServerFeatureEE>();  // replaces community SslServerFeature
#else
  addFeature<SslServerFeature>();
#endif
```

### ApplicationFeature Constructor Pattern (source: lib/ApplicationFeatures/ApplicationFeature.h)
```cpp
// Feature must have: static constexpr std::string_view name() noexcept
// Base constructor is called via template:
template<typename Server, typename Impl>
ApplicationFeature(Server& server, const Impl&)
    : ApplicationFeature{server, typeid(Impl), Impl::name()} {}
```

### Enterprise Include Path Pattern (source: arangod/RestServer/arangod_includes.h lines 165-172)
```cpp
#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#include "Enterprise/License/LicenseFeature.h"
#include "Enterprise/RClone/RCloneFeature.h"
#include "Enterprise/Ssl/SslServerFeatureEE.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif
// Files resolve against: ${PROJECT_SOURCE_DIR}/enterprise/Enterprise/...
```

### RocksDB Enterprise Hook Signatures (source: arangod/RocksDBEngine/RocksDBEngine.h lines 583-586)
```cpp
// These are MEMBER methods of RocksDBEngine — NOT free functions
#ifdef USE_ENTERPRISE
  void collectEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void validateEnterpriseOptions(std::shared_ptr<options::ProgramOptions>);
  void prepareEnterprise();
  void configureEnterpriseRocksDBOptions(rocksdb::DBOptions& options, bool createdEngineDir);
  enterprise::RocksDBEngineEEData _eeData;  // member field — must exist in stub
#endif
```

### LicenseFeature Method Called by RestLicenseHandler (source: arangod/RestHandler/RestLicenseHandler.cpp line 84-88)
```cpp
// This is exactly what ArangoDB calls at runtime:
auto& feature = server().getFeature<arangodb::LicenseFeature>();
if (feature.onlySuperUser()) {
  if (!ExecContext::current().isSuperuser()) { /* deny */ }
}
// Return false from onlySuperUser() for open-source behavior
```

### CMake Enterprise Library Integration (source: root CMakeLists.txt lines 1280-1283)
```cmake
# ArangoDB's root CMakeLists.txt:
if(USE_ENTERPRISE)
  add_definitions("-DUSE_ENTERPRISE=1")
  add_subdirectory(enterprise)  # enterprise/ CMakeLists.txt must provide arango_rclone target
endif()
```

### ASAN CI Configuration
```cmake
# In CI cmake configuration:
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
# In CI environment:
# ASAN_OPTIONS=detect_odr_violation=2
```

---

## Complete File Inventory (Phase 1 Must Create/Fix)

### Files that must exist and be correct for Phase 1 compilation

The following 35 files are required by ArangoDB's `#include "Enterprise/"` directives (confirmed by scanning the source tree). Files marked **PHASE 1** must exist with correct content before the build compiles at all.

| File | Phase 1 Action | Reason |
|------|---------------|--------|
| `Enterprise/Basics/Version.h` | CREATE — define `ARANGODB_ENTERPRISE_VERSION` | Hard `#error` if missing |
| `Enterprise/License/LicenseFeature.h` | CREATE — full `arangodb::LicenseFeature` class | Called by `RestLicenseHandler` and `arangod.cpp` |
| `Enterprise/RocksDBEngine/RocksDBEngineEE.h` | CREATE — define `enterprise::RocksDBEngineEEData` struct | Included unconditionally in `RocksDBEngine.h` |
| `Enterprise/Audit/AuditFeature.h` | CREATE — minimal `arangodb::AuditFeature` stub | Included unconditionally in `arangod_includes.h` |
| `Enterprise/Encryption/EncryptionFeature.h` | CREATE — minimal `arangodb::EncryptionFeature` stub | Included unconditionally in `arangod_includes.h` |
| `Enterprise/RClone/RCloneFeature.h` | CREATE — minimal `arangodb::RCloneFeature` stub | Included unconditionally in `arangod_includes.h` |
| `Enterprise/Ssl/SslServerFeatureEE.h` | CREATE — `arangodb::SslServerFeatureEE : SslServerFeature` | Included unconditionally in `arangod_includes.h` |
| `Enterprise/StorageEngine/HotBackupFeature.h` | CREATE — minimal `arangodb::HotBackupFeature` stub | Included unconditionally in `arangod_includes.h`; `addFeature<HotBackupFeature>()` |
| `Enterprise/RocksDBEngine/EncryptionProvider.h` | CREATE — `arangodb::enterprise::EncryptionProvider` stub | Used in `RocksDBTempStorage.h` |
| `Enterprise/RocksDBEngine/RocksDBEncryptionUtilsEE.h` | CREATE — stub | Included in `RocksDBTempStorage.cpp` |
| `Enterprise/RocksDBEngine/RocksDBBuilderIndexEE.h` | CREATE — stub | Included in `RocksDBBuilderIndex.cpp` |
| `Enterprise/RocksDBEngine/RocksDBHotBackup/RocksDBHotBackup.h` | CREATE — stub | Included in `ClusterMethods.cpp` |
| `Enterprise/RestHandler/RestHotBackupHandler.h` | CREATE — stub | Included in `GeneralServerFeature.cpp` |
| `Enterprise/Sharding/ShardingStrategyEE.h` | CREATE — stub | Included in `LogicalCollection.cpp` |
| `Enterprise/VocBase/SmartGraphSchema.h` | CREATE — stub | Included in various files |
| `Enterprise/VocBase/VirtualClusterSmartEdgeCollection.h` | CREATE — stub | Included in `LogicalCollection.cpp`, `Transaction/Manager.cpp`, `ClusterMethods.cpp` |
| `Enterprise/Aql/LocalTraversalNode.h` | CREATE — stub | Included in graph files |
| `Enterprise/Aql/LocalEnumeratePathsNode.h` | CREATE — stub | Included in graph files |
| `Enterprise/Aql/LocalGraphNode.h` | CREATE — stub | Included in graph files |
| `Enterprise/Aql/LocalShortestPathNode.h` | CREATE — stub | Included in graph files |
| `Enterprise/Graph/Providers/SmartGraphProvider.h` | CREATE — stub | Included in multiple graph files |
| `Enterprise/Graph/Providers/SmartGraphRPCCommunicator.h` | CREATE — stub | Included in `BaseProviderOptions.h` |
| `Enterprise/Graph/Providers/SingleServerProviderEE.tpp` | CREATE — stub `.tpp` | Included at end of `SingleServerProvider.cpp` |
| `Enterprise/Graph/Steps/SmartGraphStep.h` | CREATE — stub | Included in multiple graph files |
| `Enterprise/Graph/Enumerators/OneSidedEnumeratorEE.tpp` | CREATE — stub `.tpp` | Included at end of `OneSidedEnumerator.cpp` |
| `Enterprise/Graph/PathValidatorEE.cpp` | CREATE — stub `.cpp` included | Included in `PathValidator.cpp` |
| `Enterprise/IResearch/GeoAnalyzerEE.h` | CREATE — stub | IResearch dependency |
| `Enterprise/IResearch/IResearchAnalyzerFeature.h` | CREATE — stub | IResearch dependency |
| `Enterprise/IResearch/IResearchDataStoreEE.hpp` | CREATE — stub | IResearch dependency |
| `Enterprise/IResearch/IResearchDocumentEE.h` | CREATE — stub | IResearch dependency |
| `Enterprise/IResearch/IResearchDocumentEE.hpp` | CREATE — stub | IResearch dependency |
| `Enterprise/IResearch/IResearchOptimizeTopK.h` | CREATE — stub | IResearch dependency |
| `Enterprise/Maskings/AttributeMaskingEE.h` | CREATE — stub | Masking dependency |
| `Enterprise/Transaction/IgnoreNoAccessAqlTransaction.h` | CREATE — stub | Used in `TraverserEngine.cpp` |
| `Enterprise/Transaction/IgnoreNoAccessMethods.h` | CREATE — stub | Used in `TraverserEngine.cpp` |

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Free function hooks for RocksDB enterprise options | Member methods of `RocksDBEngine` (`collectEnterpriseOptions`, `prepareEnterprise`, `configureEnterpriseRocksDBOptions`) | ArangoDB 3.10+ | Must NOT implement as free functions — they are called as `this->configureEnterpriseRocksDBOptions(...)` |
| Custom enterprise namespace | `namespace arangodb` for features, `namespace arangodb::enterprise` for implementation details | Always | Determines link symbol names |
| Loose file structure | Strict `Enterprise/` subdirectory prefix required | Always | The `ENTERPRISE_INCLUDE_DIR = "enterprise"` + `Enterprise/` prefix makes all paths double-prefixed |

**Deprecated/outdated:**
- `namespace openarangodb` — never correct for ArangoDB drop-in; all existing stubs use this and must be changed.
- `arangod/` directory prefix in this repo — must be `Enterprise/`.

---

## Open Questions

1. **What does `Enterprise/RocksDBEngine/EncryptionProvider.h` declare exactly?**
   - What we know: `RocksDBTempStorage.cpp` creates `std::make_shared<enterprise::EncryptionProvider>(...)` and tests use `arangodb::enterprise::EncryptionProvider hwprovider(...)` with what appears to be a key argument.
   - What's unclear: The exact constructor signature, whether it derives from `rocksdb::EncryptionProvider` directly or wraps it.
   - Recommendation: For Phase 1 stub, forward-declare the struct with default constructor only; Phase 2 research will extract the full interface.

2. **What additional methods does `LicenseFeature` expose beyond `onlySuperUser()`?**
   - What we know: `RestLicenseHandler` only calls `onlySuperUser()`. The `GET /_admin/license` REST endpoint in the Enterprise edition returns full license information.
   - What's unclear: Whether there are other callers of `LicenseFeature` methods in the ArangoDB source beyond what's visible in community files.
   - Recommendation: Search `grep -rn "getFeature<LicenseFeature>\|getFeature<arangodb::LicenseFeature>" ${ARANGO_SRC}/` at the start of Phase 1 implementation to find all call sites.

3. **What does `Enterprise/StorageEngine/HotBackupFeature.h` need to declare?**
   - What we know: `arangod.cpp` adds `HotBackupFeature` as a feature; `GeneralServerFeature.cpp` calls `getFeature<HotBackupFeature>()` and accesses `backup.{method}()`.
   - What's unclear: Which methods are called on `HotBackupFeature` from community code.
   - Recommendation: Run `grep -n "HotBackupFeature\|hotBackupFeature\." ${ARANGO_SRC}/arangod/` before writing the stub.

4. **How does the enterprise CMakeLists.txt integrate sources into `arangoserver`?**
   - What we know: `arangod/arangoserver.cmake` does not mention `USE_ENTERPRISE`; `tests/CMakeLists.txt` links `arango_rclone`; `RocksDBEngine/CMakeLists.txt` links `arango_rclone`. The enterprise `CMakeLists.txt` must define `arango_rclone` and possibly `add_subdirectory` sub-libraries.
   - What's unclear: Whether enterprise `.cpp` files are added via `target_sources(arangoserver ...)` calls or defined as a separate library linked against `arangoserver`.
   - Recommendation: The Phase 1 approach is to create a minimal `openarangodb_enterprise` STATIC library containing stub `.cpp` files, and define `arango_rclone` as a separate STATIC library. Later phases add sources to these targets.

---

## Validation Architecture

> `workflow.nyquist_validation` key is absent from `.planning/config.json` — treating as enabled.

### Test Framework
| Property | Value |
|----------|-------|
| Framework | GoogleTest 1.17.0 + GMock |
| Config file | `tests/CMakeLists.txt` (Wave 0 gap — does not exist yet) |
| Quick run command | `ctest --test-dir build/tests -R "foundation" -V` |
| Full suite command | `ctest --test-dir build/tests -V` |

### Phase Requirements to Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| FOUND-01 | All symbols use `namespace arangodb` | CI audit | `nm -C build/libopenarangodb_enterprise.a \| grep "openarangodb"` exits non-zero | Wave 0 CI script |
| FOUND-02 | CMake with `-DUSE_ENTERPRISE=1` produces binary | build/smoke | `cmake -DUSE_ENTERPRISE=1 ... && cmake --build . --target arangod` | Wave 0 |
| FOUND-03 | Library links without unresolved symbols | build/link | `nm -u build/arangod \| grep "enterprise\|LicenseFeature"` (should be empty) | Wave 0 |
| FOUND-04 | LicenseFeature capability checks return correct values | unit | `ctest -R "LicenseFeatureTest" -V` | Wave 0 |
| FOUND-05 | AddressSanitizer clean build | CI/ASan | `ASAN_OPTIONS=detect_odr_violation=2 ./arangod --help` | Wave 0 CI |

### Sampling Rate
- **Per task commit:** `nm -C build/Enterprise/License/CMakeFiles/*.o | grep "openarangodb"` (namespace check)
- **Per wave merge:** Full build + ASan smoke test
- **Phase gate:** Full build with ASan exits clean before `/gsd:verify-work`

### Wave 0 Gaps
- [ ] `tests/CMakeLists.txt` — test infrastructure file; covers FOUND-04
- [ ] `tests/unit/LicenseFeatureTest.cpp` — unit test for `onlySuperUser()` and lifecycle methods; covers FOUND-04
- [ ] `ci/build-with-asan.sh` — CI script running full build + ASan; covers FOUND-05
- [ ] `ci/symbol-audit.sh` — runs `nm -C` scan for `openarangodb` symbols; covers FOUND-01, FOUND-03
- [ ] Framework install: `cmake -S . -B build -DCMAKE_CXX_FLAGS="-fsanitize=address"` — if not present in CI config

---

## Sources

### Primary (HIGH confidence — verified from local ArangoDB source)
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/CMakeLists.txt` lines 200, 1280-1283 — `ENTERPRISE_INCLUDE_DIR = "enterprise"`, `add_subdirectory(enterprise)` pattern, `add_definitions("-DUSE_ENTERPRISE=1")`
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/RestServer/arangod_includes.h` lines 165-172 — all 6 enterprise feature includes with `Enterprise/` prefix
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/RestServer/arangod.cpp` lines 206-215 — `addFeature<LicenseFeature>()`, `addFeature<SslServerFeature, SslServerFeatureEE>()` registration pattern
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/RestHandler/RestLicenseHandler.cpp` lines 84-88 — `arangodb::LicenseFeature` namespace confirmation, `onlySuperUser()` method call
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/RocksDBEngine/RocksDBEngine.h` lines 48-49, 477-478, 583-594 — enterprise header include, `configureEnterpriseRocksDBOptions` member method signature, `_eeData` member
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/RocksDBEngine/RocksDBTempStorage.h` lines 45-49 — `arangodb::enterprise` namespace forward declaration for `EncryptionProvider`
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/tests/RocksDBEngine/EncryptionProviderTest.cpp` lines 50, 55 — `arangodb::enterprise::EncryptionProvider` confirmed namespace
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/lib/Rest/Version.h` lines 34-40 — `Enterprise/Basics/Version.h` include and `ARANGODB_ENTERPRISE_VERSION` hard error
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/lib/ApplicationFeatures/ApplicationFeature.h` lines 41-168 — `ApplicationFeature` class definition, template constructor pattern
- `/Users/aarjay/Documents/OpenArangoDB/arangodb-core/arangod/GeneralServer/SslServerFeature.h` lines 41-47 — `SslServerFeature` class definition confirming `SslServerFeatureEE` must extend it
- Full `#include "Enterprise/"` scan of ArangoDB source — 35 required file paths confirmed

### Secondary (MEDIUM confidence — project-level research)
- `.planning/research/SUMMARY.md` — namespace analysis, build dependency graph, pitfall list
- `.planning/research/ARCHITECTURE.md` — ApplicationFeature lifecycle, RocksDB hook pattern
- `.planning/research/STACK.md` — C++20 standard confirmation, CMake version, testing stack

---

## Metadata

**Confidence breakdown:**
- Directory structure: HIGH — verified by direct source scan of all `#include "Enterprise/"` directives
- Namespace hierarchy: HIGH — verified from specific qualified names in ArangoDB source (`arangodb::LicenseFeature`, `arangodb::enterprise::EncryptionProvider`)
- CMake integration: HIGH — verified from `CMakeLists.txt` lines and `ENTERPRISE_INCLUDE_DIR` usage across 16+ target definitions
- LicenseFeature interface: HIGH — `onlySuperUser()` confirmed in `RestLicenseHandler.cpp`; additional methods LOW until full header is seen
- File inventory: HIGH — authoritative list from grep scan of entire source tree
- ASan/ODR setup: HIGH — standard compiler flags, no ArangoDB-specific configuration

**Research date:** 2026-03-30
**Valid until:** 2026-07-01 (stable ArangoDB build system; verify against devel branch if ArangoDB version changes)
