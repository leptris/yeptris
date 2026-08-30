# TODO.impl/15 — Ruby binding: FFI gem + Psych compatibility layer

Status: pending · Depends: 11, 12, 13 · Layer: `bindings/ruby` · PLAN.md phase: 3

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
