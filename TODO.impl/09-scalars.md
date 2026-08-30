# TODO.impl/09 — Scalars: trim, fold, unescape, block scalars, style recording

Status: active (v1 shipped) · Depends: 07 · Layer: `src/yeptris/scalars` (parse/) · PLAN.md phase: 1

## Goal

Every scalar's path from raw span to final value view, with one
representation (`YepView`), zero copies when possible, and one style record
that the emitter reuses. Scalar truth lives here — exactly once.

## Deliverables

- `scalars/plain.c` — trailing-whitespace trim as **view adjustment**
  (zero-copy; leading/trailing spaces stripped by shrinking the view);
  multi-line folding (single `\n` → space, N blank lines → N `\n`,
  leading/trailing line-space stripping per spec) → arena copy **only when
  the scalar spans multiple lines**; single-line plain scalars are always
  borrowed.
- `scalars/single.c` — single-quoted: `''` → `'`; copy only when a `''`
  exists or folding applies (quote_scan flag decides).
- `scalars/double.c` — double-quoted: processing gated on `has_escape`
  (from `quote_scan`); escape tables are **declared here** — a 256-entry
  table of `{is_escape, replacement_len}` plus `\x \u \U` width decoders;
  the emitter (13) **imports** these tables for sizing/escaping — SSOT,
  never redeclared. Line continuation `\` at EOL and folding rules included.
- `scalars/block.c` — literal `|` / folded `>` content assembly with
  chomping (clip/strip/keep) and explicit/auto indent detection; content
  built as arena `memcpy` runs over the line spans from
  `block_scalar_scan`; the "more-indented lines keep newlines" rule in
  folded mode.
- `scalars/style.{h,c}` — `YepScalarInfo { style, flags }` recorded at
  parse: plain/plain-safe?, has-escape, multiline, leading/trailing-space,
  special-first-char, contains-flow-indicator… The emitter's style chooser
  (13) consumes this record instead of re-analyzing — the decision is made
  once (leptris single-representation lesson).

## Design decisions

- Copy policy is data-driven and testable: borrowed unless (multiline) ∨
  (escape exists) ∨ (owner requires owned input). A debug mode asserts the
  policy table matches actual behavior.
- `!!binary` is **not decoded here** — core marks the tag (10); base64
  decoding is a binding-level codec decision (Psych parity: Psych's Ruby
  side does it). Lean core.
- Timestamps/implicit typing: not here — 10's resolvers read views +
  style flags; scalars never guess types.

## Phases

A. plain + quoted paths + goldens (yaml-test-suite scalar sections +
  psych-pure `parse-scalar.tml` cases).
B. block scalars (indent-indicator, chomping, edge blanks) + goldens.
C. Style record completeness: for the emitter roundtrip test (13), every
  parsed scalar carries enough info to re-emit byte-identically in
  canonical mode.

## Acceptance

- Borrowed-scalar ratio on block-style corpus ≥ 90% single-line case
  (measured, ledger) — the zero-copy claim made testable.
- psych-pure scalar corpora + yaml-test-suite scalar cases: 100% green.
- No scalar path allocates when its policy says borrowed (alloc-hook gate).

## References

- `~/src/external/libyaml/src/scanner.c` (scalar productions + historical
  quirks to match in compat), `~/src/external/libfyaml/src/lib/fy-atom.c`
  (atom analysis semantics), `~/src/external/psych-pure` `parse-scalar.tml`,
  psych `test_scalar.rb` / `test_scalar_scanner.rb` (Ruby-side split of
  labor).

## v1 shipped (2026-08-30)

- `scalars.{h,c}`: double-quote finishing (full escape set incl. \x \u
  \U \N \_ \L \P, line continuations, multi-line folding), single-quote
  '' doubling + folding (borrow kept when clean), plain folding (1 break
  → space, n → n-1 newlines), block scalars (literal/folded, explicit
  indent digit, clip/strip/keep chomping, more-indented lines keep
  breaks). Escape truth declared once here; kernels gained '' handling
  (differential suite updated).

## Remaining phases (next work)

1. Folded-block more-indented edge cases vs the yaml-test-suite (fold
   semantics around blank+more-indented boundaries).
2. Style-record completeness for the emitter (has-escape / multiline
   flags are implicit today; 13 needs them explicit).
3. Suite-driven refinements after 16's runner lands.
