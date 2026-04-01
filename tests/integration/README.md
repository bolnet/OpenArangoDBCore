# Integration Testing for OpenArangoDBCore

## Overview

Integration tests verify that OpenArangoDBCore compiles, links, and runs correctly
as a drop-in replacement for ArangoDB's Enterprise directory. Unlike unit tests
(which use mocks and run in isolation), integration tests build against the real
ArangoDB source tree and exercise end-to-end behavior.

## Architecture

```
                   +--------------------------+
                   |   ArangoDB Source Tree    |
                   |   (cloned from GitHub)    |
                   +------------+-------------+
                                |
                   symlink: enterprise/ -> OpenArangoDBCore/Enterprise/
                                |
                   +------------v-------------+
                   |  cmake -DUSE_ENTERPRISE=1 |
                   +------------+-------------+
                                |
                   +------------v-------------+
                   |      arangod binary       |
                   +------------+-------------+
                                |
              +-----------------+-----------------+
              |                 |                 |
        Symbol check      HTTP smoke test   Feature tests
        (nm / ldd)        (license, CRUD)   (encryption,
                                             SmartGraph,
                                             audit, backup)
```

## Test Layers

### Layer 1: Build Verification

Confirms that OpenArangoDBCore compiles and links against the ArangoDB source
without unresolved symbols. This catches API drift between our stubs and the
real ArangoDB headers.

- Script: `scripts/build-with-arangodb.sh`
- What it checks: cmake configure, compile, link (no unresolved symbols)

### Layer 2: Runtime Smoke Test

Starts an arangod instance and verifies basic enterprise behavior through HTTP.

- Script: `scripts/integration-test.sh`
- What it checks:
  - `/_admin/license` returns `enterprise: true`
  - Collection CRUD (create, insert, AQL query)
  - Enterprise CLI options accepted (encryption key, audit output)

### Layer 3: Enterprise Feature Tests

Exercises enterprise-specific features against a running arangod.

- Script: `tests/integration/smoke_test.py`
- What it checks:
  - SmartGraph creation with `smartGraphAttribute`
  - `MINHASH()` AQL function availability
  - Hot backup create / list
  - Audit logging configuration

## Prerequisites

| Dependency  | Version   | Purpose                        |
|-------------|-----------|--------------------------------|
| cmake       | >= 3.20   | Build system                   |
| C++ compiler| C++20     | Compile ArangoDB + enterprise  |
| Python 3    | >= 3.8    | Smoke test script              |
| requests    | any       | HTTP client for smoke tests    |
| curl        | any       | Used by shell scripts          |
| git         | any       | Clone ArangoDB source          |

Optional:
- `python-arango` -- used by smoke_test.py if available, falls back to raw HTTP

## Quick Start

### Run the full integration test suite

```bash
# Full build + test (takes 30-60 minutes on first run)
./scripts/integration-test.sh

# Specify ArangoDB branch
ARANGODB_BRANCH=v3.12.0 ./scripts/integration-test.sh

# Use an existing ArangoDB source directory
ARANGODB_SRC=/path/to/arangodb ./scripts/integration-test.sh
```

### Build only (no runtime tests)

```bash
./scripts/build-with-arangodb.sh
```

### Run smoke tests against an already-running arangod

```bash
# Default: localhost:8529
python3 tests/integration/smoke_test.py

# Custom endpoint
ARANGODB_ENDPOINT=http://localhost:9529 python3 tests/integration/smoke_test.py
```

## Environment Variables

| Variable            | Default                              | Description                          |
|---------------------|--------------------------------------|--------------------------------------|
| `ARANGODB_BRANCH`  | `v3.12.0`                            | ArangoDB git branch or tag to clone  |
| `ARANGODB_REPO`    | `https://github.com/arangodb/arangodb.git` | ArangoDB git repository URL   |
| `ARANGODB_SRC`     | (auto-created in /tmp)               | Path to existing ArangoDB source     |
| `ARANGODB_ENDPOINT`| `http://127.0.0.1:8529`             | Endpoint for smoke_test.py           |
| `BUILD_DIR`        | `${ARANGODB_SRC}/build`             | CMake build directory                |
| `BUILD_JOBS`       | `$(nproc)`                           | Parallel build jobs                  |
| `SKIP_BUILD`       | (unset)                              | Set to 1 to skip build, run tests only |
| `KEEP_WORKDIR`     | (unset)                              | Set to 1 to preserve temp directories |

## CI Integration

These scripts return standard exit codes (0 = pass, 1 = fail) and are designed
for use in CI pipelines. Example GitHub Actions workflow:

```yaml
jobs:
  integration:
    runs-on: ubuntu-latest
    timeout-minutes: 90
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++ python3 python3-requests curl
      - name: Run integration tests
        run: ./scripts/integration-test.sh
```

## Troubleshooting

**Build fails with missing headers**: Ensure the ArangoDB branch matches the
API surface that OpenArangoDBCore targets. Check `ARANGODB_BRANCH`.

**Unresolved symbols at link time**: A new ArangoDB API was introduced that
OpenArangoDBCore does not yet implement. Check `nm` output in the build log.

**arangod fails to start**: Check the log file at `${BUILD_DIR}/arangod.log`.
Common causes: port 8529 already in use, insufficient permissions for data dir.

**Smoke test timeouts**: arangod may need more time to initialize. The scripts
wait up to 60 seconds; increase `STARTUP_TIMEOUT` if needed.
