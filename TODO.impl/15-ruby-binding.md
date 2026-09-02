# TODO.impl/15 — Ruby binding: FFI gem + Psych compatibility layer

Status: PHASE A LANDED 2026-09-02 — ffi.rb (every public symbol attached once; check_status/Owned.string seams; pinned enums), Document (sole C-memory owner, Freed-flag double-free guard, GC finalizer, construction conveniences), Node (kind/value/style/tag/anchor accessors, typed reads, seq/map walks, Ruby materialization with alias identity via the node-id memo), Yeptris::YAML load/load_stream/load_file/parse/dump (dump rides the 11p3 mutation API; symbol and date round-trips; cycle refusal), yeptris_node_id added to the C API (query handles are transient — ids are the stable identity bindings key wrapper caches on). 25 specs incl. Psych-verified typing (y/n booleans incl. KEYS, 0o17 vs 017 octal, :sym scanning, timestamp keys). CI: ruby.yml (2 OS). Honest numbers (2k-node doc): dump 1.4× faster than Psych; load 2.7× SLOWER — the DOM-walk materialization pays 2-3 FFI calls per node; phase B's recorder-driven materialization is the designed fix (bulk records, FFI tax per chunk), ≥5× target unchanged. Emitter refinement landed en route: plain-safety now allows a leading/mid colon followed by a non-space (libyaml parity — Psych dumps ":name" plain; 152/152 gates held). 2026-09-02 (phase B): RECORDER-DRIVEN MATERIALIZATION LANDED — Yeptris::Materializer: one bulk drain (records + arena read once), then a pure-Ruby stack machine over the flat unpacked array (per-field FFI reads measured away: 5-8 calls/record -> one unpack). Typing moved to the C resolver SSOT: YeptrisEventRecord's pad byte carries tag_id (sizeof stays 36, ABI-pinned); Ruby converts by tag via Kernel#Integer/Float with three documented Psych-quirk overrides (single-char y/n stay Strings, floats need a dot + signed exponent, sexagesimal via Psych's (e-2).abs formula). yeptris_recorder_new_ex(schema) added (the recorder's engine takes the schema — compat_11 reproduces Psych typing; 47/47 differential cases vs Psych 5.4 verified end-to-end). Merge keys (<<) resolve inline (existing keys win, sequences merge in order). Numbers (2k-node doc): load 2.1x faster than Psych with 2.4x FEWER allocations (28k vs 66k); dump 1.6x. Split profile: C feed 8%, unpack 33%, walk 58% — the 5x target needs C-side bulk conversion or a leaner walk; continues in phase C/D against the 18B harness. 2026-09-02 (suite start): the LOADING-SEMANTICS half of the Psych suite is ported (spec/psych/: boolean complete incl. the Norway problem under both schemas, nil, merge keys incl. bare-hash/deferred inline-map merges and [*a,*b] ordering, alias identity, array/hash/string round-trips, date/time with the YAML 1.1 space-form timestamp normalization) — 42 examples. Fix en route: inline maps under a "<<" key merge at their END, not their start (contents arrive later). 2026-09-02 (phase C start): THE Yeptris::Psych DROP-IN NAMESPACE LANDED — require "yeptris/psych" rebinds the top-level Psych (stdlib Psych kept as Yeptris::Psych::ORIGINAL): load (Psych 5 SAFE-by-default: recursive alias opt-in via AliasNotEnabled, non-core tags raise DisallowedClass — yeptris materializes plain data only, so the class whitelist is structural), unsafe_load, safe_load, dump, parse/parse_stream returning a Psych::Nodes tree that OWNS the yeptris document (children are node handles; GC finalizer releases the C memory; to_ruby reuses the Materializer). 9 compat specs; 77/77 total. REMAINING: object-serialization half of the suite (ruby/object tags, coder, emitter, handlers — needs YAMLTree dumping + tagged deserialization), readonly mode, platform gems (D), C (full 46-file suite + .tml corpora + readonly mode + YAMLTree), D (platform gems + lockstep + symbol audit) · Depends: 11, 12, 13 · Layer: `bindings/ruby` · PLAN.md phase: 3

## Goal

`yeptris` the gem: an FFI-based (no C extension) Ruby YAML library with a
Psych-compatible surface, thin handles, bulk reads, and vendored
precompiled platform gems — the leptris-ruby playbook executed for YAML.

## Deliverables

- `bindings/ruby/lib/yeptris.rb` + `lib/yeptris/{yaml,ffi,document,node,
  scalar,pull,recorder,serialization}.rb` mirroring the leptris-ruby
  module layout (autoload-only, no `require_relative` in lib/).
- `ffi.rb` — every public C declaration attached exactly once; status
  checks via `FFI.check_status`; owned `char*` via `read_owned_string`;
  never hand-rolled per call site (leptris seams).
- Psych-compat layer (`lib/yeptris/psych_compat.rb`): `Yeptris::Psych`
  namespace implementing `load / safe_load / dump / parse / parse_stream /
 Psych::Nodes tree / Visitors::ToRuby + YAMLTree semantics`. Ruby-side
  object materialization rides the **recorder** (bulk event records →
  Ruby objects in a tight loop; no per-event FFI callbacks).
  `ScalarScanner` semantics: C `compat11` resolver (10) for the fast
  path, with a Ruby fallback for the Psych-only corners (Symbol, Date,
  complex forms) — one semantic source, the 10 golden vectors.
- `Node.wrap` identity cache; `Document` as the sole C-memory owner
  (explicit `#free` or GC finalizer; use-after-free raises, never
  segfaults); readonly mode with memoized reads (leptris pattern).
- Vendored platform gems: Rakefile pins `libyeptris` release tarball,
  builds into `lib/`, ships x86_64/aarch64-linux (glibc+musl), x86_64/
  arm64-darwin, mingw/ucrt + pure-Ruby fallback; `YEPTRIS_LIB_PATH`
  override for dev; lockstep major.minor with the C library; symbol
  audit task (`rake audit:symbols`).
- Spec suite: port psych's `test/psych/*.rb` (46 files) to rspec +
  psych-pure's `.tml` corpora as shared fixtures + ruby-json-derived
  flow specs. `benchmark/` harness: load/dump/stream vs psych and
  psych-pure on the 18 corpora (allocation counts via
  `GC.stat`/`ObjectSpace`).

## Design decisions

- FFI over C extension (leptris-ruby decision, restated): zero compile at
  install, precompiled gems, and per-call overhead is amortized by bulk
  APIs — the recorder makes the FFI tax O(chunks), where a C extension's
  advantage evaporates.
- Public-name strategy (open question, PLAN.md §9): gem `yeptris` with
  `require "yeptris/psych"` aliasing `Psych` constants for drop-in
  swaps — versus a neutral `Yeptris::YAML` surface. Ship the neutral
  surface + compat alias; decide promotion after the suite passes.
- Streams: `parse_stream` over the pull API with an IO feed loop;
  `Psych::Stream` parity via the streaming emitter (13).

## Phases

A. ffi.rb + Document/Node + parse/dump happy path against a local build.
B. Recorder-driven ToRuby + ScalarScanner fast path; scalar/encoding/
  exception suites ported.
C. Full 46-file suite + `.tml` corpora green; readonly mode; dumps of
  Ruby object graphs (yaml_tree port) for Psych parity.
D. Platform gem rake + lockstep release wiring + symbol audit.

## Acceptance

- Ported psych suite green (100% of ported examples; exclusions listed
  with reasons in VALIDATION.md).
- ≥ 5× Psych on `YAML.load_file` for the large benchmark documents;
  ≥ 10× on streaming (18's matrix, machine-relative, CI-recorded).
- Ruby allocation count on a medium document ≤ 1/10 of Psych's
  (ObjectSpace-measured; leptris precedent: ~1800× less than Ox).

## References

- `~/src/leptris/leptris-ruby/` (the blueprint: layout, seams, rake,
  platform gems, CLAUDE.md conventions).
- `~/src/external/psych/{lib,test}/`, `~/src/external/psych-pure/lib/psych/pure.rb`
  (semantics + suites), `~/src/external/ruby-json` (Ruby-level fast-path
  idioms).
