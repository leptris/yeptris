# TODO.impl/16 — Conformance harness: yaml-test-suite + corpora + divergence ledger

Status: pending · Depends: 07, 12 · Layer: `test/conformance` · PLAN.md phase: 1 (start) → 5 (finish)

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
