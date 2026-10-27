# 25 — Binding hygiene sweep (autoload, no ducks, specs)

Status: complete

## Why

Standing restructure directive: autoload-only internals, no
`require_relative`, no ducks, good specs throughout. After items
22–24 land, sweep both bindings for drift and close gaps.

## Plan

1. **Ruby**: confirm every nested constant is autoloaded from its
   parent file; the native extension loads via `require
   "yeptris/native"` guarded (LoadError → FFI ladder). No new
   `require` of library code.
2. **Ruby specs**: native path + fallback path both exercised;
   JSON beat gate as a committed perf example (skip if no native).
3. **Python**: no equivalent CPython ext this wave (pickle emitter
   is the analogous lever — measured need first, follow-up item).
   Confirm `_loader` stays isinstance-typed (no ducks).
4. **Docs**: FFI.md ladder gains rung 0 (native materializer);
   README states the JSON.parse win with numbers.

## Acceptance

- Full rspec green with and without the extension built.
- `grep` hygiene gates clean under `lib/`.
- README/FFI.md updated; CHANGELOG `[Unreleased]` carries the win.
