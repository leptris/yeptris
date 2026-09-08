# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.14] - 2026-09-08
### Fixed
- core_12 marshal typing (TODO.restructure/32 completion): the parse's
  schema is now a document property (`yeptris_document.schema`, set by
  both parse paths — strict JSON is core by construction), and the
  Marshal emitter threads it: Psych's dot-required float quirk applies
  ONLY under `YEPTRIS_SCHEMA_11_COMPAT`. Under core, `1e3` marshals as
  `{"k"=>1000.0}` (spec 10.3.2: the core float regexp has an OPTIONAL
  dot); under compat it stays `{"k"=>"1e3"}`, byte-identical to Psych.
  Regression test: `Marshal.SchemaConditionedFloatTyping` (both the
  direct `yeptris_marshal` and `yeptris_marshal_node` paths).
- The Ruby binding's core12 spec suite ran green only against a
  working-tree C build; the shipped 0.1.13.x gems vendor C v0.1.13,
  which lacked this fix — the native YAML.load path returned the
  compat typing under core_12. Fixed by this change; the next lockstep
  gem release carries it.

## [0.1.13] - 2026-09-07
### Added
- `yep_json_number_scan` (scan/json.c): the JSON number grammar walk
  FUSED with conversion — one scan validates and converts (integer
  fast-path into int64, INT64_MIN exact; `*is_float` reports the text
  shape: 0=int, 1=float, 2=integer-beyond-int64 with an approximate
  `*dv`). `yep_json_number` is now a thin wrapper. The Ruby native
  materializer's number path is one call — no more validate-then-
  reconvert double walk.
- `scripts/build-native-asset.sh` + release.yml: every GitHub Release
  carries a prebuilt native-materializer tarball for the publish
  runner's platform (build recipe versioned in scripts/, never inline
  workflow YAML).

## [0.1.12] - 2026-09-07
### Added
- The visit API (`yeptris/visit.h`): `yeptris_visit`,
  `yeptris_visit_json` (fused RFC 8259 scan — no DOM, no records),
  `yeptris_visit_node`; the JSON scan kernels and number converters
  are exported for host materializers. The Ruby native materializer
  riding this beats JSON.parse (mean 0.72x on the 152 KB corpus).

## [0.1.11] - 2026-09-07
### Added
- `yeptris_marshal`/`yeptris_marshal_node`/`yeptris_marshal_free`
  (TODO.restructure/21): the C side converts value records into Ruby
  Marshal 4.8 bytes; the binding materializes the whole object graph
  with one `Marshal.load` call. ~10× faster than the columnar walk on
  JSON-shaped input and ~5× on YAML, ~50× on the per-node DOM walk
  (`Node#to_ruby` becomes bulk). Alias identity preserved through `@`
  links; merge keys and timestamps return `ERROR_UNSUPPORTED` for the
  record-walk fallback.

### Changed
- The value-drain entry (`yep_values_from_input`) sniffs strict-JSON
  (`{`/`[` as the first non-space byte) and routes through the JSON
  scanner + DOM linearizer on the same path the YAML engine takes;
  any grammar surprise defers to the engine. Records stay byte-
  identical across routes (the Ruby binding's 2.7k-corpus
  differential pins the equivalence).

### Fixed
- DOM `lin_node` linearizer: pending anchors decorate the value that
  *follows* them, even inside a container; nested anchor bindings no
  longer clobber the outer pending index (ASAN caught a stale-index
  write on `&a [&b x]`).
- Marshal `anchor_find`: YAML lets a later `&anchor` shadow an earlier
  one of the same name; lookup scans newest-first
  (libyaml snapshot 3GZX).

## [0.1.10] - 2026-09-06
### Fixed
- MECE: the `\n`/`\r` break stop set has ONE home (scan.c) — the
  quoted-scalar path rebuilt a bit-for-bit duplicate per quote.

## [0.1.9] - 2026-09-06
### Performance
- The JSON string stop set becomes a constant (34 bitmap bits were
  cleared and set per string scan); strict-JSON strings at 479 MB/s.
- The plain-scalar stop sets become constants (clear + 4-9 adds per
  scan, twice per line). scalar-heavy parse reaches 3.36x libyaml —
  the fastest engine yet. Both bitmaps pinned against runtime builds
  (a hand-written drift ends every plain scalar early).

## [0.1.8] - 2026-09-06
### Performance
- Plain-value spans scan once: e_plain_multiline takes the caller's
  span (every plain value was scanned twice — colon decision, then
  the fold). All shapes at their bests (scalar 3.23x).

## [0.1.7] - 2026-09-05
### Fixed
- `stopset_find`'s differential test hardening (rare-byte sets let
  broken vector paths pass); the nibble-method SIMD attempt was
  built, measured a net loss, reverted — analysis ledgered.

## [0.1.6] - 2026-09-05
### Fixed
- UBSan-caught UB: NULL+0 pointer arithmetic in the zero-entry
  columnar drain and the scan_stats tails (both guarded).

## [0.1.5] - 2026-09-05
### Performance
- Prefix-slot interner: 16-byte slots carry the key's first-8-byte
  prefix + length + value INLINE — one cache line per probe for
  keys <=8 bytes (nametab-GET misses were ~14% of anchor-heavy
  parse). Best absolute times on every shape at release.

## [0.1.4] - 2026-09-04
### Performance
- scan_line rides the SIMD kernels for line end + indent behind a
  64-byte span gate (below it, dispatch overhead beats the loop).
- Fused pre-scan: one SIMD pass computes the ten occurrence counts
  plus the printable/ASCII flags (four full-buffer passes before).

## [0.1.3] - 2026-09-04
### Added
- `yeptris_value_drain_columns`: the value stream as parallel typed
  buffers carved from one allocation — column i is byte-faithful to
  record i (equivalence-pinned).

## [0.1.2] - 2026-09-04
### Fixed
- The pend anchor id rides the pend view: a props-only line before
  its node ("--- &id001" / "- *id001") carried the anchor view with
  a zero id — aliases to it failed with an empty error. Found by the
  Ruby port's self-referencing-structures spec; four-case C
  regression test added.

## [0.1.1] - 2026-09-04
### Performance
- Engine pass 1: anchor ordinals end to end, interner pre-sizing
  from the '&' count, word-at-a-time hash + inline view equality,
  per-line scan memo, DOM node-hint floor, constant-size resolver
  word checks. anchor-heavy 1.27x -> 1.96x libyaml.
### Fixed
- The line-scan memo could rewind a mid-line position and spin
  forever (found by the roundtrip harness on a directive +
  inline-comment document); loop heads that legitimately see mid-line
  positions keep the scan-from-pos semantics.

## [0.1.0] - 2026-09-03

### Added

- Bootstrap scaffold (TODO.impl/01): CMake build (C11, LTO for Release,
  ASAN/TSAN options, scoped warnings), public header skeleton (`yeptris.h`
  umbrella, opaque pointer-sized handles, pinned status enum, generated
  version header), CLI with command registry (`yeptris version`), ABI
  pinning test, CI workflows (test matrix + ASAN), `validate.sh` and
  `bump-version.sh`.
