# TODO.prompt.md — the beat-rapidyaml execution prompt (wave 5)

Paste this into a fresh session. Self-contained. Release cadence:
0.1.18-0.1.22 (lockstep .1 gems/wheels each).

---

MISSION STATE: after five waves, every cheap cut is taken and
verified (classifier 49, fused flow 50+53, block batches 54+57,
zero-copy borrow 56, short-span walks + memo seeding 58, number
hook 59; items 61/62 closed by measurement — see the ledger's
2026-09-10 entries). Local head-to-head: flow-json 2.04x,
deep-nesting 1.03x, flow-single ~1.0x, block/anchor/wide
0.88-0.98x, scalar-heavy ~0.78-0.81x. Zero open issues anywhere.

THE ONE REMAINING LEVER IS STRUCTURAL and needs the owner's
sanction BEFORE building (rewrite-class, ABI + the 64B node gate):

**63 — compact nodes / one-walk build.** ryml's residual margin is
(a) two 64B node records per `k: v` line vs leaner records, and
(b) ~2 byte-walks per line (end-find + span walks) vs one. Design
space: 32-40B node records (pack style/implicit/tag_id/kind into
one word; sviews stay), or an arena-of-words representation;
fuse scan_line+scan_shape into ONE stopset pass recording
{eol, colon, comment} offsets. Gates unchanged: both
differentials, 279/279, the h2h referee (2-3 dispatches, read the
spread), the bounded-parse law on any new growth path.

IF SANCTIONED, implement in this order: the one-walk line scan
first (contained, scan.c), then the node compaction behind the
existing dom.h seam (public ABI untouched — handles are opaque).

RULES (standing): ulimit+wall-kill wrapper on EVERY binary run;
CI h2h medians referee; differentials are PERMANENT; measure
before building; designated sink literals; GCC AND clang
warning-clean; explicit-path git adds; no AI attribution; MECE/
DRY/OCP; specs for every behavior.

RELEASE (rebase merges): as every wave — C CHANGELOG
[Unreleased] -> version PR -> release.yml next_version -> approve
action_required (silent POST — re-list to verify) -> merge; Ruby
version PR -> tag v<X.Y.Z>.1 -> C-repo release.yml
republish_version (SMOKE PASS required); Python version PR ->
tag -> wheels -> twine -> clean-venv verify. Lockstep
{c-semver}.{patch}.

DONE MEANS: every shape >= 1.15x on BOTH CI platforms, gates
green, zero issues, ledger current, lockstep releases shipped.
