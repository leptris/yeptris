# TODO.impl/19 — Hardening: sanitizers, fuzzing, differential fuzzing, limits, threads

Status: pending · Depends: 07+ (continuous) · Layer: `test/{fuzz,concurrency}` + CI · PLAN.md phase: 5

## Goal

The safety story: memory-safe, crash-free under any input, semantically
consistent with the references, thread-correct — proven continuously, not
asserted.

## Deliverables

- Sanitizer gates (CI): ASAN+LSAN on every push; TSAN on the concurrency
  suite; UBSAN in the nightly; `valgrind` job on Linux for the C suite.
- Fuzz harnesses (`test/fuzz/`, libFuzzer + a standalone
  `{afl,libfuzzer}-friendly} main`): `fuzz_parse` (all schemas + options
  matrix), `fuzz_feed` (chunked streaming — adversarial split points,
  the 06/07 state-carry nightmare cases), `fuzz_roundtrip`
  (parse→emit→parse equality, catching emitter/parser asymmetries),
  `fuzz_transcode` (encoding front-end), `fuzz_recorder` (bulk delivery
  invariants). Seed corpora: everything in `corpora.json`.
- **Differential fuzzing** vs libyaml (the big one): `yepdiff` (17)
  driven by the fuzzer — same input must yield classify-equal event
  streams or both-error, in compat mode. Mismatches file as compat bugs
  with minimized reproducers. Nightly, time-boxed, crash-artifact
  retention.
- Allocation-failure injection: the libfyaml-style test allocator over
  03's hook — every Nth allocation fails across the suite; OOM paths must
  return `YEPTRIS_ERROR_MEMORY` with the document still freeable (no
  leaks, no double-frees under failure).
- Limit guards: depth (default 1000), simple-key length (1024), line
  length (streaming-safe — no whole-line buffering requirement),
  anchor/alias counts; each with a test at the boundary and one past it.
- Thread contract tests (the leptris pack): one-document-per-thread
  parse/emit/free on N threads; read-only sharing of one document;
  thread-local vs per-document error isolation; `yeptris_thread_cleanup`
  releases per-thread caches; documented contract mirrored in CLAUDE.md.
- Streaming chunk-boundary torture: every corpus re-fed at every split
  offset 0..K (sampled + adversarial: inside escapes, inside block-scalar
  indent, inside UTF-8 sequences, inside line endings).

## Design decisions

- Fuzzers own no new truth: they check invariants (no crash, roundtrip
  equality, differential-classify-equal) — property-based, so any
  behavior change that matters trips one.
- Corpus discipline: minimized reproducers committed under
  `test/fuzz/corpus/` and re-run as regression tests in the normal suite
  (a crash found by the fuzzer becomes a ctest case the moment it's
  minimized).
- CVE posture: parsers are attack surface; the depth/length limits are
  always-on defaults, not options.

## Phases

A. ASAN/TSAN gates + `fuzz_parse` + seed corpus + nightly CI.
B. Roundtrip/feed/transcode/recorder fuzzers + chunk torture + limits.
C. Differential fuzzing + OOM injection + full thread pack.

## Acceptance

- Nightly fuzz (5–10 min in CI, longer locally): zero crashes, zero
  leaks, zero new sanitizer reports over a rolling week.
- Differential mismatch count: 0 unregistered on the corpus; fuzz-found
  mismatches each become either a fix or a registered divergence.
- OOM suite: zero leaks under failure (LSAN gate) — the hard one.

## References

- `~/src/leptris/leptris/.github/workflows/{asan,fuzz-nightly}.yml` +
  `test/concurrency/`, `~/src/external/simdjson/fuzz/` (harness shapes),
  `~/src/external/libfyaml/test/libfyaml-test-allocator.c` (OOM
  injection), `~/src/external/libyaml` (historical CVE shapes to probe:
  deep nesting, huge anchors, incomplete UTF-8 at EOF).
