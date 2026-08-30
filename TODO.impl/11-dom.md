# TODO.impl/11 — DOM: compact nodes, vtables, interning, O(1) access

Status: pending · Depends: 07, 10 · Layer: `src/yeptris/dom` · PLAN.md phase: 1

## Goal

The default consumption model: a dense, compact tree built by a sink over
the engine, with leptris's compactness discipline (int32 offsets, ~96 B
was rich XML; YAML nodes target ≤ 64 B) and pugixml-class access speed.

## Deliverables

- `dom/node.{h,c}` — `YepNode` header (kind, flags, parent, sibling links
  as compact int32) + kind-specific tails; all node kinds begin with the
  common header (safely castable — leptris invariant).
- `dom/kinds/` — one file per kind, registered in the **node vtable
  table** (`dom/node_vtable.c`): `document, mapping, sequence, scalar,
  alias`. OCP: a new kind = new file + one registration; per-kind
  behavior (name, child access, serialize) lives with the kind.
- `dom/builder.c` — the engine sink that builds trees (dense child arrays
  written forward — no per-node linked lists; O(1) indexed access from the
  first build, not a later index).
- `dom/intern.c` — mapping-key and anchor-name interning: doc-level
  open-addressing tables over the string arena (`yep_view_hash`), storing
  dense ids. Repeated keys across sibling maps hit the same id — cheaper
  comparisons, and the binding's Hash materialization gets cheap `eql?`.
- `dom/mapping.c` / `dom/sequence.c` — lookup by interned key (O(1)
  average), indexed child access, counts; `dom/scalar.c` — view + style +
  tag + lazy typed accessors (10).
- `dom/mutate.c` — the builder API for emitted-from-scratch trees
  (add/remove/set, duplicate-key policy = error per mapping uniqueness,
  unlike pugixml's tolerance) — feeds Psych's `dump` path in 15.
- `dom/query.c` — `yeptris_node_get(doc, "a/b/2/c")` style path lookup +
  alias following + `<<` merge resolution in compat mode (Psych load-time
  semantics: merged keys present, same-object alias identity preserved).
- `dom/document.c` — the owner: arena, tag table, intern tables, anchor
  registry, error slot, options snapshot; `yeptris_document_free` frees
  the world.

## Design decisions

- Aliases are nodes pointing at target node ids (the graph, not tree,
  case); cycle detection happened at parse (07); DOM mutation through an
  alias is an error (documented divergence-vs-none table vs libyaml which
  resolves at event level).
- Readonly/lazy: parse-time facts (styles, flags) ride along free;
  typed-value caching slots live in the node tail — 15's readonly mode
  exploits them for Ruby memoization.
- MECE: DOM never parses; the builder is a sink like any other (12) —
  DOM cannot know about pull/recorder and vice versa.

## Phases

A. Node kinds + builder + document ownership; node-size check in
  `validate.sh` (gate: ≤ 64 B for scalar/sequence, mapping ≤ 80 B).
B. Interning + O(1) access + mutation API; golden tree tests from
  yaml-test-suite `test.event` fixtures.
C. Path queries, alias/merge semantics; psych `test_merge_keys.rb`,
  `test_alias_and_anchor.rb` expectations ported at the C level.

## Acceptance

- Node-size gates pass; tree build adds < 25% to engine time on the
  corpus median (builder is a sink, measured separately in 18).
- 100k-node document: parse+build RSS < 3× input size (compactness gate).
- Merge/alias golden tests green (port of the psych suites at C level).

## References

- `~/src/leptris/leptris/src/leptris/dom/{compact.c,element_index.c,node_vtable.c}`
  (the compactness + vtable + index disciplines), `~/src/external/libfyaml/src/lib/fy-doc.c`
  (doc model), `~/src/external/psych/lib/psych/nodes.rb` (node vocabulary).
