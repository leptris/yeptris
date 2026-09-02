# TODO.impl/13 — Emitter: exact sizing, style tables, canonical mode, streaming

Status: phase A COMPLETE; phase B canonical-mode COMPLETE (2026-09-01: yeptris_emit_options + serialize_ex; fixed form = flow collections, quoted strings, typed words, shortest floats via 14's printer, canonical anchor renaming a0.. via the nametab, '? key :' explicit flow keys for props/alias/collection keys; parse-fixed-point gate over the 405-snapshot corpus in emit-roundtrip); 13C COMPLETE 2026-09-02: yeptris_serialize_stream — flush hooks inside the writer (append-only output makes any high-water crossing a safe cut), memory bounded by the 64 KiB mark, sink-abort propagates as 0; byte-equality with buffered output gated (incl. multi-flush on a 1 MB doc). 13B WIDTH FOLDING landed 2026-09-02: best_width option (default 80) — flow collections wrap AFTER a value past the width, re-indented to the opening bracket's column (value boundaries only; never inside a scalar); writer tracks col streaming-safely; JSON output never folds; width-respect + reparse equality gated. Plain-scalar block-context folding stays fidelity-preserving (no fold) by design · Depends: 11, 09, 10 · Layer: `src/yeptris/emit` · PLAN.md phase: 4

## Goal

Nodes/events → bytes with zero reallocation, data-driven style decisions,
and a canonical mode with the leptris guarantee:
`serialize(parse(serialize(x)))` is byte-stable. The emitter is where the
"writer" half of the mission is won.

## Deliverables

- `emit/sizing.{h,c}` — exact-output pre-pass: per-node byte cost from
  (style, escape-cost table imported from 09, indent depth, line-break
  budget) → one reserve → linear writes. `yeptris_serialize_into(buf,
  cap)` with `buf = NULL` size query (the leptris zero-alloc FFI pattern).
- `emit/writer.c` — the single writer engine (SSOT): two front-ends feed
  it — DOM walk and event stream — never two writers.
- `emit/style.{h,c}` — the style chooser as a **rule table**: predicates
  over `YepScalarInfo` (parse-recorded) or a fresh-analysis function for
  built-from-scratch values; rules: plain-safe?, needs-quotes,
  prefers-literal (multiline), flow-eligibility, width-based folding
  decisions. OCP: a new rule = a new table entry; no branching soup.
- `emit/escape.c` — escape emission using **09's imported tables** (SSOT —
  sizing and writing agree by construction; a divergence is a test failure
  on both paths).
- `emit/indent.c` — precomputed pad buffers (power-of-two lengths) +
  SIMD fill; indentation emission is memcpy, never a loop of spaces.
- `emit/fold.c` — width-aware line folding (best_width default 80,
  libyaml parity), plain-scalar line-break placement, block-scalar
  emission preferring literal for multiline content.
- `emit/canonical.c` — canonical/stable mode: fixed style choices
  (quoted keys, explicit tags option), deterministic output; property
  test asserts byte-stability across N roundtrips.
- `emit/stream.{h,c}` — streaming writer over recorder events (chunked
  output callback) for unbounded emit targets.

## Design decisions

- Emitter never parses and never re-classifies bytes chartype-wise beyond
  the imported tables — the parse-time style record is the SSOT of "what
  this scalar is like" (a re-serialized parsed document re-emits its
  original styles: roundtrip fidelity, psych `source` attr parity — see
  psych-pure's `Scalar#source`).
- Multi-document emission (`---` separators, directives on demand),
  anchor/alias re-emission (aliases emit `*a`; anchors re-anchor on first
  occurrence — cycle-safe by construction since parse rejected cycles).
- Line endings: `\n` always (libyaml parity); a CRLF-in is normalized-out
  at scan; documented.

## Phases

A. Sizing + writer + style table (block mode) + roundtrip property test.
B. Flow mode, folding, width; canonical mode; byte-stability gates.
C. Streaming writer; `serialize_into` FFI seam; perf pass (ledger).

## Acceptance

- ≥ 5× libyaml emitter on the benchmark corpus (18's matrix, CI-recorded).
- Byte-stable roundtrip property test over the whole corpus (100% pass).
- libfyaml `emitter-examples` corpus + libyaml `run-emitter` port (17)
  produce expected output or documented divergences.
- Zero reallocations after the sizing reserve (alloc-hook gate).

## References

- `~/src/leptris/leptris/src/leptris/serialize/serialize.c` (sizing +
  `serialize_into` + SIMD padding), `~/src/external/libyaml/src/emitter.c`
  (semantics), `~/src/external/libfyaml/src/lib/fy-emit.c` +
  `test/emitter-examples/`, psych-pure dump path (`lib/psych/pure.rb`).
