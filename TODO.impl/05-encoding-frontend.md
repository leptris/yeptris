# TODO.impl/05 — Encoding front-end: BOM, fused UTF-8 validation, transcode

Status: pending · Depends: 03, 04 · Layer: `src/yeptris/encoding` · PLAN.md phase: 0

## Goal

The parser always consumes validated UTF-8 — that guarantee is owned by
exactly one module. Encoding detection, validation, and one-way transcoding
happen before/alongside scanning, never inside the grammar.

## Deliverables

- `encoding/bom.{h,c}` — BOM sniffing: UTF-8 BOM (skip), UTF-16LE/BE,
  UTF-32LE/BE detection with the libyaml-compatible precedence rules and
  the documented pathological-short-input cases.
- `encoding/utf8_validate.{h,c}` — fused validation used as the scan layer's
  front-guard: ASCII fast path (one SIMD op proves the whole span ASCII),
  full validation only for spans containing high bytes; error reports the
  exact byte offset (Psych surfaces it in `SyntaxError`).
- `encoding/transcode.{h,c}` — UTF-16LE/BE and UTF-32LE/BE → UTF-8 into the
  document arena; unpaired surrogate / out-of-range handling per the
  Unicode replacement policy (match libyaml behavior in compat mode;
  hard error in strict mode).
- Registry (`encoding/encoding.c`): table of input encodings
  `{id, sniff, transcode}` — OCP: a new encoding is a new table entry.

## Design decisions

- MECE boundary: `encoding` owns charset truth; `scan` receives a buffer
  guaranteed UTF-8 and never re-validates. Output is always UTF-8 (the
  leptris serialization guarantee — the emitter never transcodes).
- Zero-copy policy: UTF-8 input is validated in place and **borrowed**;
  non-UTF-8 input is transcoded once into the arena and then borrowed.
  `encoding` decides borrowed-vs-owned; nothing downstream cares.
- Validation kernels use the simdutf algorithmic shapes (range-check + NFA
  compression) implemented in our C99 kernel framework — no C++ dependency,
  no vendored code.

## Phases

A. BOM + UTF-8 validation (differential vs a byte-by-byte DFA reference,
   exhaustive multiboundary corpus: every 1–4 byte sequence shape).
B. Transcoders + roundtrip tests (transcode(UTF-8 → UTF-16 → UTF-8) is
   identity for valid input).
C. Compat-mode behavior lock: port psych `test_encoding.rb` expectations.

## Acceptance

- Validation throughput ≥ 10 GB/s on pure-ASCII spans and ≥ 2 GB/s on
   CJK-heavy text (kernel benchmarks, recorded in the ledger).
- All psych encoding tests pass through the future parse entry (final gate
  happens in 15; here: unit-level golden outputs).

## References

- `~/src/external/simdutf` (kernel algorithms), `~/src/external/libyaml/src/reader.c`
  (libyaml's approach + its historical edge cases), `~/src/external/psych/test/psych/test_encoding.rb`.
