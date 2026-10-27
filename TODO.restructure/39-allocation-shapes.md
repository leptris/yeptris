# 39 — Allocation shapes: embedded arrays + exact-capacity hashes

Status: complete

## Why

After item 38, materialization dominates our remaining time (~0.4 ms
of 0.76 ms on ubuntu; scan is 0.35 ms). The biggest materialization
costs are container allocations:

- `rb_ary_new_capa(8)` forces a HEAP buffer even for 3-element
  arrays (Ruby embeds arrays up to RARRAY_EMBED_LEN_MAX inside the
  RVALUE itself — zero separate allocation). The corpus has 1,400
  3-element `tags` arrays paying a needless heap alloc each.
- `rb_hash_new_capa(8)` is created BEFORE the pair count is known;
  in the bulk path the hash is only needed at the END — exact
  capacity from the counted pairs avoids oversizing ar-tables.

## The experiment (knob-gated, CI referee)

`Native.shape_mode=` / `YEPTRIS_NATIVE_SHAPE`:
- `:pre` (current): capa(8) up front.
- `:natural`: arrays via `rb_ary_new()` + push (Ruby embeds small
  arrays); bulk-path hashes created AFTER the pair loop with exact
  capacity.

## Acceptance

- CI rows for both shapes on both platforms; the winner becomes the
  default and the gate margins re-read.
- Parity spec unaffected (shape is invisible to values).

## Outcome (ruled out by the referee)

CI verdict: `:natural` REGRESSES on both platforms — macos h2h
170/200 (pre) vs 82/200 (natural); ubuntu ratio 0.941x (pre) vs
1.112x (natural). The local arm64 win (0.694x) was stall-poisoned
noise (means 6-10ms on a saturated box). Default stays `:pre`; the
knob remains as standing evidence. Lesson re-confirmed: local dev
boxes under load are not measurement instruments — the fresh-runner
gate is.
