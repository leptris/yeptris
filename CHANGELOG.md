# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

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
