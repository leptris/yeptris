# 32 — core_12 typing must follow the spec table, not the Psych quirk

Status: complete

## Why

The formal spec (docs/spec/yaml-1.2.2.md, §10.3.2) resolves
`1e3`-shaped plain scalars to FLOAT — the core float regexp's dot is
optional:

    [-+]? ( \. [0-9]+ | [0-9]+ ( \. [0-9]* )? ) ( [eE] [-+]? [0-9]+ )?

The binding's record walks apply the Psych dot-required quirk
("no dot/colon/leading-dot ⇒ String") under BOTH schemas, so
`YAML.load('a: 1e3', schema: :core_12)` returns a String where the
spec says Float. The quirk is CORRECT for compat_11 (Psych parity)
and WRONG for core_12.

## Plan

1. The record carries `tag_id` (the C resolver's core12 verdict is
   already spec-faithful: it tags `1e3` FLOAT). The walks must apply
   the dot-quirk ONLY when the requested schema was compat_11.
2. Carry the schema decision ONCE: the drains already take the
   schema; the records do not carry it. Either (a) quirk inside the
   walk conditioned on a schema flag threaded from load_all_* (the
   walk gains a `compat:` parameter), or (b) the C transform bakes
   the quirk into the record kind under compat_11 (kind stays FLOAT
   but a flag bit marks quirk-eligible). Prefer (a): the quirk is a
   HOST policy (each binding's Psych/PyYAML parity), not a C
   grammar fact.
3. Mirror in: ValueML.walk, walk_columns, the Marshal emitter's
   `float_text_ok`, Node#scalar_to_ruby (check its schema source),
   and the Psych-compat surfaces (compat by construction there).
4. Spec: core_12 cases pinned against the §10.3.2 TABLE verbatim
   (int/float/bool/null/inf/nan examples from Example 10.9/10.10);
   compat_11 cases unchanged.

## Acceptance

- `core_12` typing matches §10.3.2's table exactly (spec-pinned).
- `compat_11` behavior byte-identical to today (Psych parity suite
  green).
- docs/spec/yaml-grammar-citations.md updated with the outcome.

## Outcome (2026-09-08)

Landed as designed — plan option (a): the quirk is HOST policy,
schema-conditioned at every surface in lockstep:
- both record walks (ValueML.walk / walk_columns, `compat` param),
- the Marshal emitter (C: E.compat from the marshal schema param and
  the document's schema for node marshaling),
- Node#scalar_to_ruby (Document#parse_schema).
The schema became a DOCUMENT PROPERTY (Document#parse_schema /
wrap_schema; C yeptris_document.schema set by both parse paths) —
host policies ask the model.

spec/core12_typing_spec.rb pins spec 10.3.2 / Example 10.9 VERBATIM
(0.->0.0, .5->0.5, +12e03->12000.0, -2E+05->-200000.0, 1e3->1000.0,
0o7->7, 0x3A->58) plus the compat spot-table; the ported Psych suite
stays green (compat byte-identical). The CI referee gate tightened
1.05 -> 1.00: losing to JSON.parse now fails CI outright.
