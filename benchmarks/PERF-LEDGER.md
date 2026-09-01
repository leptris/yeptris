# PERF-LEDGER — yeptris performance record

The discipline file (TODO.impl/18): every optimization attempt recorded
with before/after numbers, wins and dead ends alike. Machine-relative
ratios (same-run libyaml baseline) are the currency; absolute MB/s are
per-machine context only.

## Methodology

- Harness: `benchmarks/matrix/bench_matrix` (`--quick` CI / `--full`
  releases), min-of-iters timing, deterministic seeded corpora.
- Every measured cell probe-parses first: a corpus that fails to parse
  reports `n/a`, never a fake fast failure.
- `YEP_BENCH_DUMP_DIR=<dir>` writes every generated corpus so any
  number here is reproducible byte-for-byte (`--seed` for the synthetic
  shapes; realworld is the committed snapshot set).
- Reference: libyaml linked via `-DYEPTRIS_BENCH_LIBYAML_ROOT`
  (mandatory — it is the mission). Weakest cell is the headline.

## Baseline 2026-09-01 (Apple silicon, Release+LTO, --quick)

| shape | DOM | pull | recorder | emit | libyaml |
|---|---|---|---|---|---|
| block-heavy | 3.1x | 3.8x | 3.9x | 6.2x | 1.0x |
| flow-json | 2.5x | 2.7x | 2.8x | 6.9x | 1.0x |
| scalar-heavy | 3.6x | 3.4x | 3.5x | 4.8x | 1.0x |
| anchor-heavy | 2.2x | 2.9x | 3.0x | 6.7x | 1.0x |
| deep-nesting | 2.6x | 3.3x | 3.5x | 6.2x | 1.0x |
| wide-mapping | 3.5x | 4.0x | 4.1x | 6.3x | 1.0x |
| realworld-suite | 2.9x | 3.1x | 3.3x | 5.6x | 1.0x |

Every shape beats libyaml on every measure. Weakest cell: flow-json
DOM 2.5x — the JSON-class kernel (TODO item 08 hardening + simdjson
structural indexing) is the next headroom.

## Entries

### 2026-09-01 — anchor registry: linear scans → hash interner (WIN)

`common/nametab` (open addressing, FNV-1a + murmur finalizer, linear
probing, 70% load) replaces the O(n) anchor arrays in both the engine
and the DOM.

- Before: anchor-heavy (40k anchors, 1.8 MB) DOM **0.50 MB/s = 0.02x**
  libyaml — every define/lookup rescanned the whole table.
- After: DOM 60–66 MB/s = **2.1–2.2x** libyaml (pull/recorder ~3x).
- Same change: anchor scope fixed to document-lifetime (cleared at
  DOCUMENT_END, matching libyaml; was silently stream-lifetime).
- Also removed two latent bugs the swap exposed: `free()` before the
  NULL guard in both `yep_engine_destroy` and `yep_dom_destroy`.

### 2026-09-01 — anchor caps at 2048 (BUG, found by the harness)

The benchmark's 40k-anchor corpus failed to parse: engine and DOM
anchor tables were fixed 2048-entry arrays that silently dropped
registrations. Made growable first, then replaced by the nametab above.
Lesson: scale-test corpora are correctness tools, not just timing
tools — a capped registry passed every suite fixture.

### 2026-09-01 — props-only document swallowed the next document (BUG)

Found by delta-debugging the realworld corpus (30-byte repro):
`---\n!\n---\nfoo: bar` failed — `e_parse_value`'s following-lines
loop treated a `---`/`...` boundary line as the pending value's
content because the `depth == 0` root case bypassed the indent test.
Boundary lines now route to `empty_value`, which attaches the pending
props to the empty scalar (also fixing tag loss on `---\n!`).
Regression: `Parse.PropsOnlyDocumentThenBoundary`.

### 2026-09-01 — realworld corpus construction (METHOD)

Bare concatenation of the snapshot corpus is invalid YAML (and error
fixtures are unparseable by design). Final form: each snapshot framed
as its own document (`---\n` prefix unless self-opening; `...`
boundary + guaranteed marker for directive-led files; `---word1` is a
plain scalar, NOT a marker — check the terminator), probe-filtered to
files that parse. With libyaml linked, filtered to files BOTH parse:
the race needs common ground; yeptris-only conformance wins (e.g. 2JQS
empty-key mappings, suite-valid, libyaml-rejected) stay in the
conformance suite where the comparison belongs.
