# 09 — Terminal audit: the board is complete

## The wave-4 sweep (all clean)
- every Ruby file carries frozen_string_literal; zero trailing
  whitespace, zero tabs in lib/
- Python: no bare excepts, no mutable default args; public
  functions carry return-type hints
- lib/ contains zero banned constructs beyond the four documented
  protocol exceptions (board item 08 lists each with its justification)

## The remaining performance levers (unified reference)
These live in benchmarks/PERF-LEDGER.md with measurements and are
tracked as the perf campaign's task #42 — listed here so the boards
are one map:
- the per-line frame machinery (engine_run_impl self ~23% +
  e_parse_value self ~25% on anchor shapes) — the guarded-grammar
  approach measured dead; the viable shape is a scan-fused byte-class
  line classifier
- the direct-from-index DOM builder (bounded at 12-25% by the
  recorder-vs-DOM gap; the strict-JSON seam proved the pattern)
- binding host-walks at their per-node floors (columnar drains
  adopted; the remaining cost is host object creation itself)

## Status: COMPLETE — the board closes
Nine items: seven complete, two closed by measurement (04 by
profile, the pair-fast-path attempt by bench — PERF-LEDGER.md).
New work arrives as new items with why/plan/acceptance/status.
