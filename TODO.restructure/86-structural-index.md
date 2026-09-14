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

## Wave 3 (void — measured dead BOTH ways, 2026-09-14)

Built in full twice, gated (2M-case status+content fuzz added to
tape-diff found three real bugs in the attempts: a root-bit
consumption off-by-one, map-value scalars rejected by a misfiring
strict-key check, and — the keeper — a pre-existing grammar
disagreement where the document validator and the strict walker
ACCEPTED a container at map-key position, building inconsistent
trees; all three machines now reject it, json-suite-strict pins
unchanged 283/283).

- u32 offset array: 775 -> 300 30s-iters (-61%). 4 bytes of write
  traffic per input byte plus an 8MB-per-parse materialization.
- simdjson's bitmask shape (1 bit/byte, ctz consumption, quotes as
  bit pairs, strings validated once by the qbc jump): 775 -> ~106-147
  (the Mac was thermally unstable same-hour; the shape is a large
  regression regardless). Profile: stage-2 walk 44%, stage-1 scalar
  pass 28%.

Why it cannot win here (the same verdict as item 47, now measured
for the tape consumer too): stage 2 must VALIDATE every gap byte —
ws-only checks and number-tail checks ARE the walker's fused ws-skip,
just relocated — so the index removes no scan work, and stage 1 adds
a full pass. simdjson's edge is a 3-8 GB/s SIMD stage 1 and a
fundamentally leaner stage 2; our walker's ws-skip was already
measured near-optimal (the SWAR ws attempt regressed -40 percent).
The tape's true remaining costs — number_scan, qbc_find, the record
append — are untouched by any index.

What SURVIVED: the grammar alignment (container-at-key), the 2M-case
fuzz parity gate in tape-diff (permanent), and the JsonTape pins
(gap classes, tabs-as-ws parity). The honest next levers for the
simdjson gap: the record append (interleaved records or column
batching) and the number kernel itself.
