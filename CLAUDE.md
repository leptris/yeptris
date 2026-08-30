# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**yeptris** — the YAML counterpart of **libleptris**, an ultra-performance XML 1.0 parser/writer/streamer (see `~/src/leptris/leptris`). The deliverable set mirrors leptris: `libyeptris` (pure C11, zero required runtime deps, stable C ABI, opaque handles), a `yeptris` CLI, a benchmark suite against the incumbents, and a Ruby binding following the leptris-ruby pattern (FFI gem, no C extension, vendored precompiled platform gems).

Mission, in priority order:

1. **Conformance** — YAML 1.2 with a libyaml/Psych-compatible YAML 1.1 lenient mode. The yaml-test-suite is the conformance bar; every divergence is documented.
2. **Port the reference test suites** — libyaml's test harnesses and Psych's Ruby suite must run against yeptris drivers/bindings.
3. **Beat libyaml and Psych on every performance measure** — parse, emit, stream, allocations, peak memory, Ruby end-to-end. rapidyaml (currently 2–3× libyaml) is the field benchmark to exceed.

The repository starts empty. libleptris is the architectural source of truth: every trick it validated is the default answer here unless YAML's grammar demands otherwise. Port leptris's *machinery*, not its XML grammar.

## Reference repositories

| Path | What it is | What we take from it |
| --- | --- | --- |
| `~/src/leptris/leptris` | libleptris C core | Architecture to port: `common/simd_text*.{c,h}` (AOT SIMD TUs + runtime CPU dispatch), `memory/arena.c` (contiguous arena, content-derived sizing), `dom/compact.c` (int32 compressed pointers), single-pass direct parse, chunked event recorder, TODO-board workflow, perf ledger discipline |
| `~/src/leptris/leptris-ruby` | Ruby FFI binding | Thin-handle binding pattern (one Ruby method = one FFI call), bulk/batch APIs, identity cache, vendored platform gems, lockstep versioning with the C lib |
| `~/src/external/libyaml` | The C library to beat | Scanner/parser/emitter *semantics* to match in compat mode (YAML 1.1 quirks); its `tests/run-*` harnesses, `regression-inputs/`, and `examples/` corpora get ported |
| `~/src/external/psych` | Ruby's YAML lib (wraps libyaml; carries `*_fy.c` libfyaml experiments) | The API surface to be compatible with (`Psych.load/dump`, `Visitors`, `ScalarScanner`, `Handlers`); its `test/psych/*.rb` suite gets ported |
| `~/src/external/libfyaml` | The highest-conformance C YAML lib (100% yaml-test-suite; Psych's `_fy` backend experiments target it) | Conformance semantics and diagnostics; `fy-atom` span analysis (its zero-copy atom model), `fy-accel` O(1) access structures, durable arena + test allocator discipline, anchor interning (xxhash/blake3), its `test/` corpora (`jsontestsuite.test`, `emitter-examples/`, parse/emit bug tests) |
| `~/src/external/psych-pure` | Pure-Ruby YAML 1.2 engine layered on Psych (2025) | Readable 5k-line reference for 1.2 scanner/parser/emitter semantics (`lib/psych/pure.rb`); its `.tml` spec corpora (`parse-{block,flow,scalar,props,stream}`, `error-suite`) are conformance fixtures |
| `~/src/external/simdjson` | The GB/s SIMD JSON parser (shallow clone) | Structural-indexing + On-Demand design, ISA dispatch, differential fuzzing patterns, number/string kernels feeding the flow kernel |
| `~/src/external/simdutf` | SIMD Unicode validation/transcoding kernels (shallow clone) | Algorithmic reference for the encoding front-end: UTF-8 validation, UTF-16/32 transcoding, ASCII fast paths |
| `~/src/external/ryu` | Shortest round-trip float printing; C sources in `ryu/` | Port `d2s`/`f2s`/`d2fixed` into the emitter's float printer (Apache-2.0/Boost dual license, attribution preserved) |
| `~/src/external/json-c` | C JSON DOM + tokener | DOM/serializer API shape reference; negative perf reference (linkhash, per-token allocation) |
| `~/src/external/ruby-json` | Ruby's JSON ext | Fast paths + test corpus for the flow-style kernel — JSON is a subset of YAML 1.2 flow |
| `~/src/external/nlohmann-json` | Header-only C++ JSON | API ergonomics reference: SAX interface design, value semantics |
| `~/src/external/yajl` | C incremental SAX JSON | Push/incremental API design; generator API; emitter edge-case tests |

## Build & test (libleptris conventions — implement CMake to satisfy these, don't invent different ones)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release   # LTO is on by default for Release
cmake --build build
ctest --test-dir build --output-on-failure
```

Options follow leptris naming: `BUILD_TESTING` (ON), `YEPTRIS_BUILD_CLI` (ON), `YEPTRIS_BUILD_BENCHMARKS` (OFF), `YEPTRIS_BUILD_SHARED`/`_STATIC`, `YEPTRIS_ENABLE_ASAN`, `YEPTRIS_ENABLE_FUZZING` (libFuzzer, needs LLVM clang).

Single test (Google Test binaries under `build/test/`):

```bash
./build/test/scanner/test_scanner --gtest_filter=BlockIndent.*
```

`scripts/validate.sh` is the pre-completion sanity gate: clean build → tests → CLI → conformance → benchmarks → leak check → node-size check. Leak checks: `leaks --atExit --` (macOS), `valgrind --leak-check=full --error-exitcode=1` (Linux).

Ruby binding (when it lands): `bundle exec rspec`, single example via `spec/…:LINE`, `LEPTRIS_LIB_PATH`-style env override to test against a local build.

## Architecture

Three strict layers, top depends only on the layer below:

```
CLI (cli/)             → argument parsing, output formatting; never touches core structures directly
Public API (src/include/yeptris/) → yeptris_parse_*, yeptris_emit_*, document/node/event handles; opaque, ABI-stable
Core (src/yeptris/)    → scan, parse, events, dom, emit, memory, encoding, common
```

Planned core subsystems:

- `common/` — `simd_text` (AOT SIMD kernels: find/count/fused copy+count/classification, AVX2 + NEON + SSE2/scalar TUs, runtime dispatch via `cpu.c`), `string_view` (zero-copy slices), `chartype`, `port`.
- `scan/` — YAML-specific SIMD kernels: line-start table, per-line indentation (first non-space; tab detection), indicator classification (`-?:[]{}#&*!|>'"%@\``), quoted-span/escape scanning, flow-context structural scan (the JSON-class kernel).
- `parse/` — **single-pass, non-recursive state machine** with an explicit indentation stack (rapidyaml-style; also the leptris SAX-core pattern). Block parsing is line-oriented; entering `{`/`[` switches to the flow kernel (JSON ⊂ YAML 1.2). Depth guard rejects pathological nesting with an error, never a crash.
- `dom/` — compact nodes (int32 byte-offset compressed pointers, macOS ASLR overflow-table fallback, leptris lesson), node kinds: document, mapping, sequence, scalar (style + tag + zero-copy value view), alias. Mapping keys interned in a doc-level open-addressing table. O(1) indexed child access.
- `events/` — three consumption models over one engine: **pull** (StAX-style, host-driven, zero C→host callbacks — the FFI-friendly form), **push** events (libyaml-event-compatible so libyaml test drivers port directly), and the **recorder** (fixed-size event records + string arena, bulk drain per chunk — turns O(events) FFI callbacks into O(chunks)).
- `emit/` — exact output sizing pre-pass (per-node byte-cost table → one allocation → linear writes), scalar style chooser driven by flags collected at parse time, 256-entry escape cost/width tables, SIMD indentation padding, Schubfach/Dragonbox-class float printing, width-aware folding, and a canonical mode with the leptris guarantee: `serialize(parse(serialize(x)))` byte-stable.
- `memory/` — contiguous per-document arena with content-derived block sizing (SIMD occurrence counts of `\n`, `:`, quotes), O(1) pool, compact allocator. Zero steady-state allocations; `yeptris_document_free` frees everything in one call.
- `encoding/` — UTF-8 native (validation fused into scanning); UTF-16/32LE/BE input transcoding front-end with BOM sniffing (Keiser–Thornton-class validation). Output is always UTF-8.

### Architecture laws (enforced in review)

- **MECE** — bytes→facts is `scan/`'s alone; facts→events is the single parser engine in `parse/`; events→nodes is `dom/` sinks; nodes/events→bytes is `emit/`. Charset belongs only to `encoding/`; allocation only to `memory/`; every truth table (chartype, indicators, implicit types, escape costs) is declared once, in its owning module.
- **SSOT** — one public types header; one string representation (`YepView` offset+length); one parse engine behind DOM/pull/push/recorder; scalar style decisions made once at parse and reused by the emitter; escape tables declared in `scalars` and imported by the emitter; version synced only by `scripts/bump-version.sh`.
- **OCP** — extension is registration, never core edits: node kinds (vtable), SIMD ISA targets (TU + dispatch slot), event sinks, schema resolvers, emitter style rules, CLI commands, input encodings, bindings. A `switch` outside the single dispatch point of its table is a bug.
- **OOP-in-C** — opaque handles only; per-class constructors/destructors; vtable dispatch; no struct fields in public headers; no module reaches into another module's internals.

Schema/compat: `YEPTRIS_SCHEMA_12_CORE` (default) vs `YEPTRIS_SCHEMA_11_COMPAT` (libyaml/Psych implicit typing: `~`, `y/n`, octal `0o`/`0`, sexagesimal). Per-parse options struct in addition to thread-global toggles. Errors: status codes + thread-local *and* per-document channels; every error carries line:column.

## Testing

- `test/conformance/` — yaml-test-suite drivers (event-stream comparison).
- `test/port/libyaml/` — replacements for libyaml's `run-scanner/parser/emitter/loader/dumper` against the yeptris event API, plus its `regression-inputs/` and `examples/` corpora.
- `test/port/libfyaml/` — ports of libfyaml's functional tests where semantics agree: `jsontestsuite.test`, `emitter-examples/`, generic-scalar and parse/emit bug tests; its test allocator (allocation failure injection) and thread tests inform our harness.
- `test/…/json/` — flow-kernel conformance using corpora from `ruby-json` / `json-c` / `yajl` tests.
- Ruby binding `spec/` — port of `psych/test/psych/*.rb` (46 files) against the compat API.
- CI mirrors leptris: build + ctest matrix, ASAN, TSAN (concurrency contract: one document per thread; read-only sharing safe), nightly libFuzzer, benchmark run per push with uploaded artifacts.

## Conventions

- **C11 (`CMAKE_C_STANDARD 11`, extensions off), `-Wall -Wextra -Wno-unused-parameter`, warning-clean.** C11 (not C99) because `_Static_assert` is not a keyword in MSVC's C99 mode — same rationale as libleptris. New warnings are bugs — fix at the source; CI builds with `-Werror` (`YEPTRIS_WARNINGS_AS_ERRORS=ON`). Benchmarks are C++11. Format with the repo `.clang-format` (clang-format ≥ 14; note the Homebrew LLVM one lives at `/opt/homebrew/opt/llvm/bin/clang-format` — the PATH `clang-format` may be ancient).
- Public API: opaque pointer-sized handles (`_Static_assert`), per-function `Memory:` ownership comments, single canonical types header. Don't break ABI without a major bump.
- All allocations reachable from a document live in its arena/pool; ownership is document-scoped, no refcounting.
- Performance claims require numbers from the benchmark harness; record dead ends in the perf ledger (leptris discipline — e.g. its two-pass SIMD parser was *measured dead* at 88.5% floor cost; the single-pass direct parser is the proven shape).
- Work is tracked on TODO boards (`TODO.md` + topical `TODO.*` files), one executed plan per TODO item.
- All changes go through PRs. Never commit/push to main, never create tags — releases go through the automated release workflow only. Stage explicit file paths, never `git add -A`. No AI attribution anywhere.

## Where to look for context

- `TODO.md` + `TODO.impl/NN-*.md` (this repo) — the executable implementation board: 20 sequenced items with design decisions, acceptance gates, and reference paths.
- `PLAN.md` (this repo) — architecture plan, performance targets, phase roadmap, test-porting map.
- `~/src/leptris/leptris/CLAUDE.md`, `README.adoc`, `VALIDATION.md`, `benchmarks/README.adoc` — the conventions this repo inherits.
- `~/src/external/libyaml/src/yaml_private.h` — libyaml internals and 1.1 semantics.
- `~/src/external/libfyaml/src/lib/fy-atom.h`, `fy-emit.c` — conformance semantics, span model, and emitter behavior reference.
- `~/src/external/psych/lib/psych/` — Ruby-side semantics (Visitors, ScalarScanner, handlers) the binding must reproduce.
