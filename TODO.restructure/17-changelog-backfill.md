# 17 — The CHANGELOG stopped at 0.1.0 — ten releases undocumented

## Why
The release workflow renames `## [Unreleased]` on dispatch — but no
such header existed after 0.1.0's release, so every dispatch since
(0.1.1 through 0.1.10) shipped with NO changelog entry. A user on
0.1.10 reads a changelog that ends two days and ten releases ago.

## Plan
- Backfill [0.1.1]..[0.1.10] from the release notes and PERF-LEDGER
- Restore the `## [Unreleased]` header so the workflow's rename has
  its subject and future releases accumulate

## Acceptance
- every tag v0.1.1..v0.1.10 has a section with its shipped changes
- an [Unreleased] section exists for the next dispatch

## Status: COMPLETE
All ten sections backfilled from release notes + the ledger; the
[Unreleased] header restored so the workflow rename works again.
