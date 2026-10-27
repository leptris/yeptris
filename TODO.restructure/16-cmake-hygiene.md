# 16 — CMake target hygiene: the benchmarks were outside the warnings contract

## Why
Every C test/cli/library target gets yeptris_set_warnings (verified:
19/19 in test/, all in src/ and cli/) — but benchmarks/ never did.
bench_matrix/bench_float compile with the DEFAULT warning set: the
"new warnings are bugs" law had a hole in the harness that
measures the law's own subject.

## Plan
- yeptris_set_warnings on both benchmark targets (C++ targets: the
  helper applies to CXX too — verify)

## Acceptance
- a deliberate -Werror-visible warning in a bench TU fails the build
- build-bench compiles clean, ctest unchanged

## Status: COMPLETE
Anti-rot PROVEN: a deliberate -Wunused-variable in a bench TU fails
the build under -Werror (a namespace-scope int is NOT covered by
-Wunused-variable in C++ — locals are); restored, builds clean.
Note: the standing cache had WARNINGS_AS_ERRORS=OFF; the gate is CI
(the matrix builds with -Werror) plus any local -Werror configure.
