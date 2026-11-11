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

**Latest release: v0.6.62 — live across all five runtimes** (C +
ruby `0.6.62.2` + py `0.6.62.0` + rs/ts `0.6.62`, registry-checked
2026-11-11). This cut flips the #1257 fold default: block plain and
single-quoted scalars fold at 80 columns exactly the way libyaml and
Psych write stored files — `line_width: -1` (Psych's unwrap) and an
explicit `block_fold: 0` keep everything on one line.

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
| fuzz corpora | 543 tracked inputs | record-stream equality across chunkings |

Corpora are fetched pinned by `scripts/fetch-corpora.sh`. Unit +
port + flow suites run beside them; TSAN enforces the one-document-
per-thread contract.

The fuzz nightly is the safety campaign: 240+ rounds so far, every
catch shipped the round it was confirmed — the CHANGELOG carries
rounds r116–r244: 120 fuzz artifacts, seventeen of them genuine
engine fixes (the rest comparator-side carries where libyaml
diverges from the suite — the suite-pinned law wins, the carry
documents the exact shape) — 132 fuzz-diff pins and six roundtrip
seed pins and counting, every pin green.

## Performance (the referee numbers — interleaved h2h medians, CI fresh runners)

DOM parse vs the references (the 2026-11-09 round; ratios are
machine-relative — the bands below are the honest platform caveat,
and the two runners disagree in both directions: scalar-heavy leads
on macOS, the nested/flow shapes lead on ubuntu):

| reference | standing |
| --- | --- |
| libyaml | DOM 5.8x–11.6x per shape; emit 4.7x–21.5x (ubuntu) |
| rapidyaml | ubuntu 1.8x–8.5x on all nine shapes; macOS 1.5x–4.0x on the rest; anchor-heavy is the one parity cell (0.98x mac / 1.8x ubuntu) |
| simdjson (JSON routes, json-doc) | strict tape 0.95x mac / 1.13x ubuntu; lenient 0.82x mac / 1.02x ubuntu |

Named state, from the ledger:

- deep-nesting is back in its class — 2.24x local A/B (DOM
  ~1071 MB/s M1 Max) after the chained same-column pendings
  re-entered the fused walk (the 0.6.33 correctness bail was wider
  than the bug); ubuntu referee reads 2.6x–3.0x.
- flow-single rides 2.9x–3.5x vs ryml on ubuntu across the recent
  rounds (the flow-rooted fast path + the fused lenient walk under
  the recorder's classification).
- anchor-heavy is the last shape at ryml parity, and the wall is
  NOT the anchor table (the 2026-11-09 re-derivation exonerated
  it — L2-resident on Apple silicon): the deficit lives in the
  shared per-line lanes (line_facts 11%, block-pair 11%, shape 8%,
  classified 7% — max-perf items 02/03/09).
- The JSON routes' residual vs simdjson is per-ISA codegen: on
  fair rounds it is single-digit percent on x86, and the named
  lever is the NEON arm codegen question (the in-place front vs
  ported kernels) plus item 06's codegen shape arms — not another
  walk-level arm (three dispatch-level attacks measured dead).

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
release publishes ALL FOUR binding runtimes at the lockstep version
— five runtimes live in total — ruby
(`{c-semver}.0` via the RubyGems trusted-publisher train), py
(`{c-semver}.0` via the PyPI trusted publisher on the tag push), rs
(the `yeptris` crate on crates.io via the release-plz trusted-
publishing pipeline), and ts (`yeptris` on npm via the emcc build).
A cut that lands the C tag alone is incomplete.

Version shapes follow each registry's law: rubygems and PyPI take the
four-component `{c-semver}.{binding-patch}` (`0.6.62.0`); crates.io
and npm enforce strict three-component semver, so rs and ts carry the
EXACT C semver (`0.6.62`) — the maximum lockstep their registries
accept. A binding-only fix on ruby/py bumps the fourth component
(`0.6.62.1`); on rs/ts it rides the next engine patch release.

Architecture: `PLAN.md`. FFI contract: `docs/FFI.md`. ABI pins:
`docs/ABI.md`. Board: `TODO.md` (+ `TODO.impl/`, `TODO.restructure/`).
