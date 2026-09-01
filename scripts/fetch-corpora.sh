#!/usr/bin/env bash
# fetch-corpora.sh — pinned conformance corpora (TODO.impl/16, 08C).
#
# Both corpora are fetched at pinned revisions into test/conformance/data/
# (gitignored). Reproducible: the pins live here.
set -euo pipefail
cd "$(dirname "$0")/.."

DATA=test/conformance/data
YTS_PIN=da267a5c4782e7361e82889e76c0dc7df0e1e870 # yaml-test-suite
JTS_PIN=1ef36fa01286573e846ac449e8683f8833c5b26a # JSONTestSuite (nst)

mkdir -p "$DATA"

if [ ! -d "$DATA/yaml-test-suite" ]; then
    git clone -q https://github.com/yaml/yaml-test-suite.git "$DATA/yaml-test-suite"
    git -C "$DATA/yaml-test-suite" checkout -q "$YTS_PIN"
    echo "corpora: fetched yaml-test-suite at $YTS_PIN"
else
    echo "corpora: $DATA/yaml-test-suite already present"
fi

if [ ! -d "$DATA/json-test-suite" ]; then
    git clone -q https://github.com/nst/JSONTestSuite.git "$DATA/json-test-suite"
    git -C "$DATA/json-test-suite" checkout -q "$JTS_PIN"
    echo "corpora: fetched JSONTestSuite at $JTS_PIN"
else
    echo "corpora: $DATA/json-test-suite already present"
fi
