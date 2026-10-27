# 34 — Why loaded boxes lose: the GC-disable/freelist hypothesis + CI A/B

Status: in progress

## The phenomenon

- Idle (arm64 dev box): yeptris 0.75× mean FASTER, head-to-head 92.5%.
- Loaded (user's box, ubuntu CI runner): ~1.5–2× SLOWER.
- Raw scan alone is faster than JSON.parse's whole call (0.35 ms nop
  floor vs 0.6 ms) — "raw parse ~on-par idle" holds.

## The hypothesis (mechanism)

Our native parse calls `rb_gc_disable()` for its duration and
re-enables after. Consequences across benchmark iterations:

1. JSON.parse's allocations trigger minor GCs DURING parse — the
   previous iteration's garbage is recycled into freelists
   continuously; slot reuse is immediate.
2. With GC disabled, our garbage from iteration N is NOT swept
   before iteration N+1 allocates — we consume fresh slots/pages
   every iteration until a deferred (larger) collection fires after
   some later re-enable.
3. Fresh-page allocation is cheap on an idle big-memory box (≈ on
   par), and expensive exactly where it matters on loaded boxes:
   page faults, memory-bandwidth contention, TLB pressure. Hence
   "on-par idle, slower loaded".

Secondary suspects (measure in the same A/B): `rb_hash_bulk_insert`'s
+1.4k allocations per corpus (bought idle-median, may cost under
contention); preextended capa(8) containers.

## The experiment (CI is the referee)

1. `json_ruby.c` gains a runtime GC-strategy switch (ENV at load;
  no rebuild to compare): `disable` (today) | `none` (never disable)
  | `start` (disable, then `rb_gc_start` after re-enable — pay the
  minor GC in-window like JSON.parse). `Yeptris::Native.gc_mode`
  reports it.
2. A CI job runs `benchmark/json_profile.rb` under every mode on
  ubuntu + macos and prints the table (evidence first; the gate
  threshold comes FROM the data).

## Acceptance

- The mode table from CI identifies whether the GC strategy is the
  loaded-box regressor (mode ordering consistent with the
  hypothesis) — or rules it out with data.
- Decision recorded here; the fix is item 35.

## Outcome (the evidence)

The profile's gc-footprint phase proved the mechanism:

- disable: yeptris **+478 heap pages / 50 iters, 0 minor GCs** —
  fresh pages every iteration, nothing recycled (JSON.parse in the
  same process: −135 pages, 7 minors).
- none: yeptris **+0 pages, 16 minors** — byte-for-byte the stdlib's
  own cadence.

And the A/B (three CI rounds, separate processes — in-process mode
switching is INVALID, measured contamination; :start catastrophic
at 8×): the platforms SPLIT reproducibly — macOS favors none
(0.745/0.749/0.718×, h2h up to 91%), ubuntu favors disable
(0.911/0.922/0.961× vs none's 1.06–1.20×). Fresh x86 VMs have free
pages; loaded boxes punish page growth — exactly the user's report.
