# 68 — the SIMD stopset kernel (nibble-class `stopset_find`)

## Problem

`yep_text_stopset_find_scalar` is the #1 flat-profile entry on every losing
shape (sample, 8s, top-of-stack counts):

| shape | samples | share |
|---|---|---|
| scalar-heavy | 2156 / ~9800 | ~22% |
| block-heavy  | 946  / ~9800 | ~9.7% |
| anchor-heavy | 309  / ~3490 | ~8.9% |

The kernel is a byte-at-a-time bitmap lookup — and it is SCALAR on both
ISAs: the AVX2 and NEON kernel tables install `yep_text_stopset_find_scalar`
(the deferral was recorded in the file headers when no caller was hot; the
campaign profiles now make it the hottest kernel in the block paths).
`yep_scan_plain` and `yep_scan_line` call it for every span ≥ 64 B.

## Design

**Precompiled stop set (the SSOT fix).** The 32-byte bitmap cannot be
vectorized in place; classification needs two 16-entry nibble tables. A
runtime set would pay a 256-step build per call, so the stop set becomes a
declared struct — built once, at its owning module:

```c
typedef struct yep_stopset {
    unsigned char bitmap[32];    /* the truth (tests unchanged) */
    unsigned char hi_nib[16];    /* hi_nib[b>>4] has bit (b&15) set iff b in set */
    unsigned char pow2_lo[16];   /* {1,2,4,...,128} — bit selector by low nibble */
} yep_stopset;

void yep_stopset_init(yep_stopset* ss, const unsigned char bitmap[32]);
ptrdiff_t yep_text_stopset_find(const yep_stopset* ss, const char* s, size_t len);
```

Member test in a vector lane: `tbl(hi_nib, b>>4) & tbl(pow2_lo, b&15) != 0`.
Both tables are index-safe (indices 0–15; non-ASCII bytes read an empty
`hi_nib[8..15]` row — our sets are ASCII-only, so non-ASCII lanes classify
"not a member" exactly as the bitmap does).

- **NEON** (`vqtbl1q_u8` ×2, vmaxv probe, scalar first-hit walk of the
  16-byte chunk on hit).
- **AVX2** (`_mm256_shuffle_epi8` ×2 + movemask + ctz). Shuffle masks run
  in 128-bit halves (pshufb semantics), so each table is duplicated
  low/high — same trick as the existing kernels.
- Scalar tail below the vector width; scalar kernel stays as the reference
  (the differential suite proves ISA == scalar).

**OCP:** the kernel slot signature in `yep_text_kernels` changes
(`stopset_find(const yep_stopset*, ...)`); the three parse-side sets
(`yep_break_set`, `k_plain_stop_block`, `k_plain_stop_flow`) become
`static const`/init-once `yep_stopset` instances in scan.c (scan owns line
semantics — MECE unchanged). Emitter stop sets migrate at their own pace:
`yep_text_stopset_find` keeps a bitmap-taking wrapper for them.

## Acceptance

- SimdText differential suite green (scalar/NEON/AVX2 equivalence).
- Full ctest green; flow-direct-diff + block-pair-diff gates green.
- h2h: scalar-heavy and block-heavy measurably up (ledger numbers).
- No new warnings (clang ≥ 14, -Wall -Wextra).

## Closure (2026-09-12)

Landed: `yep_stopset` (bitmap truth + nibble-class lo/hi tables, 1-2
groups in the vector path, wider classes decline to scalar), NEON tbl +
AVX2 pshufb kernels, the three scan-side constants as prebuilt literals
pinned by the (upgraded) Parse.StopClasses spec; StopsetFind
differential extended to the new form.

Measured (interleaved h2h referee, M-series, pre-gate-fix baseline →
post): scalar-heavy 0.67x → 0.78x, flow-single 0.72x → 0.83x,
deep-nesting 0.80x → 0.91x, wide 0.85x → 0.90x. Kernel cost in
scalar-heavy's flat profile: 2156 → 621 samples; shape time −15%.
