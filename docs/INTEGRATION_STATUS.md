# Integration Build Status

## Summary

OpenArangoDBCore v1.0 integration testing against ArangoDB v3.12.0 source tree.

**Date:** 2026-04-01
**Platform:** Docker (Ubuntu 22.04, GCC 12, x86_64)

## Results

| Stage | Status | Notes |
|-------|--------|-------|
| CMake configure | PASS | `add_subdirectory(enterprise)` processes correctly |
| Generated headers | PASS | `errorfiles` dependency ensures `voc-errors.h` is available |
| Include resolution | PASS | All ArangoDB internal headers found via integration include paths |
| Enterprise lib compilation | PARTIAL | Mock API surface differs from real ArangoDB APIs (see below) |
| V8 build (ARM64) | FAIL (upstream) | ArangoDB's V8 doesn't compile on aarch64/GCC 12 |
| Standalone unit tests | PASS | 565/565 tests, 43 binaries, 0 failures |
| Benchmarks | PASS | 6 suites run successfully |

## What Works

1. **Build system integration** — Our `CMakeLists.txt` correctly detects when built inside ArangoDB's source tree, adds proper include paths, and depends on generated headers.

2. **Standalone compilation** — All 19 Enterprise modules compile and pass tests against our mock headers.

3. **Library targets** — Both `openarangodb_enterprise` and `arango_rclone` are recognized by ArangoDB's build system.

## What Needs Work (v1.1)

### API Surface Adaptation

Our implementations were written against simplified mock headers in `tests/mocks/`. The real ArangoDB headers have more complex APIs:

- **`ProgramOptions::addOption()`** — Real API has different overloads than our mock
- **`ApplicationFeature` constructor** — Real base class uses CRTP pattern with `id<Impl>`
- **`environ` linkage** — Symbol conflict between our code and system headers on Linux

These are straightforward fixes — each file needs its `#include` and API call sites updated to match the real ArangoDB headers. The logic remains the same.

### V8 ARM64 Build

ArangoDB's bundled V8 doesn't compile on ARM64 with GCC 12. This is an upstream issue, not related to our code. Workaround: build on x86_64 (`--platform linux/amd64`).

## How to Run

```bash
# Docker (recommended)
docker build -f scripts/Dockerfile.integration -t oac-integration .

# Native Linux (x86_64, GCC 12+)
MACOSX_DEPLOYMENT_TARGET=15.0 bash scripts/integration-test.sh

# With existing ArangoDB source
ARANGODB_SRC=/path/to/arangodb bash scripts/integration-test.sh
```

## Next Steps

1. Adapt API call sites to match real ArangoDB v3.12.0 headers
2. Build on x86_64 Linux (CI) to avoid V8/ARM64 issue
3. Add GitHub Actions job for integration builds
