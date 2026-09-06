# 15 — The C repo's missing README + the conformance coverage map

## Why
The flagship repo has CHANGELOG/PLAN/TODO but NO README — no front
door, no quickstart, and the verification matrix exists nowhere in
one place (the corpora pins live in fetch-corpora.sh, the numbers
in CI logs).

## Plan
- README.md: what yeptris is, build/test/validate quickstart, the
  FULL verification matrix (every corpus with its pin and current
  numbers), the honest performance scoreboard, the ecosystem
  (bindings, releases, automation), links.

## Acceptance
- a newcomer builds and tests from README instructions alone
- every corpus in test/conformance/data appears with a number
