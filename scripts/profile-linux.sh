#!/usr/bin/env bash
# profile-linux.sh — the CI profiling artifact (TODO.restructure/55).
# Runs perf against the bench corpora, saves reports as workflow
# artifacts. Usage (from a workflow one-liner):
#   scripts/profile-linux.sh <build-dir> [artifact-dir]
set -euo pipefail
cd "$(dirname "$0")/.."
BUILD="${1:-build}"
OUT="${2:-profile-out}"
mkdir -p "$OUT"

command -v perf >/dev/null || { echo "perf not installed"; exit 1; }
# Runners default to perf_event_paranoid>1; sudo opens sampling.
# Reports that come back empty mean the parry failed — say so loudly
# instead of uploading silence.
PERF="perf"
if [ "$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 3)" -gt 1 ]; then
  PERF="sudo -n perf"
fi

# The DOM bench on the two persistent asymmetry shapes, 5s samples
for shape in scalar-heavy deep-nesting; do
  $PERF record -F 199 -g -o "$OUT/$shape.data" -- \
    "$BUILD/benchmarks/bench_matrix" --quick >/dev/null 2>&1 || true
done
perf record -F 199 -g -o "$OUT/anchor-heavy.data" -- \
  "$BUILD/benchmarks/bench_matrix" --quick >/dev/null 2>&1 || true

for f in "$OUT"/*.data; do
  if [ ! -s "$f" ]; then
    echo "WARNING: no samples in $f — perf was blocked" >&2
  fi
  $PERF report -i "$f" --stdio --no-children --percent-limit 1 \
    > "${f%.data}.txt" 2>/dev/null || true
  rm -f "$f"
done
echo "profiles in $OUT:"
ls -la "$OUT"
