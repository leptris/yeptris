# yeptris — YAML at library speed

An ultra-performance YAML 1.2 parser/writer/streamer in pure C11 —
the YAML counterpart of
[libleptris](https://github.com/leptris/leptris). Zero required
runtime dependencies, a stable C ABI, opaque handles.

## Build, test, validate

```sh
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release   # LTO on by default
cmake --build build
ctest --test-dir build --output-on-failure
```

`scripts/validate.sh` is the pre-completion gate: clean
warnings-as-errors build → full tests → CLI smoke → leak check.
Sanitizer builds: `build-asan`, `build-ubsan`, `build-tsan`.

## The verification matrix

| corpus | pin | standing |
| --- | --- | --- |
| yaml-test-suite | `da267a5` | 395/395 pass |
| psych-pure `.tml` | ported | 67/67 |
| JSONTestSuite (YAML mode) | `1ef36fa` | 95/95 accept, 188/188 reject (strict) |
| libyaml emission goldens | `279` snapshots | 0 divergences (4 ledgered libyaml bugs waived) |
| libyaml parse differential | 405 inputs | full-record equality |
| fuzz corpora | 1091 inputs | record-stream equality across chunkings |

Corpora are fetched pinned by `scripts/fetch-corpora.sh`. Unit +
port + flow suites run beside them; TSAN enforces the one-document-
per-thread contract.

## Performance (same-binary, min-of-N vs libyaml)

| shape | ratio |
| --- | --- |
| scalar-heavy | up to 3.36x |
| block-nested | ~2.3x |
| wide mappings | ~2.4x |
| anchor-heavy | ~1.8x |

Every number is from the benchmark harness; every attempted lever —
shipped or dead — is recorded in `benchmarks/PERF-LEDGER.md`.

## The ecosystem

| repo | what |
| --- | --- |
| `yeptris-ruby` | Psych-compatible FFI gem (no C extension) |
| `yeptris-py` | PyYAML-compatible ctypes package |

Bindings version in lockstep (`{c-semver}.{binding-patch}`); C
releases tag from this repo's workflow, which also publishes the
gem through the RubyGems trusted publisher.

Architecture: `PLAN.md`. FFI contract: `docs/FFI.md`. ABI pins:
`docs/ABI.md`. Board: `TODO.md` (+ `TODO.impl/`, `TODO.restructure/`).
