# 76 — the fused line kernel (one vector pass per line)

## Problem

The honest 0.1.28 standing (unthrottled, fair referee): block ~0.64,
anchor ~0.53 — while flow shapes sit at ~0.95-1.0. The block paths'
flat profile is a ladder of byte walks that each RE-TOUCH the same
line bytes:

| walk | touches | flat weight (block, per 8 s) |
|---|---|---|
| `yep_scan_line` (end + indent) | whole line | 183 |
| `yep_scan_shape` / `shape_value` | key span + value span | 291 |
| `yep_scan_plain` | key span + value span (again) | 305 |
| `yep_neon_find_not` (indent re-checks) | indent prefix | 226 |

A `key: value` line's bytes are walked 3-4 times before its nodes
exist. ryml classifies a line once.

## Design

**One kernel, one pass** (scan.c owns line semantics — MECE): for each
16/32-byte chunk of the line, classify every byte against the
indicator set (\n, \r, ' ', '\t', ':', '#', '-', '?', '&', '*', '[',
'{', quote) with the 68-style nibble-class tables, and produce the
line's facts from the lane bitmask stream:

- `end` = first break lane; `indent` = first non-space lane
- `colon` = first ':' followed by blank/EOL (needs the next lane —
  a 1-byte lookahead, the existing `yep_colon_terminates` logic on
  the candidate lane only)
- `first` = byte at the indent lane (the scalar fallback reads it)

The engine's `yep_line_shape` derivation then runs on FACTS, not
bytes: `key_end` = colon lane, value start = post-blank scan from the
colon lane (≤2 lanes). `yep_scan_plain`'s short-span tiny walk stays
for spans the classification already bounded — but the span's END is
known from the kernel, eliminating the re-walk.

Contract: bit-identical line facts across ISAs (differential suite
over the yaml-test-suite corpus + the fuzz corpus); the shape
classifier is unchanged (it consumes the same struct).

This is the biggest remaining structural lever; it is also the
largest risk surface (scan.c + engine memo seam) — spec-first, one
shape at a time (line facts first, shape second), block-pair-diff and
flow-direct-diff green at every step.

## Acceptance

- Line-facts differential spec (SIMD == scalar == current) over the
  conformance corpus.
- h2h: block-heavy and anchor-heavy measurably up (target ≥0.8x);
  ledger entry.
- No regression on flow shapes (they must not pay the new kernel).
