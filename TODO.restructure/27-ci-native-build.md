# 27 — CI builds the native materializer

Status: complete

## Why

yeptris-ruby's CI builds libyeptris but never the extension, so
`spec/native_spec.rb` silently skips — the path that beats
JSON.parse has zero CI coverage. The extension must build on both
CI OSes and the full 194-example suite must run.

## Plan

1. `extconf.rb`: honor `YEPTRIS_SRC` (source root) before the
   relative candidates; CI sets it to the checked-out C repo.
2. `ci.yml`: after building libyeptris, build the extension
   (`cd ext/yeptris_native && ruby extconf.rb && make`), copy the
   bundle into `lib/yeptris/`, then run rspec as today.
3. Assert coverage: CI logs the example count (194, not 175) — a
   silently-skipping perf path is a regression of the gate.

## Acceptance

- Both OS CI runs report 194 examples, 0 failures.
- A PR that breaks the extension build fails CI (not skips).
