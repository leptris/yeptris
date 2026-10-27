# 49 — the one-pass line classifier (Phase B proper)

Status: LANDED (PR #170). Gates: 274/274 ctest, 21 new specs, all
sanitizers green. Measured under the interleaved h2h referee in the
2026-09-10 ledger entry (the old phase-biased table overstated the
"won" cells; h2h is the standing record).

## Why

The anchor-heavy profile (ledger 2026-09-09): ~60% of samples in
e_node/e_parse_value — the per-line choreography (line scan, dash/key
dispatch, value parse). That same choreography is the 37% block
wrapper around every `- {…}` line in the flow shapes. One target, two
campaigns. The bench corpora are exactly the classified shapes:

- anchor-heavy: `itemN:` / `  <<: *def0` / `  x: &xN word` / `  y: *xN`
- flow-json: `- { … }` (dash + flow)
- scalar-heavy / wide-mapping / deep-nesting: `kN: plain value`,
  `key_N: 123`, `lN:` at increasing indent

## Design

MECE: scan.c owns the byte walk and emits FACTS; the engine decides
dispatch ONCE per line from those facts. The existing helpers stay the
semantics SSOT (e_alias walks names, e_flow_json validates spans,
e_plain_multiline folds) — the classifier only chooses the door.

`yep_line_shape` (scan.h): kind ∈ {NONE, DASH (`- ` entry), KEY
(plain key + terminating colon)}; value class ∈ {EMPTY (EOL/comment),
PLAIN (span+term recorded), ALIAS (`*`), ANCHOR_PLAIN (`&name` then a
plain scalar), FLOW (`[`/`{` opener — the flow kernel owns it)};
positions: dash, key span, colon, value start, anchor span.

Classification (one walk, scan.c): DASH when first byte is `-` +
blank; KEY when plain-first AND not `& ! * ' " [ { ?` (props/quoted/
flow/explicit keys bail), via the same scan_plain block walk that
terminates on a blank-followed `:`; then the value class walk.
Anything else = NONE.

Engine: the shape is memoized per line beside the existing li_cache
(invalidated at every li_cache reset). e_node's fast arm fires only
when ALL hold: cursor at line-start content, fresh line memo, no pend
props, ctx ∈ {FRESH, VALUE_LINE}, shape ≠ NONE. Each arm mirrors the
existing emission sequence EXACTLY (same open/emit calls, same fold
call, same errors — the simple-key 1024 limit on the trimmed key
span, tab cases via the existing TAB flag, value floor = key/dash
column). ANY deviation inside an arm restores the cursor to the value
start and falls into e_parse_value — the existing chain decides. No
arm consumes anything before it can guarantee completion, so bail
semantics are total.

Explicit non-goals: quoted/flow/explicit-`?` keys, tag values, block
scalars after a key, `- &a val` entries, mid-line entries. The general
path keeps them unchanged.

## Gates

- Spec first: a shape-classifier unit spec pinning every kind/value
  class over the target shapes plus every bail shape.
- 253/253 ctest + emit-roundtrip + libyaml-diff + fuzz corpus.
- CI fresh-runner bench with the ryml columns at the branch; ledger
  entry before/after; a shape that regresses reverts.

## Acceptance

anchor-heavy and the block wrappers of the flow shapes measurably
closer on BOTH CI platforms; zero conformance movement.
