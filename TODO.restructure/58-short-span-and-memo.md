# 58 — short-span scalar walks + line-fact memo seeding

Status: LANDED (this wave)

## Why

Two per-line taxes on every block shape: (1) e_parse_value's
following-lines loop called yep_scan_line directly, so the fast
arms' memo always MISSED — every block line's facts were scanned
twice; (2) yep_scan_plain dispatched the SIMD stopset kernel for
every span, including the 2-8 byte keys that dominate block
corpora — the dispatch cost more than the scan.

## What

The loop seeds e_line_info_here when the cursor sits at the line
start (the fast arms then read the memo). scan_plain keeps a scalar
byte walk below the same 64-byte gate scan_line uses for its
end-find. Long values (scalar-heavy's sentences) keep the kernel.

## Measured (local h2h sanity, loaded box)

deep-nesting 0.59 -> 1.03x, flow-single 0.99x, block-heavy 0.98x,
anchor 0.91x, wide 0.92x vs ryml. Gates: 279/279, both
differentials 0 failures, validate, format.

## Not built (measured irrelevant): item 59's root-flow dispatch —

flow-single's whole parse pays ONE scan_line over its single giant
line (~0.3ms of ~35ms): skipping it cannot move the number. The
resolver table (old item 60) is likewise dead: core12 already
length-gates and O(1)-rejects wordy scalars.
