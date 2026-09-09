# TODO.prompt.md — the beat-rapidyaml execution prompt

Paste this into a fresh session. It is self-contained: context,
targets, designs, gates, and the release procedure. Everything it
references is already merged on main.

---

MISSION: yeptris must beat rapidyaml on every comparable benchmark
shape with margin. Four cells remain lost (CI fresh runners, macOS
DOM vs ryml): **anchor-heavy 0.33x, flow-json 0.50x, flow-single
0.59x, deep-nesting 0.67x**. Won and to be protected: block-heavy
1.48x, wide-mapping 1.13x, scalar-heavy 1.07x; Ruby/Python JSON
0.699-0.785x at their 1.00 CI gates; YAML 1.8-5.4x vs
libyaml/Psych/PyYAML. Zero open issues anywhere — keep it that way.

READ FIRST (all merged, ~15 min):
- TODO.restructure/48-flow-anchor-direct-build.md — the campaign:
  designs, ceilings, the anchor decomposition, what was already
  checked (the ALIAS arm is O(1); do not re-read it).
- TODO.restructure/45-engine-campaign-ryml.md — the intel: block
  wrapper costs 37%; e_flow_json BEATS parse_json on string-heavy
  flow; e_flow_json is two-pass by design.
- benchmarks/PERF-LEDGER.md, last three entries — the full CI ryml
  table, the anchor profile (~60% in e_node/e_parse_value — the
  per-line choreography is THE wall for anchor AND flow wrappers),
  and the negative results that must not be repeated (items 46/47:
  scanner rewrites are DEAD; the cost is the EVENT PIPELINE).

IMPLEMENT, in this order:

1. **The line classifier (Phase B)** — engine.c, e_node:2149 and
   e_parse_value:2482. Hoist a one-pass byte-class line
   classification INTO the line scan (scan.c owns scanning; the
   engine decides dispatch ONCE per line instead of re-deriving in
   e_node → e_skip_inline_space → scan_plain → e_parse_value).
   Target shapes: `key:`, `  <<: *a`, `  x: &a val`, `  y: *a`,
   `- {…}` — the anchor corpus and the flow block wrapper share
   this machinery. STRICT bail-to-the-existing-path on ANY
   deviation (the e_flow_json law). Gate: 253/253 ctest + the
   405-roundtrip + libyaml-diff + fuzz corpus; then dispatch
   bench.yml at the branch and read the ryml columns.

2. **The sink fast-path direct build (flow cells)** — yep_sink
   grows an OPTIONAL `int (*on_flow_json)(void* ctx, const char* p,
   size_t open, size_t close, yep_view anchor, yep_view tag,
   uint32_t anchor_id)`. e_flow_json calls it right after pass 1
   validates [open, close]; return 1 = subtree built (engine
   continues past the close), 0 = run pass 2 events exactly as
   today. The DOM sink (dom.c) implements it: one walk of the
   VALIDATED span creating/linking nodes via the DOM's own helpers
   (styles, implicit flags identical to the event path; anchor/tag
   bind at the root; the 1024-byte key limit and depth caps
   enforced identically). pull/push/recorder leave it NULL —
   streaming consumers are untouched BY DESIGN. GATE (hard):
   a tree-equality differential, event-built vs direct-built, over
   the conformance corpus + yaml-test-suite + fuzz inputs, added
   as a permanent ctest; any divergence reverts the unit.

3. **deep-nesting** — re-measure after 1+2; it likely rides the
   same wins. If still <1.0x, profile before touching anything.

RULES OF ENGAGEMENT (hard-won, all in the ledger):
- Measure before building; a unit whose gate fails reverts and gets
  ledgered (items 46/47 are the precedent — they are why the
  campaign is now surgical).
- The local dev box can saturate (load >100): NEVER trust local
  numbers; CI fresh runners with the ryml columns are the referee.
  Dispatch workflows at the branch if push events stall
  (workflow_dispatch is already enabled on test/asan/format/bench).
- If a dispatch fails a real check, fix it — the workaround runs
  the true gates.

RELEASE (rebase merges are enabled on all three repos — use
`gh pr merge --rebase`):
1. C repo: ensure CHANGELOG has `## [Unreleased]` entries, then
   `gh workflow run release.yml -f next_version=<X.Y.Z>`; approve
   any action_required runs on the release/v* branch (re-list to
   verify, the approve POST is silent); merge the release PR
   (--rebase).
2. Ruby: version-bump PR (lib/yeptris.rb) → CI → merge → tag
   v<X.Y.Z>.1 → `gh workflow run release.yml -f
   republish_version=<X.Y.Z>` (the smoke gates must print SMOKE
   PASS before every gem push).
3. Python: version-bump PR (pyproject.toml) → CI → merge → tag →
   release-wheels.yml runs; twine upload dist/* from the repo after
   the release attaches artifacts; verify `pip install yeptris`
   shows the native engine in a clean venv.
4. Update benchmarks/PERF-LEDGER.md with the new ryml table and
   close TODO.restructure/48 with the outcome.

CODE LAWS (standing, unchanged): C11 warning-clean, clang-format-18
(brew llvm@18 — the PATH clang-format is wrong); Ruby: no
send/instance_variable_*/respond_to?, autoload not require_relative;
MECE/DRY/OCP; specs for every behavior; explicit-path git adds,
never `git add -A`; no AI attribution; never push main or tags
except the binding-tag flow above; versions follow the lockstep
{c-semver}.{patch}.

DONE MEANS: every ryml-comparable shape > 1.0x on BOTH CI
platforms, all gates green, ledger updated, releases shipped.
