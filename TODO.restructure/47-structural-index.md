# 47 — the structural index: one SIMD pass, bit-driven descent (45-A′)

Status: closed (measured dead)

## Why

Items 37/46 measured the JSON deficit to the host-materialization
floor — but the SCAN side still pays per-token kernel calls and
byte-by-byte finding: ws skips, quote closes, structural
classification. The simdjson-proven shape fixes exactly that: ONE
SIMD pass marks every byte that can matter (whitespace, {}[]:,,
quote, backslash, C0) as a bit per byte; the descent then walks
BITS (tzcnt chains, 64 bytes per probe) and touches bytes only at
token boundaries. The whole-buffer index also improves cache
behavior: one streaming pass, then the descent reads a 19KB bit
array for a 152KB document instead of interleaving kernel calls.

## Design

- simd_text TUs gain `jsonmark(s, len, uint64_t* bits)`: bit i set
  iff byte i is ws (space/\t/\n/\r), structural ({}[]:,), quote,
  backslash, or C0. DIRECT COMPARES ONLY (13 cmpeq + OR tree per
  chunk) — no pshufb LUT, so the milestone-55 bug classes are
  absent by construction; bit-identical across ISAs (hardened
  differential: every set byte at every lane, boundaries, prefixes,
  randomized cross-checks). Public seam: `yep_json_index_build`
  (scan/json.h — the grammar SSOT owns the API).
- The Python materializer (ext/yeptris_native.c) rewrites its token
  loop bit-driven: jp_ws becomes a bit-skip; jp_str finds the close
  through the bits (escape hops validated locally, exactly today's
  semantics); object/array commas and colons classify via the byte
  AT the interesting position (the descent reads it anyway);
  numbers/literals reuse yep_json_number_scan/literal on the
  bounded slice — validation semantics unchanged by construction.
- GATE: the 142-case parity suite + the CI referees decide. If the
  reference corpus does not improve >= 5%, the descent reverts and
  the unit is ledgered dead (the stopset precedent). If it wins,
  the Ruby materializer (json_ruby.c) ports the same shape.

## Acceptance

- Differential + parity green everywhere; no referee regression.
- The ledger carries before/after on the reference corpus and the
  shape decomposition (flat str:str, long strings, ints, floats).


## Outcome (2026-09-08): BUILT, MEASURED, REVERTED

The gate fired: reference 0.785->0.961x, flat 0.95->1.422x, long
strings 0.85->1.25x — every shape worse. The index build (a full
extra pass, near-dense marks on numeric JSON because every token
start needs a bit, a per-parse 19KB malloc) costs more than the
byte scans it replaces. Full numbers and the design notes are in
the perf ledger; the survivors: the escape-grammar extraction, the
gap-garbage reject cases in the parity spec, and the lesson. A'
as originally scoped (structural index) is CLOSED; the remaining
JSON margin lives below the host-materialization floor, and the
rapidyaml campaign (45) remains the standing engine work.
