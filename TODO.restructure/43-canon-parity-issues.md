# 43 — the canon parity issues (yeptris-ruby #29-32, #37)

Status: pending

## The issues (filed by lutaml/canon — its engine switch gates on
Psych-safe_load / JSON.parse parity)

- #30 sexagesimal: `1:30` must load as 5400, not 90. Psych's fold
  (scalar_scanner.rb) is WEIGHT-based — component e weighs
  `60 ** |e - 2|` — so a 2-component value is H:M (seconds
  implicitly zero): `1:30` -> 5400, `1:30.5` -> 5430.0,
  `190:20:30` -> 685230 (3-component already matches). The C folds
  in parse/numbers.c are positional (`v = v*60 + g`): the ONLY fix
  is x60 on the total (fraction included) when there is exactly one
  colon group.
- #31 big integers: beyond int64 the resolver leaves the scalar a
  string and every surface materializes String; Psych returns
  Integer. Fix at every materialization seam: the marshal emitter
  (text -> bignum limb stream, reusing e_int's 'l' encoding), the
  Ruby value walks, and Node#scalar_to_ruby — guarded by Psych's
  integer regexp (`[-+]?(?:0|[1-9][0-9_,]*)` — no leading zeros,
  `_`/`,` separators allowed).
- #37 duplicate JSON keys: json gem 3.0 made strict-raise the
  DEFAULT (JSON::ParserError, `duplicate key "a"`). The surface's
  contract is JSON.parse parity: strictness must follow the RESOLVED
  json version (>= 3 raises). Native: O(1) post-check per object
  (final size vs pair count) + an error-path rescan that recovers
  the exact key for message parity. Fallback walk: same post-check.
- #32 (ask: JSON drain): superseded by the native materializer
  (items 22-38) — Yeptris::JSON.load beats JSON.parse at the gate.
  Its sub-ask stands: a Psych::SyntaxError-compatible error class in
  the Yeptris::Psych shim (file/line/column/offset/problem/context
  readers, Psych's message format) so drop-in consumers' rescues
  keep working.
- #29 (empty/comment-only docs): already fixed on main and shipped
  in gem 0.1.14.1 (returns nil) — close with the evidence.

## Acceptance

- test_resolve.cpp pins the Psych sexagesimal truth table
  (2- and 3-component, int and float, signs).
- Marshal/valueml/node specs pin big-int rebuilds vs
  Psych.safe_load; the ported Psych suite stays green.
- JSON parity spec pins dup-key behavior against the RESOLVED json
  gem's own behavior (both engines).
- Psych-shim spec pins the SyntaxError-compatible surface.
