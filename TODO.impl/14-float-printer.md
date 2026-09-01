# TODO.impl/14 — Float printer: shortest round-trip output

Status: COMPLETE (clean-room; v1 landed 2026-09-01: two-tier interval printer `emit/float/{print,dragon}.c` + api.h; REWRITE NOTE: the ryu vendoring was ABANDONED on licensing grounds — no third-party code embedded; the board's "port ryu" deliverables became "implement the published interval method ourselves"; 2M round-trips + shortest-oracle + printf-parity gates green; throughput 1.4-1.5x printf on realistic shapes, 0.09x on uniform-extremes — cached-power optimization queued in the perf ledger) · Depends: 13 · Layer: `src/yeptris/emit` · PLAN.md phase: 4

## Goal

IEEE-754 double/float → shortest decimal that round-trips, at ≥ 200M
numbers/s, with the ryu sources as the reference port (local clone).

## Deliverables

- `emit/float/{ryu_d2s,ryu_f2s,ryu_d2fixed}.{c,h}` + `emit/float/digit_table.h`
  — ports of `~/src/external/ryu/ryu/{d2s,f2s,d2fixed}.c` with their
  tables, kept as self-contained TUs with the upstream license headers
  preserved (Apache-2.0/Boost dual license is MIT-compatible; attribution
  comment block required — the licensing note lives in this file and
  `LICENSE` third-party section).
- `emit/float/api.h` — the emitter-facing API:
  `yeptris_d2s_shortest(double, char* buf)` (used by canonical/block
  emission), `…_fixed(precision)` for explicit-precision output, and the
  small-integer fast path (`|x| < 2^53` integral doubles print via the
  branchless u64 path — the overwhelmingly common YAML numeric case).
- Emitter integration: floats route through these functions only (SSOT —
  no `%.17g` fallbacks anywhere; `%g` appears nowhere in the tree).

## Design decisions

- Port, don't wrap: vendored ryu is C89-clean C; we keep the algorithm
  files pristine (upstreamable bugfixes stay applicable) and adapt at the
  `api.h` boundary only.
- `d2fixed` covers Psych's `Float#to_s` compat corner cases where Ruby
  prints non-shortest forms in legacy mode; compat mode chooses per the
  ScalarScanner oracle tests (10).
- Alternatives (Schubfach/Dragonbox) noted in the ledger as evaluated-
  and-deferred: ryu is local, dual-licensed, and within measurement
  noise of SOTA per the 2026 review (arXiv 2603.06581) — switching later
  is a TU swap behind `api.h`.

## Phases

A. Port d2s/f2s + upstream test vectors (ryu's exhaustive-ish sets).
B. d2fixed + small-int fast path + differential vs `printf("%.17g")`
  parse-back correctness (every printed value must parse to the same
  double via 08's parser — the roundtrip gate).
C. Emitter integration + throughput benchmark (ledger entry).

## Acceptance

- All ported ryu vectors green; roundtrip gate green on ≥ 10⁷ random +
  boundary doubles (0, denormals, ±inf, nan forms, powers of two ±1 ulp).
- ≥ 200M numbers/s shortest printing (x86_64 + arm64 CI matrix, ledger).

## References

- `~/src/external/ryu/ryu/` (sources + tests), arXiv 2603.06581 (SOTA
  review; the defer-rationale), psych `test_numeric.rb` (Ruby-side
  expectations for compat).
