# 57 — the block-open batch: `key:` lines open maps without events

Status: LANDED (this wave)

## Why

The `key:`-with-block-below line (EMPTY value class) still paid the
MAP_START + key event pair — the dominant line of deep-nesting,
block-heavy, and every nested map. Item 54's pair batch only
covered same-line values.

## What

`yep_sink.on_block_open(ctx, key, line, key_col)`: when the engine
is about to open a FRESH mapping (not a sibling continuation — the
engine mirrors e_open_map's reuse test), the DOM builds the map node
(placed, stack pushed — exactly what the MAP_START handler would
have done) plus the pending key node in one call; the engine then
pushes its frame SILENTLY (e_open_map grew a `silent` flag) and
skips both events. Sibling keys keep the key-only event. Depth-cap
falls back to the event path, which owns the error.

## Gates

block-pair-diff extended (fresh-map + nested must-fire cases; the
counter covers opens): 847 cases / 814 batch lines / 0 failures.
279/279 ctest, validate, format.
