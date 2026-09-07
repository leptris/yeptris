#!/usr/bin/env bash
# build-native-asset.sh — prebuilt native materializer for the GitHub
# Release (TODO.restructure/28). Builds libyeptris at the given tag,
# compiles the extension from the matching yeptris-ruby tag, packs a
# platform tarball. Called by release.yml with the release version.
#
# Usage: build-native-asset.sh <version> <ruby-repo-url> [workdir]
set -euo pipefail

V="${1:?version required (X.Y.Z)}"
RUBY_REPO="${2:?ruby repo URL required}"
WORK="${3:-$(mktemp -d)}"
OUT="$WORK/yeptris-native-$(uname -s | tr '[:upper:]' '[:lower:]')-$(uname -m).tar.gz"

echo "::group::Build libyeptris v$V"
git clone --quiet --depth 1 --branch "v$V" https://github.com/leptris/yeptris "$WORK/yeptris-c"
cmake -B "$WORK/yeptris-c/build" -S "$WORK/yeptris-c" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DYEPTRIS_BUILD_TESTING=OFF \
  -DYEPTRIS_BUILD_CLI=OFF -DYEPTRIS_BUILD_SHARED=ON
cmake --build "$WORK/yeptris-c/build"
echo "::endgroup::"

echo "::group::Build the extension"
git clone --quiet "$RUBY_REPO" "$WORK/yeptris-ruby"
cd "$WORK/yeptris-ruby"
LOCKSTEP="$(git tag | grep -E "^v${V//./\.}\.[0-9]+$" | sort -V | tail -1 || true)"
if [ -n "$LOCKSTEP" ]; then
  git checkout --quiet "$LOCKSTEP"
  echo "extension from $LOCKSTEP"
else
  echo "no lockstep gem tag v$V.* — extension from main"
fi
cd ext/yeptris_native
YEPTRIS_LIB_PATH="$WORK/yeptris-c/build/src/libyeptris.so" \
  YEPTRIS_SRC="$WORK/yeptris-c/src" ruby extconf.rb
make
echo "::endgroup::"

echo "::group::Pack"
ARTIFACT=native.so
[ -f "$ARTIFACT" ] || ARTIFACT=native.bundle
# cwd is ext/yeptris_native here
tar -czf "$OUT" "$ARTIFACT"
echo "artifact=$OUT"
echo "::endgroup::"
