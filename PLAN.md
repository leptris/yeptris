# yeptris — Architecture Plan & Performance Targets

Status: plan of record, 2026-08-29. yeptris = the YAML counterpart of libleptris.
Everything below is grounded in (a) leptris's measured results and its perf ledger
of dead ends, (b) 2024–2026 SOTA literature, (c) the known cost structure of
libyaml/Psych. Where a number is a projection, it says so.

## 1. Why libyaml and Psych are slow (the cost structure we delete)

libyaml (0.2.x, still YAML 1.1):

- The scanner consumes **one byte at a time with redundant buffer bounds checks
  at every step** (independently documented by the libyaml-safer Rust port;
  "simple changes → massive improvements" per its HN discussion).
- Tokens and events are heap-allocated individually and pushed through queues;
  strings are copied through intermediate buffers. O(input) allocations.
- The emitter re-analyzes and re-grows its output buffer; no exact sizing.
- No SIMD anywhere; no zero-copy; API is callback-per-event, which through FFI
  costs more per event than the parse itself.

Psych (5.2.x, wraps libyaml):

- Inherits all of the above, then pays a **Ruby object + FFI callback per
  event**, plus `Visitors::ToRuby` / `ScalarScanner` allocation churn per node.
  Documented YAML.load regressions in Ruby's own tracker; `YAML.dump`
  asymmetry is a standing complaint.
- Symptom of ecosystem dissatisfaction: `psych-pure` (pure-Ruby YAML 1.2,
  Dec 2025) exists at all, and Psych main carries `psych_parser_fy.c` /
  `psych_emitter_fy.c` experiments against libfyaml.

Field benchmark: rapidyaml (ryml) is 2–3× libyaml (~450 MB/s on JSON-flow),
state-machine, non-recursive, passes 100% of the yaml-test-suite. libfyaml is
~1–1.5× libyaml but is the **conformance leader** (100% of the yaml-test-suite,
best-in-class diagnostics) and already the backend Psych is experimenting with
(`psych_parser_fy.c` / `psych_emitter_fy.c` in Psych main) — which makes it both
the semantics reference and a direct rival. Its architecture is worth studying
and selectively inheriting: `fy-atom` zero-copy span analysis, `fy-accel`
accelerated O(1) node access, a durable arena + test allocator with failure
injection, anchor interning via xxhash/BLAKE3. **Nobody in the YAML field ships
the full leptris machinery (AOT SIMD kernels + arena + compact DOM + zero-copy +
bulk FFI).** leptris achieved 6–14× over libxml2 on parse and 24–142× over
Ox/Nokogiri on streaming in Ruby with exactly that machinery. That is the gap
yeptris opens.

## 2. Evidence base (2024–2026 SOTA + leptris's own ledger)

| Source | Year | What it informs here |
| --- | --- | --- |
| [simdjson — Parsing Gigabytes of JSON per Second (arXiv 1902.08318)](https://arxiv.org/abs/1902.08318) | 2019 | Structural indexing, lazy/On-Demand value access, the GB/s-bar for the flow kernel |
| [Scanning HTML at Tens of GB/s on ARM (arXiv 2503.01662)](https://arxiv.org/html/2503.01662v1) | 2025 | SIMD classification kernels on NEON — same techniques as our line/indicator scanner |
| [simdzone — DNS records at millions/s (arXiv 2411.12035)](https://arxiv.org/abs/2411.12035) | 2024 | SIMD parsing of line-oriented record formats — structurally the YAML block case |
| [Number Parsing at a Gigabyte per Second (arXiv 2101.11408)](https://arxiv.org/abs/2101.11408) | 2021 | Eisel–Lemire/fast_float → port to the scalar core for YAML core-schema ints/floats |
| [Validating UTF-8 In Less Than One Instruction Per Byte (arXiv 2010.03090)](https://arxiv.org/html/2010.03090v5) | 2020 | UTF-8 validation fused into scanning (Keiser–Thorton) |
| [Transcoding Billions of Unicode Characters per Second (arXiv 2109.10433)](https://arxiv.org/abs/2109.10433) + simdutf | 2021 | UTF-16/32 input transcoding front-end |
| [Fixing Ill-Formed UTF-16 with SIMD (arXiv 2601.06349)](https://arxiv.org/pdf/2601.06349) | 2026 | Modern validation-kernel shapes (Clausecker) |
| [Float→shortest-string experimental review (arXiv 2603.06581)](https://arxiv.org/html/2603.06581v1) | 2026 | Schubfach/Dragonbox-class printer for the emitter |
| [rapidyaml benchmarks](https://rapidyaml.readthedocs.io/v0.7.1/sphinx_is_it_rapid.html) | 2024 | The 2–3×-over-libyaml bar to clear; its state-machine design is validated, its lack of SIMD/arena is our headroom |
| libfyaml source (`~/src/external/libfyaml`) | 2024–25 | Conformance reference (100% yaml-test-suite): `fy-atom` span model, `fy-accel` O(1) access, test allocator (failure injection), `jsontestsuite`/`emitter-examples` corpora, thread tests |
| [libyaml-safer port notes](https://simonask.github.io/libyaml-safer/) + [HN](https://news.ycombinator.com/item?id=39304409) | 2024 | Independent audit of libyaml's redundant bounds checks |
| [psych-pure announcement](https://kddnewton.com/2025/12/25/psych-pure.html) | 2025 | Ruby-core appetite for replacing the libyaml dependency; compat target |
| libleptris internal ledger | 2025–26 | What pays: contiguous arena w/ content-derived sizing (TODO 183), single-representation attr views (184), fused scans (184), SIMD count/copy fusion (188), event recorder (measured 20 host calls for 41,806 events), single-representation everything. What died: **two-pass SIMD parser** (floor probe = memcpy+scan+stub walk already cost 88.5% of full parse, TODO 193), split-stream attribute layout (TODO 185). We inherit both lists. |

## 3. Architecture

### 3.1 Shape

```
CLI (cli/)  →  Public API (src/include/yeptris/, opaque handles, stable ABI)  →  Core (src/yeptris/)

core:
  common/   simd_text (AOT AVX2/NEON/SSE2+scalar TUs, runtime dispatch), string_view, chartype, cpu, port
  scan/     line-start table, indentation, indicator classification, quote/escape spans, flow structural scan
  parse/    single-pass non-recursive state machine (block engine + flow kernel)
  events/   pull (StAX) / push (libyaml-compatible) / recorder (bulk FFI)
  dom/      compact int32-pointer nodes, key interning, O(1) indexed access
  emit/     exact-sizing writer, style chooser, escape tables, float printer, canonical mode
  memory/   contiguous arena + pool + compact allocator
  encoding/ UTF-8 fusion validation; UTF-16/32LE/BE transcode front-end; BOM sniff
```

### 3.2 The scan kernel — indentation is the structure

XML's structure is tags; YAML's is **line starts + indentation columns +
indicator bytes**. The SIMD kernels (extending leptris `simd_text`):

- `text_find/count` on `\n` → line-start table (offset, indent, flags).
- Per line: first non-space (indent column); a tab in indentation is an error
  with exact position (YAML forbids it — cheap SIMD detection, hard for libyaml).
- Indicator classification, one compare set: `- ? : [ ] { } , # & * ! | > ' " % @ \``.
- Quoted-scalar spans: quote pairing, escape (`\`) detection; escapes processed
  **only when the escape byte exists** (SIMD test first — most scalars are clean).
- Flow-context scan: `, : [ ] { }` + quotes → the JSON-class kernel.
- Fused `copy+count3` (leptris TODO 188 pattern): arena sizing counters and the
  owns-copy of the input in one memory pass.

Critical leptris lesson applied: **do not build a two-pass parser** (structural
pass then a walk). Their two-pass floor probe cost 88.5% of the whole single-pass
parse. Instead: single pass that enters SIMD kernels per line / per scalar.

### 3.3 Block parser — single-pass indent-stack state machine

- Explicit stack of `(indent, node)` frames; zero recursion; depth guard
  (error, not stack overflow) — same contract as leptris.
- Per line: O(1) state transition (sequence entry / mapping key / value /
  scalar continuation / comment / blank). Handles `- ` compact notation,
  complex keys `?`/`:`, anchors `&x`, aliases `*x`, tags `!!str`, directives
  `%YAML`, `%TAG`, document markers `---`/`...`.
- Plain scalars: multi-line folding rules applied **only when continuation
  lines exist** — single-line scalars stay zero-copy views. Folds/escapes copy
  into the arena with memcpy runs.
- Block scalars `|`/`>`: indentation-indicator + chomping-indicator parsing;
  content assembled as arena memcpy runs.
- Multi-document streams: native; the document boundary is the iterparse unit.

### 3.4 Flow parser — the JSON-superset kernel

JSON is a strict subset of YAML 1.2 flow. Entering `{`/`[` switches to a
dedicated flow kernel (simdjson-class structural scan + fast_float numbers).
It must additionally accept YAML flow extensions: unquoted plain scalars,
`key: value` without quotes, anchors/aliases/tags in flow. One kernel serves
both, so **yeptris is incidentally a very fast JSON parser** — validated
against `ruby-json`, `json-c`, and `yajl` corpora.

### 3.5 DOM

- Compact nodes: int32 byte-offset compressed pointers (~48–64 B/node target;
  leptris hits ~96 B for richer XML elements), overflow-table fallback (macOS
  ASLR lesson, TODO 121).
- Node kinds: document, mapping, sequence, scalar (value view + style + tag),
  alias. Mapping keys interned in a doc-level open-addressing table — YAML
  repeats keys across sibling maps; interning also feeds O(1) `key?`/lookup.
- Anchors/aliases: small id-hash intern table; alias = node pointer; alias
  cycles are a parse error (Psych semantics). Merge keys (`<<`) resolve
  lazily (view) or eagerly in compat mode — matching Psych, where an alias
  materializes as the same Ruby object identity.

### 3.6 Events & streaming

One engine, three consumption models (the leptris quartet, adapted):

1. **Pull (StAX-style)** — host-driven `yeptris_pull_next`, zero C→host
   callbacks; the binding-friendly form.
2. **Push events, libyaml-event-compatible** — so libyaml's `run-*` test
   harnesses port with a shim, andPsych-style handlers work unchanged.
3. **Recorder** — fixed-size event records + string arena, drained in bulk
   per chunk. Measured in leptris: 20 host calls for a 41,806-event document.
   This is the single biggest lever for Ruby end-to-end numbers.
4. **Iterparse** — bounded-memory iteration over top-level nodes/documents.

Incremental feed carries full state across chunk boundaries: indent stack,
flow depth, partial-scalar folding state, line/column counter.

### 3.7 Emitter

- **Exact sizing pre-pass**: per-node byte-cost table (style + escapes +
   indent depth) → one buffer allocation → linear writes. No regrowth.
- Scalar style chooser driven by flags captured at parse time (free when
  re-serializing a parsed document; cheap SIMD classify when building fresh).
- 256-entry escape cost/width tables; SIMD "needs escaping?" test.
- Floats: Schubfach/Dragonbox-class shortest round-trip printing (2026 review
  confirms these as SOTA) — also fixes libyaml's float output verbosity.
- Indentation: precomputed pad buffers, memcpy emission.
- Width-aware line folding (libyaml best-width parity) + **canonical mode**:
  `serialize(parse(serialize(x)))` byte-stable (the leptris guarantee).

### 3.8 Memory

Contiguous per-document arena; block sizing derived from SIMD occurrence
counts (`\n`, `:`, quote densities) with conservative growth — the TODO 183
pattern that closed leptris's parse gap. Zero steady-state allocations.
`yeptris_document_free` = one free. Per-thread caches + explicit
`yeptris_thread_cleanup` (C99 has no thread-exit hook). Ownership rules and
`Memory:` comments per public function, exactly like leptris.

### 3.9 Encoding

UTF-8 native with validation fused into the scan kernels; UTF-16/32LE/BE
input transcoded once up front (BOM sniff) using Keiser–Thornton/Clausecker
kernel shapes; output always UTF-8 (leptris serialization guarantee).

### 3.10 Errors & compatibility

Status codes + thread-local and per-document message channels; every error
carries line:column (Psych's `SyntaxError` surface maps directly). Schema
matrix: `YEPTRIS_SCHEMA_12_CORE` default; `YEPTRIS_SCHEMA_11_COMPAT` for
libyaml/Psych implicit typing (`~`, `y/n`, `0o`/`0` octal, sexagesimal,
timestamps). Per-parse options struct in addition to thread globals.

## 4. Ruby binding (yeptris-ruby pattern)

FFI gem (no C extension), vendored precompiled platform gems, thin handles,
bulk APIs, identity cache, readonly mode with memoization — all proven in
leptris-ruby. Surface: `Psych`-compatible (`load`, `safe_load`, `dump`,
`parse`, `parse_stream`, Visitors/ScalarScanner semantics in compat mode) so
`yaml`-backed code swaps with a one-line require. Lockstep versioning with
the C library.

## 5. What's possible — performance targets

Baseline anchors: ryml ≈ 2–3× libyaml (≈450 MB/s on flow/JSON, less on
block); leptris achieved 6–14× libxml2 parse, 377–624 MB/s SAX throughput,
25× attribute writes, 24–142× Ruby streaming vs Ox/Nokogiri.

| Metric | Baseline | yeptris target | Rationale |
| --- | --- | --- | --- |
| Block-style parse (C) | libyaml ≈100–250 MB/s | **0.8–2 GB/s (≥5×)** | line-oriented SIMD + zero-copy + zero-alloc; ryml already 2–3× without SIMD/arena |
| Flow/JSON parse (C) | ryml ≈450 MB/s, libyaml less | **1–3 GB/s** | simdjson-class kernel on the JSON subset |
| Emitter (C) | libyaml | **≥5×** | exact sizing + memcpy-class writes + Schubfach floats |
| Allocations per parse | O(tokens) (libyaml) | **≈0** | contiguous arena |
| Peak RSS | libyaml/psych | **2–8× lower** | compact nodes, no token/event queues, views not copies |
| Ruby `YAML.load_file` | Psych | **≥5× (large docs 10–30×)** | recorder bulk events + ScalarScanner fast paths; leptris streaming precedent 24–142× |
| Ruby allocations | Psych (heavy) | **orders of magnitude fewer** | leptris measured ~1800× less vs Ox on medium docs |
| Multi-doc / huge streams | Psych unbounded | bounded by largest document | iterparse |

Projection honesty: block-style YAML contains long plain-scalar lines
(memcpy-class scanning, favorable); pathological short lines and deep flow
nesting are the worst cases — guarded by the depth limit, measured before
claims. Every target above gets a benchmark in `benchmarks/` producing JSON +
Markdown per CI run (leptris artifact pattern).

## 6. Phases

The executable breakdown of these phases lives in `TODO.impl/01–20` with per-item
design decisions and acceptance gates; `TODO.md` is the status index. Mapping:
Phase 0 → items 01–05, Phase 1 → 06–12 + 16–17, Phase 2 → items 04/06 kernels
landing + 18, Phase 3 → 15, Phase 4 → 13–14, Phase 5 → 19 + remaining
conformance, Phase 6 → 20.

Each phase ends green: all tests ported so far pass, benchmarks recorded, no
ASAN/leak regressions.

- **Phase 0 — Bootstrap.** Repo skeleton (CMake ≥ 3.20, presets, clang-format),
  CI (build+ctest matrix on Linux/macOS, ASAN, nightly libFuzzer), benchmark
  harness linking libyaml (+ optional libfyaml/ryml), corpora setup
  (yaml-test-suite, libyaml examples/regression, libfyaml jsontestsuite +
  emitter-examples, synthetic generators: block-heavy, flow-heavy,
  scalar-heavy, anchor-heavy, deep-nesting).
- **Phase 1 — Conformance core.** Scanner + parser as state machines (scalar
  C first), DOM + arena + StringView, event API, emitter with correct
  semantics. Port libyaml `run-*` harnesses + regression corpus; run the
  yaml-test-suite; reach ≥90% event parity. No perf claims yet.
- **Phase 2 — Performance kernel.** SIMD scan kernels + runtime dispatch,
  arena sizing pre-scan, zero-copy scalars, fused copy+count, fast_float.
  Target: ≥3× libyaml parse on the corpus median. ASAN-clean.
- **Phase 3 — Streaming + Ruby.** Pull + recorder + iterparse; yeptris-ruby
  gem; port Psych's 46-file suite against the compat API. Target ≥5× Psych
  end-to-end; allocation counts published.
- **Phase 4 — Emitter performance + canonical mode.** Sizing/writer/float/
  escape/indent kernels. Target ≥5× libyaml emitter; byte-stable roundtrip
  property test.
- **Phase 5 — Full conformance + hardening.** 100% yaml-test-suite or
  documented divergences; cross-fuzz against libyaml/libfyaml outputs; TSAN;
  release workflow; vcpkg/homebrew packaging.
- **Phase 6 — Ecosystem.** CLI polish, docs, published benchmarks vs
  ryml/libfyaml, additional bindings on demand (Rust/Python ports of the
  leptris binding contracts).

## 7. Test-porting map

| Source | Destination | Method |
| --- | --- | --- |
| libyaml `tests/run-{scanner,parser,emitter,loader,dumper}` | `test/port/libyaml/` | shim drivers over the libyaml-compatible event API; compare event streams + errors byte-for-byte |
| libyaml `regression-inputs/`, `examples/` | `test/fixtures/` | roundtrip + no-crash corpus |
| libfyaml `test/` (`jsontestsuite.test`, `emitter-examples/`, generic-scalar, parse/emit bug tests) | `test/port/libfyaml/` | port where semantics agree; adopt its test-allocator failure-injection and thread-test patterns into our harness |
| yaml-test-suite | `test/conformance/` | event-stream JSON comparison; the conformance bar |
| psych `test/psych/*.rb` (46 files) | Ruby binding `spec/` | direct port against the Psych-compat API |
| `ruby-json` / `json-c` / `yajl` tests | `test/flow/` | flow-kernel conformance + emitter edge cases (numbers, unicode escapes) |

## 8. Risks

- **Spec complexity** (tabs vs spaces, implicit typing ambiguity, flow/block
  interplay, `<<` merge edge cases): mitigated by event-level compat with
  libyaml in 1.1 mode and conformance-first phasing.
- **Two-pass temptation**: pre-declared dead per leptris TODO 193. Single pass only.
- **Alias/merge mutation semantics** in DOM vs Psych Ruby identity: decide per
  Phase 1 spec work; compat mode follows Psych exactly.
- **Chunk-boundary folding state** for streaming: carry full scalar-folding
  context; tested with adversarial chunk splits.
- **psych-pure / libfyaml drift** in Psych main: track `psych_*_fy.c`; compat
  matrix pinned to released Psych versions.

## 9. Open questions (decide in Phase 0/1)

1. Library/gem naming: `libyeptris` + `yeptris` CLI + gem `yeptris` with
   `Psych`-compat namespace — or separate `yeptris-ruby` repo à la leptris?
   (Recommendation: same repo first, split when release cadence demands it —
   matches leptris history.)
2. Default schema: 1.2 core default with 1.1 compat opt-in (recommended), or
   the reverse to guarantee Psych drop-in?
3. Whether the libyaml-event-compatible push API stays public long-term or is
   a porting-only shim.
4. Canonical-YAML mode scope in v1 (C14N-equivalent guarantee level).
