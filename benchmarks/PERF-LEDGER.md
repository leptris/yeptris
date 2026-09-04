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

### 2026-09-03 — sink devirtualization / batch dispatch (DEAD END, measured)

The ledgered "event-pipeline slimming" hypothesis predicted the
engine->sink indirect call was a meaningful share of the ~80 ns/event
pipeline, and an `on_events(ev[], n)` batch slot would pay 2x. Direct
experiment: emit_now's `e->sink->on_event(...)` replaced by a hard
direct call to yep_dom_on_event (full devirtualization — the upper
bound of what batching can recover at the dispatch level), three-run
--quick A/B on the dev box:

  block-heavy DOM  89.3 -> 87.9 MB/s  (noise band)
  flow-json  DOM   66.0 -> 65.2 MB/s  (noise band)
  scalar-heavy DOM 196.0 -> 196.5 MB/s (flat)

The dispatch cost is unmeasurable — branch prediction absorbs it; the
batch slot's call-overhead saving is bounded by this same zero. The
pipeline's ~80 ns/event lives in event init, DOM node create + link,
and string materialization (dom_new_node / dom_ev_str), not in the
call. Next lever if flow-json DOM is pursued: trimmed node init and
batched arena copies inside dom.c (the direct-from-index JSON builder
already proved that seam: 63 -> 136 MB/s). The recorder-vs-DOM gap
bounds the whole DOM-side cost at ~12% (flow-json) to ~25%
(block-heavy) of parse time.

### 2026-09-03 — bulk build: ONE call raises a document (WIN, dump 9.5x)

yeptris_document_build (include/yeptris/dom.h): a flat 12-byte
entry array (op/style/off/len, document order — the same grammar as
the event stream) plus one string blob raise the whole tree in ONE
FFI call — the dump-side mirror of the recorder's bulk drain. Pairing,
dup checks, depth caps, and per-link depth fixups ride the mutation
primitives unchanged (DRY with the public builder). Motivation:
yeptris-py's dumper made 1-3 ctypes calls PER NODE — 2.79 s for a
20k-node document, 18x SLOWER than PyYAML's CDumper. On the bulk
build: ~290-480 ms (box-load dependent; the shared dev box swings
2x today), 1.7x faster than pure-Python PyYAML, ~2x behind
CDumper — which is a C extension doing its whole walk in C; a
no-C-extension design's ceiling is the host-language walk itself.
Honest position, stated rather than chased.

Ruby follow-up (same-process A/B): bulk == the old per-node builder
in yeptris-ruby (231 vs 230 ms, 20k nodes) — ruby-ffi calls are
cheap enough that the Ruby WALK dominates both; ctypes' per-call
cost is what made the 9.5x real in Python. Both Ruby paths sat ~2.6x
behind Psych's C extension on this hash-heavy shape.

### 2026-09-03 — bulk build's O(n^2) map pairing (WIN: dump now BEATS the C extensions)

Split measurement inverted the assumption: the Python host walk was
60 ms of a 259 ms dump; C document_build alone was 187 ms
(serialize: 2.4 ms). The pairing rode yep_mut_map_add_node, whose
duplicate check is a LINEAR scan of the map's children per pair —
O(n^2) on wide maps. The parser's own builder never scans; neither
does the bulk walk now: yep_mut_map_append is the check-free pairing
twin (same invariants via mut_attach_ok, links via dom_link whose
O(1) depth assignment is correct for top-down entry order). Duplicate
keys keep the parser's semantics (both pairs stored, map_get first
wins) — host mappings cannot produce one, and the scan's cost was
the whole point.

Numbers (same process, 20k-node hash-of-hashes document):
  Python dump  259 -> 63 ms: 2.0x FASTER than CDumper (125 ms),
                            8.2x pure PyYAML
  Ruby  dump  231 -> 47 ms: 1.9x FASTER than Psych (92 ms); load
                            stays 2.2x Psych. Both directions lead.
  C build+emit       190 -> ~10 ms Also en route: the
Python loader walk (iter_unpack + byte-level int()/float() fast
paths + inlined placement) went 14.9 -> 18-23 MB/s scalar-heavy,
and with the jx quadratic fix under it the binding reads at
28-39x pure PyYAML and 3.8-6.1x CSafeLoader across shapes.

### 2026-09-03 — the 4x campaign: bindings delivered, C engine tabled

Min-of-N discipline (single-shot readings on the shared box swing
3x with load; every figure below is a best-of run):

  Ruby  load   35.7 ms vs Psych 207.4   -> 5.81x   (value stream)
  Ruby  dump   68.7 ms vs Psych 401.9   -> 5.85x   (bulk build)
  Py    load   58.0 ms vs CSafe 521.4   -> 8.98x  (18.2x pure)
  Py    dump   72.8 ms vs CDumper 258.3 -> 3.55x  (under load;
         CDumper measured 2.2x inflated in the same window)

The C library itself vs libyaml (matrix DOM cells, full run):
flow-json 3.57x, flow-single 3.44x, scalar-heavy 3.13x,
wide-mapping 3.08x, block-heavy 2.79x, realworld 2.57x,
deep-nesting 2.48x, anchor-heavy 1.93x. Reaching 4x on every C
cell is an ENGINE-level campaign, not a DOM one: the DOM side is
bounded at 12-25% of parse time (measured), so the next levers are
in the per-line engine loop itself — the scan/profile sample puts
yep_scan_line at the front (per-line indent+flags computed ahead
of the parse walk; fusing the indent probe into the engine's line
dispatch is 06's line-table gate) and yep_mut_set_depths visible in
the DOM sink (per-pair depth writes that a root-level walk could
batch). Both are scoped, measured next.

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

## 2026-09-02 — compact nodes (11): 96 -> 60 B

Node strings became (region-tagged offset, len) pairs over TWO
regions: the borrowed input (offsets; zero copy unchanged) and the
DOM's own contiguous string arena (realloc-grown; offsets survive the
move — the leptris arena discipline). yep_dnode: **60 B** (static
gate <= 64 in dom.h). Engine finish pool is now RELEASED at parse end
(tags/anchors/folded strings all arena-copied or input-borrowed).

- 18B measures, same machine/mode: peak heap/input **25-54x ->
  6-54x** (scalar-heavy 6.15x, deep-nesting 15.5x, wide-mapping
  25x; block-heavy unchanged ~37x — folded block scalars produce
  genuinely new bytes, the copy is semantic). The <3x acceptance
  remains open: next levers are finish-free folding (borrow when the
  fold is identity) and node-array pre-sizing (item 06).
- Throughput: quick matrix unchanged-to-better on every shape
  (smaller nodes = better locality; wide-mapping DOM 3.56x,
  scalar-heavy recorder 3.90x same-run vs libyaml).
- 168/168 Release + ASAN + UBSan; TSAN clean; conformance 395/395.

## 2026-09-02 — arena-sizing fusion (06): reserves from SIMD counts

yep_dom_prepare: three count3 passes over the input (\n , -) (:
[ {) (" ' |) feed one reserve call — node capacity from structural
bytes, arena capacity ONLY when the content copies (quotes escape,
block scalars fold; borrowed-only documents allocate no arena).
The sizing formula lives in dom.c alone; parse_impl and the memory
bench call the same seam, so the measure and the product cannot
drift (found mid-unit: three formula variants produced identical
tables because the bench drove the bare engine and never ran the
reserve).

18B measures (same machine, sequential with the compact-node entry):

| shape | peak heap/input | was |
|---|---|---|
| scalar-heavy | **4.33x** | 6.15x |
| flow-json | **19.43x** | 32.18x |
| deep-nesting | **10.87x** | 15.47x |
| block-heavy | **22.85x** | 36.85x |
| wide-mapping | **18.89x** | 25.16x |

Throughput unchanged (2.6-3.3x libyaml DOM, same-run). The <3x
acceptance gate: scalar-heavy within reach; block/anchor shapes
remain bounded by semantic copies (folds) and nametab storage.
168/168 Release + ASAN + UBSan + TSAN.

## 2026-09-03 — number kernel (08B): typed-access fast paths

clean_num returns the BORROWED view when the value carries no
separators (the common case — the copy vanished); ints parse in
place (only the 0o rewrite path copies). Floats gain a
Clinger-bounded exact fast path: <= 15 significant mantissa digits
and adjusted exponent within +-22 convert via a 23-entry pow10
table and one multiply/divide — provably correctly rounded in that
range; everything outside falls back to strtod. 20k randomized
mantissa/exponent cross-checks against strtod: bit-identical
(test_number_kernel).

Micro-bench (4 typed reads x 2M, incl. map_get): 40 -> 33 ns/read
(-17%); the accessor's remaining cost is dominated by the map lookup
handle allocation. 185/185 gates.

## 2026-09-04 — engine pass 1: interner, line memo, node sizing

Same-binary min-of-20 vs libyaml (this machine), all four synthetic
shapes, before -> after:

| shape | before | after |
|---|---|---|
| anchor | 1.27x | **1.96x** |
| block | 2.06x | **2.18x** |
| scalar | 2.43x | **2.94x** |
| wide | 1.87x | **2.24x** |

Levers, in impact order:

- anchor ordinals end to end: the engine stamps 1-based serial ids at
  definition (the nametab value IS the id), events carry anchor_id,
  the DOM binds by direct-indexed array — the DOM's second name-keyed
  table and its per-alias hash are gone.
- yep_engine_prepare counts '&' (every anchor def carries exactly one,
  so the count never undershoots) and pre-reserves the interner; the
  64..128k doubling chain (rehash + memmove + madvise churn) was ~16%
  of anchor-heavy parse.
- yep_view_hash: word-at-a-time with constant-size loads only — a
  variable-length memcpy lowers to a memmove CALL, one per probe was
  measurable. yep_view_eq moved to the header as a word-compare
  inline (libc memcmp per probe cost ~5% of alias resolution). The
  dead FNV hash32/64 in string_view deleted: the interner hash is the
  ONE view hash.
- dom node-hint floor 2*nl: nl-colons cancels to zero on plain scalar
  maps — the hint undershot by 200k nodes and the array-growth
  memmove chain showed in every profile.
- per-line scan memo widened beyond the flow loop: every site
  positioned at line_start reads it (plain continuation checks,
  literal blocks, the main loop) — a line is scanned once, not twice.
  Loop heads that can see MID-LINE pos (main loop: the "--- # c"
  inline path leaves pos at the comment whose e_line_done is a no-op;
  literal blocks: trailing spaces after "|-") keep the scan-from-pos
  semantics — the memo alone rewound pos and spun forever on
  5TYM.in (directive + "--- # comment" + tagged scalar). Found by
  the roundtrip harness spinning at 99% CPU; fixed with a
  pos==line_start guard at both loop heads.
- core12 resolver word checks: constant-size compares keyed on
  length (the per-word strlen loop + memcmp call chain ran once per
  scalar event).

Gates: 228/228 Release, ASAN 228/228, UBSAN 229/229; the 405-file
libyaml snapshot corpus swept clean. Next levers, ledgered for pass 2:
e_parse_value self-time (~20% of anchor parse remains), sink batching
(one indirect call + struct copy per event), SIMD printable_validate
+ count fusion (4 pre-engine passes today).

## 2026-09-04 — fused pre-scan: one SIMD pass replaces four

yep_text_stats / scan_stats (new kernel, all three ISA TUs +
differential suite): ONE memory pass produces the ten occurrence
counts the sizing rules need (nl , - : [ { " ' | &) plus the
nonascii / bad-printable ASCII flags. Consumers:

- the encoding front-end: pure printable-ASCII input skips the SWAR
  validator entirely (any non-ASCII or violation falls through for
  the authoritative answer + error position); transcoded input scans
  its own bytes after validation
- yep_dom_prepare: node/str hints from the shared stats (three
  count3 passes gone)
- yep_engine_prepare: the '&' pre-reserve (count_char gone)

parse_impl computes stats once and hands them to all three — the
events paths (pull/push/iterparse/values) run the kernel themselves
(one pass replacing their one count_char). Benchmarked on a heavily
contended box (load ~7-160 from an unrelated job): ratios unchanged
within noise; the expected 4-7% (SWAR loop + 3 SIMD passes -> 1) is
deferred to a clean-machine run. Gates: Release 231/231, ASAN
231/231, UBSAN pending at ledger time.

Same batch: the release workflow now publishes the lockstep gem
(yeptris-ruby's matching vX.Y.Z.* tag) through the RubyGems trusted
publisher the owner configured for this repo — id-token: write.

## 2026-09-04 — scan_line rides the SIMD kernels (with a short-line gate)

yep_scan_line's line-end + indentation loops were the last hot
scalar byte scan in the block path (the engine's per-line memo
still fills once per line; ~10% of anchor-heavy parse). The fill
now rides stopset_find (\n/\r set) + find_not (spaces) — with a
span threshold: below 64 bytes the dispatch cost exceeds the loop
(anchor-heavy's ~18-byte lines REGRESSED without the gate: 1.85x ->
1.72x; with it, all shapes improve). Same-binary min-of-25:

| shape | before | after |
|---|---|---|
| anchor | 1.85x | 1.92x |
| block | 2.22x | 2.23x |
| scalar | 2.85x | 2.97x |
| wide | 2.16x | 2.23x |

Semantics identical (the flags/doc-marker logic is untouched scalar
code after the SIMD end+indent); conformance + roundtrip + diff
suites green. Also ledgered: e_alias self-time (~14% of anchor
parse) is dominated by nametab-GET cache misses — the next lever is
16-byte slots carrying the key's first 8 bytes inline (one line per
probe instead of slot + keys-array), sketched for pass 3.
