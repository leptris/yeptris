# yeptris ABI policy (TODO.impl/20)

## Pinned by test (`test/unit/test_abi.cpp`)

- **Handles are opaque and pointer-sized**: `YeptrisDocument`,
  `YeptrisNode`, `YeptrisParser`, `YeptrisPullParser`,
  `YeptrisRecorder`, `YeptrisIterparse`, `YeptrisEmitter`. No struct
  fields in public headers — bindings hard-code nothing but the
  pointer.
- **Enum values are ABI**: `YeptrisStatus`, node kinds, scalar styles,
  tag ids, event types. Changing a value requires a major bump and a
  binding rebuild; adding new values at the end is compatible.
- **Status codes are 0..8, contiguous, 0 = OK.**

## Option structs are versioned by size

- `YeptrisParseOptions`, `yeptris_emit_options`: callers initialize
  with `sizeof(struct)`; the library treats a smaller-than-expected
  `size` as "older caller" and reads only the fields that existed
  then. Never reorder; only append.

## Result structs consumed across FFI

- `yeptris_json_tape` (`tape.h`, TODO.restructure/85): bindings read
  the column pointers and `count` directly after ONE
  `yeptris_parse_json_tape` call — the drain-columns shape, not a
  per-value call. The struct layout (pointer fields, `int_min`
  flag) is ABI like an option struct: never reorder, only append.
  `_block` is the library's; hosts free via `yeptris_tape_free`
  only.
- `yeptris_schema_column` / `yeptris_desc_node` (`schema.h`): gated
  by `YEPTRIS_DESC_ABI` (see docs/schema-abi.md).

## Shared library versioning (when `YEPTRIS_BUILD_SHARED` ships)

- ELF soname `libyeptris.so.MAJOR`; dylib compatibility version
  `MAJOR.0.0`. A change to any pinned value bumps MAJOR.

## Version SSOT

- `project(VERSION)` in the root `CMakeLists.txt`;
  the release workflow syncs `vcpkg.json`, the port manifest, and
  `CHANGELOG.md` from it.
