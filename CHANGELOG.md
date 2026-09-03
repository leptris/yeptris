# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.0] - 2026-09-03

### Added

- Bootstrap scaffold (TODO.impl/01): CMake build (C11, LTO for Release,
  ASAN/TSAN options, scoped warnings), public header skeleton (`yeptris.h`
  umbrella, opaque pointer-sized handles, pinned status enum, generated
  version header), CLI with command registry (`yeptris version`), ABI
  pinning test, CI workflows (test matrix + ASAN), `validate.sh` and
  `bump-version.sh`.
