# 65 — the Linux profile artifact (item 55's tool) + 2b closed by resolution

Status: LANDED (this wave)

## Why

The CI h2h referee's single-run medians swing ±0.3 (anchor-heavy
measured 0.28-1.09 across runs of near-identical code); after 64-2a
the medians sit inside that band — the referee CANNOT resolve
node-size micro-cuts, and the ubuntu asymmetries (scalar-heavy
0.41x vs mac 0.54x; deep-nesting 0.53x vs 0.74x) stay unexplained.

## What

scripts/profile-linux.sh + the Profile workflow (workflow_dispatch):
perf record/report over the asymmetry shapes at 199Hz, uploaded as
artifacts. The next perf session reads those reports BEFORE any
code (the 46/47 law, now with the right instrument).

## 64-2b: CLOSED by resolution — a 4B union (alias target sharing
first_child's slot) is ~7% of the record, an order below what the
referee can see through runner noise. 64-2c stays OPEN, gated on
what the profiles name (if line/col demotion is what the report
says, it gets built with the accessor-join cost priced in).
