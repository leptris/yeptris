# 06 — Ruby: find_anchored types its own Node (no respond_to? on our types)

## Why
visitors.rb's `node.respond_to?(:anchor)` probes OUR OWN class — a
known type. The law: is_a? for type checks. (The encode_with /
init_with probes stay: Psych's own public duck-typed protocol.)

## Plan
- `node.is_a?(Yeptris::Node) && node.anchor == name`.

## Acceptance
- grep: respond_to? only on the three documented protocol sites
- 143/143 incl. the anchor round-trip specs

## Status: COMPLETE (gem 0.1.9.1) — and better than planned
The base Node gained `anchor -> nil` (model-driven: every node HAS
an anchor concept), so find_anchored needs NO check at all. The
handle-writer work also surfaced and fixed FIVE
instance_variable_set(:@handle) sites in the Nodes builder (an
attr_writer now). The remaining protocol sites are the DOCUMENTED
EXCEPTIONS mirroring Psych itself: encode_with/init_with probes,
and visitors.rb ivar restoration ON USER OBJECTS (that IS Psych's
init_with materialization mechanism).
