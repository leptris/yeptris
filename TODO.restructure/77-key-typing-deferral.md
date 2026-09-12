# 77 — key-typing deferral (pending the binding audit)

## Problem

The resolver runs on every mapping KEY at parse (~2-4% of pair-heavy
shapes). ryml resolves nothing at parse. Keys' tag_id is consumed by:
(a) nothing in-tree — the emitter's style decisions read style/implicit
flags, map lookups compare decoded bytes; (b) possibly the BINDINGS —
Psych surfaces typed keys to Ruby (`123 =>` Integer keys under
safe_load semantics), and the yeptris bindings may mirror that by
reading key tag_id.

## Design

Phase 0 (the gate): audit yeptris-ruby and yeptris-py for key tag_id
consumption. If either binding reads it to build host keys, deferral
requires the binding to resolve at materialization (the resolver stays
the typing SSOT — it just runs at the seam) and the lockstep release
carries both sides.

Phase 1: keys parse with `tag_id = 0` + a "key, unresolved" bit (the
node's implicit/style bits already encode plainness); the marshal,
visit, and query seams resolve on first read THROUGH one accessor (no
mutation-on-read — the thread contract holds: compute, don't cache).

## Acceptance

- Binding audit recorded in this file (the decision, not just links).
- Differential gates: the flow/block builders produce identical
  RESOLVED trees (the gates read through the accessor).
- h2h: pair-heavy shapes up ~2-4%; ledger entry.
