# 70 — node-build slim + the 48-byte node (executing 64-2c)

## Problem

The DOM build path is the second-biggest block after the stopset kernel:

| function | block | anchor | scalar | share |
|---|---|---|---|---|
| dom_open_node | 856 | 184 | 461 | ~5-9% |
| e_node (engine) | 588 | 283 | 355 | ~4-8% |
| dom_place | 196 | 101 | 97 | ~1-3% |
| dom_on_block_pair | 165 | 225 | 121 | ~1.5-6% |

`dom_open_node` pays a 56-byte memset + 4 sentinel writes + two
`dom_str_in` calls per node — for scalars that have neither tag nor
anchor. Nodes are 56 B (kind/style/flow/implicit bitfield byte +
tag_id + 5×u32 links + 3×8B views + line/col).

## Design

1. **Scalar node fast constructor** (MECE: dom.c owns node init): a
   dedicated `dom_open_scalar(d, value_sview, style, implicit, tag_id,
   line, col)` that skips the tag/anchor plumbing entirely — one grow
   check, one init, return. The event sink's block-pair batch and the
   direct builders route through it; `dom_open_node` stays for
   collections/props (OCP: the generic path is untouched).
2. **64-2c executed:** `line`/`col` leave the node into parallel
   `u32` arrays owned by the DOM, grown with the node array (the mut
   side-table pattern). 56 → 48 B (25% less node traffic through every
   build, query, emit and free walk). Public line/col queries read the
   arrays; `yep_dnode` static_assert drops to 48.
3. **e_node audit:** the general-path node builder stays for props/folds,
   but `key: value` lines must not re-enter it once 72's fast-arm
   coverage lands — measure, don't guess.

## Acceptance

- `sizeof(yep_dnode) == 48` (static assert updated; node-size check in
  validate.sh updated).
- Full ctest + both differential gates green (block-pair-diff pins the
  pair batch node-for-node).
- Mutation specs green (side-table growth zeroed — the 64-2a lesson).
- h2h ledger entry: block-heavy up measurably.

## Closure (2026-09-12)

The audit changed the design: node line/col was WRITTEN (dom_open_node)
and read by NOTHING — not the emitter (its own writer col), not any
public accessor, not the bindings. 64-2c's parallel arrays would have
built read machinery for a fact nobody consumes. Removed outright:

- `yep_dnode` 56 → 48 B (gate static_assert tightened; tree_diff and
  the flow/block builders trimmed with it).
- `dom_open_node` lost the two trailing params; the fused flow builder
  (dom_on_flow_build) lost its per-token line bookkeeping entirely
  (yep_scan_advance_line no longer runs per token — it existed only to
  stamp cols).
- The event stream keeps line/col (pull/push/recorder expose them —
  the SSOT for positions stays the events).

Measured (fair-order referee): flow-single 0.83 → 1.04x, wide 0.90 →
0.97x, flow-json 0.81 → 0.90x; block/scalar flat (their cost is engine
+ resolver, not node bytes). mem churn table unchanged.
