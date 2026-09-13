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

## Postscript (2026-09-13) — the diagnostic's first catch was the campaign's biggest

`kernels: scalar(sse2)` on the v0.1.32 CI artifact: the entire SIMD
kernel campaign (68/74/76) had NEVER run on the CI runners —
`__builtin_cpu_supports` returned 0 for avx AND avx2 on real Azure
Xeons (its runtime __cpu_model init lost the environment). Fixed in
82: raw cpuid leaf-1 (OSXSAVE+AVX gated by xgetbv XCR0) + leaf-7
(AVX2/BMI behind the avx gate).

The fix's first CI run also caught a LATENT AVX2-only kernel bug the
same minute: the stopset nonzero test used signed cmpgt_epi8 — a
group's 8th member sets lane-mask bit 0x80, which reads -128 and was
called "no hit". Fixed via the eq-zero complement (NEON's unsigned
vmaxv never had it). The nibble algorithm itself is exact (200k
random-buffer simulation).

**The CI referee with kernels live (ubuntu, first ever):**
flow-json 1.34x, json-doc 1.46x (engine), deep 1.10x, flow-single
1.12x, scalar 1.03x, wide 1.01x, block 0.98x, anchor 0.91x. Six of
eight shapes beat ryml where the record is kept; block at parity;
anchor is the last gap (item 79's territory).
