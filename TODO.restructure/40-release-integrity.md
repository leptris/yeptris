# 40 — release integrity: CI must judge the artifact, not the working tree

Status: complete

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

## Outcome (2026-09-08, binding PRs #47/#48 + C PR, gem 0.1.14.2)

COMPLETE — every leg landed:

- Artifact battery: binding `scripts/gem-smoke.rb` (version match,
  native-bundle activation when shipped, JSON canaries incl. the
  surrogate-pair combining law, core_12/compat typing, round-trip,
  node surface). `spec/gem_smoke_spec.rb` runs the same battery
  against the repo lib — the battery cannot rot.
- C release.yml smokes BEFORE RubyGems: the platform-gems job
  installs the built gem into an isolated GEM_HOME via
  `scripts/smoke-gem.sh` (battery from the LOCKSTEP TAG checkout);
  the publish job builds the C shared lib from the release ref and
  smokes the ruby gem with YEPTRIS_LIB_PATH. Missing battery on a
  pre-40 tag fails loudly.
- CI judges the artifact: the binding's spec + json-profile jobs
  check out the C core at the NEWEST RELEASE TAG (a non-blocking
  ubuntu row keeps C-main as evidence). Binding PRs needing newer
  C API wait for the C release — enforced ordering.
- Branch protection on BOTH repos (enforce_admins, required checks:
  the full C matrix incl. sanitizers + format; the binding's spec +
  GATE rows). Red merges are now structurally impossible.
- Released: binding tag v0.1.14.2 + gem (hand-pushed, the
  binding-patch precedent); verified end-to-end locally — clean-tag
  gem smokes green on the ffi engine exactly as the publish job
  will run it.
