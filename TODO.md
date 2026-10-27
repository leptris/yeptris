# Open work

Implementation lives in the numbered files under `TODO.impl/`, executed in
numeric order unless a dependency says otherwise. `PLAN.md` §6 maps items
to phases. Status: pending / active / done / dead (dead = attempted and
measured-dead; record it, keep the numbers — do not delete history).

| # | Item | Depends | Status |
| --- | --- | --- | --- |
| 01 | [Bootstrap: skeleton, build, CI, CLI registry](TODO.impl/01-bootstrap.md) | — | done |
| 02 | [Common foundations: port, chartype, cpu, views, errors](TODO.impl/02-common-foundations.md) | 01 | done |
| 03 | [Memory: pool, compact allocator, content-sized arena](TODO.impl/03-memory-allocators.md) | 02 | done |
| 04 | [SIMD kernel framework (AOT TUs + dispatch)](TODO.impl/04-simd-kernels.md) | 02 | done |
| 05 | [Encoding front-end: BOM, UTF-8 validation, transcode](TODO.impl/05-encoding-frontend.md) | 03, 04 | done |
| 06 | [Scan layer](TODO.impl/06-scan-layer.md) | 04, 05 | v1 complete (arena-sizing fusion landed; streaming window resolved by 07); line-table 2GB/s gate + NEL/LS/PS scan-layer note remain |
| 07 | [Parse engine](TODO.impl/07-parse-engine.md) | 06 | v1 complete incl. resumable stepping; edge grammar items remain ([a: [1]] single-pair, simple-key limit, %TAG expansion) |
| 08 | [Flow kernel](TODO.impl/08-flow-kernel.md) | 07 | COMPLETE (A/B/C: fast path, number kernel, strict JSON; SIMD dispatch measured dead — ledger) |
| 09 | [Scalars](TODO.impl/09-scalars.md) | 07 | COMPLETE (fold/quote/block finishing green over the suite) |
| 10 | [Schema resolvers](TODO.impl/10-schema-resolvers.md) | 09 | COMPLETE (core12, compat11, parse options) |
| 11 | [DOM](TODO.impl/11-dom.md) | 07, 10 | v1 complete (60B nodes, O(1) mapindex, mutation, JSON direct-build); trimmed-init lever ledgered |
| 12 | [Event delivery](TODO.impl/12-event-delivery.md) | 07, 11 | COMPLETE (pull/push/recorder/iterparse + yaml_compat adapter) |
| 13 | [Emitter](TODO.impl/13-emitter.md) | 11, 09, 10 | COMPLETE (A, canonical, streaming, width folding) |
| 14 | [Float printer](TODO.impl/14-float-printer.md) | 13 | COMPLETE (clean-room two-tier); cached-power perf queued in ledger |
| 15 | [Ruby binding](TODO.impl/15-ruby-binding.md) | 11–13 | phases A–E landed (gem, materializer, Psych drop-in, object serialization, event API, .tml corpora); gem publishing/version = USER release decisions |
| 16 | [Conformance harness](TODO.impl/16-conformance-harness.md) | 07, 12 | COMPLETE (395/395 suite + 67/67 psych-pure, strict-gated) |
| 17 | [libyaml test port](TODO.impl/17-libyaml-test-port.md) | 12, 16 | COMPLETE (405-snapshot parse differential, 279 emitter goldens, yepdiff, 67-divergence ledger) |
| 18 | [Benchmarks](TODO.impl/18-benchmarks.md) | 06+ | COMPLETE (matrix+CI+ledger+mem measures); compactness gap logged |
| 19 | [Hardening](TODO.impl/19-hardening.md) | 07+ | COMPLETE (fuzz+nightly+alloc-inject+threads+TSAN/UBSAN/valgrind+differential fuzz) |
| 20 | [Packaging, ABI policy, automated release](TODO.impl/20-packaging-release.md) | all | core landed (install/pkg-config/vcpkg/release workflow/ABI+FFI docs); brew tap + distro submissions EXCLUDED by user |
| 21 | [JSON API compat](TODO.impl/21-json-compat-api.md) | 08, 11, 13, 18 | COMPLETE (strict JSON, jsonc drop-in incl. pretty/building, json.hpp, yajl gen + SAX, direct DOM) |
| 22 | [CBOR (RFC 8949) codec: decode/encode, deterministic profile, sequences](TODO.impl/22-cbor.md) — executed plans in `TODO.cbor/` | 11, 13, 18, 19 | 01-04 + 06-07 done (decode/encode/sequences shipped; ruby surface #150; Appendix A sweep #340; bench tier + ledger); 05 lutaml adapter = the sole remainder (cross-repo, rides the lutaml side) |

Rules inherited from libleptris: one executed plan per item; each phase
gate in the item file must pass before the item closes; performance
claims need artifact numbers (18); dead ends go in the ledger, not in the
bin. Open product decisions live in PLAN.md §9.

## Follow-up: per-minor native staging on POSIX (from the 0.6.8.1 verification)

The platform gems stage ONE native-{minor} ext — built by whichever
Ruby ran the packaging leg (darwin arm64 = 3.3; linux x86_64 =
ubuntu-latest's default; musl likewise). Every OTHER Ruby minor falls
back to the FFI ladder: functional, but the native JSON accelerator
is dark for them. Windows already solves this: per-minor legs stage
native-<minor>.dll into the shared packaging tree, one leg assembles.
Port that scheme: darwin arm64 (3.3/3.4/4.0) + x86_64, linux
glibc/musl x86_64+aarch64 — eight builder legs (or fewer if the
script compiles for multiple rubies via setup-ruby in one job), the
final leg assembles and pushes. Acceptance: the gem-smoke asserts the
native surface loads (engine == native) on every covered minor, not
just that the gem installs.


## Open: CBOR→VALUE native materializer (from #157 / the 2026-09-19 ledger entry)

The cbor gem leads CBOR.load by 3.4-4.8x (serialbench matrix). The
marshal route measured FLAT (to_ruby already marshals — ledgered).
The lever: decode + direct Ruby object construction in one C pass
inside ext/yeptris_native (json_ruby.c's sibling: cbor_load building
rb_hash/rstr/rflo directly off the decoder's DOM walk, no Marshal
intermediate, no per-node FFI). Acceptance: CBOR.load >= the cbor gem
on the serialbench fixtures; the FFI ladder stays the fallback.
