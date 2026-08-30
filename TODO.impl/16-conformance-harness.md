# TODO.impl/16 — Conformance harness: yaml-test-suite + corpora + divergence ledger

Status: active (corpus fetched, runner pending) · Depends: 07, 12 · Layer: `test/conformance` · PLAN.md phase: 1

## Goal

One harness that answers "how conformant are we?" with a number and a
named list of divergences — the bar libfyaml set (100%) and the discipline
to never regress silently.

## Deliverables

- `scripts/fetch-corpora.sh` — pinned-revision fetch (recorded SHA, not a
  submodule) of the yaml-test-suite + libyaml regression inputs +
  examples into `test/conformance/data/` (cached; reproducible).
- `test/conformance/runner.c` (GTest-driven): for each suite test:
  parse input (all four consumption models — cross-model differential by
  construction), compare our event stream against the suite's `test.event`
  expectation (format adapter in `test/conformance/event_format.c` — the
  single place that knows the suite's JSON vocabulary);
  error cases compared by error-class, not message text (the suite's
  error semantics are fuzzy — adopt the libfyaml/ryml classification of
  ambiguous error tests, listed explicitly).
- JSON-corpus runner: libfyaml's `jsontestsuite.test` + ruby-json/json-c/
  yajl corpora through the flow kernel (08).
- `VALIDATION.md` — the **divergence ledger**: every failing/accepted
  divergence with suite id, cause, and decision (bug / compat policy /
  upstream-spec-ambiguity). CI fails if an unlisted divergence appears
  (the never-regress-silently gate) — a closed ledger is the 100% claim.
- Corpora manifest (`benchmarks/corpora.json`): one manifest shared by
  conformance, benchmarks (18), and fuzzing (19) — SSOT of inputs.

## Design decisions

- The event-format adapter is the only suite-coupled code; swapping
  suite revisions touches it alone (OCP at the harness level).
- Divergences are tests with expectations, not skips: an accepted
  divergence gets a fixture asserting our exact behavior — "different,
  deliberately" beats "ignored".
- Error-class taxonomy: `syntax-error` vs `semantic-error` vs
  `unsupported`; mapping table from our error codes (02) to classes —
  declared once beside the X-macro.

## Phases

A. Fetch + runner + event adapter; establish the baseline number (expect
   ugly at first — the ledger starts honest).
B. Drive 07–09 fixes to ≥ 90% parity (Phase-1 exit gate of PLAN.md).
C. Close to 100%-or-documented by Phase 5 (every entry justified).

## Acceptance

- Runner executes in CI on every push; PASS % + ledger delta in the job
  summary.
- Final: 100% of suite tests either pass or carry a ledger entry with an
  assertion fixture — zero skips.

## References

- yaml/yaml-test-suite (fetched), `~/src/external/libfyaml/test/`
  (corpora + how the conformance leader classifies ambiguity),
  `~/src/external/psych-pure/spec/*.tml` (extra behavioral corpora).

## v1 shipped (2026-08-30)

- `scripts/fetch-corpora.sh` + the yaml-test-suite cloned at
  test/conformance/data/yaml-test-suite (352 .yaml cases, gitignored).
- The runner: `test_conformance` (suite.{h,c} frontmatter loader with
  de-visualization of U+2423 ␣ markers and fail:true error cases;
  tree.{h,c} — THE event-format adapter; main.c with --verbose/--id/
  --progress + conformance-failures.txt).
- **Baseline 41.3% → 43.0% → 49.9% (175/351; round 4 rewrote the flow-plain token loop — EOL handling now advances from the piece end (was re-scanning from the pre-scan position, producing phantom-space folds), empty tokens return to the separator path, leading ":" rides the scanned span. Was 47.3% (166/351; round 3: empty-scalar for content-less docs, prop-prefixed keys open maps at the node column, flow colon may cross a line break, block-scalar leading blanks, flow key-position colon rule)** through suite-driven fixes:
  YAML 1.2 trailing flow commas; JSON-style `:` after quoted flow keys;
  flow plain scalars may start with a non-structural `:` (`::x`);
  `?foo`/`:foo` (indicator + non-blank) are plain, not markers;
  root-level values on following lines at any indent (`&a\n- x`).
- Engine bugs found & fixed by the harness already:
  - `unquoted : value` in flow (space before colon) — colon detection
    skipped spaces; bare ':' at token position is structural (infinite
    loop in e_flow);
  - `{"foo"\n: "bar"}` — ':' starting a line inside flow;
  - `--- > folded` — doc markers with same-line content were not
    recognized (scan_line required line-exact markers);
  - root-level multi-line plain scalars (`a\nb` is ONE scalar at
    document level — continuation may sit at column 0);
  - blank-line handling in plain continuation loops (park at line end
    before counting the break — infinite loop otherwise).

## Remaining phases (next work)

1. Runner is LIVE; remaining mismatch clusters: empty-value-before-close (~10), explicit/complex keys, block-scalar edge accounting, ↵/NEL rendering. Runner is a default ctest target (stale-binary trap removed).
2. (was) parse every suite input (all consumption models),
   compare event streams against test.event expectations via the event
   format adapter; error-class comparison for error cases.
2. Baseline number + divergence ledger (VALIDATION.md).
3. libfyaml corpora (jsontestsuite, emitter-examples) into the manifest.
4. CI wiring: fetch + run, PASS% in the job summary.
