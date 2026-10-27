# 50 — the flow sink fast-path: direct DOM build on validated spans

Status: LANDED (PR #171). flow-json 0.50x -> 0.92x/0.98x (h2h
median mac/ubuntu); the differential caught and a companion fix
landed a pre-existing DOM bug (key-anchored scalars never bound
their ordinal — aliases resolved to garbage). Remaining margin for
this shape family: flow-single is scan-bound (see the ledger's next
walls).

## Why

e_flow_json pass 1 already strict-validates the whole span; pass 2
re-walks it to emit per-token events into the sink (~80ns/event:
event init + emit_now + dispatch + dom_on_event — the pipeline ryml
does not have). The DOM is the only sink that can build from the span
directly; pull/push/recorder stay event-driven BY DESIGN.

## Design (OCP: one optional callback, zero core edits)

1. yep_sink grows `int (*on_flow_json)(void* ctx, const char* p,
   size_t open, size_t close, yep_view anchor, yep_view tag,
   uint32_t anchor_id)` — appended AFTER ctx so the six positional
   sink literals stay correct. e_flow_json calls it after pass 1 +
   the existing bails (key-follows check, flow_enforce, single-line
   note, ev_scopes==0). Return 1 = subtree built (engine continues
   past the close with pass 2's exact tail-state updates); 0 = not
   handled (pass 2 runs exactly as today); <0 = abort (-2).
2. dom.c implements it: one walk of the validated span placing nodes
   through the DOM's own laws — dom_open_node (node-init extracted
   from dom_new_node; the event path delegates to it), dom_place
   (pending-key pairing shared with the event sink), dom_link,
   dom_anchor_set. Strings with escapes unescape straight into the
   arena (one copy; the event path pays two). Numbers resolve through
   the parse's resolver — yep_dom grows a resolver field set by
   parse.c beside the engine's (schema SSOT at the parse seam; NULL
   = core12). Depth caps and the map-pending-key dance identical.
   The simple-key 1024 limit is enforced by the ENGINE in pass 1
   (long keys fall to pass 2, which owns that error today).
3. Line/col bookkeeping: the same count-only-unscanned advance pass 2
   uses, extracted to scan.c (yep_scan_advance_line) and shared.

## Gates (hard)

- A PERMANENT tree-equality differential ctest: same input through
  the engine twice — sink without the callback (event-built) vs sink
  with it (direct-built) — comparing every node field recursively
  (kind/style/implicit/tag_id/flow/value/tag/anchor/line/col/count/
  links/alias targets) over: the conformance corpus, yaml-test-suite,
  the libyaml snapshots, JSONTestSuite inputs, and targeted edges
  (escapes, numbers incl. exponent forms, keys at the 1024 boundary,
  empty containers, nested seq/map, whitespace variants, CRLF,
  multi-line spans under block parents, anchored flow roots, depth
  caps). Any divergence reverts the unit.
- Full standing gates: ctest + roundtrip + libyaml-diff + fuzz.
- CI bench ryml columns; ledger before/after.

## Acceptance

flow-json / flow-single clear ryml on macOS DOM with the classifier's
wrapper win stacked; ubuntu follows in the same table.
