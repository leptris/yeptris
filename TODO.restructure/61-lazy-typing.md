# 61 — lazy typing: tag_id at the access seams

Status: pending (the wave-4 headliner; spec-first)

## Why

ryml does NOT type scalars at parse (typing defers to access). We
resolve EVERY scalar during the build — the one structural gap left
on scalar-heavy (0.51-0.81x) and a per-token tax on every flow
shape.

## Design

tag_id becomes computed at the ACCESS seams (marshal, emit, the
value visitors) — the resolver stays the typing SSOT, it just runs
there. Constraints that shape it: (a) read-only document sharing
across threads is a contract — no mutation-on-read caching; the
accessors re-resolve or the document carries a lazily-built,
atomically-published type table (preferred: one atomic swap, no
per-node writes); (b) the differential gates compare tag_id on
trees from BOTH paths — they must read through the same accessor,
else the gates stop verifying typing; (c) emit's style decisions
consume tag_id — one resolve per node at emit, which parse no
longer paid (net win only because parse sees every scalar and emit
does too — measure! if both run, lazy typing LOSES; it wins for
parse-only consumers and the DOM API).

Spec-first: typing vectors already exist (test_resolve.cpp); the
gates pin the access-side equality. If the measure shows both
sides pay, close this item as measured-dead and keep the number
hook (59's landed sibling below).
