# 62 — node-init trim: split dom_open_node by kind

Status: CLOSED by arithmetic (2026-09-10) — selective field writes
cost ~the same bytes as the 64B memset they would replace. Ledger
entry has the count.

dom_open_node memsets 64B + writes ~10 fields per node; ryml's init
is field-selective. Split: the shared prefix (links, kind) vs
kind-specific tails; scalars (the majority) skip collection-only
fields. Guard: the 64B node-size gate and the mapindex/mutate
readers of zeroed fields. Profile share first via the h2h delta on
scalar-heavy.
