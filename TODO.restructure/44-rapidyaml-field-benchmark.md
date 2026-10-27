# 44 — rapidyaml: the field benchmark, finally measured

Status: complete

## Why

The mission statement names rapidyaml (2-3x libyaml) as "the field
benchmark to exceed" — and every scoreboard so far measures only
libyaml/Psych/PyYAML/stdlib-JSON. At 1.8-3.4x libyaml we are IN
rapidyaml's class but have never raced it. "Beat ALL references"
ends here or is exposed as unmeasured.

## Plan

- [ ] Fetch rapidyaml PINNED (tag + SHA, the libyaml-fetch
      discipline) and build the static lib.
- [ ] A ryml column in bench_matrix (guarded by
      YEP_BENCH_RYML_ROOT like the libyaml column): ryml::parse
      over each corpus shape, same iterations/min-of-N discipline;
      ryml parses IN PLACE — a mutable scratch per iteration.
- [ ] Record the honest table in the perf ledger; every shape,
      both directions where ryml has an emitter.
- [ ] If any shape loses: the ledgered C engine levers become live
      work (e_parse_value self ~20%, sink batching, SIMD
      validate+count fusion — PERF-LEDGER's standing list). If we
      win: pin the table and gate what's gateable.

## Acceptance

- The ledger carries a dated yeptris-vs-rapidyaml table over the
  matrix shapes, min-of-N, same binary.
- No unmeasured "field benchmark" remains in the mission.


## Outcome (2026-09-08)

MEASURED — every leg: pinned fetch (v0.16.0, f8ac8dd, SHA-asserted
in bench.yml), the ryml column in bench_matrix (reused tree, the
buffer copy outside the timed region; a THROWING error callback —
ryml's default ABORTS and it rejects suite-valid inputs like 236B),
the honest table in the perf ledger, CI bench wired. The verdict:
we LOSE 6/7 shapes (0.44-0.97x). The decomposition scoped the
campaign: pull==DOM==recorder means the ENGINE LOOP is the wall;
even the engine-free JSON seam is 0.70x of ryml. The campaign is
[[45-engine-campaign-ryml]].
