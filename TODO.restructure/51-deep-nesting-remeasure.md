# 51 — deep-nesting: re-measure, then profile before touching anything

Status: MEASURED (h2h, 2026-09-10): 0.69x mac / 0.47x ubuntu —
did NOT ride to 1.0x. The ubuntu DOM runs deep-nesting at half the
mac MB/s (124 vs 234) while ryml is flat: platform asymmetry, not
shape. Profile on a linux runner before any code (the 46/47 law).

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
