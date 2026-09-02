# TODO.impl/21 — JSON API compatibility layers + best-API synthesis

Status: CORE LANDED 2026-09-02 — writer JSON flavor (JSON escapes incl \uXXXX, null words, single root; ASAN caught a named-table OOB the unit tests missed), yeptris_serialize_json; jsonc_compat read path (real json-c symbols, -lyeptris-jsonc, enum values pinned; building API awaits DOM mutation, 11p3); json.hpp nlohmann-flavored C++17 (RAII, move-only, exceptions at the boundary, view children). REMAINING: json_object_new_* building (needs 11p3), to_json_string PRETTY variants, yajl_compat gen API, json-c test-suite port breadth. 2026-09-02: direct-from-index DOM landed (yep_dom_build_json: JSON entry 63 -> 136 MB/s, 2.2x — see PERF-LEDGER) · Depends: 08, 11, 13, 18 · Layer: `src/yeptris/jsonapi` + `include/yeptris/json*.h` · PLAN.md phase: 6

## Goal

YAML 1.2 ⊃ JSON, so the flow kernel makes yeptris a JSON engine. This item
turns that into migration paths: API-compatible layers over the same
core so users of json-c / nlohmann / simdjson / yajl can switch by
changing a link flag — and a synthesized "best-of" API that becomes
yeptris's own recommended surface.

## The API survey (what each reference teaches)

| Library | API shape | What we take | What we reject |
| --- | --- | --- | --- |
| **json-c** (`json_object*`, `json_tokener_parse`, `json_object_to_json_string`) | Opaque C handles, per-node malloc, refcounting (`json_object_put/get`), type queries (`json_object_is_type`) | The COMPAT TARGET: names + semantics are the de-facto C JSON API; a drop-in layer wins its whole user base | Per-node malloc + refcount discipline (we use document-arena; the compat layer shims refcounts onto `yeptris_document_free`) |
| **nlohmann-json** (`json j = {...}`, `j["k"]`, `dump()`, RAII, exceptions) | Value semantics, implicit conversions, the ergonomics gold standard | The C++17 header `yeptris/json.hpp`: RAII, `operator[]`, `at`, `value`, `is_*`, `dump`, `parse`, range-for — over our C ABI (the leptris "thin handles" discipline) | The deep-template internals; value-semantics copies (our wrapper stays a view) |
| **simdjson** (On-Demand `parser`, `element`, error codes, `padded_string`) | Laziness, raw error codes over exceptions, parser reuse, tape design | Error-code discipline for C++ (`yeptris::result<T>`); On-Demand-style access on top of 12's pull API | Padding requirement; we parse in place with length-guarded kernels |
| **yajl** (SAX callbacks, `yajl_gen_*`) | Push/streaming-first, generator API | The gen API shape for our emitter (13) streaming face; incremental feed validation model | Hand-rolled allocators plumbed through every call (we pass the allocator once at setup) |

## Deliverables

1. **`yeptris/jsonc_compat.h` + `libyeptris-jsonc`** — the drop-in json-c
   migration layer (the user-requested priority):
   - Type aliases: `json_object` → `YeptrisNode`-backed handle;
     `json_type` enum values identical to json-c's.
   - Parsing: `json_object* json_tokener_parse(const char*)` (flow-mode
     entry from 08); `json_tokener_parse_ex`, `json_tokener_free`.
   - Queries: `json_object_get_type/is_type`, `json_object_get_string/
     int/double/boolean`, `json_object_object_get_ex`,
     `json_object_array_length/get_idx`.
   - Building: `json_object_new_*`, `json_object_object_add`,
     `json_object_array_add` (over the DOM mutation API, 11 phase 3).
   - Output: `json_object_to_json_string(_ext)` (13's emitter in
     JSON-flow mode — strict JSON, not YAML).
   - Refcount shims: `json_object_put/get` map to document/handle
     lifetime; one root object owns the document.
   - Symbols are the REAL json-c names, in a separate static/shared
     library — users link `-lyeptris-jsonc` instead of `-ljson-c` and
     change nothing else. A CMake option builds it (`YEPTRIS_BUILD_JSONC_COMPAT`).
2. **`yeptris/json.hpp`** — the nlohmann-flavored C++17 header: `yeptris::
   json` RAII wrapper, `parse/dump/operator[]/at/value/is_*/push_back`,
   iterator ranges, `yeptris::parse_error`; zero-copy views into the
   input by default, `.str()` for owned strings.
3. **Best-API synthesis (yeptris's own C surface)** — the lessons above
   folded into the core:
   - error-code returns + `yeptris_last_error` (already shipped) — no
     exceptions in C, no out-params for messages;
   - typed accessors stay direct (`yeptris_node_int/double/bool`, 10)
     and lazy — On-Demand discipline;
   - bulk iteration (`yeptris_node_children_into`) — the leptris
     FFI-batch lesson;
   - one allocator choice at setup, never per call.
4. **`yeptris/yajl_compat.h`** (post-MVP): SAX callbacks + gen API over
   the recorder (12) and streaming emitter (13) for yajl migrants.
5. **Tests**: port json-c's `tests/test*.c` and a slice of nlohmann's
   unit suite against the compat layers (they are the "imported test
   suites" for the JSON side); simdjson's jsoncheck/fuzz corpora as flow
   conformance (into 16's manifest).
6. **Benchmarks (with 18)**: json parse/emit vs **json-c, nlohmann-json,
   simdjson** on the same corpora; publish the matrix. Beating json-c
   and nlohmann is the near-certain outcome (per-node malloc vs arena);
   simdjson parity/exceeding is the stretch goal tracked in the ledger.

## Phases

A. Flow-mode strict-JSON entry (with 08's fast paths: fast_float,
   structural scan) + json-c compat layer for parsing/queries.
B. json-c building/output path (needs 11 mutation + 13 emitter).
C. C++ header; tests ported from json-c and nlohmann suites.
D. yajl compat; benchmark matrix + publication.

## Acceptance

- json-c's ported test programs compile and pass against
  `libyeptris-jsonc` unmodified (beyond the link flag).
- Benchmark artifact shows yeptris ≥ json-c and ≥ nlohmann on every
  JSON operation; simdjson delta reported honestly in the ledger.
- The C++ header compiles warning-clean at -Wall -Wextra and passes the
  ported nlohmann slice.

## References

- `~/src/external/json-c/{json_object.h,json_tokener.h,tests/}` — the
  compat contract and its test suite.
- `~/src/external/nlohmann-json/{include,tests/}` — ergonomics + suite.
- `~/src/external/simdjson/` — On-Demand + error-code discipline,
  jsoncheck corpora.
- `~/src/external/yajl/{src/api,}` — SAX/gen shapes.
