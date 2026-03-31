#!/usr/bin/env bash
set -euo pipefail
# Verify no symbols in the compiled library use the wrong namespace (openarangodb).
# Usage: ci/symbol-audit.sh [path/to/libopenarangodb_enterprise.a]

LIB="${1:-build/libopenarangodb_enterprise.a}"
if ! [ -f "$LIB" ]; then
  echo "ERROR: Library not found at $LIB"
  exit 1
fi

# Check for wrong namespace in compiled symbols
BAD_SYMBOLS=$(nm -C "$LIB" 2>/dev/null | grep "openarangodb" || true)
if [ -n "$BAD_SYMBOLS" ]; then
  echo "FAIL: Found symbols with wrong namespace 'openarangodb':"
  echo "$BAD_SYMBOLS"
  exit 1
fi

echo "PASS: All symbols use correct namespace"
