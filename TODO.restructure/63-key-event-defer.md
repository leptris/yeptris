# 63 — phase 1: the key-event defer; phase 2: compact nodes (open)

Status: phase 1 LANDED (this wave); phase 2 OPEN — the owner's
structural decision

## Phase 1 (landed)

The classified KEY arm built its key event unconditionally — 48
stores discarded on every line the pair path took (the dominant
case). kv now constructs only on emitting paths, from the pre-fold
captured line (the fold advances e->line; the differential caught
that as a line/col divergence before landing).

## Phase 2 (open, rewrite-class)

The residual ryml margin: two 64B node records + ~2 byte-walks per
`k: v` line vs ryml's leaner single-pass records. Compact nodes
(32-40B: pack style/implicit/tag_id/kind; sviews stay) behind the
opaque-handle seam, public ABI untouched. Needs a full session and
owner sanction; the one-walk line-scan fusion is measured-arithmetic
(saves only the end-find pass — the span walks share stop classes).
