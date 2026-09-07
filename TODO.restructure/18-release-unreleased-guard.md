# 18 — The release workflow fails loudly without [Unreleased]

## Why
The changelog backfill (item 17) exposed it: the cut phase's sed
renames `## [Unreleased]` — and silently no-ops when the header is
absent. TEN releases shipped undocumented before anyone noticed.

## Plan
- A guard step before the bump: the CHANGELOG must contain
  `## [Unreleased]`, else fail with a message naming the fix

## Acceptance
- local simulation: header present -> passes; absent -> non-zero

## Status: COMPLETE
Local simulation: header present exits 0, absent exits 1; the YAML
validates. The guard runs before the bump, so a failed dispatch
consumes nothing.
