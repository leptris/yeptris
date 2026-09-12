# 78 — the dash-item batch (on_block_item)

## Problem

block-heavy's `tags:\n    - alpha\n    - beta` entries rode the EVENT
path: e_classified's DASH arm opened the sequence frame, then each
`- plain value` item emitted a scalar event through emit_now →
yep_dom_on_event → dom_new_node — while every `key: value` line had
the on_block_pair batch (54). ~400k item events per block-heavy
parse.

## Design

The pair contract, mirrored for sequences: `on_block_item(ctx,
yep_block_value*)` — the engine's DASH/PLAIN arm prepares the value
event exactly as before (fold handling included) and offers it whole;
return 1 = built+placed (the engine skips the event), 0 = events as
before, <0 = abort. The DOM side is one node through the node-init
law placed into the sequence the engine just opened. Designated sink
initializers make the new slot zero for every other sink (OCP:
registration, no core edits — the events/pull/push/recorder sinks
never see it).

Pinned by block-pair-diff (extended): dash-item corpora run
event-only vs item-batched — trees must agree node-for-node; the
must-fire list now includes five dash shapes (nested seqs, comments,
sibling continuation).

## Closure (2026-09-12)

Landed with the gate green (30 cases, 49 fast-path lines). Local
measure: block-heavy 123-125 MB/s — within ambient noise of the
event path (the per-item event chain was already a direct call, not
a dispatch); anchor unaffected. The batch removes ~400k event structs
per block-heavy parse and completes the fast-path surface (pair /
open / item); the CI bench is the arbiter for the 0.1.30 record.
