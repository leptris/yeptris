# 21 — JSON materialization: the Marshal fast path

Status: complete

## Why

A user benchmark on a 151 KB JSON document (2026-09-07):

| path | time | vs `JSON.parse` |
| --- | --- | --- |
| `JSON.parse` (stdlib C ext) | 0.67 ms | 1x |
| `Yeptris::YAML.load(json)` — columnar drain | 19.5 ms | 29x slower |
| `parse_json + FFI DOM walk` | 111 ms | 165x slower |

Local reproduction (1400-item array, 152 KB, 29,403 values): 0.61 / 13.67 /
(111) ms. Split of the 13.67 ms: **C drain 1.51, Ruby column unpack 1.50,
Ruby walk 9.39** — 87% of the time is host-side materialization, and the
111 ms path is the known per-node FFI anti-pattern (`Node#to_ruby_walk`:
`node_id`/`kind`/`each_pair`/`tag_id` per node, ~4 FFI calls per value).

The pure-Ruby walk has a hard floor: ~29k Ruby allocations. A dedicated
C materializer (what `JSON.parse` is) allocates the same graph in 0.63 ms;
nothing pure-Ruby can follow it down. But `Marshal.load` is ALSO a core-C
materializer: measured 1.97 ms on the equivalent 131 KB graph. So the fix
is the recorder's trick one level up — **the C side emits Ruby Marshal 4.8
bytes directly from the value records; the binding materializes with one
`Marshal.load` call.** No C extension; the binding stays pure FFI.

## Plan

1. `src/yeptris/marshal.c` + `include/yeptris/marshal.h`: two-pass emitter
   (pass 1 exact sizing, pass 2 linear writes — the emitter discipline)
   over `YeptrisValue[]` + arena. Semantics mirror the binding walks
   exactly (`:sym` plain scan, single-char y/n string, dot-required
   float, bool/null/int/float payloads).
   - Objects/strings/arrays/hashes register in Marshal object-link order;
     aliases become `@` links (identity shared — matches the walks' memo
     semantics); alias-to-scalar re-emits the value (immediates).
   - Unsupported constructs return `YEPTRIS_ERROR_UNSUPPORTED` and the
     host falls back to the columnar walk: merge keys (`<<`), timestamps
     (Psych `Time`), forward alias references (defensive; YAML forbids).
2. DOM→records linearizer in `events/values.c` (the record format's
   SSOT home): one preorder walk of a DOM subtree producing the same
   records the engine path produces. Serves (a) `yeptris_marshal_node`
   (bulk `Node#to_ruby`/`Document#to_ruby` — kills the per-node walk) and
   (b) the strict-JSON route.
3. Strict-JSON sniff inside `drain_records` (first non-space byte `{`/`[`):
   `yep_dom_build_json` (the 479 MB/s scanner) + linearize; on any JSON
   grammar failure fall back to the engine. Columnar drains and the
   marshal input path all inherit it. Records stay byte-identical between
   routes (differential gate).
4. Ruby binding: attach `yeptris_marshal`/`yeptris_marshal_node`/
   `yeptris_marshal_free` (feature-detected), route `ValueML.load_all*`
   and `Node#to_ruby`/`Document#to_ruby` through it with fallback on
   UNSUPPORTED or missing symbols.

## Acceptance

- New C test `test_marshal`: byte goldens vs `Marshal.dump`, differential
  engine-route vs json-route over the JSON corpus, UNSUPPORTED statuses,
  first-doc mode, empty stream, alias identity ordering.
- Ruby spec: marshal path result == columnar walk result over the full
  spec corpus; fallback paths; encoding; multi-doc; alias `object_id`
  sharing. Existing 129+ specs green (they now ride the path implicitly).
- Perf gate (this machine, 152 KB / 29.4k values): `YAML.load(json)` <=
  3 ms (~4.5x better), `Document#to_ruby` on JSON <= 3 ms (37x+ better).
  `JSON.parse` remains ~4x ahead — that is the dedicated-C-ext floor; the
  ledger records the boundary. vs Psych on the same input: > 10x.
- Full gates: release + ASAN + UBSAN ctest, conformance, leaks; binding
  rspec; ledger entry; CHANGELOG `[Unreleased]`.

## Follow-ups (not this item)

- Python: the JSON sniff lifts `yeptris.load` on JSON for free; a pickle
  emitter for the Python walk is the analogous lever (protocol stable,
  but opcodes/memoization are a bigger lift) — measured need first.
- The float dot-quirk mirrors the walks, not Psych's exact regex
  (`1e+3` may diverge) — pre-existing binding behavior, unchanged here.
