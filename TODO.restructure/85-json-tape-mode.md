# 85 — the JSON tape mode (item 81 stage 3; the simdjson deficit)

## The deficit (the user's centerpiece number)

vs simdjson 4.6.1 DOM: **0.20x** local (parse_json ~203 MB/s vs ~1
GB/s), 0.18x on CI ubuntu. Conformance gates green (JSONTestSuite
95/95, json-suite-strict 283/283).

## Wrong path measured dead (2026-09-13)

`yeptris_value_drain_columns` on the json-doc corpus: **90.5 MB/s**
— SLOWER than parse_json's DOM route. The recorder path (record
store → transform → column carve) is not a tape; reusing it buys
nothing. A tape mode needs its own consumer of the fused walk.

## Design (the committed next build)

A tape consumer for the strict fused walk (81 stage 2's walk, strict
flag on): every `yep_json_walk_next` token appends ONE compact record
— no DOM nodes, no links, no views:

```c
/* tape record: 16 bytes, tag-parallel columns carved from one block */
typedef struct yeptris_tape_rec {
    uint32_t off;   /* token span start (spans borrow the input) */
    uint32_t len;
    uint64_t val;   /* int64/double bits (converted inline by the
                       number kernel) or container bookkeeping */
} yeptris_tape_rec;
/* kinds/tags: two u8 column arrays in the same block (the drain-
   columns carve pattern); containers carry child counts and the
   matching-close record index for O(1) skipping (simdjson shape) */
```

Public surface (additive, ABI-stable): `yeptris_parse_json_tape`
returns the tape + `yeptris_tape_free`; hosts iterate columns
directly (the drain-columns precedent for the carve/free contract).
The descriptor API (#238) gains a tape-execution mode later — same
columns contract, one less walk.

Expected: the DOM build (48 B/node + view encodes + places) is the
dominant share of parse_json's cost after the walk; eliminating it
puts the walk's scalar token machine as the floor — then item 79's
discipline (SIMD the structural step) applies. Realistic first-build
target: 350-500 MB/s (0.35-0.5x); simdjson parity needs the
structural-index pass, not just the tape.

## Gates

JSONTestSuite + json-suite-strict stay the conformance bar; a
tape-vs-DOM differential spec pins tape == DOM for every corpus
document (the same discipline as flow-direct-diff).

## Closed (2026-09-13)

Landed as `yeptris_parse_json_tape` (`src/include/yeptris/tape.h`,
`src/yeptris/tape.c`): parallel columns carved from one block
(kinds u8, offs/lens u32, vals u64); strings zero-copy, numbers
converted inline by the number kernel (int_min flags beyond-int64
integer text), OPEN/CLOSE records linked both directions. Route
shape mirrors parse_json: gate-clean container root fuses
validation into the walk; everything else validates through
yep_json_document first (error precedence byte-for-byte).

Measured (json-doc, interleaved referee, this machine): 301 MB/s vs
parse_json 207 (+45%), 0.30x vs simdjson (from 0.20x) — inside the
350-500 projection band. Gates: `tape-diff` (340 cases: 318 corpus
files + 22 pins; statuses and record streams) and 13 JsonTape unit
specs. Next lever recorded in the ledger: the structural-index
pass.
