# 28 — Platform gems ship the precompiled native materializer

Status: complete

## Why

The JSON.parse win is opt-in today (manual `extconf && make`). Users
of the platform gems get the FFI ladder only — the headline feature
is invisible by default. `spec.extensions` is the wrong tool (it
forces compilation at install); the platform gems must carry the
compiled `native.so`/`native.bundle` next to the vendored dylib.

## Plan

1. The C repo's release workflow (gem-publish job) builds the
   extension per platform after building libyeptris: `ruby extconf.rb
   && make`, place the bundle under `lib/yeptris/` in the gem tree.
2. The gemspec stays compilation-free (no `spec.extensions`); the
   precompiled artifact is just a file in `files`.
3. ABI note: the bundle links the vendored dylib at a relative
   rpath — the platform gem vendors both, so the pair stays together.

## Acceptance

- `gem install yeptris` on a stock machine loads `Yeptris::Native`
  (no compiler involved) and `Yeptris::YAML.load(json)` beats
  `JSON.parse` on the mean gate out of the box.

## Outcome (2026-09-07)

Shipped this wave: the build recipe as `scripts/build-native-asset.sh`
(checked syntax, `::group::` logging, lockstep tag checkout) +
release.yml attaches the runner-platform tarball to every GitHub
Release (validated on the next dispatch — the script is versioned
and reviewed like code, never inline YAML, per the GHA-scripts rule).

Still open (the full platform-gem matrix): vendoring the dylib+bundle
PAIR into per-OS platform gems with `$ORIGIN`/`@loader_path` rpath —
needs runner-by-runner validation; tracked here, next wave.
