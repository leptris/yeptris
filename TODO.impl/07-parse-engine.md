# TODO.impl/07 — Parse engine: one resumable state machine

Status: active (v1 shipped) · Depends: 06 · Layer: `src/yeptris/parse` · PLAN.md phase: 1

## Goal

The SSOT of grammar: **one** parser — a resumable, non-recursive state
machine that consumes scan facts and emits internal events into a sink.
DOM, pull, push, and recorder (11/12) are all sinks over this one engine.
This is the leptris lesson ("all SAX parsing routes through one state
machine; the recursive parser was removed") applied at birth.

## Deliverables

- `parse/engine.{h,c}` — `yeptris_engine_step(engine) → event | status`:
  - explicit **indent stack** of frames `{indent, container, kind}` —
    block structure is stack depth, never call depth;
  - frame modes: block-sequence, block-mapping, flow (delegates spans to
    08's kernel as a mode, same machine — no second parser), document
    (multi-doc `---`/`...`, bare documents);
  - productions: seq entry (`- ` incl. compact `- k: v`), mapping
    (simple keys from colon-terminated spans — single-line, ≤1024 — and
    explicit `?`/`:` keys), value scalars (delegate 09), anchors `&`
    (registered in the doc intern table at event time; undefined alias at
    use = error, libyaml semantics), aliases `*`, tags (`!`, `!!`, `!<…>`,
    `%TAG` handle expansion), directives (`%YAML 1.1/1.2` check with
    warning-vs-error policy per compat mode; unknown directives ignored
    with a warning, matching libyaml), comments skipped;
  - depth guard: default 1000 (libyaml `max_nest_level` parity),
    per-parse override via options (10); overflow → error, never a crash.
- `parse/sink.h` — the event sink interface: struct of per-event-type
  function pointers + `ctx` (OCP: DOM builder, recorder, push adapter,
  pull buffer are registrable sinks; a new consumer = new sink, zero
  engine edits).
- `parse/feed.{h,c}` — chunked input API: `feed(bytes, len, final)`; carries
  scanner window + engine frames + partial-scalar state across chunks;
  `pull_new_file` variant streams bounded slices.
- Internal event shape `YepEvent` — the one grammar vocabulary
  (`DOC_START/END, SEQ_START/END, MAP_START/END, SCALAR, ALIAS, ANCHOR
  props embedded`) with style, tag id, implicit flags.

## Design decisions

- No token layer, no lookahead buffer: simple-key decisions come from
  span facts (`plain_scan` already knows the span ended at `:`). libyaml's
  token queue + 1024-char simple-key buffer exist to solve a problem our
  scan layer already solves structurally.
- Error SSOT: engine reports grammar errors with line/col/offset through
  the 02 channel; scan reports geometry errors; resolvers (10) report
  typing errors. No overlaps.
- Recursion ban is testable: a CI check (grep for self-calls in parse/
  + a deep-nesting input that would blow a C stack if recursion crept in).

## Phases

A. Block productions: documents, sequences, mappings, simple/explicit
  keys, scalars via 09, anchors/aliases/tags/directives. yaml-test-suite
  event-parity ≥ 60% (block subset).
B. Flow mode wiring (08 lands in parallel); parity ≥ 80% overall.
C. Chunked feed + streaming state-carry tests (adversarial split points).

## Acceptance

- Event parity ≥ 90% on yaml-test-suite (error-classification divergences
  tracked in the 16 ledger, not silently skipped).
- Deep-nesting input (100k levels) → clean `YEPTRIS_ERROR_DEPTH`, no crash,
  no stack growth (verified under ASAN with a low stack limit).
- Zero allocations outside `memory/` in the engine path (alloc hook gate).

## References

- `~/src/leptris/leptris/src/leptris/sax/parser.c` (one-machine precedent),
  `~/src/external/libyaml/src/{scanner,parser}.c` (grammar semantics —
  compat target), `~/src/external/psych-pure/lib/psych/pure.rb`
  (readable 1.2 grammar), yaml-test-suite (parity bar).

## v1 shipped (2026-08-30)

- `engine.{h,c}`: single non-recursive machine — explicit frame stack,
  indent unwind, sibling-frame reuse, compact "- k: v", indentless
  sequences, explicit "?" keys and ":" value lines, block scalars,
  anchors/aliases (last-definition-wins, undefined-alias errors), tags
  (incl. verbatim !<...>), multi-doc (---/.../directives), depth guard
  (1000, YEPTRIS_ERROR_DEPTH).
- Flow kernel: nested collections, single-pair maps in sequences, keys
  via quoted/plain/flow/alias, trailing-comma rejection, empty [] {},
  entry counting.
- Property semantics: same-line props attach to the following node (key
  scalars); value-position props parsed at EOL carry to the value node
  (pend) and ride collection START events.
- Public surface: yeptris_parse (BOM → validate/transcode → engine →
  DOM), yeptris_last_error with line/col; empty stream = NULL + OK.
- 17 end-to-end tests green (block/flow/scalars/anchors/tags/multi-doc/
  errors/depth).

## Remaining phases (next work)

1. ~~Resumable stepping (yep_engine_step)~~ — **LANDED**. Chunks
   accumulate in a pending buffer; every feed parses the complete-
   document prefix and keeps the tail. Cut rules (all conservative —
   ambiguity delays a cut, never mis-splits): column-0 `---` lines
   only (never `...`: that would orphan an explicit DOCUMENT_END), no
   `%` directive pending (directives travel with their document), and
   only while no quoted scalar, comment, or flow collection spans the
   position (tracked incrementally; quotes open only at value
   positions, `''`/`\` escapes and truncated markers hold back until
   more bytes arrive). Line numbers stay stream-absolute via a line
   base taken from the engine's own counter; STREAM_START/END bracket
   the whole stepped stream exactly once. The recorder (12) drives
   yep_engine_step per feed; fuzz_feed asserts the stepped stream is
   record-for-record identical to whole-buffer across 1091 corpus
   inputs, and test_stepping covers the adversarial splits (byte-
   by-byte, quote spanning a marker line, plain-text apostrophes,
   flow spanning, directive attach, explicit-end orphaning).
2. ~~Event-parity vs yaml-test-suite~~ — landed with 16 (395/395).
3. Single-pair map in a sequence whose value is a nested collection
   ("[a: [1]]") — known gap, events unbalanced.
4. Simple-key 1024-char limit; %YAML version validation; %TAG handle
   expansion; merge-key "<<" marking (resolver hook, 10).
5. Chunked feed (streaming) with carried state — **LANDED** (with 12):
   yeptris_recorder_feed steps the engine per feed.
