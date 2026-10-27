# 30 — Python: the JSON beat (scoping item)

Status: complete

## Measurement (152 KB / 29.4k values)

```
json.loads     min 0.91  med 0.93  mean 1.02 ms
yeptris.load   min 14.75 med 15.65 mean 16.21 ms
```

The FFI columnar walk is ~15× slower than the C stdlib. The Ruby win
came from a fused C-API materializer; Python has the same target —
the ctypes walk is intrinsically 5–10× slower than the Ruby ext's
C-API calls for the same work.

## Decision

Implement a CPython extension over `yeptris_visit_json` (mirror of
the Ruby native materializer — `ext/yeptris_native`). Scope:

1. `yeptris-py/ext/yeptris_native.c` — fused RFC 8259 → PyObject
   descent, `PyDict_SetItem` per pair OR the bulk variant, frozen
   interned string cache for keys, GC pause via `PyObject_GC_New`
   batching (Python's allocator behavior differs — measure).
2. `yeptris-native` registration in `yeptris.py` with
   LoadError-guarded fallback.
3. The win target: parity with json.loads on min, decisive mean
   margin (≥ 1.5×) from the same fused-scan + GC-pause levers as Ruby.

This is its own wave — the implementation is not in scope for the
26–29 batch.

## Acceptance

- Recorded numbers above in the perf ledger.
- The CPython extension lands as a separate wave with its own
  gates and PR.

