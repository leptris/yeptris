# 22 — Beat JSON.parse: fused visit API + Ruby native materializer

Status: complete

## Why

User directive (2026-09-07): beat `JSON.parse` so yeptris is faster
than it. Measured floor on the 152 KB / 29.4k-value JSON corpus:

| path | time |
| --- | --- |
| `JSON.parse` | 0.51–0.65 ms |
| pure-Ruby rebuild of the same graph | 2.87 ms |
| C `parse_json` (DOM only, no Ruby objects) | 0.77 ms |
| Marshal path end-to-end | 5.81 ms |

Pure-Ruby allocation alone is 5× `JSON.parse`. Our JSON DOM build
alone already exceeds `JSON.parse`'s full parse+materialize. The
Marshal path cannot win: `Marshal.load` alone is 3.9 ms.

The only path that can win is what `JSON.parse` itself does: **one
C pass that both scans and allocates Ruby objects via the Ruby C
API**. The FFI-only contract cannot express that; the extension is
the materializer, libyeptris stays language-agnostic.

## Plan

1. **Public visit API** in libyeptris (`yeptris/visit.h`):
   language-agnostic vtable sink over scalars/containers. Two
   entry points: `yeptris_visit_json` (fused RFC 8259 scan→visit,
   no DOM, no records) and `yeptris_visit` (YAML engine→visit).
   MECE: scan kernels stay in `scan/json.c`; the visitor is a new
   consumer (OCP), not a core edit.

2. **Ruby C extension** `ext/yeptris_native/`:
   implements the vtable with `rb_hash_new` / `rb_ary_push` /
   `rb_str_new_len` / `rb_int2inum` / `rb_float_new` / Qtrue/Qfalse/Qnil.
   One Ruby→C call per load. Feature-detected; FFI ladder remains
   the fallback (Marshal → columns → records).

3. **Wire** `Yeptris::YAML.load` / `load_stream` / `Document#to_ruby`
   through `Yeptris::Native` when the extension loaded.

4. **Perf gate**: `Yeptris::YAML.load(json) < JSON.parse(json)` on
   the 152 KB corpus (min-of-20). YAML-shaped loads must not regress
   vs the Marshal path.

5. Platform: `extconf.rb` finds libyeptris via `YEPTRIS_LIB_PATH` /
   pkg-config / sibling build. Dev builds against local dylib;
   release platform gems vendor the `.bundle` alongside the dylib.

## Acceptance

- `Yeptris::YAML.load(json)` min-of-20 **strictly faster** than
  `JSON.parse` on the 152 KB corpus.
- Equal results (`== JSON.parse(json)`).
- Extension absent → silent fallback to Marshal path (173 specs green).
- ctest + ASAN + UBSAN green; extension specs cover JSON + YAML +
  anchors + fallback.
- CHANGELOG + ledger entry with the before/after numbers.
