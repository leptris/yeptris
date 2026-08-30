# TODO.impl/20 — Packaging, ABI policy, automated release, distribution

Status: pending · Depends: all · Layer: repo-wide · PLAN.md phase: 6

## Goal

Ship it the way libleptris ships: vcpkg-ready, ABI-disciplined, released
only by the automated workflow, with the binding lockstep wired end to end.

## Deliverables

- `vcpkg.json` + `ports/yeptris/portfile.cmake` + `usage` (the leptris
  jemalloc-convention pattern); pkg-config file; `find_package(yeptris)`
  config + `yeptris::yeptris` target.
- ABI policy (documented in `docs/ABI.md` + enforced by tests):
  - opaque pointer-sized handles (static asserts, 01);
  - enum values pinned by the ABI test — changing one requires a major
    bump + binding rebuild;
  - soname: `libyeptris.so.MAJOR` (ELF) / dylib compatibility version;
  - struct-layout compatibility promise only for public option structs,
    versioned by size (`sizeof` passed in or reserved fields) — never
    extend by reordering.
- Automated release workflow (`.github/workflows/release.yml`):
  `workflow_dispatch(next_version)` → `release/vX.Y.Z` branch → version
  bump (CMakeLists + vcpkg.json + CHANGELOG via `bump-version.sh`) → PR
  → on merge: tag + GitHub Release with CHANGELOG notes. **No manual
  tags, no manual releases — ever** (repo law, global law).
- Binding lockstep: gem major.minor tracks the C library (15);
  `rake audit:symbols` (attached == exported on the vendored library,
  fails listing both drift directions).
- Docs: `README.adoc` (leptris-style exhaustive reference, grown
  incrementally — start it in Phase 1, not at the end), Doxygen API
  toggle, `docs/FFI.md` contract when bindings stabilize, man pages via
  the AdocMan pattern (optional, last).
- Distribution (post-MVP, tracked here so it isn't forgotten): Homebrew
  tap formula mirroring `brew install lutaml/tap/libleptris`, Debian/
  Alpine/MSYS2 submissions after vcpkg.
- CI format-check job (clang-format ≥ 14 across the tree; found during
  item 04 that the PATH clang-format can be ancient — pin a version in CI),
  and MSVC `/arch:AVX2` wiring for the AVX2 TU (guarded out today; CI
  matrix is gcc/clang).

## Design decisions

- The release PR carries three files only (CMakeLists VERSION,
  vcpkg.json, CHANGELOG) — `bump-version.sh` is the SSOT mechanism; hand
  edits to version strings anywhere are review-rejected.
- CHANGELOG entries are written when features land (per-PR), not
  reconstructed at release — the release PR edits copy, not history.
- Precompiled platform gems publish from the binding's workflow with the
  idempotent publish loop (already-published gems skipped — the
  leptris-ruby behavior).

## Phases

A. vcpkg manifest + portfile template + pkg-config + find_package.
B. ABI doc + soname policy + versioned-options audit; symbol audit task
  from 15 wired into CI.
C. Release workflow dry-run on a `-rc` tag; first real release drill.

## Acceptance

- A clean-room consumer project builds via both `find_package` and
  pkg-config on Linux + macOS (+ MSVC in CI matrix).
- Release drill completes with zero manual git commands (audited);
  `rake audit:symbols` green on the released gem.

## References

- `~/src/leptris/leptris/{vcpkg.json,ports/,docs/,cli/man/}` +
  `.github/workflows/`, `~/src/leptris/leptris-ruby/` (platform-gem +
  lockstep + audit machinery), `~/src/leptris/leptris/CLAUDE.md`
  (release law text to mirror).
