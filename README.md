# yeptris — YAML at library speed

An ultra-performance YAML 1.2 parser/writer/streamer in pure C11 —
the YAML counterpart of
[libleptris](https://github.com/leptris/leptris). Zero required
runtime dependencies, a stable C ABI, opaque handles.

The bar we hold: **the best YAML/JSON parser/builder ever built — on
every axis at once, in C, Ruby, Rust, and JavaScript.** Conformance
(the suite bar, every divergence documented), performance (every
claim carries harness numbers), safety (nightly fuzzing across every
harness; every catch fixed and pinned), APIs (the right idiom per
language), transport correctness (pinned FFI/ABI contracts, lockstep
release trains, byte-stable roundtrips), and trust (published
corpora and reproducible releases). The achievement program lives in
[`PLAN.md`](PLAN.md) ("The best-ever program"); the bindings live in
their own repos — see "The ecosystem" below.

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
| `yeptris-rs` | Rust binding (crates.io; links `libyeptris`) |
| `yeptris-ts` | TypeScript/WASM binding (npm; emcc build of the engine) |

**A release is not a release until every runtime ships.** Every C
release publishes ALL FOUR runtimes at the lockstep version — ruby
(`{c-semver}.0` via the RubyGems trusted-publisher train), py
(`{c-semver}.0` via the PyPI trusted publisher on the tag push), rs
(the `yeptris` crate on crates.io via the release-plz trusted-
publishing pipeline), and ts (`yeptris-wasm` on npm via the emcc
build). A cut that lands the C tag alone is incomplete.

Version shapes follow each registry's law: rubygems and PyPI take the
four-component `{c-semver}.{binding-patch}` (`0.6.50.0`); crates.io
and npm enforce strict three-component semver, so rs and ts carry the
EXACT C semver (`0.6.50`) — the maximum lockstep their registries
accept. A binding-only fix on ruby/py bumps the fourth component
(`0.6.50.1`); on rs/ts it rides the next engine patch release.

Architecture: `PLAN.md`. FFI contract: `docs/FFI.md`. ABI pins:
`docs/ABI.md`. Board: `TODO.md` (+ `TODO.impl/`, `TODO.restructure/`).
