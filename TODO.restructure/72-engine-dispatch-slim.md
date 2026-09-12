# 72 — engine dispatch slim + fast-arm coverage

## Problem

`engine_run_impl` self-time (the loop, dispatch, memo lookups — not its
callees) is 8-14% of every shape:

| shape | samples | share |
|---|---|---|
| scalar-heavy | 1390 | ~14% |
| block-heavy  | 782  | ~8% |
| anchor-heavy | 332  | ~9.5% |

And `e_node` (the general-path node builder) still shows 4-8% on shapes
whose lines are plain `key: value` — lines the 49/54/57 fast arms were
supposed to own end-to-end.

Also: `yep_text_active()` (the dispatch-table fetch) is itself 1%+ — it
runs per scan call, not per document.

## Design

1. **Fast-arm coverage audit (measure, then fix):** instrument or break-
   point-count the block-pair/open batch entries per corpus; any
   `key: value` line falling through to `e_node` is a classified-shape
   miss — extend the shape classifier, not the general path (OCP).
2. **Dispatch loop slim:** the per-line work between classifications
   (memo seed, stack top reload, `yep_text_active()` re-fetch) hoists to
   locals; the kernels pointer loads once per document in scan.c's
   callers where the loop is.
3. Keep the single dispatch point — no switch spreading (architecture
   law).

## Acceptance

- Differential gates green (any fast-arm extension is node-for-node
  pinned by block-pair-diff).
- h2h ledger entry with the coverage numbers before/after.
- Full ctest green.

## Closure (2026-09-12) — audit executed, rework measured-deferred

The coverage audit's answer: e_node is NOT a fast-arm miss on any bench
shape — it is the engine's nested-map opener (`key:` with the value on
following lines dispatches through e_parse_value → e_node), and its flat
weight (6-8%) is the frame/props machinery those documents genuinely
need. Every `key: value`, alias, and anchor-plain line in every corpus
goes through the on_block_pair/on_block_open batches (block-pair-diff
pins them node-for-node).

Deferred with numbers: engine_run_impl self-time (7-14% flat) is the
line loop + memo plumbing; a focused rewrite is a full session's risk
for the remaining ~0.5-0.7x on anchor/block, and the session's budget
went to the measured-bigger wins (68/69/70/71). Next session's item if
the CI profile (round 3, now meaningful post-gate-fix) still shows it.
