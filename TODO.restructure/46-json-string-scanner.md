# 46 — the JSON string scanner: SIMD qbc_find (45-A′ slice one)

Status: complete

## Why

Item 45's pre-measured verdict: the scanner is the mandatory phase.
The string scan is the weakest cell — yep_json_string rides the
SCALAR bitmap stopset_find for every string (the milestone-55
vector stopset revert left stopset scalar), because JSON needs C0
rejection that quote_scan does not do. String-heavy JSON cells are
our flattest: flat str:str 0.95x, long strings 0.85x (numbers
already 0.63x). Every surface rides this kernel: the strict-JSON
DOM seam, the engine's flow fast path, and BOTH bindings' native
materializers — one kernel lifts all of them.

## Design

- simd_text gains `qbc_find(s, len)`: offset of the first byte that
  is '"', '\\', or a C0 control (the JSON string-stop set — scan/
  json.c owns the grammar, simd_text owns the finding). ONE SIMD
  pass, three compares OR'd, no table LUT (milestone 55's three
  bugs are absent by construction: compare masks are naturally
  0x00/0xFF; no nibble LUT; no per-call build).
  - AVX2: cmpeq('"') | cmpeq('\\') | cmpeq(min_epu8(v,0x1F), v)
    (the unsigned-lt idiom) -> movemask -> ctz.
  - NEON: same three compares OR'd; first-hit via the existing
    neon mask helper.
  - Scalar tail + sub-32B span: the tight 3-way byte loop
    (yep_text_qbc_find_scalar), the quote_scan pattern.
- yep_json_string's loop switches to k->qbc_find; the byte at the
  hit classifies exactly as today (quote = close, backslash =
  escape validate, other = C0 reject). Semantics unchanged by
  construction; the kernel is bit-faithful across ISAs.

## Gates

- The hardened differential (milestone 55's kept win): every stop
  byte alone at every lane offset, all prefixes, tails, mixed
  escapes+controls; cross-ISA equality.
- No JSON gate regression anywhere; the three referees (C bench
  flow-single, Ruby json_profile, Python json_profile) must move
  or stay flat — a loss reverts (the stopset precedent).

## Acceptance

- The ledger carries before/after on all three surfaces.
- The weakest cells (flat str:str, long strings) improve toward
  the numbers' margin class.


## Outcome (2026-09-08)

LANDED — kernel, hardened differential, C0-deep rejection test,
253/253, no regression on any referee. HONEST RESULT: flat on
short-string corpora (the span gate puts keys/short values on the
tight scalar loop by design), +2% on the 55-byte-string cell
(0.847 -> 0.865x), reference corpora flat — confirming item 37's
verdict that the remaining JSON deficit is host-object
materialization, not the scan. The kernel's value is structural:
the 45-A' index reuses these masks as its index bits. The LARGE
margin ask lands on A' (one-pass structural index + local-loop
materialization) — scoped, next.
