# The schema-descriptor ABI (issue #238, TODO.restructure/83)

Co-design doc for the committed consumer (lutaml-model). The header
`src/include/yeptris/schema.h` is the normative reference; this page
records the CONTRACT behind it and the decisions the consumer asked
for explicitly.

## The descriptor is the ABI, not a schema language

XSD/JSON-Schema cannot express consumer semantics — two-names-one-slot
merges, value maps, key renames, raw capture. The CONSUMER compiles
those into the flat positional plan:

```c
typedef struct yeptris_desc_node {        /* 24 bytes, pinned */
    const char* wire_name;   /* mapping key match (NULL = root)     */
    uint8_t  kind;           /* SCALAR | SEQUENCE | MAPPING | CALLBACK */
    uint8_t  type_tag;       /* STR | INT | FLOAT | BOOL | NULL      */
    uint16_t flags;          /* REQUIRED | FIRST_WINS               */
    uint32_t child_index;    /* SEQUENCE: the element plan;
                                MAPPING: first child                */
    uint32_t child_count;    /* MAPPING: children from child_index   */
    uint32_t reserved;       /* 0 (ABI headroom)                    */
} yeptris_desc_node;
```

**Two-names-one-slot** is a shared child slice: two MAPPING nodes may
point at the SAME [child_index, child_count) range — every match from
either name lands in the same columns, in document order. (Pinned by
`SchemaLoad.TypedColumnsMatchTheDocument`.)

## The four contract requirements

1. **Escape hatch (kind CALLBACK)** — the raw value span (off/len
   into the input, zero-copy) plus the node's byte offset come back
   as records; Ruby finishes just those fields. Scalars carry their
   content span; collections carry position only (the caller that
   needs the tree takes the generic surface). serde-derive's
   AOT-with-escape-hatch shape.
2. **Whole-document results, never per value across FFI** — the
   caller supplies output buffers (`yeptris_schema_column`:
   caller-allocated typed array + capacity) and receives per-node
   counts after ONE call. Capacity overflow reports MEMORY with the
   walk uncorrupted (re-call with bigger buffers).
3. **Versioned, lockstep** — `YEPTRIS_DESC_ABI` (1) gates every
   call; a mismatch is YEPTRIS_ERROR_ARG with a recompile hint. The
   descriptor ABI changes only with a C minor bump; the bindings ride
   `{c-semver}.{binding-patch}`.
4. **TOML sibling** — teptris adopts the same `yeptris_desc_node`
   shape so a framework compiles descriptors once per model and runs
   them on every engine. The type tags are engine-neutral.

## Semantics pinned by the spec (test/unit/test_schema.cpp)

- SCALAR columns: INT via the fused number kernel (`yep_json_number_scan`,
  exact int64, INT64_MIN included; float text converts), FLOAT, BOOL
  (t/T/y/Y/o/1 leads — compat words), STR as input-borrowing spans,
  NULL count-only. A scalar that is not the plan's type is
  YEPTRIS_ERROR_PARSE naming the node.
- SEQUENCE runs its single element plan for every element.
- MAPPING children match by wire_name; unknown keys are SKIPPED (the
  generic surface still has them — capture needs an explicit CALLBACK
  plan).
- REQUIRED: zero matches at a matched mapping is YEPTRIS_ERROR_SCHEMA
  naming the node index.
- YAML and strict JSON enter the same door (schema selects the
  resolver as everywhere).

## Not in v1 (recorded, by design)

- `YEP_ST_ANY` and `YEP_SF_FIRST_WINS` are DECLARED (ABI headroom)
  but not yet executed — the walk implements them when a consumer
  needs them; the flags are versioned so adding execution is not an
  ABI change.
- Streams: v1 materializes the FIRST document (the `YAML.load`
  contract); a stream variant can come as an entry-point addition.
- The recorder-path projection (parse fused with the walk, no DOM
  interpose): the columns are the contract, not the walk — when the
  recorder learns descriptor projection it drops in behind this ABI.
