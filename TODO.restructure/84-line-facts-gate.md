# 84 — the line-facts vector gate tested the wrong length

## Problem

76 gated the facts vector sweep at "remaining buffer >= 64" — which is
true for EVERY line but the last of a document. Every 18-30 byte line
of anchor-heavy ran the per-chunk mask chain that 76 itself had
measured 2x slower than the tight scalar loops: yep_neon_line_facts
was anchor-heavy's #1 flat entry (1146 samples, ~12%), where the
pre-76 scan_line cost ~2%.

## Fix

A 32-byte scalar probe for the first break decides: a break inside it
is a short line (scalar facts — one fused walk); no break and a
>=64B span runs the vector sweep. The probe is the first 32 steps of
the walk it gates — no double scanning of substance.

## Measured

Vector-entry samples 1146 -> 556 (the sweep no longer runs on short
lines; the residual is the probe+dispatch). The facts machinery
(~1200 samples across probe+scalar+derive) remains the block paths'
largest single cost — the honest remaining lever is item 79's fused
engine, which would restructure where facts are computed at all.

## Status

Landed with the item-81 round; CI referee arbitrates the shapes
(anchor-heavy was 0.90x on the last kernels-live run — the only
shape under parity).
