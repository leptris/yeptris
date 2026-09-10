# 62 — node-init trim: split dom_open_node by kind

Status: pending (measure the memset share first)

dom_open_node memsets 64B + writes ~10 fields per node; ryml's init
is field-selective. Split: the shared prefix (links, kind) vs
kind-specific tails; scalars (the majority) skip collection-only
fields. Guard: the 64B node-size gate and the mapindex/mutate
readers of zeroed fields. Profile share first via the h2h delta on
scalar-heavy.
