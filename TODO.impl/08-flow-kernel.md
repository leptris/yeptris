# TODO.impl/08 — Flow kernel: the JSON-superset fast path

Status: v1 SHIPPED inside 07 (e_flow: correct YAML flow, suite+differential green). This item's remaining substance: C (JSON corpora — jsonsuite runner + pinned verdicts landed 2026-09-01; strict RFC 8259 `yeptris_parse_json` mode remains) and A's SIMD structural fast path (profiled wall: ~80 ns/event; PERF-LEDGER 2026-09-01). B (number kernel) is typed-access perf, not parse perf. NOTE: the deliverables text below is wrong about trailing commas — libyaml ACCEPTS them and the YAML 1.2 flow grammar allows them; yeptris follows spec+libyaml (jsonsuite verdicts pin it) · Depends: 07 · Layer: `src/yeptris/parse` · PLAN.md phase: 1

## Goal

A dedicated kernel for flow context (`[…]`, `{…}`) that is simultaneously a
simdjson-class JSON parser — JSON is a strict subset of YAML 1.2 flow — and
a correct YAML flow parser (plain scalars, anchors/aliases/tags, JSON-style
`"key":value` without spaces). One kernel, both guarantees.

## Deliverables

- `parse/flow.c` — flow-mode frame handling inside the engine (07):
  - structural pass over the flow span via `classify16/32` +
    `stopset_find` (`,`, `:`, `[`, `]`, `{`, `}`, quotes, line-break) —
    newline folding rules inside flow (newlines act as spaces; a line
    break inside flow is never an indent event);
  - JSON compat: after a quoted key, `:` immediately followed by value
    (no required space); after plain keys, spec spacing (`: ` or `:,`);
    trailing commas are errors in both modes (parity check against
    libyaml, which rejects them);
  - nested depth counted toward the shared depth guard;
  - comments inside flow: only outside quoted scalars, spec-legal
    positions only.
- `parse/number.{h,c}` — Eisel–Lemire (fast_float) port for the
  1.2-core float/int fast paths: branchless integer accumulate with
  overflow → double fallback; the slow/precise path (huge exponents,
  64-bit boundary mantissas) falls back to a correct loop — parse
  correctness is never traded for speed (arXiv 2101.11408 discipline).
- JSON conformance suite runner (`test/flow/`): drives the kernel with
  corpora from `ruby-json`, `json-c`, `yajl`, and libfyaml's
  `jsontestsuite.test` — every valid JSON document must parse to the
  identical event/value stream, every invalid one must fail.

## Design decisions

- The kernel is used by **both** worlds: YAML flow and raw-JSON mode
  (`yeptris_parse_json`, a thin entry that forces flow mode for the whole
  document) — SSOT: one implementation of JSON grammar, exercised by
  three test suites.
- Number results are stored as views + a resolved-kind bit by the
  resolver (10); the kernel itself never converts to double unless the
  caller asked for typed values (lazy — the simdjson On-Demand lesson:
  don't pay for what isn't consumed).
- Escapes: quoted-scalar processing delegates to 09's tables; flow never
  duplicates escape truth.

## Phases

A. Structural flow pass + scalar delegation; yaml-test-suite flow cases
   (incl. the notorious flow-in-block and block-in-flow edge cases).
B. Number kernel + differential tests vs `strtod`-exact reference on a
   boundary-value corpus (ported from fast_float test sets).
C. JSON-mode entry + full JSON corpora green.

## Acceptance

- 100% of the JSON corpora behave correctly (valid→equal values,
  invalid→error) — the same bar libfyaml's jsontestsuite holds.
- Number parsing ≥ 500 MB/s on numeric-dense flow input (ledger entry).
- Roundtrip: flow-heavy documents survive `parse → emit → parse` equality
  (property test with 13).

## References

- `~/src/external/simdjson` (structural pass + On-Demand laziness),
  `~/src/external/ruby-json/test/json`, `~/src/external/json-c/tests`,
  `~/src/external/yajl/test/parsing`, `~/src/external/libfyaml/test/jsontestsuite.test`.
