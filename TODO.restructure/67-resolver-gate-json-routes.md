# 67 — the resolver first-byte gate + the last scan_stats routes

Status: LANDED (this wave; profile round 2 named both)

## Why

Profile round 2 (ubuntu, post-66): core12's reject chain cost 6.5%
of deep-nesting (every key pays it), and the parse_json / values
routes still ran the multi-class stats sweep the main path dropped.

## What

core12 gains a first-byte gate: a lead byte that can begin no core
word (not a digit, sign, dot, or the first letters of the
null/bool/inf/nan words — y/Y ride along for compat harmlessly) is
a string, one compare, full stop. The JSON routes ride
gate/heuristic sizing like the main path; the values engine skips
the nametab reserve entirely (strict JSON has no anchors).

## Measured (local h2h): flow-single 1.08x, flow-json 1.47x,
deep-nesting 1.01x, scalar-heavy 0.83x. Gates: 279/279, both
differentials 0, validate, format.
