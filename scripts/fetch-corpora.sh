#!/usr/bin/env bash
# fetch-corpora.sh — pinned conformance corpora (TODO.impl/16).
#
# The yaml-test-suite is fetched at a pinned revision into
# test/conformance/data/ (gitignored). Reproducible: the pin lives here.
set -euo pipefail
cd "$(dirname "$0")/.."

DATA=test/conformance/data
YTS_PIN=HEAD # pin a commit hash once the runner lands (stability)

if [ -d "$DATA/yaml-test-suite" ]; then
    echo "corpora: $DATA/yaml-test-suite already present"
    exit 0
fi

mkdir -p "$DATA"
git clone --depth 1 https://github.com/yaml/yaml-test-suite.git "$DATA/yaml-test-suite"
echo "corpora: fetched yaml-test-suite at $YTS_PIN"
