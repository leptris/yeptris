# 40 — release integrity: CI must judge the artifact, not the working tree

Status: pending

## Why

The 0.1.13.5 train shipped a broken core_12 surface (see
[[32-core12-spec-typing]] postmortem): the binding's spec suite went
green against a working-tree C build, the PR merged with RED CI, and
the platform gems vendored a C tag that lacked the fix. Three gaps
stacked:

1. **Red merges.** Binding PR #44 merged with 4 failing CI runs on
   the strength of a local run. The gate must be CI, not a local
   machine.
2. **CI-of-source vs artifact-of-tag.** The binding's spec job
   checks out C **main** (no `ref:`), while the platform gems build
   C at the **tag**. Green-on-main says nothing about the tag the
   gem vendors.
3. **No gem smoke.** Nothing installs the just-built platform gem
   and runs the suite against its vendored lib before RubyGems
   push. (The bug was caught only by a manual post-release smoke.)

## Plan

- [ ] Binding release workflow: after `gem build`, add a job that
      installs the built .gem into a scratch GEM_HOME, loads it,
      and runs `bundle exec rspec` with `YEPTRIS_LIB_PATH` pointing
      at the gem's vendored libyeptris — BEFORE `gem push`. GATE
      semantics: a failing smoke blocks the push.
- [ ] Same for the ruby-platform gem path (install, compile against
      the release's native asset, run the suite).
- [ ] Decide the C checkout `ref:` policy for the spec job: pin to
      the newest C tag at run time (matches what users of the
      platform gems get), with main as an explicit opt-in row.
- [ ] Branch protection on both repos: require the CI checks green
      before merge, so red merges are structurally impossible.

## Acceptance

- A deliberately broken binding tag cannot reach RubyGems (the
  smoke fails and blocks the push).
- Merging a PR with failing checks is refused by GitHub, not by
  discipline.
