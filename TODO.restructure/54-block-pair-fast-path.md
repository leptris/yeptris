# 54 — the block pair fast path: batch the classifier's line events

Status: LANDED (PR #173). block-pair-diff gate added (shares
tree_diff.h with the flow gate); the differential caught two bugs
pre-landing. CI h2h after: wide 0.74x mac (from 0.67), flow-single
0.80x mac (from 0.51); the margin to 1.0x remains — item 53 and the
linux profile are next.

## Why

anchor-heavy (0.57x/0.72x), block-heavy (0.61x/0.70x), wide-mapping
(0.67x/0.63x) and deep-nesting (0.69x/0.47x) are block shapes: after
the classifier removed the derivation cost, the remaining per-line
engine cost is the EVENT pair — e_event_init ×2, emit_now ×2 (dispatch
+ resolver), dom_on_event switch ×2, dom_new_node ×2 — for lines whose
complete shape scan already classified.

## Design (OCP mirror of the flow fast path)

yep_sink grows an OPTIONAL on_block_pair: the engine's KEY fast arm
offers the classified line (key span, value class + span/anchor, col
facts); return 1 = both nodes built and placed (the DOM resolves the
value scalar through the document schema — one resolver decision,
whichever path builds); 0 = events as today. The MAP_START/SEQ_START
events stay with the engine (frames are engine state). EMPTY values
(value-on-following-lines) keep events — no batch exists there.

## Gates

A block-pair differential sibling of flow-direct-diff (event-built vs
pair-built trees over the corpora) as a permanent ctest; full suite;
h2h referee.

## Acceptance

anchor-heavy and block-heavy clear 0.8x on both platforms, or the
profile names the next wall.
