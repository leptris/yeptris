# TODO.impl/01 — Bootstrap: skeleton, build, CI, CLI registry

Status: pending · Depends: — · Layer: repo-wide · PLAN.md phase: 0

## Goal

Stand up the repository so every later item lands into a working, CI-gated
tree: CMake build, directory contract, version SSOT, CI workflows, CLI
skeleton with the OCP command registry, and the public-header skeleton with
ABI pinning tests.

## Deliverables

- `CMakeLists.txt` (≥ 3.20), `CMakePresets.json` (`debug`, `release`, `asan`,
  `tsan`, `fuzz`), `.clang-format` (copied from libleptris), `.gitattributes`.
- Options: `BUILD_TESTING` (ON), `YEPTRIS_BUILD_CLI` (ON),
  `YEPTRIS_BUILD_SHARED`/`_STATIC` (ON/OFF), `YEPTRIS_BUILD_BENCHMARKS` (OFF),
  `YEPTRIS_ENABLE_LTO` (ON for Release/RelWithDebInfo), `YEPTRIS_ENABLE_ASAN`,
  `YEPTRIS_ENABLE_FUZZING`.
- Directory contract (create empty with `.gitkeep` or first real file):

```
src/include/yeptris/    public ABI headers (types.h, error.h, version.h stubs)
src/yeptris/            core: common/ scan/ parse/ events/ dom/ emit/ memory/ encoding/
cli/                    commands/ + main.c (registry pattern)
test/                   unit/ conformance/ port/ flow/ fuzz/ concurrency/
benchmarks/             matrix + corpora manifest
scripts/                validate.sh, fetch-corpora.sh, bump-version.sh
bindings/ruby/          (item 15; placeholder only)
docs/                   FFI contract when bindings land
```

- Public header skeleton: `yeptris.h` umbrella + `types.h` with opaque
  pointer-sized handles and `_Static_assert(sizeof(YeptrisX) == sizeof(void*))`;
  `error.h` with the status enum.
- ABI/enum pinning test (`test/unit/test_abi.cpp`, the leptris HeaderHygiene
  pattern): compile-time table of enum values asserted — bindings hard-code
  these; changing one is an ABI break requiring a major bump.
- CLI: `cli/main.c` + `cli/commands/{parse,emit,version}.c` registered via a
  command table (`cli/commands.h`); adding a command = new file + one
  registration line. Commands do argument parsing and printing only — all
  work goes through the public API. Output formats are MECE: YAML / JSON /
  text; never mixed.
- CI: `.github/workflows/{test,asan,bench,fuzz-nightly}.yml` mirroring
  libleptris (Linux + macOS matrix, ctest, artifact upload for bench).
- `scripts/validate.sh`: clean build → ctest → CLI smoke → leak check gates.
- Version SSOT: `project(yeptris VERSION …)` in CMakeLists is the single
  source; `scripts/bump-version.sh` syncs `vcpkg.json` + `CHANGELOG.md`.
  Releases only via the automated release workflow — never manual tags.

## Design decisions

- C99, `-Wall -Wextra` warning-clean (CI adds `-Werror`), `C_EXTENSIONS OFF`.
  No GNU extensions. Benchmarks are C++11.
- Error model contract (implemented in 02): every public entry point writes
  public `YeptrisStatus` codes; internal diagnostics flow through the
  thread-local/per-document channels.
- The recursive question is settled at birth: **there is no recursive parser
  to remove later** — the engine (07) is a state machine from day one.

## Phases

A. CMake + presets + headers + ABI test + validate.sh.
B. CLI registry skeleton (`yeptris version` works end-to-end).
C. CI workflows green on an empty test suite.

## Acceptance

- Fresh clone → `cmake --build build` → `ctest` → `./build/cli/yeptris version`
  all succeed on macOS (arm64) and Linux (x86_64 CI).
- `validate.sh` exits 0. Zero warnings.

## References

- `~/src/leptris/leptris/CLAUDE.md` (conventions), `CMakeLists.txt`,
  `scripts/`, `.github/workflows/`, `cli/CLI_ARCHITECTURE.md`.
