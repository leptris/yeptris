# 53 — fused validate+build: one walk for JSON-class flow spans

Status: LANDED (PR #179). flow-json 1.11x mac / 1.15x ubuntu — WON
on both platforms (from 0.50x at campaign start). flow-single
0.68/0.86. The remaining shapes are block-path work: item 54's
second lever + item 55's linux profile.

## Why

flow-single (one giant flow line) sits at 0.51x/0.75x: every byte of
the span is walked twice — e_flow_json pass 1 validates, then the
direct build re-walks to place nodes. ryml walks once.

## Design

The grammar walk becomes a SSOT in scan/json.c: a token-stepping
walker (ws/string/number/literal/close, the kind/expect state
machine, the simple-key and depth caps) consumed by BOTH the engine's
validate-only pass and the DOM's build pass (OCP: one grammar, two
registrations). The DOM build is scratch-commit: nodes are created
past ncount, links form only among scratch nodes, the root placement
and anchor binding defer to commit, escape-arena writes roll back by
restoring str_len. ANY grammar deviation or engine bail condition
(key-colon after close, enforce-floor) aborts with zero live-state
mutation → the existing pass-2/event fallback runs unchanged.

The engine-side close checks (key-follows, flow_enforce, single-line)
move to scan facts shared by both consumers — grammar decisions stay
in the engine, byte facts stay in scan (MECE).

## Gates

The standing flow-direct-diff ctest (848 cases) pins tree equality
including the fallback paths; full suite + sanitizers; h2h referee
before/after on both platforms.

## Acceptance

flow-single ≥ 0.9x on ubuntu; measurable gain on flow-json toward
clear margin on both platforms.
