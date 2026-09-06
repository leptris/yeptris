# 10 — C: the break stop set gets ONE home

## Why
engine.c's quoted-scalar path builds the \n\r stop bitmap per quote
(clear + 2 adds) — AND duplicates scan.c's k_break_set bit-for-bit.
Two homes for one truth: a MECE violation and the last per-call
stop-set build in the engine.

## Plan
- scan.c's set becomes `yep_break_set` (extern; declared beside the
  plain-stop constants in scan.h — scan owns the line-break concept)
- scan_line and the quote path both use it; the local build dies

## Acceptance
- grep: no yep_stopset_clear/add in parse/engine.c
- the constant pinned against a runtime build (same as the plain stops)
- full gates; quoted-heavy shapes no worse on the bench
