# YAML 1.2.2 grammar reference (decision citations)

Source: `yaml-1.2.2.md` in this directory (converted from
<https://yaml.org/spec/1.2.2/>, sha256 of the original HTML recorded
below). This file cites the productions behind yeptris's contract
decisions so code and board items reference section numbers, not
memory.

## YAML ⊃ JSON (the two-surface rationale)

> "Its primary focus was making YAML a strict superset of JSON."

(§1.2 history, of the 2009 YAML 1.2 spec.) YAML accepts grammars JSON
rejects — and TYPES differently on shared ones under different
schemas. `Yeptris::YAML` carries the YAML/Psych contract;
`Yeptris::JSON` the RFC 8259 one.

## Trailing commas in flow collections — LEGAL in YAML

```
[138] ns-s-flow-seq-entries(n,c) ::=
 ns-flow-seq-entry(n,c)
 s-separate(n,c)?
 ( c-collect-entry s-separate(n,c)? ns-s-flow-seq-entries(n,c)? )?
[141] ns-s-flow-map-entries(n,c) ::=  (same tail shape)
```

The tail after `c-collect-entry` is OPTIONAL — Example 7.15 literally
shows `{ one : two , three: four , }`. RFC 8259 rejects the same
bytes. Hence: `Yeptris::YAML.load('{"a": [1,]}')` parses;
`Yeptris::JSON.load` raises. Both spec-pinned
(`spec/json_parity_spec.rb`).

## Scalar typing — the 1e3 correction (a lesson in citing, not remembering)

The §10.3.2 core-schema float regexp is verbatim:

```
[-+]? ( \. [0-9]+ | [0-9]+ ( \. [0-9]* )? ) ( [eE] [-+]? [0-9]+ )?
```

The dot is OPTIONAL (`[0-9]+ ( \. [0-9]* )?`): **`1e3` resolves to a
core-schema float** — an earlier draft of this note claimed otherwise
from memory; the spec text above is the correction (the very
anti-pattern this file exists to prevent).

So the surfaces type `1e3` differently for TWO distinct reasons:
- `Yeptris::JSON.load` → `1000.0` (RFC 8259 §6).
- `Yeptris::YAML.load` → `"1e3"` — a Psych **1.1-compat** typing
  (the binding's default schema), NOT a core-schema fact.
- Under `schema: :core_12`, spec-faithful typing makes `1e3` a
  Float — the binding's dot-required quirk currently applies to
  both schemas; the core_12 audit is TODO.restructure/32.

## Anchor on an empty scalar binds null

```
[105] e-scalar ::= ""   (the empty scalar is a node)
[101] c-ns-anchor-property decorates the node that follows
```

`a: &anchor\nb: *anchor` — the anchored node is the empty scalar
(§8.1: "an empty node"). The alias resolves to null, NOT to the next
sibling (libyaml regression `6KGN`; fixed in the walk and the Marshal
emitter).

## Anchor rebinding

§8.1: "an anchor name MAY occur at most once per document" — MAY,
not MUST; libyaml accepts rebinding and the LAST binding wins for
later aliases (snapshot `3GZX`). yeptris matches libyaml (the compat
target): newest-binding-first lookup in the emitter.

## JSON compatibility claims used by the C kernels

RFC 8259 (`rfc8259.txt`, sha256 below): §6 numbers
(`-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][-+]?[0-9]+)?`), §7 strings
(escape table, surrogate PAIR requirement), §2 value/ws grammar.
`scan/json.c` is the one home of that grammar (MECE).

---

- yaml-1.2.2.html sha256: `aa671a8cb790578ff40b9b2213cb5f574b3b576139df61fe34f4a75dd7c371cf`
- rfc8259.txt sha256: `61a5378f4255c720beb2a4b4a63b29540147c140f36988bf086291989b4cd2d7`
