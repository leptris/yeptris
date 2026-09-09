# 48 — the flow/anchor direct build: beat ryml on the four lost shapes

Status: IN FLIGHT. 49+50 landed (see the 2026-09-10 ledger entry
for the h2h standing table and the named next walls). The 2026-09-09
WON/LOST table above is superseded: its columns were phase-biased
(interleaved medians are the referee). Acceptance unchanged.

## The full CI verdict (2026-09-09 baseline, macOS DOM vs ryml)

WON: block-heavy 1.48x, scalar-heavy 1.07x, wide-mapping 1.13x.
(realworld: ryml cannot parse the suite corpus — n/a.)

LOST — the campaign:

| shape | yeptris | ryml | vs ryml | ubuntu |
|---|---|---|---|---|
| anchor-heavy | 44.8 | 137.0 | **0.33x** | 0.47x |
| flow-json | 81.6 | 163.8 | 0.50x | 0.54x |
| flow-single | 90.2 | 152.3 | 0.59x | 0.51x |
| deep-nesting | 285.1 | 427.3 | 0.67x | — |

## The design (Phase A done right this time)

Item 47's index failed because it REWROTE THE FINDING; the win is
removing the EVENT PIPELINE, not the scanner. e_flow_json already
validates the whole span in pass 1 — the emit pass (per-token
yep_event init + emit_now + sink dispatch + dom_on_event, the
measured ~80ns/event wall) is what ryml does not have.

1. **Sink fast-path (OCP)**: yep_sink grows an OPTIONAL callback
   `int (*on_flow_json)(void* ctx, const char* p, size_t open,
   size_t close, yep_view anchor, yep_view tag, uint32_t anchor_id)`.
   The engine calls it right after pass 1 validates; return 1 =
   subtree built (engine continues past the close), 0 = not
   handled (engine runs pass 2 events exactly as today — pull and
   recorder sinks leave it NULL and are unaffected).
2. **The DOM builder** (dom.c): one walk of the validated span
   creating/linking yep_dnodes directly — mirroring pass 2's token
   walk (styles, implicit flags) and reusing the DOM's own
   node-creation helpers so node outcomes are IDENTICAL to the
   event path (the differential gate pins this: event-built vs
   direct-built trees byte-equal on the whole corpus + suite).
   Anchor/tag bind at the subtree root; "<<" quoted keys stay
   literal (tag-driven merge law); the 1024-byte simple-key limit
   and depth caps enforce identically.
3. **Ceiling math (honest)**: the event pipeline is most of our
   per-token cost; removing it projects flow-single 90 -> ~140-165
   (ryml 152) — BORDERLINE. To CLEAR ryml, pair with the scalar
   scan work (strings are where ryml's kernel beats ours, item
   46's +2% ceiling note) or accept parity-class wins per shape.
4. **anchor-heavy (0.33x — the worst cell)**: investigate BEFORE
   the direct build — the corpus's anchors are block-level (&a on
   scalars/maps), which e_flow_json never sees; the loss is in the
   BLOCK path's anchor machinery (nametab + event hops + the alias
   re-emit in e_alias). Profile first; the fix may be independent
   and bigger than the flow work.
5. **deep-nesting (0.67x)**: our depth cap walk + per-level event
   pairs vs ryml's stack; likely rides the same direct-build win.

## Gates

- Tree-equality differential (event path vs direct path) over the
  conformance corpus, yaml-test-suite, fuzz corpus.
- The CI bench with ryml columns is the referee; per-shape ledger
  entries before/after; a shape that regresses reverts.

## Acceptance

Every ryml-comparable shape > 1.0x on BOTH CI platforms.

## Anchor-heavy decomposition (from the same CI run — a concrete lead)

| measure | MB/s | reading |
|---|---|---|
| pull | 65.9 | engine + events, no tree — 0.48x of ryml |
| recorder | 45.6 | ≈ DOM |
| DOM | 44.8 | **0.68x of our OWN pull** |

Every other shape has DOM ≈ pull; anchor-heavy alone pays 1.5x in
the DOM SINK (alias/anchor node creation + resolution in dom.c) on
top of the engine's own deficit. TWO independent losses to attack:
(1) the dom-side anchor/alias penalty (unique to this shape —
measure dom.c's alias node path first); (2) the engine events
(the same pipeline the flow direct-build removes — block-level
anchors need the per-event cost cut, not a span shortcut).

## Anchor lead 1, checked (2026-09-09): the ALIAS arm is already O(1)

dom.c's YEP_EV_ALIAS resolves via the event's anchor_id into the
direct-indexed array (dom_anchor_get) — no name re-resolution, no
nametab in the sink. The 1.5x DOM-vs-pull penalty is NOT a naive
lookup; it is spread across alias-node creation, anchor binding,
dom_ev_str copies, and placement — PROFILE dom_place/dom_new_node
on the anchor corpus next (Instruments/perf), do not re-read the
alias arm expecting the smoking gun.
