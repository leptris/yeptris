# 26 — Fused JSON number kernel: one scan, validate + convert

Status: complete

## Why

The native materializer's `jr_num` calls `yep_json_number` (which
walks the digits to validate) and then re-walks the same digits to
convert them — every number is scanned twice, and the second walk is
a GRAMMAR COPY living in the extension (a DRY violation by the letter
of the law: the grammar has one home, `scan/json.c`). ~4.2k numbers
on the reference corpus.

## Plan

1. `scan/json.c` gains `yep_json_number_scan(p, len, &i, &is_float,
   &iv, &dv)` — THE grammar walk, fused with conversion: digits
   accumulate into `int64` with overflow promotion to `double`
   (strtod on the validated span); any '.'/'e' makes it a float.
   `yep_json_number` becomes a thin wrapper (validator callers
   unchanged — OCP on the existing surface).
2. Export it; the extension's `jr_num` becomes one call, the copy
   deleted.
3. Unit tests: the full number table (int/float/overflow/exponent
   forms, grammar rejects) through both the wrapper and the fused
   entry — verdicts identical.

## Acceptance

- ctest green (new number-kernel tests), fuzz corpus clean.
- Extension rspec 194/194; the JSON.parse mean gate still wins.
- No double scan remains (`grep` shows one grammar walk).
