# OpenArangoDBCore

## What This Is

Open-source C++ implementation of ArangoDB Enterprise features. A drop-in replacement for the proprietary `Enterprise/` module — clone it as the `enterprise/` directory, build ArangoDB with `-DUSE_ENTERPRISE=1`, and get Enterprise capabilities without the license.

## Core Value

Every Enterprise-gated feature compiles and links correctly against ArangoDB, providing functionally equivalent open-source alternatives to the proprietary Enterprise module.

## Current Milestone: v1.0 Full Enterprise Parity

**Goal:** Implement all 19 Enterprise-equivalent modules so OpenArangoDBCore is a complete drop-in replacement.

**Target features:**
- Security: Audit Logging, Encryption at Rest, LDAP Auth, External RBAC, Data Masking, Enhanced SSL/TLS
- Graph & Cluster: SmartGraphs, Disjoint SmartGraphs, SatelliteGraphs, SmartToSat Edge Validation, Shard-local Graph Execution, ReadFromFollower
- Search: MinHash Similarity, TopK Query Optimization
- Replication: DC-to-DC Replication
- Backup & Ops: Hot Backup, Cloud Backup (RClone), Parallel Index Building
- Infrastructure: Open License Feature (always enabled)

## Requirements

### Validated

(None yet — ship to validate)

### Active

- [ ] All 19 Enterprise modules implemented with correct ABI/header compatibility
- [ ] Compiles against ArangoDB latest stable with `-DUSE_ENTERPRISE=1`
- [ ] Unit tests for each module
- [ ] Integration tests verifying ArangoDB linkage

### Out of Scope

- GUI / Web UI changes — C++ core only
- ArangoDB fork maintenance — we provide the Enterprise directory, not ArangoDB itself
- Performance parity benchmarks — functional correctness first
- Python OpenArangoDB integration — separate project

## Context

- ArangoDB gates Enterprise features behind `#ifdef USE_ENTERPRISE` preprocessor checks
- The proprietary code lives in an `enterprise/` directory compiled when `-DUSE_ENTERPRISE=1`
- OpenArangoDBCore provides matching headers and symbols so ArangoDB links against our implementations
- Project structure mirrors ArangoDB's enterprise directory layout
- All 19 modules have stub files (.h + .cpp) already created
- Related Python project (OpenArangoDB) implements similar features at the application layer

## Constraints

- **C++20**: Required for ArangoDB compatibility (GCC 12+, Clang 15+, MSVC 2022+)
- **CMake 3.20+**: Build system must integrate with ArangoDB's cmake
- **ABI Compatibility**: Headers must match ArangoDB's expected Enterprise symbols exactly
- **Dependencies**: OpenSSL 3.0+ (encryption), ldap library (auth), RocksDB (bundled with ArangoDB)
- **License**: Apache 2.0

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Mirror ArangoDB enterprise/ layout | Drop-in replacement requires same paths | -- Pending |
| C++20 standard | Match ArangoDB's compiler requirements | -- Pending |
| Static library (openarangodb_enterprise) | Links into ArangoDB binary at build time | -- Pending |

---
*Last updated: 2026-03-29 after milestone v1.0 initialization*
