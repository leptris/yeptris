# TODO.impl/08 — Flow kernel: the JSON-superset fast path

Status: v1 SHIPPED inside 07; C COMPLETE 2026-09-02 (jsonsuite runner + pinned YAML verdicts + STRICT gate 95/95 + 188/188 via yeptris_parse_json; scan/json.c is the grammar SSOT; surrogate pairing enforced engine+scanner+decoder). 2026-09-03: NUMBER KERNEL (B) LANDED — clean_num zero-copies separator-free values (in-place int parse; only the 0o rewrite copies), floats take a Clinger-bounded exact fast path (<=15 sig digits, adjusted exponent +-22, 23-entry pow10; 20k randomized strtod cross-checks bit-identical), 40->33 ns/typed-read (ledger). 2026-09-03: A CLOSED — strings already ride the SIMD stopset table (scan/json.c via yep_text_active()); the remaining 'SIMD hardening' hypothesis (structural-index pass / batched dispatch) is MEASURED DEAD: full sink devirtualization is within noise on every shape (three-run A/B, PERF-LEDGER 2026-09-03), so the dispatch was never the wall. The real remaining DOM-side levers (trimmed node init, batched arena copies in dom.c — the direct-from-index JSON seam) are recorded in the ledger as future units, not open items here. B (number kernel) is typed-access perf, not parse perf. NOTE: the deliverables text below is wrong about trailing commas — libyaml ACCEPTS them and the YAML 1.2 flow grammar allows them; yeptris follows spec+libyaml (jsonsuite verdicts pin it) · Depends: 07 · Layer: `src/yeptris/parse` · PLAN.md phase: 1

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
