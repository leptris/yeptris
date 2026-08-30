# TODO.impl/18 — Benchmarks: matrix, corpora, CI artifacts, perf ledger

Status: pending · Depends: 06+ (parse path); grows with every item · Layer: `benchmarks` · PLAN.md phase: 2+

## Goal

Numbers or it didn't happen: a matrix harness measuring us against
libyaml (always), libfyaml/rapidyaml (when available via CMake options),
and Psych/psych-pure (Ruby, in 15), with per-push CI artifacts and a
ledger that records both wins and dead ends.

## Deliverables

- `benchmarks/matrix/` — C++11 harness, deterministic corpora
  (`corpora.json` manifest, shared with 16/19):
  - shapes: block-heavy, flow-heavy/JSON, scalar-heavy, anchor-heavy,
    deep-nesting, wide-mappings, mixed real-world (suite files + libyaml
    examples);
  - generators seeded (`--seed` reproducible), sizes 10 KB–100 MB;
  - measures: MB/s parse (DOM/pull/recorder separately), emit MB/s,
    allocations/parse (alloc hook), peak RSS, node-bytes-per-input-byte,
    builder sink share, scalar borrowed-ratio;
  - output: JSON + Markdown (leptris artifact format), `--quick` mode
    for CI, `--full` for releases.
- `benchmarks/ruby/` — Ruby end-to-end (load/dump/stream vs psych +
  psych-pure; `ObjectSpace` allocation counts; per-shape tables).
- `benchmarks/PERF-LEDGER.md` — the discipline file: every optimization
  attempt recorded with before/after numbers, including failures
  (leptris's "measured dead" entries are the most valuable pages of its
  history — e.g. two-pass parser floor 88.5%, split-stream attrs loss).
- CI: `bench.yml` runs the matrix per push, uploads artifacts, posts the
  delta vs the previous commit's artifact in the job summary.

## Design decisions

- libyaml comparison is mandatory and unconditionally linked (it is the
  mission); libfyaml/rapidyaml optional `-DYEPTRIS_BENCH_RYML=ON` etc.,
  gracefully skipped when absent — numbers from every machine, field
  comparisons when available.
- Machine-relative reporting (leptris README convention): ratios vs the
  same-machine libyaml run, never absolute cross-machine claims.
- Targets from PLAN.md §5 are the standing scoreboard; misses are ledger
  entries, not secrets.

## Phases

A. Harness + libyaml baseline recorded (before we're fast — the "0.5×"
   baseline page of the ledger is part of the story).
B. Per-item measurement as 04–14 land; scoreboard tracking.
C. Ruby matrix when 15 lands; release-mode full runs.

## Acceptance

- `matrix --quick` runs in CI < 5 min; artifacts present on every push.
- Every perf-target claim in PLAN.md §5 backed by an artifact run by
  Phase-5 exit; ledger has entries for every attempted lever.

## References

- `~/src/leptris/leptris/benchmarks/` (harness + artifact patterns +
  README methodology), `~/src/external/rapidyaml` benchmarks docs (the
  field's methodology), `metanorma/serialbench` (Ruby field comparison
  pattern leptris uses).
