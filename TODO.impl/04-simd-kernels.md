# TODO.impl/04 — SIMD kernel framework (AOT TUs + runtime dispatch)

Status: done · Depends: 02 · Layer: `src/yeptris/common` · PLAN.md phase: 0

## Settled decisions (recorded at implementation time)

- **stopset_find stays scalar in both ISA tables** (documented in the TU
  headers). A runtime bitmap defeats cmpeq classification — the vector
  version needs nibble-table tricks (Muła-style) whose complexity must be
  earned by item 06's profiles first. The scalar 256-bit lookup is
  L1-resident. Revisit with profile data.
- **NEON movemask** uses a stack-reduce helper (correct-first); counts use
  UADDV over 0/1 lanes, which is the perf-critical path (arena sizing).
  Refinement candidate, ledger-tracked.
- **Include-order trap**: the ISA guards (`YEP_ARCH_*`, defined in
  port.h) MUST come after `#include "port.h"` — both TUs originally guarded
  before including port.h and compiled to empty objects (linker caught it).
- **Verification split**: the arm64 host exercises the NEON table; the
  AVX2 table's differential coverage comes from the x86_64 CI runner.
  Both tables share the differential suite (test_simd_text.cpp) with
  naive test-local references.
- MSVC `/arch:AVX2` wiring is deferred to item 20 (CI matrix today is
  gcc/clang).

## Goal

The AOT SIMD framework from libleptris, extended with the classification ops
YAML needs: scalar (always compiled) reference implementations + AVX2/NEON
TUs compiled with the right `-m` flags + one runtime dispatch through
`cpu.c`. Every op is proven equivalent to the scalar truth on every input.

## Deliverables

- `common/simd_text.{h,c}` (dispatch + scalar), `common/simd_text_avx2.c`,
  `common/simd_text_neon.c` — port the leptris op set first:
  `contains`, `find`, `find3`, `count_char`, `count3`, `copy_count3`
  (fused single-pass sizing+copy, TODO 188 pattern).
- New classification ops for YAML (implemented per-ISA, contract below):
  - `classify16/32(buf, len, class_mask)` — per-byte membership bitmask of
    chartype classes (line-break, space, tab, indicator set) via vector
    compares + movemask/vaddvq; the bridge between chartype (02) and vector
    code.
  - `find_not(buf, len, c)` — first byte ≠ c (indentation column scan).
  - `stopset_find(buf, len, mask)` — first byte in a class set (plain-scalar
    end detection: `: ` / ` #` / line-break / flow stop set).
  - `quote_scan(buf, len, q, &has_escape)` — locate the closing quote,
    escape-aware, reporting whether any `\` occurred (so 09 only unescapes
    when needed).
- Contract (leptris `simd_text.h`): every function reads at most `len`
  bytes; no faults past `len` (length-guarded chunking, never full-width
  loads beyond the end).

## Design decisions

- OCP: a new ISA target = new TU + one dispatch slot; core code calls only
  the dispatch API. A new op = declaration in `simd_text.h` + scalar impl +
  per-ISA impls (an ISA may fall back to scalar — declared, not hidden).
- SSE2 is the always-available baseline on x86_64 (leptris precedent);
  AVX-512 is deferred until a benchmark pays for it (perf-ledger entry if
  tried).
- Differential testing is the SSOT of kernel correctness: a fuzz-style test
  generates buffers of every length 0–96, every alignment 0–3, dense random
  + adversarial patterns (all-same, alternating, class boundaries), and
  asserts SIMD == scalar == chartype-table for every op.

## Phases

A. Port leptris op set + dispatch; differential suite.
B. Classification ops (classify16/32, find_not, stopset_find, quote_scan).
C. `copy_count3` fused sizing integration test (feeds 03/06).

## Acceptance

- Differential suite green over ≥ 10⁶ generated buffers, all ISAs available
  on the host (CI runs x86_64 + arm64).
- `count3`-class ops measured ≥ 8× scalar on 64 KiB buffers (guard against
  silent regressions; recorded in the perf ledger).

## References

- `~/src/leptris/leptris/src/leptris/common/simd_text*.{h,c}` and
  `src/CMakeLists.txt` (per-TU `-m` flags).
- `~/src/external/simdjson` (dispatch + structural-indexing patterns),
  `~/src/external/simdutf` (kernel shapes, length-guard techniques).
