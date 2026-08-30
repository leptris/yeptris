#!/usr/bin/env bash
# validate.sh — the pre-completion sanity gate (TODO.impl/01).
#
# Clean build -> tests -> CLI smoke -> leak check. Run this before claiming
# any work complete. Usage: scripts/validate.sh [build-dir]
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="${1:-build-validate}"

cmake -B "$BUILD_DIR" -S . -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DYEPTRIS_WARNINGS_AS_ERRORS=ON
cmake --build "$BUILD_DIR"
ctest --test-dir "$BUILD_DIR" --output-on-failure
"$BUILD_DIR/cli/yeptris" version

# Leak gate: macOS leaks(1) when available; valgrind on Linux when available.
OS="$(uname -s)"
if [ "$OS" = "Darwin" ] && command -v leaks >/dev/null 2>&1; then
    leaks --atExit -- "$BUILD_DIR/cli/yeptris" version >/dev/null
elif command -v valgrind >/dev/null 2>&1; then
    valgrind --leak-check=full --error-exitcode=1 \
        "$BUILD_DIR/cli/yeptris" version >/dev/null
fi

echo "validate: OK"
