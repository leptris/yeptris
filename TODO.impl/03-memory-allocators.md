# TODO.impl/03 — Memory: pool, compact allocator, content-sized arena

Status: done · Depends: 02 · Layer: `src/yeptris/memory` · PLAN.md phase: 0

## Design decisions (settled at implementation time)

- `alloc_hook.{h,c}` is implemented as **`allocator.{h,c}`** with the
  allocator passed explicitly (`yep_pool_create(sys, …)`); there is no
  process-global override — cleaner, thread-safe by construction, and the
  test injection just passes a counting allocator.
- `compact.{h,c}` (int32 offset-pointer encoding + overflow table) is
  **dropped in favor of an id-index design landing with the DOM (item 11)**:
  nodes reference each other by dense `uint32` node ids into an
  arena-resident array. Same 4-byte reference size, but the entire bug
  class of the libleptris macOS ASLR overflow (TODO 121) is structurally
  impossible — compressing pointers was the hazard; indexing nodes is
  sounder. The "forced-threshold" test branch therefore has nothing to
  test here and moves to 11's id-table growth tests.

## Goal

All allocation in the library flows through three primitives, and nothing
outside `memory/` calls `malloc`. Freeing a document is one call. Steady-state
parse allocation count is zero (growth aside).

## Deliverables

- `memory/pool.{h,c}` — O(1) bump pool for small, same-lifetime objects
  (port of leptris `memory/pool.c`).
- `memory/arena.{h,c}` — the **contiguous per-document arena** (leptris
  TODO 183 pattern): growable block list, but sizing comes from a caller
  hint: `yeptris_arena_reserve(arena, const YepSizingHints*)` where hints
  are the SIMD occurrence counts produced by scan (06) — newline, colon,
  quote, anchor densities — so the first block covers ~all of a typical
  document and realloc-churn is rare. Fallback growth is amortized-doubling.
- `memory/compact.{h,c}` — 32-bit offset-pointer encoding for intra-arena
  references plus the overflow table for the rare >2 GiB / high-ASLR cases
  (the leptris TODO 121 macOS lesson; encode/decode helpers + the fallback
  path, tested on both branches via a forced-threshold build flag).
- `memory/allocator.{h,c}` — the single indirection point for system
  memory: `yep_allocator {alloc, free, ctx}` passed explicitly by every
  consumer; `yep_system_allocator()` is the default. Lets 19's tests
  inject allocation failure and count allocations (libfyaml's
  discipline, our SSOT for allocation testing).

## Design decisions

- Ownership law: every byte reachable from a `YeptrisDocument` lives in its
  arena/pool; `yeptris_document_free` = one release; no refcounting.
- Node structs, interned strings, fold/escape scratch, recorder event
  buffers: all arena. Input buffer: borrowed (caller-owned) or
  arena-transcoded (encoding, 05) — never both.
- Allocation failure behavior is uniform: return `YEPTRIS_ERROR_MEMORY`,
  document remains freeable (no partial states).

## Phases

A. pool + tests (reuse patterns, no per-alloc syscalls).
B. compact allocator + forced-overflow tests.
C. arena with sizing-hint API + growth tests; wire the alloc hook.

## Acceptance

- Zero-leak gate: `leaks --atExit` / valgrind clean on the unit suite.
- Instrumented run: parsing a corpus file performs 0 system allocations
  after the initial reserve (growth events logged, target < 1 per doc on
  the corpus median).

## References

- `~/src/leptris/leptris/src/leptris/memory/{arena.c,pool.c,compact_allocator.c}`.
- `~/src/external/libfyaml/src/allocator/` + `test/libfyaml-test-allocator.c`
  (failure-injection discipline).
