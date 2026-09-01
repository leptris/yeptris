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
- CI (`bench.yml`) races the same matrix on shared runners with
  libyaml pinned at `90a56d4500aa1a1798514c5cb55c3ad4cb095f94`.
  Shared-runner ratios run lower than dev-machine ratios (noisy
  neighbors, throttling): first run measured a 1.35x floor. Compare CI
  artifacts ACROSS COMMITS on the same runner class, not against the
  dev-machine table.

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

### 2026-09-01 — flow-json tuning pass (FLAT, kept: strictly less work)

Profiled the flow kernel (sample, no-LTO build, per-line symbolization).
Two real inefficiencies fixed:

- `e_quoted_floor` walked every quoted scalar's span up to three extra
  times byte-wise (multiline detect, escape pre-validation, newline
  count). Now one SIMD `stopset_find` pass finds breaks (folding the
  newline count in), and escape validation is gated on `has_esc`, which
  `quote_scan` already reports. Strictly less work per quoted scalar.
- The flow loop's line-start checks ran `yep_scan_line` twice per line
  (loop head + post-ws); memoized per line (`li_cache`, invalidated on
  every line advance and run reset).

Measured: flow-json DOM 2.51x (was 2.44–2.53x) — flat within noise.
All other shapes held or improved slightly (deep-nesting DOM 2.63 →
2.90x, scalar-heavy DOM 3.59 → 3.66x). Kept: correct and less work,
but the wall is elsewhere — per-event machinery. flow-json costs
~80 ns/event (240 cycles) across ~14 events per line, spread thin:
frame-state guards, event init+copy, resolver, DOM node create+link,
single-pair-key deferral replay (`e_buf_send`), ws skipping. No single
hotspot above ~12% remains. Conclusion: the next 2x on flow-json is
architectural — a JSON-class structural fast path (SIMD scan → bulk
events for pure-JSON flow spans, TODO 08/21) or event-struct slimming
(invasive). Recorded here so the next attempt starts from this wall.

### 2026-09-01 — JSON-class structural fast path (WIN, +8% isolated A/B)

`e_flow_json` (engine.c): pure-JSON flow spans validate in one linear
pass (strict RFC 8259 grammar; single stopset walk per string covering
close/break/escape) then emit events in a second tight walk, skipping
the general kernel's per-node guards. STRICTLY conservative — tabs,
comments, YAML scalars, trailing commas, non-string keys, indent
anomalies, or a key-position colon all fall back to the general kernel
untouched, so semantics cannot diverge (gates: 98/98 incl. conformance
395/395, libyaml-diff 0, json-suite verdicts unchanged). Hooked ahead
of `e_skip_flow`, replacing its pre-scan when it applies.

Isolated A/B (300 iterations, -O3, single process, YEP_NO_FAST toggle
in a scratch build): 41.7 → 45.0 MB/s (+8%). Matrix runs on the shared
dev box (load 16–38 today) read flow-json DOM 2.0x–2.5x — within the
2.44–2.53x baseline noise band; re-measure on a quiet machine.

Analysis: the general kernel's per-node guards were ~15% of event cost,
not the 40% hoped. The confirmed wall is the per-event pipeline itself
(~80 ns/event): event init (struct memset+copy), emit_now resolution,
DOM node create+link. The next 2x on flow-json is event-pipeline
slimming — smaller `yep_event`, batch sink entry point (`on_events(ev[],
n)` vtable slot, DOM overrides with a tight loop) — a wider unit;
this fast path is the foundation strict-JSON mode (08C) builds on.
