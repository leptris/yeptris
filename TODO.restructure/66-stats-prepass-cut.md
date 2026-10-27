# 66 — kill the stats pre-pass (profile-named, ubuntu shapes)

Status: spec'd with CI perf data (see the 2026-09-12 ledger entry);
implement next

## Why

The Profile artifact's first data: the whole-buffer scan_stats
pre-sizing sweep costs ~12-13% self-time on scalar-heavy and
deep-nesting (ubuntu, the exact asymmetry shapes). ryml has no
pre-pass.

## Design

parse_impl replaces yep_dom_prepare/yep_engine_prepare's exact
stats with length heuristics (node_hint = len/2, nametab reserve
from a cheap amp memchr or skip), letting dom_grow absorb growth.
The 18B allocation table gates allocs/MB (no regression); the h2h
medians referee the win. If growth memmoves reappear above the
sweep's cost, the cut reverts (the 46/47 law).

## Gates

279/279 + differentials + validate; the 18B table before/after;
CI h2h spread (2-3 dispatches).
