# 51 — deep-nesting: re-measure, then profile before touching anything

Status: pending

deep-nesting (0.67x; the ubuntu cell is still blank in the 2026-09-09
table). The corpus is `lN:` keys at increasing indent — a pure
per-line choreography shape with a value on the final line only. It
should ride items 49 (every line is the `key:` shape) and 50 (no flow
at all — but the per-line cut compounds). ubuntu's missing row comes
from the same CI dispatch.

Rules: after 49+50 land, dispatch bench.yml at the branch; if either
platform is still < 1.0x, Instruments/perf on the bench corpus BEFORE
any code — items 46/47 are the precedent for building unprofiled.

Acceptance: > 1.0x on both platforms, or a profiled decomposition in
the ledger naming the next wall.
