# 42 — Python: platform wheels vendoring libyeptris

Status: complete

## Why

Item 41's native loader only activates when the environment can
compile/link it (YEPTRIS_LIB_PATH / YEPTRIS_SRC). `pip install
yeptris` on a stock machine gets the pure-ctypes package — which
needs libyeptris anyway via the runtime ladder — and none of the
JSON win. The Ruby binding solved the identical problem with
platform gems (item 28): vendored libyeptris at the package root,
relative rpath, no host-runtime link to the interpreter.

## Plan

- [ ] Wheel matrix mirroring the platform-gem job: macOS arm64
      (MACOSX_DEPLOYMENT_TARGET pinned), manylinux x86_64 (auditwheel
      repair for the vendored .so), each wheel carrying
      libyeptris.{dylib,so} + the compiled extension.
- [ ] The extension's link shape mirrors the platform-gem rules:
      extension resolves libyeptris via $ORIGIN/@loader_path relative
      rpath; auditwheel must not rewrite it to an absolute path.
- [ ] The ffi/ctypes ladder gains the wheel-root vendor path (the
      Ruby ffi.rb ../../ ladder equivalent).
- [ ] Release flow: build + twine per Python-version tag matrix on
      the yeptris-py release trigger, idempotence via the PyPI JSON
      API (the RubyGems versions-API lesson — never trust fetch
      exit codes).
- [ ] Gem-smoke equivalent: install the built wheel in a scratch
      venv, assert the native loader ACTIVE + the canary battery
      ([[40-release-integrity]] discipline), before twine.

## Acceptance

- `pip install yeptris` on stock macOS/Linux loads the native JSON
  path with zero compiler/env (verified in a clean venv).
- Pure sdist path still installs pure (feature-detect falls back).

## Outcome (2026-09-08, py PRs #24/#26/#27/#28; PyPI 0.1.15.1)

COMPLETE — pip install gets the native JSON engine with zero
compiler and zero env:

- ABI3: Py_LIMITED_API 0x03090000 — ONE wheel per platform serves
  CPython 3.9+. The limited API forced two improvements:
  PyBytes_AsString/Size (no macros) and the key cache storing the
  fill-time key BYTES inline in its slot — the probe is a plain
  memcmp, no CPython unicode internals on any path.
- Vendoring: setup.py YEPTRIS_VENDOR=1 copies libyeptris into
  yeptris/_platform/<v>/ under the REAL SONAME name
  (libyeptris.so.0 / libyeptris.0.dylib) beside the plain name —
  the extension's DT_NEEDED is the soname and auditwheel checks the
  wheel tree for it; the extension links with a RELATIVE rpath
  (@loader_path / $ORIGIN — the platform-gem laws).
- release-wheels.yml: lockstep-tag fired; C core at the matching C
  tag (coordinates from scripts/resolve-version.sh — the pyproject
  SSOT; inline-python-in-YAML quoting was mangled, and
  GITHUB_REF_NAME broke main-ref dispatches: a dispatch at the tag
  runs the TAG's workflow text); macOS floor tag macosx_11_0 via
  `wheel tags --remove` (packaging picks the BUILD MACHINE's OS —
  14.0 — despite the deployment target); manylinux via auditwheel
  repair; SMOKE in a clean venv with zero env (engine MUST be
  native, the vendored lib MUST be the one loaded) before release
  attach; a pure job ships the sdist + py3-none-any fallback.
  Publish rides twine from a maintainer machine until PyPI trusted
  publishing is configured (the same credentials story as before).
- Found by the smoke along the way: __version__ was stale
  (hardcoded 0.1.0; pyproject is now the SSOT via
  importlib.metadata + dev-tree fallback), and beyond-int64
  integers returned String on the YAML surface where PyYAML returns
  int — fixed in the pair fast path, _cvalue, and _value via one
  _maybe_bigint helper (PyYAML's decimal shape; leading zeros stay
  strings per PyYAML's octal rule; PyYAML differential green incl.
  big-int KEYS).
- VERIFIED END-TO-END: pip install yeptris on stock macOS resolves
  the platform wheel — engine native, version 0.1.15.1, 0.820x min
  vs json.loads from the PyPI artifact, YAML surface correct.
  Branch protection on yeptris-py (tests + both GATE rows).

The board is now COMPLETE: TODO.impl 01-20 and TODO.restructure
01-43 all done or closed-by-measurement.
