#!/usr/bin/env bash
# =============================================================================
# build-with-arangodb.sh
#
# Downloads ArangoDB source and builds it with OpenArangoDBCore as the
# enterprise directory. Reports build success or failure.
#
# This is the build-only counterpart to integration-test.sh -- it does not
# start arangod or run any runtime tests.
#
# Environment variables:
#   ARANGODB_REPO     -- Git repo URL (default: https://github.com/arangodb/arangodb.git)
#   ARANGODB_BRANCH   -- Branch or tag (default: v3.12.0)
#   ARANGODB_SRC      -- Path to existing ArangoDB source (skip clone)
#   BUILD_DIR         -- CMake build directory (default: ${ARANGODB_SRC}/build)
#   BUILD_JOBS        -- Parallel jobs (default: nproc)
#   BUILD_TARGET      -- CMake target (default: arangod)
#   CMAKE_BUILD_TYPE  -- Build type (default: RelWithDebInfo)
#   KEEP_WORKDIR      -- Set to 1 to preserve temp directories
# =============================================================================

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

ARANGODB_REPO="${ARANGODB_REPO:-https://github.com/arangodb/arangodb.git}"
ARANGODB_BRANCH="${ARANGODB_BRANCH:-v3.12.0}"
BUILD_JOBS="${BUILD_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"
BUILD_TARGET="${BUILD_TARGET:-arangod}"
CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"
KEEP_WORKDIR="${KEEP_WORKDIR:-}"

# ---------------------------------------------------------------------------
# Colors
# ---------------------------------------------------------------------------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
RESET='\033[0m'

log_info()    { echo -e "${BLUE}[INFO]${RESET}  $*"; }
log_success() { echo -e "${GREEN}[OK]${RESET}    $*"; }
log_error()   { echo -e "${RED}[ERROR]${RESET} $*"; }
log_section() { echo -e "\n${BOLD}--- $* ---${RESET}\n"; }

# ---------------------------------------------------------------------------
# Cleanup
# ---------------------------------------------------------------------------
cleanup() {
    local exit_code=$?
    if [[ -z "${KEEP_WORKDIR}" && -n "${WORKDIR:-}" && "${WORKDIR_CREATED:-false}" == "true" ]]; then
        log_info "Cleaning up temp directory: ${WORKDIR}"
        rm -rf "${WORKDIR}"
    fi
    exit "${exit_code}"
}
trap cleanup EXIT

# ---------------------------------------------------------------------------
# Preflight checks
# ---------------------------------------------------------------------------
log_section "Preflight Checks"

for cmd in git cmake; do
    if ! command -v "${cmd}" &>/dev/null; then
        log_error "Required command not found: ${cmd}"
        exit 1
    fi
done

# Verify a C++ compiler is available
CXX="${CXX:-$(command -v g++ || command -v clang++ || echo "")}"
if [[ -z "${CXX}" ]]; then
    log_error "No C++ compiler found. Install g++ or clang++."
    exit 1
fi

log_success "git:    $(git --version)"
log_success "cmake:  $(cmake --version | head -1)"
log_success "C++:    $(${CXX} --version | head -1)"
log_info "Project root: ${PROJECT_ROOT}"

# ---------------------------------------------------------------------------
# Step 1: Obtain ArangoDB source
# ---------------------------------------------------------------------------
log_section "Step 1: Obtain ArangoDB Source"

WORKDIR_CREATED=false

if [[ -n "${ARANGODB_SRC:-}" && -d "${ARANGODB_SRC}" ]]; then
    log_info "Using existing ArangoDB source: ${ARANGODB_SRC}"
    WORKDIR="$(dirname "${ARANGODB_SRC}")"
else
    WORKDIR="$(mktemp -d /tmp/oac-build.XXXXXX)"
    WORKDIR_CREATED=true
    ARANGODB_SRC="${WORKDIR}/arangodb"

    log_info "Cloning ArangoDB (${ARANGODB_BRANCH})..."
    log_info "Repository: ${ARANGODB_REPO}"
    log_info "Destination: ${ARANGODB_SRC}"

    if ! git clone --depth 1 --branch "${ARANGODB_BRANCH}" "${ARANGODB_REPO}" "${ARANGODB_SRC}"; then
        log_error "Failed to clone ArangoDB repository."
        log_info "Verify the branch/tag '${ARANGODB_BRANCH}' exists."
        exit 1
    fi

    log_success "ArangoDB source cloned."
fi

# ---------------------------------------------------------------------------
# Step 2: Link OpenArangoDBCore as enterprise/
# ---------------------------------------------------------------------------
log_section "Step 2: Link Enterprise Directory"

ENTERPRISE_DIR="${ARANGODB_SRC}/enterprise"

if [[ -e "${ENTERPRISE_DIR}" || -L "${ENTERPRISE_DIR}" ]]; then
    log_info "Removing existing enterprise directory/symlink..."
    rm -rf "${ENTERPRISE_DIR}"
fi

ln -s "${PROJECT_ROOT}" "${ENTERPRISE_DIR}"
log_success "Symlinked: ${ENTERPRISE_DIR} -> ${PROJECT_ROOT}"

# Sanity check
if [[ ! -d "${ENTERPRISE_DIR}/Enterprise" ]]; then
    log_error "Enterprise/ subdirectory not accessible through symlink."
    exit 1
fi
log_success "Enterprise/ directory structure verified."

# ---------------------------------------------------------------------------
# Step 3: Configure CMake
# ---------------------------------------------------------------------------
log_section "Step 3: CMake Configure"

BUILD_DIR="${BUILD_DIR:-${ARANGODB_SRC}/build}"
mkdir -p "${BUILD_DIR}"

log_info "Build type:  ${CMAKE_BUILD_TYPE}"
log_info "Build dir:   ${BUILD_DIR}"
log_info "Target:      ${BUILD_TARGET}"
log_info "Jobs:        ${BUILD_JOBS}"

CMAKE_LOG="${BUILD_DIR}/cmake-configure.log"

log_info "Running cmake configure (log: ${CMAKE_LOG})..."
if cmake -S "${ARANGODB_SRC}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DUSE_ENTERPRISE=1 \
    -DUSE_MAINTAINER_MODE=OFF \
    > "${CMAKE_LOG}" 2>&1; then
    log_success "CMake configuration succeeded."
else
    log_error "CMake configuration failed. Last 30 lines of log:"
    tail -30 "${CMAKE_LOG}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 4: Build
# ---------------------------------------------------------------------------
log_section "Step 4: Build"

BUILD_LOG="${BUILD_DIR}/cmake-build.log"

log_info "Building target '${BUILD_TARGET}' with ${BUILD_JOBS} jobs..."
log_info "Build log: ${BUILD_LOG}"

BUILD_START=$(date +%s)

if cmake --build "${BUILD_DIR}" --target "${BUILD_TARGET}" -j "${BUILD_JOBS}" \
    > "${BUILD_LOG}" 2>&1; then
    BUILD_END=$(date +%s)
    BUILD_DURATION=$((BUILD_END - BUILD_START))
    log_success "Build succeeded in ${BUILD_DURATION}s."
else
    BUILD_END=$(date +%s)
    BUILD_DURATION=$((BUILD_END - BUILD_START))
    log_error "Build failed after ${BUILD_DURATION}s. Last 40 lines of log:"
    tail -40 "${BUILD_LOG}"
    exit 1
fi

# ---------------------------------------------------------------------------
# Step 5: Verify binary
# ---------------------------------------------------------------------------
log_section "Step 5: Verify Build Output"

ARANGOD_BIN="${BUILD_DIR}/bin/arangod"
if [[ ! -f "${ARANGOD_BIN}" ]]; then
    ARANGOD_BIN="$(find "${BUILD_DIR}" -name arangod -type f -executable 2>/dev/null | head -1)"
fi

if [[ -n "${ARANGOD_BIN}" && -f "${ARANGOD_BIN}" ]]; then
    BIN_SIZE=$(du -h "${ARANGOD_BIN}" | cut -f1)
    log_success "Binary: ${ARANGOD_BIN} (${BIN_SIZE})"

    # Check for unresolved enterprise symbols
    if command -v nm &>/dev/null; then
        UNDEF_COUNT=$(nm "${ARANGOD_BIN}" 2>/dev/null \
            | grep ' U ' \
            | grep -ciE '(enterprise|EE|SmartGraph|Audit|HotBackup|Encryption)' \
            || echo "0")

        if [[ "${UNDEF_COUNT}" -eq 0 ]]; then
            log_success "No unresolved enterprise symbols."
        else
            log_error "${UNDEF_COUNT} unresolved enterprise symbol(s) detected."
            nm "${ARANGOD_BIN}" 2>/dev/null \
                | grep ' U ' \
                | grep -iE '(enterprise|EE|SmartGraph|Audit|HotBackup|Encryption)' \
                | head -20
        fi
    fi
else
    log_error "arangod binary not found in build output."
    log_info "This may be expected if ArangoDB's build system produces the binary elsewhere."
fi

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
log_section "Build Summary"

echo -e "  Repository:  ${ARANGODB_REPO}"
echo -e "  Branch:      ${ARANGODB_BRANCH}"
echo -e "  Build type:  ${CMAKE_BUILD_TYPE}"
echo -e "  Target:      ${BUILD_TARGET}"
echo -e "  Duration:    ${BUILD_DURATION:-?}s"
echo -e "  Build dir:   ${BUILD_DIR}"
echo ""
echo -e "${GREEN}${BOLD}BUILD SUCCEEDED${RESET}"
