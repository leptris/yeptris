# 42 — Python: platform wheels vendoring libyeptris

Status: pending

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
