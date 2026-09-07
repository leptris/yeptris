# 24 — Visit API SSOT + JSON fused path in C

Status: complete

## Why

Item 22's native materializer needs a language-agnostic sink. The
value-record path allocates an intermediate buffer; the DOM path
allocates nodes. Both lose to `JSON.parse` before Ruby objects exist.
A fused JSON visitor that never builds a tree is the C-side half of
beating `JSON.parse`, and the same vtable serves Python/Rust later
(OCP).

## Plan

1. `src/include/yeptris/visit.h` — vtable + `yeptris_visit_json` +
   `yeptris_visit` + `yeptris_visit_node` (DOM subtree → visit,
   for `Node#to_ruby` without records).
2. `src/yeptris/visit/json_visit.c` — fused RFC 8259 scan using
   existing `yep_json_*` kernels; no DOM, no arena beyond decode
   scratch for escaped strings.
3. `src/yeptris/visit/yaml_visit.c` — engine sink that fires the
   same vtable (YAML path for the native materializer).
4. `src/yeptris/visit/dom_visit.c` — preorder DOM walk → vtable
   (node materialization without records/Marshal).
5. Unit tests: visit event streams match value-drain kinds for the
   JSON corpus and a YAML slice; OOM/depth/parse errors surface.

## Acceptance

- Public header installed; symbols exported from shared lib.
- ctest covers the visitor; ASAN clean on the libyaml snapshot
  corpus via a visit-only fuzz probe.
- No behavior change to existing drains/DOM/marshal.
