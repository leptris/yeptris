# 86 — the structural-index pass (the simdjson front; 81 stage 4)

## Standing after waves 1–2

vs simdjson 4.6.1 DOM on json-doc: 0.33x CI / 0.30x local at v0.2.2
(tape 253/301 MB/s). Wave 1 (the ONE-pass number token, PR #251):
every number was scanned three times (walker validate, is_float text
re-walk, tape conversion) — the walker now runs yep_json_number_scan
and the token carries the conversion; +20% tape same-machine.

## Tape profile after wave 1 (json-doc, 30s sample)

walk_next 41%, number_scan 29% (pre-fusion), tape_walk self 16%,
qbc_find 6%, json_string 5%. After the fusion the walker's remaining
cost is the per-token whitespace skip and byte classification —
exactly what a structural index eliminates.

## Wave 2 (void — measured dead 2026-09-14)

The SWAR ws skip REGRESSED the tape -40% (769 -> 464 iters): the
inter-token ws runs are 0-2 bytes, the byte loop exits in 0-2
predicted iterations, and a SWAR chunk pays ~20 ops regardless. SWAR
pays when the run is the unit (line facts), not per-token.

## Wave 3 (the pass): structural indices, simdjson's shape

Stage 1 — one linear pass over the buffer emits every STRUCTURAL
byte offset into a u32 array: `{ } [ ] : ,` and `"` outside strings.
String-awareness via the qbc jump (a quote at i → qbc_find from i+1 →
resume past the close: escapes handled by construction, no parity
trickery). v1 scalar in scan/json.c; an ISA kernel slot (appended
LAST — the table's positional law) if the pass measures.

Stage 2 — the indexed tape walk: the walker's strict state machine
ported onto the index cursor. Between structurals, a VALUE position's
text is [prev+1, next structural): number_scan/literal over the trim
plus a ws-only tail check. Whitespace skipping disappears — the walk
jumps index-to-index. The plain walker stays (SSOT for the DOM route;
the indexed walk is a tape-internal consumer sharing the kernels).

Error semantics: the indexed walk only decides ACCEPT/REJECT — every
reject falls to yep_json_document exactly as today, so the reported
byte and precedence never move. Gates: tape-diff 340 + json-suite-
strict unchanged.

## Honest target

simdjson's stage 1 is input-bound (multiple GB/s); our stage 2 still
runs the number conversion and record append. Realistic: 0.5–0.7x
after wave 3. Parity additionally needs the record append to shrink
(16% self: four column stores per token — consider interleaved
records or column batching).
