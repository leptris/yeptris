# 29 — Perf-statistics honesty + the DSO-inlining dead end

Status: complete

## Why

The JSON.parse race is close at min/median and clear at mean.
Reports must carry the full distribution, and the measured dead end
(inline kernel copies in the extension) must be ledgered so nobody
re-chases it.

Facts (152 KB / 29.4k values, Ruby 3.4, arm64):

- Local-copy kernels (no cross-DSO calls): min 0.625 vs shared-lib
  0.66 — NOISE-LEVEL. The dyld call tax is not the gap; the kernels
  stay in the shared library (SSOT by construction).
- Distribution shape: our parse pauses GC for its duration, so the
  allocation batch is one young-gen sweep later; JSON.parse takes
  mid-parse collections and carries a fat tail (mean up to 5.7 ms on
  noisy runs vs median 1.3). We win mean-of-many consistently
  (0.72–0.82× across sessions); min/median fluctuate around parity.

## Plan

1. Ledger the DSO-inlining dead end with the numbers above.
2. The perf gate spec stays mean-of-many (min-of-N is noise-bound);
   board/README/ledger tables carry min/med/mean together.

## Acceptance

- PERF-LEDGER entry with the distribution table and the dead end.
- No unreverted kernel copies anywhere (SSOT intact).
