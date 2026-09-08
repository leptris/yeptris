# 41 — Python: the native JSON loader (the json.loads beat)

Status: complete

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

## Outcome (2026-09-08, yeptris-py PRs #22/#23, PyPI 0.1.14.1)

COMPLETE — every acceptance leg, with two measured corrections
en route:

- THE GATE NUMBERS (CI, fresh runners, order-alternating referee,
  gated at 1.00): ubuntu 0.785x mean 198/200 head-to-head, macos
  0.699x mean 181/200 — from item 30's ~15x BEHIND. Local runs:
  0.67-0.87x mean, 83-85% h2h. Shape decomposition: int 0.63x,
  float 0.77x, long strings 0.85x, repeated keys 0.85x, nested
  0.94x, unique-key maps 0.95x. Item 30's 1.5x aspiration was set
  against a RUBY reference; json.loads is a C scanner — beating it
  21-30% mean with 90-99% h2h is the honest result.
- MEASURED CORRECTION 1 (the intern tax): the first cut interned
  cached keys — PyUnicode_InternInPlace registers EVERY key in the
  interned dict; unique-key corpora measured 2.07x (pure loss when
  keys never repeat). The cache's identical objects already give
  dict probes the pointer-equality fast path. Interning dropped:
  every shape under 1.0.
- MEASURED CORRECTION 2 (parity over purity): stdlib json.loads
  ACCEPTS NaN/Infinity by default. Literal arms in the native walk;
  the fallback delegates exactly that extension (gate-reject +
  stdlib-accept is precisely the constant case).
- 142 parity tests vs the stdlib oracle (both engines); CI builds
  the extension on 3.9+3.13 both OSes, pins the C core to the
  newest release tag (item 40's policy), and gates both platforms.
  GOTCHA: py3.13 runners do not bundle setuptools — install it
  before setup.py.
- NEXT LEVER (ledgered, not built): compiling the kernel TUs into
  the extension removes ~20k cross-DSO calls/parse (~5-7% est.) —
  blocked on distutils lacking per-source compile flags for the
  ISA-dispatch TUs (scan/json.c depends on simd_text). Would also
  make the JSON path self-contained (no libyeptris at runtime).

Pure-ctypes contract intact: no env → pure package; platform wheels
that vendor the lib are [[42-python-platform-wheels]].
