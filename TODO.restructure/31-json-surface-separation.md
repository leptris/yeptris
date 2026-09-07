# 31 — JSON surface separation + parity spec + comparison profile

Status: complete

## Why

A downstream user reports yeptris JSON ~2× slower than JSON.parse in
steady-state on a loaded machine — the opposite of our interleaved
measurements. Independent of whose benchmark is right, the current
DEFAULT is indefensible on semantics alone: `YAML.load`'s strict-JSON
sniff routes JSON-shaped input to RFC 8259 semantics, but `YAML.load`
carries the Psych contract — `{"a": [1,]}` (legal flow YAML) and
`"1e3"` (Psych String) must not change meaning because the input
happens to look like JSON.

## Plan

1. `YAML.load` reverts to the FFI ladder for ALL inputs (Psych
   semantics, unchanged, safe default).
2. New `Yeptris::JSON` module — the explicit strict-JSON surface:
   `Yeptris::JSON.load` targets EXACT JSON.parse semantics (that is
   its parity target). Native extension when loaded; otherwise the
   record drain with a strict conversion walk (floats always Float,
   no Psych quirk table).
3. Parity spec: `JSON.parse` vs `Yeptris::JSON.load` over a value
   corpus, error cases, big ints, exponent-only floats, duplicate
   keys, encodings.
4. Committed comparison profile (`benchmark/json_profile.rb`): the
   interleaved A/B methodology — min/median/mean, pair ratios,
   head-to-head — so any machine's "steady state" claim is
   reproducible. The flip of any default waits on this profile plus
   the parity spec, not on one side's benchmark.

## Acceptance

- `YAML.load` behaves Psych-identically on JSON-shaped inputs
  (trailing-comma flow, "1e3" as String) — spec-pinned.
- `Yeptris::JSON.load == JSON.parse` on the parity corpus.
- Profile committed; README documents both surfaces and the rule:
  defaults follow proof, not benchmarks-of-the-day.
