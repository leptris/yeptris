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

### 2026-09-01 — float printer: clean-room two-tier interval printer (LANDED)

TODO.impl/14 rewritten clean-room after the licensing decision: NO
vendored ryu code (the port was deleted before commit). Our own
implementation of the published interval method (Dragon4/Steele-White
family): tier A runs the digit loop over exact u128 integers; tier B
runs the identical loop over a fixed limb vector (exact for extreme
exponents, and the test oracle for tier A).

- Correctness: 2M random-bit round-trips + 100k shortest-oracle (vs
  minimal %.*e precision) + fixed-vs-printf parity (100k) + float32
  round-trips + boundary vectors — all bitwise. Two subtle rules
  derived and pinned by tests: the round-trip interval uses HALF-ulp
  boundaries (r=2*m2 over s=2^(1-e2)), and exact boundary hits are
  acceptable only when v's significand is even (reparse
  ties-to-even); powers of two widen the lower half by one binade.
- Throughput (bench_float, min-of-reps, loaded dev box): nice x.5
  1.7M/s = 1.40x printf %.17g; decimal-ish 1.49x printf; uniform bit
  patterns 0.09x (extremes exercise the limb tier at ~13us).
  Standalone -O2 single-shot measured 314ns (nice) / 636ns
  (decimal-ish) — bench numbers are load-contaminated; CI artifacts
  across commits are the reliable trend.
- Next lever (queued): cached-power scaling — precompute 128-bit
  10^k tables, jump straight to the digit position with two 128-bit
  multiplies, extract digits in pairs (Grisu/Ryu-class engineering,
  our own tables generated at init). Target: >100M/s on all shapes
  incl. uniform. Deferring until 13B canonical emission lands (the
  emitter decides which float shapes actually matter).

### 2026-09-02 — fast path truly engaged + strict JSON mode (WIN + fixes)

Two latent fast-path bugs surfaced by strict JSON mode's tests, both
fixed: (1) the span validator's close branch rejected the
COMMA_OR_CLOSE state — every collection whose last value completed
before the close fell back to the general kernel (the measured "+8%"
was pass-1-only engagement); (2) the emission walk never advanced
over true/false/null literals — an INFINITE LOOP once (1) let maps
with literal tails take the fast path (found by JsonMode tests,
alarm-guarded repro). Post-fix numbers on the loaded dev box:
flow-json pull 2.86x / recorder 3.01x (was 2.72/2.95); DOM within
noise. Re-measure quiet.

Strict JSON mode (yeptris_parse_json, 8C): the whole-input RFC 8259
validator lives in scan/json.c (grammar SSOT — the engine fast path
and JSON mode share the token primitives); strict gate: 95/95 y_
accept, 188/188 n_ reject (json-suite-strict ctest). JSON charset =
UTF-8 well-formedness only — RFC 8259 allows DEL and noncharacters
that YAML c-printable rejects (the front-end has a json_mode branch).
Surrogate escapes must pair (YAML 1.2): pairs combine at decode
(finish_double), lone high/low reject in the engine pre-validator and
the JSON string scanner; previously lone surrogates silently decoded
to CESU-8 garbage. i_ verdicts re-pinned (12 now reject).

### 2026-09-02 — JSON-mode entry throughput (DATA POINT)

yeptris_parse_json on a 2.8 MB strict-JSON array (the flow-json shape
re-cast): 63.0 MB/s DOM vs 66.3 MB/s YAML-mode on the equivalent YAML
corpus (min-of-20 x 10 iters, quiet dev box) — the strict whole-input
validation pass costs ~5% over the general front-end, and the engine's
fast path carries both. The simdjson-class gap (GB/s) is the
direct-from-index DOM (task: skip the event pipeline; pre-size the
node array from validation token counts) — the measured per-event wall
(~80 ns/event) is the same wall documented 2026-09-01; that unit is
the single next lever for the JSON race and reuses scan/json.c's
validator to produce the index.

### 2026-09-02 — direct-from-index JSON DOM (WIN: 2.2x on JSON entry)

yeptris_parse_json now builds the DOM straight from the validated
buffer (yep_dom_build_json): one walk creates and links nodes with no
engine, no event structs, no sink dispatch, no deferral machinery —
strings unescape directly into the DOM pool (one copy; the engine
path pays two), numbers resolve through the core12 resolver (typing
SSOT), count semantics match the event sink exactly (children,
halved by the accessors).

Measured (2.8 MB strict-JSON array, 40k objects / 160k pairs,
min-of-50 x 10 iters): 63.0 -> 135.9 MB/s = 2.16x. Post-format
re-run 122.1 (noise band; CI artifacts track). Structure verified:
tree shape, values, serialize->reparse roundtrip; all 131 gates
green including both jsonsuite gates, jsonc, json.hpp (they exercise
this path). A builder/validator disagreement defers to the engine
(belt and braces; unreachable in-tree).

Remaining JSON-gap analysis: the walk still pays resolver calls on
numbers and per-node memset; the simdjson-class next steps are
table-driven tag classification (grammar position implies the tag —
no resolver call) and node-field initialization trimmed to the
fields JSON uses. yaml-mode flow spans could adopt the same builder
by widening the validator's index, but anchors/tags/aliases make
that a separate design.

### 2026-09-02 — 18B memory measures landed (FINDING: compactness gap)

The matrix now reports per shape: allocations/MB, cumulative
allocation churn / input, and PEAK outstanding heap / input (a
size-prefixed counting allocator keeps an exact live-bytes ledger —
deterministic, platform-independent; an earlier fork+ru_maxrss
approach was COW-noise on macOS and was dropped).

First measurements (quiet dev box, --quick corpora):

| shape | allocs/MB | churn/input | peak/input |
|---|---|---|---|
| block-heavy | 5 | 36.9x | 36.9x |
| flow-json | 9 | 32.2x | 32.2x |
| scalar-heavy | 54 | 6.2x | 6.2x |
| deep-nesting | 54 | 15.5x | 15.5x |
| anchor-heavy | 47 | 54.1x | 52.4x |
| wide-mapping | 7 | 25.2x | 25.2x |

Findings:
1. allocs/MB is EXCELLENT (5-54 per MB) — amortized growth works;
   the event pipeline allocates almost nothing per event.
2. peak == churn on most shapes: allocations stay live until the
   document is freed (correct — document ownership), so peak IS the
   memory story.
3. COMPACTNESS GAP: peak/input 25-54x vs TODO.impl/11's <3x
   acceptance target. Dominated by the 56 B yep_dnode (8x input on
   scalar-dense corpora by itself) plus the DOM pool's doubling
   churn and the finish pool blocks. scalar-heavy's 6.2x shows the
   floor; block/anchor-heavy's ~37-54x shows pools + node overhead
   compounding. This is the top memory lever: arena-allocated
   compact nodes (the 11 board's original design: int32 offsets,
   per-kind tails) would land ~3-6x. Recorded as the follow-up unit.

## 2026-09-02 — O(1) map lookup (11: dom/mapindex)

Lazy per-mapping index (built on first `map_get`): 20k-key map lookup
**217 ns** including the per-call handle allocation — the linear scan
it replaces averages ~10k view comparisons (memcmp) on the same map,
i.e. tens of µs. First-use build cost is one pair walk (~150 µs on the
20k map, measured in-suite); mutation (add/set/del) frees the table —
no pool leak (tables live on the system allocator). Concurrent
first-lookups serialize on the document index mutex; probes after the
build are immutable reads (read-sharing contract intact, TSAN-clean).
