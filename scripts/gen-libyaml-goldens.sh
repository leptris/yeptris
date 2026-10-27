#!/bin/sh
# Regenerates the libyaml differential goldens (TODO.impl/17).
#
#   scripts/gen-libyaml-goldens.sh /path/to/libyaml
#
# Builds libyaml, rebuilds the generator, and rewrites
# test/port/libyaml/snapshots/. The snapshots are committed; CI never
# needs libyaml. Divergences the suite disproves go to ledger.txt.
set -e
LIBYAML=${1:?usage: gen-libyaml-goldens.sh /path/to/libyaml}
cmake -B "$LIBYAML/build" -S "$LIBYAML" -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=OFF -DYAML_BUILD_SHARED_LIBS=OFF >/dev/null
cmake --build "$LIBYAML/build" >/dev/null
cmake -B build-validate -S . -DCMAKE_BUILD_TYPE=Release \
      -DYEPTRIS_LIBYAML_ROOT="$LIBYAML" >/dev/null
cmake --build build-validate --target gen_libyaml_golden >/dev/null
rm -rf test/port/libyaml/snapshots
mkdir -p test/port/libyaml/snapshots
./build-validate/test/gen_libyaml_golden test/port/libyaml/snapshots \
    --suite test/conformance/data/yaml-test-suite/src
for f in "$LIBYAML"/examples/*.yaml; do
    n=$(basename "$f" .yaml)
    ./build-validate/test/gen_libyaml_golden test/port/libyaml/snapshots \
        --file "example-$n" "$f"
done
for f in "$LIBYAML"/regression-inputs/*; do
    n=$(basename "$f"); n=${n%.*}
    ./build-validate/test/gen_libyaml_golden test/port/libyaml/snapshots \
        --file "regression-$n" "$f"
done
echo "snapshots regenerated: $(ls test/port/libyaml/snapshots | wc -l) files"
