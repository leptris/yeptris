# 83 — the schema-descriptor materialization API (issue #238)

## What landed (2026-09-13)

`src/include/yeptris/schema.h` — the descriptor ABI (24-byte plan
nodes, kinds SCALAR/SEQUENCE/MAPPING/CALLBACK, type tags, REQUIRED)
and `yeptris_schema_load`: one parse + one fused walk of the DOM
against the caller's plan into caller-allocated typed columns.
Whole-document results; spans borrow the input (zero-copy to the
host); capacity overflow reports MEMORY cleanly.

The four contract requirements from the committed consumer are met
and pinned by `test/unit/test_schema.cpp` (7 specs: typed columns,
shared child slices — the two-names-one-slot merge —, sequence
element plans, nesting, the CALLBACK escape hatch with span+position,
REQUIRED/SCHEMA error naming the node, ABI guard, unknown-key skip,
overflow, strict-JSON door). `docs/schema-abi.md` is the co-design
doc.

New public status `YEPTRIS_ERROR_SCHEMA` (+ internal YEP_ERR_SCHEMA
in the error X-list); new internal error carried by the existing
channel.

## Consumer notes (lutaml-model)

Map `KeyValue::Transform#build_kv_rule_plan` plans onto
`yeptris_desc_node` 1:1 (the fields match the issue's sketch). The
Ruby-side FFI wrapper is YOUR committed wiring — the C entry points
and the ABI doc are this side's deliverable. YEP_ST_ANY and
YEP_SF_FIRST_WINS are declared headroom, executed on first need.

## Follow-ups on the board

- Recorder-path projection behind the same ABI (parse fused with the
  walk — no DOM interpose) when a consumer measure asks for it.
- Stream variant (every document's root into the columns).
- The teptris sibling (same descriptor shape) — teptris's call.
