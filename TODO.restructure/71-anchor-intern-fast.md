# 71 — anchor/alias intern fast path

## Problem

anchor-heavy h2h is the worst shape (0.44-0.48x). The anchor machinery
owns ~19% of its flat profile:

| function | samples | share |
|---|---|---|
| nametab_set | 229 | 6.6% |
| e_alias | 53 | 1.5% |
| nametab_get | 45 | 1.3% |
| e_props | 43 | 1.2% |
| yep_view_hash | 22 | 0.6% |

Every anchor declaration and alias reference interns the NAME through the
nametab (open addressing, FNV-1a + finalizer over the name bytes, prefix
slot compare). Anchor names in the corpus are 9-16 B — one byte loop +
one finalizer + probe per anchor and per alias.

## Design

The nametab slot already carries the first 8 bytes (`prefix`); the hash
may stop feeding on the byte loop for the common case: hash from
(len, first-8-bytes, last-8-bytes) — three word reads, no loop, names
≤16 B hashed without touching middle bytes (collisions resolve by the
prefix/keys compare that the probe already does; correctness is the
probe's, the hash only distributes). Longer names keep the full mix.

`yep_view_hash` is shared with the map index (SSOT): the change is in
`yep_view_hash` itself — the map index benefits identically.

Conditional on the post-68/69/70 profile still showing the intern path
≥3% — if the other items move the shape enough, drop this item instead
of shaving a cold path (measure first).

## Acceptance

- nametab specs green (collisions/overwrite/clear/reserve paths).
- mapping-key interning specs green (mapindex shares the hash).
- h2h: anchor-heavy up measurably; ledger entry.

## Closure (2026-09-12)

Executed (the interner's hash was already word-wise — item 18A's work
held; the residual costs were elsewhere):

1. **core12 single-branch shape**: gate → word-lead branch (null/true/
   false memcmp + return, no walk) → number path. anchor-heavy flat
   profile: core12 649 → 249 samples. The old chain ran the word
   memcmps for every length-4/5 scalar before ruling.
2. **Repeat-alias memo** in the engine (anchor_id_of): merge-key YAML
   re-uses one alias thousands of times; the memo answers those with one
   input memcmp, skipping hash + a cache-missing probe. Ping-ponging
   alias streams still miss — measured acceptable.
3. **Anchor table first-alloc sizing** (dom_anchor_set): growth from a
   bare 64 re-copied the ordinals table ~18× per anchor-heavy parse;
   len/32 first allocation (guarded, doubling continues above it).

Measured (fair-order referee, anchor-heavy): 0.44-0.48x → 0.47-0.49x —
small in ratio terms; the remaining gap is engine_run_impl + e_node
(the nested-map opener) + the intrinsic 48B-node build. Ledger numbers
recorded. The hash itself needed nothing — the item's design sketch was
wrong about where the cost was; measurement beat the guess.
