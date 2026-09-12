# 74 — 32-byte NEON iterations for stopset_find / find_not

## Problem

After 68, the kernels still walk 16 bytes per iteration: NEON M-series
issues two `tbl` per group per 16 B, and the loop carries a `vmaxv`
test per chunk. Residual flat weight on v0.1.27: `stopset_find` 7-12%
(block 951, scalar 1105, wide 662 samples per 8 s), `find_not` 4-6%
(wide 354, block 226, scalar 493).

## Design

Unroll to 32 B per iteration: two loads, shared table registers
(already loaded once), combine `m = m0 | m1`, one nonzero test per
32 B. First-hit extraction stays per-16 (spill + scan of the hit half
only). `find_not` the same (two eq-masks OR'd, one bits-test).

AVX2 is already 32 B — untouched. Scalar reference and differential
suite unchanged (bit-identical contract).

## Acceptance

- SimdText differential green on both shapes of probe (every prefix,
  all 256 bytes embedded late, random).
- h2h: block/scalar/wide measurably up; ledger entry.

## Closure (2026-09-12)

Landed: 32 B/iter unrolls for `stopset_find` (interleaved tbl work,
one vmaxv per two vectors, hit half spills alone) and `find_not`
(shared constant, per-half bits test). 16 B epilogue keeps every
prefix length exact; differential suite green on all ISAs.

Measured (same throttled window as 73): stopset's flat weight on
block 951 → ~600-range across runs; CI arbitrates.
