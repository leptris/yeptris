# 73 — pair batch + alias-name borrow (the DOM build path)

## Problem

v0.1.27 flat profiles, four losing shapes: `dom_open_node` 8-10% and
`dom_place`/`dom_on_block_pair` another ~3-5% — every `key: value` line
pays two separate grow-checks, two memsets, two sentinel fills, two
parent-link rounds. And BOTH paths (event + block-pair) COPY the alias
name into the DOM string arena (`borrowed=0`): anchor-heavy pays 266k
`str_put` copies for names that are views into the same input every
plain scalar already borrows from.

## Design

1. **Alias names borrow** — `e_alias` stamps `ev->borrowed = 1` (the
   name is an input view exactly like a plain scalar value);
   `dom_on_block_pair`'s alias arm passes `borrowed=1` to match. The
   event path already threads `ev->borrowed` through `dom_str_in`, so
   one engine flag aligns both (the differential gates compare decoded
   views — unaffected).
2. **Pair placement batch** (dom.c owns node law): `dom_on_block_pair`
   and `dom_on_block_open` reserve both nodes with ONE grow, initialize
   them adjacently, and link both to the parent with one parent deref —
   `dom_place` stays for the general event path (OCP: the SSOT of
   linking; the batch is a specialization inside the same module, not
   a second law).
3. `dom_open_node` keeps its shape (the single-node law); the batch
   helper inlines it twice — no signature churn for other builders.

## Acceptance

- Full ctest + flow-direct-diff + block-pair-diff green (borrowedness
  is not compared, but node counts and decoded strings are).
- Mutation/marshal/visit specs green (alias views read through
  `yep_dom_view` everywhere).
- h2h: anchor-heavy and block-heavy up measurably; ledger entry with
  the str-arena churn delta (18B table).

## Closure (2026-09-12)

Landed:
1. **Alias names borrow** — `e_alias` stamps `ev->borrowed = 1` (the
   name is an input view exactly like a plain scalar value) and the
   block-pair alias arm passes borrowed=1 to match. anchor-heavy
   stopped paying 266k `str_put` copies per parse.
2. **Template node init** — `dom_open_node`'s memset + four sentinel
   patches became one 48 B template copy (the sview zero-init rides
   the template).
3. The pair-placement micro-batch was measured-skipped: `dom_place`
   is two predictable branches + one deref; fusing saves single-digit
   cycles per pair against three-digit totals — not worth a second
   linking path in the module (MECE).

Measured under heavy ambient throttle (ratios only, machine-relative
absolutes collapsed 4-5x for ALL parsers incl. libyaml): anchor-heavy
0.62 → 0.57-0.81 across runs, block 0.69 → 0.93-1.00. CI is the
arbiter of record for 0.1.28.
