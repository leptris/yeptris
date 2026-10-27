# 55 — profile the standing table: anchor-heavy, flow-single, and the ubuntu asymmetry

Status: this session's diagnostic

The h2h table has two unexplained facts: (1) the per-shape residual
walls after 49+50 (which code holds anchor-heavy 0.57x?); (2) the
PLATFORM asymmetry — scalar-heavy 0.96x mac vs 0.41x ubuntu,
deep-nesting DOM at 234 MB/s mac vs 124 ubuntu while ryml is flat.
Same allocs both platforms, so it is CPU, not allocator.

Method: macOS `sample` on the bench corpora for the distribution of
our own work (load-tolerant: percentages, not absolutes); a Linux
side via a CI profiling artifact (perf record in a dispatched
workflow, report uploaded) if the box stays saturated. Deliverable: a
ledger perf-diag entry naming the next wall per lost shape — the
46/47 law applies to anything built on top.
