# 12 — docs/FFI.md: document the shipped consumption paths

## Why
FFI.md (written for the NEXT binding author) documents the recorder,
the event records, and the per-node DOM builder — but both bindings
moved on: the value drain (typed records) and its COLUMNAR form are
the load fast paths (v0.1.2+, adopted 0.1.2-0.1.3), and dumping is
yeptris_document_build (one call, entries+blob; v0.1.1). The doc's
own audience would build against 2026-09-03's architecture.

## Plan
- Loading section: recorder -> value drain -> drain_columns (the
  adoption ladder, when to choose which, feature-detect guidance)
- Dumping section: document_build first (the bulk path), the
  per-node builder as the mutation-flow tool
- ABI note: YeptrisValueColumns layout status (public header,
  pinned by the equivalence test)

## Acceptance
- every CONSUMPTION PATH the bindings ride is documented (the
  loading ladder: recorder -> value drain -> columnar; the bulk
  dump; the per-node builder as the mutation tool)
- FFI.md stays a concepts doc; the public headers are the symbol
  reference (an earlier draft of this acceptance demanded every
  symbol in the doc — the wrong bar, it would bloat the concepts)

## Status: COMPLETE (main; docs ride with the next release)
