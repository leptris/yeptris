# 45 — the engine campaign: close the rapidyaml gap (0.44-0.97x -> beat)

Status: pending

## The measured gap (item 44's table)

ryml beats us on 6/7 shapes; the worst are the flow/anchor shapes
(0.44-0.48x). The scoping decomposition:

- pull ≈ DOM ≈ recorder on the flow shape (95-111 MB/s) — the DOM
  sink is NOT the wall; the ENGINE LOOP is.
- Our engine-free strict-JSON seam (yeptris_parse_json) hits
  161-166 MB/s on the same scalar mix — 0.70x of ryml's 232.
  Even the direct seam is behind.

## Phases (each independently gated, measured before built — the
milestone-45 discipline)

### A. Simple-flow direct build (the biggest lever)

Extend the direct seam beyond strict JSON: the engine pre-scans a
flow span; if it contains ONLY flow-simple scalars (double-quoted
with escapes, plain without flow-special leading bytes, numbers,
bools, nulls — NO anchors/tags/aliases/merge/multiline), build the
DOM subtree directly from the jx token index (yep_dom_build_json's
shape generalized to flow-YAML-simple), skipping the event
pipeline for the subtree. ANY deviation falls back to e_flow
(conservative by construction, the e_flow_json law).

- Expected: flow-json/flow-single toward 140-160 MB/s.
- Gate: full conformance + libyaml-diff + the emitter round-trip
  corpus on a corpus-derived differential; the fallback path must
  be provably conservative (the pre-scan's grammar table IS the
  gate's spec).

### B. Engine-loop slimming (block/anchor shapes, 0.45-0.97x)

- The pass-4 shape from milestone 58: hoist a one-pass byte-class
  line classifier INTO the line scan so the dispatch decision is
  made once per line (not re-derived in e_node/e_parse_value).
- Anchor-heavy 0.45x: measure where the anchor path diverges from
  ryml (their anchor binding interleaves with node creation; ours
  pays the nametab + event hops).

### C. Scalar scan (0.72x)

ryml's scalar loop vs our scan-line choreography; the SIMD stop
tables exist — the per-line overhead around them is the candidate.

## Acceptance

- The item-44 table re-run: every shape > 1.0x vs ryml (block
  already 0.97 — parity plus margin), realworld noted as ryml-n/a.
- Zero conformance regressions (the standing gates).
- CI bench carries the ryml columns (landed with item 44).

## The Phase-A ceiling (measured 2026-09-08, before building)

The upper bound of Phase A (subtree-skipping) is ALREADY known:
the engine-free strict-JSON seam — our fastest possible flow path —
measures 161-166 MB/s on the flow-single scalar mix against ryml's
232 (0.70x). Skipping the engine for flow subtrees alone CANNOT
reach 1.0x on the flow shapes: the scanner itself must get faster
(inline the jx kernels into the callers; the simdjson structural
index is the proven shape for flow). Phase A remains worth it (94
-> ~150 projected) but A' (scanner) is the mandatory phase for the
win. Build A' first if the campaign must pick one.

## Status note

This is the standing campaign item — scoped, decomposed, ceiling-
measured, CI-gated (the ryml columns ride every bench run). Like
item 30's Python wave, it is its own wave; the session that picks
it up starts from this file's measurements, not from re-discovery.
