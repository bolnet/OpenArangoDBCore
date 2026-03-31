#!/usr/bin/env bash
set -euo pipefail
# Build OpenArangoDBCore with AddressSanitizer for ODR violation detection.
# Requires: cmake 3.20+, gcc or clang with ASan support
# Usage: ci/build-with-asan.sh [arango-source-root]

BUILD_DIR="build-asan"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON

ASAN_OPTIONS=detect_odr_violation=2 cmake --build "$BUILD_DIR" --parallel

echo "ASan build complete. Run tests with:"
echo "ASAN_OPTIONS=detect_odr_violation=2 ctest --test-dir $BUILD_DIR/tests -V"
