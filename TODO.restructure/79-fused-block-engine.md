# 79 — the fused block engine (the last structural lever)

## Problem

Ubuntu CI flat profile (v0.1.30, Linux perf, DOM-relevant symbols):
`e_node` 11.2% (the inlined classified path — GCC/LTO attribution
differs from clang's, it IS the per-line fast arms), stopset 8.7%,
`engine_run_impl` 8.0%, `scan_plain` 6.3%, core12 3.5%, dom_place
2.7%. Mac shows the same shape at half the percentages. The per-line
state machine (loop head → flags → frame unwind → dispatch → fast arm
→ fold lookahead) plus the per-node build is ~19-25% of block-shape
time — the one place ryml's design is categorically leaner (its
block loop is a single hand-rolled scanner with no event seam).

Everything smaller is done: string borrows are end-to-end, nodes are
48 B, the fast-path surface is complete (pair/open/item), kernels are
SIMD, the resolver is gated, facts are computed once.

## Design (leptris's direct-parse shape, ported to YAML's grammar)

ONE loop owns a block document: the line loop and the node build
fuse. Per line: facts kernel → shape derive (no function-call chain —
the arms become inline cases) → direct DOM placement. The event seam
stays for: flow values (the fused JSON walker already bypasses
events), pull/push/recorder consumers (they keep the full event
engine — the two paths share the classified arms as functions, but
the DOM sink gets a dedicated direct loop). Conformance bar: the
differential gates (block-pair-diff, flow-direct-diff, the
yaml-test-suite event comparison) must stay byte-identical; the
engine's error positions and line/col events are public API.

Scope: a focused rewrite of `engine_run_impl`'s block dispatch +
`e_classified`/`e_node`'s block arms into one TU-local loop with the
DOM sink compiled beside it (LTO already inlines much of this — the
win is removing the remaining seam: per-line memo struct copies,
frame-array double-derefs, event-struct fills on fallback paths).

Estimate: 2-3 focused sessions; spec-first (every yaml-test-suite
event stream byte-identical), one shape at a time.

## Status

Specced, not started. The next session picks this up with the CI
profile artifacts (which now carry the kernel-table and CPU flags
diagnostics — a scalar-dispatched run can no longer masquerade as an
engine problem).
