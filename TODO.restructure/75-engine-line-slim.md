# 75 — engine line-loop slim

## Problem

`engine_run_impl` SELF-time (loop + memo + frame bookkeeping, not
callees) is the single largest entry on three shapes: scalar-heavy 17%,
wide 10%, block 9%. Per line the loop: two separate memo probes
(`li_cache_line`, then `shape_cache_line` — both compare against
`e->line` and both re-check `e->pos == e->line_start`), re-derefs
`e->frames[e->depth-1]` up to five times in the unwind/continues logic,
computes the dash-blank test twice (SEQ branch, MAP branch), and calls
`e_line_done` per line.

## Design

1. **One memo probe**: the line-info and shape memos share their
   invalidation key (`e->line`) and their guard — fold into a single
   combined memo (cache line number + both structs).
2. **Top-frame local**: one load of the frame (col/kind/inline) reused
   by the unwind and continues logic; write-back only on change.
3. **Dash-blank test hoisted**: computed once per line into a local
   (the `-` + blank rule), used by both branches.
4. `e_line_done` inlined for the common single-\n case.

No dispatch-surface changes (the single-dispatch-point law holds).

## Acceptance

- Full ctest + differential gates green.
- h2h: scalar/wide/block up; ledger entry with self-time samples
  before/after.

## Closure (2026-09-12)

Landed: the dash-blank test hoisted once per line (was spelled twice
in the continues logic); the top frame loads once per unwind
iteration (`yep_frame` typedef); `e_line_done`/memo probes measured
as already-minimal (three predictable compares per line) — the memo
merge was dropped as unmeasurable. The big self-time (9-17%) is the
genuine per-line state machine; the structural candidate NOT taken:
skipping key-scalar resolution at parse (bindings may surface key
types — needs a binding-semantics audit first; recorded in the
ledger as the candidate with its risk).
