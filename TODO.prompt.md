# TODO.prompt.md — the beat-rapidyaml execution prompt (wave 4)

Paste this into a fresh session. Self-contained: context, targets,
designs, gates, release. Release cadence: 0.1.18-0.1.20 (lockstep
.1 gems/wheels each).

---

MISSION: beat rapidyaml on every comparable shape WITH MARGIN on
BOTH CI platforms. Standing (CI h2h medians after waves 1-4):
WON flow-json (1.05-1.69x), block-heavy (~1.0x), anchor-heavy
(~1.0x), wide-mapping (~1.0x), deep-nesting (~1.0x); flow-single
0.69-0.99x and scalar-heavy 0.51-0.81x remain behind — BOTH are
dominated by ryml's laziness advantages: ryml does NOT type
scalars at parse (we resolve every scalar) and its node init is
leaner. Zero open issues on all three repos — keep it.

READ FIRST (merged): TODO.restructure/49,50,53,54,56,57,58 — the
landed designs (line classifier, fused flow walk, block pair/open
batches, zero-copy borrow, short-span walks + memo seeding).
benchmarks/PERF-LEDGER.md last five entries (the h2h referee, the
bounded-parse law, every dead end). src/yeptris/parse/events.h
(the sink contracts), scan/json.h (the walker), scan/scan.h
(line shapes).

IMPLEMENT next (measure-first; a unit that regresses reverts):

1. **Lazy typing (61)** — the one structural gap left: our DOM
   stores tag_id at parse; ryml defers typing to access. Design:
   tag_id becomes computed-on-demand at the ACCESS seams (marshal/
   emit/visitors) with the parse-side resolve() removed from the
   hot paths (pair/open/fused builders). The resolver stays the
   typing SSOT — it just runs at access. Differential gates stay
   green because they compare trees INCLUDING tag_id (both sides
   then compute lazily at comparison). BIG: touches every tag_id
   consumer; spec-first.

2. **Node-init trim (62)** — dom_open_node memsets 64B per node;
   ryml's node init is field-selective. Split the init: the fields
   every kind needs vs kind-specific. Profile via the h2h delta.

3. **The ubuntu asymmetry (55)** — scalar-heavy 0.51x ubuntu vs
   0.79x mac persists at identical allocs; the CI profile artifact
   (scripts/profile-linux.sh, still to be added) names the cause.

RULES (unchanged, hard-won): ulimit+wall-kill wrapper on EVERY
binary run (four 130-240GB incidents); CI h2h medians referee —
dispatch bench.yml 2-3x and read the spread; differential gates
are PERMANENT (any new fast path grows its must-fire list);
measure before building; designated sink literals; GCC AND clang
warning-clean; explicit-path git adds; no AI attribution; MECE/
DRY/OCP; the bounded-parse law covers any new growth path.

RELEASE (rebase merges): as waves 1-3 did — C CHANGELOG
[Unreleased] -> version PR -> release.yml next_version ->
approve action_required (silent POST — re-list to verify) ->
merge; Ruby version PR -> tag v<X.Y.Z>.1 -> C-repo release.yml
republish_version (SMOKE PASS required); Python version PR ->
tag -> wheels -> twine -> clean-venv verify. Lockstep
{c-semver}.{patch}.

DONE MEANS: every shape > 1.0x with >= 1.15x margin on BOTH
platforms, gates green, zero issues, ledger current, releases
shipped.
