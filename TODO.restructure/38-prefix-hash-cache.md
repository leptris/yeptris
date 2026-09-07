# 38 — Prefix-hash the token cache (the leptris nametab trick)

Status: complete

## Why

Item 37's decomposition isolated the x86 deficit to the cached
token path's hash cost. The byte-wise FNV loop spent ~15-20ns per
token x ~15k tokens per reference-corpus parse - most of the ubuntu
gap to JSON.parse.

## What

`jr_hash` becomes an 8-byte-prefix key: one safe `memcpy` load of
min(8, len) bytes, mixed with length via two multiplies and a shift
(the same shape leptris's nametab interner validated). Equal-length
short keys differ in the first 8 bytes almost always; collisions
still resolve through the existing full `memcmp`.

## Result (CI referee, gated rows)

- ubuntu (disable/bulk/cache=on): **0.745x mean, 200/200 h2h**
  (was 0.96-0.99x) - 25% faster with the margin asked for.
- macos (none/bulk/cache=on): 0.759x mean, 169/200 h2h.

Both platforms now ship ~0.75x against the stdlib.
