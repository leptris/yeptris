# TODO.impl/11 — DOM: compact nodes, vtables, interning, O(1) access

Status: active (v1 shipped) · Depends: 07, 10 · Layer: `src/yeptris/dom` · PLAN.md phase: 1

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

## v1 shipped (2026-08-30)

- `dom.{h,c}`: sibling-linked nodes (first/last child + next_sibling,
  UINT32_MAX sentinels — dense-array interleaving bug avoided), builder
  sink over engine events, anchor binding incl. collection anchors,
  alias nodes with target ids, document roots.
- Public surface: document count/root/free (one free releases pool +
  handles — handles allocated from the document pool), node kind/value/
  style/tag/anchor/alias-target, seq count/at, map count/get (linear).
- Pinned enums (node kinds, scalar styles) in the ABI test.

## Remaining phases (next work)

1. ~~Key interning + O(1) map lookup~~ — LANDED 2026-09-02 as
   dom/mapindex.{h,c}: LAZY per-mapping open-addressing tables (view
   hash from the nametab SSOT -> key child id; full view compare on
   hash match), built on first lookup under a document mutex (the
   read-only sharing contract holds), freed-not-pooled so mutation
   invalidation is leak-free (map_add/set/del drop the table). First
   key wins (the scan's contract); collection keys never indexed.
   20k-key map: 217 ns/lookup incl. handle alloc. Divergence note:
   the PARSER accepts duplicate keys (libyaml/Psych parity — Psych
   materializes last-wins, map_get is first-wins; both documented).
2. ~~Node-size gate~~ — LANDED 2026-09-02: node strings are
   region-tagged (offset,len) pairs (input region zero-copy; the
   DOM's own contiguous realloc'd string arena for copies — offsets
   survive the move). yep_dnode **60 B**, `_Static_assert` gate in
   dom.h. Peak heap/input 25-54x -> 6-54x (ledger); <3x needs
   finish-free folding + pre-sizing (06). Engine finish pool now
   released at parse end (nothing in the tree references it).
3. ~~Mutation + builder API~~ — phase 3 core shipped 2026-09-02 (see
   below); per-kind vtables deferred until polymorphic behavior appears.
4. ~~Merge-key resolution~~ — resolved at MATERIALIZATION (the Ruby
   Materializer: existing keys win, sequences merge in order —
   spec'd in spec/psych/loading_spec.rb); yeptris has no C-side
   to_ruby, so there is no second merge implementation to keep
   true (MECE by construction).
5. ~~Node-handle identity~~ — yeptris_node_id (15A); bindings key
   wrapper caches on it.

## Phase 3 shipped (2026-09-02)

- `dom/mutate.c`: from-scratch construction with parse-identical
  semantics — runtime values copied into the DOM pool, plain scalars
  typed by the core12 resolver (typing SSOT), links form only through
  `dom_link`. Invariants enforced at attach: one parent per node,
  balanced map pairs, duplicate `map_add` = error (strict) while
  `map_set` replaces in place (json-c position semantics), depth
  capped at YEP_DOM_MAX_DEPTH (the writer recurses — the cap is
  load-bearing), cross-document links rejected.
- `build.c` public surface: `yeptris_document_new/set_root`,
  `node_new_{mapping,sequence,scalar}`, `map_add/set/del`,
  `seq_add/del`. Statuses map to ERROR_ARG/MEMORY/DEPTH/PARSE.
- Depth truth: builder links form bottom-up, so `yep_mut_set_depths`
  runs per root at STREAM_END (one O(n) walk) — synthesized and parsed
  trees share the same global-depth invariant.
- json-c building API (21 v2) rides on it: new_object family,
  object_add (replace-on-duplicate, NULL-deletes), array_add/del_idx;
  pending wrappers materialize lazily (O(1) attach, no copies);
  compact JSON output (`json_compact` writer flag — json-c
  to_json_string spacing). 13 tests (Mutate + JsonCBuild), 152/152.
