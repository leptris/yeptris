# 52 — close the campaign: ledger, item 48, releases

Status: pending

1. benchmarks/PERF-LEDGER.md: the post-49/50/51 CI ryml tables
   (both platforms), the honest deltas, dead ends if any.
2. Close TODO.restructure/48 with the outcome (every ryml-comparable
   shape vs 1.0x, both platforms); note the protected cells
   (block-heavy 1.48x, wide-mapping 1.13x, scalar-heavy 1.07x, the
   Ruby/Python JSON gates, YAML 1.8-5.4x vs libyaml/Psych/PyYAML).
3. Release wave (rebase merges, `gh pr merge --rebase`; the version
   numbers come from the user before any dispatch):
   - C: CHANGELOG `## [Unreleased]` entries, release.yml
     next_version, approve action_required on release/v*, merge.
   - Ruby: version-bump PR (lib/yeptris.rb) → CI → merge → tag
     v<X.Y.Z>.1 → release.yml republish_version (SMOKE PASS first).
   - Python: version-bump PR (pyproject.toml) → CI → merge → tag →
     release-wheels.yml; twine upload; pip-install verify in a clean
     venv.
4. Zero open issues anywhere; TODO.prompt.md retired.

DONE: every ryml-comparable shape > 1.0x on BOTH CI platforms, gates
green, ledger updated, releases shipped.
