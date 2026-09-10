# TODO.prompt.md — the beat-rapidyaml execution prompt (wave 3)

Paste this into a fresh session. It is self-contained: context,
targets, designs, gates, and the release procedure. Everything it
references is merged on main. Release cadence so far: 0.1.18,
0.1.19 (lockstep .1 gems/wheels).

---

MISSION: yeptris must beat rapidyaml on every comparable benchmark
shape WITH MARGIN on BOTH CI platforms. Won outright: flow-json
1.11x/1.15x (item 53's fused walk + item 56's zero-copy borrow).
Still behind (best CI h2h medians, mac/ubuntu): flow-single
0.68/0.91, scalar-heavy 0.42/0.81, anchor-heavy noisy 0.6-0.9,
deep-nesting 0.50/0.80, block-heavy 0.71/0.77, wide-mapping
0.68/0.91. The gap to 1.0x+margin is per-shape engineering with a
named lever below — not rediscovery. Also standing: fix ALL GitHub
issues (currently zero open across all three repos — keep it).

READ FIRST (merged, ~20 min):
- TODO.restructure/53,54,56,57 — the landed designs: the fused
  validate+build (one walk per flow span), the block pair/open
  batches (one sink call per line), and the zero-copy borrow fix
  (THE single biggest win: parse.c had set dom->input_base AFTER
  the run — every string was arena-copied).
- benchmarks/PERF-LEDGER.md, last four entries — the h2h referee
  (interleaved medians; separate-phase tables are phase-biased and
  BANNED), the bounded-parse law, the standing numbers.
- src/yeptris/scan/json.h (the walker SSOT), scan/scan.h (line
  shapes), parse/events.h (the sink contracts: flow build/commit/
  rollback, block pair, block open).

IMPLEMENT, in this order (each unit: spec + gates first, CI h2h
referee, ledger before/after; a unit that regresses reverts):

1. **One-walk line scan (58)** — fuse scan_line + scan_shape: the
   classifier makes up to three passes over a line's bytes (SIMD
   end-find, key scan_plain, value scan_plain). One walk yields li
   + shape together. Targets scalar-heavy (12-word values) and
   every block shape's per-line floor.

2. **Root-flow dispatch without the line scan (59)** — a document
   whose first content byte is '['/'{' pays a full SIMD end-find
   over a possibly-megabyte line before the fused walk re-walks it.
   Dispatch flow-at-root directly; the line end falls out of the
   walk. Targets flow-single (0.68-0.91).

3. **The resolver fast path (60)** — core12's resolve() runs per
   scalar as a chain of length checks + memcmps; ryml types with a
   first-byte dispatch. Table-drive the core12 impl (OCP: the
   resolver interface unchanged). Profile first via item 55's
   linux artifact. Targets scalar-heavy and every key resolution.

4. **What the linux profile names (55)** — scalar-heavy 0.42x
   ubuntu vs 0.54x mac at identical allocation counts is still
   unexplained; deep-nesting 0.50x vs 0.80x likewise. The CI
   profiling artifact (scripts/profile-linux.sh + the Profile
   workflow) is the tool.

RULES OF ENGAGEMENT (hard-won, all in the ledger):
- NEVER run a test/bench binary unattended: ulimit + wall-kill
  wrapper (four 130-240GB runaway incidents; the bounded-parse law
  caps parses at O(input), but the wrapper is still mandatory).
- The dev box saturates: CI fresh-runner h2h medians referee.
  Single-run medians swing ±0.15 — dispatch bench.yml 2-3x and read
  the spread before concluding.
- The differential gates (flow-direct-diff, block-pair-diff) are
  PERMANENT: any new fast path grows its must-fire list and reuses
  the tree comparator (test/flow/tree_diff.h). Divergence = the
  unit does not land.
- Measure before building (46/47 precedent). GCC AND clang
  warning-clean (validate.sh covers both); clang-format-18.
- All sink literals designated; new sink fields optional (NULL
  default) — streaming sinks untouched by design.

RELEASE (rebase merges; `gh pr merge --rebase`): C CHANGELOG
[Unreleased] entries → version-bump PR → release.yml
next_version=<X.Y.Z> → approve action_required on release/v* (the
approve POST is silent — re-list to verify) → merge. Ruby:
version-bump PR (lib/yeptris.rb) → merge → tag v<X.Y.Z>.1 → C-repo
release.yml republish_version=<X.Y.Z> (SMOKE PASS must print before
every gem push). Python: version-bump PR (pyproject.toml) → merge
→ tag → release-wheels.yml → twine upload the downloaded artifacts
→ clean-venv pip verify. Versions: lockstep {c-semver}.{patch},
reasonable increments (0.1.18 → 0.1.19 cadence).

CODE LAWS (standing): C11 warning-clean, designated sink literals,
explicit-path git adds (never `git add -A`), no AI attribution,
never push main or tags except the binding flow above, MECE/DRY/
OCP, specs for every behavior, the bounded-parse law applies to any
new growth path.

DONE MEANS: every ryml-comparable shape > 1.0x WITH margin (target
≥1.15x) on BOTH CI platforms, all gates green, zero open issues,
ledger updated, lockstep releases shipped.
