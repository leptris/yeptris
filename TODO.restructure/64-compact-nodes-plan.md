# 64 — compact nodes: the executable phase-2 plan (measured inventory inside)

Status: spec'd, NOT started — rewrite-class; execute in the three
sub-phases below, each independently gated and landed

## The measured inventory (2026-09-11)

Consumers: `attached`/`depth` — mutate.c + jsonc_compat + dom
internals (36 refs outside mutate); parse writes them per node via
dom_link but only the MUTATION API reads them. `line`/`col` — NOT
read by emit or visit (verified); read by accessors and the
differential gates (which pin path equality through them — keep).
value/tag/anchor sviews are already 8B offset records.

## Phase 2a — evict the mutation fields (64B -> 56B, ~3-5% on
node-dominated shapes)

`attached` + `depth` (5B + padding) leave yep_dnode: parse/link
stops writing them; the mutation API computes or maintains them in
a doc-level side table populated lazily on first mutation (parse
docs never pay). Gate: mutate/visit specs unchanged; both
differentials; the bounded-parse law covers the side table's growth.

## Phase 2b — link packing (56B -> 48B)

first_child/last_child/next_sibling/count are 4×uint32 + count;
last_child is derivable (first_child + count walk is O(n) — NO:
keep last_child, pack count into the depth-16 spare? Measure
first). Only proceed if the bench moves; otherwise close 2b by
arithmetic like 62.

## Phase 2c — line/col demotion (48B -> 40B, ABI-adjacent)

line/col move to a parallel array the ACCESSORS join through.
Touches: build.c accessors, the differential gate (reads through
the accessor — its equality pin is preserved), bindings. Owner
sign-off on the accessor cost before building.

## Rules

Each sub-phase: spec first, both differentials green, CI h2h
medians (2-3 dispatches) before/after in the ledger, the 64B-gate
static assert updated with the new bound, bounded-parse law on any
new growth path. A sub-phase that doesn't move the h2h closes by
measurement (the 46/47/61/62 precedent).
