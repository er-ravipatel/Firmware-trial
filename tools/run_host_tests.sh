#!/usr/bin/env bash
# Build and run the host-side unit tests (no hardware needed).
# Run inside WSL2 / Linux:  ./tools/run_host_tests.sh
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TESTDIR="$HERE/../tests/host"

cmake -S "$TESTDIR" -B "$TESTDIR/build" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$TESTDIR/build" -j "$(nproc)"
"$TESTDIR/build/host_tests"
