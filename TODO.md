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
| 06 | [Scan layer: line table, indentation, indicators, spans](TODO.impl/06-scan-layer.md) | 04, 05 | active (v1) |
| 07 | [Parse engine: one resumable state machine](TODO.impl/07-parse-engine.md) | 06 | active (v1) |
| 08 | [Flow kernel](TODO.impl/08-flow-kernel.md) | 07 | v1 in 07 + 08C strict JSON mode done (jsonsuite 95/95+188/188); number kernel + SIMD hardening remain |
| 09 | [Scalars: trim, fold, unescape, style recording](TODO.impl/09-scalars.md) | 07 | active (v1) |
| 10 | [Schema resolvers: 1.2 core, 1.1 compat, options](TODO.impl/10-schema-resolvers.md) | 09 | pending |
| 11 | [DOM: compact nodes, vtables, interning, O(1) access](TODO.impl/11-dom.md) | 07, 10 | active (v1) |
| 12 | [Event delivery](TODO.impl/12-event-delivery.md) | 07, 11 | COMPLETE (pull/push/recorder/iterparse + yaml_compat adapter) |
| 13 | [Emitter](TODO.impl/13-emitter.md) | 11, 09, 10 | A + canonical done (fixed-point gate 405/405); folding + streaming remain |
| 14 | [Float printer](TODO.impl/14-float-printer.md) | 13 | clean-room v1 landed; cached-power perf queued |
| 15 | [Ruby binding: FFI gem + Psych compatibility](TODO.impl/15-ruby-binding.md) | 11–13 | pending |
| 16 | [Conformance harness: test-suite + divergence ledger](TODO.impl/16-conformance-harness.md) | 07, 12 | active (corpus) |
| 17 | [libyaml test port](TODO.impl/17-libyaml-test-port.md) | 12, 16 | parse + emitter differentials green (279 .ly goldens); yepdiff tool |
| 18 | [Benchmarks: matrix, corpora, CI artifacts, ledger](TODO.impl/18-benchmarks.md) | 06+ | A done: matrix+ledger+CI bench.yml, all shapes >2.1x vs libyaml |
| 19 | [Hardening: sanitizers, fuzzing, differential, limits](TODO.impl/19-hardening.md) | 07+ | pending |
| 20 | [Packaging, ABI policy, automated release](TODO.impl/20-packaging-release.md) | all | pending |
| 21 | [JSON API compat](TODO.impl/21-json-compat-api.md) | 08, 11, 13, 18 | core landed: jsonc drop-in + json.hpp + JSON writer; building API awaits 11p3 |

Rules inherited from libleptris: one executed plan per item; each phase
gate in the item file must pass before the item closes; performance
claims need artifact numbers (18); dead ends go in the ledger, not in the
bin. Open product decisions live in PLAN.md §9.
