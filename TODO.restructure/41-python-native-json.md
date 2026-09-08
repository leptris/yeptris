# 41 — Python: the native JSON loader (the json.loads beat)

Status: pending

## Measurement that motivates (152 KB / 29.4k values, item 30)

```
json.loads     min 0.91  med 0.93  mean 1.02 ms
yeptris.load   min 14.75 med 15.65 mean 16.21 ms   (ctypes columnar)
```

~15x behind. The ctypes walk is intrinsically 5-10x slower per
record than C-API calls; no walk tuning closes it. Ruby closed the
identical gap with a fused C-API materializer (json_ruby.c, items
22/37/38).

## Design correction vs item 30

Item 30 scoped "a CPython extension over yeptris_visit_json". The
PERF-LEDGER killed that shape in Ruby: the VTABLE visit measured 2.2x
SLOWER than hand-driving the exported scan kernels. The proven shape
(json_ruby.c) is a fused RFC 8259 -> PyObject descent over
`scan/json.h` kernels — port it function-for-function:

- `yep_json_string` for spans (escape decode via
  `yep_finish_double_into` into a scratch buffer) ->
  `PyUnicode_DecodeUTF8`
- `yep_json_number_scan` (the fused grammar+conversion kernel):
  shape 0 -> `PyLong_FromLongLong`, 1 -> `PyFloat_FromDouble`,
  2 (beyond int64) -> `PyLong_FromString` on the validated span
  (json.loads returns exact ints)
- 1024-slot 8-byte-prefix-hash KEY CACHE (item 38's jr_hash) holding
  INTERNED PyUnicode — Python caches str hashes on the object and
  dict lookups pointer-compare interned keys first; one hash ever
  per unique key, the same win the cache delivers in Ruby
- depth guard 1000 (Ruby's YEP_JR_MAX); error raises the SAME
  exception type the ctypes fallback raises (parity)
- NO gc/ins/shape knobs on first landing: Python dicts have no
  capa/bulk variants and refcounting replaces the GC-page mechanism;
  the cache stays ON with an env override (YEPTRIS_NATIVE_CACHE)
  so the CI referee can A/B without a rebuild

## Scope

1. `yeptris-py/ext/yeptris_native.c` + env-driven setup.py build
   (YEPTRIS_LIB_PATH/YEPTRIS_SRC, mirroring extconf.rb) that SKIPS
   gracefully when the lib is absent — the pure-ctypes contract is
   the package's design pillar and stays the default install.
2. Feature-detect import in the JSON surface (native first, ctypes
   fallback) — the YAML surface keeps the record/columnar ladder.
3. Parity battery vs json.loads mirroring the Ruby 56-case spec
   (values, rejects, types, both engines agree).
4. `benchmark/json_profile.py` (order-alternating interleave, the
   fair-benchmark lesson) + a gated CI job: the DEFAULT build must
   beat json.loads (GATE=1.00) on both platforms, evidence rows
   ungated.

## Acceptance

- min-of-N parity or better with json.loads on min; DECISIVE mean
  margin (item 30's target: >= 1.5x, i.e. mean ratio <= 0.67x).
- The gated CI rows are the referee; numbers recorded in the perf
  ledger.
- The pure-ctypes install path is untouched (no extension -> same
  behavior, suite green both paths).

Platform WHEELS that vendor libyeptris (so `pip install` users get
the native loader with zero build) are [[42-python-platform-wheels]]
— a separate wave with its own distribution work.
