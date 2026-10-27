# 20 — The vcpkg overlay port installs for real (SHA512 filled)

## Why
portfile.cmake carries `SHA512 0` — vcpkg_from_github rejects it, so
the in-repo overlay port (the user-facing consumption path, distinct
from the excluded brew/distro submissions) cannot actually install.

## Plan
- compute the GitHub tag-tarball SHA512 for the current version,
  fill it in; note the update cadence (per release)

## Acceptance
- the hash is the real one for v0.1.10; vcpkg_from_github semantics
  documented in a comment

## Status: COMPLETE
SHA512 of the v0.1.10 tag tarball filled (verified 128-hex); the
recompute recipe documented beside it. Future releases: the release
workflow could automate this (noted for the next pass).
