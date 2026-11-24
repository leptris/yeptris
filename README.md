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

**Latest release: v0.6.79 — live across all five runtimes** (C +
ruby `0.6.79.0` + py `0.6.79.0` + rs/ts `0.6.79`, registry-checked
2026-11-24). v0.6.79 carries the r374-r377 comparator roll (all
zero-`src/`): the `\uXXXX` escape join over a 21-row measured decode
matrix (every well-formed escape face renders byte-identical on both
engines; surrogates and malformed hex stay BOTH-ERROR); the
tag-property lead as classifier law l3 and the `+` byte joining the
quoted-exotic-tab key charset via the drill's own bounded soak (the
soak carried the next nightly red before the nightly fired); the
r331 tab-position law's deferred cells landing (22 probed rows); and
the glued collection key, the lead pre-run, and the clean-eat SEQ
cell joining the soup laws (the 73KB exotic-break giant rides the
composed law — no size-bound pin). Zero YAML engine divergence
standing beyond #1411's deliberate fix, #1468 (0.6.75), #1584
(0.6.77) and #1615 (0.6.78). **The bindings shipped their sibling
faces this wave**: the plan/tape/events (pull + `next_batch` +
iterparse) faces landed on the Python binding (#192, ctypes,
capability-gated on the released engine tag) and the TypeScript/WASM
binding (#48, `compilePlan`/`plan.walk` + the JSON tape face), and
the Ruby binding adopted the org native-layer contract (#409 — the
DOT-FREE per-minor bundles, the candidate-order SSOT, the
no-unversioned-fallback law) alongside its pull/iterparse and tape
faces (#411); all five runtimes now expose the plan/schema/pull/tape
family. The previous cut: v0.6.78 — the feed-contract fold-line fix
(#1615): the fold-line state machine rode `\n` only, a CR-terminated
fold line swallowed the next line's flow openers, and the
`norm_next_fold_break` SSOT restored whole-vs-chunked identity
byte-for-byte. The cut before it: v0.6.77 — the roundtrip
guarantee's empty-inline anchor twin (#1584): the emitter spells
`k: &n: ''` whenever the anchor name's last byte is `:`, byte-stable
round-trips, over the r351-r359 roll (eight pinned escapes).
The cut before that: v0.6.76 — the character-column emitter (#1490,
PR #1493): the fold column rides code points with the purity fact as
a parse product, all emitter lanes at-or-above the pre-change
levels, over the r314-r350 comparator roll (~37 pinned nightly
escapes).
The cut before that closes the six long-horizon competition fronts
with measured verdicts (#474/#739/#742/#331/#352 + the binding faces
#862/#857, every attempt ledgered): anchor-heavy — the last DOM
shape that trailed — rides the per-line memo to 1.34x vs rapidyaml;
the emitter's DQ cells +21.6%/+17.7% on parse-time style facts; the
Psych-compat ruby facade beats stdlib Psych on every lane (5.6x
clean safe_load / 1.7x gated; the node-free dump face 1.71x at
-55.7% allocations); the JSON write leg +23% (referee) / +84-88%
(campaign); CBOR SAX decode reads 1.29-1.41x past tinycbor; and the
#474 arm64 verdict is recorded structural — a flat walk with no hot
target, NEON engaged on both CI legs. The fuzz side shipped the
classifier era: the #1310 law-verifying classifier replaced member
enumeration and the r272 mining harvest closed the decline surface
(48 pins + the [?::] model extension) — zero engine defects since
r248 (#1281).

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
| fuzz corpora | 689 tracked inputs | record-stream equality across chunkings |

Corpora are fetched pinned by `scripts/fetch-corpora.sh`. Unit +
port + flow suites run beside them; TSAN enforces the one-document-
per-thread contract.

The fuzz nightly is the safety campaign: 360+ rounds so far, every
catch drilled the same day — genuine engine fixes are rare and loudly
reported (the latest: r358's roundtrip-guarantee repair, #1584,
released in v0.6.77; before it #1468's CBOR strtab use-after-free in
v0.6.75 and #1411 in v0.6.71) — the rest comparator-side
carries where libyaml diverges from the suite (the suite-pinned law
wins, the carry documents the exact shape; since the r250–r254
treadmill fix the comparator leg fuzzes its own corpus snapshot).
The classifier era: the soup family proved measured-unbounded
(five consecutive nightly carries r263–r267), so PR #1346's
law-verifying classifier replaced member enumeration — it derives
both engines' expected event dumps and requires them byte-exact,
one unplaced byte staying DIFFER — and the r272 mining harvest
(#1355) closed the decline surface: 72 shapes dispositioned as
48 EXACT-BYTES pins + 10 classifier-claimed composites (the [?::]
model extension), the rest documented-skipped soup density — 235
fuzzing-gated pins and six roundtrip seed pins and counting, every
pin green.

## Performance (the referee numbers — interleaved h2h medians, CI fresh runners)

DOM parse vs the references (the 2026-11-09 round; ratios are
machine-relative — the bands below are the honest platform caveat,
and the two runners disagree in both directions: scalar-heavy leads
on macOS, the nested/flow shapes lead on ubuntu):

| reference | standing |
| --- | --- |
| libyaml | DOM 5.8x–11.6x per shape; emit 4.7x–21.5x (ubuntu) |
| rapidyaml | ubuntu 1.8x–8.5x on all nine shapes; macOS 1.5x–4.0x on the rest; anchor-heavy — the one parity cell at that round (0.98x mac / 1.8x ubuntu) — has since moved to 1.34x (the per-line memo, PR #1329) |
| simdjson (JSON routes, json-doc) | strict tape 0.95x mac / 1.13x ubuntu; lenient 0.82x mac / 1.02x ubuntu |

Named state, from the ledger — the six long-horizon competition
fronts are CLOSED with measured verdicts (#474/#739/#742/#331/#352
+ the binding faces #862/#857; every attempt ledgered):

- anchor-heavy is past its wall — **1.34x vs rapidyaml** after the
  per-line memo fused the lane's three caches behind one key (PR
  #1329, max-perf item 02; the 2026-11-09 re-derivation had
  exonerated the anchor table — L2-resident on Apple silicon — and
  named exactly these shared per-line lanes).
- the emitter's DQ cells ride parse-time style facts (PR #1328):
  **+21.6% / +17.7%** — `plain_ok`/`json_clean` set at the
  `dom_set_value` SSOT, byte-identical outputs.
- the binding faces: the Psych-compat ruby facade beats stdlib
  Psych on every lane — safe_load **5.6x** clean / **1.7x** gated
  (yeptris-ruby#386) — and the dump face rides the bulk build
  node-free: **1.71x** per dump at **-55.7%** allocations
  (yeptris-ruby#385).
- the JSON write leg lands skip_col + the json_clean fact + the
  fused pair walk (PR #1331): write **+23%** (referee) / **+84-88%**
  (campaign), generation +19%; the parse leg verified at its
  ledgered floor (95.8% one fused symbol).
- the CBOR decode lane pays only for the bytes it reads (PR
  #1330): SAX decode reads **1.29-1.41x past tinycbor** on every
  serialbench fixture, verdict-identical over 2.06M differential
  cases.
- the #474 arm64 verdict is recorded structural (PR #1332): the
  strict tape stops carrying the record region it never touches
  (17 → 9 bytes/input byte), and the per-instruction profile is a
  FLAT walk (max lane 4.21%) with NEON engaged on both CI legs —
  no targeted kernel rewrite has a hot target.
- deep-nesting is back in its class — 2.24x local A/B (DOM
  ~1071 MB/s M1 Max) after the chained same-column pendings
  re-entered the fused walk (the 0.6.33 correctness bail was wider
  than the bug); ubuntu referee reads 2.6x–3.0x.
- flow-single rides 2.9x–3.5x vs ryml on ubuntu across the recent
  rounds (the flow-rooted fast path + the fused lenient walk under
  the recorder's classification).
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
