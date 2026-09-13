# 80 — the kernel/CPU diagnostics in every bench + profile artifact

## Problem

The ubuntu perf artifact showed scalar-kernel symbols (stopset/scans)
at 8-11% — indistinguishable after the fact between (a) LTO symbol
mis-attribution, (b) tail-call attribution, and (c) an actual
scalar-table dispatch (which would explain the entire ubuntu/local
asymmetry). Artifacts must carry the ground truth, not inference.

## Design

- `bench_matrix` prints `kernels: avx2|neon|scalar...` as its first
  line, derived from the same `yep_cpu_detect` the dispatcher uses —
  every bench artifact (local and CI) states the table that ran.
- `scripts/profile-linux.sh` records `/proc/cpuinfo`'s avx2 flag into
  the artifact — the CPU's capability beside the dispatcher's choice.

## Closure (2026-09-13)

Landed with the 79 item; local artifact prints `kernels: neon`. The
next CI run's artifacts decide the mis-attribution question for good.
