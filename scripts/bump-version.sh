#!/usr/bin/env bash
# bump-version.sh — the version SSOT mechanism (TODO.impl/01).
#
# CMakeLists.txt project(VERSION) is the single source of truth. This script
# computes the next version, updates CMakeLists.txt, vcpkg.json, and
# CHANGELOG.md in one shot, and verifies all three agree before exiting.
#
# Usage: scripts/bump-version.sh <X.Y.Z | major | minor | patch>
set -euo pipefail
cd "$(dirname "$0")/.."

if [ $# -ne 1 ]; then
    echo "usage: $0 <X.Y.Z | major | minor | patch>" >&2
    exit 1
fi

CURRENT="$(sed -nE 's/^ *VERSION ([0-9]+\.[0-9]+\.[0-9]+)$/\1/p' CMakeLists.txt | head -1)"
if [ -z "$CURRENT" ]; then
    echo "error: could not read project VERSION from CMakeLists.txt" >&2
    exit 1
fi

IFS=. read -r MAJOR MINOR PATCH <<<"$CURRENT"
case "$1" in
major) NEXT="$((MAJOR + 1)).0.0" ;;
minor) NEXT="$MAJOR.$((MINOR + 1)).0" ;;
patch) NEXT="$MAJOR.$MINOR.$((PATCH + 1))" ;;
*)
    if ! [[ "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "error: '$1' is not X.Y.Z, major, minor, or patch" >&2
        exit 1
    fi
    NEXT="$1"
    ;;
esac

if [ "$NEXT" = "$CURRENT" ]; then
    echo "error: next version equals current ($CURRENT)" >&2
    exit 1
fi

sed -i.bak -E "s/^( *VERSION )$CURRENT\$/\1$NEXT/" CMakeLists.txt && rm -f CMakeLists.txt.bak
sed -i.bak -E "s/(\"version\": )\"$CURRENT\"/\1\"$NEXT\"/" vcpkg.json && rm -f vcpkg.json.bak

if ! grep -q "## \[$NEXT\]" CHANGELOG.md; then
    sed -i.bak "2a\\
\\
## [$NEXT] - Unreleased" CHANGELOG.md && rm -f CHANGELOG.md.bak
fi

# Verify: all three sources must now agree.
V_CMAKE="$(sed -nE 's/^ *VERSION ([0-9]+\.[0-9]+\.[0-9]+)$/\1/p' CMakeLists.txt | head -1)"
V_VCPKG="$(sed -nE 's/.*"version": "([0-9]+\.[0-9]+\.[0-9]+)".*/\1/p' vcpkg.json | head -1)"
grep -q "## \[$NEXT\]" CHANGELOG.md
[ "$V_CMAKE" = "$NEXT" ] && [ "$V_VCPKG" = "$NEXT" ] || {
    echo "error: sync check failed (cmake=$V_CMAKE vcpkg=$V_VCPKG expected=$NEXT)" >&2
    exit 1
}

echo "version: $CURRENT -> $NEXT (CMakeLists.txt, vcpkg.json, CHANGELOG.md)"
