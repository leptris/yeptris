# TODO.impl/10 — Schema resolvers: 1.2 core, 1.1 compat, per-parse options

Status: COMPLETE (A+B+C: interface, tag table, core12, compat11, parse options; strict/tab_policy/recover stay reserved pins) · Depends: 09 · Layer: `src/yeptris/resolve` · PLAN.md phase: 1

## Goal

Typing is pluggable and outside the grammar. A resolver decides, for a
plain scalar view, whether it carries an implicit tag. Psych parity and
YAML 1.2 core correctness are two resolvers over one interface.

## Deliverables

- `resolve/resolver.h` — the interface:

```c
typedef struct yeptris_resolver {
    yeptris_tag_id (*resolve)(void* ctx, YepView raw, YepScalarInfo info);
    void* ctx;
} yeptris_resolver;
```

  OCP: a new schema = a new resolver instance; the engine calls the
  configured resolver at event time and stores a tag id — no schema
  branches anywhere else.
- `resolve/tags.{h,c}` — the per-document **tag table**: dense id ↔
  string mapping with preallocated core ids (`str, int, float, bool, null,
  timestamp, seq, map, binary, merge`) and dynamic ids for `!`/`!!`/`%TAG`
  handles — the SSOT of tag identity for DOM, emitter, and bindings.
- `resolve/core12.c` — YAML 1.2 core schema: null (`null/Null/NULL/~`),
  bool (`true/false` + case variants), int (dec `[-+]?[0-9]+`, `0o` octal,
  `0x` hex), float (incl. `.inf/.Inf/.INF/.nan` forms with sign), else str.
- `resolve/compat11.c` — libyaml/Psych implicit semantics: `y/Y/yes/on/…`
  booleans, `~`, leading-`0` octal, `0x`, sexagesimal (`1:30`), the
  timestamp productions, `=` value-tag. Behavioral SSOT: psych's
  `scalar_scanner.rb` is the oracle; every divergence is a compat bug
  (ported tests in 15 enforce it).
- `parse/options.{h,c}` — `YepParseOptions { schema, max_depth, strict,
  tab_policy, recover }` applied per parse (the leptris
  TODO.bindings/05 scoped-options pattern); thread-global defaults remain
  for the compat header.

## Design decisions

- Resolution is lazy-able: raw views + style survive regardless; typed
  conversion (int/float/timestamp values) happens only on accessor
  demand (`yeptris_scalar_int` etc.) using 08's number kernel — parse
  speed never pays for typing the caller never reads.
- `<<` merge keys: the resolver marks `merge`; resolution of merged
  mappings happens at the DOM query layer (11) in compat mode — matching
  Psych, where merge is a load-time concern, not a grammar one.
- The C fast path (core12) + the Ruby-side ScalarScanner (15) share this
  one semantic definition: compat11.c's table is mirrored by test vectors
  generated from the same table, consumed by the binding's specs — one
  truth, two implementations, one test source.

## Phases

A. Interface + tag table + core12 (vector tests from the 1.2 spec tables).
B. compat11 with tests generated from psych's ScalarScanner behavior
  (golden vectors committed as fixtures).
C. Options struct wiring through all entry points + compat global setters.

## Acceptance

- core12: 100% on the 1.2 core-schema test vectors; compat11: parity on
  all generated ScalarScanner vectors (divergence list = zero or
  explicitly accepted, documented in VALIDATION.md).
- Resolution adds < 2% to parse time on scalar-heavy corpus (measured).

## References

- `~/src/external/psych/lib/psych/scalar_scanner.rb` (1.1 oracle),
  `~/src/external/psych-pure/lib/psych/pure.rb` (1.2 reading of the same),
  YAML 1.2 core schema spec, `~/src/external/libfyaml` tag resolution.
