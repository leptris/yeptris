# test/ — Google Test binaries. One binary per suite directory as they grow
# (unit, conformance, port, flow, fuzz, concurrency — TODO.impl/01/16/17/19).

# Tests are validation, not payload: build them without interprocedural
# optimization even when the libraries keep it. Under musl + GCC 15
# (ruby:3.4-alpine as of 2026-10) the LTO final-link repartition collides
# with Alpine's fortified always_inline vsnprintf — the lto-wrapper dies
# mid-ltrans and the whole platform-gem release build fails with it. LTO
# buys nothing for a test binary.
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION FALSE)


find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    message(STATUS "GTest not found locally — fetching googletest v1.17.0")
    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(test_unit
    unit/test_abi.cpp
    unit/test_alloc_fail.cpp
    unit/test_threads.cpp
    unit/test_chartype.cpp
    unit/test_cpu.cpp
    unit/test_encoding.cpp
    unit/test_events.cpp
    unit/test_stepping.cpp
    unit/test_emit.cpp
    unit/test_float.cpp
    unit/test_event_marks.cpp
    unit/test_float_limbs.cpp
    unit/test_resolve.cpp
    unit/test_error.cpp
    unit/test_memory.cpp
    unit/test_nametab.cpp
    unit/test_parse.cpp
    unit/test_simd_text.cpp
    unit/test_string_view.cpp
    unit/test_json_hpp.cpp
    unit/test_jsonc_compat.cpp
    unit/test_yajl_compat.cpp
    unit/test_number_kernel.cpp
    unit/test_mutate.cpp
    unit/test_mapindex.cpp
    unit/test_limits.cpp
    unit/test_yaml_compat.cpp
    unit/test_marshal.cpp
    unit/test_marshal_json.cpp
    unit/test_visit.cpp
    unit/test_json_number_scan.cpp
    unit/test_line_shape.cpp
    unit/test_schema.cpp
    unit/test_validate.cpp
    unit/test_stream_iter.cpp
    unit/test_validate_stream.cpp
    unit/test_validate_output.cpp
    unit/test_tape.cpp
    unit/test_ytape.cpp
    unit/test_plan.cpp
    unit/test_cbor.cpp
    unit/test_cbor_marshal.cpp
)
target_compile_definitions(test_unit PRIVATE
    TEST_YTAPE_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/conformance/data"
    TEST_JSONSCHEMA_OUTPUT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/conformance/jsonschema/output")

find_package(Threads REQUIRED)
target_link_libraries(test_unit PRIVATE yeptris_static Threads::Threads
    $<TARGET_NAME_IF_EXISTS:yeptris_jsonc> $<TARGET_NAME_IF_EXISTS:yeptris_yajl> GTest::gtest_main)
target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(test_unit)

include(GoogleTest)
# the suite crossed the 5 s default under load (LTO link + parallel
# ctest) and discovery "terminated due to timeout" — a flaky red, not a
# real failure
gtest_discover_tests(test_unit DISCOVERY_TIMEOUT 60)

# ---- emitter roundtrip gate (TODO.impl/13) -----------------------------
add_executable(test_emit_roundtrip emit_roundtrip.c)
target_link_libraries(test_emit_roundtrip PRIVATE yeptris_static)
target_include_directories(test_emit_roundtrip PRIVATE ${CMAKE_SOURCE_DIR}/test
    ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(test_emit_roundtrip)
add_test(
    NAME emit-roundtrip
    COMMAND test_emit_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

# ---- cross-model event check (TODO.impl/12) ----------------------------
add_executable(test_events_crosscheck events_crosscheck.c)
target_link_libraries(test_events_crosscheck PRIVATE yeptris_static)
target_include_directories(test_events_crosscheck PRIVATE ${CMAKE_SOURCE_DIR}/test)
yeptris_set_warnings(test_events_crosscheck)
add_test(
    NAME events-crosscheck
    COMMAND test_events_crosscheck ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

# ---- libyaml differential (TODO.impl/17) --------------------------------
# The snapshots are libyaml's own event streams over the corpora,
# committed once via scripts/gen-libyaml-goldens.sh; CI replays them
# against yeptris without libyaml present.
add_executable(test_libyaml_diff port/libyaml/diff_runner.c)
target_link_libraries(test_libyaml_diff PRIVATE yeptris_static)
target_include_directories(test_libyaml_diff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(test_libyaml_diff)
add_test(
    NAME libyaml-diff
    COMMAND test_libyaml_diff ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
            ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/ledger.txt
)
set_tests_properties(libyaml-diff PROPERTIES ENVIRONMENT "YEP_STRICT=1")

# ---- differential drivers (TODO.cbor/06) ------------------------------
add_executable(cbor_canon tools/cbor_canon.c)
target_link_libraries(cbor_canon PRIVATE yeptris_static)
target_include_directories(cbor_canon PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
yeptris_set_warnings(cbor_canon)

# ---- fuzz harnesses (TODO.impl/19) ------------------------------------
# Standalone corpus walkers (crash on invariant violation); the
# libFuzzer build is CI-nightly (YEPTRIS_ENABLE_FUZZING).
add_executable(fuzz_parse fuzz/fuzz_parse.c)
target_link_libraries(fuzz_parse PRIVATE yeptris_static)
target_include_directories(fuzz_parse PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_parse PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_parse)

add_executable(fuzz_roundtrip fuzz/fuzz_roundtrip.c)
target_link_libraries(fuzz_roundtrip PRIVATE yeptris_static)
target_compile_definitions(fuzz_roundtrip PRIVATE YEP_FUZZ_STANDALONE)

add_executable(fuzz_feed fuzz/fuzz_feed.c)
target_link_libraries(fuzz_feed PRIVATE yeptris_static)
target_include_directories(fuzz_feed PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_feed PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_feed)

add_executable(fuzz_cbor fuzz/fuzz_cbor.c)
target_link_libraries(fuzz_cbor PRIVATE yeptris_static)
target_include_directories(fuzz_cbor PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_cbor PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_cbor)

add_executable(fuzz_transcode fuzz/fuzz_transcode.c)
target_link_libraries(fuzz_transcode PRIVATE yeptris_static)
target_include_directories(fuzz_transcode PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_transcode PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_transcode)
yeptris_set_warnings(fuzz_roundtrip)

# the two Marshal wire readers (marshal/to_json.c + cbor/from_marshal.c):
# the 2026-10 audit found the format's readers un-fuzzed
add_executable(fuzz_marshal fuzz/fuzz_marshal.c)
target_link_libraries(fuzz_marshal PRIVATE yeptris_static)
target_include_directories(fuzz_marshal PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_marshal PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_marshal)

add_test(
    NAME fuzz-corpus
    COMMAND fuzz_parse ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

add_test(
    NAME fuzz-parse-regression
    COMMAND fuzz_parse ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse
)

add_test(
    NAME fuzz-cbor-corpus
    COMMAND fuzz_cbor ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/cbor
)
add_test(
    NAME fuzz-marshal-corpus
    COMMAND fuzz_marshal ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/marshal
)

# the validation face (#882): both walks over untrusted bytes must be
# crash-clean (statuses OK/SCHEMA/PARSE/DEPTH/ENCODING/MEMORY/ARG only)
add_executable(fuzz_validate fuzz/fuzz_validate.c)
target_link_libraries(fuzz_validate PRIVATE yeptris_static)
target_include_directories(fuzz_validate PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_validate PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_validate)

add_test(
    NAME fuzz-validate-corpus
    COMMAND fuzz_validate ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/validate
)

# the compiled-schema economy (#885 item 3): untrusted SCHEMA bytes —
# not just instance bytes — through compile/validate/free must be
# crash- and leak-clean (statuses OK/SCHEMA/PARSE/DEPTH/ENCODING/
# MEMORY/ARG only); the compile arena, the pattern-program builder and
# the $ref depth guards all live past the call
add_executable(fuzz_schema_compile fuzz/fuzz_schema_compile.c)
target_link_libraries(fuzz_schema_compile PRIVATE yeptris_static)
target_include_directories(fuzz_schema_compile PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_schema_compile PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_schema_compile)

add_test(
    NAME fuzz-schema-compile-corpus
    COMMAND fuzz_schema_compile ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/schema_compile
)

# ---- JSON-Schema-Test-Suite (draft2020-12) driver (#885 item 2) --------
# The vendored fixtures under conformance/jsonschema/tests are the
# official suite's main tests; optional/ carries the pieces the
# subset's divergence is documented against. carries.txt names the
# expected failures (remote refs, $id/$anchor machinery, unimplemented
# keywords) — see conformance/jsonschema/MATRIX.md.
add_executable(test_jsonschema_suite conformance/jsonschema/js_suite.c)
target_link_libraries(test_jsonschema_suite PRIVATE yeptris_static)
target_include_directories(test_jsonschema_suite PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
yeptris_set_warnings(test_jsonschema_suite)
add_test(
    NAME jsonschema-suite
    COMMAND test_jsonschema_suite
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/jsonschema/carries.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/jsonschema/tests
)
add_test(
    NAME jsonschema-suite-optional
    COMMAND test_jsonschema_suite
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/jsonschema/carries.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/jsonschema/optional
)
add_test(
    NAME fuzz-roundtrip-corpus
    COMMAND fuzz_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)
add_test(
    NAME fuzz-feed-corpus
    COMMAND fuzz_feed ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)
add_test(
    NAME fuzz-feed-regression
    COMMAND fuzz_feed ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/feed
)
add_test(
    NAME fuzz-transcode-corpus
    COMMAND fuzz_transcode ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

if(YEPTRIS_ENABLE_FUZZING)
    set(YEP_FUZZ_HARNESS_LINK_OPTIONS -fsanitize=fuzzer)
    add_executable(fuzz_parse_lf fuzz/fuzz_parse.c)
    target_link_libraries(fuzz_parse_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_parse_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_parse_lf)
    target_link_options(fuzz_parse_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_roundtrip_lf fuzz/fuzz_roundtrip.c)
    target_link_libraries(fuzz_roundtrip_lf PRIVATE yeptris_static)
    yeptris_set_warnings(fuzz_roundtrip_lf)
    target_link_options(fuzz_roundtrip_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_feed_lf fuzz/fuzz_feed.c)
    target_link_libraries(fuzz_feed_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_feed_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_feed_lf)
    target_link_options(fuzz_feed_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_transcode_lf fuzz/fuzz_transcode.c)
    target_link_libraries(fuzz_transcode_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_transcode_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_transcode_lf)
    target_link_options(fuzz_transcode_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    if(YEPTRIS_LIBYAML_ROOT)
        # differential under fuzzing: libyaml linked into the harness
        add_executable(fuzz_diff_lf fuzz/fuzz_diff.c fuzz/family_classify.c)
        target_link_libraries(fuzz_diff_lf PRIVATE diff_core)
        target_include_directories(fuzz_diff_lf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/fuzz)
        yeptris_set_warnings(fuzz_diff_lf)
    target_link_options(fuzz_diff_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})
    
    add_executable(fuzz_cbor_lf fuzz/fuzz_cbor.c)
    target_link_libraries(fuzz_cbor_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_cbor_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_cbor_lf)
    target_link_options(fuzz_cbor_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_marshal_lf fuzz/fuzz_marshal.c)
    target_link_libraries(fuzz_marshal_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_marshal_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_marshal_lf)
    target_link_options(fuzz_marshal_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_validate_lf fuzz/fuzz_validate.c)
    target_link_libraries(fuzz_validate_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_validate_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_validate_lf)
    target_link_options(fuzz_validate_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})

    add_executable(fuzz_schema_compile_lf fuzz/fuzz_schema_compile.c)
    target_link_libraries(fuzz_schema_compile_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_schema_compile_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_schema_compile_lf)
    target_link_options(fuzz_schema_compile_lf PRIVATE ${YEP_FUZZ_HARNESS_LINK_OPTIONS})
endif()
endif()

# ---- JSONTestSuite under YAML semantics (TODO.impl/08C) ---------------
add_executable(flow_direct_diff flow/flow_direct_diff.c)
target_include_directories(flow_direct_diff PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/flow)
target_link_libraries(flow_direct_diff PRIVATE yeptris_static)
target_include_directories(flow_direct_diff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(flow_direct_diff)
add_test(
    NAME flow-direct-diff
    COMMAND flow_direct_diff ${CMAKE_CURRENT_SOURCE_DIR}/conformance/data/yaml-test-suite/src
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/data/psych-pure
            ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

add_executable(block_pair_diff flow/block_pair_diff.c)
target_link_libraries(block_pair_diff PRIVATE yeptris_static)
target_include_directories(block_pair_diff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris
    ${CMAKE_CURRENT_SOURCE_DIR}/flow)
yeptris_set_warnings(block_pair_diff)
add_test(
    NAME block-pair-diff
    COMMAND block_pair_diff ${CMAKE_CURRENT_SOURCE_DIR}/conformance/data/yaml-test-suite/src
            ${CMAKE_CURRENT_SOURCE_DIR}/conformance/data/psych-pure
            ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

add_executable(test_jsonsuite flow/jsonsuite.c)
target_link_libraries(test_jsonsuite PRIVATE yeptris_static)
target_include_directories(test_jsonsuite PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
yeptris_set_warnings(test_jsonsuite)
add_test(
    NAME json-suite
    COMMAND test_jsonsuite
            ${CMAKE_SOURCE_DIR}/test/conformance/data/json-test-suite/test_parsing
            ${CMAKE_CURRENT_SOURCE_DIR}/flow/jsonsuite-expected.txt
)
add_test(
    NAME json-suite-strict
    COMMAND test_jsonsuite --strict
            ${CMAKE_SOURCE_DIR}/test/conformance/data/json-test-suite/test_parsing
            ${CMAKE_CURRENT_SOURCE_DIR}/flow/jsonsuite-expected.txt
)

# tape-vs-DOM differential (TODO.restructure/85): every corpus
# document through parse_json AND parse_json_tape — statuses and the
# record stream against the tree
add_executable(tape_diff flow/tape_diff.c)
target_link_libraries(tape_diff PRIVATE yeptris_static)
target_include_directories(tape_diff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris
    ${CMAKE_SOURCE_DIR}/src/include)
yeptris_set_warnings(tape_diff)
add_test(
    NAME tape-diff
    COMMAND tape_diff ${CMAKE_CURRENT_SOURCE_DIR}/conformance/data/json-test-suite/test_parsing
)

# ---- emitter differential vs libyaml's own emission (TODO.impl/17B) ---
add_executable(test_emit_diff port/libyaml/emit_diff.c)
target_link_libraries(test_emit_diff PRIVATE yeptris_static)
target_include_directories(test_emit_diff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris
    ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml)
yeptris_set_warnings(test_emit_diff)
add_test(
    NAME emit-diff
    COMMAND test_emit_diff ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
            ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/ledger.txt
)

# One-time regeneration tool: builds only when a libyaml checkout is
# provided (-DYEPTRIS_LIBYAML_ROOT=$HOME/src/external/libyaml).
set(YEPTRIS_LIBYAML_ROOT "" CACHE PATH "libyaml checkout for golden regeneration")
if(YEPTRIS_LIBYAML_ROOT)
    add_executable(gen_libyaml_golden
        port/libyaml/gen_golden.c
        conformance/suite.c
    )
    target_include_directories(gen_libyaml_golden PRIVATE
        ${CMAKE_SOURCE_DIR}/src/yeptris
        ${CMAKE_SOURCE_DIR}/test/port/libyaml
        ${YEPTRIS_LIBYAML_ROOT}/include
    )
    target_link_libraries(gen_libyaml_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_libyaml_golden)

    add_library(diff_core STATIC port/libyaml/diff_core.c)
    target_include_directories(diff_core PUBLIC
        ${CMAKE_SOURCE_DIR}/src/yeptris
        ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(diff_core PUBLIC yeptris_static
        ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(diff_core)

    add_executable(yepdiff ../tools/yepdiff.c)
    target_include_directories(yepdiff PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris
        ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(yepdiff PRIVATE diff_core)
    yeptris_set_warnings(yepdiff)

    add_executable(fuzz_diff fuzz/fuzz_diff.c fuzz/family_classify.c)
    target_include_directories(fuzz_diff PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml ${YEPTRIS_LIBYAML_ROOT}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/fuzz)
    target_link_libraries(fuzz_diff PRIVATE diff_core)
    target_compile_definitions(fuzz_diff PRIVATE YEP_FUZZ_STANDALONE)
    yeptris_set_warnings(fuzz_diff)

    add_test(
        NAME fuzz-diff-corpus
        COMMAND fuzz_diff
                --ledger=${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/ledger.txt
                ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
    )

    # the #1310 classifier self-test: every soup corpus seed classifies
    # (the law derivation reproduces both actual dumps byte-exact); the
    # dump-tamper firewall (any one-byte change to either dump declines)
    # and the law-violating input mutants (spliced closers, broken
    # aliases, restored comments, tag/anchor/nesting splices) must all
    # stay UNKNOWN
    add_test(
        NAME fuzz-diff-family-classify
        COMMAND fuzz_diff
                --family-classify
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse
    )

    # the r104 nightly's composition (anchor-charset x PS-led plain
    # folds, 16 sites): the carry's forced-additive walk must classify
    # it known, not abort
    add_test(
        NAME fuzz-diff-r104-composition
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-ps-crcr-additive-deep.bin
    )

    # the r105 nightly's composition (anchor-charset x a PS-mixed break
    # RUN): the run arm walks position-wise — additive while the
    # escape counts match, the round-72 replacement one-short — and
    # must classify it known without un-classifying the r104 seed
    add_test(
        NAME fuzz-diff-r105-composition
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-ps-fold-replace-run.bin
    )

    # the r106 nightly's composition (flow-? explicit-key x flow-:
    # value-split, "{\n?{?\n:cic l},\n?M,\n?:}"): the qflow walk's
    # empty-key colon fold must explain BOTH family members in one
    # pass and classify it known without un-classifying r104/r105
    add_test(
        NAME fuzz-diff-r106-composition
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-q-nested-indicator.bin
    )

    # the r106 carry's own composition check (the bounded session's
    # mutant, "{\n?:-.?:M,\n?:}\n"): the ?-colon-value fold plus the
    # plain-? key in ONE walk — the qcolon walk's new ?X twin; must
    # stay known beside the r106 artifact
    add_test(
        NAME fuzz-diff-r106-qdash-split
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qdash-value-split.bin
    )

    # the r107 nightly's composition (crash-7fda24ee, a 25k dump whose
    # one content BOM sits LATE): the BOM-strip probe gated on the dump
    # TAIL but memcpy'd the dump HEAD into an 8k stack buffer -- the
    # whole-length gate must classify it known without un-classifying
    # the earlier seeds
    add_test(
        NAME fuzz-diff-r107-bom-late-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-content-late-head-overflow.bin
    )

    # the r107 validation session's second candidate (the "{?#}" family,
    # a SPLIT: libyaml's flow-? KEY token glues the '#' into a comment
    # and rejects what yeptris's plain-'?' law accepts) — the quote-out
    # split arm must classify both the minimal artifact and its
    # two-member composition, without un-classifying the r10x seeds
    add_test(
        NAME fuzz-diff-qhash-glued-comment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-q-hash-glued-comment.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-q-hash-glued-comment-pair.bin
    )

    # the r108 nightly's composition (the glued-tag x alias-name
    # charset, crash-54b5b756): the artifact's "!!i&a:" is ONE tag in
    # both engines — the '&' rides the ns-uri-char suffix — and "*a:"
    # names "a:" per the suite (2SXE). The charset walk split inside
    # the tag URI (corrupting the copy, so the verify's early return
    # skipped the r95 solo retries) and declined its own alias split;
    # tag-URI bytes are not name runs, and a glued apostrophe rides the
    # suffix too (the 6-byte drift the same session's minimizer found).
    # All three shapes — the 128-byte artifact, the 12-byte glued-'&'
    # minimal, the 6-byte glued-quote drift — must classify known
    add_test(
        NAME fuzz-diff-r108-composition
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/alias-charset-glued-tag.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/alias-charset-glued-tag-min.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/alias-charset-glued-quote.bin
    )

    # the r115 nightly's seed (crash-01361e26, the glued-?# family's
    # accept/accept verdict: libyaml empties the key at the glued '#',
    # yeptris's suite-pinned plain-'?' law keeps the run): the
    # structural emptied-key classifier must claim it — the quote-out
    # transform cannot (quoting freezes the fold, breaking libyaml's
    # line structure) — without un-classifying r104-r108
    add_test(
        NAME fuzz-diff-r115-qflow-differ
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-differ.bin
    )

    # the r116 nightly's seed (crash-e55e36dc: the glued-?#/#| family
    # with TAB-bearing plain keys — accept/accept-equal on NEON, a
    # YD_DIFFER the classifiers do not claim on x86): the pin rides
    # green where the engines agree and dumps both event streams
    # (then aborts) where the AVX2 lane's verdict differs — the CI
    # ubuntu legs are the referee for the platform-dependent carve.
    add_test(
        NAME fuzz-diff-r116-qflow-x86-differ
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-differ-r116.bin
    )

    # the r117 nightly's mutated seed (crash-383474ba): libyaml's
    # glued-# comment-kill SPLITS a multi-line plain yeptris folds —
    # the continuation line becomes its own scalar whose content is a
    # suffix of yeptris's single folded scalar
    add_test(
        NAME fuzz-diff-r117-qflow-split
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-split-r117.bin
    )

    # the r118 nightly's seed (crash-8b31932e): NEL (U+0085) inside a
    # plain scalar — libyaml (1.1) folds it to the break space, the
    # 1.2 break set keeps it as raw content (the quoted twin is the
    # round-47 carry; this is the plain shape)
    add_test(
        NAME fuzz-diff-r118-plain-nel-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/plain-nel-fold-r118.bin
    )

    # the r119 nightly's mutated seed (crash-4aa4023b): RUNS of NEL —
    # libyaml folds k>=2 consecutive NELs into ONE escaped break (a backslash-n pair in the dump) while yeptris keeps every raw C2 85
    add_test(
        NAME fuzz-diff-r119-plain-nel-run
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/plain-nel-run-r119.bin
    )

    # the r120 nightly's seed (crash-b1d3d17a): the glued-# family's
    # tail-member shape — libyaml's comment-kill consumes the tail
    # entry entirely (straight to the closer) while yeptris keeps it
    # as content per the suite-pinned plain-'?' law
    add_test(
        NAME fuzz-diff-r120-qflow-tail-member
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-tail-member-r120.bin
    )

    # the r121 nightly's seed (crash-4aa4023b's successor): the
    # member-PERMUTATION gray input — both engines accept but the
    # streams are reordered; carried by the exact-bytes claim (the
    # ledger's by-input discipline), never a shape rule
    add_test(
        NAME fuzz-diff-r121-permuted-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/permuted-gray-r121.bin
    )

    # the r122 nightly's seed (crash-b1d3d17a's sibling): a TRAILING
    # NEL — a break at end-of-input is not a fold, so libyaml's dump
    # drops it entirely where yeptris keeps the raw C2 85
    add_test(
        NAME fuzz-diff-r122-plain-nel-eof
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/plain-nel-eof-r122.bin
    )

    # the r123 nightly's seed: a ':' inside a glued !-run is tag-URI
    # content (r108) -- the colon-led plain law opened a phantom plain
    # that swallowed the next line's alias, and the r54 alias-charset
    # family's split never armed; the classifier now guards the law on
    # tag_run (both walks) and the split reconciles the skeletons
    add_test(
        NAME fuzz-diff-r123-tag-run-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/tag-run-colon-alias-r123.bin
    )

    # the r124 nightly's seed: the glued-# comment-kill ate the run
    # ENTIRELY mid-stream (no emptied pair survives) -- yeptris's
    # killed-content keys ride '#'-bearing vals the walk skips so the
    # survived continuation re-pairs downstream
    add_test(
        NAME fuzz-diff-r124-hash-killed-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-hash-killed-key-r124.bin
    )

    # the r125 nightly's seed: both skeletons carry the alias at the
    # same EVENT position, but yeptris's plain-'?' pairs sit BEFORE it
    # where libyaml's kill emptied them away -- the walk skips yeptris's
    # VALs (VALs only) until its own =ALI line, which must match
    # byte-exact
    add_test(
        NAME fuzz-diff-r125-alias-realign
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-alias-realign-r125.bin
    )

    # the r127 nightly's seed: the killed-content residue can be
    # '#'-less (the kill sat on a later line of the composed plain) --
    # the killed-key arm is content-agnostic; the skeleton must still
    # fully re-align or the claim fails
    add_test(
        NAME fuzz-diff-r127-hash-killed-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-hash-killed-key-r127.bin
    )

    # the r128 nightly's seed: LONE raw 0x85 bytes (not C2 85 pairs)
    # -- yeptris keeps them raw where libyaml's fold maps singles to
    # spaces, k-runs to k-1 escaped breaks, and eats runs entirely at
    # fold boundaries; the walk also under-consumed k>2 escaped runs
    add_test(
        NAME fuzz-diff-r128-nel-lone-run
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-lone-run-r128.bin
    )

    # the r129 nightly's seed: a 2938-byte mutation soup (anchor-charset
    # x tag-run x digit-run compositions, hundreds of splits) riding the
    # EXACT-BYTES gray claim -- permutation beyond the transform caps is
    # not soundly generalizable
    add_test(
        NAME fuzz-diff-r129-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r129.bin
    )

    # the r130 nightly's seed: the qflow family's STRUCTURAL
    # permutation (libyaml re-pairs the killed continuation under a
    # +MAP wrapper) -- the exact-bytes gray claim until the
    # structural-realign arm lands (#945)
    add_test(
        NAME fuzz-diff-r130-structural-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/structural-gray-r130.bin
    )

    # the r131 nightly's seed: a NEAR-TWIN of r130's structural shape
    # (30 vs 31 bytes) -- the exact-bytes pattern bled per-mutation, so
    # the STRUCTURAL-REALIGN ARM (#945) landed instead: skip yeptris's
    # killed VAL + libyaml's opener line; the owed closer re-aligns at
    # struct_skips
    add_test(
        NAME fuzz-diff-r131-structural-arm
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/structural-gray-r131.bin
    )

    # the r132 nightly's seeds (#949): the glued-? x glued-# COMPOSITION
    # -- the entry kill (libyaml comment-kills the ?#-led seq entry whose
    # next member BOTH walks open; only yeptris's VAL steps aside via the
    # shared-opener tell) and the tail wrapper (libyaml's glued-? opens
    # an explicit-key map around the anchored-empty key, claimed by the
    # structural arm). yeptris correct per the plain-'?' and kill laws.
    add_test(
        NAME fuzz-diff-r132-qcomp-shared-opener
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-shared-opener-r132.bin
    )
    add_test(
        NAME fuzz-diff-r132-qcomp-tail-anchored-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-tail-anchored-key-r132.bin
    )

    # the r133 nightly's seed (#951): a plain continuation line whose
    # "!<" opened a PHANTOM verbatim tag in the break normalizer -- the
    # tok_start armed by the folded NEL carried no memory of the
    # before-break byte, so the continuation's "!<" read as props and
    # every following NEL rode unnormalized (raw 0x85 into the scalar).
    # The bb gate keys the open on the byte before the fold: plain
    # content blocks it, an indicator (':') keeps it.
    add_test(
        NAME fuzz-diff-r133-phantom-verbatim
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-phantom-verbatim-r133.bin
    )

    # the r135 nightly's seeds: the flow-'?' KEY-token law's TAG-RIDING
    # twin -- libyaml eats the '?' AND its glued '!' tag plus the blanks
    # after it (none ride the dump), yeptris's suite-pinned plain keeps
    # the whole run as key content; the soup pairs that shape beside a
    # pure '?' twin, which declines the drop-'?' rewrite's byte-strict
    # verify -- the qflow walk's bounded tag-riding arm ('?' then '!'s,
    # spaces, escaped blanks; suffix-exact) carries both
    add_test(
        NAME fuzz-diff-r135-qbang-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-r135-qbang-soup.bin
    )
    add_test(
        NAME fuzz-diff-r135-qbang-twin
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-r135-qbang-twin.bin
    )

    # the r136 nightly's seed: the glued-?! TAIL WRAPPER -- libyaml's
    # flow-'?' KEY-token law fires on ANY follower, so the glued '?'
    # opens an explicit-key map and the glued '!' rides as its invisible
    # non-specific tag (the blanks after the fold carry it away); '-8'
    # becomes the key, the value empty. yeptris's suite-pinned plain-'?'
    # law keeps '?! -8' ONE plain scalar (the break folds). The pass-1
    # marker now scores '?!' beside '?#'; the r130/r131 structural arm
    # consumes the wrapper + banks the closer, the L-only vals and the
    # empty VAL re-align -- the arm chain claimed it unchanged.
    add_test(
        NAME fuzz-diff-r136-qbang-wrapper
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-qbang-wrapper-r136.bin
    )

    # the r137 nightly's seed: the r121 permutation family's TAIL
    # COMPOSITION -- a 997-byte glued-'?' x glued-'#' x tag-URI x CR-run
    # soup whose first ~31 members realign through the arm chain's
    # bounded skips, then libyaml's comment-glue law eats every remaining
    # '?#'-led member as a comment and CLOSES the map 28 pairs before
    # yeptris (the suite-pinned plain-'?' law rides all 59 pairs to the
    # '}'). A yeptris-only tail of any size would generalize member-count
    # skew -- the exact signature of genuine member-drop bugs -- so the
    # prefix-minimal artifact rides the EXACT-BYTES gray claim (the
    # r121/r129 pattern), not an arm.
    add_test(
        NAME fuzz-diff-r137-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r137.bin
    )

    # the r138 nightly's seed: the alias-swallow TAIL COMPOSITION -- a
    # 66-byte glued-'?' x glued-'#' x lone-backtick x explicit-key-alias
    # soup. yeptris's suite-pinned plain-'?' law rides 8 pairs (one
    # ALI-keyed) to the '}'; libyaml's comment-glue law empties every
    # member but the final '<' (4 pairs) and its second run's comment
    # swallows the '\t*L:' explicit key THROUGH the alias -- yeptris
    # emits an =ALI libyaml has no counterpart for, and every completing
    # skeleton composition needs a yeptris-only tail after libyaml's
    # '-MAP' closes (member-count skew -- the exact signature of genuine
    # member-drop bugs, ruled unsound to generalize by the r137
    # verdict). An 'yeptris =ALI vs libyaml anything' arm would mask
    # genuine alias-drop bugs, so the prefix-minimal artifact rides the
    # EXACT-BYTES gray claim (the r121/r129/r137 pattern), not an arm.
    add_test(
        NAME fuzz-diff-r138-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r138.bin
    )

    # the r139 nightly's seed: the alias-swallow TAIL COMPOSITION's
    # REDUCED SIBLING -- a 35-byte cut of the r138 soup (the same
    # glued-'?' "?#?#|#|+`" head and '\t*L:' explicit-key alias pair;
    # the head fold and three middle members dropped, the tail plain
    # unbackticked). yeptris's suite-pinned plain-'?' law rides 4 pairs
    # (one ALI-keyed) to the '}'; libyaml's comment-glue law empties
    # the head pair and swallows EVERYTHING from the lone-backtick
    # plain THROUGH the '\t*L:' alias to the tail -- the stream closes
    # after ONE emptied pair. The bisect over every prefix found no
    # shorter diverging cut, and the first unexplained walk line is
    # yeptris's '=ALI *L' against libyaml's '-MAP': an 'yeptris =ALI vs
    # libyaml anything' arm would mask genuine alias-drop bugs, and
    # every completing skeleton composition needs a yeptris-only tail
    # after libyaml's '-MAP' closes (member-count skew -- the exact
    # signature of genuine member-drop bugs, ruled unsound to
    # generalize by the r137/r138 verdicts). The prefix-minimal
    # artifact rides the EXACT-BYTES gray claim (the
    # r121/r129/r137/r138 pattern), not an arm.
    add_test(
        NAME fuzz-diff-r139-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r139.bin
    )

    # the r140 nightly's seed: the alias-swallow TAIL COMPOSITION's
    # SIBLING -- a 33-byte cut of the r139 soup (the same glued-'?'
    # head and '\t*L:' explicit-key alias pair; the '+' and the first
    # backtick of the r139 backtick pair dropped, so the head plain
    # folds clean and ONE lone backtick plain rides before the alias).
    # yeptris's suite-pinned plain-'?' law rides 4 pairs (one
    # ALI-keyed) to the '}'; libyaml's comment-glue law empties the
    # head pair and swallows EVERYTHING from the lone-backtick plain
    # THROUGH the '\t*L:' alias to the tail -- the stream closes after
    # ONE emptied pair. The bisect over every prefix found no shorter
    # diverging cut, and the first unexplained walk line is yeptris's
    # '=ALI *L' against libyaml's '-MAP': an 'yeptris =ALI vs libyaml
    # anything' arm would mask genuine alias-drop bugs, and every
    # completing skeleton composition needs a yeptris-only tail after
    # libyaml's '-MAP' closes (member-count skew -- the exact signature
    # of genuine member-drop bugs, ruled unsound to generalize by the
    # r137/r138/r139 verdicts). The prefix-minimal artifact rides the
    # EXACT-BYTES gray claim (the r121/r129/r137/r138/r139 pattern),
    # not an arm.
    add_test(
        NAME fuzz-diff-r140-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r140.bin
    )

    # the r141 nightly's seed: the alias-swallow TAIL COMPOSITION's
    # SIBLING -- a 26-byte cut of the r140 soup (the same glued-'?'
    # head and '\t*L:' explicit-key alias pair; r140's tail plain
    # member dropped entirely, so the closer is a NEWLINE-LED ')' --
    # ONE plain after the input's single break). yeptris's
    # suite-pinned plain-'?' law rides 4 pairs (one ALI-keyed) to the
    # '}'; libyaml's comment-glue law swallows EVERYTHING from the
    # glued '#' THROUGH the '\t*L:' alias to the input's single break
    # -- and the pending explicit key takes the NEXT line's ')' as its
    # content, so the stream closes after ONE pair whose content is
    # ')'. The bisect over every prefix found no shorter diverging
    # cut, and the first unexplained walk line is yeptris's '=ALI *L'
    # against libyaml's '-MAP': an 'yeptris =ALI vs libyaml anything'
    # arm would mask genuine alias-drop bugs, and every completing
    # skeleton composition needs a yeptris-only tail after libyaml's
    # '-MAP' closes (member-count skew -- the exact signature of
    # genuine member-drop bugs, ruled unsound to generalize by the
    # r137/r138/r139/r140 verdicts). The prefix-minimal artifact rides
    # the EXACT-BYTES gray claim (the
    # r121/r129/r137/r138/r139/r140 pattern), not an arm.
    add_test(
        NAME fuzz-diff-r141-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r141.bin
    )

    # the r151 nightly's seed (crash-a3c7e5d7, run 42085146127, main
    # 3e7bedf1): the alias-swallow TAIL COMPOSITION's ONE-BYTE-DELETION
    # SIBLING -- the r138 soup with the first member's second '?' dropped
    # ("?#?#|#|+`" -> "?##|#|+`"), 65 bytes, every other byte identical
    # (the r138 battery flipped bytes; a DELETION inside the doubled '?'
    # head sat outside its neighborhood). yeptris's suite-pinned plain-'?'
    # law rides 8 pairs (one ALI-keyed) to the '}'; libyaml's comment-glue
    # law empties every member but the final '<' -- 4 pairs -- and swallows
    # the "\t*L:" explicit key THROUGH the alias, so yeptris's '=ALI *L'
    # has NO libyaml counterpart at all. The walk's emptied-key arm pairs
    # members 1-3 one-to-one, then dies mid-stream at yeptris's '=ALI *L'
    # against libyaml's '=VAL : <': the r142 terminal-tail arm never
    # reaches (libyaml still rides '<'), the r144 residue arm declines
    # (the alias survives NOWHERE in libyaml's dump), and an 'yeptris =ALI
    # vs libyaml anything' arm would mask genuine alias-drop bugs while
    # every completing skeleton composition needs a yeptris-only tail
    # after libyaml's '-MAP' closes (member-count skew -- the exact
    # signature of genuine member-drop bugs, ruled unsound to generalize
    # by the r137/r138 verdicts and re-affirmed by r144). The bisect over
    # every prefix found no shorter diverging cut; the prefix-minimal
    # artifact rides the EXACT-BYTES gray claim (the
    # r121/r129/r137/r138/r139/r140/r141 pattern), not an arm.
    add_test(
        NAME fuzz-diff-r151-soup-gray
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-gray-r151.bin
    )

    # the r142 nightly's seed (crash-9c3fb9f1, run 41961599250, main
    # 68f74daa): the soup family's TERMINAL TAIL COMPOSITION -- the walk
    # dies only on the yeptris-only tail once libyaml's skeleton is
    # spent (six consecutive nightly rounds r137-r141 rode exact-bytes
    # pins, one byte flip per round: a new red forever). The
    # TERMINAL-TAIL arm (#949 family, the strategic round) rides it:
    # libyaml's remaining lines are closers only, yeptris's tail is
    # kept-content members (=VAL/=ALI, <=64) followed by libyaml's
    # closers verbatim; any structure in the tail declines, and a
    # genuine mid-stream member drop still aborts (the negative
    # composition "{a: 1, b: {c: 2, ?#d: [x, y],}, e: 5}" -- libyaml
    # loses member d and its nested seq mid-stream with (e: 5) riding
    # after in BOTH streams -- aborts at the walk level with the arm in
    # place). The r137-r141 pins stay pinned; consults are OR'd.
    add_test(
        NAME fuzz-diff-r142-soup-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-tail-r142.bin
    )

    # the r143 nightly's seed (crash-d4f874ee.., run 41975575045, main
    # 531ad570): the r132 composition's '+#'-bearing wrapper variant,
    # DOUBLED ("?+#\r\n&m-7" and "?+#\r\n&m-"). With '+' the '#' no
    # longer glues to the '?' -- libyaml's post-'?'-token comment-glue
    # never fires, but its KEY-token law still opens an explicit-key
    # wrapper whose KEY carries the run as REAL content ("=VAL : +#
    # &m-7") -- a salvage block (+MAP / content KEY / empty VAL / -MAP)
    # per killed member where yeptris's suite-pinned plain-'?' law keeps
    # one plain. DOUBLED, the line-wise pairing dies (member 2 pairs
    # against wrapper 1's KEY; wrapper 2's opener finds yeptris's
    # closer). The structural arm's WRAPPER RIDE claims the well-formed
    # block when its KEY is a non-empty suffix of yeptris's killed
    # member and does not suffix-pair the next surviving member (the
    # r130 suffix-pair shape keeps the line-wise path; the r132
    # comment-glue KEY carries a fold-space and never suffixes).
    # Bounded: the fixed four-line block, shared skip cap;
    # prefix-minimal at the full 82 bytes.
    add_test(
        NAME fuzz-diff-r143-qplus-wrapper
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-qplus-wrapper-r143.bin
    )

    # the r144 nightly's seed (crash-fe4ea5a7, run 41995393016, main
    # 21088fcb): the soup family's ALIAS-SWALLOW RE-PAIRING -- libyaml's
    # comment-glue swallows the killed members THROUGH the "\t*L:"
    # alias-bearing explicit-key head and RE-PAIRS the pending KEY token
    # with the next content ("( `", the swallowed member's VALUE), so
    # yeptris's "=ALI *L" event has no counterpart AT ITS POSITION while
    # the same alias line rides libyaml's dump LATER (the second "\t*L:"
    # member) -- pair ordering/re-pairing, not the member-count skew the
    # r138-r141 exact-bytes claims carry (those shapes have NO libyaml
    # alias at all). The r125 alias-realign arm spends libyaml's twin on
    # yeptris's FIRST alias, leaving the second unexplained against the
    # re-paired content; the ALIAS-SWALLOW RESIDUE arm rides it gated on
    # the alias surviving libyaml's dump elsewhere (a genuine alias-drop
    # declines and still aborts). Family-gated (glued ?#), bounded by the
    # shared skip cap; prefix-minimal at the full 36 bytes.
    add_test(
        NAME fuzz-diff-r144-alias-repair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-alias-r144.bin
    )

    # the r145 nightly's seed (crash-ca766c70, run 42003383477, main
    # 417fab59): the glued-?# composition behind a MID-PLAIN QUOTE --
    # "- [trueWft?\",?##...,\n ]" carries a lone '"' glued to the
    # trueWft? plain, and both engines pin it as CONTENT ("=VAL :
    # trueWft?\"" on BOTH sides, the catch-85 word-start law), but the
    # marker's naive quote tracker opened a phantom quote there that
    # never closes, so the glued '?#' after the ',' never scored and
    # the whole r132-r144 arm chain sat behind a dead gate. The pass-1
    # marker now tracks the PLAIN RUN: a quote opens only where a node
    # can start (not mid-plain, after a separated indicator/props
    # position, after a flow boundary); fold blanks ride, flow plains
    # fold across breaks, block lines end at theirs, a blank-led
    # comment ends the node. The existing arms claim the composition
    # unchanged (structural skip + closer bank -> emptied-key ->
    # L-only empty -> closer bank; libyaml's comment-glue law empties
    # the rest of the line into a +MAP/empty/empty/-MAP wrapper where
    # yeptris keeps the ?#- and `|-shaped members). No arm or cap
    # changed; the genuine alias-drop negative composed with the same
    # phantom-quote shape still aborts. Bounded by every existing cap;
    # prefix-minimal at the full 37 bytes (the ] closer is
    # load-bearing).
    add_test(
        NAME fuzz-diff-r145-midplain-quote
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-midplain-quote-r145.bin
    )

    # the r146 nightly's seed (crash-50405bba, run 42013806681, main
    # 22e2b1ac): the soup family under libFuzzer's REAL multi-byte
    # mutation operators -- the r137 seed with a 1-byte URI insertion
    # ("tag:ytag:yaml.org" -> "tag:ytag:yam2l.org") PLUS a 730-byte
    # splice of its own soup content (997 -> 1728 bytes). The r137
    # exact-bytes claim cannot match, and the walk dies on the
    # TERMINAL TAIL the #971 arm shipped with a FIXED 64-content ride
    # cap: the splice doubles the member count, yeptris's kept-content
    # tail carries 96 members to libyaml's closers, the cap declines,
    # and the per-line arms run out of shared skip budget (96 lines
    # against ~5 headroom). The TERMINAL-TAIL arm's ride cap now rides
    # the classifier's own input bound (every kept-content member
    # consumed >=1 input byte, so ride <= size is provable; size <=
    # 8192 is already gated at entry). The load-bearing gates are
    # unchanged: terminality (libyaml's remainder closers-only), a
    # kept-content-only yeptris remainder (=VAL/=ALI, no structure),
    # the byte-exact closer suffix, one-shot exhaustion; the
    # structure-tail negative (",?#z: {q: 1}" planted in this
    # artifact's tail region) still aborts. The #971 convergence claim
    # is upgraded with the artifact: its 20/20 uniform single-byte
    # flips never exercised the insertion/splice operators this catch
    # rode; post-fix, 20 single-byte flips PLUS 20 multi-byte
    # insertions/substitutions/deletions of the r137 seed all classify.
    add_test(
        NAME fuzz-diff-r146-splice-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-splice-tail-r146.bin
    )

    # the r147 nightly's seed (run 42029425465, main 9144fdf7): the
    # #836/#951 phantom-verbatim class's THIRD arming shape, past the
    # r133 bb gate. The root plain's "-  " line ends LF + NEL; the
    # fold arms bb='\n' (the break run's own tail, in the r133
    # indicator set), so the continuation's "!<" opened the phantom
    # verbatim and every later NEL rode unnormalized -- and the plain
    # piece's multibyte check probes only the FIRST C2/E2 lead, so the
    # yen-sign lead (C2 A5, not a break) SHIELDED the NELs behind it:
    # raw C2 85 rode into the value where libyaml's READ_LINE spells
    # NEL -> LF and folds. The r147 law carries plain_open (a plain
    # scalar VALUE word in progress: set by non-indicator content
    # bytes, preserved across breaks, cleared at the indicator-armed
    # slots and document markers) -- a fold-armed tok_start over an
    # open plain is a continuation line whose "!<" is content, never
    # props. The legal props forms ("key: <NEL>!<a>", "- <NEL>!<a>")
    # keep their opens byte-identically (plain_open is 0 there). The
    # value post-fix folds to libyaml's exactly.
    add_test(
        NAME fuzz-diff-r147-plain-open-phantom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-phantom-verbatim-r147.bin
    )

    # the r148 nightly's seed (crash-9b423bc7, run 42043432048, main
    # 99803f9a): the soup family's &m-ANCHORED-KEY wrapper. The r132
    # comment-glue law consumed the glued '?##' through libyaml's KEY
    # token and re-paired the pending KEY with the continuation's
    # ANCHOR, so each killed member rides a salvage wrapper whose KEY
    # event renders PROPS-ONLY ("=VAL : &m " — anchor in the props
    # region, empty value) where yeptris's suite-pinned plain-'?' law
    # (HM87/652Z) keeps "?## &m" ONE plain folded across the break.
    # The r143 wrapper ride declined the shape on a rendering
    # technicality: its KEY-suffix gate compared the UNstripped region
    # ("&m ", props + the render's separator blank) against yeptris's
    # tail (" &m", fold space + the name) — shifted by exactly that
    # byte — so the walk died at the SECOND wrapper's opener ("-SEQ"
    # vs "+MAP" past the shared skip budget). The ride's gate now
    # strips one trailing blank from a plain-styled KEY line (a plain
    # cannot end in a blank as content; the prefix check pins the
    # style against quoted keys whose content MAY end in a blank) and
    # the same suffix/pairs_next compares ride the stripped props
    # region. The byte-exact residue gate, the four-line block, the
    # !pairs_next routing and the shared skip cap are unchanged. The
    # artifact composes the whole r132-r147 arm chain: three '?##|'
    # members ride the shared-opener skips, the PS-fold key drift
    # ("/\n\n51" vs "/\n\n<PS>51") rides the r124 content-agnostic
    # skips, and both &m wrappers ride the extended ride. Post-fix the
    # battery converges: 30 mutants (20 single-byte flips + 10
    # multi-byte ins/sub/del), 0 DIFFER-aborts (7 pre-fix DIFFERs all
    # classify; the 21 raw-byte splits are the suite-gray libyaml-
    # rejects family, pre/post identical).
    add_test(
        NAME fuzz-diff-r148-anchored-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-anchored-key-r148.bin
    )

    # the r149 nightly's seed (crash-96123edf, run 42061122149, main
    # a7cb5ae3): the r148 ANCHORED-KEY shape riding CR breaks, plus one
    # new law: the FIRST wrapper's KEY is the RE-PAIRED plain tail.
    # libyaml's post-'?'-token comment-glue ate the glued '?##' THROUGH
    # the CR and re-paired the pending KEY token with the NEXT member's
    # plain ("b:inary" — the r144 re-pairing law, now on the KEY event),
    # so the wrapper's KEY renders plain-content where yeptris's
    # suite-pinned plain-'?' law (HM87/652Z) keeps BOTH members: the
    # killed head "?##|" AND the folded tail ":binary b:inary" whose
    # content ENDS with the KEY. The r143/r148 wrapper ride declined
    # (its gate demands the KEY be the killed member's own residue) and
    # the r130 line-wise path let the wrapper's own empty VAL and
    # closer graze the FOLLOWING members — the emptied-key arm ate
    # member 3, the r120 close arm ate member 4 — dying at the second
    # wrapper's opener ("-SEQ" vs "+MAP"). The re-paired-KEY ride now
    # claims the whole wrapper plus both members when the KEY (stripped
    # of the r148 render separator when props-only) is a byte-exact
    # SUFFIX of yeptris's next surviving member's content — an invented
    # key declines — while carrying none of the killed member's tail;
    # the fixed four-line block and the shared skip cap are unchanged.
    # The two trailing &m wrappers then ride the existing r148 ride.
    # Post-fix the battery converges: 30 mutants (20 single-byte flips
    # + 10 multi-byte ins/sub/del), 0 DIFFER-aborts (7 pre-fix DIFFERs
    # all classify; the 21 raw-byte YEPTRIS-ONLY-ACCEPTs are the
    # suite-gray libyaml-rejects family, pre/post identical). The
    # 268-file corpus walk is byte-identical pre/post (30 historical
    # aborts).
    add_test(
        NAME fuzz-diff-r149-repaired-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-repaired-key-r149.bin
    )

    # the r150 nightly's seed (crash-4d640bc4, run 42072059847, main
    # b4d6671f): the soup family interleaving the glue members with
    # plain fold runs mixing LS/PS (U+2029) into the LF/CR breaks
    # ("?\r/\n\r\r<PS> 50:" keys). yeptris is suite-correct on both
    # families — the plain-'?' law keeps the glued '?##'-led members
    # (the r132-r149 arm chain), and the #638 b-char algebra folds the
    # PS into the break run where libyaml's READ_LINE joins the raw
    # triple as content (the r104 forced ADDITIVE form: libyaml's
    # scalar is yeptris's with the raw triple INSERTED, "/\n\n50" vs
    # "/\n\n<PS>50"). But the glue walk had no arm for the fold pair
    # and brute-forced past it with the generic content-agnostic skips
    # — FOUR shared-cap skips per site (yeptris's key, its surviving
    # value, both of libyaml's lines) — and this soup's 6 fold sites +
    # 10 glue members burned 66 skips past the fixed 64, the walk dying
    # at the second-to-last wrapper's opener with every difference
    # law-explained. The LS/PS-fold PAIR arm now consumes both lines as
    # ONE paired diff (the emptied-key arm's pattern: both pointers
    # advance, bounded by construction, no cap needed) under the
    # byte-exact +3-length relation with the triple pinned by memcmp —
    # no wildcards; the family gate (pass 1) and the full-exhaustion
    # claim law are unchanged. The structure-tail negative (",?#z: {q:
    # 1}" spliced mid-stream) still aborts — post-fix the walk declines
    # at the spliced member itself (skips=2, a DECLINE, not the cap).
    # Post-fix the battery converges: 30 mutants (20 single-byte flips
    # + 10 multi-byte ins/sub/del), 0 DIFFER-aborts (10 pre-fix DIFFERs
    # all classify; the 20 raw-byte YEPTRIS-ONLY-ACCEPTs are the
    # suite-gray libyaml-rejects family, pre/post identical). The
    # 269-file corpus walk is byte-identical pre/post (30 historical
    # aborts).
    add_test(
        NAME fuzz-diff-r150-lsps-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-lsps-fold-r150.bin
    )

    # the r152 nightly's seed (crash-6689bee7, run 42095444821, main
    # 115ac2de): the soup family's RE-PAIRED-VALUE composition -- a
    # 66-byte doubled flow-map soup ("{?#?,`|,% \n:\t{" ... "}") whose
    # nested map hangs on member 3 in yeptris's reading and on the
    # EMPTIED member 1 in libyaml's. yeptris's suite-pinned plain-'?'
    # law keeps every glued-'?' run as content: member 1 "?#?" + empty
    # value, member 2 "`|" + empty, member 3 "%" whose VALUE is the
    # nested map, then ":#|G" and "<" pairs -- 22 event lines. libyaml's
    # comment-glue law (the flow-'?' KEY token fires on ANY follower;
    # the post-token skip eats a glued '#' as a comment to the break)
    # empties the killed run THROUGH the key token and RE-PAIRES the
    # pending KEY with the member's nested flow map -- the r148/r149
    # re-pairing law on the VALUE side: its member 1 renders as the
    # emptied key with the nested +MAP as its VALUE -- 16 event lines.
    # The walk's emptied-key arm pairs member 1's key, and the first
    # unexplained state is yeptris's EMPTY value line against libyaml's
    # +MAP: the r132 shared-opener arm demands y_val (the residue's
    # current line is empty, yl == vp exactly), the r130 structural arm
    # consumes libyaml's opener WITH yeptris's line (the inner bodies
    # then mis-pair -- yeptris's own +MAP grazes a libyaml VAL and no
    # arm fires), and the r116 extra-empty arm declines on l[0] == '+'.
    # The RE-PAIRED-VALUE ride carries it: yeptris's kept residue VALs
    # (the current line plus up to 15 further =VAL lines; only kept
    # -content scalars may sit between -- a structure line or an =ALI
    # in the run declines, the r144 survival gate stays the alias's
    # only path) step aside until yeptris's OWN byte-exact copy of
    # libyaml's 4-byte opener; libyaml's opener is NOT consumed -- the
    # twin pairs as an equal line, the inner bodies pair through the
    # existing arms, and the maps' closers pair 1:1 (no struct_skips
    # owed; a nesting skew surfaces as an unexplained closer and
    # bails). Family-gated by pass 1 (the glued ?# marker); bounded by
    # the per-site run cap (16, the r125 alias arm's precedent) and the
    # shared skip cap; the claim stands only on full exhaustion (the
    # walk's law). The prefix bisect found no shorter diverging cut
    # (the artifact is prefix-minimal at 66 bytes). Post-fix the battery
    # converges: 30 mutants (20 single-byte flips + 10 multi-byte
    # ins/sub/del) -- 15 classify green, 15 are the suite-gray
    # YEPTRIS-ONLY-ACCEPT family, pre/post byte-identical -- and the
    # 271-file corpus walk is byte-identical pre/post (30 historical
    # aborts).
    add_test(
        NAME fuzz-diff-r152-repaired-value
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-repaired-value-r152.bin
    )

    # the r153 nightly's seed (crash-aad3f269, run 42106098158, main
    # efcf0e5c): the soup family's TRIPLE-WRAPPER priority composition
    # -- a 149-byte prefix-minimal flow seq ("[11.0e@:\r\r ,?##|,..."
    # "... ,?+#\r\n&m-7\r\r ,?+#\r\n&m7\r\r ,?+#\r\n&m-]") stacking
    # THREE '?+#\n&m<N>' wrappers after '?##'-glue members. yeptris's
    # suite-pinned plain-'?' law keeps every wrapper run as ONE plain
    # ("=VAL : ?+# &m-7", the fold riding the break) and every glue
    # member as a scalar; libyaml's flow-'?' KEY-token law opens an
    # explicit-key salvage block per wrapper (+MAP / content KEY =
    # the run minus the leading '?' / empty VAL / -MAP) while its
    # comment-glue law swallows the glue members THROUGH the comma
    # ("?##|,?" dies to the break). The walk died on the PRIORITY of
    # two claims, not on a missing one: the r132 shared-opener arm's
    # opener byte-equality tell (yeptris's own next member happens to
    # open with the same "+MAP" line libyaml's wrapper opens with)
    # preempted the r143 wrapper ride at the FIRST wrapper, only
    # yeptris's killed scalar stepped aside, and every later pairing
    # ran one member off -- the byte walk grazed shape-equal blocks
    # (r124/r127 skips on both sides) until it died at the THIRD
    # wrapper's opener against yeptris's closer with no arm. The
    # refinement: the byte-exact-verified rides are attempted FIRST
    # inside the structural branch and the bare shared-opener skip is
    # their FALLBACK -- the ride's KEY (yeptris's killed member minus
    # the leading '?'), empty VAL and bare close pin the whole block,
    # which is strictly stronger evidence than one compared line; the
    # r132 shape (no wrapper block, the opener IS the next member's)
    # never satisfies the ride gates, so the prior routing is
    # unchanged where their claims stood. The prefix bisect found no
    # shorter diverging cut (the artifact is prefix-minimal at 149
    # bytes). Post-fix the battery converges: 30 mutants (20
    # single-byte flips + 10 multi-byte ins/sub/del) -- 11 DIFFER
    # mutants classify green, 19 stay in the suite-gray
    # YEPTRIS-ONLY-ACCEPT family, pre/post byte-identical, zero
    # regressions; a 300-flip sweep adds zero pre-green/post-abort
    # regressions and zero verdict flips -- and the 272-file corpus
    # walk is byte-identical pre/post (30 historical aborts).
    add_test(
        NAME fuzz-diff-r153-wrapper-priority
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-wrapper-priority-r153.bin
    )

    # the r154 nightly's seed (crash-97fad546, run 42115966505, main
    # 2386dcf6): the double-BOM ANCHORED-KEY composition -- 9 bytes
    # "BOM BOM '&U:'" (EF BB BF x2 then an anchored key), minimal by
    # byte-deletion ('&' and ':' are load-bearing; every BOM-corrupting
    # deletion moves the input to the invalid-UTF-8 split family).
    # yeptris strips the encoding mark and keeps the second BOM as
    # CONTENT (the catch-121 law: one mark stripped, content BOMs
    # ride), so the plain key scans "BOM&U" with the '&' mid-token --
    # a map forms at column 0. libyaml strips the second BOM too (its
    # scan_to_next_token skips a BOM at mark column 0: the
    # encoding-mark law is not stream-position-bound), so its residual
    # "&U" is an ANCHORED EMPTY SCALAR -- props, not content. The BOM
    # classifiers both declined: yd_bom_content_divergence's strip
    # leaves "=VAL : &U" (content) vs libyaml's "=VAL : &U " (props +
    # separator blank), and yd_bom_line_start_divergence's removal
    # probes are not pure -- the BOM-less residual "&U:" is itself the
    # r76 anchor-keymap shape (yeptris's 1.2 grammar rides the ':' into
    # the anchor name; libyaml's 1.1 set cuts at it and opens a map).
    # The twin: after the catch-121 strip, file yeptris's residual into
    # libyaml's prop form -- insert the one separator blank before the
    # line's break -- and compare; fires only when the residual is a
    # bare "&name" (one token, no blank in it), so the claim stays
    # byte-pinned. No engine violates its own law: yeptris never
    # double-strips, libyaml never keeps a column-0 BOM -- the
    # composition is the documented 1.1-vs-1.2 BOM-position divergence.
    # Post-fix the battery converges: 30 mutants (20 single-byte flips
    # + 10 multi-byte ins/sub/del) -- 3 benign classify green, 27 stay
    # in the suite-gray YEPTRIS-ONLY-ACCEPT family (the invalid-UTF-8
    # splits), zero verdict flips; 10 targeted family variants (long
    # anchor names, with-value, seq-sibling, three-BOM) all classify,
    # and the split-tail siblings (a column-0 sibling dedents libyaml's
    # BOM-shifted map indent and it rejects mid-stream) hold the same
    # YEPTRIS-ONLY-ACCEPT verdict pre/post; the 273-file corpus walk is
    # byte-identical pre/post (30 historical aborts).
    add_test(
        NAME fuzz-diff-r154-bom-anchored-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-anchored-key-r154.bin
    )

    # the r155 nightly's seed (run 42124396552, main 24f78376): the
    # #836/#951 phantom-verbatim class's FOURTH arming shape, past the
    # r133 bb gate and the r147 plain_open fact. The root plain
    # "(999999;!<1]" contains a mid-word ']' -- in block context that is
    # plain CONTENT (a plain cannot start with a c-indicator), but the
    # ']' / '}' branch cleared plain_open unconditionally while arming
    # the token slot. The following NEL fold then re-armed tok_start
    # with bb=']' -- itself in the r133 indicator set, so bb_plain = 0
    # and plain_open = 0 -- and the continuation's "!<" opened the
    # phantom verbatim tag. No '>' exists in the input, so the NELs
    # after it rode unnormalized into the normalized buffer; the plain
    # piece's first-lead-only multibyte probe then folded the first
    # raw NEL but let the yen-shielded runs behind it ride, and the
    # value diverged from libyaml's fold exactly at the "¥" + break
    # (yeptris "¥<NEL><NEL>9999" vs libyaml "¥\n9999").
    # The r155 law: the clear is a FLOW closer's act -- in flow, ']'/'}'
    # genuinely end the node; in block, a plain_open value word makes
    # the byte interior content and the fact rides (the catch-85 law's
    # indicator twin). The legal flow forms keep their clears
    # byte-identically, and the block props forms ("key: <NEL>!<a>")
    # keep their opens (plain_open is 0 there). Post-fix the value
    # folds to libyaml's exactly at the NEL sites and the composed
    # PS-fold tail classifies through the carried arms.
    add_test(
        NAME fuzz-diff-r155-plain-open-bracket-phantom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-phantom-verbatim-r155.bin
    )

    # the r156 nightly's seed (crash-3adc7533, run 42132975720, main
    # 821b27e7): the double-BOM ANCHORED-KEY PROP-SPLIT composition --
    # 10 bytes "BOM BOM '&U:#'" (the r154 9-byte form with a '#'
    # GLUED after the ':'), minimal by byte-deletion ('&' and ':' are
    # load-bearing; deleting the '#' reaches the r154 twin and every
    # BOM-corrupting deletion moves the input to the invalid-UTF-8
    # split family). yeptris strips the encoding mark and keeps the
    # second BOM as CONTENT (the catch-121 law: one mark stripped,
    # content BOMs ride), so the plain scans "BOM&U:#" -- one glued
    # run (the catch-85 law: the ':' has no blank follower and the
    # glued '#' opens no comment). libyaml strips the second BOM too
    # (its scan_to_next_token skips a BOM at mark column 0), and its
    # anchor scan consumes the MAXIMAL run of 1.1 anchor-set bytes and
    # cuts at the ':' -- so its residual files as props "&U" plus the
    # one separator blank plus ":#" riding as plain CONTENT. The r154
    # twin declines: its separator blank lands at the LINE's end for
    # an empty value ("&U:# ") instead of at the charset cut ("&U
    # :#"). The twin: after the catch-121 strip, file yeptris's
    # residual into libyaml's prop form at the 1.1 anchor-name cut --
    # the separator blank goes AFTER the name, preserving the glued
    # tail byte-for-byte -- and compare; fires only on an exact dump
    # match, so the claim stays byte-pinned. The r154 bare form is the
    # tail-empty special case this law degenerates to (and the r154
    # arm stays byte-pinned ahead of it). No engine violates its own
    # law: yeptris never double-strips, libyaml never keeps a
    # column-0 BOM, and the blank position is the documented 1.1
    # anchor-name cut (the r76 law). Post-fix the battery converges:
    # 42 mutants (20 single-byte flips + 10 multi-byte ins/sub/del +
    # 12 targeted family variants) -- 12 classify green (the 6 family
    # DIFFER members ride the prop-split arm), 30 hold identical
    # suite-gray verdicts, zero regressions; the 275-file corpus walk
    # is byte-identical pre/post (30 historical aborts).
    add_test(
        NAME fuzz-diff-r156-bom-anchored-key-prop-split
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-anchored-key-prop-split-r156.bin
    )

    # the r157 nightly's seed (crash-7319c8e328d7c72cb876fe062de9991e21d9e71a,
    # run 42139600292, main a017a870): the whole-line ANCHOR-DECL x
    # COLON-RIDING ALIAS ROOT COLLAPSE -- 12 bytes "&a:o:\n  *a:\n",
    # prefix-minimal (prefixes 0-10 agree; the trailing break is
    # load-bearing). yeptris's suite-pinned riding laws (2SXE: "&a: key"
    # anchors "a:" and "*a:" names "a:"; W5VH keeps colons inside names)
    # consume both lines whole: line 1 is a props-only anchor declaration
    # whose name rides the colons to the break (the anchor pends), line 2
    # an alias whose name rides the colon -- no value indicator ever
    # opens a map, and the document collapses to ONE root alias event
    # carrying the pended anchor as props. The alias names a
    # never-defined target ("a:" vs the defined "a:o:") -- the event
    # route emits ANY alias name by design (the compose split: DOM
    # composers reject undefined targets, event consumers carry every
    # alias like libyaml's parser; the suite pins no undefined-alias
    # error). libyaml's 1.1 laws compose the other way: IS_ALPHA cuts
    # the anchor at the first colon (the r142/r76 law), the residual
    # ":o:" re-lexes as plain ":o" with the final colon riding as the
    # value token (line 1 = the anchored KEY of a root map), and line
    # 2's alias "a" (the r54 cut) plus its own EOL colon opens a NESTED
    # map with the alias as key and an empty plain value -- the 9-event
    # form. The charset-split twin declines both orientations: forward
    # needs libyaml to read the single root alias (impossible), and the
    # reversed orientation gates yeptris(original) to the one-alias
    # signature -- the 'yeptris =ALI vs libyaml anything' mask the
    # r137/r138/r139 verdicts ruled unsound for event-kind divergences.
    # The composition rides the EXACT-BYTES gray claim; family siblings
    # (no-indent/1-space/"&a::" DIFFER members, empty-cut/tab-indent/
    # defined-alias libyaml-reject members) get their own entries or an
    # engine dig. Post-fix the battery converges: 32 mutants (20
    # single-byte flips + 10 multi-byte ins/sub/del + 2 targeted) hold
    # identical verdicts pre/post, zero regressions; the 276-file corpus
    # walk is byte-identical pre/post (30 historical aborts).
    add_test(
        NAME fuzz-diff-r157-anchor-decl-alias-collapse
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-decl-alias-collapse-r157.bin
    )

    # the r158 nightly's seed (crash-f0be7ede11bfeff3e9010eefcd933b50f1d112bb,
    # run 42147730364, main a77532d): the LITERAL-BLOCK LS x BREAKLESS-EOF
    # CLIP COMPOSITION -- 14 bytes "|", two space-blank lines, an
    # LS-only line, then " - m" ending at EOF with no break.
    # yeptris's value is LF LF LF LF "- m" LF (the #638 posture rewrites
    # the LS to a break, the blank lines' spaces strip per l-empty, and
    # clip chomping keeps the trailing break at breakless EOF -- the
    # r140 suite carry L24T#2); libyaml's is LF LF <raw LS> LF "- m"
    # (its READ_LINE joins the LS triple verbatim, scanner.c
    # LS|PS->LS|PS, and drops the breakless-EOF clip break). Each
    # adjudicated family classifies its own members alone -- an LS twin
    # with a final break rides the yd_dump_eq_mod_lsps_fold replace
    # reading, a breakless twin without an LS rides the
    # yd_clip_breakless_divergence exact-equality transform -- but the
    # composition bails both: the walk rejects the value-trailing clip
    # escape it cannot absorb, and the clip arm's strcmp sees the raw
    # triple it cannot strip. No engine violates its own law: both
    # strip the blank lines' spaces, and both deltas are documented
    # carries. The new arm composes the two adjudicated transforms in
    # the clip arm's own direction: append the break libyaml drops,
    # re-dump libyaml on that, require the exact lsps-walk relation
    # against yeptris's original dump -- a byte-bounded dump-shaped
    # relation, not an input-pinned gray. Post-fix the battery
    # converges: 35 mutants (20 single-byte flips + 10 multi-byte
    # ins/sub/del + 5 targeted family variants: PS/NEL twins, the
    # plain-LF control, a double-LS run, LS mid-content) classify or
    # hold identical verdicts pre/post, zero regressions; the 277-file
    # corpus walk is byte-identical pre/post (30 historical aborts).
    add_test(
        NAME fuzz-diff-r158-lsps-breakless-clip-composition
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/literal-ls-clip-breakless-r158.bin
    )

    # the r159 nightly's seed (crash-daa73e2d5be6fba79aac719149250a32012641d7,
    # run 42154482223, main 7b55b9a): the UTF-16 COLUMN-0 BOM x PLAIN-NEL
    # FOLD composition -- 16 bytes, the BE mark, U+0085, U+FEFF, U+0085,
    # then "a", U+2D29, U+4600, U+4562. Both engines strip the leading NEL
    # as a plain's leading break; yeptris keeps the mid-plain FEFF as
    # CONTENT (the catch-121 law: one mark stripped, content BOMs ride) so
    # the second NEL folds to ONE space (the plain-NEL fold law), while
    # libyaml skips the column-0 BOM (its scan_to_next_token, the r154
    # law) and the NEL stays leading -- consumed whole, no fold artifact.
    # yeptris "=VAL : <BOM> a..." vs libyaml "=VAL : a...". The r154/r156
    # twins decline (the residual is not an anchored key) and the
    # line-start removal probes decline (a UTF-16 NEL unit is never a
    # byte-level line start, so no byte-space removal is pure). The twin:
    # strip the BOM AND the one fold space behind it -- the pair is
    # exactly "content BOM + the break that folded" (a space is never a
    # plain's head content in either engine) -- and compare; fires only on
    # an exact dump match, so the claim stays byte-pinned. The UTF-16LE
    # twin classifies through the same dump-level relation.
    add_test(
        NAME fuzz-diff-r159-bom-fold-space-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-nel-fold-bom-r159.bin
    )

    # the r160 nightly's seed (run 42160225920, main 6aa80d7): the
    # FOLDED-BLOCK NEL-CONTENT law -- 15 bytes "3e 0a 20 21 3c c2 85
    # 20 74 65 78 74 39 0a 0a": a folded-block-scalar header, then the
    # content line " !<" + U+0085 (NEL) + " text9", then the empty
    # clip line. Both engines accept; the values differ at the NEL:
    # yeptris "=VAL : !<\xc2\x85 text9\n" (the FULL two-byte pair kept
    # as CONTENT -- inside block-scalar content the 1.2 break set is
    # LF/CR/CRLF only, spec 8.1, so the multi-byte breaks are content)
    # vs libyaml "=VAL : !< text9\n" (its 1.1 IS_BREAK counts the NEL
    # as a break INSIDE the block scalar, splitting the content line;
    # the lone break folds to the fold space and the continuation's
    # indent space strips). The suite sides with yeptris (1.2
    # content), so this is a carry -- NOT the phantom-verbatim class:
    # the value bytes are the full C2 85 pair (no lead-stripped 0x85),
    # and the ASCII-only control (">" + " !< text9", no NEL) agrees
    # byte-exact on both engines, so nothing arms. The plain-NEL fold
    # arm declines (yeptris's value keeps a content space behind the
    # pair where its k==1 fold law maps the pair onto libyaml's one
    # space, leaving a ' '-vs-'t' mismatch); the quoted twin's
    # ' '-vs-'\n' signature does not fire (the position aligns
    # space-for-space). The new block-content arm walks the dump pair
    # with libyaml's two split artifacts: up to (k-1) escaped breaks
    # per NEL run (the multi-break collapse, the plain arm's law) and
    # exactly one re-alignment byte behind the run -- yeptris's
    # verbatim space against libyaml's break artifact (the literal
    # twin), yeptris's space that libyaml ate as the continuation's
    # indent (the double-NEL twin), or libyaml's fold space with no
    # yeptris counterpart (the glue shape). Beyond that the dumps must
    # be byte-identical. The LS/PS twins agree on both engines (the
    # front end normalizes LS/PS, only NEL stays content). The
    # structural member of the family (the NEL-induced dedent: a
    # 2-space content line whose NEL break makes libyaml's next line a
    # dedent that ends the block early) stays UNCLAIMED by design --
    # claiming it needs a loose "tail may vanish" relation; it holds
    # identical verdicts pre/post. Post-fix the battery converges: 35
    # mutants (20 single-byte flips + 10 multi-byte ins/sub/del + 5+10
    # targeted family variants and controls) classify or hold identical
    # verdicts pre/post, zero regressions; the corpus walk is
    # byte-identical pre/post on the shared 279 files (30 historical
    # aborts) with the new seed riding green (280 post).
    add_test(
        NAME fuzz-diff-r160-block-nel-content
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-block-nel-content-r160.bin
    )

    # the r161 nightly's seed (run 42166243287, main 32b62a9): the
    # TERMINAL-TAIL sibling of the r160 block-content law -- 9 bytes
    # "3e 0a 20 21 3c c2 85 20 0a": the same folded header and " !<" +
    # U+0085 (NEL) content line, with the mutation walk's deleted
    # " text9" word leaving the content space TERMINAL: " !<" + NEL +
    # " " then the single clip break. Both engines accept; the tails
    # differ (byte-exact dumps, terminal rendering stripped):
    # yeptris "=VAL : !<\xc2\x85 \n" -- NEL stays CONTENT (the 1.2
    # block-scalar break set is LF/CR/CRLF only, spec 8.1), the space
    # stays content, the clip break escapes as \n -- vs libyaml
    # "=VAL : !<\n": its 1.1 IS_BREAK splits the line at the NEL and
    # its scan keeps the NEL break as the value's ONLY break (clip
    # joins leading_break; the blank continuation line's space was
    # eaten as indentation and that line's break is dropped at the
    # scan loop's exit). The suite sides with yeptris (1.2 content) --
    # a carry; nothing NEW is engine-suspicious (yeptris's clip keeps
    # exactly one trailing break, and the breakless-EOF member rides
    # the documented r140 clip law). The r160 arm DECLINES this member:
    # its "space is libyaml's break" branch eats libyaml's escaped
    # break -- but that break is libyaml's CLIP break here, so
    # yeptris's own escaped clip break strands behind it and the walk
    # bails. The extension adds ONE bound branch at the NEL run: the
    # TERMINAL tail -- yeptris "SP + \n" against libyaml's single
    # escaped break -- is accepted only when the remainders behind
    # BOTH escaped breaks are byte-identical (the value frames end the
    # same way), so it can never swallow arbitrary divergence past the
    # scalar; it fires before the (k-1) collapse so every run length
    # k >= 1 rides the same law (the literal twin and the k=2/k=3 runs
    # classify with it), and the r160 shapes keep their branches (the
    # r160 seed and its content-bearing twins miss the tail check: the
    # byte behind their space is not the escape). The indent-0 twin is
    # a SPLIT (libyaml rejects an indent-0 block scalar), not a DIFFER
    # -- it holds identical pre/post. Post-fix the battery converges:
    # 47 mutants (20 single-byte flips + 10 multi-byte ins/sub/del +
    # 17 targeted family variants and controls) classify (5: the
    # literal twin, the k=2/k=3 runs, the breakless-EOF member and one
    # deletion mutant) or hold identical verdicts pre/post (42), zero
    # regressions; the corpus walk is byte-identical pre/post on the
    # shared 280 files (30 historical aborts) with the new seed riding
    # green (281 post); leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r161-block-nel-terminal-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-block-nel-content-r161.bin
    )

    # the r162 nightly's seed (run 42171805647, main cf17954): 33 bytes
    # "5b 31 31 0d e2 80 a9 20 37 2e 74 61 67 0d 20 2c 3f 23 23 7c 2c
    # 3f 0d 32 30 30 3a 0a 20 20 6d 2c 5d" -- a flow seq whose member 1
    # carries the break run "11<CR><PS> 7.tag" INSIDE its plain fold,
    # followed by the r116 soup: the glued "?##|" member (libyaml's
    # post-'?'-token comment-glue law eats "?##|,?" whole), the explicit
    # "?<CR>200:<LF>  m" map, and the close. Both engines accept; two
    # dump lines differ:
    #   yeptris "=VAL : 11 7.tag" / "=VAL : ?##|" then +MAP 200 m -MAP
    #   libyaml "=VAL : 11<PS>7.tag" then +MAP 200 m -MAP
    # (yeptris folds CR+PS+blank to ONE blank, libyaml joins the PS raw
    # and eats CR + blank -- the #638 READ_LINE law; libyaml's kill
    # leaves its map opening where yeptris keeps the killed member THEN
    # its map -- the r132 shared-opener shape). Neither arm's gate
    # covers the FIRST difference: the r150 pair arm accepts the
    # INSERTED triple (delta exactly 3, yeptris's text byte-identical
    # around it), but here yeptris's own fold blank IS the site --
    # libyaml's content is yeptris's with the blank REPLACED by the
    # triple, a delta of 2 -- so the additive gate declines, the
    # content-agnostic skip eats yeptris's side, and the walk dies one
    # step later at libyaml's surviving line against yeptris's map
    # opener with no arm. The generic lsps fold walk (r56) owns the
    # substitution law but not the soup; the soup walk owns the soup but
    # not the substitution -- the composition needed both. The extension
    # adds ONE pair form to the r150 branch: yd_qgh_lsps_substituted
    # walks the member pair position-wise (equal bytes advance both, a
    # yeptris blank facing a raw LS/PS triple advances 1/3 as one site,
    # anything else declines; at least one site, both contents consumed,
    # pointers never rewind), consuming BOTH lines as one diff -- the
    # members are the same event on both sides, so the walk stays
    # bounded by construction (the r150 pattern). Engine side is
    # law-clean on both deltas: yeptris's fold is the #638 adjudicated
    # 1.2 reading, and its "?<CR>200:m" explicit-key map across the CR
    # break is the #713 carried flow-? law (libyaml agrees on the map
    # body 200:m; only the kill differs). Nothing NEW is
    # engine-suspicious. The double-triple run (yeptris folds to one
    # blank, libyaml joins 2 triples) has no adjudicated reading and
    # keeps aborting BY DESIGN, as does the genuine mid-stream member
    # drop composed with the same shape. Post-fix the battery
    # converges: 41 mutants (20 single-byte flips + 10 multi-byte
    # ins/sub/del + 11 targeted family variants and controls) classify
    # (8) or hold identical verdicts pre/post (33), zero regressions;
    # the corpus walk is byte-identical pre/post on the shared 281 files
    # (30 historical aborts) with the new seed riding green (282 post);
    # leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r162-lsps-substituted
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-lsps-substituted-r162.bin
    )

    # the r163 nightly's seed (run 42177815461, main ec8fc99): 26 bytes
    # "0a 0d 7b 31 3a 0d 0d 0d 0a 09 20 2c 3f 23 23 0d 0d 0d 0d 2a 35
    # 0d 0a 0a 7d 0a" -- a flow map whose member 1 is the empty pair
    # "1:" and whose member 2 is the glued "?##" run followed by the
    # CR CR CR CR break run and "*5", then the close. Both engines
    # accept; the member-2 KEY line differs:
    #   yeptris "=VAL : ?##\n\n\n*5"
    #   libyaml "=ALI *5"
    # (yeptris's suite-pinned plain-'?' law keeps the glued run ONE
    # plain folded across the breaks and the '*' is ns-plain-char
    # CONTENT there -- the indicator exclusions bind only
    # ns-plain-first, so mid-plain '*' is content in BOTH engines:
    # "{x<CR>*5}" agrees byte-exact, the alias-vs-content split only
    # exists on the glue path. libyaml's post-'?'-token comment-glue
    # law consumed the glued '?##' THROUGH its flow-KEY token and
    # swallowed the break run, so "*5" became an ALIAS token re-paired
    # with the pending KEY. The fold itself is shared algebra: the
    # b-l-trimmed reading renders a break followed by k empty lines as
    # k escaped breaks with no fold space -- "{a:<CR>x4 b}" agrees
    # byte-exact -- so nothing NEW is engine-suspicious.) The soup
    # family's alias-swallow composition in the REVERSE orientation of
    # the r138-r141 gray claims: HERE libyaml carries the =ALI and
    # yeptris the kept content. Neither existing arm covers the pair:
    # the r125 alias-realign arm demands yeptris carry its OWN =ALI
    # downstream (yeptris never emits one -- the spelling rode into
    # the kept plain) and declined at yeptris's -MAP, and the r144
    # survival arm is not consulted at all (yeptris's line is a VAL).
    # The extension adds ONE pair arm to the soup walk BEFORE the r125
    # branch: libyaml's full alias rendering must be yeptris's kept
    # content's byte-exact SUFFIX (name non-empty, no wildcards -- at
    # a node start yeptris emits =ALI itself and the line pairs
    # equal, so a content CARRYING the spelling is the direct
    # observation that the '*' was mid-plain by construction; the
    # r138-r141 masking concern does not apply -- libyaml's alias is
    # never dropped, it is right there), and the kept content must
    # carry the glue fingerprint LOCALLY (a glued "?#"/"?!" inside the
    # pre-spelling prefix -- the plain-'?' law's own marker, keeping
    # the arm family-bound rather than any suffix-match). The pair
    # consumes BOTH lines as one diff (the emptied-key arm's pattern:
    # both pointers advance, bounded by construction, no cap; the
    # claim stands only on full exhaustion). The doubled alias
    # ("*5*6": yeptris's tail "*5*6" is not libyaml's "=ALI *5" tail)
    # declines into the r125 arm and keeps aborting BY DESIGN, as does
    # the '?!' twin (libyaml's tag path, a different family with no
    # adjudicated reading) and a genuine mid-stream member drop
    # composed with the shape. Post-fix the battery converges: 42
    # mutants (20 single-byte flips + 10 multi-byte ins/sub/del + 12
    # targeted family variants and controls) classify (13) or hold
    # identical verdicts pre/post (29), zero regressions; the corpus
    # walk is byte-identical pre/post on the shared 282 files (30
    # historical aborts) with the new seed riding green (283 post);
    # leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r163-alias-content-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-alias-content-tail-r163.bin
    )

    # the r164 nightly's seed (crash-bf69aeae0017e8d09f029795f0285f757864b481,
    # run 42184829386, main 66f9218): 1198 bytes "5b 31 31 20 2c 3f 23
    # 23 7c 2c 3f 0d 2f 0a 0d 0d e2 80 a9 20 35 30 3a 0d ..." -- the
    # LARGEST soup artifact yet, a flow seq packed with repeated
    # members: '?##|'-led killed members, libyaml salvage maps
    # ("/<LF><LF><PS>50"/"1.0e17.tag", "211"/empty, "2002:binary"/empty),
    # the '?####|' double-glue shape, '?## &m' props members (the r148
    # props-stripped KEY), '2002:binaryg:yamlaml.org' run tails, a
    # '&m'-anchored wrapper, a 'y---|' tail, break runs folding to
    # LF-LF-LF escapes. Prefix truncation does not shrink it (every
    # proper prefix classifies -- the death is skip DENSITY at stream
    # end, so the whole composition is the minimal form). The walk
    # re-aligns through nearly all of it: the first raw mismatch (line
    # 3, yeptris "=VAL : ?##|" vs libyaml "+MAP") rides the r132
    # shared-opener tell, the salvage maps pair through the emptied-key
    # and r143/r148 wrapper-ride arms, the r150 additive sites pair,
    # and the walk dies at skips=65 > 64 on the r117 l-only arm with
    # every difference law-explained. The burn is a THIRD fold-count
    # form of the #638 LS/PS pair relation beside the r150 ADDITIVE
    # (libyaml = yeptris + one raw triple, yeptris folds the run to
    # nothing) and the r162 SUBSTITUTED (yeptris folds it to one
    # blank) outcomes: the soup's break run CR LF CR CR PS SP ("?
    # <CR>/<LF><CR><CR><PS> 51:") is FOUR breaks in yeptris's b-char
    # algebra (LS/PS are breaks in 1.2) rendered as THREE escaped
    # breaks ("=VAL : /\n\n\n51") while libyaml's READ_LINE folds only
    # the CR/LF count and joins the PS RAW ("=VAL : /\n\n<PS>51") --
    # yeptris's 2-byte "\n" ESCAPE is libyaml's 3-byte triple at the
    # site, a delta of +1 neither earlier pin matches. Both engines
    # stand on their own documented laws (#638 / READ_LINE), the
    # structure ("51:" splitting key/value, the member count) agrees
    # byte-exact, and nothing NEW is engine-suspicious; the generic
    # arms then spent FOUR shared-cap skips per site (r124/r127 on
    # yeptris's line + l-only on libyaml's) and five such sites plus
    # the glue members outran the fixed 64. The extension adds
    # yd_qgh_lsps_escbreak to the r150/r162 pair arm's OR: a
    # position-wise walk where equal bytes advance both, a yeptris
    # "\n" escape facing the raw LS/PS triple advances 2/3 (a site,
    # byte-pinned -- backslash+n vs E2 80 A8/A9, no wildcards),
    # anything else declines, at least one site, both contents fully
    # consumed. The pair consumes BOTH lines as one diff (both
    # pointers advance, bounded by construction, no cap needed; the
    # pointers never rewind, so the walk terminates). No existing arm
    # or cap is weakened: the skips the paired arm no longer spends
    # stay available to the shapes that still need them. The
    # candidates that did NOT contribute are pinned benign: the
    # '?####|' double-glue scores pass 1 (the first '?' faces '#') and
    # rides the shared-opener tell; the '?## &m' member rides the
    # r143/r148 props-only KEY wrapper ride; glue-member scaling alone
    # never binds the cap. The genuine mid-stream member drop composed
    # with the shape ("[?##|,<escbreak-member>?##{x: 2},z]") keeps
    # aborting (the terminal-tail gate declines: structure
    # mid-stream), as does the one-escape-vs-two-triples decline
    # control and the '?!'/'LS-twin' family variants with no
    # adjudicated reading. Post-fix the battery converges: 45 mutants
    # (20 single-byte flips + 10 multi-byte ins/sub/del + 15 targeted
    # family variants and controls) classify (9) or hold identical
    # verdicts pre/post (36), zero regressions; the corpus walk is
    # byte-identical pre/post on the shared 283 files (30 historical
    # aborts) with the new seed riding green (284 post); leaks
    # --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r164-lsps-foldcount
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/soup-lsps-foldcount-r164.bin
    )

    # the r165 nightly's seed
    # (crash-2e1d2b7299811a9945807abc77c3873f63375a75, run 42193562130,
    # main 57f2e4b): the r159 shape with ONE MORE NEL unit -- 16 bytes,
    # the BE mark, U+0085, U+FEFF, U+0085, U+0085, then "a", U+2629,
    # U+4662. The THIRD NEL makes a BLANK LINE between the FEFF content
    # and 'a', so the plain-NEL fold law renders an escaped BREAK, not
    # the r159 fold space (k == 1 behind the content folds to one
    # space, k >= 2 collapses to k-1 escaped breaks). Both engines
    # strip the leading NEL; yeptris keeps the mid-plain FEFF as
    # CONTENT (the catch-121 law) so the break run renders
    # "=VAL : <BOM>\na..." while libyaml skips the column-0 BOM (the
    # r154 law) and the whole NEL run stays leading -- consumed whole,
    # no artifact for the blank line either. The r159 space gate
    # declines (the residual opens with the two-byte "\n" escape, not a
    # space) and the r154/r156 anchored twins decline (the residual is
    # not an anchored key). The twin: strip the BOM AND the maximal run
    # of "\n" escapes behind it -- exactly "content BOM + the breaks
    # that folded" -- and compare; fires only with non-space content
    # behind the run and on an exact dump match, so the claim stays
    # byte-pinned (a run is never a value's tail -- trailing breaks
    # strip, the r122 law -- and a space is never a continuation's head
    # content). Encoding- and break-symmetric: the UTF-16LE twin and
    # the LS/PS/mixed members classify through the same dump-level
    # relation. No existing arm is weakened.
    add_test(
        NAME fuzz-diff-r165-bom-fold-break-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-nel-fold-breaks-r165.bin
    )

    # the r166 nightly's seed
    # (crash-095dea85d20a4b6687f4539f98dba8eae8df87d4, run
    # 42202493551, main a6d4b41): a 490-byte root plain packed with
    # break runs (NEL pairs, LS triples, CR, LF) where the LS/PS
    # raw-join sites (libyaml's READ_LINE joins the LS/PS break RAW
    # and folds the trailing LF/CR run in FULL -- the
    # yd_dump_eq_mod_lsps_fold family, r56/r72/r86/r104) INTERLEAVE
    # with raw-NEL content sites (yeptris keeps the 1.2 non-break NEL
    # as raw content while libyaml folds it, k == 1 to one fold space
    # and k >= 2 to k-1 escaped breaks -- the yd_dump_eq_mod_nel_fold
    # family, r118/r119/r122/r128). First mismatch at dump offset 29
    # ("[{" + two escaped folds vs "[" + the raw LS triple + the same
    # two escaped folds): the NEL walk dies on its first divergent
    # byte (backslash vs 0xE2, no LS arm); the LS walk dies at the
    # first NEL site (0xC2 vs backslash, no NEL arm). The r158 lesson
    # applied to walks: ONE composed walk carries both site kinds --
    # equal bytes advance both pointers, an LS/PS triple rides the
    # fold-walk's run law (additive first, backtrack to replace,
    # depth-capped, the r90 lesson), a fold space facing the triple
    # is the r56 plain law, a NEL run (raw pairs, lone 0x85 bytes
    # under the lead-byte guard, \N escapes) rides the r118 k-law
    # with the r122 trailing-run vanish, anything else declines,
    # pointers never rewind. Gated on BOTH byte families in the input
    # and ordered behind the single-family arms, so every existing
    # member still reaches its own (untouched) arm first.
    add_test(
        NAME fuzz-diff-r166-plain-nel-lsps-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/plain-nel-lsps-fold-r166.bin
    )

    # the r167 nightly's seed
    # (crash-a6a61def8116bbde86fc61dff1ac018afd3b0fed, run 42211827282,
    # main d6b6098): 13 bytes, "[=,?!,?!\n,8]" -- TWO mid-stream '?!'
    # members followed by a trailing "8" member in one flow seq. The
    # r136 tail-wrapper shape MID-STREAM, doubled: libyaml's flow-'?'
    # KEY-token law opens an explicit-key salvage map per member and
    # the glued '!' rides it as the non-specific tag, so each '?!'
    # renders the four-line empty block (+MAP / "=VAL : " / "=VAL : " /
    # -MAP -- the tag rode the key away, the pair's value is the
    # salvaged null) while yeptris's suite-pinned plain-'?' law keeps
    # each glued run ONE plain member ("?!" -- the r135 tag-riding law;
    # the break folds). Both engines agree member 1 ("="), member 4
    # ("8") and the skeletons around them. The r143/r149 wrapper rides
    # decline (their gates demand the KEY carry content -- kl > 0) and
    # the r130 line-wise path over-consumes: wrapper 1's opener eats
    # member 2, wrapper 1's emptied key and value GRAZE members 3 and
    # 4 through the emptied-key arm, the closer bank eats its "-MAP",
    # and wrapper 2's opener faces yeptris's "-SEQ" closer with no arm
    # (mid-stream, so the r142 terminal-tail gate declines too). The
    # r167 ride: the structural arm claims the WHOLE wrapper when it is
    # the byte-pinned empty block and yeptris's killed member is
    # glue-LED (its content opens with '?' plus the glued '#' or '!' --
    # the pass-1 marker's own byte pair at the member). Both pointers
    # advance every claim; the claim stands only on full exhaustion.
    add_test(
        NAME fuzz-diff-r167-qbang-mid-wrapper
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-qbang-mid-wrapper-r167.bin
    )

    # the r168 nightly's seed
    # (crash-42d70bee0ba24bfc4025b40bade40ebc64c5517e, run 42219263275,
    # main be3b9af): 71 bytes "[11.0e ,?##|,:|,:binary<CR> bnary<CR>
    # b:inary<CR> binary<CR> bnary<CR> b:ibinary<CR><CR> ,?#<LF>&m,]"
    # -- the soup family's distance-TWO re-pairing. libyaml's
    # post-'?'-token comment-glue law (the '#' at token start comments
    # to the NEXT break) ate the glued '?##' run THROUGH TWO members --
    # the killed member's tail "?##|" AND ":|" whole, plus ":binary" --
    # and re-paired the pending KEY token with the CR-folded
    # continuation: wrapper 1 renders (+MAP / "=VAL : bnary b:inary
    # binary bnary b:ibinary" / "=VAL : " / -MAP) whose KEY is the
    # byte-exact SUFFIX of yeptris's member at distance 2 (yeptris's
    # suite-pinned laws keep '?##|' and ':|' as plains -- a '#'
    # comments only behind a blank, a flow ':' needs the blank too --
    # and fold the CR run into the SAME member's head ":binary bnary
    # b:inary binary bnary b:ibinary"). The r143 ride declines (its
    # gate demands the KEY be the killed member's OWN residue),
    # r149 declines (its gate demands the suffix of the IMMEDIATELY
    # next surviving member -- the comment consumed that member WHOLE),
    # r167 declines (the KEY carries content), and the line-wise path
    # mis-pairs to the r167 commentary's own death: the structural
    # skip burns the owed closer on wrapper 1's "-MAP" while the
    # emptied-key arm eats yeptris's LAST member ("?# &m") against
    # wrapper 1's salvaged null, so wrapper 2's opener ("?#<LF>&m" --
    # the r148 props-only anchored KEY) faces yeptris's "-SEQ" closer
    # with no arm. The r168 ride: the structural arm claims wrapper 1
    # plus BOTH consumed members when the killed member is glue-led
    # (the r167 lead-form pin), the member between is a content VAL,
    # and the member at distance 2 is a content VAL whose content ENDS
    # with the wrapper's KEY byte-exact (the r148 render separator
    # stripped) -- the direct observation that the KEY's bytes are the
    # kept plain's own tail, not an invented key. Wrapper 2 then pairs
    # through the EXISTING r149 ride (its "&m" anchor is the kept
    # "?# &m" member's own tail). The CR folds themselves AGREE
    # byte-exact in both engines (a lone CR folds to one space) --
    # nothing NEW is engine-suspicious in the fold algebra; the ride is
    # bounded by the fixed four-line block, the byte-exact suffix pin,
    # both pointers advancing every claim, and the shared skip cap;
    # the claim stands only on full exhaustion (the walk's law).
    # Prefix truncation shrinks the artifact to 24 bytes
    # ("[?##|,:|,:b<CR> c<CR><CR> ,?#<LF>&m,]", classified); the RAW
    # artifact is pinned. Post-fix the battery converges: 51 mutants
    # (20 single-byte flips + 10 multi-byte ins/sub/del + 21 targeted
    # family variants and controls) classify (12) or hold identical
    # verdicts pre/post (39), zero regressions; the corpus walk is
    # byte-identical pre/post on the shared 287 files (30 historical
    # aborts) with the new seed riding green (288 post); leaks
    # --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r168-rekey-distance2
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-rekey-dist2-r168.bin
    )

    # the r169 nightly's seed
    # (crash-03fc7cd6b847d0ac23175a4a3fc1135d8e0fd836, run 42227304538,
    # main bf2db8d): a 168-byte root plain whose NEL fold sites hide
    # behind FALSE LEADS -- yep_scan_plain's multibyte-break check
    # tested only the FIRST C2/E2 lead byte of each piece; when that
    # lead was an ordinary character (the U+00A5 yen) the check
    # declined and NEVER examined later leads, so a NEL run after the
    # yen rode RAW into the plain (the r74 plain-ends-at-NEL law
    # defeated by the lead's own position; "a:{b]" +NEL+ "!<" +yen+
    # NEL^2 + "b" carried raw c2 85 c2 85 where libyaml folds the
    # break). The composed twin (the fifth phantom-verbatim arming
    # shape) rode the normalizer: the glued ':'-twin's PHANTOM flow
    # open ("a:{b]" -- the #836-accepted prev==':' state) inflated the
    # walk's monotone in_flow, the interior ']' then hit the r155
    # clear's in_flow>0 arm and cleared plain_open, and the next
    # fold-armed "!<" (bb=']' blanks bb_plain too) opened a
    # never-closed verbatim that swallowed the rest of the input's
    # NELs unnormalized. The fixes: scan_plain loops the lead check
    # over EVERY lead in the piece (a false lead advances to the next
    # lead and re-tests), and the normalizer tracks the phantom depth
    # separately -- the r155 clear keys on REAL flow (in_flow above the
    # phantom count), so block-context ']' / '}' keep the plain_open
    # fact. Post-fix the repro's value folds byte-for-byte with
    # libyaml at the NEL sites and the artifact classifies. The unit
    # probes pin the law both ways: yen+NEL^2 ends the span at the
    # NEL, and a plain yen WORD with no NELs is untouched.
    add_test(
        NAME fuzz-diff-r169-plain-nel-yen-ride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/plain-nel-yen-ride-r169.bin
    )

    # the r170 nightly's seed
    # (crash-d0c77945da7d71eeb449137e49f26840c558d175, run 42241061657,
    # main fbb7232): the r157 root-alias collapse's NO-INDENT SIBLING --
    # 10 bytes "&a:o:\n*a:\n", byte-identical to the r157 seed with the
    # two-space indent deleted (the r157 pin's family list named this
    # member; the r151 precedent gives deletion siblings of exact-bytes
    # claims their own entries). Both engines accept; the streams differ
    # one root alias (5 events) vs libyaml's 9-event map form. yeptris's
    # riding laws are unchanged on post-r169 main (raw-event probe:
    # ALIAS value "a:", anchor "a:o:" -- the pended anchor riding as
    # props, the alias name riding the colon); libyaml's 1.1 cut re-opens
    # the map at the root (line 1 = anchored ":o" key + empty value,
    # line 2 = alias-keyed nested map + empty value). The charset-split
    # twin declines both orientations exactly as r157's (libyaml can
    # never read the single root alias; the reversed orientation gates
    # yeptris to the one-alias signature -- the 'yeptris =ALI vs libyaml
    # anything' mask the r137/r138/r139 verdicts ruled unsound for
    # event-kind divergences), so the composition rides the EXACT-BYTES
    # gray claim. Minimality: prefixes 0-8 agree; prefix 9 diverges
    # IDENTICALLY (the trailing break is NOT load-bearing here, unlike
    # the r157 seed -- the breakless twin stays unclaimed beside the
    # pinned cut); the 'o'-deletion "&a::" member keeps the skeleton
    # walk (its own entry), the name/colon deletions land in the
    # libyaml-reject split members. The 50-mutant battery converges:
    # the artifact classifies; the r157 indented seed stays green; the
    # breakless/"&a::"/indent/defined-alias siblings and the split
    # members hold identical verdicts pre/post, zero regressions; the
    # corpus walk is byte-identical pre/post on the shared files (30
    # historical aborts) with the new seed riding green; leaks --atExit
    # clean on the seed.
    add_test(
        NAME fuzz-diff-r170-root-alias-collapse-noindent
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-decl-alias-collapse-r170.bin
    )

    # the r171 nightly's seed (crash-0500383c2a886aef6c49d3e3d15a383a18b
    # 0d7ce, run 42249881864, main 7a78020): 8 bytes "3e 20 23 3c c2 85
    # 20 74" -- a folded-block-scalar header whose line ends at the
    # comment's NEL ("> #<" + U+0085 + " t", no trailing break). The
    # normalizer's header token swallowed the tail to '\n' only (no
    # '\n' exists in the input), so the NEL rode unnormalized to the
    # engine, whose comment skip then ate " #<"+NEL+" t" whole: yeptris
    # emitted an EMPTY value where libyaml's IS_BREAKZ comment law ends
    # the line at the NEL and reads the content line " t" as "t". An
    # ENGINE BUG (an internal-law inconsistency), not a carry: yeptris's
    # own #638 posture folds NEL EVERYWHERE outside quoted regions (its
    # in_comment arm folds NEL; step_scan's comment arms fold
    # NEL/LS/PS), and the header line is OUTSIDE the block-content
    # area, so the r160 content-NEL law (content NELs stay content)
    # never applied. The controls settle it: the ASCII twins
    # ("> #x\n t" breakless and with the break) agree byte-exact on
    # both engines (nothing arms without the multibyte break), the LF
    # twin agrees, and the divergence reproduces with the '<' deleted,
    # with a bare '#' comment, on the literal twin, and on the PS/LS
    # twins (all three multibyte breaks ride the same arm). The fix
    # ends the header token at ANY fold break (norm_next_fold_break:
    # '\n', lone '\r'/CRLF, NEL, LS/PS), folding it into the token so
    # the copy splices the '\n', and arms the break state so the next
    # line rides the bs branch from its own indent; the streaming twin
    # ends the header line the same way (its line-start gate now honors
    # at_break, the one-shot walk's own law). Minimality: every
    # artifact byte is load-bearing per the deletion battery (the
    # comment, the NEL, the space, the 't'; the '<' is the nightly's
    # shape). Post-fix the artifact classifies (yeptris "t\n" per the
    # suite's breakless-EOF clip law vs libyaml's bare "t" -- the
    # carried r140 arm), the family twins (literal, PS, LS, no-'<',
    # bare-'#') classify with it, and the r160 content seed stays
    # green. The 50-mutant battery converges with zero regressions and
    # zero verdict-class flips; the corpus walk is byte-identical
    # pre/post on the shared files (the historical aborts) with the new
    # seed riding green; leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r171-block-header-comment-nel
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-header-comment-nel-r171.bin
    )

    # the r172 nightly's seed
    # (crash-6dcd3a2343fe26ff8c4fbed84831a3ccd421c62b, run 42274633465,
    # main 93fd25d): 57 bytes "[11.0e ,?##|,:|,:bi.0e ,?##|,:|,:bYMA
    # y<CR> bnaary<CR><CR> ,?#<LF>&m,]" -- the soup family's re-pairing
    # at KEY distance FIVE. libyaml's post-'?'-token comment-glue law
    # (the '#' at token start comments to the NEXT break) ate the glued
    # '?##' run THROUGH FIVE members -- the killed member's tail
    # "?##|", then ":|", ":bi.0e", the DOUBLED "?##|", and ":|" whole
    # (the whole run to the first CR is one libyaml line; the doubled
    # glue pair and the "YMA y" interior are incidental: any break-free
    # bytes ride the comment invisibly) -- and re-paired the pending
    # KEY token with the CR-folded continuation ("bnaary"; the trailing
    # "\r\r ,"'s blank line and comma end the plain). Wrapper 1 renders
    # (+MAP / "=VAL : bnaary" / "=VAL : " / -MAP) whose KEY is the
    # byte-exact SUFFIX of yeptris's member at distance 5 (yeptris's
    # suite-pinned laws keep every glued run as content -- a '#'
    # comments only behind a blank -- and fold the CR run into the same
    # member's head ":bYMA y bnaary"); wrapper 2 then pairs through the
    # EXISTING r149 ride (its props-only "&m " KEY is the kept "?# &m"
    # member's own tail, the r168 tail pattern). The r143/r149/r167/
    # r168 rides decline (distances 0/1/2 own their gates; the KEY
    # carries content; the comment consumed four members WHOLE), and
    # the line-wise path mis-pairs to the r168 commentary's death one
    # wrapper deeper: the structural skip burns the owed closer on
    # wrapper 1's own "-MAP", r124/r127 graze the eaten members against
    # wrapper 1's KEY, the r117 split arm consumes the suffix member
    # (its content ends with the KEY byte-exact), the emptied-key arm
    # eats the trailing member against wrapper 1's salvaged null, and
    # the closer bank eats "-MAP" -- wrapper 2's opener then faces
    # yeptris's "-SEQ" with struct_skips spent and no arm. The r172
    # ride generalizes the family's law to distance k: the ride claims
    # the killed member plus the eaten run when the killed member is
    # glue-led (the r167 lead-form pin), EVERY member between is a
    # kept-content PLAIN VAL (the comment's bytes are invisible in an
    # event dump, the r124/r127 content-agnostic precedent), the member
    # at distance k is a kept-content plain VAL whose content ENDS with
    # the wrapper's KEY byte-exact (the r148 render separator stripped
    # -- the direct observation that the KEY's bytes are the kept
    # plain's own tail, not an invented key), and the wrapper is the
    # well-formed salvage block -- consulted only after r143/r149/r168
    # declined (code order: distances 0/1/2 stay theirs, the shapes
    # stay disjoint). Bounded: the fixed four-line block, the
    # byte-exact suffix pin, both pointers advancing every claim, and
    # the shared skip cap charging the wrapper's four lines plus one
    # per consumed member (monotone in k -- the comma-separated
    # distance-56/61 twins abort BY DESIGN past the cap, the
    # distance-50 twin rides under it); a non-content member between
    # (the empty-val twin) declines and keeps aborting; the claim
    # stands only on full exhaustion (the walk's law). Minimality: all
    # 57 prefixes agree (the flow ']' is load-bearing by construction),
    # and every single-byte deletion lands green or a different-family
    # red (the RAW artifact is pinned). Post-fix the battery converges:
    # 51 mutants (20 single-byte flips + 10 multi-byte ins/sub/del + 21
    # targeted family variants, cap twins and controls) classify (15)
    # or hold identical verdicts pre/post (36), zero regressions; the
    # corpus walk is byte-identical pre/post on the shared 291 files
    # (30 historical aborts) with the new seed riding green (292 post);
    # leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r172-rekey-distance5
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qcomp-rekey-dist5-r172.bin
    )

    # the r173 nightly's seed (run 42283551985, main 1e6cb19, crash
    # d2bcd9d9): the RUN-FOLD CONTINUATION sibling of the r160/r161
    # block-content law -- 27 bytes: a folded-block-scalar header, then
    # the content line " !<[!p", then TWICE the line " !<" + two U+0085
    # NEL pairs + a trailing space. Both engines accept; the values
    # differ at the first run (the byte-exact dumps, verified by the
    # lldb argument read): yeptris "=VAL : !<[!p !<NN  !<NN \n" -- the
    # pairs stay CONTENT (the 1.2 block-scalar break set is LF/CR/CRLF
    # only, spec 8.1), the content line's trailing space is kept AND
    # the fold space is emitted, so TWO spaces ride behind the run --
    # vs libyaml "=VAL : !<[!p !<\n\n !<\n": its 1.1 IS_BREAK splits
    # the line at the NELs, its empty-line scan eats the spaces BETWEEN
    # its literal breaks (3 breaks with a space between -> (n-1)=2
    # escaped breaks), and the continuation's space strips. The suite
    # sides with yeptris (1.2 content) -- a carry. The r160 arm
    # DECLINES this member: at the first run it consumes the (k-1)
    # collapse + ONE space<->break pair, then yeptris's SURPLUS fold
    # space faces libyaml's " !<" outside any run (the space-surplus
    # arm sits behind the space<->break continue and the loop-top
    # byte-equality check kills it first). The extension generalizes
    # the run's-exit re-alignment into a bounded continuation: up to k
    # space<->break PAIRS (one-for-one, both pointers advancing)
    # followed by the surplus spaces -- yeptris's spaces libyaml ate as
    # the continuation's indentation, or libyaml's fold spaces with no
    # yeptris counterpart -- every arm firing within a total budget of
    # k steps (the run's own length: the claim cannot outrun the run)
    # and every iteration advancing at least one pointer; the walk past
    # the section stays byte-bound, so a dropped member's frame bytes
    # cannot ride (probed byte-exact: the seed accepts, a dropped tail
    # frame byte / changed tail frame / any post-section divergence
    # rejects). The r161 terminal-tail end-anchor stays binding above
    # the extension, untouched -- every ride ends at it with
    # byte-identical remainders (the second run of the seed and of the
    # k=3/k=4 ladder, the glue twins, the flips). The corrupted staged
    # copy of the artifact (an earlier printf had inserted a backslash
    # at offset 4, " !\[!p") is a DIFFERENT parse -- it agrees on both
    # engines and stays agreeing pre/post; the REAL nightly artifact
    # bytes are canonical. Post-fix the battery converges: 50 mutants
    # (20 single-byte flips + 10 multi-byte ins/sub/del + 20 targeted
    # family variants and controls) classify or hold identical verdicts
    # pre/post (7 rides: the seed, two flips that glue/rename the
    # content, the deletion twin, the k=3/k=4 run ladder and the
    # dedent twin whose dumps are byte-identical to the seed's), zero
    # regressions; the corpus walk is byte-identical pre/post on the
    # shared 292 files (30 historical aborts) with the new seed riding
    # green (293 post); leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r173-block-nel-run-fold-continuation
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-run-fold-continuation-r173.bin
    )

    # the r174 nightly's seed (run 42296429313, main 3787be5, crash
    # 1a5cbe0a): 15 bytes "&a:o&a:o::\n*a:\n" -- the r157/r170
    # root-alias collapse's DOUBLED-HEAD sibling (the r170 seed with
    # the mutation walk's second "&a:o" inserted; a NEW byte form, so
    # per the r151/r170 deletion-sibling precedent it takes its own
    # exact-bytes gray claim beside yd_r157/yd_r170). Both engines
    # accept; yeptris collapses the document to ONE root alias (the
    # raw-event probe: ALIAS value "a:", anchor "a:o&a:o::") -- the
    # prop-char law ends a name only at blank/break/flow-indicator/
    # comma/'#', so the interior '&' rides the anchor name beside the
    # colons (Y2GN pins the colon ride; the '&'-in-name split vs
    # libyaml's IS_ALPHA cut is the r142 anchor-charset family, a
    # documented comparator carry), the trailing "::" passes the
    # riding-colon un-consume gate (the follower scan crosses the break
    # to '*', no flow/block-scalar opener) and the anchor pends; line
    # 2's riding alias swallows its colon so no value indicator ever
    # opens a map. libyaml's 1.1 cuts compose its r170 column-0 shape
    # one '&' run richer: anchor "a" (r142/r76), residual ":o&a:o::"
    # re-lexes as plain ":o&a:o:" (mid-plain '&' is CONTENT) with the
    # final colon as the value token -- the anchored KEY -- and the
    # alias lands as a second flat pair (alias key, empty plain value)
    # in the same map. The charset-split twin declines both
    # orientations: the blank-split copy is rejected by yeptris itself
    # (a second props run on one node, the suite's 4JVG), and the
    # reversed orientation is the 'yeptris =ALI vs libyaml anything'
    # mask the r137-r139 verdicts ruled unsound (the r163
    # alias-content-tail arm is the reverse orientation and declines by
    # construction). Minimality: prefixes 0-12 agree, prefix 13
    # ("...\n*a", no colon) is the libyaml-reject alias-only split
    # member, prefixes 14/15 diverge identically (the trailing break is
    # NOT load-bearing -- the breakless twin stays unclaimed); the
    # '::' tail is not load-bearing either (the single-colon member,
    # the 'a:x' interior variant, the triple head, and the head-only
    # '&a::' member diverge with the same skeleton -- each its own
    # entry or an engine dig). Post-pin the battery converges: 50
    # mutants (20 single-byte flips + 10 multi-byte ins/sub/del + 20
    # targeted family variants: the r157/r170 seeds stay green, the
    # single/triple heads, the '::'-vs-':' tails, the name/anchor
    # flips) classify or hold identical verdicts pre/post, zero
    # regressions; the honest corpus walk is byte-identical pre/post on
    # the shared 293 files (30 historical aborts) with the new seed
    # riding green (294 post); fresh-configure ctest carries all 67
    # prior pins plus this one (68/68); leaks --atExit clean on the
    # seed.
    add_test(
        NAME fuzz-diff-r174-root-alias-double-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-collapse-double-head-r174.bin
    )

    # the r175 nightly's seed (run 42304338744, main 3552e51, crash
    # 1fff0dca): the FOLD-PREFIX + CLIP-BREAK-SURPLUS sibling of the
    # r160/r161/r173 block-content law -- 14 bytes: a folded-block-scalar
    # header, the content line " ! ", then the line " !<" + two U+0085
    # NEL pairs + the final LF. Both engines accept; the values differ
    # at the run's exit (the byte-exact dumps, captured by the
    # pipe-through-xxd method): yeptris "=VAL : !  !<NN\n" -- the pairs
    # stay CONTENT (the 1.2 block-scalar break set is LF/CR/CRLF only,
    # spec 8.1), the fold space rides behind the content line's kept
    # trailing space, and the scalar's final LF stays as the trailing
    # (clip) break -- vs libyaml "=VAL : !  !<\n": its 1.1 IS_BREAK
    # splits the content line at the NELs, and its trailing-break
    # accounting absorbs the final LF into the NEL region it collapses
    # (k NEL breaks + the final LF -> (k-1) collapsed -> clip-trimmed
    # to the single break the (k-1) loop consumes). The suite sides
    # with yeptris -- a carry (the r160 law verdict stands; the
    # engine-side reading is spec-8.1 correct on the composed form).
    # The r173 continuation DECLINES this member: after the collapse,
    # yeptris's clip break faces libyaml's end-of-value and every
    # continuation flavor (space<->break pair / surplus yeptris space /
    # surplus libyaml space) requires a space on one side -- steps=0,
    # decline (pinned by a Python sim transcribing the arm verbatim).
    # The refinement adds the fourth flavor INSIDE the same budget-k
    # loop: yeptris's escaped break with no b counterpart fires like
    # the surplus spaces; the r161 terminal-tail end-anchor above
    # stays binding and untouched (it still fires first for its
    # SP+break tail with byte-identical remainders), a b-side escaped
    # break never satisfies the new branch (a longer libyaml tail
    # still declines), and the walk past the section stays byte-bound
    # -- a dropped member's frame bytes cannot ride. Minimality: every
    # single-byte deletion changes the verdict (agreements, lone-lead
    # accept/reject splits, or same-law siblings); the prefix ladder
    # 1-9 and 11 agree (the k=1 bare tail rides the main loop's byte
    # equality), divergence onset at the full pair structure; the
    # " !NN"-glued twin (no verbatim) normalizes to LF and coincides
    # with libyaml -- an agreeing control by two different routes.
    # Post-fix the battery converges: 56 members (20 single-byte flips
    # + 10 multi-byte ins/sub/del + 25 targeted family variants and
    # controls) classify or hold identical verdicts pre/post -- 9
    # rides: the seed, the glue flip, the k=3/k=4 ladder, the
    # run-without-fold-prefix twin, the two-' ! '-lines twin, the
    # glued-single-line twin, the dedent twin, the breakless twin
    # (dumps byte-identical to the seed's); the surplus-space member
    # stays red at budget k=2 (documented), the lone-lead-byte /
    # no-lead-sp / seq-composition negatives stay red (accept/reject
    # splits and a different-law multi-line literal-fold sibling);
    # zero regressions. The corpus walk is byte-identical pre/post on
    # the shared 294 files (30 historical aborts) with the new seed
    # riding green (295 post); leaks --atExit clean on the seed.
    add_test(
        NAME fuzz-diff-r175-block-nel-clip-break-surplus
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-content-prec-line-r175.bin
    )

    # the r176 nightly's seed (run 42311445947, main 9748e6d, crash
    # 2611c5939bcf8ee920ec74b2a81cf0622833a70c): the CONTENT-AFTER-RUN
    # + BREAKLESS-EOF composition of the r160/r161/r173/r175
    # block-content law -- 13 bytes: a folded-block-scalar header, then
    # ONE content line " !<" + one U+0085 NEL pair + " text9", ending
    # at EOF with NO trailing break. Both engines accept; the values
    # differ at the tail (the byte-exact dumps, captured by the
    # pipe-through-xxd method): yeptris "=VAL : !<NN text9\n" -- the
    # pair stays CONTENT (the 1.2 block-scalar break set is LF/CR/CRLF
    # only, spec 8.1), the fold space rides behind it, and the r140
    # breakless-EOF clip law appends the trailing break -- vs libyaml
    # "=VAL : !< text9" (bare): its 1.1 IS_BREAK folds the NEL into
    # the fold space and its breakless tail drops the break entirely.
    # The suite sides with yeptris on BOTH carries (the r160 law
    # verdict stands; the engine-side reading is spec-8.1 + L24T#2
    # correct on the composed form), so this is a carry of two
    # adjudicated verdicts composing in one input -- the r158 lesson.
    # The existing arms decline the composition: the r158
    # composed-transform arm's data gate owns the LS/PS TRIPLE only
    # (E2 80 A8/A9 -- the PS/LS twins of this exact shape ride it and
    # are green), and the block-NEL walk's section arms all live
    # BEHIND the run: the run's exit agrees byte-for-byte (the fold
    # space), the walk past it stays byte-bound, and the divergence
    # (yeptris's trailing "\n" escape vs libyaml's frame LF) hits the
    # strict main-loop law with no surplus arm -- the r175 surplus
    # flavor sits INSIDE the bounded continuation, which only fires
    # when the byte right behind the run mismatches. The refinement
    # composes the two deltas in the clip arm's own direction (the
    # r158 arm's shape, the yd_clip_breakless_divergence move): the
    # family walk is extracted verbatim (yd_block_nel_walk) and the
    # arm, after that walk DECLINED the original dumps and only on a
    # breakless final byte (the clip precondition; the 8192 cap
    # bounds the re-dump), appends the ONE break libyaml drops to a
    # copy of libyaml's input, re-dumps libyaml on THAT, and requires
    # the exact same byte-bounded walk against yeptris's original
    # dump. No existing arm weakened, no budget changed: the r161
    # terminal-tail branch still fires first, the pure members keep
    # their branches, and the LS/PS composition stays with the r158
    # arm (the families keep their own walks). Minimality: every
    # single-byte deletion changes the verdict (header/`!`/`<`
    # deletions land in already-owned families; the indent deletion
    # is the root-block col-0 sibling; the lead-byte deletions are
    # libyaml error shapes; every text-tail deletion is a same-law
    # member that rides post-fix). Post-fix the battery converges:
    # 61 members (20 single-byte flips + 10 multi-byte ins/sub/del +
    # 30 targeted family variants and controls) classify or hold
    # identical verdicts pre/post -- 9 rides: the seed, the literal-`|`
    # twin, the k=2 folded and literal twins, the k=3 ladder member,
    # the map-value twin, the two-words-after-run member, and the
    # two tail-byte deletions; the red set is byte-identical
    # pre/post (the indent-0 continuation siblings -- libyaml's
    # NEL-split line model ends the scalar early -- the lone-lead
    # error shapes, the deeper-indent/seq-entry geometry splits);
    # zero regressions. The corpus walk is byte-identical pre/post on
    # the shared 295 files (30 historical aborts) with the new seed
    # riding green (296 post); fresh-configure ctest carries all 69
    # prior pins plus this one (70/70); leaks --atExit clean on the
    # seed.
    add_test(
        NAME fuzz-diff-r176-block-nel-breakless-clip
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-breakless-clip-r176.bin
    )

    # the r177 nightly's seed (run 42316995946, main 0494937, crash
    # dfe2e81ec239a0c55f247b032d0a116b889bdcdb): the FOLD-SUPPRESSION
    # flavor of the r160/r161/r173/r175/r176 block-content law -- 13
    # bytes: a folded-block-scalar header, then ONE content line
    # " !<" + one U+0085 NEL pair + a trailing space, a blank-ish
    # continuation line, then a second content line " !<". Both
    # engines accept; the values differ at the first fold boundary
    # (the byte-exact dumps, captured by the pipe-through-xxd
    # method): yeptris "=VAL : !<NN  !<\n" -- the pair stays CONTENT
    # (the 1.2 block-scalar break set is LF/CR/CRLF only, spec 8.1),
    # and the fold keeps BOTH the line's trailing space and the fold
    # space -- vs libyaml "=VAL : !<\n !<\n": its 1.1 IS_BREAK turns
    # the NEL into the line break and its fold writes the space only
    # when the boundary carries no blank-line breaks (scanner.c's
    # `*trailing_breaks.start == '\0'` gate), so the suppressed fold
    # space and the blank line's break collapse into ONE escaped
    # break. The suite sides with yeptris (the r160 law verdict
    # stands; the engine-side reading is spec-8.1 correct), so this
    # is a carry of the adjudicated law's fourth mechanism. The
    # existing arms decline the composition: the r173 continuation's
    # budget-k exit carries one space<->break pair, but the
    # suppression adds a SECOND yeptris space (the fold space) with
    # no libyaml counterpart, so the byte-walk behind the run
    # misaligns by one space. The refinement makes the family entry
    # two-phase: the ORIGINAL budget-k walk first (every member that
    # rode before r177 keeps its exact path -- the continuation is
    # greedy with no lookahead, and the doubled budget alone flipped
    # the r173 seed's more-indented-line sibling from ridden to
    # aborting), then only on a decline the budget-2k retry, whose
    # extra surplus steps absorb the suppressed-fold space. The
    # bound stays byte-tight: mult*k steps (mult 2 = k alignments
    # plus k suppressed-fold/eaten-indent surpluses -- the claim
    # cannot outrun the run), every iteration advances at least one
    # pointer, content equality is still enforced behind every run.
    # Minimality: every single-byte deletion changes the verdict
    # (the header/indent deletions land in the root-block col-0
    # sibling; the lead-byte deletions are the r128 lone-spelling
    # shapes; the content-tail deletions are same-law members that
    # ride post-fix via the retry or the r176 composed transform).
    # Post-fix the battery converges: 60 members (20 single-byte
    # flips + 10 multi-byte ins/sub/del + 30 targeted family
    # variants and controls) classify or hold identical verdicts
    # pre/post -- 10 rides: the seed, the 2-line/3-line real
    # compositions, the lf-only-tail member, the map-value twin,
    # the indent-2 twin, the CR twin, the minimal 2-line shape, and
    # the flip/del mutants inside the second run's frame; the red
    # set is byte-identical pre/post (the seq-entry geometry --
    # libyaml-only error -- the lone-lead error shapes, the
    # same-line two-words-after-run r176 negative, the deeper
    # frame flips); zero regressions. The corpus walk is
    # byte-identical pre/post on all six harness corpora (parse 297
    # runs, feed 65, roundtrip 2, marshal 18, cbor 102, validate 5);
    # fresh ctest carries all 70 prior pins plus this one (71/71);
    # leaks --atExit clean on the artifact and the seed.
    add_test(
        NAME fuzz-diff-r177-block-nel-fold-suppression
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-fold-suppression-r177.bin
    )

    # the r178 nightly's seed (run 42322793822, main 68700ce, crash
    # 4663982fe3724e68041f75726a42a80cb3dc27c2): the GLUED-HASH family's
    # plain-first-'?' x alias-tail composition -- 9 bytes, minimal (every
    # single-byte deletion changes the verdict): "{?#\n,?*L}". Both
    # engines accept; the streams differ at the second member: yeptris
    # "=VAL : ?*L" (the suite-pinned plain-'?' law -- '?' followed by the
    # ns-plain-safe '*' is a legal plain FIRST char, so "?*L" is ONE
    # plain) vs libyaml "=ALI *L" (its flow-KEY token fires on the '?'
    # unconditionally and the '*' re-lexes as an ALIAS). The first member
    # rides the emptied-key pair ("?#\n" -- the glued '#' opens libyaml's
    # comment to the line end), but the alias pair declined everywhere:
    # the r163 alias-as-content-tail arm demands the glue fingerprint
    # LOCALLY in the pre-spelling prefix (a glued "?#"/"?!" inside it),
    # and here the prefix is exactly the one '?' byte -- the marker lives
    # in the OTHER member -- so r163 declined and the r125 alias-realign
    # arm bailed (yeptris never emits its own =ALI). The r163 gate gains
    # the plain-first-'?' form byte-tight (prefix length pinned to 1,
    # byte pinned to '?', the alias spelling still the content's
    # byte-exact suffix, libyaml's alias PRESENT so the r138-r141
    # alias-drop masking cannot apply, family-gated by pass 1). The
    # 58-byte artifact rides the second refinement: libyaml's
    # comment-glue ate the marker member ("?#?#|##|#|") WHOLE, and the
    # plain r124/r127 y-only skip leaves its EMPTY value line to
    # mis-pair -- the L-only arm then eats libyaml's residue and every
    # later pairing runs one member off (death at yeptris's empty vs
    # libyaml's "=ALI *L"). The KILLED-MEMBER FRAME skip consumes both
    # lines of the marker member (the glue-marked content VAL plus its
    # exact "=VAL : " partner -- the frame tell pins the shape
    # byte-exact) and KEEPS libyaml's line, so the fold-tail residue "z"
    # suffix-pairs yeptris's next surviving member "` z" through the
    # existing r117 SPLIT arm; the alias then rides the widened r163
    # form at the third member. One skip count per frame (the r146 soup
    # rides 96 kept-content members past the shared 64 -- a per-line
    # count outruns the cap mid-stream). Post-fix battery: the seed, the
    # 58-byte artifact, the "?*d" alias-name sibling and the r146
    # terminal-tail soup all classify known; the single-byte-flip
    # neighborhood is SPLIT-dominated (libyaml rejects the flipped
    # followers -- the documented suite-gray zone); corpus walk
    # byte-identical pre/post on all six harness corpora; fresh ctest
    # carries all 71 prior pins plus this one (72/72); leaks --atExit
    # clean.
    add_test(
        NAME fuzz-diff-r178-qflow-glued-hash-plain-question-alias
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-plain-question-alias-r178.bin
    )

    # r179 (run 42331311239, main e201a69, crash-0b3aacc6b8fea8fe8861224e5406e0301f3220fd):
    # "{?#??\t*L:\n( `,*L:(}" (19 bytes, every single-byte deletion
    # changes the verdict) is the r142/r76 IS_ALPHA charset law COMPOSED
    # into the glue family. libyaml's scan_anchor cuts the alias name at
    # the first byte outside [0-9A-Za-z_-] -- here the ':' gluing the
    # member's own value indicator -- freeing that byte for its VALUE
    # token and the tail for a plain value, while yeptris follows the
    # spec's ns-anchor-char (c-flow indicators and white excluded only),
    # so ONE run "*L:( }" renders yeptris "=ALI *L:(" with the value
    # eaten EMPTY and libyaml "=ALI *L" + "=VAL : (". The glue side
    # supplies the framing: the pass-1 "?#" marker, libyaml's
    # comment-glue eating the first member whole (its KEY token fires on
    # the '?', the glued '#' comments to the line end), and yeptris's
    # surviving value pairing libyaml's re-paired key byte-exact. The
    # walk pre-fix died twice with no arm: yeptris's extended alias
    # faced libyaml's salvaged null (the r116 l-only skip demanded a
    # VAL/empty/'-' y-line), then the extended alias faced libyaml's cut
    # alias (r125 demands a y-VAL, r163 demands libyaml's alias be a
    # suffix of yeptris's VAL content, r144 demands the exact line
    # survive libyaml's dump). The carry: the r179 ALIAS-CHARSET-CUT
    # pair -- byte-exact end to end (proper-prefix name, ':' cut byte,
    # libyaml-charset kept bytes, libyaml's next line "=VAL : " plus the
    # exact eaten suffix, yeptris's next line the exact empty val) -- and
    # the r116 l-only-empty gate widened to accept a yeptris "=ALI " line
    # (the salvaged null may sit directly before the re-positioned alias;
    # the alias itself must still pair byte-exact downstream or the
    # claim dies). Post-fix battery: the 25-byte artifact and this seed
    # classify known; 39 paired mutants pre/post -- 0 regressions, 14
    # carried (the artifact, the seed, the "?!" marker flavor, value and
    # multi-byte compositions), 19 red in both (the documented gray
    # zone: the colon-less "("-cut sibling keeps aborting BY DESIGN --
    # its cut byte carries no VALUE token, so libyaml re-lexes the tail
    # as an EXTRA key and the member counts diverge); corpus walk
    # byte-identical pre/post on all six harness corpora (parse = 298
    # with r178's accounted seed); fresh ctest carries all 72 prior pins
    # plus this one (73/73); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r179-qflow-glued-hash-alias-charset-cut
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-alias-charset-cut-r179.bin
    )

    # r180 (run 42336246527, main 8619d71, crash-d6d32ef72a558f283275477dd0e9203349ba83f2):
    # the 1728-byte soup ("{?. ,?ml?. ..." -- ?#-glued members separated
    # by CR/LF runs) is the r178 KILLED-MEMBER FRAME skip composed at
    # soup DENSITY. libyaml's post-'?'-token comment-glue law kills the
    # marker member AND ITS SAME-LINE NEIGHBORS whole (the comment after
    # the KEY token runs to the NEXT BREAK, so "?#,0,<CR>" kills the
    # marker AND "0"; "?#,=,<LF>" kills marker AND "="), while yeptris's
    # suite-pinned laws keep every member. The r178 frame skip covered
    # only the marker itself (its fingerprint pin is LOCAL), so every
    # neighbor cost TWO shared-cap skips (the y-only content skip plus
    # the L-only partner skip) and the bare-'?' explicit-key member ate
    # libyaml's SURVIVORS through the L-only arm -- the artifact's
    # ~47-member runs outran the shared 64 in BOTH walk passes. The
    # carry: the marker frame's run grant (frame_run, refilled by every
    # marker frame, capped at 16 -- the comment cannot cross a break, so
    # the run is line-local by construction) carries the NEIGHBOR
    # frames (content + exact empty partner, keep-l, ONE skip each),
    # the DOUBLE-EMPTY frames (the bare-'?' {null: null} member -- the
    # r116 y-only-empty skip would leave the pair one member off), and
    # the MARKER-FRAME PRIORITY over the r117 SPLIT reading (libyaml's
    # surviving later member may suffix-match the killed plain's tail --
    # "?#g" vs "g" -- and the split reading would strand yeptris's own
    # survivor; a false priority can only leave the DIFFER visible, the
    # claim still dies on non-exhaustion); the shared skip cap scales
    # for the frame pass (64 + size/8 -- the walk is line-bounded by
    # construction, frame 0 keeps the classic 64). Post-fix battery:
    # the artifact, the 89-byte and 193-byte minimals classify known;
    # 46 paired mutants -- 0 regressions, 16 carried, 31 red in both
    # (the documented gray zone); corpus walk byte-identical pre/post
    # on all six harness corpora (parse = 299 with r179's accounted
    # seed); fresh ctest carries all 73 prior pins plus this one
    # (74/74); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r180-qflow-glued-hash-comment-run
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-comment-run-r180.bin
    )

    # r181 (run 42341337776, main 225ccd4, crash-0d856bdbeef74a848e253f4382fdafe6be4b86fd):
    # the 240-byte soup ("[-123.123fo\r..." -- ?#-glued members, CR/LF
    # breaks, LS fold triples, &m-anchored empties) is the comment-run
    # kill STRADDLING a collection boundary. libyaml's post-'?'-token
    # comment-glue law (the '#' at token scan position comments to the
    # NEXT break) ate the glue-led member's tail together with the
    # members following it on the same line, while yeptris's
    # suite-pinned plain-first-'?' law keeps every glued run as content
    # (a '#' comments only behind a blank, the HM87/r85 law) -- so
    # yeptris carries a run of KEPT members that libyaml has none of,
    # and libyaml's current line is the OPENER of a collection BOTH
    # skeletons carry downstream. Every earlier route mis-pairs there:
    # the wrapper rides read the block as a killed member's SALVAGE and
    # consume it, the r132 shared-opener tell demands yeptris's
    # IMMEDIATE next line be the opener (the killed run's residue VALs
    # sit between), and the bare structural skip then eats libyaml's
    # opener of the SHARED block while yeptris's own copy is still
    # pending -- the walk died at yeptris's opener against libyaml's
    # body VAL with no arm in BOTH walk passes. The carry: the
    # SHARED-BLOCK ride (the r152 RE-PAIRED-VALUE pattern on the
    # content side, pinned to the WHOLE four-line block) -- yeptris's
    # glue-led member plus its kept residue (=VAL lines only, the r152
    # per-site cap of 16) step aside until yeptris's OWN byte-exact
    # copy of opener + KEY + salvaged null + bare closer; libyaml's
    # opener is NOT consumed and pairs as an equal line on the next
    # step. Frame pass only (the r178 routing); the glue-led lead pin
    # is the r167 family's own; bounded by the run cap and the shared
    # skip cap. Post-fix battery: the artifact and the 12-byte
    # minimality twin "[2,?#,?|,?\r]" classify known; 50 paired
    # mutants (32 single-byte flips, 10 span deletions, 8 targeted) --
    # 0 regressions, 30 carried, 17 red in both (the documented gray
    # zone), 3 pass in both; corpus walk byte-identical pre/post on
    # all six harness corpora; fresh ctest carries all 74 prior pins
    # plus this one (75/75); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r181-qflow-glued-hash-shared-block
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-shared-block-r181.bin
    )

    # r182 (run 42345850260, main fdbf503, crash-1482cdc94456ff989f9db92f6b0dd224b4b72cda):
    # the 11-byte soup "&a::\n  *a:\n" is the r157/r170/r174 root-alias
    # collapse's "&a::" INDENTED member (the r157 seed with the head's 'o'
    # deleted; the r157/r170 lists name the member, r174's names the
    # indented twin -- the r151/r170 deletion-sibling precedent gives it
    # its own entry). yeptris's suite-pinned riding laws (2SXE/W5VH:
    # colons ride anchor/alias names) compose line 1 "&a::" into a
    # props-only pended anchor and line 2 "  *a:" into a colon-riding
    # alias name, so no value indicator ever opens a map and the
    # document collapses to ONE root alias (+STR /=ALI *a: /-STR);
    # libyaml's 1.1 IS_ALPHA cut re-lexes the residual "::" as plain ":"
    # with the final colon riding as the value token, making line 1 the
    # anchored KEY of a root map and line 2's alias the key of a NESTED
    # map with an empty plain value -- the 9-event form. The prefix chain
    # diverges from the head-only "&a::" member on (the r174 list's
    # named sibling), so the entry pins the artifact byte-exactly; the
    # breakless/no-indent/onecolon/colonless-head/1-space siblings stay
    # unclaimed beside the pinned cut. A/B: pre-r181 8619d71 fails the
    # artifact identically -- a fresh finding, not an r181 regression.
    # Post-fix battery: the artifact classifies known; 50 paired mutants
    # (20 single-byte flips, 10 span deletions, 20 targeted) -- 0
    # regressions; corpus walk exit-0 pre/post on all six harness
    # corpora, walk outputs identical (normalized); fresh ctest carries
    # all 75 prior pins plus this one (76/76); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r182-root-alias-colonhead
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-colonhead-r182.bin
    )

    # r183 (run 42386647198, main 9e4cbe1, crash-0732517306d80ae0f1163934edb5f99005bcfca0):
    # the 20-byte "[-1.0<PS> 7.tag<CR> ,?.]" is the flow-? glued-follower
    # family's SEQUENCE orientation, a fresh finding (A/B: pre-r182
    # fdbf503 fails the artifact identically -- not an r182 regression;
    # the r182 arm is an exact-bytes root-alias gray entry, unrelated).
    # The first member is a multi-line flow plain folding over PS then
    # CR ("-1.0 7.tag", identical on both sides); the trailing ",?.]"
    # is where the engines split: libyaml's 1.1 law makes a KEY token
    # for a flow '?' with ANY follower (scanner.c fetch_next_token's
    # "flow_level ||" arm), so '.' becomes the key of a single-pair
    # mapping the enclosing SEQUENCE wraps as a nested member
    # (+MAP =VAL : . =VAL : -MAP); yeptris's suite-pinned plain reading
    # (HM87/652Z: a flow '?' with a non-space follower rides as plain
    # content) keeps "?." ONE plain scalar member. The r71 drop-'?'
    # spell cannot reproduce that reading -- in a MAPPING the dropped
    # '?' leaves the follower a key-position plain (libyaml's same
    # pair), but in a SEQUENCE it degrades to a plain member and the
    # nested pair vanishes, which is exactly why every earlier arm's
    # verify declined this orientation. The new arm spells the pair
    # explicitly -- "{run: }" -- which both engines read as the nested
    # pair libyaml produced, with an LS/PS-normalized verify fallback
    # (libyaml's dump keeps the mid-plain U+2029 raw while yeptris
    # folds it; normalizing LS/PS/NEL/CR -> LF on both sides is
    # meaning-preserving and the triple compare arbitrates). The
    # two-break prefix is the gate's composition ingredient: the
    # single-break twin "[a<CR> ,?.]" rides the pre-existing arms, so
    # the entry pins the artifact byte-exactly. Post-fix battery: the
    # artifact classifies known; 50 paired mutants (20 single-byte
    # flips, 10 span deletions, 20 targeted) -- 0 regressions, 22
    # newly carried (the map-context/?:/alias/tag follower siblings
    # stay unclaimed beside the pinned cut, all pre-diverged);
    # corpus walk exit-0 pre/post on all six harness corpora, walk
    # outputs identical (normalized; the parse delta is the r182
    # seed's +1 file, cross-checked); fresh ctest carries all 76
    # prior pins plus this one (77/77); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r183-flow-seq-question-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-question-pair-r183.bin
    )
    # r185 (run 42392584152, main 3c87d4a, crash-187062ddd3ac7c5035c0183d3bf7dce08a24cf29):
    # the 52-byte "[11.0e ,1.0e ,?<U+0503>,:<CR>&binary<CR>, ?<U+0503>,:<CR>&binary<CR>, (?,]"
    # is the glued-hash family's COMMENT-CR PAIR orientation, a fresh
    # finding (A/B: post 3c87d4a, mid f01c0e0 -- the r183 arm -- and pre
    # 9e4cbe1 -- the r182 main -- ALL abort identically; the #1077 tape
    # perf cut is exonerated twice over, its walk_lenient_fused is not
    # even linked into this harness, so the escalation clause does not
    # fire). libyaml's 1.1 KEY-token law makes the flow '?' a KEY token
    # whatever follows, and the scanner's next-token loop then treats
    # the GLUED '#' as a boundary comment: the comment swallows
    # "?<U+0503>,:" up to the CR and the CR exposes the next line's node
    # as the keyless pair's key -- a plain run rides as {i: <empty>}, an
    # anchor rides as the anchored empty scalar {&binary<empty>: <empty>}.
    # yeptris's suite-pinned plain reading keeps "?#..." ONE plain
    # member and binds the bare ':' pair to the post-break run as its
    # VALUE, with the map persisting across the ',' -- the r132 walk
    # cannot pair the shapes because its emptied-key law expects
    # libyaml to keep LESS content, while this orientation PROMOTES the
    # follower into the key slot yeptris leaves null. The new arm
    # spells the swallowed span into the explicit single-pair map --
    # '?' + comment-through-break + follower becomes "{follower: }",
    # with the anchor spell riding TWO separators ("{ &binary : }") --
    # yeptris's wider anchor-name charset (the r142/r179 law) swallows
    # a ':' glued to the name. The finder only fires on a '?' glued to
    # '#' at flow depth in a SEQUENCE frame whose comment span reaches
    # a line break and whose post-break follower starts a same-line
    # plain run or an anchor-then-boundary empty ride; the byte-strict
    # double verify against libyaml's ORIGINAL dump (both engines must
    # read the rewrite exactly as libyaml read the original)
    # arbitrates, with the CR/LS/PS/NEL-normalized triple as the
    # fallback. Post-fix battery: the artifact and its 12-byte
    # delta-minimized twin "[1,?#,:<CR>i,?]" classify known; 587
    # mutants (single-byte flips, span deletions, targeted follower/
    # break variants over both forms) -- 0 regressions; corpus walk
    # exit-0 pre/post on all six harness corpora, walk outputs
    # identical (normalized; the parse delta is this seed's +1 file,
    # cross-checked); fresh ctest carries all 77 prior pins plus this
    # one (78/78); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r185-glued-hash-comment-cr-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-hash-comment-cr-pair-r185.bin
    )

    # the r186 round (run 42399477506, main 7bb0f69, crash-cc801571):
    # the nightly's fuzzer grew a qflow glued-hash soup PAST the arm
    # bank's shared 8192-byte cap (8984 bytes; the nightly sets no
    # -max_len, so len_control rides the seed corpus's own maximum).
    # The composition itself is fully adjudicated — the killed-member
    # frames ride the r132 shared-opener tell, the multi-line plain key
    # riding the '?'-KEY + glued-comment law carries the r150/r162/r164
    # LS/PS fold pairs ("?##|,?\r/\n\r\r<PS> 50:" — libyaml joins the
    # raw triple where yeptris's b-char algebra folds the run) — and
    # the frame-1 walk exhausts BOTH skeletons byte-exact once the
    # size gate lets it run. The carry is the gate alone:
    # yd_qflow_glued_hash_differ is allocation-free end to end (pass-1
    # marker scan + line-wise walk on the ORIGINAL dumps), so its gate
    # moves above the corpus maximum (262144) where the family's
    # transform arms keep their shared 8192. Regression triage: the
    # parent commit 3c87d4a aborts on the artifact identically (fresh
    # cap finding, not an r185-arm regression). Post-fix battery: the
    # artifact classifies known (exit 0); the byte-exact 20-byte
    # delta-minimized twin "[:inary\r\r ,?##\r\n&m,]" is the split
    # family's own shape (YEPTRIS-ONLY, the r132 consult's verdict);
    # 60 mutants (single-byte flips, span deletions, cap-boundary
    # truncations) -- 0 regressions; corpus walk exit-0 pre/post on all
    # six harness corpora, walk outputs identical (the parse delta is
    # this seed's +1 file, cross-checked); fresh ctest carries all 78
    # prior pins plus this one (79/79); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r186-qflow-glued-hash-soup-sizegate
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-soup-sizegate-r186.bin
    )

    # the r187 round (run 42404973005, main 4865289, crash-
    # d5a609db0f966ebae750560b629652d9042284f0): the 52-byte
    # "[11n.0e ,?##|,:|,:binary<CR>b's'<LF>        bravo: 0x788,]" is
    # the glued-hash family's FOLDED-PLAIN-PAIR orientation, a fresh
    # finding (A/B: pre b9a511f and post 4865289 abort identically --
    # the artifact is 52 bytes, far under the family's shared 8192
    # transform gate, so the r186 size-gate widening is exonerated; the
    # escalation clause does not fire, yeptris's reading is the
    # strict-1.2 one). libyaml's 1.1 KEY-token law makes the flow '?'
    # a KEY token whatever follows, and the scanner's next-token loop
    # then treats the GLUED '#' as a boundary comment: the comment
    # swallows "...##|,:|,:binary" up to the CR, and the pending
    # explicit KEY context then admits what the simple-key law forbids
    # elsewhere -- the post-break MULTI-LINE PLAIN follower folds
    # ("b's'<LF>        bravo" turns "b's' bravo", the apostrophes
    # riding as mid-plain content) and binds ": 0x788" as its value,
    # one keyless pair. yeptris's suite-pinned HM87 law ('#' mid-plain
    # after a non-blank is content) keeps "11n.0e", "?##|", ":|" as
    # members and reads ":binary b's' bravo: 0x788" as its own pair.
    # The r185 arm keeps its follower same-line and colon-free, so this
    # orientation declines there; deletion mutants also reach the
    # EMPTY-COLON twin "[?#,|,:y<CR>:]" (the bare ':' completes the
    # keyless pair with an empty key and value). The new arm's
    # preserving spell deletes the swallowed span and RESTORES the
    # explicit '?': "?#...<break>" turns "? " with the post-break tail
    # riding untouched -- libyaml keeps its KEY context (the
    # "{k: v}" brace form cannot carry the multi-line key: libyaml's
    # simple-key law rejects it outright) and both engines then read
    # the tail identically. The finder fires on a '?' glued to '#' at
    # flow depth in a SEQUENCE frame whose comment span reaches a line
    # break and whose post-break tail is bracket-free (plain/quoted
    # content, ':'s binding values, at most 8 fold breaks) closed by
    # ',' or ']'; the quote tracker only OPENS a scalar at a token
    # start (a quote mid-word is plain content in both engines), and
    # the byte-strict double verify against libyaml's ORIGINAL dump
    # (both engines must read the rewrite exactly as libyaml read the
    # original) arbitrates, with the CR/LS/PS/NEL-normalized triple as
    # the fallback. Post-fix battery: the artifact and the 11-byte
    # delta-minimized twin classify known (exit 0); 80 mutants
    # (single-byte flips, byte deletions, targeted span deletions,
    # truncations, random deletions/chaos) -- 0 regressions, 39 newly
    # claimed, 0 still-differ; 19-form control table (split siblings,
    # spell rewrites, brace-declines) -- verdicts unchanged; corpus
    # walk exit-0 pre/post on all six harness corpora, walk outputs
    # identical (the parse delta is this seed's +1 file, cross-
    # checked); fresh ctest carries all 79 prior pins plus this one
    # (80/80); leaks --atExit clean.
    add_test(
        NAME fuzz-diff-r187-glued-hash-plain-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-plain-pair-r187.bin
    )

    # the r188 round (run 42410383201, main 42c22b2, crash-
    # 9804dfee0001eb3191e6833a692bec5efda96e53): the 8-byte
    # ";!<CR><NEL><CR>;<CR>" is a genuine ENGINE bug in the break
    # normalizer's rewrite, not a comparator carry (the escalation
    # clause fires; the A/B against the parent 4865289 aborts
    # identically, so the r187 arm is exonerated). The NEL/LS/PS ->
    # '\n' copy did not look at the preceding byte: a lone raw CR
    # riding just before the rewrite fused with the replacement into a
    # phantom CRLF pair, and the byte readers count CR LF as ONE break
    # (SKIP_LINE's pair law) where the input's CR + NEL were TWO — the
    # plain fold's (n-1) newline tally halved (";!\r<NEL>\r;\r" read
    # ";!\n;" where libyaml and the spec fold ";!\n\n;"; the
    # minimal form "a\r<NEL>\rb" shows it bare). The r47
    # space-vs-'\n' NEL arm covered the single-break residue, so the
    # family hid until the run grew a THIRD break around the pair.
    # The fix rewrites the whole trailing run of raw CRs to '\n'
    # before the replacement lands: one CR is one break, LFs never
    # pair with each other, and a shallower rewrite would just move
    # the phantom pair one byte left ("a\r\r<NEL>" -> "a\r\n\n"
    # re-pairs on the first CR; the run makes it "a\n\n\n", breaks
    # in, breaks out). Both engines then read the artifact
    # ";!\n\n;". Battery: artifact pre-fix exit 134 / post-fix 0;
    # 50-mutant paired battery (targeted CR/NEL/LS/PS forms + random
    # flips/insertions/deletions) -- 0 regressions, 20 newly agreeing,
    # 8 fail-both unchanged (prior comparator carries), 22 both-pass;
    # corpus walk exit-0 pre/post on all six harness corpora plus
    # schema_compile, outputs identical (parse 307, feed 65, roundtrip
    # 2, marshal 18, cbor 102, validate 5, schema_compile 13); fresh
    # ctest carries all 80 prior pins plus this one (81/81); leaks
    # --atExit clean; clang-format clean.
    add_test(
        NAME fuzz-diff-r188-cr-exotic-phantom-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/norm-cr-exotic-phantom-pair-r188.bin
    )

    # the r189 round (run 42415431809, main 31373b2, crash-
    # aece9943b9918db235388f7c0b5b0c42ee442102): the 7-byte
    # "> <NEL># t" is the r19 root-block-content carry spelled with a
    # Unicode break as the header's terminator. The A/B against the
    # parent 12bfeda aborts identically (and the input has no raw CR,
    # so the fresh r188 phantom-CR fix is untouched) -- a fresh
    # gate-widening, not an engine regression. yeptris normalizes the
    # NEL to LF (the #638 walk folds NEL everywhere outside quotes) and
    # then takes the col-0 '# t' line as content exactly like its LF
    # twin ("> \n# t", which rides this arm since r19), while libyaml's
    # reader ends the header at the raw NEL into the same floor-1 empty
    # scalar (scanner.c:3031 floors the auto-detect at 1). The arm's
    # line walk modeled only LF/CR terminators, saw ONE unterminated
    # line, and never entered block mode. The widening teaches the walk
    # the NEL/LS/PS terminators (a dangling multibyte lead rides as
    # content, the yep_scan_mb_break_len law), and the LS twin
    # ("> <LS># t") rides the same widened path. Battery: artifact
    # pre-fix exit 134 / post-fix 0; 90-mutant paired battery (all
    # flips/deletions + targeted NEL/LS/PS/CR/insert forms + seeded
    # random) -- 0 regressions, 36 newly riding, 21 fail-both unchanged,
    # 33 both-pass; corpus walk exit-0 pre/post byte-identical on all
    # seven lf corpora (parse 307, feed 64, roundtrip 1, marshal 17,
    # cbor 101, validate 4, schema_compile 12, the nightly's
    # ulimit -s 32768 for the 205KB nested-flow seed); fresh ctest
    # -R fuzz-diff carries all 81 prior pins plus this one (82/82);
    # leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r189-root-block-nel-header
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-block-nel-header-r189.bin
    )

    # the r190 round (run 42420197746, main 0fd2ff4, crash-
    # bf009b5896e99d9c8aca7b26a7ad86a7e613e7f9): the 32772-byte glued-
    # '#' soup ("[11 ,?##|,?\r/..." with CR/LF/NEL/LS/PS breaks through-
    # out) is the glued-flow-'?#' comment family's MULTI-LINE-FOLLOWER
    # member, a fresh finding (A/B: the parent 31373b2 aborts
    # identically -- the fresh r189 NEL/LS/PS terminator modeling in
    # yd_root_block_content_divergence is exonerated; the escalation
    # clause does not fire, yeptris's reading is the suite-pinned HM87
    # one). libyaml's 1.1 law makes the flow '?' a KEY token whatever
    # follows and the glued '#' a boundary comment, and the pending
    # explicit-KEY context then admits the post-break plain follower --
    # which here does NOT stop at the line break: scan_plain_scalar's
    # continuation folds the break and rides the NEXT line's quote as
    # mid-plain content ("g\r\"" reads "g \"" in both engines), so
    # r187's token-start tracker -- which opens a quoted scalar at the
    # break-refreshed token position -- hid the family's SECOND member
    # (",?#" riding an LS/PS terminator with an empty tail) inside the
    # phantom quote span, and the artifact carries 921 '?#' hits plus
    # the r183 flow-'?' explicit-key shape ("?g", whose run rides a
    # break, so r183 declines). The new arm is r187's finder with three
    # teach-ins: the comment terminator scan learns NEL/LS/PS
    # (multibyte, continuation-checked, the r189/yep_scan_mb_break_len
    # law), the walk tracks plain continuation (a break does not end a
    # plain run; a quote only opens a scalar when the run is closed),
    # and a second hit kind spells the r183 flow-'?' shape over
    # break-riding runs. The spells restore the explicit '?' ("?#...
    # <break>" -> "? " with the tail riding untouched; a "?<plain>"
    # run -> "? " likewise; libyaml's KEY token stays word-for-word,
    # yeptris flips to the explicit-key reading) -- the brace form
    # "{? }" ONLY for the empty-tail member (breaks only before the
    # close, where libyaml lands the empty-key pair and the bare
    # "? <exotic break>]" errors in yeptris; the brace form must not
    # carry a multi-line key: libyaml's simple-key law rejects it
    # outright, the r187 law). Gates: at least one hit must carry a
    # NEW-SHAPE feature (an exotic terminator, a quote absorbed
    # mid-plain, or a break-riding run) else the shape is
    # r183/r185/r187 turf; size bounded by the r186 gate (262144 --
    # the artifact is 32772 bytes, so the family's old 8192 transform
    # gate is superseded here). The byte-strict double verify against
    # libyaml's ORIGINAL dump arbitrates, with the
    # CR/LS/PS/NEL-normalized triple as the fallback (the artifact
    # rides one: its members carry raw LS/PS in scalar content where
    # yeptris follows the fold law). Minimizer drift documented: the
    # unconstrained deletion minimizer reaches a 5-byte SPLIT-family
    # twin ("[|,?]", yeptris-only-accept -- a different, uncarried
    # family), so the minimizer was re-run verdict-constrained
    # (DIFFER-only) to the 33-byte DIFFER twin
    # "[1<LS/PS x4>,?#,:\rg\r\"",?<LS/PS x2>]", which this arm
    # carries byte-exactly; the true artifact is seeded intact.
    # Battery: artifact pre-fix exit 134 / post-fix exit 0; 56-mutant
    # paired battery (seeded flips/deletions/truncations/targeted
    # PS/CR/quote/?#-span ops/chunk deletions/chaos) -- 0 regressions,
    # 32 newly claimed, 13 still-abort unchanged, 11 both-pass;
    # corpus walk exit-0 pre/post on the six lf corpora, outputs
    # identical modulo libFuzzer's volatile seed/rss lines (parse 308,
    # feed 64, roundtrip 2, marshal 18, cbor 102, validate 5; the
    # ulimit -s 32768 for the 205KB nested-flow seed); fresh ctest -R
    # fuzz-diff carries all 82 prior pins plus this one (83/83);
    # leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r190-glued-hash-multiline-follower
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-glued-hash-multiline-follower-r190.bin
    )

    # the r191 round (run 42426475722, main d07420b, crash-
    # 05a4b458213a98f879553cc9ed211875687ab3bb): the 20-byte artifact
    # "[-1.0<U+2029> '.tag<CR> ,?.]" is the r183 flow-'?' explicit-key
    # shape with a QUOTE-RIDING HEAD -- a fresh finding (A/B: the parent
    # 0fd2ff4 aborts identically, so the r190 arm is exonerated; the
    # escalation clause does not fire, yeptris's reading is the
    # suite-pinned HM87 one). The head plain run (-1.0) rides the PS
    # break per the fold law and absorbs the next line's quote as
    # mid-plain content ("g<break>\"" reads "g \"" in both engines --
    # the r190 law), where r183's unconditional quote-open opens a
    # phantom quoted scalar over the absorbed quote and cannot reach
    # its own flow-'?' finder at all (the tail "?." -- libyaml's KEY
    # token nesting the keyless pair as a single-pair map member vs
    # yeptris's plain "?.") hides inside the phantom quote span. The
    # fix widens the r190 arm's KIND 2 NEW-SHAPE gate: the walker
    # flags a quote absorbed into an OPEN plain run (rode_head_quote)
    # and the gate admits it alongside the break-riding run, so the
    # KIND 2 spell restores the explicit '?' ("?." -> "? ."; libyaml's
    # KEY token stays word-for-word, yeptris flips to the explicit-key
    # reading) and the byte-strict double verify arbitrates with the
    # CR/LS/PS/NEL-normalized triple as the fallback (the artifact
    # rides one: libyaml keeps the mid-plain PS raw while yeptris
    # folds it to a space, so the strict compare can never hold).
    # Minimizer drift documented: the unconstrained deletion minimizer
    # reaches a 1-byte SPLIT-family twin (a bare dangling 0xA9 lead,
    # yeptris-only-accept -- a different, uncarried family), so the
    # minimizer was re-run verdict-constrained (DIFFER-only) to the
    # 10-byte DIFFER twin "[0<U+2029>',?.]", which this arm carries
    # byte-exactly; the true artifact is seeded intact. Battery:
    # artifact pre-fix exit 134 / post-fix exit 0 (on BOTH the r190
    # baseline 681eb5e and the parent 0fd2ff4); 77-mutant paired
    # battery (targeted break swaps PS/LS/NEL/CR/LF/CRLF, follower
    # charset, quote toggles, two-hit forms, mapping orientation,
    # glued-hash crossover + seeded flips/deletions/truncations/
    # insertions/chaos over the artifact, the minimal, the r183
    # sibling, and the r190 seed head) -- 0 regressions, 23 newly
    # claimed vs the r190 baseline (the widening's exact delta), 8
    # still-abort unchanged, 46 both-pass; corpus walk exit-0 pre/post
    # on the six lf corpora, outputs identical modulo libFuzzer's
    # volatile seed/rss lines (parse 310, feed 64, roundtrip 1,
    # marshal 17, cbor 101, validate 5; ulimit -s 32768 for the 205KB
    # nested-flow seed); fresh ctest -R fuzz-diff carries all 83 prior
    # pins plus this one (84/84); leaks --atExit clean; clang-format
    # 18 clean.
    add_test(
        NAME fuzz-diff-r191-qflow-seq-question-quoted-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-question-quoted-head-r191.bin
    )

    # the r192 round (run 42430913685, main d86219b, crash-
    # db5cae45b2bc0a9213226fab206c8a970e836f02): the 16-byte artifact
    # "&al:o&a:o::\n*a:\n" is the r174 doubled-head root-alias collapse's
    # ALPHA-HEAD sibling -- byte-identical to the r174 seed with an 'l'
    # inserted after the head's 'a', a fresh finding (A/B: the r191
    # parent 0fd2ff4 aborts identically, so the rode_head_quote widening
    # is exonerated; the escalation clause does not fire, yeptris's
    # reading is the suite-pinned riding law the family has carried
    # since r157). yeptris reads line 1 "&al:o&a:o::" as a props-only
    # anchor declaration whose name rides BOTH colon runs and the
    # interior '&' to the break (the trailing "::" passes the
    # riding-colon un-consume gate -- the follower scan crosses the
    # break to '*', no flow/block-scalar opener), so the anchor PENDS;
    # line 2 "*a:" names "a:", no value indicator ever opens a map, and
    # the document collapses to ONE root alias event (3 dump lines).
    # libyaml's IS_ALPHA cut reads TWO alpha chars ("al" -- r174's head
    # cut at one, the head length being this member's whole delta), the
    # residual ":o&a:o::" re-lexes as plain ":o&a:o:" with the final
    # colon as the value token, so line 1 is the anchored KEY of a root
    # map and line 2's alias "a" at column 0 is a second flat pair
    # (alias key, empty plain value) -- the same 8-line map form. The
    # fix is the exact-bytes gray arm yd_r192_root_alias_alpha_head_gray
    # beside the r157/r170/r174/r182 family claims. Minimality:
    # prefixes 0-13 agree, prefix 14 diverges, 15/16 identically (the
    # trailing break not load-bearing); the 'l' deletion is the r174
    # seed (its arm claims it); the '&'/break/'*' deletions agree; the
    # two/three-alpha and alphanumeric heads, the head-only "&al::"
    # member, the single-trailing-colon member, the indented twin, and
    # the breakless twin diverge with the same skeleton -- each its own
    # entry or an engine dig. Battery: artifact pre-fix exit 134 /
    # post-fix exit 0; 55-mutant paired battery (20 seeded single-byte
    # flips, 10 span deletions, 20 targeted head-length/colon/break/
    # alias forms, plus the r157/r170/r174/r182 seeds and the artifact
    # as controls) -- 0 regressions, 1 newly claimed (the artifact, the
    # exact-bytes arm's exact delta), 46 both-abort unchanged, 8
    # both-pass; corpus walk exit-0 pre/post on the six lf corpora,
    # outputs identical modulo libFuzzer's volatile seed/rss lines
    # (parse 310, feed 64, roundtrip 1, marshal 17, cbor 101, validate
    # 5; ulimit -s 32768 for the 205KB nested-flow seed); fresh ctest
    # -R fuzz-diff carries all 84 prior pins plus this one (85/85);
    # leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r192-root-alias-alpha-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-alpha-head-r192.bin
    )

    # the r193 round (run 42434624908, main 6eec12e, crash-
    # abdb19e20b6100e0fdc16a8c59425954201a5c7f): the 14284-byte soup of
    # the qflow family whose unclaimed residue is the member '?"#|' -- a
    # fresh finding (A/B: the r192 parent d86219b aborts identically on
    # the same artifact, byte-identical dumps, so the r192 gray arm is
    # exonerated; the escalation clause does not fire). yeptris's
    # suite-pinned plain-first-'?' law (HM87/652Z) keeps '?"#|' ONE
    # plain (the quote glued to the indicator-led run is content, the
    # r145 word-start law), while libyaml's flow-'?' KEY-token law
    # consumes the '?' and frees the glued quote to OPEN a quoted
    # scalar at the token start -- the span rides to the NEXT
    # member-final quote (those both engines read as mid-plain content,
    # the fold law's absorption), swallowing the members between as the
    # KEY's folded value (the 755-byte dump line
    # "=VAL : #|,? 2002:binary ,..."), the pair closing with the
    # implicit empty value. The r183/r190 arms decline by construction
    # (their follower gates list quote chars among the indicators that
    # stay with the other qflow arms) and no walk route carries the
    # swallow -- the walk died at the span's trailing residue
    # ("y=[+MAP] l=[=VAL : 211]", the interior members' structure with
    # no counterpart). The fix is the transform arm
    # yd_r193_glued_question_quote_divergence beside the qflow cascade:
    # the preserving spell inserts ONE blank (glued -> separated),
    # which makes the quote a token start for BOTH engines (yeptris's
    # separated-'?' law opens the explicit-key map with the quoted
    # scalar as the KEY, the r151 explicit form; libyaml's token stream
    # is unchanged -- pinned by the ldump2 == ldump byte compare), and
    # the compound soup's remaining residue (the '?#'-comment kills and
    # the LS/PS fold pairs) must be claimed by the family's walk --
    # the r115 quote-out classifier's form the compound extends.
    # Minimality: the two glued hits (both ',"?"#|' members at offsets
    # 5702 and 10217) are the arm's own finder's only scores; the
    # prefix cut floors at 9685 (shorter forms get claimed by the walk
    # or turn into libyaml-only splits -- the DIFFER needs the full
    # soup's unclaimed residue), so the true artifact stays the seed.
    # Battery: artifact pre-fix exit 134 (on BOTH the r192 parent
    # d86219b and the worktree baseline) / post-fix exit 0; 45-mutant
    # paired battery (20 seeded single-byte flips, 10 span deletions,
    # 13 targeted quote-family forms, plus the r192 seed as control)
    # -- 0 regressions, 18 newly claimed, 19 both-abort unchanged, 8
    # both-pass; corpus walk exit-0 pre/post on the six lf corpora,
    # outputs identical modulo libFuzzer's volatile seed/address/counter
    # lines (parse 310/312, feed 64, roundtrip 1, marshal 17, cbor 101,
    # validate 5; ulimit -s 32768 for the 205KB nested-flow seed);
    # fresh ctest -R fuzz-diff carries all 85 prior pins plus this one
    # (86/86); leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r193-glued-question-quote
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/glued-question-quote-r193.bin
    )

    # the r194 round (run 42440637478, main f8400d2, crash-
    # 0aba386e78291e98e4703acc2e541eb5a57e4628): a FRESH finding in the
    # roundtrip harness (A/B by changed-file evidence: 6eec12e..f8400d2
    # touches only test/fuzz/fuzz_diff.c, its pins, and docs -- the
    # roundtrip binary links none of it, so the r193 arm is exonerated;
    # the artifact aborts identically on the pristine 4ed66fa build).
    # 27 bytes ">\n ! \n !<aaa: bbb\n...\n<NEL><NEL>\n": the break
    # normalizer's token-space arm consumed a fold line's leading
    # indent spaces one byte at a time (the at_break stays armed), the
    # block-scalar content arm then counted the line's indent from the
    # FIRST NON-SPACE byte, read ind=0 for the one-space-indented line,
    # and the dedent rule (ind <= bs_indent) ended the fold
    # mid-content; the next byte lexed as a token and the line's "!<"
    # armed a phantom verbatim whose breaks-ride-raw law left the whole
    # tail unnormalized -- the raw NEL run classified as one content
    # line and the framing walk spawned a phantom empty PLAIN second
    # document libyaml never emits (the comparator's second "=VAL :").
    # The empty plain root emitted zero bytes (the #290 null
    # rendering), re-parsed as style-ANY, and the next pass rendered
    # "" -- 26 -> 29 bytes, the roundtrip trap. The fix reads the
    # block-scalar content arm BEFORE the token-space arm in
    # norm_step: the fold line consumes wholesale FROM the leading
    # space with its true indent, the fold survives to the col-0
    # marker, the NEL tail folds (libyaml parity: one document), and
    # the roundtrip is byte-stable. The r160 content-NEL law is
    # untouched (interior content pairs ride the value verbatim; the
    # pin at test_parse.cpp passes unchanged). Battery: artifact
    # pre-fix exit 133 / post-fix exit 0 on the standalone harness and
    # a DIFFER-then-parity flip on fuzz_diff; 52-mutant paired battery
    # (27 single-byte flips, 10 span deletions, 15 targeted
    # verbatim/NEL-family forms) -- 0 regressions, 16 newly claimed,
    # 36 both-pass, 0 both-abort; corpus walks exit-0 pre/post on the
    # six lf corpora with logs identical modulo libFuzzer's volatile
    # lines (parse 313, feed 65, roundtrip 2, marshal 18, cbor 102,
    # validate 5; ulimit -s 32768); full ctest 867/870 with only the
    # pre-existing local conformance/json-suite failures (identical on
    # the pristine baseline); leaks --atExit clean; clang-format 18
    # clean.
    add_test(
        NAME fuzz-diff-r194-fold-bang-verbatim-nel-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/fold-bang-verbatim-nel-tail-r194.bin
    )

    # the r195 round (run 42445063442, main 89aaf7c, crash-
    # 14c7060d646ccf352adead05d17d0dd3ef0111c1): a REGRESSION from the
    # r194 engine fix (A/B: the 8-byte artifact ">\n#ML<NEL>\n" exits
    # 0 on the pristine pre-fix f8400d2 build and aborts on the
    # post-fix build; a revert-only experiment -- the token-space arm
    # moved back above the content arm -- restores exit 0, so the
    # reorder is causal). The input itself takes the '#' comment-arm
    # path in BOTH builds; the caught shape is the r19-family indent
    # transform's SUB "(">\n #ML<NEL>\n", one space indented into the
    # col-0 content line): post-reorder the content arm's
    # blank/comment-line branch ran FIRST at the space byte and its
    # line-end walk looked for LF ONLY, consuming " #ML<NEL>\n"
    # wholesale with fold_off=-1 -- the NEL rode RAW into the engine's
    # verbatim block-scalar reader and the transform's re-classify
    # stopped flipping to parity, so the documented col-0-content
    # carry lost its claim and the harness aborted on the raw input's
    # DIFFER (yeptris "=VAL : #ML\n" vs libyaml's empty scalar, the
    # reader's floor-1 clamp). Pre-reorder the space arm ate the space
    # byte-by-byte (at_break stayed armed) and the '#' hit the comment
    # arm, whose nel case FOLDS the break. The fix ends the
    # blank/comment branch's line at ANY break spelling -- a multibyte
    # break folds at the token tail (fold_off/fold_len) exactly like
    # the comment arm; lone CR ends the line like the comment arm's CR
    # case; the r160 content-NEL law is untouched (the CONTENT branch
    # keeps its verbatim ride; the "> \n !<\xc2\x85 text9" pin
    # passes unchanged). Battery: artifact pre-fix exit 134 / post-fix
    # exit 0; the r194 seed fold-bang-verbatim-nel-tail-r194.bin exits
    # 0 (the fold-line indent law preserved); revert-only experiment
    # confirms causality; 50-mutant paired battery -- 0 regressions;
    # corpus walks exit-0 pre/post on the six lf corpora with logs
    # identical modulo libFuzzer's volatile lines; full ctest with
    # only the pre-existing local conformance/json-suite failures;
    # leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r195-fold-comment-nel-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/fold-comment-nel-tail-r195.bin
    )

    # the r196 round (run 42449356146, main 6ccf3db, crash-
    # f9837513619c855054f72e49dd4bc75b744bd3ba): a FRESH find, not an
    # r195 regression (A/B: the 16-byte artifact
    # "{?#??\t*L=:\n*L:(}" aborts byte-identically on BOTH the
    # pre-fix 89aaf7c build and the post-fix main build -- the r195
    # comment walk change never touches this input). The caught shape
    # is the r179 ALIAS-CHARSET-CUT law composed with the r132
    # glued-'?'-plain law in the VALUE orientation: libyaml's flow-'?'
    # KEY-token law eats the '?' and its comment-glue swallows
    # "#??<TAB>*L=:" whole (the '#' rides to the LF), so the line-2
    # alias "*L" becomes the KEY's content -- scan_anchor cuts the
    # name at the ':' (the r142/r76 IS_ALPHA law), freeing ':' for
    # the VALUE token and '(' for a plain value. yeptris keeps
    # "?#??<TAB>*L=" ONE plain (the suite-pinned plain-'?' law, the
    # '#' mid-plain is content) as the KEY, and the alias is its
    # VALUE with the spec's ns-anchor-char name "L:(" (c-flow
    # indicators and white excluded only). The walk died at the
    # r125 realign's byte-exact demand after its skip
    # ("=ALI *L:(" vs "=ALI *L") -- the r179 helper declined
    # because its pin demands yeptris's next line be the eaten-empty
    # val, but HERE the alias was the member's own VALUE, so nothing
    # follows it but the closer. The carry adds the VALUED
    # orientation helper (yd_qgh_alias_cut_valued_pair): the same
    # cut pins (proper-prefix name, ':' cut byte, libyaml-charset
    # kept bytes, libyaml's next line "=VAL : " plus the exact
    # eaten suffix) plus yeptris's next line a flow closer and
    # libyaml's NEXT line that same closer; the r125 consult rides
    # it on the byte-exact mismatch. An invented name or a genuine
    # yeptris alias-charset bug leaves a pin unsatisfied and the
    # bail stands. Battery: artifact pre-fix exit 134 / post-fix
    # exit 0 on BOTH commits' binaries; the r194/r195 seeds exit 0;
    # 50-mutant paired battery -- 0 regressions; corpus walk
    # exit-0 pre/post with logs identical; ctest fuzz-diff pins all
    # green; leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r196-qflow-alias-cut-valued
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-alias-cut-valued-r196.bin
    )

    # the r197 round (run 42453001309, main b2fdddd, crash-
    # 03377781d85315161195672dfa01431810e65868): a FRESH find, not an
    # r196 regression (A/B: the 23-byte artifact "> #<11<CR><PS>
    # 700:<LF>  m<NEL> t" aborts on BOTH the pre-fix 6341eb4 build and
    # the post-fix main build -- the r196 alias-cut-valued arm never
    # touches this input). The caught shape is the r160 BLOCK-NEL
    # CONTENT law composed with the #638/r158 LS-PS RAW-KEEP law: a
    # root folded scalar whose header line ends at a CR, followed by a
    # PS-only leading line and a content line holding a raw NEL. Both
    # sides are law-clean: yeptris (1.2) normalizes the PS break to LF
    # (the r195 every-break-spelling law) and keeps the NEL as CONTENT
    # (spec 8.1 b-break is LF/CR/CRLF only), adding the clip break at
    # breakless EOF (the suite's L24T#2 pin); libyaml's block reader
    # keeps the PS triple RAW into the leading breaks (scanner.c
    # READ_LINE's LS|PS->LS|PS branch) while its IS_BREAK eats the NEL
    # as a break (1.1 legacy) and drops the trailing break. The two
    # walks each own one delta and bail on the other: the block-NEL
    # walk's byte-bound main loop died on the PS head delta before the
    # NEL run was ever reached, and the r158 lsps walk has no arm for
    # the NEL polarity (yeptris RAW where libyaml spells \n). The carry
    # adds the LS/PS raw-keep tolerance to yd_block_nel_walk_mult's
    # main loop: yeptris's spelled \n is libyaml's raw LS/PS triple --
    # a 2-vs-3-byte window firing only on the documented delta --
    # after which the existing NEL run branch plus the r176 append-LF
    # transform own the whole relation. An invented tolerance or a
    # genuine engine bug leaves a pin unsatisfied and the bail stands.
    # Battery: artifact pre-fix exit 134 / post-fix exit 0 on BOTH
    # commits' binaries; the r194/r195/r196 seeds exit 0; 50-mutant
    # paired battery -- 0 regressions (16 fixed); six corpus walks
    # exit-0 pre/post with logs identical (parse count +1 rides the
    # r196 seed's absence on the pre-fix commit); ctest fuzz-diff pins
    # all green; leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r197-block-nel-lsps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-lsps-r197.bin
    )

    # the r198 round (run 42456705754, main 8ef3eb6, crash-
    # 5f49897536e3c39c7d21b03a3e61e07f628de64d): a FRESH find, not an
    # r197 regression (A/B: the 10-byte artifact "&a::<LF><SP><SP>*a:"
    # aborts on BOTH the pre-fix b2fdddd build and the post-fix main
    # build -- the r197 block-NEL LS/PS raw-keep delta never touches
    # this plain-LF input). The caught shape is the r157/r170/r174/
    # r182/r192 ROOT-ALIAS COLLAPSE's breakless member (r182's seed
    # with the trailing break deleted -- the member r182's family list
    # names), fired on SEED-CORPUS replay at MS: 0. yeptris's
    # suite-pinned riding laws (2SXE: "&a: key" anchors "a:" and "*a:"
    # names "a:"; W5VH keeps colons inside names) read line 1 as a
    # props-only anchor declaration whose name rides both colons, the
    # anchor PENDS, and line 2's colon-riding alias collapses the
    # document to ONE root alias (3 dump lines); libyaml's 1.1 IS_ALPHA
    # cut re-lexes the residual "::" as plain ":" + value token (the
    # anchored key of a root map) and line 2's cut alias + EOL colon
    # opens a nested map (alias key, empty value) -- the 9-event form.
    # The carry rides the EXACT-BYTES gray claim
    # (yd_r198_root_alias_breakless_gray), the r151/r170
    # deletion-sibling precedent. Minimality: prefixes 0-3 agree raw;
    # 4-6 ride the head-only "&a::" charset-split transform, prefix 8
    # is the BOTH-ERROR alias-only member, prefix 9 rides the same
    # transform; 50-mutant paired battery -- 0 regressions.
    add_test(
        NAME fuzz-diff-r198-root-alias-breakless
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-breakless-r198.bin
    )

    # the r199 round (run 42461709304, main ba2c7f4, crash-
    # 925b0aed9341409f49159b3d1bc3c388bc0b5b18): a FRESH find, not an
    # r198 regression (A/B: the 15-byte artifact
    # "{?*L:<LF>( `,*L:(}" aborts on BOTH the pre-r198 8ef3eb6 build
    # and the post-fix main build -- the r198 arm's exact-bytes seed is
    # the 10-byte "&a::<LF><SP><SP>*a:" member, never this input). The
    # caught shape is the qflow bare-?' glued plain follower COMPOSED
    # with the alias-colon-ride charset split, in one flow mapping,
    # fired on SEED-CORPUS replay at MS: 0. yeptris's suite-pinned
    # riding laws (HM87/652Z: a flow '?' with a non-space follower
    # rides as plain content; 2SXE/W5VH + catch-142: anchor names are
    # ns-char minus the flow indicators, so "L:(" rides the colon when
    # a plain-safe char follows) read the head "?*L" as one plain key
    # and the tail alias as anchor "L:(" with an empty value; libyaml's
    # 1.1 laws compose the other way: the '?' is the unconditional flow
    # KEY token (alias key, the r142/r76 IS_ALPHA cut) and the tail
    # alias cuts at "L", re-lexing the ':' as the VALUE token with the
    # '(' as the plain value. The carry rides the EXACT-BYTES gray
    # claim (yd_r199_qflow_bareq_alias_ride_gray): the r71 drop-'?'
    # spell declines (yeptris rejects its sub) and the charset-split
    # twin declines both orientations (libyaml can never read the
    # riding anchor). Minimality: prefixes 1-14 are all BOTH-ERROR;
    # every single-byte deletion changes the classification.
    add_test(
        NAME fuzz-diff-r199-qflow-bareq-alias-ride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-bareq-alias-ride-r199.bin
    )

    # the r200 round (run 42465797391, main 0b67393, crash-
    # 1b6d4cbe2ac56ceb226c11139e339c6dadd369a8): a FRESH find, not an
    # r199 regression (A/B: the 20-byte artifact
    # "{?#??<TAB>?#??<TAB>*L=<LF>*L:(}" aborts on BOTH the pre-r199
    # ba2c7f4 build and the post-fix main build -- the r199 arm's
    # exact-bytes seed is the 15-byte "{?*L:<LF>( `,*L:(}" member, never
    # this input), fired on SEED-CORPUS replay at MS: 0. The caught
    # shape is the qflow glued-'#' comment-glue kill composed with the
    # flow MULTI-LINE-PLAIN restart: libyaml's KEY-token law fires on
    # the flow '?' and scan_to_next_token's unconditional '#' eat
    # consumes the glued run to the LINE BREAK, so the next line's '*'
    # is a NODE START re-lexed as an ALIAS KEY ("=ALI *L" + "=VAL : (")
    # re-paired with the pending KEY, while yeptris's suite-pinned
    # plain-first-'?' law (HM87/652Z) keeps the glued run ONE plain
    # FOLDED across the break (no ':'/'/' boundary sits before it --
    # the r196 input's trailing ':' is what made THAT member cut) and
    # absorbs the spelling as mid-plain content ("=VAL : ?#??\t?#??\t*L=
    # *L:(" + the implicit empty value). The carry rides the family
    # walk's new ALIAS-RESTART-FOLD pair (yd_qgh_alias_restart_fold_pair,
    # in yd_qflow_glued_hash_differ): byte-exact pins -- yeptris's plain
    # ends exactly "*<N>:<S>" where N is libyaml's own alias name and S
    # its own value, yeptris's next line is the exact empty val, the
    # pre-alias prefix carries the glue fingerprint LOCALLY. The r163
    # tail arm declines by construction (its member ENDS at the alias
    # spelling), the r179/r196 cut pairs decline (yeptris carries no
    # "=ALI" line of its own here). Minimality: the UNCONSTRAINED
    # deletion minimizer drifts to the 4-byte "{??}" split-family twin
    # (YEPTRIS-ONLY-ACCEPT, a different verdict class -- the r186-r199
    # drift law again); the shape-constrained rerun pins the 9-byte
    # "{?#<LF>*L:(}" where every single-byte deletion changes the
    # classification (del '?' -> LIBYAML-ONLY-ACCEPT, del 'L'/del ':' ->
    # YEPTRIS-ONLY-ACCEPT twins, the rest lose the DIFFER). 81-mutant
    # paired battery: pre-fix 65 aborts, post-fix 33, 0 regressions
    # (the 33 residue abort identically on both builds -- pre-existing
    # family members outside this round's carry).
    add_test(
        NAME fuzz-diff-r200-qflow-alias-restart-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-alias-restart-fold-r200.bin
    )

    # the r201 round (run 42469831950, main 904ce9a, crash-
    # d1b11cbf81d831c2857fb048fb29a037df43dace): a FRESH find, not an
    # r200 regression (A/B: the 26-byte artifact aborts on BOTH the
    # pre-r200 0b67393 build and the post-r200 main build -- r200 touched
    # only the differ harness), fired on SEED-CORPUS replay at MS: 0 in
    # fuzz_roundtrip. The caught shape is the emit-side EMPTY-ROOT-NEL
    # family: the doc-end marker followed by a break-only tail
    # (">\n! \n !<aaa: bbb\n...\n<NEL><NEL>" -- the NELs become the
    # second document's break-only plain content, a PLAIN-empty root)
    # serialized the empty root as NOTHING after the "---" marker, while
    # every other empty-root spelling (the bare "---" doc close: an
    # ANY-styled implicit scalar; the re-parse of that bare form: same)
    # rendered "" -- so serialize(parse(serialize(x))) flipped the doc
    # to the quoted form and the bytes diverged on round two. Engine fix
    # in emit_node (writer.c): a document-root empty scalar rides the
    # quoted form like libyaml's "--- ''"; the #290 value law ("k:"
    # emits nothing) is untouched -- the arm gates on parent_col < 0
    # (the root sentinel), non-canonical/non-JSON, scalar kind, style
    # ANY-or-PLAIN, zero length. Minimality: the greedy deletion
    # minimizer rides the pre-fix oracle to the 15-byte
    # ">\n! !<\n...\n<NEL><NEL>" (every further deletion changes the
    # class); the pinned seed is the TRUE 26-byte artifact. 107-mutant
    # paired battery: 0 regressions (the 26 mutants that abort pre-fix
    # are all clean post-fix -- same family members).
    add_test(
        NAME fuzz-roundtrip-r201-empty-root-nel-doc
        COMMAND fuzz_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip
    )

    # the r202 round (run 42472937638, main 4a16549, crash-
    # ac0d873dc7a4d5aa5fb2ef1a0c7bf745b208fb8e): a FRESH find, not an
    # r201 regression (A/B: the 23-byte artifact aborts on BOTH the
    # pre-r201-fix 0b67393 build and the post-fix main build -- the r201
    # engine fix touched only the emit root-sentinel path), fired on
    # SEED-CORPUS replay at MS: 0 in fuzz_diff. The caught shape is the
    # block-NEL family's LS CARRIER: a literal block scalar whose
    # content carries raw U+2028 with NO NEL byte ("|\n \n \n<LS>\n \n
    # \n<LS>\n\n - m" -- yeptris spells "\n\n\n\n\n\n\n\n\n- m\n",
    # libyaml keeps "\n\n<LS>\n\n\n<LS>\n\n- m"), so the family gate's
    # NEL-only scan declined before the walk could speak. Comparator
    # carry in yd_block_nel_content_divergence: the gate now also
    # admits the raw LS/PS triple -- the deltas are the family's own
    # documented law (the r197 raw-keep tolerance in yd_block_nel_walk
    # consumes each yeptris-spelled-\n vs libyaml-raw-triple window
    # byte-wise wherever it sits; the r176 transform owns the breakless
    # trailing break), and the walk stays byte-bound with subs > 0, so
    # an admitted input can only ride on the documented deltas. Both
    # readings are spec-permitted (1.2 productions b-break vs
    # b-as-space). Minimality: the UNCONSTRAINED greedy deletion
    # minimizer drifts to the 1-byte "|" split-family twin (the
    # r186-r201 drift law again); the family-constrained rerun (require
    # the raw-LS-VAL signature) pins the 11-byte "|\n<LS>\n<LS> m";
    # the pinned seed is the TRUE 23-byte artifact. 60-mutant paired
    # battery: 6 aborts post-fix, 0 regressions (all 6 abort
    # identically on the pre-fix build).
    add_test(
        NAME fuzz-diff-r202-block-ls-literal-rawkeep
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ls-literal-rawkeep-r202.bin
    )

    # the r203 round (run 42484238632, main 9c32c73, crash-
    # 961f75c2aafe47fb867bd2bd7098c48ef6006d2e): a FRESH find, not an
    # r202 regression (A/B: the 37-byte artifact aborts on BOTH the
    # pre-r202 4581c78 build and the post-fix main build -- the r202
    # arm touched only the block-NEL family gate), fired on SEED-CORPUS
    # replay at MS: 0 in fuzz_diff. The caught shape is the flow-SEQ
    # question family's MULTI-LINE member: a glued '?' whose plain run
    # RIDES LS/PS/CR breaks ("[\n?+D<LS> 'c<PS>.\n--&k:j:(<PS>
    # \r|\r\n.oa\"]" -- libyaml's flow KEY-token law nests a
    # single-pair map with the run as key, yeptris's suite-pinned plain
    # reading keeps one '?'-led scalar member), and the KEY VALUE BYTES
    # themselves diverge: libyaml's fold law keeps mid-plain LS/PS RAW
    # (READ_LINE copies them verbatim) while yeptris follows the spec's
    # fold law -- so r183's brace spell (its same-line run law declines
    # any break) AND r190's KIND 2 "? " spell (both engines would fold
    # the run differently) can never carry it. Comparator carry in
    # yd_r203_flow_seq_question_multiline_divergence: the fold is
    # SIMULATED with libyaml's own law (raw LS/PS lead rides with its
    # LF-normalized trailing breaks; an LF-lead folds to ' ' with no
    # trailing, else the trailing rides verbatim; same-line spaces
    # ride, break-adjacent spaces drop) and the result bakes into a
    # double-quoted key with every fold-sensitive byte escaped
    # (\L / \P / \n / \t / \"), so NEITHER engine folds the rewrite
    # and the byte-strict verify pins libyaml's original dump from both
    # sides. Both readings are suite-permitted (HM87/652Z plain-'?'
    # over libyaml's 1.1 KEY law; b-as-space fold over libyaml's
    # raw-keep). Minimality: the UNCONSTRAINED greedy deletion minimizer
    # drifts to the 5-byte "[:\r|]" YEPTRIS-ONLY split twin (the
    # r186-r202 drift law again; a ledger-class split, not a nightly-
    # reachable DIFFER); the DIFFER-constrained rerun (require the
    # '?'-led yeptris plain vs the libyaml +MAP tell) pins the 15-byte
    # "[?D<LS><PS>:(<PS>]"; the pinned seed is the TRUE 37-byte
    # artifact. 117-mutant paired battery: 66 carried post-fix,
    # 0 regressions.
    add_test(
        NAME fuzz-diff-r203-qflow-multiline-lsps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-multiline-lsps-r203.bin
    )

    # the r206 round (run 42498570153, main 7d5483e, crash-
    # df52ab9fc21e989fc250fe4592c46bc982c9570d): a FRESH finding (the
    # A/B against the parent b23d553 aborts on BOTH builds -- the r205
    # aligned-triple arm is innocent). The 25-byte
    # "[11n<CR><PS> 7.tag<CR> ,?d#  m,]" composes two adjudicated
    # deltas: the flow-SEQ '?' KEY-token law reads "?d#  m" as an
    # explicit key {d#  m: <empty>} (libyaml's 1.1 quirk) while
    # yeptris's suite-pinned plain-'?' law keeps "?d#  m" ONE plain
    # member (the HM87 law -- the '#' glued after 'd' is mid-plain
    # content, not a comment boundary), AND the head plain's CR+PS
    # double break renders as yeptris's spelled-LF empty line vs
    # libyaml's raw kept PS (the LS/PS non-fold quirk -- libyaml's
    # fold law only folds leading '\n' breaks). Comparator carry in
    # yd_r183_flow_seq_question_divergence: the run scanner admits a
    # GLUED '#' (the preceding byte is a non-blank, non-break tail --
    # content in both engines, so the "{run: }" spell keeps the pair
    # reproducible) while a '# after a blank or byte-glued to an
    # exotic-break tail (NEL C2 85, LS/PS E2 80 A8/A9 continuations)
    # stays the comment boundary and declines (the r185 glue is its
    # own arm); the r183 LS/PS fallback owns the head's raw-PS delta.
    # Minimality: greedy deletion pins the 11-byte "[n<PS>g,?d#]"
    # WITH both deltas alive (yeptris's lone-PS fold space vs
    # libyaml's raw PS, byte-checked); the pinned seed is the TRUE
    # 25-byte artifact. 101-mutant paired battery: 48 gained (the
    # family's own mutants), 35 carried both-abort, 0 regressions.
    add_test(
        NAME fuzz-diff-r206-flow-question-glued-hash
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-question-glued-hash-r206.bin
    )

    # the r207 round (run 42503100451, main 20e01be, crash-
    # f810355b2666b6760e63636bc6e112028c29de7b): a FRESH finding (the
    # A/B against the parent 7d5483e aborts on BOTH builds -- the r206
    # glued-hash widening is innocent). The 24-byte
    # "{\\n?\\t \\n\\t\\t \\t\\t: ,?:-,?*M,}" composes three flow-'?'
    # entries: the '?'-with-BLANK-GAP-then-':' explicit-key pair
    # ("?\\t \\n\\t\\t \\t\\t:"), the r85 bare-'?,' boundary pair, and
    # the r71 glued-'?*M' hit (yeptris's suite-pinned plain-'?' law
    # reads "?*M" ONE plain key -- the HM87 law -- while libyaml's
    # unconditional flow KEY-token law binds the alias *M as the KEY).
    # The qflow arm's r85 boundary spell ('""' ':') DOUBLED the first
    # pair's own separator -- the stray ':' rejects the rewritten
    # entry in BOTH engines, so the arm's byte-strict verify died
    # before the glued hits were even read. Comparator carry in
    # yd_qflow_question_colon_divergence: a '?' whose follower is a
    # blank/break gap (SP/TAB/LF/CR, NEL C2 85, LS/PS E2 80 A8/A9)
    # up to the entry's OWN ':' rides VERBATIM -- both engines read
    # that explicit-key pair identically (libyaml's KEY token,
    # yeptris's flow-'?' carve), so no spell is needed; a gap ending
    # at anything else keeps the r85 spell. The widening is strictly
    # claim-additive: a blank-gap-':' '?' under the old spell always
    # errored the rewrite, so no carried pin can contain the form.
    # Minimality: the UNCONSTRAINED greedy deletion minimizer drifts
    # to the 3-byte "{:}" split twin (the r186-r202 drift law); the
    # DIFFER-constrained rerun pins the 11-byte "{?\\t:,?,?*M}" with
    # all three deltas alive; the pinned seed is the TRUE 24-byte
    # artifact. 430-mutant paired battery: 78 gained (the family's
    # own mutants), 147 carried both-red, 205 quiet, 0 regressions.
    add_test(
        NAME fuzz-diff-r207-qflow-question-gap-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-question-gap-colon-r207.bin
    )

    # the r208 round (run 42507908569, main 0398dc6, crash-
    # 251030f166bcf9087724662620981e09e99ac343): a FRESH finding (the
    # A/B against the parent 20e01be aborts on BOTH builds -- the r207
    # question-colon widening is innocent; the input has no '?' at
    # all). The 25-byte ">\n !<[!p\n !<NEL CR CR !<NEL NEL SP\n" reads
    # a root folded block scalar whose content carries raw NELs: the
    # 1.2 break set inside bs content is LF/CR ONLY (spec 8.1), so
    # yeptris keeps each NEL (C2 85) as CONTENT bytes, while libyaml's
    # 1.1 reader breaks on them too (its IS_BREAK) -- the r160 family
    # with a delta its walk arms all decline: at the run's exit
    # libyaml's collapsed escaped-break run is exactly k LONGER than
    # yeptris's (its collapse counts the NEL-turned-breaks; yeptris's
    # counts only the real breaks around the kept bytes -- the
    # artifact's first site: yeptris NEL + 1 escaped break from the CR
    # CR pair, libyaml 2 escaped breaks from NEL CR CR), with the
    # content behind both runs coinciding. Comparator carry in
    # yd_block_nel_walk_mult: after the terminal-tail arm declines,
    # eat both escaped-break runs, require the surplus == k and the
    # alignment byte behind the runs to coincide (and not be an
    # escape), then rejoin the byte-bound walk -- pointers never
    # rewind, the surplus is bound by the run, and the site is pinned
    # by the alignment byte. Strictly claim-additive: b-surplus
    # escapes beyond the (k-1) collapse never rode any branch before,
    # so no carried pin can contain the form. Minimality: the
    # UNCONSTRAINED greedy deletion minimizer drifts to the 9-byte
    # ">\n<NEL SP NEL NEL" split twin (yeptris leads with an escaped
    # break where libyaml folds -- a different delta, r186-r207 drift
    # law; documented, still open); the DIFFER-constrained rerun pins
    # the 14-byte ">\n <NEL CR CR <NEL NEL" with the surplus-break
    # delta alive; the pinned seed is the TRUE 25-byte artifact.
    # 110-mutant paired battery: 23 gained (every one a NEL carrier
    # with the artifact's shape), 60 carried both-abort (the ledgered
    # split zoo, unchanged), 27 quiet, 0 regressions.
    add_test(
        NAME fuzz-diff-r208-block-nel-surplus-break
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-surplus-break-r208.bin
    )

    # the r209 round (run 42512673554, main 939c2f1, crash-
    # 55c53c6c6a1169156a444bc82fc3696a759c3252): a FRESH finding (the
    # A/B against the pre-r208 parent 0398dc6 aborts IDENTICALLY --
    # exit 134, the same 12-byte artifact, the same dump pair -- so the
    # r208 surplus-break arm is exonerated) and NOT issue #1149's
    # documented drift twin (the 9-byte ">\n<NEL SP NEL NEL"
    # block-scalar form: no '>', no NEL here). The 12-byte
    # "{?*L:<LF>,*L:(}" is the r199 exact-bytes gray's family SIBLING --
    # the same bare-'?'-glue x alias-ride composition with the head's
    # value bound EMPTY -- claimed STRUCTURALLY by the new spelled arm
    # (yd_r209_qflow_bareq_alias_spelled_divergence) so the family
    # stops resurfacing one exact member per nightly: the preserving
    # spell inserts one blank at each glue site ("?*L" -> "? *L",
    # "L:(" -> "L :(", and the measured in-name colon sites) and
    # requires libyaml(rewrite) == libyaml(original) byte-exact plus
    # yeptris(rewrite) == libyaml(rewrite) -- the r193 certificate
    # without the walk fallback. Minimality: the shape-constrained and
    # any-constrained greedy deletions BOTH converge on the 10-byte
    # "{?*L,*L:(}" with the dump pair byte-identical (no split-family
    # drift); the pinned seed is the TRUE 12-byte artifact. The r199
    # 15-byte sibling declines the arm BY MEASUREMENT (its spell makes
    # yeptris reject the rewrite) and stays with its gray claim.
    add_test(
        NAME fuzz-diff-r209-qflow-bareq-alias-spelled
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-bareq-alias-spelled-r209.bin
    )

    # the r210 round (run 42517419521, main 7bc84d6, timeout-
    # a9991d98471f8190127b39bb60b0202dbb47ab07): the nightly's FIRST
    # TIMEOUT-class finding — no divergence at all (both engines
    # accept; the semantic dump pair is byte-identical pre/post). The
    # 82KB flow-'?' soup drives 13401 events, and the probe chain
    # rides ~50 full-input dumps through the arms
    # (yd_anchor_name_charset_divergence's verify copies alone are
    # ~22). The sem-dump append re-derived strlen()+realloc() PER
    # EVENT — O(events^2) byte scans — and yd_libyaml_dump re-copied
    # its whole buffer per event the same way; under the CI's
    # Debug+ASan harness the unit crossed the 5s libFuzzer alarm
    # inside yd_charset_verify_copy's yeptris leg. NOT a regression:
    # the A/B against the pre-r209 parent 1e9f7a8 times identically
    # (Release ~1s both sides, exit 0, "no divergences"; the CI-like
    # Debug+ASan build measures 6s pre-fix vs 1s post-fix on the same
    # arm64 box), and it is NOT issue #1149's drift twin (no NEL
    # block-scalar form here). The fix is harness-only amortized
    # growth: yd_sem_out tracks buf/len/cap in sem_dump.h and
    # yd_libyaml_dump tracks acap in diff_core.c — the emitted bytes
    # are IDENTICAL (verified byte-exact on the artifact and across
    # the 50-mutant battery). Minimality does not apply to the
    # timeout class (the property is size-dependent; the 10 battery
    # truncation mutants both pass clean); the pinned seed is the
    # TRUE 81936-byte unit.
    add_test(
        NAME fuzz-diff-r210-timeout-flow-soup-dump-append
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/timeout-flow-soup-dump-append-r210.bin
    )

    # the r211 round (run 42522196631, main b5264ab, crash-
    # 925daa548a18486557bd9b53e66b18006a504cfb): a FRESH finding and an
    # ENGINE round — the A/B against the pre-r210 parent 6e225e1 aborts
    # IDENTICALLY (exit 134, the same DIFFER dump pair), exonerating
    # the r210 harness change. 100 bytes
    # "[-123122kdy: &an<O*87>chor\n !!312    3123]": a flow-seq member
    # whose value is an anchor with the TAG on the NEXT LINE. The flow
    # node parse (e_flow_node) ran e_props at the anchor's inline
    # position — its loop skips INLINE space only — so the cursor
    # stopped at the break; the following e_flow_ws crossed the break
    # and the '!'-led line fell into the PLAIN branch (yep_plain_first_ok
    # allows '!' as plain content, and plain scalars may not merely
    # CONTAIN '!' but here STARTED with the whole tag): the node lost
    # its tag entirely and glued "!!312    3123" into the scalar value,
    # while libyaml resolves the tag (tag:yaml.org,2002:312) with value
    # "3123". YAML 1.2 grammar: properties ride s-separate-in-flow
    # (breaks allowed), and a plain scalar must not START with the
    # c-indicator '!' — libyaml's reading is the spec-correct one. The
    # fix re-lexes a riding property run in e_flow_node (the cursor at
    # '&'/'!' after the ws crossing) with the duplicate-property state
    # threaded across the calls (the 4JVG law holds: "&a\n&b x" now
    # errors like libyaml instead of gluing "&b x"). The tag-led order
    # ("!t\n&a x") and the comment-separated gap ("&a\n# c\n!!t v")
    # ride the same loop; same-line props, block-context props (the
    # pend mechanism), quoted/alias/collection followers, and mid-plain
    # '!' ("x\n!!t y" stays one folded plain in BOTH engines) are
    # untouched. Minimality: DIFFER-constrained greedy deletion pins
    # the 14-byte "[y: &r\n!!312 ]" with the same dump pair; the
    # pinned seed is the TRUE 100-byte artifact. Battery: artifact
    # pre-fix exit 134 / post-fix exit 0; 53-mutant paired battery on
    # the artifact and 55 on the minimum — pre-DIFFER 33/17 all fixed,
    # 0 regressions (the p1/p2 second-anchor twins flip from tolerated
    # YEPTRIS-ONLY-ACCEPT to BOTH-ERROR, never an abort class); corpus
    # walks exit-0 pre/post on the six lf corpora with logs identical
    # modulo libFuzzer's volatile lines (ulimit -s 32768); fresh ctest
    # pins green; leaks --atExit clean; clang-format 18 clean.
    add_test(
        NAME fuzz-diff-r211-flow-props-nextline
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-props-nextline-r211.bin
    )

    # the r212 round (run 42527856270, main cb168e1, crash-
    # 4532cf850a2d34c3597f172fa1eac42cc911bb81): a FRESH find, not an
    # r211 regression (A/B: the 14-byte artifact "&a:o&a:o::<LF>*a:"
    # aborts IDENTICALLY -- exit 134, the same DIFFER pair -- on BOTH
    # the pre-r211 parent 55d1ab5 and the post-fix main build, so the
    # r211 riding flow-property re-lex is exonerated). The caught shape
    # is the root-alias collapse family's BREAKLESS DOUBLED-HEAD member:
    # byte-identical to the r174 seed "&a:o&a:o::<LF>*a:<LF>" with the
    # trailing break deleted -- r174's own minimality list names this
    # twin ("the trailing break is NOT load-bearing ... the breakless
    # twin stays unclaimed beside the pinned cut") and the r151/r170
    # deletion-sibling precedent gives it its own entry. yeptris's
    # suite-pinned riding laws (2SXE/W5VH + the r142 anchor-charset
    # carry) read line 1 as a props-only anchor declaration whose name
    # rides both colon runs and the interior '&' to the break (the
    # anchor pends) and line 2 "*a:" as an alias whose name rides the
    # colon -- no value indicator ever opens a map, so the document
    # collapses to ONE root alias carrying the pended anchor. libyaml's
    # 1.1 laws compose the other way: IS_ALPHA cuts the anchor at "a",
    # ":o&a:o:" re-lexes as a plain (the '&' is mid-plain CONTENT) with
    # the final colon as the VALUE token (the anchored map key), and
    # the line-2 alias "a" sits inside the same map as a second flat
    # pair. The carry rides the EXACT-BYTES gray claim
    # (yd_r212_root_alias_double_head_breakless_gray): the charset-split
    # twin declines both orientations (libyaml can never read the
    # single root alias; the reversed orientation is the unsound
    # r137/r138/r139 mask). Minimality: prefix 13 is the
    # libyaml-reject alias-only split member; the artifact IS the
    # 14-byte input, libFuzzer-minimal at MS: 0 (seed-corpora read, no
    # mutation chain). 50-mutant paired battery: 0 regressions, every
    # verdict identical pre/post (the surviving abort-both members are
    # the r174 list's named family siblings).
    add_test(
        NAME fuzz-diff-r212-root-alias-double-head-breakless
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-double-head-breakless-r212.bin
    )

    # the r205 round (run 42492751265, main a0f2ff6, crash-
    # 4d4e23c665b950d0d9b238595533be47073fe111): the block-NEL family's
    # ALIGNED-LS/PS-TRIPLE INDENT SURPLUS. The artifact
    # "> #<11<CR><PS> 70<PS> 700:\\n  m<NEL> t0:\\n  m<NEL> t" reads as
    # a folded block scalar whose content lines carry a MID-LINE PS and
    # mid-line NELs. yeptris's pre-parse normalizer rides each bs
    # content line WHOLESALE to its \\n (the r194 law), so the mid-line
    # exotic breaks stay RAW CONTENT (kept_raw) and the engine's block
    # reader keeps the continuation's leading space: value
    # "\\n70<PS> 700:\\n m<NEL> t0:\\n m<NEL> t\\n". libyaml breaks the
    # line AT every NEL/LS/PS (its IS_BREAK), READ_LINE keeps LS/PS
    # verbatim and normalizes NEL to \\n, and each split eats the
    # continuation's indent: value "<PS>70<PS>700:\\n m\\nt0:\\n m\\nt"
    # (no trailing clip break at the breakless EOF). Three adjudicated
    # deltas compose: the r197 spelled-\\n-vs-raw-triple head (the
    # PS-only line), the NEW aligned-triple indent surplus (both dumps
    # carry the raw PS at the same offset; yeptris keeps the space
    # libyaml ate as the continuation's indentation), and the r140
    # breakless clip tail via the existing r176 composed transform.
    # Comparator carry in yd_block_nel_walk_mult: the aligned branch
    # (both dumps' raw triples coincide) skips both triples and eats
    # yeptris's SURPLUS spaces -- only while the dumps disagree, capped
    # at 64 (the block's indent column) -- so a space both dumps carry
    # stays byte-bound and the claim cannot outrun the delta. The
    # A/B against the parent a3f9b1e aborts on BOTH builds (fresh
    # finding; the r204 engine fix is innocent). Minimality: greedy
    # deletion pins the 34-byte ">\\r<PS> 70<PS> 700:\\n  m<NEL>
    # t0:\\n  m<NEL> t" with the byte-identical dump pair; the pinned
    # seed is the TRUE 39-byte artifact. 52-mutant paired battery:
    # 30 carried, 22 gained (the family's own mutants), 0 regressions.
    add_test(
        NAME fuzz-diff-r205-block-nel-ps-aligned-indent
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-ps-aligned-indent-r205.bin
    )

    # the r204 round (run 42488986434, main a3f9b1e, crash-
    # c9eef6e31fb6eac34b7e594e99bd1c3483bf8c25): a REGRESSION of the
    # ac088c0 perf slice (A/B: the 85-byte artifact aborts on the
    # post-slice a3f9b1e build ONLY -- it passes on the pre-slice
    # 9c32c73 build AND on the 955df7a r203-arm-only build, so the
    # r203 comparator arm is innocent; fired in fuzz_roundtrip on
    # SEED-CORPUS replay at MS: 0). The caught shape is the
    # chained-pending family's SIBLING-REOPEN member: "b:\n c: d\n
    # q:\n b:\n  c: d\n" -- after the chained-pending arm resolves q
    # in place and the spent key-SCALAR slot is reused, the PENDSEC
    # phantom close left the closed frame's content_col behind, and
    # the stale column rode yt_f_open_form's sibling test at the next
    # pending's pair resolution: the reopened "b" container misfiled
    # as a key-SCALAR ("q: b" + "? {c: d}" instead of
    # "q: {b: {c: d}}") and serialize(parse(serialize(x))) diverged
    # on round two. Engine fix in yt_f_run (dom/ytape.c): the PENDSEC
    # phantom close now resets content_col -- the stale-frame law the
    # sibling path already carried (run 38179552341). Minimality: the
    # greedy deletion minimizer drifts to the 23-byte
    # "b:\n c: d\nq:\n b:\n  c: d\n" FIRST-PARSE twin (the
    # r186-r203 drift law again); the pinned seed is the TRUE 85-byte
    # artifact (both parses ride it: the original bytes parse
    # correct, the canonical re-serialization trips the stale
    # column). 100-mutant paired battery: 0 regressions, 0
    # fingerprint drift. Deep-nesting DOM bench post-fix: 1075 MB/s
    # (the slice's 1042-1080 level holds; all seven other shapes
    # flat).
    add_test(
        NAME fuzz-roundtrip-r204-pend-sibling-reopen
        COMMAND fuzz_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip
    )

    # the r213 round (run 42533789224, main c73e1f0, crash-
    # d8e3b3f392c37a62257576ce20d5a19b351f66ad): a FRESH find and an
    # ENGINE round — the nightly's two-arm triage exonerates BOTH
    # fresh changes (the 147-byte artifact aborts IDENTICALLY -- exit
    # 133, the same [ri] l1=128 l2=127 pair -- on cb168e1
    # (pre-r212-arm), 344c37f (the anchor slice), and c73e1f0 (both)).
    # The catch is fuzz_roundtrip's byte-instability invariant, not a
    # fuzz_diff comparator case: serialize(parse(serialize(x)))
    # diverged on round two. Shape: a completed root pair ("2: e")
    # arms root_pair_col, then a pending ("4:") chains into an
    # ANCHORED pending ("  5: &5") whose deeper line ("    5: e")
    # resolves it. The chain: yt_f_open_form answers the SIBLING
    # key-SCALAR form for the outer pending (col+1 == content_col),
    # the spent-frame slot-reuse keeps depth, and the anchored
    # pending's event-path resolution emits its KEY SCALAR before the
    # anchored MAP_START — the key pairs with the stranded outer key
    # and the anchored map strands as a sibling key of the parent
    # ({2:e, 4:"5", {5:e}&5:"7", s:null} instead of
    # {2:e, 4:{5:{5:e}&5}, 7:s}; Psych/libyaml parse the emitted s1
    # correctly, so the re-parse is the wrong side). The engine route
    # (push events, eager DOM) is proven correct on the shape — the
    # fix routes the composition there: yt_f_run's chained-pending
    # arm bails when the outer pending resolves SCALAR-form and the
    # incoming pending is anchored (the "anchored pendings keep the
    # engine" law; the OPEN-form chain is untouched — the YTP_OPEN
    # record materializes container+key in one and pairs correctly).
    # A/B anchor-lane exoneration: the abort predates the anchor
    # slice (fused_run ownership needs a non-NULL resolver, which is
    # why the slice's memo/placed work is irrelevant). Minimality:
    # greedy deletion pins the 28-byte "2: \n e\n4:\n  5: &5\n
    # 5: e\n7: s" (domdump: eager parse correct, lazy parse corrupt
    # — the exact route pair); the pinned seed is the TRUE 147-byte
    # artifact. Battery: artifact pre-fix exit 133 / post-fix 0;
    # 55-mutant paired battery (pre-fix 344c37f vs post-fix) — 24
    # fixed, 0 regressions, 0 mixed; corpus walks exit-0 pre/post on
    # all six lf corpora (-runs=0, ulimit -s 32768); fresh ctest
    # green (892, corpora fetched); the anchor slice's pins
    # (AliasAfterRedefinitionBindsLastDefinition /
    # AliasMemoSurvivesAnchorAlternation) green; deep-nesting DOM
    # bench 1075 MB/s (the >=1000 gate holds); leaks --atExit clean;
    # clang-format 18 clean.
    add_test(
        NAME fuzz-roundtrip-r213-anchored-pending-chain
        COMMAND fuzz_roundtrip ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip
    )

    # the r214 round (run 42538075383, main dc73734, crash-
    # f8c03a6da175197af2afb7634314e3ff0b8df7f1): a FRESH finding —
    # the A/B against the pre-r213 parent c21173c aborts IDENTICALLY
    # (exit 134, the same dump pair; a probe linking both builds'
    # yep_engine_run prints byte-identical dumps), so the r213
    # anchored-chained-pending bail is exonerated (the harness dump
    # is the pure engine route; the tape never runs here). The
    # 14-byte ">\n  <\u{85}...\u{85} \n" reads a root folded block
    # scalar whose content carries raw NELs (the r160 family): at the
    # FIRST NEL libyaml's 1.1 reader breaks the line, sees the next
    # line ("...", the document-end marker) at column 0 under a col-2
    # content indent, and ENDS the scalar — its value is "<" plus the
    # ONE kept break while the marker line leaves the value entirely;
    # yeptris never broke at the NEL (the bs wholesale ride, the
    # 1.2 LF/CR-only break set inside bs content), so its value keeps
    # the NEL run AND the whole terminator line as verbatim content
    # before its own trailing break. No existing arm owns it: the
    # r208 surplus-break law needs the alignment byte to coincide
    # (yeptris's kept '.' vs libyaml's frame tail), the r19 root
    # family's col-0 indent transform cannot fix a break that never
    # happened, and the r176 composed transform declines (the input
    # ends with a break). Comparator carry in yd_block_nel_walk_mult
    # (the r214 terminator law, fired LAST so every existing member
    # keeps its exact path): after the r173 continuation declines,
    # yeptris's remainder must be a SINGLE-LINE surplus (plain bytes
    # plus kept raw NEL/LS/PS — no escapes, no raw LFs: this rides
    # ONE line) followed by its ONE trailing escape, libyaml's
    # remainder exactly ONE escape, and the remainders behind BOTH
    # escapes byte-identical to the very end (the r161 complete
    # binding — nothing past the scalar can ride); every step
    # advances the yeptris pointer (the walk stays bounded by the
    # dump length). Strictly claim-additive: "libyaml ended the value
    # one escaped break EARLIER with identical tails" never rode any
    # branch before. Minimality: greedy deletion pins the 11-byte
    # ">\n <\u{85}...\u{85}" (the same terminator delta — yeptris's
    # trailing escape there is the suite-pinned breakless-EOF clip
    # break; the same branch rides it); the pinned seed is the TRUE
    # 14-byte artifact. 80-mutant paired battery (pre-fix parent
    # c21173c vs post-arm): 14 fixed, 0 regressions, 49 carried
    # both-abort, 17 quiet.
    add_test(
        NAME fuzz-diff-r214-block-nel-terminator-docend
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-terminator-docend-r214.bin
    )
    # the r215 round (run 42543960307, main d380532, crash
    # 45808742369d734fa2c16e250ad26a9af4f1a2f6): a FRESH finding — the
    # A/B against the pre-r214 parent 4ac09a3 aborts IDENTICALLY (exit
    # 134, the same dump pair), so the r214 terminator law is
    # exonerated. The 21-byte ">\n !<[LF] [NEL NEL SP][LF] <[NEL NEL
    # SP][LF]" reads a root folded block scalar whose content carries
    # raw NELs mid-line (the r160 family): yeptris's 1.2 reader keeps
    # them as content and writes the fold SPACE at the line boundary,
    # so its value is "!<" + fold space + NELNEL + kept trailing space
    # + fold space + "<" + NELNEL + space + the clip break; libyaml's
    # 1.1 reader breaks AT the NELs, its blank-line accounting
    # suppresses the fold space, and its break run opens with the
    # boundary break — "!<" + 3 escaped breaks + "<" + the clip. No
    # existing arm owns it: the walk's byte-bound loop stops at the
    # space-vs-escape delta BEFORE the NEL branch's gate (which wants a
    # sitting ON the run) is ever reached, so r161/r208/(k-1)/r173/r214
    # all declined; the r176 composed transform declines (the input
    # ends with a break). Comparator carry in yd_block_nel_walk_mult's
    # mismatch handler (fired LAST; both arms previously declined here
    # unconditionally, so every existing member keeps its exact path):
    # (1) the FOLD-SPACE arm eats only the one space, gated on the NEL
    # pair immediately behind it and an escape on libyaml's side, and
    # rejoins the loop top where the branch's own laws reconcile the
    # rest under their existing budgets; (2) the FOLD-BREAK-SURPLUS arm
    # owns the drift sibling's site (yeptris's escape run LONGER than
    # libyaml's before the kept NELs — the 14-byte ">\n <[LF][NEL NEL
    # SP][NEL NEL]" greedy minimum, reachable from the seed by two
    # deletions): yeptris rides x escapes then the k-NEL run, libyaml y
    # escapes, and behind the run yeptris carries exactly (x - y)
    # surplus escapes THEN libyaml's remainder byte-identical to the
    # very end — a complete binding nothing past the site can ride. The
    # empty-value drift twins (11/12 bytes, libyaml's whole value
    # spent) need (x - y) escapes behind the run that yeptris does not
    # carry, so the binding fails and they keep aborting (a different
    # mechanism, documented not carried). Minimality: the
    # DIFFER-constrained greedy deletion drifts to the empty-value twin
    # (11-byte ">\n[NEL NEL SP][NEL NEL]"); constrained to the
    # artifact's mechanism (libyaml's value non-empty) it pins the
    # 14-byte fold-break-surplus sibling, which the arm carries; the
    # pinned seed is the TRUE 21-byte artifact. 83-mutant paired
    # battery (pre-fix parent 4ac09a3 vs post-arm): 24 fixed, 0
    # regressions, 21 carried both-ride, 38 both-abort.
    add_test(
        NAME fuzz-diff-r215-block-nel-foldsurplus
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-foldsurplus-r215.bin
    )
    # the r216 round (run 42549204693, main 27c92f1, crash
    # 1ef50f3004ab57c6342cbbd770633608d74086a2): a FRESH finding — the
    # A/B against the pre-r215 parent d380532 aborts IDENTICALLY (exit
    # 134, the same dump pair, on both the 35-byte artifact
    # ">\n !<[LF] [NEL NEL SP][LF][LF] [NEL NEL SP][LF] <[NEL NEL SP]
    # <[NEL NEL SP][LF]" and the 8-byte greedy minimum
    # ">\n [NEL][LF] <"), so the r215 round is exonerated. The shape is
    # the r160 family's MIRROR polarity: a folded block scalar whose
    # break run leaves LIBYAML one escape LONGER at a plain content
    # byte. The 8-byte minimum reads "NEL" on its first content line:
    # yeptris's 1.2 reader keeps the NEL as content and writes the fold
    # space at the L2-L3 boundary ("NEL" + fold space + "<" + clip
    # break); libyaml's 1.1 reader breaks AT the NEL and keeps the
    # boundary break too ("\n\n<"). The walk enters the NEL branch (k
    # == 1), the (k-1) collapse has nothing to eat, and the r173
    # continuation spends its whole budget pairing yeptris's fold space
    # against ONE of libyaml's two breaks — the pointers cannot rewind,
    # so libyaml's boundary break survives with no yeptris counterpart
    # and the byte-bound loop stops at escape-vs-content: the r215
    # arms own only the a-side surpluses (a space/NEL pair, a's longer
    # escape run) and r197 owns a's spelled break, so every law
    # declined. Comparator carry in yd_block_nel_walk_mult's mismatch
    # handler, fired LAST (previously an unconditional decline, so
    # every existing member keeps its exact path): the FOLD-BOUNDARY
    # arm eats libyaml's ONE surplus escape, gated hard — the run is
    # exactly one escape (a wider surplus keeps aborting), a sits on a
    # PLAIN content byte (not the frame end, not a space -- the
    # r173/r215 sites -- not an escape, not a raw NEL/LS/PS lead byte
    # the NEL branch owns), and the byte behind libyaml's escape EQUALS
    # it, so the walk rejoins byte-bound at an agreement byte and the
    # relation behind the site stays owned by the loop and the NEL
    # branch's budgeted laws. 268-mutant paired battery (pre-fix
    # parent d380532 vs post-arm): 41 fixed, 0 regressions, 160
    # carried both-abort, 67 both-pass.
    add_test(
        NAME fuzz-diff-r216-block-nel-foldboundary
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-foldboundary-r216.bin
    )

    # the r217 round (run 42555057049, main 0eb3b0f, crash
    # 13cb50cd9b9756179bbe0eb250c30a10e5f68a04): an ENGINE round — the
    # A/B against the pre-r216 parent 27c92f1 aborts IDENTICALLY (exit
    # 134, the same dump pair), so the r216 round is exonerated. The
    # 35-byte artifact ";!&a:\\to&a-:{*,,,!<!<[NEL^3]NNa::ao\\t:" (the
    # 15-byte DIFFER-constrained minimum ";:\\t:{!<[NEL^3]::") rides the
    # break normalizer's context walk: the glued ':'-twin '{' opened a
    # PHANTOM flow whose interior ','s armed a props slot (the phantom
    # inflated in_flow past the comma arm's "in flow" gate), and the
    # '!<' at that slot opened a phantom VERBATIM that swallowed the
    # whole tail — every NEL rode unnormalized raw into the engine, the
    # raw-NEL block walk minted an empty key, and yeptris emitted five
    # scalars in one map against libyaml's four. Three gates, all in
    # norm_step (engine.c): the phantom opener no longer clears
    # plain_open (its bytes are interior to the open plain VALUE word —
    # the r155/r169 law's opener twin), the comma arm keys on REAL flow
    # (in_flow > phantom_flow — the r169 gate's comma twin), and the
    # DECLINED ':' arms plain_open (a ':' before a non-blank byte is
    # interior to a plain or the plain's own lead — the #833 predicate's
    # plain-context fact). 142-mutant paired battery (pre-fix parent
    # 27c92f1 vs post-fix): 71 fixed, 0 regressions, 9 carried
    # both-abort, 62 both-pass; the shared six-corpus walk is
    # byte-identical pre/post.
    add_test(
        NAME fuzz-diff-r217-norm-phantom-verbatim-lead
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/norm-phantom-verbatim-lead-r217.bin
    )

    # the r218 round (run 42562016233, main ceea4ae, crash
    # acc644af2f53c3a0391417902cfac62e34e9bc9d): a COMPARATOR carry --
    # the BLANK-RUN member of the r157/r170/r174/r182/r192/r198/r212
    # root-alias collapse family (the r170 no-indent seed plus an
    # inert blank-line run between the anchored head "&a:o:" and the
    # alias line "*a:"). The A/B against the pre-r217 parent 0eb3b0f
    # aborts IDENTICALLY (exit 134, the same dump pair), so the r217
    # engine round is exonerated; the escalation clause does not fire
    # -- yeptris's one-root-alias reading is the suite-pinned riding
    # law the family has carried since r157 (2SXE/W5VH), and the
    # blank run is inert on both sides' walks. The arm's byte-shape
    # gate (head + blank-run + indent + "*a:" + blank-run tail,
    # tab-free) claims the measured member family; the tab blanks and
    # the plain-follower/alias-plainname splits decline. 29-case
    # blast-radius battery (16 members, 12 declines, 1 control): 0
    # mismatches; 50-mutant paired battery: 0 regressions; the shared
    # six-corpus walk is byte-identical pre/post; the r217 repro pin
    # still exits 0.
    add_test(
        NAME fuzz-diff-r218-root-alias-blankrun
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-blankrun-r218.bin
    )

    # the r219 round (run 42567242070, main 3b7b3f8, crash
    # 181967eb654e91fe5d56b394c48b90eb85987370, minimized to the 15-byte
    # "[?+D<U+2028>.\nF:-?-]"): a COMPARATOR carry -- the EMBEDDED-'?'
    # member of the r203 flow-SEQ question family. libyaml's unconditional
    # flow KEY-token law reads the glued '?' as a KEY (nested single-pair
    # map member with a null value), while yeptris's suite-pinned plain
    # reading (HM87/652Z) keeps "?+D..." one multi-line plain member; the
    # key bytes themselves diverge because libyaml's fold law keeps the
    # mid-plain U+2028 RAW while yeptris folds one break to a space. The
    # A/B against the pre-r218 parent ceea4ae aborts IDENTICALLY (exit
    # 134, the same dump pair), so the r218 arm is exonerated -- and the
    # artifact matches none of #1180's documented alias siblings. r203's
    # finder declined: its mid-run charset refused an embedded '?'. The
    # widened admission (mid-plain '?' is content in both engines'
    # originals; the bake quotes it away inside the double-quoted key)
    # claims the member; the byte-strict verify still arbitrates every
    # hit. 54-mutant paired battery: 0 regressions (18 intended
    # abort->pass family flips); the shared six-corpus walk is
    # byte-identical pre/post; the r218 repro pin still exits 0.
    add_test(
        NAME fuzz-diff-r219-qflow-seq-question-embedded
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-question-embedded-r219.bin
    )

    # the r220 round (run 42572682702, main 025d613, crash
    # e9a970cf9a7ed7d123d1b16bb2f18cb8529cc9a7, 52-byte artifact,
    # minimized to the 27-byte "> LF NELx10 SP NELNEL"): a COMPARATOR
    # carry -- the NEL-CARPET member of the block-NEL family (the
    # r160/r161/r173/r175/r176/r177/r197/r202/r205/r208/r214/r215/r216
    # walk): a folded block scalar whose content lines are
    # NEL-carpeted, so libyaml's 1.1 reader (NEL is a break) splits
    # every carpeted line into space-only sub-lines whose blank-line
    # accounting collapses into one long escaped-break run, while
    # yeptris's 1.2 reader (NEL is content) interleaves kept NEL pairs
    # and fold spaces. The byte-bound walk cannot reach the shape: the
    # per-branch budget (mult*k fold-material steps) cannot absorb the
    # ~3k blank-line breaks each kept NEL pair maps to, and the greedy
    # walk hits a content byte while libyaml's run is still underway.
    # The A/B against the pre-r219 parent 3b7b3f8 aborts IDENTICALLY
    # (exit 134, the same dump pair) -- the r219 widening is
    # exonerated, a fresh site fired LAST where the handler previously
    # declined unconditionally. The arm is the family's complete
    # binding at the input level: replace every raw NEL pair with an
    # LF (pure to libyaml -- its dump must be byte-identical, pinned
    # by the first strcmp), re-dump yeptris on that, and require the
    # result equals libyaml's ORIGINAL dump byte-for-byte (the second
    # strcmp) -- the whole divergence is then the documented
    # kept-NEL-content delta and nothing else can ride. 60-mutant
    # paired battery: 0 regressions (11 family flips abort->pass);
    # the shared six-corpus walk is byte-identical pre/post; the r219
    # repro pin still exits 0.
    add_test(
        NAME fuzz-diff-r220-block-nel-carpet
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-carpet-r220.bin
    )

    # the r221 nightly (run 42579140515, main 6fed677, crash-ccf950b7):
    # the GLUED-COLON PAIR of the flow-SEQ '?' family. libyaml's
    # unconditional KEY-token law reads the glued '?' as a KEY and its
    # scan_plain stop law ends the key's run at a ':'+blankz or a flow
    # boundary ("00" + value "m$"), while yeptris's suite-pinned plain
    # reading keeps "?00" one plain key (HM87/652Z). The r183 spells
    # decline: its run scanner refuses ANY ':' inside the run, and the
    # "{run: }" wrap cannot absorb the pair's own separator and value.
    # The arm quotes the KEY RUN ("?" "00" -> \"00\") and rides the
    # separator/value verbatim; glued-colon runs ("?00:$]") wrap into
    # "{00:$: }" instead (libyaml's empty value). The A/B against the
    # pre-r220 parent 88864c4 aborts IDENTICALLY (exit 134, the same
    # dump pair) -- the r220 NEL-carpet arm is exonerated, a fresh
    # site whose ':' the earlier arms decline by construction. The
    # byte-strict verify arbitrates with the CR/LS/PS/NEL-normalized
    # triple as the fallback (the artifact rides one: its head plain
    # keeps the raw PS in libyaml's dump while yeptris folds it). The
    # minimizer's twin ("[)<PS>.,?00:$]", kept at /tmp, rides the same
    # arm's wrap spell). 60-mutant paired battery: 0 regressions (28
    # family flips abort->pass); the shared six-corpus walk is
    # byte-identical pre/post; the 110 existing pins stay green.
    add_test(
        NAME fuzz-diff-r221-qflow-seq-glued-colon-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-glued-colon-pair-r221.bin
    )

    # the r222 nightly (run 42586898367, main f45cc5b, crash-a651e854):
    # the COMMENT-SWALLOW pair of the flow-SEQ '?' family. libyaml's
    # unconditional KEY-token law reads the glued '?' as a KEY and the
    # '#' GLUED to it sits at the next token position --
    # scan_to_next_token's unconditional '#' eat swallows "##|,?"
    # through the break (the entry separator AND the following '?'),
    # so the next line's plain re-lexes as the KEY's own scalar, while
    # yeptris's suite-pinned plain reading keeps "?##|" one plain
    # member (the '#' after a non-blank is mid-plain content, the r206
    # law) and the following '?' its own explicit key. The r183 spell
    # declines by construction: its follower gate refuses '#' (a '#'
    # glued to the '?' is the comment boundary at the token position
    # libyaml's KEY freed, not mid-plain content). The arm DELETES the
    # glued '#' through the comment's end and KEEPS the '?' --
    # separated from the next line by the break it re-lexes as the
    # explicit KEY in BOTH engines (a bare delete would demote the
    # next line's multiline plain to an IMPLICIT key, and a multiline
    # plain is no simple key -- libyaml rejects). The innermost frame
    # stays a SEQUENCE (the mapping orientation remains the qflow
    # glued-hash walk's turf). The A/B against the pre-r221 parent
    # 5826206 aborts IDENTICALLY (exit 134, the same dump pair) -- the
    # r221 glued-colon arm is exonerated, a fresh site the earlier
    # arms decline by construction. The byte-strict verify arbitrates
    # with the CR/LS/PS/NEL-normalized triple as the fallback (the
    # artifact's multiline key keeps the raw PS in libyaml's dump
    # while yeptris folds it). The naive tracker declines too (the
    # artifact's three double-quotes all ride GLUED mid-plain; the
    # r145 plain-run-aware tracker counts all 60 hits). The
    # minimizer's DIFFER-constrained twin ("[1,?##|,?<CR>:m]", kept at
    # /tmp) rides the same arm's keep-'?' spell; the unconstrained
    # drift twin ("[<0xA9>&m,]") is a UTF-8-continuation SPLIT, not
    # this family. 50-mutant paired battery: 0 regressions (24 family
    # flips abort->pass; the 6 still-fires are all SPLIT verdicts,
    # unchanged pre/post); the shared six-corpus walk is
    # byte-identical pre/post; the 111 existing pins stay green.
    add_test(
        NAME fuzz-diff-r222-qflow-seq-glued-hash-comment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-glued-hash-comment-r222.bin
    )

    # the r223 nightly (run 42595582912, main 6330309, crash-
    # 3f0335158b94): the ANCHORED GLUED-DOUBLE-COLON COLLAPSE of the
    # block family ("&a::\n  *e:\n", 11 bytes). libyaml's 0.2 plain law
    # frees the FIRST glued colon after an anchor (block ':' not
    # blank-followed is a plain-FIRST char), the plain stops at the
    # SECOND glued colon, and that colon re-lexes as the IMPLICIT KEY's
    # value indicator: {&a ':' -> {e -> ""}}. yeptris's line model
    # reads the same head as ONE plain scalar ("&a::" whole, the glued
    # '&' mid-plain) and the indented "*e:" line -- whose ':' rides
    # INTO the alias name (the r142/r76 charset carry) -- collapses the
    # document into the bare alias "=ALI *e:" (no map, no key, no
    # anchor). The arm rewrites the head into the EXPLICIT anchored key
    # "&a ':' :" (quoted ':' is style-blind in the semantic dump) and
    # heals the glued alias separator to "*e :" -- both engines then
    # read exactly libyaml's original dump (byte-strict verify, the
    # CR/LS/PS/NEL-normalized triple as fallback). The A/B against the
    # pre-r222 parent 3730a36 aborts IDENTICALLY -- the r221/r222 arms
    # are exonerated, a fresh composition. The finder demands one
    # "&<name>::" head and one "*<name>:" value-indicator glue, both at
    # node positions. 60-mutant paired battery: 0 regressions (9
    # family flips abort->pass; the 22 still-fires are all SPLIT
    # verdicts -- empty-name anchor/alias and quote-glued heads --,
    # unchanged pre/post). The deletion sweep's split twins ("&::",
    # "*:") ride the split path, not this arm.
    add_test(
        NAME fuzz-diff-r223-block-anchored-glued-colon-alias
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-anchored-glued-colon-alias-r223.bin
    )

    # the r224 nightly (run 42604872810, main 988ba3e, crash-
    # 3922bb61f22630e1b2517b89fc52494d4ee1cc84): the CR-CROSSING
    # COMMENT-SWALLOW KEY BINDING of the flow-'?' family (the 21-byte
    # seed "[Q,?##|,:\rbnaaryM,?g]"). libyaml's 1.1 KEY-token law makes
    # the flow '?' a KEY token whatever follows, and the scanner's
    # next-token loop then treats the GLUED '#' as a boundary comment:
    # the comment swallows to the NEXT LINE BREAK -- a lone CR is a
    # break -- and the post-break plain rides as the keyless pair's
    # key ({bnaaryM: <empty>}). yeptris's suite-pinned plain-first-'?'
    # law (HM87) reads the glued run as flat scalars ("?##|", ":|")
    # and folds the CR INSIDE the plain. No single prior arm claims
    # the soup: r185's comment-CR spell heals one '?#'+follower hit
    # but leaves the tail's bare glued-'?' runs unhealed, and r183's
    # spell declines the tail's runs (its clean check rejects any ':'
    # in the run) while the artifact's tail rides "?tag:yaml.org" --
    # a GLUED-':' run. The arm COMPOSES the two healing spells in one
    # walk: HIT A keeps the '?' and deletes the '#' through the
    # comment's end (the r222 keep-? law -- separated from its
    # follower by the break, the '?' re-lexes as the explicit KEY in
    # BOTH engines; the comment must END at a CR/LF break and the
    # post-break follower must start a wordy plain run); HIT B
    # rewrites "?run" -> "{run: }" (the r183 spell) with the r224
    # widening -- a ':' GLUED inside the run rides as mid-plain
    # content, a ':' after a blank declines, and a '#' glued after a
    # non-blank is content while a blank-led '#' declines. The
    # byte-strict verify against libyaml's ORIGINAL dump arbitrates,
    # with the CR/LS/PS/NEL-normalized triple as fallback. The A/B
    # against the pre-r223 parent 6330309 aborts IDENTICALLY -- the
    # r223 arm is exonerated, a fresh composition. 60-mutant paired
    # battery vs main d5778d8: 0 regressions (19 family flips
    # abort->pass; the 23 still-fires are byte-identical pre/post).
    # The six-corpus *_lf walks (parse 343, feed 65, marshal 18, cbor
    # 102, validate 5, roundtrip 5) are byte-identical pre/post; the
    # 113 existing pins stay green.
    add_test(
        NAME fuzz-diff-r224-qflow-seq-glued-hash-cr-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-seq-glued-hash-cr-key-r224.bin
    )

    # the r225 nightly (run 42613510600, main 355568f, crash-
    # 277015f0d109025c7c028c614561e349224db928): the FOLD-CAP WIDENING
    # of the r203 flow-SEQ question family -- the r203 artifact's own
    # direct descendant ("[\n?+D<LS> 'c<PS>.:j:(<PS>\n?+D<LS> 'c<PS>
    # .:j:(<PS> \r|\r\n.\r|\r\n.tag:yamd.org,2002:m*a \r|\r\n.\r|\r\n
    # .tag:yamd.org,2002:m*apoa\"]", 112 bytes). libyaml's flow
    # KEY-token law reads the glued '?' as a KEY and its fold law keeps
    # the mid-plain LS/PS RAW (READ_LINE copies them verbatim; the
    # leading byte is never '\n', so the join keeps lead+trailing
    # verbatim), while yeptris's spec fold law collapses the run
    # (k>=2 breaks -> k-1 LFs) -- the exact KEY VALUE BYTES diverge,
    # and the r203 bake carries libyaml's folded key verbatim. The run
    # rides 11 folds (LS PS PS LF LS PS PS CR CRLF CR CRLF) and the
    # arm's fold cap rode 8 -- every other admission already held: the
    # GLUED ':'s ride the pair-separator law (each ':' precedes a
    # non-blank), the embedded second '?' rides the r219 admission,
    # the mid-run quote rides the r191 law. The cap widens 8 -> 16;
    # the trailing[24] bake buffer still declines gracefully past its
    # caps (wok=0 rides the hit as content), so the widening only
    # reaches folds the byte-strict verify then arbitrates. A FRESH
    # find, not an r224 regression: the A/B against the pre-r224
    # parent d5778d8 aborts IDENTICALLY on both builds (same stream
    # pair). Minimality: the unconstrained deletion minimizer drifts
    # to the 1-byte "\xe2" YEPTRIS-ONLY split twin (the r186-r224
    # drift law); the DIFFER-constrained rerun (require the
    # libyaml-+MAP-vs-yeptris-plain tell) pins the 26-byte
    # "[?D<LS><PS><PS>\n<LS><PS><PS>\r\r\"]"; the pinned seed is the
    # TRUE 112-byte artifact. 60-mutant paired battery vs main
    # d5778d8: 0 regressions (29 admissions abort->pass; the 22
    # still-fires are byte-identical pre/post; 9 clean-same). The
    # six-corpus *_lf walks (parse 343, feed 64, marshal 17, cbor 101,
    # validate 5, roundtrip 4) are byte-identical pre/post; the 114
    # existing pins stay green.
    add_test(
        NAME fuzz-diff-r225-qflow-multiline-fold-run
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-multiline-fold-run-r225.bin
    )

    # the r226 nightly (run 42622039396, main 892510b, crash-
    # c76eee55d5fcf2ea27de2badded914ebd28bba3b): the NEL member of the
    # UTF-16 line-start BOM family -- UTF-16BE "mark<NEL><BOM><NEL><BOM>
    # <NEL>a" (14 bytes). libyaml's scan_to_next_token drops a BOM at
    # mark column 0 whatever break opened the line (the r154 law,
    # byte-probed across LF/CR/NEL/LS/PS), while yeptris keeps every
    # content BOM (the catch-121 law): yeptris's plain reads
    # "<BOM> <BOM> a" (the two interior NELs fold to spaces, the first
    # rides as the plain's consumed leading break) against libyaml's
    # bare "a". The r100 arm's line-start gate tracked LF only, so both
    # NEL-line-start BOMs slipped past it (feature never fired); the
    # r159 single-(BOM+fold) strip reaches only one pair, so the
    # TWO-BOM composition stayed unknown. A FRESH find, not an r225
    # regression: the A/B against the pre-r225 parent fe760cd aborts
    # IDENTICALLY on both builds (same stream pair). The widening is
    # gate-only (the arm's twin + skeleton equality gates are
    # untouched): 60-mutant paired battery vs main 892510b, 0
    # regressions (27 admissions abort->pass; the 10 still-fires are
    # byte-identical pre/post; 23 clean-same). The UTF-8 byte arm's
    # probes widened in lockstep (C2 85 / E2 80 A8/A9 line starts --
    # the u2 twin "\n?NEL<BOM>NEL<BOM>NELa" member). The six-corpus
    # *_lf walks (parse 344, feed 64, marshal 17, cbor 101, validate 5,
    # roundtrip 4) are clean pre/post; the 115 existing pins stay
    # green. Minimality: the unit-deletion sweep pins the 10-byte
    # mark<NEL><BOM><NEL><BOM>a twin (any single-unit cut that keeps
    # two NEL-line-start BOMs stays unknown; single-BOM members already
    # classify through the r159 strip); the pinned seed is the TRUE
    # 14-byte artifact.
    add_test(
        NAME fuzz-diff-r226-bom-utf16-nel-line-start
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-utf16-nel-line-start-r226.bin
    )

    # the r227 nightly (run 42630534433, main 0b72c24, crash-
    # 03fcab71b7649852477c5f7163d0a42ff70f2bba): the DIRECT-ADJACENCY
    # member of the r218 root-alias family -- the 9-byte "&a:o:\n*a:"
    # (the mutator deleted the r218 seed's 19-LF blank run down to the
    # bare break). libyaml's 1.1 IS_ALPHA anchor/alias charset cuts
    # both names at ':' and reads the flat pair map (key "o" anchor
    # "a" + empty value, then the alias key "a" + empty value), while
    # yeptris's suite-pinned riding law (2SXE: "&a: key" anchors "a:"
    # and "*a:" names "a:"; W5VH keeps colons inside names -- the
    # spec's ns-anchor-char includes ':') keeps "a:o:" pending at the
    # root and names the alias "a:", so no value indicator opens a map
    # and the document collapses to ONE riding-name alias event. A
    # FRESH member, not a regression: the r225/r226 fixes (#1198,
    # #1204) are comparator-only and the artifact's stream pair is the
    # r218 family verbatim. The carry widens the r218 arm in place
    # (the blank run was always inert on the collapse): the separator
    # law goes ZERO-OR-MORE blank lines, leading blank lines claim as
    # an inert prefix, the exact-byte name pins widen to a conservative
    # riding-name set ([A-Za-z0-9_.-] + the riding ':' -- each run
    # required to END on its ':'), and a line-start "..." doc-end tail
    # claims as an inert suffix (all dumpboth-verified same pairs).
    # The 97-mutant battery around the artifact: 96 clean, 1 decline
    # (the no-tab gate's tab member, by design). Still unclaimed:
    # props-restart names, non-ASCII names, colon-glued "...". The
    # six-corpus *_lf walks are clean pre/post; the 116 existing pins
    # stay green. Minimality: the artifact is the 9-byte minimum
    # itself (the "&a:o:\n*a:" skeleton has no smaller DIFFER member;
    # every single-byte deletion classifies clean or splits).
    add_test(
        NAME fuzz-diff-r227-root-alias-direct-adjacency
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-direct-adjacency-r227.bin
    )

    # the r228 nightly (run 42645063770, main 77b1709, crash-
    # 8100227a1abca67d3bebc6afaa1c301090471b08): the PROPS-LED member
    # of the r29 flow-follower colon family -- the 11-byte "&k2: !
    # {\n}\n". The r29/r31 un-consume law fired only when the riding
    # colon's follower was a FLOW collection or BLOCK SCALAR HEADER
    # directly; here the run between the separator and the collection
    # is the VALUE's own properties ("! {" -- the tag "<!>" belongs to
    # the flow map), so the law never fired: yeptris rode the colon
    # into the anchor name ("k2:") and stamped name + tag on the map
    # itself ("+MAP &k2: <!>") where libyaml reads the anchored null
    # KEY plus the tagged flow map. The ENGINE fix (the r29 doctrine:
    # the riding law yields where its result is observable) extends
    # the follower scan in e_props_ex to cross ONE props run
    # ("!tag"/"&anch", verbatim "!<!..>" included) before the
    # indicator check, and a crossed run un-consumes regardless of
    # what follows -- the tag-led PLAIN member ("&k2: ! t", dumpboth
    # pre-fix: yeptris's anchored "&k2:" scalar vs libyaml's pair) and
    # the bare-tag/EOF tail ("&k2: !") heal too, and the anchor-led
    # twin ("&k2: &v {...}", pre-fix a REJECT vs libyaml's accept)
    # now parses the anchored pair. Plain-led followers never cross a
    # props run: 2SXE/W5VH ("&a: key") keep riding, and the r227
    # root-alias family's alias follower is untouched. Three r44/r156-
    # era pins moved with the law (the composite "&va2: &node2" now
    # parses the anchored pair -- byte-exact vs libyaml; the round-44
    # "&:\n! |\n:\n" form now REJECTS like libyaml's empty-anchor-name
    # law, dissolving the unserializable-block concern). Paired
    # battery vs parent 77b1709: 128 mutants, 17 admissions abort->
    # pass, 0 regressions, 52 still-fires -- ALL splits (yeptris
    # errors) under the lf DIFFER-only gate: 0 aborts. The residual
    # layer (multi-line flow content/closer at column 0 under a block
    # pair: yeptris BAD_INDENT vs libyaml's lenient span) is
    # PRE-EXISTING on main (anchor-free "a: {\n}" errors identically
    # pre/post) and stays outside this round, so the flow-form
    # artifact itself lands as a SPLIT post-fix (corpus seed
    # anchored-props-led-flow-r228.bin rides the nightly's lf gate,
    # which does not abort splits) while the PIN replays the
    # tag-led-PLAIN member "&k2: ! t" -- EQUAL post-fix, a riding
    # DIFFER on any revert of the engine law (the same crossing), so
    # the regression detection is exact. The six corpus walks are
    # clean pre/post; full ctest 907/907; `leaks --atExit` clean.
    add_test(
        NAME fuzz-diff-r228-anchored-props-led-flow
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchored-props-led-plain-r228.bin
    )

    # the r229 nightly (run 42655755989, main 97bd9df, crash-
    # 603d262373336d8b98a7e77396f5f418a8690d26): the QUOTE-COMMENT-GLUE
    # member of the qflow glued-# family -- the 16-byte minimum
    # "[1,?#,\"<CR>g\",?g:m]" (byte 8 a RAW CR) and the TRUE 10,732-byte
    # artifact over the family's repeated members. libyaml's
    # comment-glue (the r178 law: it eats from the glued # to the raw
    # break WITHOUT tracking quotes) swallows the '#,' AND THE OPENING
    # QUOTE and resumes mid-scalar at the baked break: the g" tail is
    # plain content, the ',' separates the orphaned '?' KEY token's
    # map (key g", empty), and the trailing '?g:m]' opens the SECOND
    # map (key g:m, empty). yeptris composes the other way: the
    # plain-'?' law keeps '?#' as content, the quoted scalar parses
    # WHOLE (the raw break folds inside the flow quotes), and '?g:m'
    # rides as one plain -- 4 scalars vs 2 maps, an event-KIND
    # divergence the r137/r138 verdicts rule unsound to generalize.
    # REGRESSION TRIAGE CLEAN: the artifact aborts IDENTICALLY (exit
    # 134, the same dump pair) on the pre-r228 parent 77b1709 -- the
    # r228 props-led ENGINE round is exonerated. The carried arms
    # decline: r224's HIT A comment scan is not quote-aware, the
    # r137-r141/r163 walks assume comment ranges without baked breaks,
    # the r203 escape-baked arm is a different spell; no spell
    # transform reconciles the pair. The carry rides the r129/r139
    # EXACT-BYTES gray discipline: the skeleton walk (yd_r229_flow_
    # quote_comment_glue_gray -- ws* [ ws* 1 ws* ,?#," RAWBREAK g" ws*
    # , ws* ?g:m] ws*) claims the minimum plus the six MEASURED
    # inert-whitespace positions (the r228 law), and the artifact rides
    # length+FNV-1a-64 against its committed corpus seed. The
    # 118-mutant census around the minimum: 72 claimed (the skeleton's
    # ws law), 46 still-fires -- interior content variants
    # (glue-position shifts, quoted-body bytes, trailing-pair bytes),
    # each its own entry per the cadence. The six corpus walks are
    # clean (parse 349); the 118 existing pins stay green. Seeds:
    # qflow-quote-comment-glue-r229.bin (the minimum, this pin) +
    # qflow-quote-comment-glue-artifact-r229.bin (the TRUE artifact).
    add_test(
        NAME fuzz-diff-r229-qflow-quote-comment-glue
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-quote-comment-glue-r229.bin
    )

    # the r230 nightly (run 42665259808, main 73c447e, crash-
    # b841bb3aa948b2f36e68252006f4a7869af10dff): the DOUBLE-split member
    # of the r142 anchor-name charset family -- the 59-byte
    # "&I:919999k: &ab?cd v\n--- !!m~~top4~~~...": TWO riding names on
    # one line ('&I:919999k' riding the interior colon at the key, then
    # the VALUE's '&ab?cd' riding the '?' -- libyaml's IS_ALPHA set
    # excludes both ':' and '?'), separated by the KEY colon. yeptris's
    # ns-char set rides both ("I:919999k" and "ab?cd" as the anchor
    # names, empty key + plain "v"); libyaml cuts at the first
    # out-of-set byte of each run (anchor "I" + plain key ":919999k",
    # anchor "ab" + plain "?cd v"). Regression triage CLEAN: the
    # pre-r229 main 97bd9df aborts identically (the r229 quote-glue
    # round is comparator-only and exonerated). The r142 arm's split
    # transform carried only the FIRST split: the residual blind copy
    # ran no colon law, so node_pos2 stayed dead after it and the
    # SECOND '&ab' split was skipped -- the copy kept the riding name
    # and the copy-agrees leg declined the arm's own family (both
    # verify legs hold by hand on the fully-split copy: dumpboth-
    # verified). The fix (the r230 residual-colon law) runs the gate's
    # colon law THROUGH the residual blind copy (the key colon ends
    # the riding plain and arms node_pos2; a colon-led residual arms
    # the plain), and the trailing plain_residual only arms when the
    # residual had NO colon of its own -- clobbering plain_open2 after
    # the key colon reopened the plain over the node position (the
    # gate never clobbers). 359-mutant census: 0 aborts (the arm is
    # general -- the whole name-variant neighborhood carries). The six
    # corpus walks are clean (parse 352); the 119 existing pins stay
    # green. Seed: anchor-charset-residual-colon-r230.bin (this pin).
    add_test(
        NAME fuzz-diff-r230-anchor-charset-residual-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-residual-colon-r230.bin
    )

    # The r231 nightly (run 42677255629, main edcece1, crash-238624b1)
    # caught the r203 flow-'?' x LS/PS fold family's ALL-BLANK-RUN
    # member: the 112-byte artifact "[\n?+D<LS> 'c<PS>.:j:(<PS> \r|
    # \r\n.D ..." repeats the ?+D member four times with an interior
    # raw LF inside the break run, and the 42-byte minimal
    # "[?D<LS><PS><PS>^3<LF><LS><PS><PS>\"]" rides 13 breaks (12
    # trailing). Two bake-buffer obstructions, both work bounds the
    # byte-strict verify arbitrates: the trailing[24] buffer stored
    # only 8 LS/PS triples past the raw lead (the 12-trailing-break
    # run needs 34 bytes -- wok=0 declined the hit), and the 16-break
    # cap (the artifact rides 23 folds). The carry widens the trailing
    # buffer to 64*3 and the break cap to 64. 1540-mutant paired
    # battery vs parent edcece1: 579 fixed / 0 regressions / 41
    # both-abort / 920 both-pass; six corpus walks clean (parse 353);
    # the 120 existing pins stay green. Seeds:
    # qflow-lsps-allblank-min-r231.bin + qflow-lsps-allblank-r231.bin
    # (this pin replays the TRUE artifact: it needs BOTH widenings --
    # the buffer alone still declines at the 16-break cap -- so revert
    # detection is exact on both legs).
    add_test(
        NAME fuzz-diff-r231-qflow-lsps-allblank-caps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-lsps-allblank-r231.bin
    )

    # The r232 nightly (run 42688765844, main e8ba885, crash-1020a8db)
    # caught the r229 quote-comment-glue gray arm's DOUBLED member: the
    # 18-byte minimum '[1,?#,"<CR>g",?g?,""]' and the TRUE 24-byte
    # artifact '[1,?#,"<CR>g",?g:m?#,"<CR>g",]' -- the flow-'?' member's
    # plain tail rides the GLUED '?#' (mid-plain content in BOTH
    # engines; libyaml's KEY fired only at the leading '?', its second
    # salvage map's key is the byte-exact 'g:m?#'), and the trailing
    # quoted member ('"<CR>g"' folded to ' g') parses WHOLE on both
    # sides -- libyaml's map closed empty at the comma, so the tail
    # members AGREE. Regression triage CLEAN: the pre-r231 parent
    # edcece1 aborts identically. The carry widens the r229 skeleton in
    # place: the tail is the measured set [a-z0-9:?#] (non-empty,
    # ':'/'#' never last), then comma-separated inert members ("") and
    # the CR-folded quoted form) with a trailing comma allowed -- the
    # exact '?g:m]' path is the empty-suffix member unchanged; the TRUE
    # artifact rides a length+FNV-1a-64 pin. 504-mutant paired battery
    # vs parent edcece1: 37 fixed / 0 regressions / 97 both-abort (the
    # gray cadence) / 370 both-pass; the r229 seeds stay green; six
    # corpus walks clean (parse 355); the 121 existing pins stay green.
    # Seeds: qflow-quote-comment-glue-doubled-min-r232.bin (this pin,
    # the skeleton leg) + qflow-quote-comment-glue-doubled-r232.bin
    # (the TRUE artifact, corpus-side, its FNV leg).
    add_test(
        NAME fuzz-diff-r232-qflow-quote-comment-glue-doubled
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-quote-comment-glue-doubled-min-r232.bin
    )

    # The r233 nightly (run 42697464143, main e2b7464, crash-72a25efe)
    # caught the r203 flow-'?' arm's DOWNSTREAM-LS/PS composition: the
    # 132-byte artifact carries its raw LS/PS bytes in members the
    # rewrite never touches -- the r203 bake+exact-verify handled the
    # '?'-led member (baked key "+D\P\n\L'c") but the exact compare
    # declined on member 3's delta alone (libyaml keeps the downstream
    # plain's LS/PS RAW verbatim -- the r231 finding -- where yeptris
    # folds). The carry is the r233 TWO-PHASE VERIFY in the r203 arm:
    # phase 1 the exact compare (every previously-exact member keeps
    # its exact path); phase 2 on decline -- the rewrite exact for
    # libyaml (ldump2 == ldump byte-exact) and the yeptris leg
    # re-verified through the adjudicated LS/PS fold walk
    # (yd_dump_eq_mod_lsps_fold) -- the composition of two trusted
    # carries, the r166 lesson applied to the verify. 1320-mutant
    # paired battery vs parent 9e78756: 1028 fixed / 0 regressions /
    # 16 both-abort / 276 both-pass (the walk admits the whole
    # downstream-LS/PS neighborhood); six corpus walks clean (parse
    # 356); the 122 existing pins stay green. Seed:
    # qflow-lsps-downstream-plain-r233.bin (this pin).
    add_test(
        NAME fuzz-diff-r233-qflow-lsps-downstream-plain
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-lsps-downstream-plain-r233.bin
    )

    # The r234 nightly (run 42706321967, main f856286, crash-4262eff1)
    # caught the r142 anchor-charset arm's PS-terminated-doc-marker
    # member: the 18-byte "---<PS> ? &--02:in~" (PS = raw U+2029).
    # The '-x' plain clause armed a phantom plain on the FIRST dash
    # ('-' followed by the non-blank '-'), and the doc-marker clause
    # that resets it (coll_col=-1, plain_open=0) required the
    # terminator in {space,tab,nl,cr} -- the raw PS is none, so the
    # reset never fired, node_pos rode 0 forever, and the gate
    # declined its own family (the bare "? &--02:in~" and the
    # LF-terminated doc marker both carried -- with an LF the
    # doc-marker clause fires on every dash). The carry is the r234
    # law in BOTH the gate and copy walks: the doc-marker terminator
    # set admits the raw multibyte breaks (the r87 break model,
    # yd_mb_break_len). Regression triage CLEAN: the pre-r234 parent
    # f759d1f aborts identically. 198-mutant paired battery vs parent:
    # 55 fixed / 0 regressions / 5 both-abort / 138 both-pass; the
    # bare controls stay green; six corpus walks clean (parse 357);
    # the 123 existing pins stay green. Seed:
    # anchor-charset-ps-docstart-r234.bin (this pin).
    add_test(
        NAME fuzz-diff-r234-anchor-charset-ps-docstart
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-ps-docstart-r234.bin
    )

    # The r235 nightly (run 42739927952, main 52cc501, crash-5de6f9ff)
    # was the fuzz_roundtrip harness's catch: an ENGINE emit bug. The
    # 19-byte artifact `&k:": #-]<PS> ? &in~` parses to a root map
    # anchored by a RIDING-COLON name (`k:":` -- the ns-anchor-char
    # set rides ':' and '"' to the blank), and the root-props LEAD
    # LINE spell (the catch-56 root-side law: root block-collection
    # props ride their own leading line) wrote it UNQUOTED as
    # `&k:":\n<content>` -- the lead-line path never consulted the
    # anchor spell law. The re-parse mis-attaches the riding name (the
    # anchor is lost; the 11-byte minimum `&: #<PS>? &~` rides to
    # `&:` which re-parses as an anchored empty KEY of the follow-on
    # collection -- s1 != s2, byte growth). The fix is the r235 law:
    # the root lead line gates its anchor write on the SAME
    # anchor_name_unspellable law as every other anchor write path --
    # a colon-tailed (or break-named) anchor on a root block
    # collection has NO safe lead-line spelling, so the serialize
    # fails (the r228 "&: |" law's root-side twin). Normal anchored
    # root maps (`&a\nb: 1`) keep the lead-line spell. Regression
    # triage CLEAN: the pre-r235 parent 03526bb aborts identically.
    # 360-mutant paired battery vs parent: 103 fixed / 0 regressions /
    # 257 both-pass (the unsafe neighborhood now fails the serialize
    # cleanly); controls `&a\nb: 1`, `&a:b\nk: v` stay green; six
    # corpus walks clean; the 124 existing pins stay green. Seed:
    # roundtrip-root-anchor-colon-lead-r235.bin (this pin).
    add_test(
        NAME fuzz-roundtrip-r235-root-anchor-colon-lead
        COMMAND fuzz_roundtrip
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/roundtrip-root-anchor-colon-lead-r235.bin
    )

    # The r358 nightly (run 45040187068, main dd156125, crash-
    # 753f0d6ec8ce167d93ae14aec28d24caa4eda111): the round-44
    # trailing-colon anchor law's empty-inline twin — a DOM anchor
    # whose name ends at ':' on an EMPTY plain value spelled the
    # anchor line-final ("&4:- \"\": &4: " + EOL; the non-empty rider
    # "k: &n: v" always carried its own follower byte), and the
    # re-parse's riding-colon follower scan crossed the break into
    # the props-led "&1 ..." line, un-consumed the colon, and the
    # stripped ':' died ("mapping values are not allowed in this
    # context" at pass1 line 8 col 12) — yeptris rejecting its own
    # emitter's output, the roundtrip guarantee's face. The empty
    # twin now takes the explicit-empty quoted spell ("&4: ''"): the
    # quote byte is a plain-led follower, the name rides whole, and
    # the sticky quoted style re-spells byte-identically at pass2.
    # Adjudication: ENGINE (emit-side), not comparator — parse1's
    # DOM is the pinned suite-1.2 anchor law (libfyaml 04e0b58 reads
    # the same long names; libyaml 90a56d4's [0-9A-Za-z_-] charset
    # and libfyaml both REJECT the artifact input outright, so the
    # roundtrip contract here is yeptris-internal), and the emitter
    # owns a stable spell. Regression triage: the artifact round-
    # trips byte-stable post-fix; non-empty riders spell unchanged
    # ("k: &n: v"); the round-44 unserializable fails stay. Seed:
    # anchor-colon-empty-value-r358.bin (this pin).
    add_test(
        NAME fuzz-roundtrip-r358-anchor-colon-empty-value
        COMMAND fuzz_roundtrip
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip/anchor-colon-empty-value-r358.bin
    )

    # The r236 nightly (run 42752605314, main 5632789, crash-6a632d98)
    # rode the r231 flow-'?' x LS/PS fold family with EXOTIC content
    # between the break runs: the 38-byte artifact
    # `[?D<LS><PS><PS><U+2A29><PS><PS> U\<CR> <PS><PS><LF><LS><PS><PS>"]`
    # (32-byte minimal: the r231 shape with one U+2A29). The exotic
    # bytes are INERT content both engines keep raw -- the r203 run
    # scan declined them (d >= 0x80) and the DQ bake declined
    # (b > 0x7E), and the artifact's mid-run backslash hit the scan's
    # conservative '\' decline. The carry widens three gates in the
    # r203 arm, all work bounds the byte-strict verify arbitrates:
    # (1) the run scan admits a complete VALID UTF-8 sequence that is
    # not a break unit (fragments and overruns decline); (2) the DQ
    # bake's escape law rides bytes >= 0x80 raw (valid UTF-8 carries
    # raw in both engines' double-quoted scalars); (3) the '\' decline
    # lifts -- a plain backslash is CONTENT in both engines and the
    # bake escapes it as \\. 840-mutant paired battery vs parent
    # 5e2141e: 79 fixed / 0 regressions / 761 both-pass; six corpus
    # walks clean (parse 358); the 125 existing pins stay green. Seed:
    # qflow-lsps-utf8-content-r236.bin (this pin).
    add_test(
        NAME fuzz-diff-r236-qflow-lsps-utf8-content
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-lsps-utf8-content-r236.bin
    )

    # r237 (nightly round 237, crash-7be6d7634714addf17298a04006b77a3a8644572,
    # 18-byte minimum "> #<PS> :\n  <NEL> 0\n t"): the literal block
    # scalar's post-NEL-run fold boundary. yeptris keeps the NEL raw and
    # the continuation line's leading space as content while libyaml
    # escaped the NEL break and ate the space as the continuation's
    # indent ("[NEL] SP 0" vs "\n0"), and behind that yeptris's 1.2 fold
    # emits an escaped line break where libyaml kept the fold space
    # ("esc t" vs "SP t"). Two arms in yd_block_nel_walk_mult: (1) the
    # r208 surplus scan now eats yeptris's NEL-site indent surplus before
    # its escape-run scan (only while the dumps disagree behind, capped,
    # so the run law's surplus==k + alignment byte still bind); (2) the
    # r237 fold-space mirror in the walk's main loop rides ONE
    # escape<->space pair gated on the alignment byte behind the pair --
    # both arms sit where the walk previously declined unconditionally,
    # so every existing member keeps its exact path. 129-mutant paired
    # battery vs parent 931d7d7: 12 fixed / 0 regressions / 117
    # both-pass; six corpus walks clean (parse 359); the 126 existing
    # pins stay green. Seed: block-nel-nel-foldspace-r237.bin (this pin).
    add_test(
        NAME fuzz-diff-r237-block-nel-foldspace-mirror
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-nel-foldspace-r237.bin
    )

    # r238 (nightly round 238, crash-3d571437a4027d3b46c234964191c77cd4febdf4,
    # 10-byte minimum "&2: '<LS>\r'", 18-byte artifact
    # "&k2: 'j:(<LS> \r|\n '"): the r218/r227 collapse family's
    # QUOTED-FOLLOWER member. yeptris's riding laws (2SXE/W5VH) keep the
    # anchor name's colon and collapse the line to ONE scalar whose
    # content embeds the anchor spelling plus the quoted body through
    # the plain fold; libyaml's 1.1 charset cut frees the colon as the
    # value indicator and parses the quoted scalar as the map value
    # under its own fold. Measured composition-only: the standalone
    # quoted probes ('a<LF>'/'a<CR>'/'a<LS>'/'a<LS><CR>'/'<LS><CR>'/
    # 'a<CR><LF>') ALL AGREE -- no standalone engine bug. The r238 gray
    # arm walks BOTH dumps against the input bytes: printables byte-
    # equal, raw NEL/LS/PS verbatim (the r231 law), a space run before
    # a break folding with it (yeptris to ONE space; libyaml to esc \n
    # after a raw triple -- its blank-state quark -- else one space), a
    # standalone break folding the same way, and a trailing space run
    # merging with a just-folded break on BOTH engines. REGRESSION
    # TRIAGE CLEAN: the pre-r238 parent fdda224 aborts identically --
    # a fresh find, the r237 carry exonerated. 230-mutant paired
    # battery vs parent fdda224: 36 fixed / 0 regressions / 181
    # both-pass / 13 both-abort; six corpus walks clean; the 127
    # existing pins stay green. Seed: root-anchor-quoted-follow-r238.bin
    # (this pin).
    add_test(
        NAME fuzz-diff-r238-root-anchor-quoted-follow
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-r238.bin
    )

    # r239 (nightly round 239, run 42786114675, main e75c3949,
    # crash-05d604c94124e5d64489ca5700dc3879fbf139ff, 20-byte minimum
    # "<NEL>...<NEL> >\n <NEL>...<NEL>\nt"): the value drains skipped
    # the parse entry's break normalizer. yeptris_parse_ex (compat
    # schema) normalizes NEL/LS/PS to LF up front (parse_frontend) and
    # accepted the document, but yep_values_from_input only did BOM +
    # UTF-16 transcoding -- the raw NEL rode to the SAME compat-grammar
    # engine and the "..." marker line-start gate errored the run, so
    # yeptris_marshal returned ERROR_PARSE on a parseable document (the
    # fuzz_roundtrip marshal-contract trap, line 42). The fix threads
    # the same normalize step into the drain's engine branch (JSON
    # routes stay above it, the parse_frontend law); ownership rides
    # the existing `transcoded` plumbing. ENGINE round #17. Unit pin
    # Marshal.ParseableNelMarkerDocMarshalParity (fails pre-fix, passes
    # post-fix); 484-mutant paired battery vs parent e75c3949: 116
    # fixed / 0 regressions / 368 both-pass; six corpus walks clean;
    # the 128 existing pins stay green. Seed:
    # nel-docmarker-drain-r239.bin (this pin).
    add_test(
        NAME fuzz-roundtrip-r239-nel-docmarker-drain-normalize
        COMMAND fuzz_roundtrip
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip/nel-docmarker-drain-r239.bin
    )

    # the r248 round (run 43151917346, main 14ccfa6, crash-
    # b8d4a4b3315658e37cfab11523638871d81c2ab7): BYTE INSTABILITY in the
    # emitter's folded-line walk - the fold-point selection (break AT a
    # space past best_width) refused space-adjacent breaks but not
    # TAB-adjacent ones, so a fold landed with a TAB at the folded
    # line's END (stripped as trailing s-white on re-parse) and/or a
    # TAB at the continuation's START (stripped as leading s-white):
    # 149 -> 146 bytes across parse-serialize-parse-serialize. The
    # #1268 single-quoted fold walk and the plain twin both grow the
    # tab guards (the fold consumes the space; p[i-1]/p[i+1] != '\t').
    # 933/933 ctest with the libyaml emission goldens UNCHANGED (the
    # guard only shifts bytes for tab-adjacent folds, which the goldens
    # do not exercise); paired 50-mutant battery PRE/POST (matched
    # standalone binaries, same seed): 0 regressions, POST fixes the
    # artifact. Seed: tab-fold-r248.bin (this pin).
    add_test(
        NAME fuzz-roundtrip-r248-tab-fold
        COMMAND fuzz_roundtrip
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/roundtrip/tab-fold-r248.bin
    )

    # r240 (nightly round 240, run 42801813638, main bf8342c era,
    # crash-63cbaa8cea06537b9ba5ded6a935c75e9c34174c, 12-byte minimum
    # "[)<PS>(?t,?$]"): TWO suite-over-libyaml laws compose and neither
    # alone equalizes the pair. The first member carries an interior
    # PS: yeptris (1.2) normalizes and folds it to ONE space in the
    # flow plain; libyaml's READ_LINE joins the break RAW (the r56
    # plain-LS/PS law). The last member starts with a bare '?' glued
    # to a non-space: yeptris reads it as ONE plain scalar (the
    # suite-pinned plain-'?' law); libyaml's 1.1 grammar takes it as
    # the explicit-key indicator and opens "? $ : <empty>" (the
    # carried flow-explicit-key family). libyaml's dump closes
    # everything (this family CLIPS nothing); the composite defeats
    # the single-law arms, so yd_r240_flow_qtail_clip_gray walks both
    # dumps against the input bytes and declines anything its gates
    # cannot verify. Regression triage: the pre-arm parent b41870e
    # aborts identically (a fresh find, no arm regression, no engine
    # bug). Seed: flow-qtail-psfold-r240.bin (this pin).
    add_test(
        NAME fuzz-diff-r240-flow-qtail-psfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qtail-psfold-r240.bin
    )

    # r241 (nightly round 241, run 42919365127, main f6111d8,
    # crash-aa326ab184531278a80ecf9d719d6ef1cfc26c17, 9-byte minimum
    # "&a:\t>\n*a:" — libFuzzer-minimal, the artifact itself): the
    # root-alias collapse family's TAB + folded-header member. THREE
    # agreements and the family's own delta: BOTH engines render the
    # head scalar as `&` + name + ONE space (the TAB blank folds like
    # any blank run), BOTH take the `>` as an EMPTY folded-scalar
    # value (the `*a:` line sits at column 0), and at the second key
    # yeptris's suite-pinned riding laws (2SXE/W5VH) let the colon
    # RIDE the alias name (`=ALI *a:` as an unvalued key) while
    # libyaml's 1.1 IS_ALPHA cut splits it (`=ALI *a` + an empty
    # value, the r142/r76 law). The r218 arm declines this shape twice
    # (its no-tab gate, its blank-run separator law), so
    # yd_r241_alias_tab_fold_gray gates the byte shape and walks both
    # dumps exactly, declining anything its gates cannot verify (the
    # header suffixes and trailing breaks escalate). Regression triage:
    # a fresh worktree build at the pre-r238-arm parent ad84a13a
    # aborts identically (a fresh find, no arm regression, no engine
    # bug). Seed: root-alias-tabfold-r241.bin (this pin).
    add_test(
        NAME fuzz-diff-r241-alias-tab-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-alias-tabfold-r241.bin
    )

    # r242 (nightly round 242, run 42930337137, main dc6d2e2 — the
    # nightly's own failure IS the pre-arm A/B, crash-be5b7d7bc6a1d1db
    # 754c32fe8e146caa473e85a3, 13-byte minimum "[?,,g,?g?,\"\"]"): the
    # flow explicit-key family's glued-? MIDDLE member with no exotic
    # bytes — the r240 carry pinned the glued-? tail behind a PS-folded
    # head, this member is the grammar itself. The lone '?' member
    # opens the explicit-key map on BOTH sides (they agree — the suite
    # law protects only the GLUED '?'); quoted/plain/empty members
    # agree; the glued-? member ('?' + a non-space indicator-free run,
    # interior/trailing '?' included — content on both engines at
    # non-leading positions) is the delta: yeptris's suite-pinned
    # plain-'?' law (2SXE) reads it as ONE plain scalar while libyaml's
    # 1.1 grammar opens `? <rest> : <empty>`. yd_r242_flow_glued_q_
    # members_gray splits the members, classifies each, builds BOTH
    # expected dumps from the input bytes, and requires exact equality
    # on both sides; any byte the grammar cannot classify declines
    # (the escalation clause stays live). Comparator carry only — no
    # engine change. Seed: flow-glued-q-members-r242.bin (this pin).
    add_test(
        NAME fuzz-diff-r242-flow-glued-q-members
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-glued-q-members-r242.bin
    )

    # r243 (nightly round 243, run 42942836432, main f04fd40-era,
    # crash-1a7cef2afb69be84709fe4c13346cdce0a130940, 5206-byte artifact
    # that is ALREADY single-byte-deletion minimal — mutation-fragile as
    # a whole): the UTF-16BE mid-stream BOM composite. The input opens
    # with THREE `feff 0085` (BOM + NEL) code-unit pairs. yeptris (the
    # r144 fix) consumes ONLY the stream-initial BOM and keeps every
    # mid-stream `feff` code unit as U+FEFF CONTENT — the normalized
    # NEL following each folds to ONE space; libyaml's UTF-16 reader
    # eats the mid-stream `feff` code units as BOMs, so both its FEFF
    # characters and their fold spaces vanish. The relation is exact
    # and dump-shaped: libyaml's dump equals yeptris's dump with every
    # U+FEFF triple removed together with the single fold space that
    # follows each, plus ONE tail allowance — at the scalar-content end
    # libyaml's non-validating UTF-16 writer carries a bounded residue
    # (up to two complete characters plus one TRUNCATED UTF-8 sequence
    # at EOF) that yeptris's validating front-end declines. The A/B is
    # FREE: the nightly ran on pre-arm main and its failure IS the
    # pre-arm abort. Comparator carry only — no engine change. Seed:
    # utf16-midstream-bom-r243.bin (this pin).
    add_test(
        NAME fuzz-diff-r243-utf16-midstream-bom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-midstream-bom-r243.bin
    )
add_test(
        NAME fuzz-diff-r244-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r244.bin
    )

    add_test(
        NAME fuzz-diff-r245-flow-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r245.bin
    )
    add_test(
        NAME fuzz-diff-r245-flow-qsoup-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-artifact-r245.bin
    )
    add_test(
        NAME fuzz-diff-r246-flow-qsoup2
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup2-r246.bin
    )
    add_test(
        NAME fuzz-diff-r246-flow-qsoup2-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup2-artifact-r246.bin
    )
    add_test(
        NAME fuzz-diff-r247-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r247.bin
    )
    add_test(
        NAME fuzz-diff-r249-flow-qsoup2-ls
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup2-ls-r249.bin
    )
    add_test(
        NAME fuzz-diff-r250-flow-qsoup3
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup3-r250.bin
    )
    add_test(
        NAME fuzz-diff-r251-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r251.bin
    )
    add_test(
        NAME fuzz-diff-r252-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r252.bin
    )
    add_test(
        NAME fuzz-diff-r253-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r253.bin
    )
    add_test(
        NAME fuzz-diff-r254-flow-qhash-cr
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r254.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-00
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m00.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-01
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m01.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-02
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m02.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-03
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m03.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-04
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m04.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-05
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m05.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-06
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m06.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-07
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m07.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-08
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m08.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-09
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m09.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-10
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m10.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-11
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m11.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-12
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m12.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-13
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m13.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-14
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m14.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-15
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m15.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-16
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m16.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-17
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m17.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-18
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m18.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-19
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m19.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-20
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m20.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-21
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m21.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-22
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m22.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-23
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m23.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-24
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m24.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-25
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m25.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-26
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m26.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-27
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m27.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-28
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m28.bin
    )
    add_test(
        NAME fuzz-diff-r255-mined-29
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r255-m29.bin
    )
    add_test(
        NAME fuzz-diff-r256-flow-map-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-map-soup-r256.bin
    )
    add_test(
        NAME fuzz-diff-r257-flow-map-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-map-soup-r257.bin
    )
    add_test(
        NAME fuzz-diff-r258-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r258.bin
    )
    add_test(
        NAME fuzz-diff-r259-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r259.bin
    )
    add_test(
        NAME fuzz-diff-r260-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r260.bin
    )
    add_test(
        NAME fuzz-diff-r262-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r262.bin
    )
    add_test(
        NAME fuzz-diff-r263-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r263.bin
    )
    add_test(
        NAME fuzz-diff-r264-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r264.bin
    )
    add_test(
        NAME fuzz-diff-r265-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r265.bin
    )
    add_test(
        NAME fuzz-diff-r266-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r266.bin
    )
    add_test(
        NAME fuzz-diff-r267-qhash-cr-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r267.bin
    )
    add_test(
        NAME fuzz-diff-r270-qhash-alias-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r270.bin
    )
    add_test(
        NAME fuzz-diff-r269-qhash-nest
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r269.bin
    )
    add_test(
        NAME fuzz-diff-r271-block-ps-nel-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ps-nel-fold-r271.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-00
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f00.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-01
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f01.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-02
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f02.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-03
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f03.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-04
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f04.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-05
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f05.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-06
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f06.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-07
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f07.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-08
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f08.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-09
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f09.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-10
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f10.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-11
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f11.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-12
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f12.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-13
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f13.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-14
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f14.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-15
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f15.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-16
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f16.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-17
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f17.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-18
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f18.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-19
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f19.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-20
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f20.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-21
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f21.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-22
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f22.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-23
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f23.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-24
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f24.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-25
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f25.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-26
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f26.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-27
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f27.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-28
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f28.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-29
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f29.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-30
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-f30.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-31
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m31.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-32
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m32.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-33
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m33.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-34
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m34.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-35
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m35.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-36
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m36.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-37
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m37.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-38
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m38.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-39
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m39.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-40
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m40.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-41
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m41.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-42
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m42.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-43
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m43.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-44
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m44.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-45
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m45.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-46
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-b46.bin
    )
    add_test(
        NAME fuzz-diff-r272-mined-47
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r272-m47.bin
    )
    add_test(
        NAME fuzz-diff-r272-classified
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-floor.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-floor2.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m00.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m01.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m02.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m03.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m04.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m05.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m06.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r272-m07.bin

    )
    add_test(
        NAME fuzz-diff-r261-nel-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-fold-r261.bin
    )

    # the r273 nightly escape (run 43611144511): the glued-? x tag soup
    # rides the yd_r273_mined_soup_gray arm EXACT-BYTES; the seed's
    # flow-qhash-nest-* prefix stays out of the classifier's positive
    # seed set and joins the nightly comparator-snapshot exclusion
    add_test(
        NAME fuzz-diff-r273-mined-00
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r273-m00.bin
    )

    # the r274 nightly escape (run 43621335462): the glued-? x break-only
    # quoted span rides the #1310 classifier's model extension (the
    # break-only span folds to ONE space at the close on both engines);
    # the flow-qhash-cr- prefix joins the classifier's positive seed set
    add_test(
        NAME fuzz-diff-r274-breakdq-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-cr-r274.bin
    )

    # the r275 nightly escape (run 43638904766): the indicator-? and
    # colon-led key slots ride the #1310 classifier's model extension
    # (a `?` directly followed by a flow indicator drops the `?` on
    # both engines; a `?`+break key slot closed by a `:`+blank is the
    # empty-key entry); the flow-qsoup- prefix joins the classifier's
    # positive seed set
    add_test(
        NAME fuzz-diff-r275-qflow-colon-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r275.bin
    )

    # the r277 nightly escape (run 43657665584): the MAP-steal members
    # ride the #1310 classifier's model extension (a `?`+blank explicit
    # key whose key-position scan hits a blank-preceded `#` comment with
    # zero separators inside the run: the post-break content is the key
    # slot, and a GLUED `:` leading it is plain content on yeptris while
    # libyaml's flow scan splits ANY colon as the VALUE token — the
    # 9-byte floor {?TAB#LF:#LF}); the whole-line comment run between a
    # member's plain tail and its separator rides inert on both engines.
    # The flow-qsoup- prefix joins the classifier's positive seed set.
    add_test(
        NAME fuzz-diff-r277-qmap-colon-steal
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r277.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r277-artifact.bin
    )

    # the r278 nightly escape (run 43675241311): the quoted-tail members
    # ride the #1310 classifier's model extension — inside a glued-`?`
    # member the quoted-span bytes ride yeptris's flat plain as content
    # (the plain fold law) while libyaml scans a real double-quoted
    # scalar whose break runs join per the leading_break/
    # trailing_breaks dance (a CR/LF/NEL-led run folds to ONE space
    # when it is the only break, else the leading break discards and
    # the rest join; an LS/PS-led run joins every break with LS/PS
    # raw — the 12-byte floor [?#<CR>"2<CR>",?m]); pure trailing
    # blanks stay UNKNOWN (libyaml joins them raw, yeptris trims).
    # The flow-qsoup- prefix joins the classifier's positive seed set.
    add_test(
        NAME fuzz-diff-r278-qtail-break-dq
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r278.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r278-artifact.bin
    )

    # the r279 nightly escape (run 43688038100): the plain-tail comment
    # members ride the #1310 classifier's model extension (law h) — a
    # blank-preceded `#` INSIDE a member's plain ends the plain on BOTH
    # engines (the run eats inertly to the break) and the member
    # boundary after the break must derive: a flow indicator (the
    # 16-byte floor {?#<SP>#<LF>,?#,*L<LF>,<}) or a `:`+blank/break
    # VALUE boundary (the artifact's first member: the glued-`?` plain
    # ends at the run, the colon re-lexes as the VALUE token with an
    # empty value; libyaml fires the KEY token for the flow `?`,
    # comment-eats the rest of the line, and pairs KEY/VALUE with
    # inserted empties). Content resuming after the run, a second
    # comment line, and eats to EOF stay UNKNOWN.
    # The flow-qsoup- prefix joins the classifier's positive seed set.
    add_test(
        NAME fuzz-diff-r279-qtail-comment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r279.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r279-artifact.bin
    )

    # the r280 nightly escape (run 43695970290): the UTF-16BE mid-stream
    # BOM composite rides the r144/r226/r243 front-end family's dump
    # relation, widened on two fronts the 5206-byte sibling (12 bytes
    # off the pinned r243 seed) exposed. (1) The fold residue is now
    # DERIVED from the input in unit space: after each mid-stream `feff`
    # the break run renders per the 1.2 plain-fold law (one break folds
    # ONE space, n>1 fold n-1 escaped newlines; boundary blanks drop) —
    # the 14-byte floor `feff feff 0085 feff 0085 2029 220a` rides the
    # second FEFF's NEL+PS two-break run (one "\n" escape libyaml
    # swallowed whole with the breaks). (2) The harness's 4096-byte line
    # cap truncates BOTH giant dumps at possibly DIFFERENT char phases,
    # so the residue window decodes as [one partial char tail][complete
    # chars][one partial char head] and EITHER fragment proves the
    # window is the cap cut, not extra content. Runs past the cap, runs
    # that eat to EOF, and composites past the FEFF table decline. The
    # A/B is FREE: the nightly ran on pre-widen main and its failure IS
    # the pre-widen abort. Comparator carry only — no engine change.
    # Seeds: utf16-midstream-bom-r280.bin (this pin) and the raw
    # artifact.
    add_test(
        NAME fuzz-diff-r280-utf16-midstream-bom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-midstream-bom-r280.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-midstream-bom-r280-artifact.bin
    )

    # the r359 nightly escape (run 45047942019): the UTF-16BE column-0
    # eaten-FEFF pair composed with a KEPT mid-scalar FEFF. The r243
    # relation stripped EVERY U+FEFF triple from yeptris's dump, but
    # libyaml's scan_to_next_token eat is a COLUMN-0 law: the eaten
    # sites are the mark-follower and the after-break FEFFs (any break
    # kind resets the column; the r226 byte probes), while the
    # mid-scalar FEFF (behind the 住-run content) rides BOTH engines as
    # plain content -- so libyaml's dump RETAINS its triple and the
    # all-sites strip mismatched. The 16-byte floor rides the same law:
    # `fe ff fe ff 0085 fe ff 0085 0085 4f4f fe ff` -- yeptris's scalar
    # opens at the kept mark-follower FEFF and folds the double-NEL to
    # one "\n" escape libyaml never sees (its scalar opens at the 住).
    # The walk now splits the sites: eaten sites strip triple+residue
    # (the r243 law unchanged), kept sites verify the triple verbatim
    # in libyaml's window and advance both. A kept site libyaml lacks
    # still declines. Comparator carry only -- no engine change.
    # Seeds: utf16-kept-mid-feff-r359.bin (this pin) and the raw
    # artifact.
    add_test(
        NAME fuzz-diff-r359-utf16-kept-mid-feff
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-kept-mid-feff-r359.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-kept-mid-feff-r359-artifact.bin
    )

    # the r281 nightly escape (run 43705128492): the glued-`?#` soup
    # composed with ONE under-modeled container shape — the nested flow
    # map. The 9-byte floor `{?#,:<LF>{}}` splits on the slot the
    # empty `{}` lands in: yeptris splits the `#,:` line at the
    # comment-eat (member 1 = glued-`?#` plain + empty; member 2 =
    # `:`+LF colon-led EMPTY key whose value IS the nested map), while
    # libyaml's scan_to_next_token comment-eats `#,:` from the token
    # start and the parser reaches the `{` as the explicit-`?` member's
    # KEY node. The classifier models the nesting recursively (law i):
    # the yeptris value walk renders +MAP/-MAP around the same member
    # laws, the `:`-led member derives the empty key, libyaml's
    # simple-key slots ride per flow level and its flow-mapping body
    # recurses at FLOW-MAPPING-START nodes. Maps only — a nested `[`,
    # member-start collections, and past-32 nesting stay UNKNOWN. The
    # member-start collections, and past-32 nesting stay UNKNOWN. The
    # 66-byte artifact composes the floor with the `% :{...}` value
    # split (its nested map carries six members against libyaml's
    # three empty-key pairs). Comparator carry only — no engine change.
    # Seeds: flow-qsoup-r281.bin (this floor) and the raw artifact.
    add_test(
        NAME fuzz-diff-r281-nested-map
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r281.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r281-artifact.bin
    )

    # the r282 nightly escape (run 43719730493): the BLOCK-NEL
    # more-indent surplus - a folded block scalar whose content NEL
    # (yeptris's 1.2 kept-content law) meets a space-only continuation
    # sub-line (libyaml's 1.1 keeps exactly the indent-surplus space
    # behind the escaped break; yeptris keeps all the spaces). Floor
    # 7 bytes "> LF SP < NEL SP SP"; the raw 24-byte artifact rides
    # the same arm. Comparator carry only - no engine change.
    # Seeds: block-nel-moreindent-r282.bin (this floor) and the raw
    # artifact.
    add_test(
        NAME fuzz-diff-r282-block-nel-moreindent
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-moreindent-r282.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-moreindent-r282-artifact.bin
    )

    # the r283 nightly escape (run 43737119210): the BLOCK-NEL
    # header-comment composition - the r282 shape riding a COMMENTED
    # block-scalar header ("> # PS :" - the header line's break is the
    # PS) plus two more documented sites past the NEL law: the LS/PS
    # raw-keep polarity (yeptris spells the empty-line break it
    # normalized to an escape; libyaml's READ_LINE keeps the raw
    # triple) and the dropped-fold-break surplus the NEL split
    # manufactures (libyaml's fold branch drops the gap's leading
    # break; yeptris's more-indented kept-content line keeps it
    # literal). Floor 12 bytes "> SP # PS SP : NEL SP SP"; the raw
    # 32-byte artifact rides the same arm. Comparator carry only -
    # no engine change.
    # Seeds: block-nel-header-ps-r283.bin (this floor) and the raw
    # artifact.
    add_test(
        NAME fuzz-diff-r283-block-nel-header-ps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-header-ps-r283.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-header-ps-r283-artifact.bin
    )

    # the r284 nightly escape (run 43745100701): the GLUED-?/ANCHOR flow
    # soup - SEQ members where yeptris's 1.2 reads the glued `?` as
    # plain-first content (`?5:&` stays one plain; `?/<PS>m` folds the PS
    # break to the fold space) while libyaml's 1.1 fires the KEY token on
    # EVERY flow `?` (the glued content rides the single-pair map's key,
    # value empty; libyaml's READ_LINE keeps the raw PS mid-plain), plus
    # the bare `&m` anchor member both engines render as the prop +
    # separator blank + empty value. Floor 16 bytes
    # "[?5:&,&m,?/<PS>m]"; the raw 18552-byte soup rides whole past the
    # raised budgets (~1400 members). Comparator carry only - no engine
    # change.
    # Seeds: flow-anchored-qsoup-r284.bin (this floor) and the raw
    # artifact.
    add_test(
        NAME fuzz-diff-r284-flow-anchored-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-anchored-qsoup-r284.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-anchored-qsoup-r284-artifact.bin
    )

    # the r285 nightly escape (run 43752700542): the CR-RUN/PS fold soup -
    # SEQ members where yeptris's 1.2 folds the CR-run + PS break runs
    # (5 consecutive breaks join as n-1 newlines) while libyaml's 1.1
    # READ_LINE keeps the raw PS mid-plain (it is NOT a fold break; the
    # raw bytes ride the join), plus the r284 glued-`?`/`?##` members and
    # the bare `&m` anchor member riding whole. The models derive the
    # 32772-byte artifact byte-exact with zero law changes - the only
    # decline was the conservatism budget (2324 members past the 2048
    # cap, 32772 bytes past the 20480 input cap). Comparator carry only -
    # no engine change, budgets 20480/2048 -> 40960/4096.
    # Seeds: flow-crps-qsoup-r285.bin (the greedy floor at the input
    # budget boundary, 20484 bytes) and the raw artifact.
    add_test(
        NAME fuzz-diff-r285-flow-crps-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-crps-qsoup-r285.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-crps-qsoup-r285-artifact.bin
    )

    # the r286 nightly escape (run 43760525981): the SPLIT-? COLON-IND
    # flow soup - members where the split `?` (blank follows) opens the
    # explicit key and the key slot's `:` rides the 1.2 flow
    # VALUE-boundary law (`:` + flow indicator = the VALUE boundary,
    # `?<TAB>:,` reads the empty key and the empty value on BOTH
    # engines) while the classifier's r275 arm only modeled `:` +
    # blank/break and declined the colon-ind twin, leaving the 95-byte
    # soup unexplained. The widening is yeptris-model-side only; the
    # libyaml model already derived the soup (KEY, VALUE, FLOW-ENTRY
    # token dispatch), and the GLUED `?:,` twin stays red (libyaml fires
    # KEY on the glued `?` and reads `:,` mid-plain, yeptris keeps `?`
    # plain-first - probed DIFFER, out of this arm). Comparator carry
    # only - no engine change. (The seeds ride the `flow-qsoup*`
    # comparator-snapshot exclusion - the family is dense.)
    # Seeds: flow-qsoup-qcolon-r286.bin (the greedy byte-law floor, 16
    # bytes: `{?#,*%,?<TAB>:,?<LF>,<}` - the glued-?# plain, the bare
    # alias, the r286 colon-ind member, a split-? pair, the `<` keyless
    # member) and the raw artifact.
    add_test(
        NAME fuzz-diff-r286-flow-qsoup-qcolon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-qcolon-r286.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-qcolon-r286-artifact.bin
    )

    # the r287 nightly escape (run 43769925521): the BLANK-LED DQ SPAN
    # flow soup - members where a double-quoted span opens with a CR
    # break followed by a blank run (the artifact's `"<CR>` + 34 SP +
    # `g"`): both engines fold the break to ONE space and strip the
    # continuation blanks (` g` byte-exact on both sides) while the
    # classifier's twin gates declined any blank-led span head, leaving
    # the 58-byte soup unexplained. The widening is twin-model-side (the
    # yeptris walk's fc_quoted_value first-content gate and the libyaml
    # walk's l_dq_scan first gate admit the blank run after a SINGLE
    # leading CR/LF/NEL break; multi-break leads and raw LS/PS-led
    # heads stay declined). Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-dqblankled-r287.bin (the constrained ddmin floor,
    # 21 bytes: `[1,?#,"<CR>g",?m?,"<CR><SP>g"]` - the plain lead, the
    # glued-?# member eaten by libyaml's comment run, the glued-? pair,
    # the blank-led DQ member) and the raw artifact.
    add_test(
        NAME fuzz-diff-r287-flow-qsoup-dqblankled
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqblankled-r287.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqblankled-r287-artifact.bin
    )

    # the r288 nightly escape (run 43778032929): the BLOCK-NEL
    # fold-space surplus - inside the r282/r283 family the fold-adjacent
    # space diverges both ways (yeptris's fold eats it with the spelled
    # break, libyaml's keeps it past the dropped break; the NEL-site
    # space-run delta widens past one where the sub-line is blank-ish).
    # Floor 27 bytes (commented PS-led header, two NEL lines, the "2 t"
    # tail); the raw 33-byte artifact rides the same arm. Comparator
    # carry only - no engine change.
    add_test(
        NAME fuzz-diff-r288-block-nel-foldspace
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-foldspace-r288.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-foldspace-r288-artifact.bin
    )

    # the r289 nightly escape (run 43809383047): the TAB BLANK-RUN
    # dump-escape repair - a plain member whose INTERIOR blank run
    # carries a TAB byte (the floor's `,1<TAB>.` member; the artifact
    # rides TABs at 57 sites). Both engines agree byte-exact (the TAB
    # rides the libyaml plain; the actual dump escapes it as `\t`) but
    # the libyaml model's interior-blank join wrote the run RAW while
    # its l_dq_scan twin and the yeptris plain walk already escaped -
    # SP-only runs are escape-neutral, so the repair un-classifies
    # nothing. Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-tabblank-r289.bin (the greedy floor, 97 bytes:
    # the split-? plain members, the glued-?# comment-eat members, the
    # CR/PS break soup, and the TAB-bearing `1<TAB>.` member) and the
    # raw artifact.
    add_test(
        NAME fuzz-diff-r289-flow-qsoup-tabblank
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tabblank-r289.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tabblank-r289-artifact.bin
    )

    # the r290 nightly escape (run 43819857599): the BARE-NEL site -
    # inside the r282/r283/r288 arm the NEL-split sub-line is fully
    # blank (the NEL rides directly against the next break), so BOTH
    # space runs at the site are empty (the r288 lower-bound-of-one
    # space gate declined) and libyaml escapes one MORE break than
    # yeptris keeps NELs (the surplus LF ride pairs byte-wise behind
    # the k-vs-k NEL pairing); the floor's twin carries libyaml's
    # kept-raw PS where yeptris folded the consecutive-break pair
    # away. Reproduces on Debug AND Release pre-fix. Comparator
    # carry only - no engine change.
    # Seeds: block-nel-barenel-r290.bin (the greedy floor, 17 bytes:
    # the commented PS-led header, the bare NEL sub-line, the PS-led
    # " :" line and the trailing NEL) and the raw 31-byte artifact.
    add_test(
        NAME fuzz-diff-r290-block-nel-barenel
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-barenel-r290.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-barenel-r290-artifact.bin
    )

    # the r291 nightly escape (run 43830356926): the POST-BREAK VALUE
    # boundary - a plain member whose blank/break run is followed by
    # `:` + a flow indicator (the floor's "` SP TAB LF `:,`" member;
    # the artifact rides it after the second backtick). yeptris ends
    # the plain at the run and re-lexes the colon as the 1.2 flow
    # VALUE boundary (the r286 law at the post-break continuation
    # position, pair (plain, empty)) while the model's piece walk hit
    # the glued-`:`+flow-ind gate. libyaml can only see these bytes
    # when its comment-eat swallows the whole member - every
    # libyaml-visible instance is YEPTRIS-ONLY (its still-open plain
    # hits the "unexpected ':'" scanner error), so the glued
    # no-break, SP-only-run, first-piece, and member-end positions all
    # stay declined. Reproduces on Debug AND Release pre-fix.
    # Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-postbreak-qcolon-r291.bin (the 24-byte floor,
    # the family-consult-constrained fixpoint: the plain member, the
    # glued-?# comment-eat member, the alias member, and the
    # post-break colon member) and the raw 66-byte artifact (both
    # byte-scale walks fixpointed at it).
    add_test(
        NAME fuzz-diff-r291-flow-qsoup-postbreak-qcolon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-postbreak-qcolon-r291.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-postbreak-qcolon-r291-artifact.bin
    )

    # the r292 nightly escape (run 43845785461): the TAG shorthand in
    # the glued-`?` KEY slot - the libyaml model never tokenized `!`,
    # so the family classifier declined every tag-bearing member.
    # yeptris reads the glued-`?` member as ONE flat plain (the `!`,
    # `(` and `%` bytes ride plain content; the r272 plain-? law) while
    # libyaml scans the shorthand tag (scan_tag: the alpha-run head
    # rides into the suffix minus the leading `!`, %XX octet escapes
    # decode per the UTF-8 leading/trailing-octet discipline, only `!`
    # and `!!` are default-resolvable handles, and the tag may end only
    # at a blank, a break, `,` in flow, or EOF) and pairs the tagged
    # EMPTY scalar as the single-pair wrapper's key - the artifact's
    # `!(4%21bz!` renders URI-unescaped as `!(4!bz!` while the bare `!`
    # member's tag drops (the non-specific tag, "resolves by style").
    # The model now tokenizes LT_TAG (the resolved event tag rides the
    # token) and attaches it in parse_node's properties walk; the
    # verbatim `!<`, the `!h!`/`!!` handles, the invalid escapes and
    # the non-`,` closers all stay declined. The yeptris model is
    # untouched (a member LED by `!` still declines - only the glued
    # position rides). Reproduces on Debug AND Release pre-fix.
    # Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-r292.bin (the 7-byte greedy floor `[?!x,?]`:
    # the tag-bearing glued-`?` member + the r275 indicator-? member)
    # and the raw 20-byte artifact.
    add_test(
        NAME fuzz-diff-r292-flow-qsoup-tag
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r292.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-artifact-r292.bin
    )

    # the r293 nightly escape (run 43867368224): the 49677-byte
    # CR-run/PS fold soup OUTGREW the r285-raised budgets - the family
    # classifier declined at the budget gate (law n) before any
    # derivation (the artifact exceeds FC_MAX_INPUT 40960, and its
    # libyaml-token ride sits between 8193 and 9216, past
    # FC_MAX_TOKENS 8192). The classifier's laws derive both dumps
    # byte-exact with the caps alone raised (the r285 "rides past
    # raised budgets" shape one size class later); the model is
    # untouched, the firewalled decline surface is byte-identical.
    # The `!`-byte plains ride MID-PLAIN on both engines (indicators
    # matter only at token start) - the r292 tag model is not
    # implicated. Reproduces on Debug AND Release pre-fix.
    # Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-r293.bin (the 43995-byte member-boundary prefix
    # floor - over the old input cap, under the new one) and the raw
    # 49677-byte artifact (which additionally rides past the token cap).
    add_test(
        NAME fuzz-diff-r293-flow-qsoup-crps-budget
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r293.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r293-artifact.bin
    )

    # the r294 nightly escape (run 43884852403): the ESC-LESS NEL fold
    # site - the `>`-header + NEL/PS composition's sixth member. At the
    # site yeptris keeps the raw NEL as 1.2 content (the r202 raw
    # carrier) with the space riding verbatim behind it, while
    # libyaml's fold branch turned the break into the ONE space its
    # plain fold spells ("?<NEL> SP 0" reads "? SP 0", the
    # break-leading blank eaten with the fold) - no escape at all on
    # libyaml's side, so the NEL-site gate's esc pairing had nothing
    # to pair against and declined unconditionally. The widened arm
    # pairs the one NEL to the one fold space behind the same
    # r283/r288/r290 gates (r==0, k==1, the alignment byte behind the
    # fold coincides with the first byte behind yeptris's NEL+space
    # run and sits on plain content); every prior member keeps its
    # exact path (the sub-case sits behind the r290 `r < k` decline,
    # which held for all of them). Reproduces on Debug AND Release
    # pre-fix.
    # Comparator carry only - no engine change.
    # Seeds: block-nel-escless-fold-r294.bin (the 25-byte DIFFER- and
    # shape-constrained floor `> SP # PS SP : LF SP SP NEL SP 0 LF PS
    # SP ? NEL SP 0` - the r283/r288 template's first 16 bytes then
    # the `?`-content NEL site) and the raw 62-byte artifact (whose
    # `?g:yaml.org,2##|#|+`,?!\t--x` soup and `  t` tail ride the same
    # laws and are not load-bearing).
    add_test(
        NAME fuzz-diff-r294-block-nel-escless-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-escless-fold-r294.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-escless-fold-r294-artifact.bin
    )

    # the r295 nightly escape (run 43912266935): the 81936-byte CR-run/PS
    # fold soup outgrew the r293-raised budgets AND carried a shape the
    # yeptris model declined outright - a `, #` COMMENT MEMBER (a `#`
    # after a blank at the member loop-top). Both engines eat the comment
    # to the break between members (probes `, #c` LF, `[a, #c` LF,
    # `[a,\n #c` LF are byte-identical), so the model gains the loop-top
    # comment ride (the tail gate's own law, moved between members;
    # gated on the preceding blank/break - the `,`-glued or `[`-glued `#`
    # is the documented yeptris-rejects/libyaml-accepts split, never a
    # comment) plus the budget raises: FC_MAX_INPUT 65536 -> 131072 (the
    # r285/r293 "outgrew the budget" shape one size class later) and
    # FC_MAX_MEMBERS 4096 -> 8192 (the soup rides 5906 members; the
    # 13935-token ride stays under FC_MAX_TOKENS 16384). The isolation
    # matrix: the raises alone decline at the comment gate (pos 38108);
    # the comment ride alone declines at the members gate (pos 56835);
    # the full change classifies both dumps byte-exact. Reproduces on
    # Debug AND Release pre-fix.
    # Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-r295.bin (the 65650-byte member-boundary prefix
    # floor - over the old input cap, under the new one, carrying the
    # comment site) and the raw 81936-byte artifact (which additionally
    # rides past the members cap).
    add_test(
        NAME fuzz-diff-r295-flow-qsoup-comment-budget
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r295.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-r295-artifact.bin
    )

    # the r296 nightly escape (run 43940004067): the 21-byte artifact
    # `\n [?#,"\rg",?m?,"\n"] \n` - a LEADING blank/break run before the
    # root bracket. The classifier's law-j gate read data[0] strictly, so
    # the whole family declined before any derivation; the artifact minus
    # its two lead bytes already classified PRE (the no-lead twin rides
    # the old gate byte-exact), which isolates the lead as the only
    # missing piece. Both engines scan-to-token before the root token
    # (blanks and breaks, every kind - the r285 CR/PS law's own
    # mechanics), so the gate and the yeptris model's root walk gain the
    # lead run; any other lead byte (a comment, a TAB - libyaml does not
    # eat a stream-start tab in block context, a directive, a document
    # marker) still declines byte-exact. Reproduces on Debug AND Release
    # pre-fix (nightly config is Debug).
    # Comparator carry only - no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion -
    # the family is dense.)
    # Seeds: flow-qsoup-leadrun-r296.bin (the 14-byte DIFFER-constrained
    # floor `\n[?#,"\rg",?m?]` - the lead-bearing glued-`?#` KEY-eat head,
    # the raw-CR DQ fold, the glued-`?m?` plain) and the raw 21-byte
    # artifact (which additionally carries the closing `"\n"]` fold and
    # the trailing run).
    add_test(
        NAME fuzz-diff-r296-flow-qsoup-leadrun
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-leadrun-r296.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-leadrun-r296-artifact.bin
    )

    # the r297 nightly escape (run 43952064060): the 42-byte artifact
    # `> SP #<PS 700: LF SP #<PS 700: LF SP SP m<NEL> SP<NEL> SP
    # m<NEL> SP<NEL> SP t` - a folded block scalar whose SECOND content
    # line is COMMENT-LED with a mid-line raw PS. The engine behaviors
    # are the documented ones (A/B at the pre-arm parent f13e6553
    # aborts identically; zero src/ divergence since r248), but the
    # composition is fresh: at the mid-content `#`-first PS line
    # yeptris's comment-eat ends AT the mb-break and the remainder
    # rescans as a fresh fold line (the boundary folds to the ONE
    # space), while libyaml breaks at the PS mid-line, READ_LINE keeps
    # the raw triple in leading_break, and the raw lead (NE LF) fails
    # the fold gate - no fold space, the triple rides verbatim (the
    # r197/r290 polarity's ESC-LESS mirror: yeptris spells a SPACE
    # where every prior law wants an escape). The tail's second
    # kept-NEL pair is the spent-NEL site: libyaml's fold branch
    # dropped the leading break (trailing_breaks non-empty clears it)
    # and its breaks-scan ate the spaces, so yeptris's NEL+space has
    # no counterpart before the coinciding plain byte. The widened arm
    # (the r282/r283/r288/r290/r294 lineage) gains both site laws,
    # gated on the coinciding plain alignment byte and the byte-bound
    # continuation; every prior member keeps its exact path (both laws
    # sit behind declines that held for all of them - the r290
    # kept-raw gate demands a[0] == b[3] non-space; the r294 `r < k`
    # decline owned the tail unconditionally). Reproduces on Debug AND
    # Release pre-fix (nightly config is Debug).
    # Comparator carry only - no engine change.
    # (The seeds ride the `block-ps-nel-fold-*` comparator-snapshot
    # exclusion - the r271 family's own glob.)
    # Seeds: block-ps-nel-fold-r297.bin (the 25-byte DIFFER- and
    # shape-constrained floor `> SP # PS SP # PS SP NEL NEL SP m NEL SP
    # NEL SP t` - the PS-fold site with the NEL-lead alignment and the
    # spent-NEL tail) and the raw 42-byte artifact (whose repeated
    # `#<PS 700:` fold line and `700:`/` m` regions ride the same laws
    # with the plain-byte alignment).
    add_test(
        NAME fuzz-diff-r297-block-nel-psfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ps-nel-fold-r297.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ps-nel-fold-r297-artifact.bin
    )

    # the r298 nightly escape (run 43969459612): the UTF-16BE composite
    # rides the mid-content LS/PS raw-join site, a ONE-BYTE sibling of
    # the pinned r280 floor — the floor's second `0085` (NEL) became
    # `4185` (U+4185 content), so the PS that followed it now folds
    # MID-CONTENT: yeptris's plain scalar folds the single break to ONE
    # space (the 1.2 plain-fold law) while libyaml's READ_LINE joins
    # the raw triple into leading_break and its fold gate wants an LF
    # lead, so the triple rides the value verbatim (scanner.c
    # LS|PS -> LS|PS; the r56 plain law). Every plain-fold arm's data
    # gate scans the UTF-8 `e2 80 a8/a9` triples — blind to the
    # UTF-16BE `20 28/20 29` code units — and the FEFF-strip walk owns
    # the dump head, so the law rides the r144/r226/r243/r280 family
    # arm: a unit-space site pass counts the mid-content single LS/PS
    # breaks (both flanking units content — a flanking blank or break
    # belongs to the derivation above or declines) and the walk bounds
    # yeptris's ONE fold space against libyaml's ONE raw triple by that
    # count (composites past the cap decline). The 14-byte artifact is
    # floor-minimal (every unit load-bearing; the plain ddmin walk
    # cannot shed the BOM, either FEFF, the NEL residue run, or either
    # site flank byte). A/B at the r243-arm parent 0de5db23^ (Release):
    # RC 77 with a byte-identical dump pair — the divergence predates
    # the whole arm lineage, a fresh composition of the r144 BOM law,
    # the r56 plain LS/PS law, and the r280 residue law. Reproduces on
    # Debug AND Release (nightly config is Debug).
    # Comparator carry only — no engine change. Seeds: the 14-byte
    # floor artifact (this pin; the seed joins the comparator-snapshot
    # exclusion via the new utf16-midstream-bom-* glob — the family's
    # third arm-model gap, all one-byte mutations of the same seed).
    add_test(
        NAME fuzz-diff-r298-utf16-midcontent-lsps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-midstream-bom-r298.bin
    )

    # the r299 nightly escape (run 43983729736): the 55-byte artifact
    # `> SP #<11 CR<PS> 70<PS> 70<PS> 700: LF SP<PS> 700: LF SP SP
    # m<NEL> SP t0: LF SP SP m<NEL> SP t` - a folded block scalar whose
    # header comment ends at a CR break (the r283 header gate's
    # break-terminated-comment case) and whose PLAIN content lines
    # carry mid-line raw PS breaks with content on BOTH flanks. The
    # engine behaviors are the documented ones (A/B at the pre-#1411
    # parent 9d715344 aborts identically with a byte-identical dump
    # pair - the divergence predates the engine fix; zero src/
    # divergence), but the composition is fresh: at the mid-content
    # plain PS site BOTH engines keep the raw triple (libyaml's block
    # reader breaks at the PS mid-line, READ_LINE keeps it in
    # leading_break, the raw lead fails the fold gate), but yeptris's
    # block fold writes the ONE fold space on EACH FLANK of the triple
    # it also keeps (twin spaces) where libyaml carries none - the r290
    # kept-raw surplus demands a[0] == b[3] non-space and the r297
    # PS-fold site wants a WITHOUT its own triple, so the twin-flanked
    # sites fell through to the unconditional decline. The widened arm
    # (the r282/r283/r288/r290/r294/r297 lineage) gains the two
    # twin-space site laws: the LEAD flank (eat the ONE surplus space
    # when a carries the SAME raw triple b leads - the triples pair
    # byte-wise behind it) and the TRAIL flank (eat the ONE surplus
    # space behind a byte-paired raw triple, back-reference-bound,
    # gated on the coinciding plain alignment byte and the byte-bound
    # continuation). The tail rides the NEL site and the r176 clip as
    # before; every prior member keeps its exact path (both laws sit
    # behind declines that held for all of them). Reproduces on Debug
    # AND Release pre-fix (nightly config is Debug).
    # Comparator carry only - no engine change.
    # (The seeds ride the `block-ps-nel-fold-*` comparator-snapshot
    # exclusion - the r271 family's own glob.)
    # Seeds: block-ps-nel-fold-r299.bin (the 17-byte DIFFER- and
    # shape-constrained floor `> SP #<PS SP : LF SP<PS> SP m<NEL>` -
    # the twin-lead site, the byte-paired triple's twin-trail site, the
    # kept-NEL site, and the r176 clip) and the raw 55-byte artifact
    # (whose three plain twin sites and the `  m`/` t0:` tail ride the
    # same laws with the r197 leading-empty-line pairing).
    add_test(
        NAME fuzz-diff-r299-block-nel-twinspace
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ps-nel-fold-r299.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-ps-nel-fold-r299-artifact.bin
    )

    # the r300 nightly escape (run 43997338071): the 14-byte artifact
    # `BOM<NEL><FEFF><NEL><FEFF><NEL>'-'` in UTF-16BE code units -- a
    # ONE-UNIT sibling of the pinned r226 seed (bom-utf16-nel-line-start),
    # its final `0061` ('a') became `002d` ('-'). The engine behaviors
    # are the documented ones (A/B at the pre-#1411 parent 9d715344
    # aborts identically with a byte-identical dump pair -- the
    # divergence predates the engine fix; zero src/ divergence), but
    # the composition is fresh: behind the all-FEFF/break prefix the
    # trailing `-` indicator flips libyaml's scanner to the STRUCTURAL
    # level -- its reader eats every mid-stream FEFF (the r144 law), so
    # it sees a bare break run then the `-` at column 0 and opens an
    # implicit block SEQUENCE with the empty scalar, while yeptris
    # keeps the FEFFs as content, folds the NEL runs into the derived
    # residues, and rides the `-` as interior plain text. The scalar-
    # content walk is blind to the wrapper, so the shape fell through
    # to the unconditional decline. The widened arm (the
    # r144/r226/r243/r280/r298 lineage) gains the structural-eat site
    # law: unit-space tight (even size, the trailing `00 2D` unit at
    # EOF, only FEFF/break units between the BOM and the indicator),
    # libyaml's dump must be the empty-SEQ wrapper (the `003f` '?'
    # twin's implicit-MAP wrapper rides the same law), yeptris's the
    # flat scalar, and the FEFF/residue strip over yeptris's value
    # leaves the eaten indicator alone (composites -- a second
    # indicator, any content unit, a wrapper with extra entries --
    # decline). The 14-byte artifact is floor-minimal (every unit
    # load-bearing; the DIFFER-constrained ddmin walk cannot shed the
    # BOM, either FEFF, any NEL, or the indicator). A/B at 9d715344
    # (Release): RC 134 with a byte-identical dump pair. Reproduces on
    # Debug AND Release (nightly config is Debug).
    # Comparator carry only -- no engine change.
    # (The seed rides the `utf16-midstream-bom-*` comparator-snapshot
    # exclusion -- the family's own glob.)
    # Seed: utf16-midstream-bom-r300.bin (the 14-byte artifact, which
    # is also the floor).
    add_test(
        NAME fuzz-diff-r300-utf16-structural-eat
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-midstream-bom-r300.bin
    )

    # the r301 nightly escape (run 44006575967): the 70-byte UTF-8 flow
    # map `{?#?#|#|+` <sp><TAB><sp><LF><TAB> ,?##|#|,` ,?<TAB>*%L<TAB><TAB>
    # <sp> ,?#|#|,` <LF>:<TAB> ,?---<TAB>:#|#|+` <LF> ,<*<LF><TAB><sp> }`
    # -- the glued-`?`/#/breaks flow-soup family (the #1310 classifier's
    # own laws a-e compose here: the KEY-adjacent `#` comment-eat, the
    # glued-`?#` plain, the fold-point colon, the plain-`?`), but the
    # split explicit-key slot's ALIAS (`?<TAB>*%L` -- `?`+blank splits
    # the key, the alias rides the key slot) carried a blank/break RUN
    # between the alias name and the member's separator. yeptris parses
    # the member fine (`=ALI *%L` + the parser's empty value scalar);
    # the classifier's split-alias path only accepted the name glued
    # straight to its boundary and declined the whole input (the
    # libyaml model already derived its side byte-exact: libyaml's
    # comment-eat from the `#` glued to the FIRST flow `?` KEY swallows
    # the separator AND the alias member to EOL, so libyaml renders
    # (empty,empty) pairs and never sees the alias at all). The widening
    # (law c): the split-slot alias rides the same fc_skip_blank_break
    # walk to the separator/closer the bare-alias member walks; a ':'
    # across the run (a VALUE boundary the probes never cover) and any
    # other byte still decline. Firewall counts byte-identical pre/post
    # (89 seeds / 4770 tamper / 564 mutants before the seeds ride).
    # Reproduces on the nightly-mirroring Debug build AND on Release
    # (standalone abort RC 134 both, pre-patch). A/B at the pre-#1411
    # parent 9d715344: identical abort with a byte-identical dump pair
    # -- not an engine regression (the divergence is classifier-scope
    # comparator law, zero src/ change).
    # Comparator carry only -- no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion --
    # the family's own glob.)
    # Seeds: flow-qsoup-aliasblank-r301.bin (the 15-byte
    # DIFFER-constrained ddmin floor `{?#,? *L ,`<LF>,<>}` -- every byte
    # load-bearing; the artifact's TAB before the alias folds to SP in
    # the floor and rides), flow-qsoup-aliasblank-r301-artifact.bin (the
    # 70-byte raw artifact).
    add_test(
        NAME fuzz-diff-r301-flow-qsoup-aliasblank
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-aliasblank-r301.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-aliasblank-r301-artifact.bin
    )

    # the r302 nightly escape (run 44034382404): the 67-byte UTF-8 flow
    # map `{?#?#|#|+` <sp><TAB><sp><LF><TAB> ,?##|#|,` <TAB>?,L:<TAB>*%<sp>
    # ,?#|#|,` <LF>z <TAB> ,?<TAB>:#|#J+` <LF> ,<<LF><TAB><sp> }` -- the
    # glued-`?`/#/breaks flow-soup family (the #1310 classifier's own
    # laws a-e compose here), but the ALIAS at a `:`-boundary VALUE slot
    # (`?#:<TAB>*%` -- the fold-colon value followed by the TAB-riding
    # alias `*%`; the plain-key twin `L:<TAB>*%<sp>` rides the same slot)
    # was the documented decline ("an alias value slot: untested").
    # yeptris parses the member fine (`=ALI *%` in the value position --
    # the engine's e_alias prop-char name cut takes the non-alpha `%`);
    # libyaml's comment-eat from the `#` glued to the member's `?`
    # swallows the whole member to EOL, so libyaml never sees the alias
    # and its side already derived byte-exact. The widening (law c): the
    # alias value rides the engine's name cut verbatim with the same
    # blank/break run to the terminator the bare-alias member and the
    # r301 split-key slot take; a ':' VALUE boundary after the alias and
    # any other byte still decline. Firewall counts byte-identical
    # pre/post (89 seeds / 4770 tamper / 564 mutants before the seeds
    # ride; 91/4875/580 with them riding -- the r301 post-seed counts).
    # Reproduces on the nightly-mirroring Debug build (standalone abort
    # RC 134 pre-patch). A/B at the pre-#1411 parent 9d715344 (Debug):
    # identical abort with a BYTE-IDENTICAL dump pair (diff-verified) --
    # not an engine regression (the divergence is classifier-scope
    # comparator law, zero src/ change). The 13-byte DIFFER-constrained
    # ddmin floor `{?,?#:<TAB>*%,<LF><>}` is minimality-checked under
    # the libFuzzer oracle (every single-byte deletion stops the
    # true-DIFFER; single-byte splits of the floor ride the ledger).
    # Comparator carry only -- no engine change.
    # (The seeds ride the `flow-qsoup*` comparator-snapshot exclusion --
    # the family's own glob.)
    # Seeds: flow-qsoup-valalias-r302.bin (the 13-byte floor) and
    # flow-qsoup-valalias-r302-artifact.bin (the 67-byte raw artifact).
    add_test(
        NAME fuzz-diff-r302-flow-qsoup-valalias
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-valalias-r302.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-valalias-r302-artifact.bin
    )

    # the r303 nightly escape (run 44046151152): the 32-byte artifact
    # `> SP #<PS> SP : LF SP SP<NEL> SP 0 LF<PS> SP : LF SP SP<NEL> SP 0
    # LF SP<NEL>` - a folded block scalar whose header comment ends at a
    # PS break (the r283 header gate's comment-terminated case) whose
    # MORE-INDENTED content lines carry mid-line raw NELs (the kept-NEL
    # sites) and whose SECOND content line is PS-LED AT COLUMN ZERO (an
    # empty line - the PS is the line's whole content), ending on a
    # blank-led final line whose NEL sits at END OF INPUT. The engine
    # behaviors are the documented ones (A/B at the pre-#1411 parent
    # 9d715344 aborts identically with a BYTE-IDENTICAL dump pair - the
    # divergence predates the engine fix; zero src/ divergence since
    # r248), but the composition is fresh. The value pairs through the
    # standing laws - the NEL-site pairings (kept raw C2 85 vs the one
    # escape), the surplus-space rides (libyaml's breaks-scan eats the
    # NEL-adjacent spaces), the r197 polarity (yeptris's spelled escape
    # vs libyaml's raw PS triple at the column-zero empty line - the
    # PS-led line's break rides normalized on yeptris, raw-tripled on
    # libyaml with its leading break spent), and the r283 surplus
    # escape - but the TAIL is new: the final line's LF rides LITERAL
    # on yeptris (the blank-led line suppresses the fold), its NEL rides
    # RAW as 1.2 content and the clip break spells the ONE escape behind
    # it ("0 \n<NEL>\n"), while libyaml's trailing-breaks join collapsed
    # the whole tail - the line LF, the blank-led spaces, the EOF NEL -
    # into the ONE escape the byte-match already paired. After the byte
    # match spends b's last escape on a's line LF, a's remainder is
    # exactly the kept NEL plus the clip escape ahead of the byte-bound
    # frame suffix, and every law declined unconditionally there (the
    # r297 spent-NEL gate wants b on plain content; the r176 tail owns
    # the pure-escape surplus). The widened arm (the r282/r283/r288/
    # r290/r294/r297/r299 lineage) gains the ONE EOF-tail kept-NEL site
    # law: a's surplus must be exactly the kept-NEL pair followed by
    # the ONE escape with the remainders behind the claim coinciding
    # completely (the r161 strcmp law), gated on subs > 0 (at least one
    # documented law rode behind). Every prior member keeps its exact
    # path (all 296 pinned seeds classify pre/post on Debug AND
    # Release). The 25-byte `:<NEL><NEL>` tail sibling still declines -
    # at the r290 cap's r < k site (two kept NELs vs one escape), a
    # deliberately un-widened shape. Reproduces on the nightly-mirroring
    # Debug build AND on Release (standalone abort RC 134 pre-patch on
    # both; the nightly config is Debug).
    # Comparator carry only - no engine change.
    # (The seed rides the comparator corpus like the r282/r290/r294
    # block-nel seeds - no snapshot exclusion, no workflow edit.)
    # Seed: block-nel-eofkeep-r303.bin - the artifact is its own floor:
    # 32/32 bytes load-bearing under the dump-pair-preserving walk, the
    # family-signature walk, AND the same-site-shape walk (every
    # single-byte deletion breaks the shape; the unconstrained walk
    # leaves the family at 1 byte).
    add_test(
        NAME fuzz-diff-r303-block-nel-eofkeep
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-eofkeep-r303.bin
    )

    # the r304 nightly escape (run 44091514439): the 79-byte artifact
    # `> SP #<PS> SP : LF SP SP<NEL> SP 0 LF<PS> SP : LF SP f*47 SP<NEL>
    # SP 0 LF SP<NEL>` - the r303 block-NEL family's head (the folded
    # scalar whose header comment ends at a PS break, whose more-indented
    # content lines carry mid-line raw NELs, whose third line is PS-led
    # at column zero) with the final content line PLAIN-LED (" f*47<NEL>
    # 0" - the r303 floor's line was NEL-led) and the same blank-led
    # final line whose NEL sits at END OF INPUT. The plain lead does not
    # suppress yeptris's fold: the line break folds to the ONE space, so
    # yeptris's tail reads "0 SP<NEL>\n" (fold space, raw 1.2-content
    # NEL, clip escape) while libyaml's trailing-breaks join still
    # collapses the whole tail - the line LF, the final line's spaces,
    # the EOF NEL - into the ONE escape that stays PENDING on b (the
    # r303 floor's tail byte-matched its twin against a's literal line
    # LF; the fold space here blocks that match, so the r303 law's
    # strcmp never sees equal remainders). The engine behaviors are the
    # documented ones (A/B at the pre-#1411 parent 9d715344 aborts
    # identically with a BYTE-IDENTICAL dump pair - the divergence
    # predates the engine fix; zero src/ divergence since r248), and
    # every pinned floor rides: all 448 corpus seeds classify pre/post
    # (the 26-seed block-NEL family included), the classifier firewall
    # counts byte-identical to the r303 post-seed gate. The widened arm
    # gains the ONE EOF-tail fold-space kept-NEL site law: a's surplus
    # must be exactly the ONE fold space + the kept-NEL pair, b must
    # still hold the ONE pending escape, the clip escapes pair, and the
    # remainders behind the claim coincide completely (the r161 strcmp
    # law - the claim can never swallow divergence past the value). The
    # r299 twin sites own the space lead with a raw LS/PS triple behind
    # (a[1] == 0xE2), disjoint from this gate's a[1] == 0xC2; previously
    # the shape declined unconditionally, so no existing member's path
    # changes. Reproduces on the nightly-mirroring Debug build AND on
    # Release (standalone abort RC 134 pre-patch on both; the nightly
    # config is Debug).
    # Comparator carry only - no engine change.
    # (The seeds ride the comparator corpus like the r282/r290/r294/
    # r303 block-nel seeds - no snapshot exclusion, no workflow edit.)
    # Seeds: block-nel-eofkeep-foldspace-r304.bin (the 33-byte ddmin
    # floor "> SP #<PS> SP : LF SP SP<NEL> SP 0 LF<PS> SP : LF SP f SP
    # <NEL> SP 0 LF SP<NEL>"; every single-byte deletion stops the
    # DIFFER) and block-nel-eofkeep-foldspace-r304-artifact.bin (the
    # 79-byte raw artifact - same dump pair modulo the f-run).
    add_test(
        NAME fuzz-diff-r304-block-nel-eofkeep-foldspace
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-eofkeep-foldspace-r304.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-nel-eofkeep-foldspace-r304-artifact.bin
    )

    # the r305 nightly escape (run 44110729107): the 104-byte artifact
    # `&<8*60>:<8*18><7*15>6a : TAB > LF *a:` - the anchor-name charset
    # family's head (the round-69/round-54 law: the suite keeps every
    # ns-anchor-char INSIDE the name - 2SXE "&a: key" anchors 'a:' and
    # names the tail alias 'a:'; the ledger carries 2SXE/W5VH as
    # suite-VALID, libyaml rejects - while libyaml's IS_ALPHA charset
    # truncates at the ':' and re-lexes the residual) COMPOSED with a
    # TAB-separated folded '>' value slot and an alias-as-key tail at
    # column zero. The arm's transform split the '&' half but the '*'
    # twin never fired: the riding name's residual ends in a KEY colon
    # (':' + TAB), and the residual walk's key-colon law cleared
    # plain_open2/node_pos2 without the gate's coll_col twin - so
    # coll_col2 stayed -1, the next line at column 0 rode the phantom
    # plain ("a root-level plain swallows every line"), and the copy
    # kept the riding alias name; the copy-agrees verify leg then
    # declined the arm's own family (measured pre-fix: nsplits=1,
    # amps=2 - the gate counted two splits, the transform performed
    # one). The engine behaviors are the documented ones (the ledger's
    # 2SXE/W5VH entries; libfyaml at the pinned tip rejects the artifact
    # outright on its strict missing-document-start law, so no engine
    # divergence is claimed - zero src/ divergence since r248), and
    # every pinned floor rides: all corpus seeds classify pre/post, the
    # #1310 soup firewall is untouched (the seeds are not soup-named),
    # the 50-mutant paired battery flips 9 pre-fix family aborts to 0
    # post-fix with zero new DIFFERs. Reproduces on the nightly-
    # mirroring Debug build AND on Release (libFuzzer exit 77 pre-patch
    # on both; the nightly config is Debug). The widened arm gains the
    # ONE residual-key-colon coll_col law: the transform now mirrors
    # the gate's own key-colon line ("the mapping's indent is the key's
    # column") inside the residual walk - the r230 law fixed node_pos2
    # here, this fixes its coll_col2 twin; the walk's other sites keep
    # their state, so no existing member's path changes.
    # Comparator carry only - no engine change.
    # (The seeds ride the comparator corpus like the r230/r104 charset
    # seeds - no snapshot exclusion, no workflow edit.)
    # Seeds: anchor-charset-residual-keycolon-r305.bin (the 10-byte ddmin
    # floor "&8::\t>\n*a:"; every single-byte deletion stops the DIFFER)
    # and anchor-charset-residual-keycolon-r305-artifact.bin (the
    # 104-byte raw artifact - same dump pair modulo the digit runs).
    add_test(
        NAME fuzz-diff-r305-anchor-charset-residual-keycolon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-residual-keycolon-r305.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-residual-keycolon-r305-artifact.bin
    )

    # the r306 nightly escape (run 44127434757): the 32836-byte artifact
    # `[11 ,?##|,?\r/\n\r\r  **  **  50: ...<BOM><BOM><BOM>...&m ,]` - a
    # dense mixed-context flow soup: glued-`?#` members (the `?#` split
    # family's plain-? law, d), `?`-led split pairs and simple-key pairs
    # whose values ride CR break runs (the 1.2 break-fold law, b, with
    # the alias-collapse law, c, eating the `**` runs), and — the fresh
    # tail — THREE MID-LINE UTF-8 BOMs glued after a `?\r2` member
    # (offset ~12685). The BOMs are inert passengers: both engines keep
    # the marks byte-exact INSIDE their plains (3 in each actual dump),
    # so the soup divergence is unchanged - but the classifier's BOM
    # gate declined any BOM byte outright, and the BOM arms decline too
    # (the line-start arm's transform is line-start-only and its 8192
    # cap outruns the artifact; the content-strip transform cannot
    # reach a soup-shaped dump pair). The gate widening: a MID-LINE BOM
    # rides as inert content (the models' byte-exact plains already
    # reproduce it); a LINE-START BOM still routes to the BOM families
    # (the r226 law - libyaml strips at mark column 0 whatever break
    # opened the line, yeptris keeps it as content - the byte-exact
    # derivation declines those shapes on its own, the gate keeps the
    # routing explicit). Stripping the three marks turns the artifact
    # into an already-classified soup (the pre-patch classifier claims
    # the 32827-byte BOM-free twin), pinning the BOMs as the sole
    # unexplained piece. The 18-byte floor
    # `[1,?#,7,:\rK ?#,\rm]` adds the SECOND fresh law: the #741
    # colon-led SEQ single-pair STAYS OPEN across the separator (the
    # floor's `,\rm` member folds INTO the pair's map as a second
    # entry - libfyaml 04e0b58 and libyaml both read `m` as the bare
    # seq member that follows, and libyaml rejects the bare-colon
    # member outright, the ledgered CFD4 deviation - but the engine's
    # pair-stays-open transition is the deliberate #741 law, CFD4
    # suite-VALID), so the classifier gains the map-entry side: plain
    # (glued colon/question content rides whole), quoted, and
    # anchored-empty members take the key slot with the parser's
    # empty-scalar value; the bare-colon member keeps its empty key
    # with its value node; everything else declines. Comparator carry
    # only - no engine change.
    # (The seeds ride the comparator corpus like the r282-r305 soup
    # seeds - no snapshot exclusion, no workflow edit.)
    # Seed: flow-qsoup-bomride-r306.bin - the artifact rides whole
    # (charmin floor 32836 -> 18; the byte-deletion walk reshaped the
    # simple-key chunk into the bare-colon member, so the floor carries
    # the fold-in law the artifact itself never fires - both stay
    # pinned).
    add_test(
        NAME fuzz-diff-r306-flow-qsoup-bomride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-bomride-r306.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-bomride-r306-artifact.bin
    )

    # the r307 nightly escape (run 44144369860, the r306 carry's first
    # post-merge nightly): the parser legs write coverage-gaining soup
    # mutations into the shared corpus under libFuzzer hash names, so a
    # fresh soup scramble escaped the comparator snapshot's name-based
    # exclusion (the `flow-qsoup*` deletes) and seeded the diff leg.
    # The 10732-byte artifact is a multi-site scramble of the family's
    # own pinned shapes; the FIRST unexplained site is the value's tail
    # comment run: `1.0e17.tag\r #,|#\r#,??2002:binary\r\r,` - after the
    # r277 single whole-line comment (blank-# eats to its break, inert
    # on both engines) the mutant adds ANOTHER line-start `#` comment
    # (plus an inert break run) before the `,` terminator. The ride
    # loops: each whole-line comment eats to its own break, breaks and
    # blanks ride inert past it (the plain already ended), and only the
    # terminator (a separator/closer) is demanded - a plain byte
    # resuming after the run still declines (the r277 law's both-error
    # shape, never consulted). Byte-exact on both sides; the tamper
    # firewall stays red for anything the laws cannot place.
    # Comparator carry only - no engine change.
    # (The seeds ride the corpus like the r282-r306 soup seeds and are
    # excluded from the comparator snapshot by the same `flow-qsoup*`
    # deletes - no mutation pressure on the nightly diff leg.)
    # Seed: flow-qsoup-multicomment-r307.bin - the 8684-byte
    # tail-protected-ddmin floor (the scramble's mutant neighborhood is
    # dense with family-law compositions; the ctest pins stay the
    # regression bar, the comparator leg explores fresh space).
    add_test(
        NAME fuzz-diff-r307-flow-qsoup-multicomment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-multicomment-r307.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-multicomment-r307-artifact.bin
    )

    # the r308 nightly escape (run 44160636260, the r307 carry's first
    # post-merge nightly): another parser-leg soup scramble under a
    # libFuzzer hash name escaped the comparator snapshot's name-based
    # `flow-qsoup*` exclusion (the r269 accepted trade - each nightly
    # red is one more carry). The 32777-byte artifact is the family's
    # own pinned shapes at scale (608 `?##|` members, 977 yeptris pairs
    # vs libyaml's 1219 split pairs); the FIRST unexplained site is the
    # member-LED double-quoted scalar `",?g??<CR>2002:bi,...<CR>"` - a
    # 153-byte span whose interior break runs carry an EXOTIC member
    # (LS U+2028 mid-run after `/<LF><CR><CR>`) and whose tail break
    # sits right before the close. The old quoted walk declined ANY
    # exotic break inside a span outright (untested). The engines agree
    # byte-exact on the span (zero raw LS bytes in either actual dump):
    # yeptris's 1.2 fold law counts the exotic break in the run like
    # any other break (`/<LF><CR><CR><LS>` renders as three newlines,
    # n-1 LFs for the 4-break run) and the pending ONE-break tail joins
    # the close as the ONE fold space (the r278 `?"2<CR>"` -> `2 ` law;
    # the value ends `.tag `). The widening: the quoted walk folds
    # NEL/LS/PS as breaks (the repo-adjudicated break set), and the
    # close-join joins the bare one-break tail. A multi-break tail at
    # the close, trailing blanks (pure or riding the tail run), and a
    # multi-break-LED span all stay UNKNOWN. libyaml never OPENS this
    # span - the law-a comment-eat swallows the glued-`#` member's tail
    # including the quote - so the span rides the plain-comment walk on
    # the libyaml side and the derivation stays byte-exact both sides.
    # the ddmin floor: 38 of 32777 bytes survive the dual-bar walk
    # (pre-patch DIFFER + post-patch classify; every surviving byte is
    # load-bearing - the `,"g:<CR>"` member IS the minimal close-join
    # site; the artifact seed carries the LS-mid-span composition).
    # The scramble's neighborhood is dense with family-law
    # compositions; the ctest pins stay the regression bar, the
    # comparator leg explores fresh space.
    # Comparator carry only - no engine change.
    # (The seeds ride the corpus like the r282-r307 soup seeds and are
    # excluded from the comparator snapshot by the same `flow-qsoup*`
    # deletes - no mutation pressure on the nightly diff leg.)
    add_test(
        NAME fuzz-diff-r308-flow-qsoup-dqexotic
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqexotic-r308.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqexotic-r308-artifact.bin
    )

    # the r309 nightly escape (run 44178312079, the r308 carry's first
    # post-merge nightly): a sibling scramble of the r308 artifact (the
    # first 4096 bytes are shared; the motif pool gains only the digits
    # '4'/'9') escaped the comparator snapshot's `flow-qsoup*` exclusion
    # again. The 36555-byte artifact's FIRST unexplained site is the
    # pair's VALUE slot riding a whole-line comment:
    # `?<CR>/<LF><CR><CR><LS> 50:<CR>##|,?<CR>2002:binary` - after the
    # fold-colon `:` boundary and ONE CR break the `#` is
    # blank/break-preceded (the engine's comment condition), so BOTH
    # engines eat `##|,?` to its break and the value resumes
    # `2002:binary` - the pair (\n\n\n50, 2002:binary) rides byte-exact
    # on both sides, while the un-ridden model read `##|,?` as the
    # value plain. The widening: the value walk rides whole-line
    # comments across the blank/break run (the r295 loop-top ride's
    # mechanics, one slot deeper), looping for multi-comment runs (the
    # r307 law) - a GLUED colon-`#` stays out (the yeptris-rejects/
    # libyaml-accepts split, never a comment), and an eat to EOF stays
    # declined.
    # Comparator carry only - no engine change.
    # (The seeds ride the corpus like the r282-r308 soup seeds and are
    # excluded from the comparator snapshot by the same `flow-qsoup*`
    # deletes - no mutation pressure on the nightly diff leg.)
    add_test(
        NAME fuzz-diff-r309-flow-qsoup-valueslotcomment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-valueslotcomment-r309.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-valueslotcomment-r309-artifact.bin
    )

    # the r310 nightly escape (run 44191067135, the r309 carry's first
    # post-merge nightly): another parser-leg soup mutation under a
    # libFuzzer hash name escaped the comparator snapshot's name-based
    # `flow-qsoup*` exclusion (the r269 accepted trade). The 48-byte
    # artifact is a block x nested-flow hybrid - the root member
    # `?\t{...}` hangs a NESTED flow map off the split-explicit-key
    # member's KEY slot (the r281 mechanics one slot up from the value
    # slot), and the collection members plus the root members carry the
    # glued-`?` plains `?:-` / `?*M`: yeptris keeps each ONE flat plain
    # (the law-d ns-plain-first rule, suite HM87/652Z) while libyaml's
    # 1.1 flow KEY-token quirk splits the explicit keys (`:` rides as
    # the VALUE token -> the empty key with the plain `-` value; `*M`
    # reaches the alias scan -> the `=ALI *M` key). The fresh SITE is
    # byte-agreed on both engines (the nested key collection renders
    # +MAP/-MAP + the empty value pair identically) - the divergence is
    # the documented law-d split trapped behind the unmodeled key-slot
    # collection, which the old walk declined outright (`{` at the
    # split key slot was outside the family shape). The widening: the
    # split-key slot accepts `{` - the same member laws derive the
    # collection recursively, the blank/break run after the close rides
    # to the `:` VALUE boundary (the r286 shape; a glued colon and any
    # other non-boundary byte stay out).
    # Comparator carry only - no engine change.
    # the ddmin floor: 13 of 48 bytes survive the dual-bar walk
    # (pre-patch DIFFER + post-patch classify; every byte is
    # load-bearing - `?\t{}` IS the minimal key-slot collection member,
    # and `?-` / `?*M` are the minimal law-d twins the escape hid
    # behind it).
    # (The seeds ride the corpus like the r282-r309 soup seeds and are
    # excluded from the comparator snapshot by the same `flow-qsoup*`
    # deletes - no mutation pressure on the nightly diff leg.)
    add_test(
        NAME fuzz-diff-r310-flow-qsoup-keynestmap
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-keynestmap-r310.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-keynestmap-r310-artifact.bin
    )

    # the r311 nightly escape (run 44268982441): a dense multi-document
    # composition rode the anchor-name-charset arm past its carry search.
    # The law itself is the standing anchor-name split - `&4:` glues the
    # name to the colon: yeptris declines the anchor (the name charset
    # ends at the colon, the glued follower fails the props check) and
    # keeps the whole run ONE flat plain, while libyaml 0.2.x truncates
    # the name at the ':' (scan_anchor's legal-end set includes it) and
    # pairs anchor `&4` with the plain `:-` twin (suite 2SXE, the
    # ledger's standing upstream deviation; libfyaml reads a third way,
    # the 1.2 ns-anchor-char law - the colon IN the name). The arm's
    # transform carries that law by splitting the name at the truncation
    # point and verifying the copy; the carry search held all-splits +
    # single-split retries (r95). The fresh SITE is the COMBINATOR: the
    # load-bearing set here is a MULTI-split prefix (the two `&4:` sites
    # of the first document must ride the copy together - yeptris's copy
    # reading must equal libyaml's at each), while a LATER corrupting
    # split (the trailing `*0:-` alias twin - its blind space cuts a
    # plain both engines read as content) poisons the all-splits copy,
    # and NO single split verifies alone. The widening: the PREFIX
    # LADDER - each scan-order prefix of the recorded split list gets
    # the same verify legs (the corrupting split trailing the scan
    # order is exactly the r95 shape one member up; a corrupting split
    # EARLIER than a load-bearing one and every leg-rejected shape stay
    # out).
    # Comparator carry only - no engine change.
    # the ddmin floor: 30 of 8209 bytes survive the dual-bar walk
    # (pre-patch DIFFER + post-patch carry; every byte is load-bearing -
    # the two `&4:` sites are the minimal anchor-twin pair, `---`/`--`
    # stage the document cross, and `! *0:-` is the minimal corrupting
    # alias twin the escape hid behind).
    add_test(
        NAME fuzz-diff-r311-anchor-charset-docsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-docsoup-r311.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-docsoup-r311-artifact.bin
    )

    # the r312 nightly escape (run 44292507063): the anchor-name-charset
    # family again, with the corrupting alias twin now LEADING. The law
    # is unchanged - `&4:` glues the name to the colon and yeptris keeps
    # ONE flat plain while libyaml 0.2.x truncates at ':' (suite 2SXE) -
    # but the r311 prefix ladder covers a corrupting split only when it
    # TRAILS the load-bearing prefix. Here a `--` line stages the twin:
    # the walk's seq-dash law took the SECOND dash of `--` for a
    # construct boundary (coll_col = line_first with no position check),
    # closed the root-level plain the -x law had opened, and staged the
    # next line's `*0:a` as a phantom alias NODE. Its split ("*0:a" ->
    # "*0 :a" inserts a space into a plain both engines read as one
    # root-level rider, issue #718's indent -1 law) is libyaml-NON-
    # invariant and LED the scan-order split list, ahead of the
    # load-bearing `&4:` pair - no ladder prefix covered the pair and
    # the carry declined its own family.
    # The widening: the seq-dash law takes the guard its '?' explicit-key
    # sibling has always carried - col == line_first, the dash is the
    # LINE'S FIRST TOKEN. The glued second dash of `--` is mid-token
    # plain content, the riding plain keeps swallowing (the root law),
    # and the twin's run stays content: the phantom split site
    # disappears and the all-splits copy (the two `&4:` truncations,
    # r311's own composition) carries on every seed. Model fix only -
    # the carry search gains zero new power.
    # Comparator carry only - no engine change.
    # the ddmin floor: 28 of 14240 bytes survive the dual-bar walk
    # (pre-patch DIFFER + post-patch carry; 28/28 single-byte deletions
    # lose the dual bar - `--` stages the dash-drift plain, `*0:a` is
    # the minimal leading corrupting twin, `---` crosses the document,
    # and `9: &4:-` / `&4:: &1` are the minimal anchor-twin pair).
    add_test(
        NAME fuzz-diff-r312-anchor-charset-dashtwin
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-dashtwin-r312.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-dashtwin-r312-artifact.bin
    )

    # the r313 nightly escape (run 44314834088): the anchor-name-charset
    # family again - the r234 docstart seed's content DOUBLED
    # ("---<PS> ? &--02:in~---<PS> ? &--02:in~"), the two explicit-key
    # lines the new ingredient. The law is unchanged - yeptris keeps the
    # 1.2 ns-anchor-char wide name (':' is not a flow indicator; only a
    # blank/break ends it) riding an anchored-empty while libyaml 0.2.x
    # truncates at ':' and pairs the residual ':'-led plain (suite 2SXE;
    # libfyaml 04e0b58 A/B reads yeptris's stream byte-exact on the LF
    # twin) - but the walk's '?' explicit-key law carried a guard that
    # NEVER fired: col is incremented before the arms run, so the arm's
    # `col == line_first` compared the NEXT byte's column against the
    # first-token column. coll_col kept the root -1, the ':'-led
    # residual plain (the HM87 riding-colon law) crossed the PS into the
    # second line's '?', the second anchor run rode as mid-plain CONTENT,
    # its split never staged, and the all-splits copy (the r311
    # composition, one truncation per '&' run) declined its own family.
    # The fix: the guard takes the byte's own column (col - 1), the
    # '?' sets coll_col to its line's first-token column, the second
    # line's '?' ends the riding plain, and the EXISTING all-splits copy
    # carries. Model fix only - the carry search gains zero new power
    # (the seq-dash r312 sibling keeps its shipped dead-guard state; the
    # dash arm is untouched in this carry).
    # Comparator carry only - no engine change.
    # the ddmin floor: 20 of 36 bytes survive the dual-bar walk
    # (pre-patch DIFFER exit 77 + post-patch carry exit 0; 20/20
    # single-byte deletions lose the dual bar - `---<PS>` stages the
    # PS-terminated doc marker, the first `? -` member rides a seq
    # entry into the map, and `? &2:i` is the minimal colon-in-name
    # anchor twin).
    add_test(
        NAME fuzz-diff-r313-anchor-charset-explicitkey
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-explicitkey-r313.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-explicitkey-r313-artifact.bin
    )

    # the r314 nightly escape (run 44354924371): the 8290-byte soup
    # artifact `{<CR-run>  **  51:<CR><CR>1.0e17,2002:binaryg:...` - the
    # flow-qsoup family (the ?##| glued members ride laws a/d, the CR
    # runs ride law b, the ** runs ride law c) with TWO fresh members
    # the walk never modeled, each carrying its own widening. THE FLOOR
    # LAW: the minimizer's 7-byte floor `{[?&m]} ` - a nested flow
    # SEQUENCE at a MAP member start. yeptris reads the member as the
    # nested collection itself (+SEQ/-SEQ, the glued-`?` plain `?&m`
    # riding the HM87/652Z plain-? law, d - `&` is mid-plain content
    # after the non-blank `?`) against libyaml 0.2.x's unconditional
    # flow-`?`-KEY law (scanner.c fetch_next_token: `?` in flow_level is
    # a KEY whatever follows - an INDICATOR follower, outside the r183
    # arm's plain-safe gate), which wraps the single pair as a NESTED
    # map with the anchored-empty key (`=VAL : &m `). The A/B: libfyaml
    # 04e0b58 reads the floor byte-exact as yeptris does (one plain
    # inside the seq - 2-of-3 with the 1.2 grammar's ns-plain-first,
    # where `?` followed by an ns-plain-safe byte starts a plain scalar
    # in flow context too) - comparator divergence, no engine claim.
    # THE TAIL LAW: the artifact's last member is the quoted `"empky":`
    # at a map member start - the engines AGREE byte-exact on the
    # probes (`{"a"}` / `{"a":}` / `{"a":x}`; a `:` splits
    # unconditionally after a quoted scalar in flow), the walk just
    # needed to claim the shape to reach the tail. The classifier gains:
    # the y-side nested-`[` member arm (the r281/r310 stash mechanics
    # with the per-level seq_at stamp - the member walks read the
    # ENCLOSING level's kind, the value-slot/split-key `{` walks restamp
    # 0 so sibling branches never read stale), the y-side quoted-KEY
    # member arm, the l-side nested-seq tokenizer slots (the depth>0
    # `[` case mirrors `{`) and the recursive l_render_seq body (the
    # root walk's KEY-wrapper semantics, ending at the level's closer),
    # and the self-test's nesting-splice mutant moves to a `{`-led
    # member (the empty-seq splice `,[]` is law-governed now, the
    # mutant-3 precedent - 49 of them claimed byte-exact pre-move, 0
    # post).
    # Comparator carry only - no engine change.
    # (The seeds ride the comparator corpus like the r282-r313 soup
    # seeds - no snapshot exclusion, no workflow edit.)
    # Seeds: flow-qsoup-nestedseq-r314.bin (the 7-byte ddmin floor
    # `{[?&m]}<SP>`; 7/7 single-byte deletions lose the dual bar -
    # pre-patch DIFFER exit 77, post-patch carry exit 0) and
    # flow-qsoup-nestedseq-r314-artifact.bin (the 8290-byte raw
    # artifact - classified byte-exact post-widening, both models
    # reproduce the full 1193-line/937-line dump pair).
    add_test(
        NAME fuzz-diff-r314-flow-qsoup-nestedseq
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-nestedseq-r314.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-nestedseq-r314-artifact.bin
    )

    # the r315 nightly escape (run 44411162279): the 52-byte soup
    # artifact `[1,?0e\Su.0v ,?#<U+0503>,:&<CR>&binary<CR>, ?#<U+0503>,
    # :&<CR>&binary<CR>, (?,]` - the flow-qsoup family (the glued-`?`
    # plains ride laws a/d, the CR runs ride law b) with the ANCHOR
    # slots' ride gap. THE FLOOR LAW: the minimizer's 14-byte floor
    # `[?\,?#,<CR>&i<CR>,?]` - a bare-anchor member whose terminator
    # sits across a blank/break run. yeptris reads `&i<CR>,` as the
    # anchored-empty member (the prop walk skips the run before the
    # flow indicator, nothing folds into the empty node: `=VAL : &i `)
    # against libyaml 0.2.x where the member-2 comment-eat swallows
    # `#,` to the CR and the post-break `&i` rebinds as the explicit
    # key (the law-a post-break clause; the anchor renders on the empty
    # key scalar, `=VAL : &i ` in the KEY slot). The same ride shape
    # gates the artifact's `:<CR>&binary<CR>,` fold-in members (the
    # #741 open pair, the anchored-empty value slot) and the open
    # pair's glued-`?` plain member (P7's content law - the gate
    # predating it declined the whole `?` spell under the pair). The
    # classifier gains: the bare-anchor member arm, the anchored-empty
    # value slot, and the fold-in anchor member all ride the
    # blank/break run to the terminator (the r301/r302 alias-slot law's
    # anchor twins), and the fold-in `?` gate splits glued (plain walk,
    # P7) from split-explicit-key (stays out). THE A/B: both models
    # reproduce the full dump pair byte-exact post-widening; the
    # libyaml side rides the documented 1.1 flow-`?`-KEY +
    # comment-eat-to-breakz laws (the `#` eats THROUGH the `,` to the
    # CR), no engine claim.
    # Comparator carry only - no engine change.
    # (The seeds ride the comparator corpus like the r282-r314 soup
    # seeds - no snapshot exclusion, no workflow edit.)
    # Seeds: flow-qsoup-anchor-breakride-r315.bin (the 14-byte floor;
    # 14/14 single-byte deletions lose the unclaimed-DIFFER dual bar -
    # 10 become agreements, 1 rides the r284 glued-anchor arm as a
    # claimed member, 3 become the documented accept/reject splits:
    # the yeptris-rejects/libyaml-accepts glued-`#` member and two
    # libyaml-rejects rides) and
    # flow-qsoup-anchor-breakride-r315-artifact.bin (the 52-byte raw
    # artifact - classified byte-exact post-widening, both models
    # reproduce the full dump pair).
    add_test(
        NAME fuzz-diff-r315-flow-qsoup-anchor-breakride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-anchor-breakride-r315.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-anchor-breakride-r315-artifact.bin
    )

    # the r317 nightly red (run 44439765034, main f540eeed — the first
    # post-#1478 budget raise, 11s in): the 30-byte folded-scalar
    # artifact "> #<PS> :\n <NEL>\n<PS> :\n  <NEL> 0\n 2 t" — a `>`
    # header whose space-preceded comment is terminated by a raw PS
    # with doubled NEL pairs riding the body lines. The divergence is
    # the r283 header-comment-LS/PS split landing one line EARLIER
    # than the kept-NEL site: yeptris's whole-route normalizer folds
    # the header-comment PS (the in_comment arm) and keeps the body
    # NEL pairs raw in block bodies (1.2 content), so the break before
    # the NEL line folds to the ONE space ("SP NEL"); libyaml's 1.1
    # reader breaks AT the NEL (IS_BREAK), the line is a blank, and
    # both breaks escape, while its READ_LINE keeps the raw PS triple
    # in the value where yeptris spells the escape. The walk model
    # gains ONE branch (the pre-NEL fold-space site: pair the s fold
    # spaces + k kept NELs to the first s+k escapes, short-run
    # declines, surplus stays — the r290 direction law); the r197
    # polarity, the r288 fold-space and the r176 tail own everything
    # behind. THE A/B: libyaml 90a56d4 ERRORS on the pure-NEL body
    # twins ("> \n<NEL>y" — "did not find expected <document start>")
    # while accepting the mixed seed, and libfyaml 04e0b58 KEEPS the
    # raw NEL as block content (the LF twins' "\N" render) — the
    # comparators split, so this is comparator carry only, no engine
    # change.
    # the ddmin floor: 20 of 30 bytes survive the dual bar
    # (pre-patch DIFFER exit 77 + post-patch carry exit 0): "> #<PS>
    # :\n <NEL>\n<PS><NEL> 0" — the header-comment-PS, the NEL-riding
    # blank-ish line, and the glued PS-NEL pair; every further single
    # deletion loses the unclaimed-DIFFER arm.
    # the 50-mutant paired battery (30 single + 20 double, seed 317):
    # 13 newly-carried (family members the branch explains), 35
    # clean-both, 0 anomalies, and 2 HONEST DECLINES (del '0'/del the
    # LF behind it — the `:` line's break becomes libyaml's
    # dropped-fold-break surplus, the r283 gap class, still red).
    add_test(
        NAME fuzz-diff-r317-folded-header-ps-nel
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-ps-nel-r317.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-ps-nel-r317-artifact.bin
    )

    # the r318 nightly red (run 44458062455, main af1c8e72 — the first
    # post-r317 red): the 12-byte artifact "{?*L:\n,&m:(}" — a flow map
    # whose first member is the GLUED-`?` explicit key with an ALIAS as
    # the key (`?*L:` — the law-(d)/(c) plain-? + alias-as-plain-content
    # split: yeptris renders `=VAL : ?*L` + the empty value, libyaml's
    # 1.1 KEY-token split surfaces `=ALI *L`), then the bare-anchor
    # member `&m:(}` at the MAP key slot. THE DIVERGENCE is the anchor
    # charset cut at that key slot: the engine's yep_scan_prop_end
    # keeps `:(` inside the name per the suite (2SXE/W5VH — the r311
    # law, libyaml behind the suite), so NO flow VALUE token fires and
    # the parser inserts the empty value at the closer (`=VAL : &m:( ` +
    # `=VAL : `); libyaml's scan_anchor cuts at `m` (IS_ALPHA) and the
    # 1.1 flow VALUE token fires on the GLUED ':' (key = anchored-empty
    # `&m`, value = the plain `(`). The family classifier's `&` member
    # walk owned both gates that declined it — the MAP level and the
    # agreeing-name charset — and gains ONE branch: the MAP key slot's
    # charset-divergent anchor member (the ':' glued AT the libyaml
    # cut, the prop name terminated by a flow indicator, the divergence
    # real, MAP levels only); the libyaml walk derives the cut tail
    # independently and the entry's dual byte-equality is the firewall.
    # THE A/B: libfyaml 04e0b58 reads yeptris's stream byte-exact on
    # every probe (`{&m:(}` / `{*L,&m:}` / the artifact) — comparator
    # carry only, no engine change.
    # the ddmin floor: 8 of 12 bytes survive the dual bar (pre-patch
    # DIFFER exit 77 + post-patch carry exit 0): "{*L,&m:}" — the bare
    # alias member both engines agree on (`=ALI *L`) and the `&m:}`
    # name-cut twin; every further single deletion loses the red arm.
    # the 50-mutant paired battery (30 single + 20 double, seed 318):
    # 8 newly-carried (family members the branch explains), 21
    # clean-both, 0 anomalies, and 21 HONEST DECLINES (the `:` line's
    # break deleted — the comma glued after the colon declines in the
    # law-(d) member walk's own gate — and the YEPTRIS-ONLY-ACCEPT
    # verdict classes the classifier is never consulted on).
    add_test(
        NAME fuzz-diff-r318-anchor-charset-key-map
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-key-map-r318.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-key-map-r318-artifact.bin
    )

    # the r339 nightly red (run 44864014989, main 026d7a11): the 12-byte
    # artifact "\n{?*L:,&m:(}" — the r318 composition's COMMA-GLUED-COLON
    # face. Member 1 is the law-(d)/(c) plain-? + alias-as-plain-content
    # split with its colon boundary GLUED to the flow separator (`?*L:,`
    # — yeptris renders `=VAL : ?*L` + the empty value, libyaml's 1.1
    # KEY-token split surfaces `=ALI *L` + the empty value), member 2
    # the r318 anchor charset cut (`&m:(}` — `=VAL : &m:( ` + `=VAL : `
    # vs libyaml's `=VAL : &m ` + `=VAL : (`). The family classifier
    # declined exactly where the r318 session's honest-declines list
    # pinned it: "the law-(d) member walk's own gate on the
    # comma-glued-colon line" — fc_piece's `:` + flow-indicator decline
    # fires under the MAP-root qm=0 walk. THE WIDENING (the rider
    # carve-out): libyaml's flow `?` fires the KEY token and the `*`
    # follower fires the ALIAS token — fetch_key removes the simple key
    # and sets `simple_key_allowed = (!flow_level)` = 0, so the alias
    # saves NO key, and fetch_value's complex-key path is check-free in
    # flow — NO PLAIN IS EVER OPEN at the glued colon, so the r291
    # "still-open plain hits the unexpected ':' error" premise is false
    # and the colon + flow indicator derives as the VALUE boundary on
    # BOTH engines. THE A/B: libfyaml 04e0b58 reads the artifact
    # byte-exact yeptris's way (parse-dump: `SCALAR ?*L` / `SCALAR ` /
    # `SCALAR &m:( ` / `SCALAR `) — comparator carry only, no engine
    # change. The floor: 10 of 12 bytes survive the dual bar
    # "{?*L:,&m:}" (the leading break and the `(` are inert — the r318
    # cut fires on the glued `:` alone); the 50-mutant paired battery
    # (30 single + 20 double, seed 339): 6 newly-carried (alias-name
    # variants `?*U`/`?*8`/`?*g`/`?*4` and the `&o` twin), 21 honest
    # declines (plain/indicator followers that keep the r291 premise
    # — `?9`-style plain heads, `!`/`&` riders, the break-form twin
    # without its comma), 23 clean-both, 0 anomalies.
    add_test(
        NAME fuzz-diff-r339-alias-rider-glued-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-rider-glued-colon-r339.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-rider-glued-colon-r339-artifact.bin
    )

    # the r319 nightly red (run 44470194441, main 4732ed9f — the first
    # post-r318 red, and the first input the r316 timeout raise let the
    # comparator FINISH: the same `?##`+CR+`1.0e17.tag`+`2002:binary`+
    # `&m,` soup family as the r316 timeout sibling, one level deeper).
    # The 9525-byte artifact is a flow SEQ of glued-`?` soup members;
    # THE DIVERGENCE is the law-(a) member the libyaml side resolves
    # through its 1.1 KEY-token + unconditional-comment quirk: the
    # member `?#` fires libyaml's flow KEY token (the 1.1 unconditional
    # quirk), the GLUED `#` (the scanner lands on it post-token, no
    # blank-before requirement) comment-eats THROUGH the member's `,`
    # separator AND the next member's `?` up to the CR, and the
    # post-break bytes rebind as that map's KEY (the artifact's `?## &m`
    # twin rebinds onto the ANCHOR: `+MAP, =VAL : &m , =VAL : , -MAP`
    # — law-c's prop-on-empty-scalar rendering), while yeptris keeps
    # `?#` a flat SEQ-member plain (the glued `#` is not blank-preceded,
    # the 1.2 comment condition) and `?<CR>/...` a split-explicit-key
    # member. The libyaml model already derived the artifact byte-exact
    # (16731 bytes); the YEPTRIS model needed three widenings, ALL on
    # shapes both comparator engines render byte-identically (EQUAL
    # probes — comparator carry, no engine change):
    #  1. the silent comma ride (the floor's `?,,g` member): the
    #     engines consume ONE stray ',' past the indicator member's
    #     terminator byte-exact (`[1,?,,g,h]` -> (1,{e,e},g,h),
    #     `[1,?,,]` -> (1,{e,e})); one comma only — the triple form
    #     (`[1,?,,,g]`) and the break-between form (`[1,?,\rg]`) are
    #     BOTH-ERROR shapes, and the MAP root errors the ride
    #     (`{1,?,,g}`) — SEQ-rooted only; the libyaml model's member
    #     loop already skipped the leading FLOW-ENTRY;
    #  2. the comment-run separator count is INERT comment text: the
    #     artifact's `?<CR>#, 12?1#|,?<CR>/...` member carries TWO
    #     commas inside the eat and both engines render the identical
    #     r245 steal (the probes `[a,?\r#x,y,b\nc]` /
    #     `[a,?\r#x,y,z,b\nc]` render `+MAP(c,)` byte-exact) — the
    #     root matrix keeps its shape (a comma-carrying run on the MAP
    #     root and a clean eat on the SEQ root stay untested-declined),
    #     an eat to EOF still declines;
    #  3. the stolen key rides the SAME plain walk the split plain-key
    #     slot takes — SEQ-rooted, so the r272 KEY-implicit colon cut
    #     applies (the artifact's stolen key `/<LF><CR><CR><PS>
    #     51:<CR><CR>1.0e17.tag` cuts at the colon+break boundary:
    #     key = `/\n\n\n51`, the common colon path opens the value
    #     `1.0e17.tag`); the MAP-rooted clean eat keeps the r277 plain
    #     law; flat stolen keys never see the difference.
    # THE A/B: libfyaml 04e0b58 joins the shared reading on the steal
    # probes byte-exact (`[a,?\r#x,b\nc]` / the multi-comma twins) and
    # rejects the ride as a third reading ("flow sequence with invalid
    # extra comma") — the r318 precedent's comparator-carry class, no
    # engine change.
    # the ddmin floor: 28 of 9525 bytes survive the dual bar (pre-patch
    # DIFFER exit 77 + post-patch carry exit 0):
    # `[1,?,,g\r?#<PS>,?#,?#,?\r/<PS>m]` — the `?`-indicator member
    # with the silent-comma ride, the both-agreeing `g ?#` fold plain,
    # and the two `?#` KEY-token/comment-eat members whose rebind
    # steals the `?<CR>/<PS>m` split-key member's bytes; every further
    # single deletion loses the red arm (717-run ddmin).
    # the 50-mutant paired battery (30 single + 20 double, seed 319):
    # 5 newly-carried (family members the widenings explain), 20
    # clean-both, 0 anomalies, and 25 HONEST NON-CLAIMS (the
    # YEPTRIS-ONLY-ACCEPT verdict classes the classifier is never
    # consulted on — the floor's single-bit mutants land the lenient
    # UTF-8-lead class).
    add_test(
        NAME fuzz-diff-r319-comma-ride-commentrun
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-comma-ride-r319.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-comma-ride-r319-artifact.bin
    )

    # the r320 nightly red (run 44499300534, main 40e5dac9 — the first
    # post-r319 red, DIFFER 33s into the diff leg): the 17-byte artifact
    # "> #<PS> <NEL><LF><PS><NEL> 0" — a folded-scalar header whose
    # space-preceded comment carries a raw PS, with a NEL/LF/PS/NEL
    # break run and a trailing ` 0` plain behind. The r317 sibling
    # covered the PRE-NEL fold-space site in this same walk; the r320
    # shape is the folded-HEADER side and its tail INVERTS: yeptris's
    # #638 normalizer folds the header-comment PS (the in_comment arm,
    # the r283 header-comment shape) and its bs-content token consumes
    # the " <NEL>" line FOLD-BLIND (the r317 kept-NEL law — the NEL
    # rides raw into the buffer and the engine reads the line as
    # CONTENT at indent 1 off its leading space), so the kept-NEL line
    # absorbs its own break into the fold composition (breaks_before 3
    # → the fold emits n-1) and the value reads
    # [kept NEL][ESC][ESC][0][clip ESC]; libyaml's 1.1 reader breaks AT
    # the NEL — four blank lines whose breaks join LITERALLY — so its
    # value reads [ESC][ESC][raw PS][ESC][0] (the PS-line's READ_LINE
    # keeps the triple; the r197 polarity pairs it behind the walk's
    # first three pairings). At the value tail the one-short ledger
    # INVERTS the final break/content pair: yeptris [plain][ESC] where
    # libyaml [ESC][plain]. The walk gains ONE branch (the kept-NEL
    # fold-ledger inversion site): one escape on each side, the SAME
    # plain byte behind each (not a space — the space-run pairing is
    # the NEL-site law's own material; not an escape — the r197
    # polarity owns those; not a raw NEL/LS/PS lead byte), and the
    # remainders behind the claim coincide completely (the r161 strcmp
    # law). All existing members keep their exact path (the branch
    # sits behind the r197 polarity and ahead of the r283
    # dropped-fold-break arm, whose gate wants a ON the escape).
    # THE A/B: libyaml 90a56d4 reads the four blank lines literally
    # ("=VAL : \n\n<PS>\n0"), libfyaml 04e0b58 keeps BOTH the PS and
    # the NEL raw as the body's verbatim content
    # ("=VAL > <PS><NEL> 0\n" — 1.2: no exotic break is a break) — the
    # comparators split THREE ways, so this is comparator carry only,
    # no engine change (the r317 precedent).
    # Floor: the nightly's artifact arrives PRE-MINIMIZED — the 17
    # bytes ARE the dual-bar floor (pre-patch DIFFER exit 77 +
    # post-patch carry exit 0): all 17 single-byte deletions lose the
    # red arm (pre-patch clean on every deletion), so the seed and the
    # artifact are one file here.
    # the 50-mutant paired battery (30 single + 20 double, seed 320):
    # 50 clean-both, 0 newly-carried, 0 anomalies, 0 declines — the
    # 17-byte ultra-minimal floor's neighborhood is entirely
    # agreements (consistent with the deletion sweep: the divergence
    # needs all 17 bytes).
    add_test(
        NAME fuzz-diff-r320-folded-header-comment-nel-inversion
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-comment-nel-inversion-r320.bin
    )

    # the r321 scheduled-nightly red (run 44511876660, main 40e5dac9 —
    # the r317/r320 folded-header family's third composition): the
    # 32-byte artifact "> #<PS> :<LF>  <NEL> 5<LF><PS> #<PS><NEL>
    # 0<LF>  t" — the folded `>` header whose space-preceded comment
    # terminates at the raw PS, a kept-NEL content line, and the
    # PS-comment line with a trailing ` 0`/`  t` behind. The 18-byte
    # ddmin floor "> #<PS> <NEL><PS> <PS><NEL>" is the family's
    # ALL-BLANK-BODY composition: every body line is spaces + an
    # exotic break (never a folding break), so libyaml's 1.1 reader
    # composes an EMPTY value (blank lines feed max_indent, the clip
    # chomps every trailing break) while yeptris's 1.2 whole-route
    # normalizer keeps the exotic breaks raw IN the body (the r317
    # kept-NEL law: the post-header bytes are ONE content line at the
    # auto-indent, the leading space stripped once, interior spaces
    # riding raw) and the clip appends the one LF escape. The walk
    # gains the all-blank-body site (the r321 site: b's value EMPTY at
    # an empty-value line end, a's value one raw NEL/PS/SP run with a
    # kept NEL or PS, terminated by the clip's LF escape) and the
    # r216-class libyaml surplus-escape mirror (the older four-pass
    # walk's own law, composed into this walk's r197+r283 chain: the
    # artifact's PS-comment line pairs esc-esc-# esc against PS # PS
    # esc and leaves libyaml's ONE NEL-break escape uneaten — yeptris's
    # comment/fold accounting consumed the exotic break pair into the
    # ONE escape it spells). THE A/B: libfyaml 04e0b58 renders the
    # same EMPTY value as libyaml on the floor (both comparators agree
    # against yeptris's raw-kept reading) — comparator carry only, no
    # engine change (the r317 precedent).
    # Floor: the artifact was NOT pre-minimized — the 18-byte floor is
    # the dual-bar survivor (ddmin 251 runs; pre-patch DIFFER exit 77 +
    # post-patch carry exit 0 on BOTH the floor and the full artifact).
    add_test(
        NAME fuzz-diff-r321-folded-header-allblank-body
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-allblank-body-r321.bin
    )

    # the r321 scheduled-nightly red's second composition (the same
    # run, the DIFFER that fired first — the soup family one level
    # deeper than r319): the 18555-byte artifact's flow soup of
    # `?##`-comment-run members, CR breaks, `1.0e17.tag`/`2002:binary`
    # tag-plain members, and `&m,` anchor rides. The 18-byte ddmin
    # floor "[1,?#,:<CR>1,?/<PS>:<CR>]" composes THREE documented
    # laws: (1) the `?#` KEY-token + comment-eat member (the r245/r319
    # law — libyaml's post-break `1` rebinds as the map's key, the
    # SEQ's comma closes it as {1: empty}); (2) the bare-colon member
    # (the r281 colon-led law — yeptris opens the pair that STAYS
    # open, key empty); (3) the pair member `?/` with the post-break
    # `:` VALUE boundary — the classifier's pair walk gains the
    # colon-boundary widening (the engine re-lexes the post-break `:`
    # as the VALUE token; the value node rides y_value_node exactly
    # like the MAP-root/plain member path; only the plain/empty value
    # form claims — every isolated probe instance is YEPTRIS-ONLY, so
    # the classifier is consulted only on the composed DIFFER shapes).
    # The full artifact additionally needed the l_tokenize correction:
    # libyaml's fetch law fires the `---`/`...` document indicator
    # ONLY when the byte after the triple is IS_BLANKZ — the
    # artifact's `...L` tail member (a `...` followed by a non-blank
    # at a line start) rides as PLAIN CONTENT, and the model's
    # check was stricter than the engine (false decline).
    # Floor: 18 of 18555 bytes survive the dual bar (ddmin 984 runs;
    # pre-patch DIFFER exit 77 + post-patch carry exit 0 on BOTH the
    # floor and the full artifact, plain and _lf).
    add_test(
        NAME fuzz-diff-r321-qsoup-colon-boundary-pair
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-colon-boundary-pair-r321.bin
    )

    # the r322 scheduled-nightly red (run 44560533569, main cc1b3c33 —
    # the r62 anchor-glued-follower family one line earlier): the
    # 11-byte floor "- :\"\n- &g:-" — a block seq whose FIRST entry is
    # the ':'-led plain ':"' and whose SECOND entry is '&g:-'. libyaml
    # anchors 'g' and reads the plain ':-' (its anchor charset accepts
    # the glued ':' follower); yeptris's props scan requires a
    # separator after the name and bails the WHOLE token to plain
    # (the r62 law, the trailing-space shape of the bail riding since
    # that carry). The classifier's anchor-glued-follower walk ALREADY
    # claims the one-line forms ('- &g:-' alone passes the probe) —
    # the nightly escape was the tracker's quote-open arm: the '"'
    # glued to the block ':' of the FIRST entry opened a phantom quote
    # that swallowed the newline and the '&' on line 2 never gated.
    # The widening: a quote glued to a ':' at depth 0 is plain CONTENT
    # (both engines' glued-indicator plain reading — the HM87/652Z
    # suite law; libyaml's block plain breaks only on ':'+blank), so
    # the tracker rides the newline; in flow the ':'+quote pair is the
    # VALUE indicator plus a REAL quote and the arm stays depth-gated.
    # THE A/B: the three-oracle spread on '&g:-' (libyaml 90a56d4
    # anchor 'g' + plain ':-'; libfyaml 04e0b58 anchor 'g' + EMPTY
    # value; yeptris the whole-token plain bail) — three lenient
    # readings of one glued-indicator form, the r62 adjudication —
    # comparator carry only, no engine change.
    # Floor: the artifact IS the floor — 11 of 11 bytes survive the
    # dual bar (ddmin found no removable byte; pre-patch DIFFER exit
    # 77 + post-patch carry exit 0, plain and _lf; the space-separated
    # rewrite "- :\"\n- &g :-" reads EQUAL on both engines and equals
    # libyaml's original dump byte-exact).
    add_test(
        NAME fuzz-diff-r322-anchor-glued-quote-led
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-anchor-glued-follower-quote-led-r322.bin
    )

    # the r323 scheduled-nightly red (run 44579814561, main 37327760 —
    # the r317/r320/r321 folded-header family's fourth composition):
    # the 158-byte artifact's doubled "> #<PS> :" comment headers, the
    # long f-plain bodies, and the blank-per-1.1 interleave lines.
    # The headers and plains ride the existing r283/r197/r320/r317
    # laws; the two NEW sites are (1) the blank-run kept-exotic
    # interleave BETWEEN content lines ("SP <NEL><NEL> SP" before a
    # "0" — yeptris keeps the blank line's material RAW in the value
    # while libyaml's trailing-breaks join spells it as its 1..k LF
    # escapes and eats the spaces) and (2) the EOF-tail kept NEL with
    # the fold space riding on BOTH sides ("LF SP <NEL>": yeptris's
    # tail reads SP NEL SP ESC where libyaml collapses the whole tail
    # into the ONE escape). THE A/B: libfyaml 04e0b58 keeps BOTH the
    # PS and the NEL raw as the body's verbatim content (1.2: no
    # exotic break is a break), splitting THREE ways exactly as the
    # r320 A/B did — comparator carry only, no engine change (the
    # r317 precedent).
    # Floor: 23 of 158 bytes survive the dual bar (ddmin; pre-patch
    # DIFFER exit 77 + post-patch carry exit 0 on BOTH the floor and
    # the full artifact, plain and _lf; every single-byte deletion of
    # the floor either reads clean or drifts the face). Both the floor
    # and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r323-folded-header-blankrun-kept-exotic
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-blankrun-kept-exotic-r323.bin
    )
    add_test(
        NAME fuzz-diff-r323-folded-header-blankrun-kept-exotic-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-blankrun-kept-exotic-r323-artifact.bin
    )

    # the r324 nightly red (run 44597736377, main 4a75570c — the
    # r317/r320/r321/r323 folded-header family's FIFTH composition):
    # the 32-byte artifact's doubled "> #<PS> :" comment headers, the
    # mid-body kept-NEL line at indent 2, and the EOF tail grown to
    # the "SP <NEL> SP <NEL>" DOUBLE kept-exotic run behind a "0"
    # content line. The headers and the mid-body line ride the
    # r283/r197/r317 laws behind the walk's earlier pairings; the
    # site the r303/r304/r323 tail arms cannot reach is the doubled
    # exotic tail: yeptris's 1.2 whole-route keeps the tail raw (NEL,
    # interior spaces, NEL) and spells its own clip escape behind,
    # while libyaml's 1.1 trailing-breaks join collapses the whole
    # tail into the few escapes its ledger counted. THE WIDENING —
    # the family's GENERALIZING TAIL GRAMMAR, not a sixth pin: a's
    # remainder is a pure run of spaces / kept NELs / kept LS-PS
    # triples (>= ONE kept exotic — the family signature; a
    # pure-space surplus is the real divergence shape, still
    # declined), everything behind the run is byte-identical to b's
    # WHOLE remainder (the r161 strcmp law — b-side bytes are never
    # eaten), and at most the ONE clip escape rides beside it as
    # surplus. The grammar sits at TWO arrival points: the walk's END
    # (behind every member, so each existing member keeps its exact
    # path) and inside the NEL-site branch's r < k cap (behind the
    # r294 sub-case, whose exact path holds) — the floor's alignment
    # shifts one escape earlier than the artifact's, so the same
    # shape arrives with the cap's return in between. THE A/B:
    # libyaml 90a56d4 eats the twin's whole exotic tail (the value
    # empties, then it demands the document start), libfyaml 04e0b58
    # keeps BOTH NELs raw ("=VAL > 0\n \N \N" — 1.2: no exotic break
    # is a break) — the two 1.2 readers agree the exotics are
    # content, comparator carry only, no engine change (the r317
    # precedent).
    # Floor: 25 of 32 bytes survive the dual bar (greedy single-byte
    # ddmin; pre-patch DIFFER exit 77 + post-patch carry exit 0 on
    # BOTH the floor and the full artifact, plain and _lf; every
    # single-byte deletion of the floor reads clean pre-patch — the
    # r320 ultra-minimal form). The 50-mutant paired battery (30
    # single + 20 double, seed 324): 43 clean-both, 7 newly-carried,
    # 0 declines, 0 anomalies. Both the floor and the full artifact
    # ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r324-folded-header-family-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-tail-r324.bin
    )
    add_test(
        NAME fuzz-diff-r324-folded-header-family-tail-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-tail-r324-artifact.bin
    )

    # the r325 nightly red (run 44611344281, main 37e7e09d — the
    # r317/r320/r321/r323/r324 folded-header family's SIXTH composition,
    # the first since the r324 tail grammar landed): the 35-byte artifact
    # "> #<LS> :<LF>  <NEL> 5%<LS> :<LF> <LS> <NEL> 0<LF> <NEL>" — the
    # header comment whose line break is the LS (the r283 shape), the
    # more-indented kept-exotic body line at indent 2 (the r317 shape,
    # now with a mid-plain `%` riding as inert content), and the
    # exotic-led normal-indent line behind it. THE TWO NEW DIMENSIONS
    # the r324 tail grammar cannot reach (both MID-BODY, not the EOF
    # tail): (1) the fold-DROPPED realign — libyaml's 1.1 fold branch
    # discards the more-indented line's break yeptris spells literally
    # (the r297 spent-NEL law's shape at an escape-vs-triple site), so
    # a carries ONE surplus escape and the raw LS/PS triple behind it is
    # the SAME break libyaml's READ_LINE kept raw at the blank line —
    # the walk eats a's escape and rejoins byte-bound at the coinciding
    # triple; gated on the full 3-byte coincidence, and verdict-
    # preserving for every existing member (whenever the gate fires,
    # r197's fires too and both leave the walk at the identical state);
    # (2) the EOF fold-space surplus — the r303/r304/r323 lattice
    # completed: the blank-led final line's fold space rides beside the
    # ONE clip escape with NO kept exotic behind (r176 owns the bare
    # escape, r303/r304/r323 demand the NEL pair, the r324 grammar
    # demands >= ONE exotic in the run), claimed only when a's whole
    # remainder is exactly SP + escape and the r161 strcmp binds b's
    # remainder. THE A/B: libfyaml 04e0b58 ERRORS on both floor and
    # artifact ("missing document start" at the final blank-led line —
    # a third reading), so the r318-class dual-engine consensus law
    # does NOT carry this composition; libyaml 90a56d4 reads the 1.1
    # split; yeptris keeps the exotics raw per the 1.2 whole-route —
    # comparator carry only, no engine change (the r317 precedent).
    # The artifact's full path rides the r299 twin-trail, the new
    # realign arm, the r323 blank-run interleave and the r324 walk-end
    # grammar; deeper mid-body plain permutations stay declined — the
    # grammar boundary this round documents.
    # Floor: 24 of 35 bytes survive the dual bar (greedy single-byte
    # ddmin; pre-patch DIFFER exit 77 + post-patch carry exit 0 on
    # BOTH the floor and the full artifact; every single-byte deletion
    # of the floor loses the dual bar — the r320 ultra-minimal form).
    # The 50-mutant paired battery (30 single + 20 double, seed 325):
    # see the round issue. Both the floor and the full artifact ride
    # the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r325-folded-header-family-realign
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-realign-r325.bin
    )
    add_test(
        NAME fuzz-diff-r325-folded-header-family-realign-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-realign-r325-artifact.bin
    )

    # r326 (run 44624659366, main 51d167d5): the glued-?/# flow-soup
    # family's alias-slot comment ride — the nightly artifact's
    # `?\t*%L #|...` split-explicit-key alias whose post-name blank/break
    # run reaches a blank-preceded `#` instead of a bare boundary. On
    # yeptris the engine's comment condition fires: the run eats
    # inertly to its break and the member boundary derives after it
    # (the floor's `?\t*L #\n,` site — the separator), or a ':' +
    # blank/break re-lexes as the VALUE boundary (the artifact's L3
    # `:\t ,` — the pair's empty value opens, `=ALI *%L` + the inserted
    # empty, identical rendering). On libyaml the member never scans:
    # its law-a `?`-KEY + comment-eat swallows the whole line tail
    # INCLUDING the alias (the artifact's `*%L` rides the `?##|#|`
    # member's eat), so the engine's wider alias-name cut (libyaml's
    # scan_anchor stops at the non-IS_ALPHA `%`, yeptris keeps `%L`
    # per the suite's prop-charset law) never surfaces against it.
    # The soup plains themselves (m1/m2/m5: `?#?#|#|+``, `?##|#|`,
    # `:#|#|+``) ride the law-a comment-eat + law-d plain-? + the
    # glued-colon plain law verbatim; the bare backtick member rides
    # the y-side plain-content walk while libyaml eats it inside the
    # m2 comment. THE A/B: libfyaml 04e0b58 reads the soup plains
    # yeptris's way (members 1-2 parse as scalars byte-identically)
    # and then ERRORS on the bare backtick member ("plain scalar
    # cannot start with '`'" — spec-tight, a third reading); no
    # two-engine consensus exists, so the r318-class consensus law
    # does NOT carry — comparator carry only, no engine change.
    # Floor: 18 of 67 bytes survive the dual bar (greedy single-byte
    # ddmin; pre-patch DIFFER exit 77 + NOT classified, post-patch
    # carry exit 0 + classified, on BOTH the floor and the full
    # artifact; the ddmin converges to the same floor from both runs —
    # the r320 ultra-minimal form). The 50-mutant paired battery
    # (30 single + 20 double, seed 326): see the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r326-alias-comment-ride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-comment-ride-r326.bin
    )
    add_test(
        NAME fuzz-diff-r326-alias-comment-ride-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-comment-ride-r326-artifact.bin
    )

    # r327 (run 44638379551, main 7c033da3): the folded-header family's
    # SEVENTH composition — the artifact's `> #<PS><PS> <NEL><LF><PS>
    # <NEL> ---` rides the r320/r323 doubled-PS header comment and the
    # r317 kept-NEL body, but its SECOND kept exotic faces libyaml's
    # kept LS/PS raw triple: at the same alignment slot EACH engine's
    # value carries the OTHER exotic's raw bytes (yeptris keeps the NEL
    # pair as 1.2 content, libyaml's READ_LINE LS|PS branch keeps the
    # triple it broke ON — the polarity law's keep-keep composition,
    # the first MID-BODY split past the r325 boundary note). The
    # tail's `---` is NOT the dimension (the byte-exact dumps join it
    # as scalar content on BOTH engines — single =VAL, no -DOC split —
    # and the dual-bar floor drops it to a lone `-` with the divergence
    # intact): the fresh site is the kept-NEL/kept-triple split with
    # libyaml's ONE break-spelling escape (its trailing-breaks join
    # spells the NEL's break as the LF escape) and yeptris's fold-blind
    # interior space behind it. The arm pairs the kept NEL and the kept
    # triple, eats a's spaces (libyaml's eaten indent — the r297 class)
    # and b's EXACTLY ONE escape (the r320 ledger law: one break per
    # site), and requires the plain byte behind both sides to coincide
    # on non-space/non-escape/non-exotic-lead/non-frame-end content (the
    # r323 coincidence law) so the walk rejoins byte-bound and the
    # r176 clip tail's strcmp completes the binding. THE A/B: libyaml
    # 90a56d4 spells the kept triple and the join escapes
    # ("=VAL : <PS>\n\n<PS>\n-"), libfyaml 04e0b58 keeps BOTH exotics
    # raw as one content line ("=VAL ><PS> <NEL> -\n" — 1.2: no exotic
    # break is a break), yeptris keeps the NELs and folds the PS to its
    # normalized LF — the comparators split three ways exactly as the
    # r320/r323 A/B did; comparator carry only, no engine change (the
    # r317 precedent).
    # Floor: 21 of 23 bytes survive the dual bar (greedy ddmin:
    # run-length collapse then single-byte deletion; pre-patch DIFFER
    # exit 77 + NOT classified, post-patch carry exit 0, on BOTH the
    # floor and the full artifact; no single-byte deletion of the floor
    # keeps the dual bar — the r320 ultra-minimal form). The 50-mutant
    # paired battery (30 single + 20 double, seed 327): 94 clean-both,
    # 6 newly-carried, 0 stayed-red, 0 crashes — see the round issue.
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r327-folded-header-family-polarity
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-polarity-r327.bin
    )
    add_test(
        NAME fuzz-diff-r327-folded-header-family-polarity-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-polarity-r327-artifact.bin
    )

    # r328 (run 44655871592, main e4c86cc6): the folded-header family's
    # EIGHTH composition — the artifact's `> #<PS> :<NEL> <PS> :<NEL>
    # ` rides the r283/r320 header-comment law and the r317 kept-NEL
    # body, but its kept exotics sit ADJACENT to the content colons —
    # the SPACING dimension (the r320-r327 carries held line-position
    # exotics; the greedy dual-bar floor GLUES the NEL to the PS
    # triple, a spacing variant every earlier arm declined). The
    # artifact site is esc-free on BOTH sides: libyaml's fold branch
    # SUPPRESSED the fold space at its kept-triple site
    # (trailing_breaks non-empty clears the fold gate — scanner.c), so
    # a reads [NEL][SP][PS][SP]`:`... (yeptris's 1.2 verbatim
    # continuation, the r317 law at mid-body position) where b reads
    # [PS]`:`... (libyaml's READ_LINE LS|PS keep + the suppressed
    # fold). The kept-NEL/kept-triple esc-free arm pairs the two exotics
    # (the r327 polarity law's keep-keep composition, now esc-free),
    # eats a's verbatim continuation run (spaces + kept NELs + kept
    # LS/PS triples, >= ONE exotic — the family signature; the SPACING
    # generalization: the exotic's position within the run is free),
    # and requires the plain byte behind both sides to coincide on
    # non-space/non-escape/non-exotic-lead/non-frame-end content (the
    # r323 coincidence law) so the walk rejoins byte-bound; the tail
    # behind stays owned by the loop's existing laws. The floor's own
    # site is the NEL-site lead-space entry: the kept NEL leads its
    # sub-line BEHIND a verbatim space libyaml consumed as the
    # sub-line's INDENTATION and spent its ONE escape on the NEL itself
    # (a reads [SP][NEL][SP...][clip ESC], b reads [ESC][content SP]) —
    # the r317 fold-accounting gate (r >= L + k) declines there, so the
    # lead-space entry arm eats a's leading spaces (no b counterpart —
    # the eaten indent, the r282/r288 class), pairs the k kept NELs to
    # the FIRST r escapes (r >= k — the r320 ledger law), fires ONLY
    # when the r317 gate FAILED (r < L + k — the fold-space mapping
    # owns r >= L + k), and demands the trailing surplus na >= nb (the
    # branch's own law). THE A/B: libyaml 90a56d4 reads the 1.1 split
    # (artifact value `:<PS>:\n ` — NEL->LF, the PS kept raw
    # suppressing the fold, hand-derived byte-exact against scanner.c),
    # libfyaml 04e0b58 reads the artifact EMPTY (pure 1.2: the PS is no
    # break, the `#` comment eats to the breakless EOF — nobody agrees
    # with anybody), yeptris keeps the whole body verbatim + the clip
    # escape — the comparators split three ways; comparator carry only,
    # no engine change (the r317 precedent).
    # Floor: 18 of 20 bytes survive the dual bar (greedy ddmin,
    # single-byte deletion to a fixed point; pre-patch DIFFER exit 77 +
    # NOT classified, post-patch carry exit 0, on BOTH the floor and
    # the full artifact; no further single-byte deletion keeps the dual
    # bar — the r320 ultra-minimal form). The 50-mutant paired battery
    # (30 single + 20 double, seed 328): 49 clean-both, 1
    # newly-carried, 0 stayed-red, 0 crashes — see the round issue.
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r328-folded-header-family-spacing
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-spacing-r328.bin
    )
    add_test(
        NAME fuzz-diff-r328-folded-header-family-spacing-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-spacing-r328-artifact.bin
    )

    # r329 (run 44667118830, main 7d252607): the folded-header family's
    # NINTH composition and the first CROSS-FAMILY one — the kept-NEL
    # delta rides in one document while the LS/PS delta rides in another,
    # so every existing single-family binding dies. The artifact opens
    # `> #<PS><PS> <NEL><LF>---` (the r320 doubled-PS header comment, the
    # kept-NEL body line) with the whole empty-or-newline-only-quoted-
    # strings testsuite seed spliced BEHIND the block scalar: yeptris's
    # doc 1 keeps `ESC NEL ESC` (the whole-route normalizer rewrites the
    # comment's PS pair to LFs — the comment splits, the kept NEL becomes
    # the body's one content line, the r317/r320 family root) where both
    # comparators read EMPTY (libyaml: the NEL is a 1.1 break and the clip
    # chomps it; libfyaml 04e0b58: pure 1.2, the comment eats to the LF
    # and the marker bounds the doc — the r321 A/B's polarity), and doc 2
    # diverges at its own LS/PS site: the emit block's `e: '<PS> <NEL>...'
    # line keeps the space AFTER the raw PS in yeptris where libyaml
    # strips it (the ps-space law) while libfyaml keeps PS AND space AND
    # NEL raw (a three-way split, the r320/r327 polarities). The 22-byte
    # dual-bar floor `> #<PS><PS> <NEL><LF>---<LF>S<PS>'` carries the
    # plain sibling: doc 2 is `S<PS>'` where yeptris's plain folds the PS
    # to the ONE space (the round-56 plain law) and BOTH comparators keep
    # the raw triple. The stage (inside the r220 NEL-carpet arm, behind
    # its exact-strcmp stage): when the carpet is PURE to libyaml (the
    # same strcmp(ldump3, ldump) gate — its 1.1 reader breaks at the NEL
    # either way), the residual pair (yeptris's dump of the carpet,
    # libyaml's ORIGINAL dump) must ride one of the two documented LS/PS
    # dump walks — the ps-space walk (the artifact's residual) or the
    # plain fold walk (the floor's residual) — both byte-bound, both
    # demanding diffs > 0 at every admitted site. Comparator carry only,
    # no engine change (the r317 precedent).
    # Floor: 22 of 924 bytes survive the dual bar (greedy ddmin to a
    # fixed point; pre-patch DIFFER exit 77 + NOT classified, post-patch
    # carry exit 0, on BOTH the floor and the full artifact, plain and
    # _lf). The 50-mutant paired battery (30 single + 20 double, seed
    # 329): 49 clean-both / 0 newly-carried / 1 declined / 0 crashes —
    # the declined mutant (`>` + LF + `#<PS><PS> <NEL>` + the same tail,
    # space→LF at position 1) composes THREE families (the root-block
    # content law's `#` body line x the kept-NEL carpet x the plain
    # LS/PS fold — a deeper chain the r325 grammar-boundary precedent
    # documents as declined); see the round issue.
    add_test(
        NAME fuzz-diff-r329-folded-header-family-crossfamily
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-crossfamily-r329.bin
    )
    add_test(
        NAME fuzz-diff-r329-folded-header-family-crossfamily-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-family-crossfamily-r329-artifact.bin
    )

    # r330 (run 44682066625, main 22dd1700): the r238 root-anchor-
    # quoted-follow family's first WIDENED member — the artifact
    # `&2?*: '<PS><CR>'` (12 bytes) glues the '?'/'*' INTO the anchor
    # name ahead of the riding ':': yeptris keeps the whole `2?*:`
    # (1.2 ns-anchor-char, the 2SXE/W5VH/Y2GN suite law — libyaml is
    # behind the suite, the r311 law) and reads ONE root anchored
    # scalar whose body is the quoted `<PS><CR>` (the PS kept raw, the
    # CR folded to the one space — the r238 y-walk), while libyaml's
    # 1.1 IS_ALPHA cut lands at the FIRST out-of-charset byte (the
    # '?'), the cut residual `?*` re-lexes as the key's PLAIN content
    # (the plain-'?' law — '?' glued to a non-blank is content, the
    # r199 law), the freed ': ' fires the VALUE token, and the quoted
    # scalar parses as the map value (`+MAP / =VAL : &2 ?* / =VAL :
    # <PS>\n` — the PS kept raw, the CR normalized to the kept LF, no
    # fold space — the r238 l-walk's blank-state quark). The fresh
    # territory is the QUOTED-BODY exotic dimension (the r329
    # crossover): every pre-r330 member of the shape (the ASCII-body
    # `&2?*: 'a'`, the PS-only body, the no-quote tails) is carried by
    # the anchor-name-charset arm, because its split copy (`&2 ?*:
    # '...'`) agrees there — but the split copy is YEPTRIS-REJECTED
    # under the PS/CR body ("wrongly indented single-quoted scalar":
    # the PS is a 1.2 line break and the map-value continuation sits
    # at column 0 — the indent law), so the charset arm's verify leg 1
    # dies and the composition reds. THE A/B (probed): libfyaml
    # 04e0b58 reads yeptris's artifact stream byte-exact (SCALAR
    # anchor `2?*:` value `<PS>+<SP>` on both), and rejects the split
    # copy IDENTICALLY to yeptris (the same indent error) where
    # libyaml accepts — comparator carry only, no engine change (the
    # r318 precedent). THE CARRY: the r238 arm's riding-name set gains
    # the glued '?'/'*' and its libyaml model cuts at the first
    # out-of-charset byte with the residual (cut..colon) rendered as
    # the key's plain content — a leading '?' then the plain-safe run
    # ([A-Za-z0-9_-?*]; a ':' inside would re-lex the VALUE boundary,
    # a '*' lead would re-lex as an ALIAS — both stay out, unmeasured
    # shapes); every existing r238 member keeps its exact path (their
    # names cut AT the riding ':', the anchored-empty-key arm).
    # Floor: 11 of 12 bytes survive the dual bar (greedy ddmin,
    # single-byte deletion to a fixed point; pre-patch DIFFER exit 77
    # + NOT classified, post-patch carry exit 0, on BOTH the floor
    # `&2?: '<PS><CR>'` and the full artifact; no further single-byte
    # deletion keeps the dual bar — the r320 ultra-minimal form). The
    # 50-mutant paired battery (30 single + 20 double, seed 330): see
    # the round issue. Both the floor and the full artifact ride the
    # corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r330-root-anchor-quoted-follow-qsplat
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-qsplat-r330.bin
    )
    add_test(
        NAME fuzz-diff-r330-root-anchor-quoted-follow-qsplat-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-qsplat-r330-artifact.bin
    )

    # r356 (run 45023668328, main 73f5b517): the r330 family's next
    # mutant-of-own-pin red — the nightly's artifact
    # `&2?*&: '<PS><CR>'` (13 bytes) is ONE InsertByte from the r330
    # artifact pin `&2?*: '<PS><CR>'`: the dump-vocabulary '&' grown
    # before the riding ':'. yeptris glues the '&' INTO the anchor name
    # (`=VAL : &2?*&: <PS>+<SP>` — the prop charset walk, '&' a 1.2
    # ns-anchor-char), libyaml cuts at the FIRST out-of-charset byte
    # (the '?') and the residual `?*&` re-lexes as the key's plain
    # content with the '&' GLUING MID-PLAIN (a non-leading indicator is
    # ns-plain content in 1.1 — the r331 battery's `3&2`/`o&2` faces
    # and the r346 `:&:` face own the plain-key slot's copy of that
    # law; never the head: "the r238 arm owns every anchor-entry
    # shape"), and the freed ': ' fires the VALUE token exactly as the
    # r330 members. The A/B (probed, DOM level): libfyaml 04e0b58 reads
    # yeptris's structure BYTE-EXACT on BOTH modes — root SCALAR,
    # anchor `<2?*&:>` riding (its 1.2 value `e280a9` = the raw PS, the
    # trailing CR folded away at the scalar end; its 1.1 value empty,
    # the PS itself a 1.1 break) — NEVER libyaml's map; comparator
    # carry only, no engine change (the r318/r330/r331/r332/r336/r346
    # precedent). THE CARRY: the r238 arm's riding-name set gains the
    # '&', MID-name only (a name-head '&' is libyaml's scan_anchor
    # ERROR shape, unmeasured — declined), and the residual walk admits
    # the '&' mid-run with the lead staying the pinned '?' (an '&'-lead
    # residual would re-lex as the key's own ANCHOR — the `&2&: ...`
    # twin probed shape-dead; a ':' inside still re-lexes the VALUE
    # boundary, a '*' lead still re-lexes as an ALIAS). Floor: the
    # 12-byte `&2?&: '<PS><CR>'` (greedy single-byte ddmin: the '*'
    # dies into the already-widened shape; the '&' deletion is the
    # CLASSIFIED r330 seed exit 0; the PS/CR deletions land on the
    # classified single-break bodies exit 0; the '?' deletion dies the
    # shape bar; no other deletion keeps the dual bar). The 50-mutant
    # paired battery (seed 356, 20 flips + 10 deletions + 20 doubles
    # per shape): 0 regressions over 100 paired runs — every verdict
    # class identical pre/post except the widening's own differ→carry
    # turns (4 mutants); 41 stable-reds recorded (red pre AND post —
    # the pre-existing unclaimed faces; see the round issue). Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r356-root-anchor-quoted-follow-amp
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-amp-r356.bin
    )
    add_test(
        NAME fuzz-diff-r356-root-anchor-quoted-follow-amp-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-amp-r356-artifact.bin
    )

    # r366 (run 45125049264, main 79e66828): the family's THIRD
    # mutant-of-own-pin red — the nightly's floor
    # `&2?*: '<PS><CR>\'` (13 bytes) is ONE InsertByte from the r330
    # artifact pin `&2?*: '<PS><CR>'`: the dump-vocabulary backslash
    # grown INSIDE the single-quoted body (the full nightly artifact
    # is the 100-byte tail-content form `&2?*: '<PS><CR>"\u1: ...:::'`,
    # the same body backslash plus riding content). A single-quoted
    # scalar has no escapes (1.2 8.1.2): the `\` rides BOTH engines'
    # values as content and both dumps spell it `\\` through the
    # shared yd_escape_append table — the r238 arm's byte-equal
    # content branch stranded the second `\` before the tail sentinel
    # and the strict walks declined their own family. THE WIDENING:
    # both body walkers gain the backslash branch (one dump `\\` pair
    # per body `\`, fold/exotic state cleared with it). The structure
    # face is unchanged — the r330 law (2SXE/W5VH/Y2GN pin the
    # ns-anchor-char riding name `2?*:`; libyaml cuts at the first
    # out-of-charset byte, its IS_ALPHA 1.1 set; libfyaml 04e0b58
    # reads yeptris byte-exact on BOTH modes, probed again on the
    # r366 floor: `=VAL &2?*: '\P \\` 1.2). Comparator carry only, no
    # engine change (the r318/r330/r331/r332/r336/r346/r356
    # precedent). Floor: the 13-byte `&2?*: '<PS><CR>\'` (greedy
    # single-byte ddmin fixed point pre-patch: DIFFER exit 77 on the
    # floor alone, every deletion classified exit 0 — INCLUDING the
    # '*' deletion, which drops into the r330 floor shape the arm
    # already carries). The 50-mutant paired battery (seed 366, 20
    # flips + 10 deletions + 20 doubles per shape): 0 regressions
    # over 100 paired runs; every 77->0 turn rides a
    # body-backslash-retaining family shape; stable-reds stay stable.
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r366-root-anchor-quoted-follow-bslash
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-bslash-r366.bin
    )
    add_test(
        NAME fuzz-diff-r366-root-anchor-quoted-follow-bslash-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-bslash-r366-artifact.bin
    )

    # r367 (run 45136829807, main 1340927a): the root-block-content
    # family's EXOTIC-BREAK composition — the nightly's 23-byte
    # artifact `>\n#mL> <NEL>\n# :<NEL><PS> <NEL>  ` rides the r367
    # floor `>\n#<PS>` (6 bytes, greedy ddmin fixed point: every
    # deletion classified pre-patch except the LS/PS/NEL carriers and
    # the `#` line itself). The floor splits exactly like the family's
    # LF twin — yeptris takes the column-0 `#` line as the block
    # scalar's body (the suite pins the root reading: M7A3 spec
    # example 9.3 and DWX9 spec example 8.8 take the root literal's
    # col-0 line as content; libyaml 90a56d4's unconditional
    # min-indent-1 clamp is the ledgered upstream deviation; libfyaml
    # 04e0b58 reads the floor faces byte-exact as yeptris in BOTH
    # modes, `=VAL >c\n` / `=VAL >#c\n`, while PyYAML 6.0.2 sides with
    # the clamp — the r317 version-line split) — but the LS/PS
    # spelling defeats the arm's byte-equality gate: the indented
    # copy's re-dump pair carries the adjudicated #638 exotic deltas
    # (yeptris folds LS/PS to LF and keeps blank-run NELs as 1.2
    # content; libyaml keeps LS/PS raw per READ_LINE LS|PS->LS|PS and
    # breaks at every NEL). THE WIDENING: when the input carries a raw
    # exotic break, the arm also binds the COMPOSED claim — the
    # yd_composed_break_divergence move: rebuild the input with the
    # exotic spellings normalized to LF on top of the col-0 indent and
    # require BOTH engines to agree on that copy, with the same
    # breakless-EOF clip residual the clean gate already composes (the
    # only delta the nightly's rebuild leaves). The claim stays
    # byte-bounded: any divergence not carried by the two transform
    # families survives the rebuild and keeps the pair red. Comparator
    # carry only, no engine change (the r317/r318/r330 precedent).
    # The 50-mutant paired battery (seed 367: the floor + the full
    # artifact, 20 flips + 10 deletions + 20 doubles per shape; plus
    # 300 paired runs over the r364/r365/r366 pins): 0 regressions;
    # the 8 turns all ride artifact exotic positions; the standing
    # pins' shapes stay exactly as the pre-patch harness left them.
    add_test(
        NAME fuzz-diff-r367-root-block-content-exotic
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-block-content-exotic-r367.bin
    )
    add_test(
        NAME fuzz-diff-r367-root-block-content-exotic-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-block-content-exotic-r367-artifact.bin
    )

    # r368 (run 45152658419, main 26a6ade2): the r366 qsplat head
    # composed with the CLAMP-OVERFLOW body — the nightly's 10,890-byte
    # artifact `&2?*: '<PS><CR>"\u1: ...\n-f &"": [{"":[{"":...{U:ba:
    # \n- 4-ba:I::*:::::'` grows the r366 head a ~10.8KB single-quoted
    # body: 2157 `[{"":[` map-in-seq chain units plus a `U:ba:\n- 4-ba:
    # I::*:::::` tail whose closing `:'` sits at the LAST byte, so the
    # whole ~4300-deep chain rides INSIDE the scalar as content and
    # NEITHER engine's depth wall fires (probed: the chain alone is
    # BOTH-ERROR — both engines reject the ~4300-deep flow chain at
    # their own limits, the r354 bracket-LEAD error-equality law; the
    # head alone is BOTH-ERROR too — the unterminated quote). Both
    # dumps stay the r330 structure (yeptris ONE anchored scalar, raw
    # PS + CR fold exactly the r231/r366 law; libyaml the anchor-`2`
    # map with the `\n` blank-state escape) — but the shared dump
    # vocabulary's per-event line budget (char line[4096]) cuts BOTH
    # scalar lines at 4095 chars (ydump 4105 total, ldump 4128:
    # measured), the strict byte walks ran off the visible region ~4KB
    # into the body, and the arm declined its own family. THE WIDENING:
    # the walkers take the caller-computed visible region end; past
    # the cut the body rides the clamp-tail law (forced-equal units
    # only — printables byte-equal, mid-body space runs, lone clean
    # CR/LF; tabs/backslashes/exotics/trailing-run-end decline as
    # unmeasured past the cut). Clamp detection is EXACT-LENGTH
    # (region 4082/4081 y, 4088/4087 l — the 4095-char cut line minus
    # the render prefix), so a tamper-truncated dump loses the gate
    # and declines (the firewall holds). Boundary probed: the 4088-byte
    # twin (body escaped 4081) still walks to its sentinel and
    # classifies pre-patch; 4091 bytes DIFFER. Floor: the 4090-byte
    # size-bound clamp-boundary form `&2?*: '"":[{"":[...{U:ba:\n-
    # 4-ba:I::*:::::'` (greedy single-byte ddmin to the clamp boundary:
    # the DIFFER band ends exactly at the clamp-overflow length, the
    # size IS the finding — the r361 size-bound floor precedent). The
    # 50-mutant paired battery (seed 368, 20 flips + 10 deletions +
    # 20 doubles per shape): 0 regressions; every 77->0 turn rides the
    # clamp-overflow family shape; stable-reds stay stable. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    # Comparator carry only, no engine change (the r318/r330/r331/
    # r332/r336/r346/r356/r366 precedent).
    add_test(
        NAME fuzz-diff-r368-qflow-depth-clamp
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-depth-clamp-r368.bin
    )
    add_test(
        NAME fuzz-diff-r368-qflow-depth-clamp-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-depth-clamp-r368-artifact.bin
    )

    # r369 (run 45180416420, main 031e326e): the r368 pin's THIRD
    # mutant-of-own-pin red — the artifact is the r368 pin byte-for-byte
    # with a 36-byte dump-vocabulary text block inserted at offset 1048,
    # inside the quoted body's visible region:
    # `+STR\n=VAL : :\\n<NEL> 5%<PS> :\\n<NEL> 5%<PS>` (literal
    # backslash-n pairs riding the r366 escape law, two raw NELs, two
    # raw PS). The r238 walkers declined at the NEL units — both
    # modeled a RAW-NEL dump spelling no engine path produces.
    # THE ADJUDICATION: suite-first, the suite carries ZERO raw
    # NEL/LS/PS rows (scanned da267a5c: no .in.yaml/.test file
    # contains the bytes), the JSONTestSuite y_string_u+2028/2029 rows
    # pin raw LS/PS as VALID content inside double quotes (satisfied
    # by both engines), so the face resolves on the engines' own
    # pinned laws — yeptris's #638/#839 normalizer (NEL folds
    # EVERYWHERE including quoted regions; LS/PS stay raw content
    # there), libyaml's READ_LINE (NEL -> LF; LS|PS -> raw) with the
    # fold gate `leading_break[0]=='\n'`. THE WIDENING (all units
    # probed at engine level, 8 shapes): NEL is a BREAK on both
    # engines — the clean NEL folds to the one-space spelling on both
    # (`<NEL>a` reads ` a`); the space run strips before it (`<SP>
    # <NEL>a` reads ` a`) and a just-folded break eats the following
    # run (`<NEL><SP>a` reads ` a`); behind a kept-raw PS the faces
    # split exactly on the pinned normalizer law — yeptris rides the
    # blank as content (`<PS><SP>a` reads `<PS> a`), libyaml's
    # blank-state swallows it (`<PS>a`, the r332 law re-proven), and a
    # clean NEL directly before a kept-raw LS/PS renders NOTHING on
    # libyaml (the '\n' is the leading break, the raw triple the
    # trailing one: `<NEL><PS>a` reads `<PS>a`) against yeptris's
    # fold-plus-raw (`<PS> a`). Floor: the 16-byte form
    # `&2: '<PS> :<NEL><PS>'` (greedy ddmin, 285 candidates — both
    # faces of the law pair in one scalar). The 50-mutant paired
    # battery (seed 369, replace/unit/insert/delete palette per
    # shape): 0 regressions; stable-reds stay stable. Comparator carry
    # only, no engine change (the r318/r330/r331/r332/r336/r346/r356/
    # r366/r368 precedent).
    add_test(
        NAME fuzz-diff-r369-nel-blank-state
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-nel-blank-r369.bin
    )
    add_test(
        NAME fuzz-diff-r369-nel-blank-state-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-nel-blank-r369-artifact.bin
    )

    # r371 (run 45217557947, main 783f203a): the nested `[?::]`
    # flow-soup member — the nightly's artifact `[?::[?::JJ]]` (12
    # bytes, crash-eb8e447ef01cc0af943507fa7c9729e9580e86f3) is a
    # NESTED flow-seq composition of the `?::` explicit-key face (the
    # r272 [?::] model's namesake family): inner `[?::JJ]`, `JJ` tail.
    # The r272-era model covered the single-level glued-`?` member
    # (plain/empty value slots), the r281 widening took the value-slot
    # `{` MAP and the r314 law the member-slot `[` — the value-slot `[`
    # stayed declined as an untested shape, and the artifact measures
    # exactly that slot. THE LAW (the y-side widening): the glued-`?`
    # KEY-implicit member's VALUE slot admits the nested flow SEQUENCE
    # — the r281 nested-map arm's SEQ twin (the same member laws derive
    # its content recursively; +SEQ/-SEQ stashed on the node, the
    # caller renders the key line then the stash). The inner `?::JJ`
    # glues the `?` into the plain (law d) — its colons ride as
    # content, neither a blank/break nor a flow indicator following, so
    # no VALUE boundary fires; yeptris: `+MAP / =VAL : ?: / +SEQ /
    # =VAL : ?::JJ / -SEQ / -MAP`. THE LAW (the l-side, already
    # standing): the 1.1 explicit-KEY law through the nested level —
    # the member-start `?` fires the KEY token (the single-pair +MAP
    # wrapper), the key slot empty on the VALUE at the mapping-key
    # state (libyaml's flow_sequence_entry_mapping_key CONSUMES the
    # VALUE token and inserts the empty scalar), and the SECOND VALUE
    # is consumed by the mapping-value state with the node after it as
    # the pair's value (the scanner's fetch_value emits VALUE in flow
    # unconditionally — the complex-key arm); libyaml: `+SEQ / +MAP /
    # =VAL :  / +SEQ / +MAP / =VAL :  / =VAL : JJ / -MAP / -SEQ /
    # -MAP / -SEQ`. The A/B (probed, JSON mode byte-exact): libfyaml
    # 04e0b58 reads yeptris's artifact stream byte-exact (`{"?:":
    # ["?::JJ"]}` — the outer pair key `?:` riding, the inner seq's
    # plain `?::JJ`) in BOTH --yaml-1.2 and --yaml-1.1 passes — the
    # comparators split along the libyaml-compat bar. Comparator carry
    # only, no engine change (the r318/r330/r331/r332/r336/r346/r356/
    # r366/r368/r369/r370 precedent). FLOOR: the greedy ddmin fixed
    # point AT the artifact — every chunk (6/3-byte) and single-byte
    # deletion (18 probes) loses the dual+shape bar (the nested
    # composition fully load-bearing: the outer wrapper, the `?::`
    # head, the inner `[`, the double colon — libyaml's second VALUE —
    # and the `JJ` pair value), so ONE pin carries the bytes (floor ==
    # artifact). THE COMPOSITION ROW (measured, the r361 lesson): the
    # artifact and the deeper nestings `[?::[[?::JJ]]]` /
    # `[?::[?::[?::JJ]]]` classifier-claimed; the sibling faces
    # `[?::[?::]]`, `[?::[?::J]]`, `[?::[JJ]]`, `[?::[::JJ]]`,
    # `[?::[?::JJ,JJ]]`, `[?::[?::JJ],[?::KK]]`, the MAP-root twin
    # `{?::[?::JJ]}`, the r281 value-slot map twin `[?::{?::JJ}]`, the
    # single-colon head `[?:[?::JJ]]`, and the triple-colon heads
    # `[?::[?:::JJ]]` / `[?::::[?::JJ]]` are engine-EQUAL (never a
    # DIFFER, never consulted); the unclosed / extra-closer /
    # `[?::[?::JJ]x]` faces are BOTH-ERROR — never consulted. The
    # 50-mutant paired battery (seed 371): 0 regressions (6
    # newly-carried, 44 stable-carried); the standing
    # r330/r366/r367/r368/r369/r370 pins byte-identical; the
    # classifier self-test green (124 seeds, 6651 dump-tamper declines,
    # 803 mutants, 0 failures). THE TREADMILL: no workflow snapshot
    # change — the comparator snapshot's flow-qhash-nest exclusion
    # already covers this family (the r370-era list); the pin stays
    # under the full ctest.
    add_test(
        NAME fuzz-diff-r371-flow-qhash-nest
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-nest-r371.bin
    )

    # r374 (run 45259544033, main 84150a76): the `\uXXXX` escape joins
    # the classifier. The nightly's artifact
    # `[1,?#,"\rg61クリス",?g?,""]` (38 bytes,
    # crash-c50fa796bab07be63af8816fc6e0d4af8b3f3f8b) is a ROOT flow
    # sequence whose DIFFER decomposes into TWO faces: (1) the `?#`
    # member — libyaml's 1.1 unconditional flow KEY-token law fires the
    # KEY on the `?`, the `#` sits at a fresh token position so the
    # comment-eat runs to the RAW CR (the input's only break), eating
    # `,"\` and landing the DQ span's tail on the new line, where
    # `g61クリス"` re-lexes as the KEY's PLAIN content
    # (literal backslash-u bytes, the closing quote glued in — plains
    # carry quotes mid-text) and the parser binds the implicit
    # single-pair map — the law-a comment-eat + post-break rebinding
    # the r244-era model already derives; yeptris reads TWO members
    # (`?#` flat by the plain-`?` law, the DQ span folded+decoded by
    # the 1.2 laws); (2) the `?g?` member — the pinned plain-`?` law-d
    # split (libyaml MAPs it, yeptris keeps the flat plain). The
    # residual red was NEITHER face: the classifier's blanket escape
    # decline (the r337 arm admitted only the escaped-break and `\n`
    # followers) could not derive the artifact's THREE `\uXXXX`
    # members-wide escapes, so any DIFFER sharing a seq with an
    # escape-bearing member stayed red even when the escape member
    # itself renders byte-identical on both engines (the floor's
    # literal-katakana twin `[1,"ク",?g?,""]` classified all along).
    # THE LAW (the 21-row matrix, measured this session): yeptris and
    # libyaml decode a well-formed `\uXXXX` IDENTICALLY — mid-content,
    # chained, at the lead, at the close edge, across the fold
    # compositions (`["a<CR>クb"]`); the decoded codepoint rides the
    # dump escape (`A`, the decoded backslash, LF, C0, DEL, the C1 NEL
    # byte, NBSP, the decoded LS, U+FFFD, U+FFFE — all EQUAL); the
    # decline domain is BOTH-ERROR, never a consult (surrogate halves,
    # short hex, a bad hex digit). The widening takes the `\u` follower
    # in BOTH quoted-span walkers (fc_quoted_value ahead of the r337
    # tight gates — ITS pending-state compositions are measured; the
    # libyaml-side l_dq_scan twin mirrors the same admission), the
    # pending fold run / blank run flushes exactly as a content byte
    # does, a surrogate declines (cheap), and every other escape
    # follower stays out. THE A/B (probed, the fyprobe scalars):
    # libfyaml 04e0b58 reads floor and artifact BYTE-EXACT yeptris's
    # way in BOTH 1.2 and 1.1 modes (flat `?#`, ` g61クリス` with the
    # CR folded to the leading space and the escapes decoded, flat
    # `?g?`) — the comparators split along the libyaml-compat bar,
    # engine defect REFUTED (the r318/r330/r331/r332/r336/r346/r356/
    # r366/r368/r369/r370/r371 precedent). Comparator carry only, no
    # engine change. FLOOR: the greedy ddmin fixed point
    # `[1,"ク",?g?,""]` (19 bytes, hex 5b312c225c7533306166222c
    # 3f673f2c22225d) — the `?#`+CR fused face dropped out (its own
    # derivation was already sound), the escape spelling stayed
    # load-bearing against the classifier only. RED pre-patch on both
    # harnesses (lf exit 77, standalone abort 134); CLASSIFIED
    # post-patch (floor + artifact, both harnesses). The 50-mutant
    # paired battery (seed 374): 0 regressions (11 newly-carried, 26
    # stable-green, 13 stable-red); the standing r2xx/r3xx pins
    # verdict-identical pre/post (347 files, 0 divergences); the
    # classifier self-test green (126 seeds, 6759 dump-tamper
    # declines, 814 mutants, 0 failures).
    add_test(
        NAME fuzz-diff-r374-flow-u-escape
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-u-escape-r374.bin
    )
    add_test(
        NAME fuzz-diff-r374-flow-u-escape-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-u-escape-r374-artifact.bin
    )

    # r375 (run 45281713143, main 4e6f0e78): the tag-property lead joins
    # the classifier. The nightly's artifact `!\n[?+<LS>:#,?]` (13 bytes,
    # crash-e9eb1b947b10bfff4113afb851d18234c27ca0f4, MS:
    # 1 PersAutoDict- DE: "[?+\342\200\250:#,?]" — a persistent
    # auto-dictionary entry, not derived from any pinned seed) is a
    # TAG-INDICATOR line lead (`!` + LF) ahead of a ROOT flow sequence
    # whose first member is the GLUED `?+` plain carrying a RAW LS
    # (U+2028) mid-member followed by a GLUED `:#`, then a comma and a
    # trailing `?]`. The DIFFER: yeptris renders `+SEQ <!>` then ONE
    # FLAT plain member `?+ :#` (the LS folded to the fold space, the
    # glued `#` riding content per the plain law) followed by the
    # parser's inserted empty-`?`-member map — total ONE map; libyaml
    # renders `+SEQ <!>` (the tag rides identically) then an IMPLICIT
    # single-pair map FIRST (key `+ :#` — the 1.1 unconditional flow
    # KEY-token law with the LS folded the same way — value empty), then
    # a SECOND `+MAP` for the trailing `?` — total TWO maps. The
    # member split is the PINNED plain-`?` law-d (libfyaml 04e0b58
    # reads the member byte-exact yeptris's way, `?+\L:#` one plain
    # scalar with the LS kept raw, in BOTH strict-1.2 and 1.1 — the
    # comparators split along the libyaml-compat bar, engine defect
    # REFUTED, the r318/.../r374 precedent); the NEW face was the
    # LEAD: the flow-rooted gate declined every non-bracket head, so
    # the whole tag-led composition stayed red even though the lead
    # itself is an AGREEING composition. THE LAW (the probe matrix,
    # measured this session): a bare `!` tag at offset 0 followed by a
    # blank/break run and then the root bracket rides the ROOT header
    # `<!>` byte-equal on both engines across LF, blank, CRLF,
    # blank+LF and LF+LF runs, seq AND map roots (the collection's tag
    # never "resolves by style" — only the scalar's empty render drops
    # it, the r335 law). Gates: the bare `!` EXACTLY (a `!!` handle
    # lead is a YEPTRIS-ONLY-ACCEPT split — yeptris renders
    # `<tag:yaml.org,2002:>`, libyaml errors the parse; a `!name`/
    # `!<verbatim>` lead carries another resolved tag and stays out),
    # the run NON-EMPTY (a GLUED `![` is a YEPTRIS-ONLY-ACCEPT split —
    # libyaml errors the tag-end), no comment inside the run, and the
    # bracket DIRECTLY after the run. FLOOR: the greedy ddmin fixed
    # point IS the artifact (13 bytes, hex 210a5b3f2be280a83a232c3f5d)
    # — every byte load-bearing under the dual bar + shape bar (the
    # tag lead, the LS, the glued `:#`, the `?` heads, the trailing
    # `,?` all drop the DIFFER or flip the shape alone; LS->PS
    # reproduces the byte-exact pair, LS->SP/LF/CR resolves it). RED
    # pre-patch on the lf harness (exit 77, the map-count split);
    # CLASSIFIED post-patch. The 50-mutant paired battery (30 single +
    # 20 double per target, seed 375): 0 unexplained regressions
    # (100/100 exit 0). The r373/r374 pins verdict-identical pre/post;
    # the classifier self-test green (126 seeds, 6759 dump-tamper
    # declines, 814 mutants, 0 failures). The seed joins the
    # flow-qsoup* comparator-snapshot exclusion (the law-d family is
    # mutation-dense; the classify self-test carries the bytes under
    # the full ctest).
    add_test(
        NAME fuzz-diff-r375-taglead-flow-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-taglead-r375.bin
    )

    # r373 (run 45244905519, main f1019048): the PS/NEL-dense block
    # break-run split. The nightly's artifact `> #<PS> #<PS><PS>
    # <NEL>\n<PS> <NEL> -` (26 bytes, CopyPart off 0cef14c6) DIFFERs in
    # the block-scalar break-run rendering: yeptris `=VAL : #\n<NEL>\n
    # <NEL> -\n` vs libyaml `=VAL : #<PS><PS>\n\n<PS>\n-`. The
    # hand-off's header-termination guess (the PS/NEL PLAIN-fold
    # space-retention face) died on the probes: the r171 carry already
    # ends yeptris's header LINE at any break spelling (the header
    # boundary converges — the `#` rides content on both sides) and the
    # r372 carry spells the fold's line ends — the residual is purely
    # the CONTENT-line break-run rendering. THE LAW (the break-set bar
    # both engines ride): YAML 1.2.2 (yaml-spec main, Line Break
    # Characters) defines b-char ::= x0A | x0D ONLY — the spec's own
    # note: "Note that these include the non-ASCII line breaks: next
    # line (x85), line separator (x2028) and paragraph separator
    # (x2029). [YAML 1.1] did support the above non-ASCII line break
    # characters; ... YAML treats them as non-break characters as of
    # version 1.2." yeptris's #638/#839 walk keeps the raw spellings as
    # 1.2 content in block-content lines (the r171 carry ends only the
    # header line at any spelling; the r220 NEL-carpet law keeps
    # content-line NELs first-class; the r372 fold-line crossing spells
    # the fold's line ends) while libyaml 90a56d4 breaks at every
    # spelling (IS_BREAKZ) and renders per READ_LINE (NEL -> LF; LS|PS
    # -> raw), its ledgered 1.1 posture. The suite carries ZERO raw
    # NEL/LS/PS rows (scanned da267a5c), so the comparator splits along
    # the compat bar (the r335/r317/r369 precedent); libfyaml 04e0b58
    # confirms the version line on the floor `> #<PS> #<PS><PS> <NEL>\n -`:
    # 1.2 mode sides with yeptris's family (`-\n` — the `#` runs ride the
    # header comment, no `#` content), 1.1 mode sides with libyaml's
    # (`#  \n\n-\n` — the first PS ended the header and the second `#` is
    # content). THE WIDENING (the r367 composed-claim move generalized
    # off the root-content face): the new yd_exotic_rebuild_divergence
    # arm — when a DIFFER's only fresh ingredient is the raw spelling,
    # normalize every NEL/LS/PS to LF in a copy, re-classify BOTH
    # engines on that copy, and claim the pair only when the copy
    # agrees byte-for-byte or rides exactly the breakless-EOF clip
    # residual (the r140/L24T#2 law). FLOOR: the 20-byte greedy ddmin
    # fixed point `> #<PS> #<PS><PS> <NEL>\n -` (hex 3e 20 23 e2 80 a9
    # 20 23 e2 80 a9 e2 80 a9 20 c2 85 0a 20 2d) — red pre-patch on
    # both harnesses (lf exit 77, standalone abort), its LF rebuild
    # riding yd_clip_breakless_divergence exactly. The claim stays
    # byte-bounded: a divergence the rebuild does not carry survives
    # and keeps the pair red. Comparator carry only, no engine change
    # (the r318/r330/r331/r332/r336/r346/r356/r366/r367/r369
    # precedent).
    add_test(
        NAME fuzz-diff-r373-block-exotic-rebuild
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-exotic-spelling-rebuild-r373.bin
    )
    add_test(
        NAME fuzz-diff-r373-block-exotic-rebuild-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-exotic-spelling-rebuild-r373-artifact.bin
    )

    # r370 (run 45200249484, main fff9640f): the r366 pin's mutant-of-
    # own-pin red riding the qsplat HEAD-CHARSET — the nightly's
    # artifact `&2?/: '<PS><CR>\'` (13 bytes) is the pinned r366 floor
    # `&2?*: '<PS><CR>\'` with exactly ONE ChangeByte at offset 3,
    # `*` (0x2a) -> `/` (0x2f). The r366/r368/r369 body widenings all
    # hold (the `<PS><CR>\` units match byte-exact); only the HEAD
    # spelling declines — the r238 arm's riding-name walk stopped at
    # the `/`, unmodeled. The law: `/` IS a 1.2.2 ns-anchor-char
    # (ns-anchor-char = ns-char - c-flow-indicator; ns-char excludes
    # only the breaks, so every printable non-break byte outside
    # `,[]{}` is legal in an anchor name — the 2SXE/W5VH suite law
    # pins the colon case), yeptris rides the name `2?/:` (ONE
    # anchored scalar, `=VAL : &2?/: <PS>+<SP> \\`), libyaml cuts at
    # the FIRST out-of-IS_ALPHA byte (the '?') and the residual `?/`
    # re-lexes as the key's plain content exactly as the r330/r356
    # residuals (`+MAP / =VAL : &2 ?/ / =VAL : <PS>\n\\`; `/` glued
    # mid-plain is 1.1 content — libyaml's IS_ALPHA cut is its
    # ledgered 1.1 divergence). THE WIDENING: the head-charset row,
    # measured (the r361 lesson: state a charset row, not a byte
    # list) — every member probed at engine level through BOTH faces
    # (`&2?X: '<PS><CR>\'` composed, `&2X: '<PS><CR>\'` bare): the
    # family face holds for `/` and every printable non-break byte
    # probed (`"` `'` backtick `$` `%` parens `+` `;` `<` `=` `>` `@`
    # `^` `~` `|` `\` `*` `&` `?` alnum, the high bytes U+00E9 /
    # U+00A0 / U+4E2D / U+2027, and the composed runs `?`+`*`,
    # `?<><`, `?U+00E9/`, `?"x`, `?'x`, `?\x`, backtick/percent/at/
    # tilde/pipe/gt composed — the catch-85 mid-word law holds;
    # libyaml's scan_anchor follow set is never reached because the
    # '?' cut precedes them). MEASURED OUT with verdict classes: the
    # c-flow-indicator six and `#` (yeptris rejects the name —
    # LIBYAML-ONLY-ACCEPT composed / BOTH-ERROR bare-head), `!`
    # (structure-unstable: a glued `!` + tag-char run re-lexes as a
    # TAG prop — `?!$` reads an anchored SEQ and the PS body then
    # sits at column 0), 0x7F and U+0080 (yeptris glues but libyaml
    # rejects — the YEPTRIS-ONLY-ACCEPT class this arm does not
    # own), space/tab/breaks, and the interior-residual `:` (the
    # r336 pure-run branch owns pure-colon residuals). The non-`?`
    # residual leads stay out: libyaml's scan_anchor follow set
    # errors the bare-heads (`&2/:` / `&2$:` probed, both
    # YEPTRIS-ONLY-ACCEPT). A/B (probed, JSON mode byte-exact):
    # libfyaml 04e0b58 reads yeptris's artifact stream byte-exact in
    # 1.2 (root SCALAR, anchor `2?/:` riding, value
    # U+2029+fold-space+backslash); its 1.1 mode rides the
    # PS-as-break matching libyaml's value. Floor: the 12-byte
    # `&2?/: '<PS>'` (greedy single-byte ddmin fixed point — the CR
    # deletion dies into the classified PS-only body, the `\`
    # deletion into the r366-classified body; every deletion
    # classified, the floor alone DIFFERs exit 77 pre-patch). The
    # 50-mutant paired battery (seed 370): 0 regressions, the
    # standing pins byte-identical (see the round issue). THE
    # TREADMILL: the crasher is one ChangeByte from a PINNED SEED
    # and the comparator snapshot's exclusion list did not cover
    # this family — the workflow snapshot gains the
    # root-anchor-quoted-follow exclusion this round. Comparator
    # carry only, no engine change (the r318/r330/r331/r332/r336/
    # r346/r356/r366/r368/r369 precedent).
    add_test(
        NAME fuzz-diff-r370-root-anchor-quoted-follow-slash
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-slash-r370.bin
    )
    add_test(
        NAME fuzz-diff-r370-root-anchor-quoted-follow-slash-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-slash-r370-artifact.bin
    )

    # r376 (run 45299095581, main df5722db, crash-ad6bb839b5597f4891139e74d
    # 5d17be28e47d336): the family's tab-fold mutant-of-pin red — the
    # artifact is the r368 clamp head `&2?*: '"":[{...` grown the fold
    # cluster `\n\tfoo,\n foo\n ]\n` mid-body (a tab-LEAD continuation
    # line inside the giant single-quoted body) plus the `=ALI *`/`%{`
    # content islands. The r238 arm's walks declined at the r331
    # position law's deferred cell: the old tab branch admitted a
    # tab run behind a clean CR/LF fold ONLY at the body end, and
    # the mid-line tab stayed on "the OTHER arms" — whose
    # anchor-charset split copy loses its libyaml byte-invariance at
    # the dump clamp (the 4095-char line budget), so the giant-body
    # composition was unclaimed. THE LAW (all cells probed byte-equal
    # on both engines, q1-q22/e1-e3 in the session log): inside the
    # single-quoted body every blank byte is line-anchored — a blank
    # run touching a clean line break strips (LEAD `a\n\tb` -> `a b`,
    # TRAILING `a\t\nb` -> `a b`, mixed runs `a\t \nb`/`a \t\nb` ->
    # `a b`), a blank run strictly between content bytes rides the
    # content spell (one escaped \t pair per tab, `a\t\t \tb` ->
    # `a\t\t \tb`), and n clean breaks (a CR LF pair is ONE break —
    # the r188 pair law; blank-only lines ride the run — `a\n \nb` ->
    # `a\nb`) spell (n-1) escaped newlines for n>=2 and one fold
    # space for n==1 (`a\n\nb` -> `a\nb`, `a\n\n\nb` -> `a\n\nb`, the
    # trailing `a\n'` -> `a `, `a\n\n'` -> `a\n`). The walks gain the
    # three tab cells + the run-aware break branch (both engines'
    # walkers + the clamp-tail helper + the arm's body-extent
    # pre-scan). A/B (probed, JSON mode byte-exact): libfyaml 04e0b58
    # reads yeptris's artifact stream byte-exact in 1.2 (root SCALAR,
    # anchor `2?*:` riding, the fold-cluster body with the tab lead
    # stripped); its 1.1 mode matches libyaml's map+key structure —
    # comparators-split along the libyaml-compat bar, comparator
    # carry only, no engine change (the r318/r330/r331/r332/r336/
    # r346/r356/r366/r368/r369/r370 precedent). Floor: the 4097-byte
    # clamp-boundary unit (greedy soup-unit ddmin under the dual bar
    # + the shape bar; the libyaml value line rides exactly at the
    # 4095-char clamp cut — one soup unit less unclamps into the
    # charset arm's claim). 50-mutant paired battery (seed 376, 20
    # flips + 10 deletions + 20 doubles per shape): 0 regressions, 73
    # newly agreeing, 0 both-red, 27 both-pass; the standing pins
    # byte-identical (see the round issue).
    add_test(
        NAME fuzz-diff-r376-root-anchor-quoted-follow-tabfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-tabfold-r376.bin
    )
    add_test(
        NAME fuzz-diff-r376-root-anchor-quoted-follow-tabfold-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-tabfold-r376-artifact.bin
    )

    # r377 (run 45317828972, main 7171f226 — the nightly's own red plus
    # the r376b soak's two finds, all three faces carried in one
    # composed widening). THE THREE FACES (each probe-verified
    # byte-equal on both engines before admission, the A/B law:
    # libfyaml 04e0b58 declines the glued-`?{` shape outright in BOTH
    # 1.2 and 1.1 passes (the documented third reading) and joins
    # yeptris's PS-fold member reading in 1.2 — comparators split
    # along the libyaml-compat bar, comparator carry only, no engine
    # change; the r318/r330/r331/r336/r346/r356/r366/r368/r369/r370/
    # r375 precedent):
    # - FIND 1 (the 15-byte floor `[=,?{?#,`,*L\n}]`, the 163-byte
    #   artifact): the glued collection follower — a `{`/`[` directly
    #   after the member's `?` rides the split machinery (the engine's
    #   `?` is the explicit-key indicator with the collection as the
    #   key node on BOTH engines; probed `[?{a}]`/`[?[a]]`/`{?{a}}`/
    #   `[? {a}]` all EQUAL). The floor composes it with the
    #   nested-map law-a comment-eat (the KEY token's glued `#` eats
    #   `#,\`,*L` to the fold, the nested pair empties) and the law-c
    #   alias ride (`*L` renders `=ALI *L` on yeptris, never lexes on
    #   libyaml). No new state: the r310/r351 nested-collection arms
    #   walk it once the follower routes there.
    # - FIND 2 (the 16-byte floor `\n---\n[?/ ?,?/]`): the doc-marker
    #   LEAD admits a blank/break RUN before the FIRST marker line
    #   (both engines scan-to-token before the doc start; probed
    #   `\n---\n[1]`/` \n---\n[1]`/`\n \n---\n[1]`/`\r---\n[1]` and the
    #   exotic-break heads `NEL---`/`LS---`/`PS---`/`\nLS---` EQUAL;
    #   the marker-count law composes unchanged behind the run; a tab
    #   before the triple is BOTH-ERROR on both engines — the walk's
    #   decline matches). The
    #   member faces are the standing law-d split + the PS raw-ride
    #   (libyaml's KEY plain keeps the PS raw, yeptris folds it to the
    #   one space). The 66-byte artifact's `----\n` lead (a FOUR-dash
    #   line is plain CONTENT — the multi-line BLOCK plain riding the
    #   break runs into the `[`...) is the documented residual: a
    #   BLOCK-context face (law j), the flow family stays out; the
    #   16-byte floor carries the law.
    # - FIND 3 (the nightly's 73,133-byte giant, ddmin'd to the
    #   23-byte floor `[1,?#\r\r1<U+2029>n,?\r#\n5 ?#\n]` — chunk then
    #   byte ddmin, 272 bar calls total): the r319 steal law's clean
    #   eat joins the SEQ ROOT (the old gate declined it as untested).
    #   Probed byte-equal `[1,?\r##\na,2]` -> `+SEQ/1/+MAP/a//-SEQ`:
    #   the parser inserts the empty value at the closer and the
    #   stolen key rides the same plain walk as the separator form —
    #   the member is an AGREEING composition riding the dual
    #   byte-equality firewall. The floor's divergence lives in the
    #   sibling glued-`?#` member (the CR-run/LS fold faces, law b/d —
    #   the r361 neighborhood, claimed by the standing arms); the
    #   giant decomposes to these faces + the clean-eat member, NOT
    #   the soak finds' conjunctions (three independent faces).
    # 50-mutant paired battery (seed 377, 10 ChangeByte/InsertByte/
    # DeleteByte/SpliceDuplicate per shape across all three floors +
    # the find-1 artifact + the giant): 0 regressions, 15
    # newly-carried, 16 stable-green, 19 stable-red; the classifier
    # self-test green (127 seeds, 6813 dump-tamper cases declined, 818
    # mutants, 0 failures).
    add_test(
        NAME fuzz-diff-r377-flow-qcollection-glued-aliasfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qcollection-glued-aliasfold-r377.bin
    )
    add_test(
        NAME fuzz-diff-r377-flow-qcollection-glued-aliasfold-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qcollection-glued-aliasfold-r377-artifact.bin
    )
    add_test(
        NAME fuzz-diff-r377-flow-doclead-prerun-psfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-doclead-prerun-psfold-r377.bin
    )
    add_test(
        NAME fuzz-diff-r377-flow-cleaneat-seq-exoticfold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-cleaneat-seq-exoticfold-r377.bin
    )
    add_test(
        NAME fuzz-diff-r377-flow-cleaneat-seq-exoticfold-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-cleaneat-seq-exoticfold-r377-artifact.bin
    )

    # r378 (run 45329321744, main bc1452c4, crash-e421dc2e63fe1ad3703200e52eeeb469ce41f61c):
    # the r273 mined-soup law's tag-glue member grown the SECONDARY-handle
    # tag resolution — the 33-byte artifact
    # `[11>0e1:\r\r ,?!!l,?\r7\r\r ?\r ,?##m\r]` carries a `?!!l` member
    # (the glued '?' + the `!!` tag) over CR runs, a `##m` comment-eat tail,
    # and a `>`-mid-plain key that AGREES (`=VAL : 11>0e1` byte-equal on
    # both engines — the issue's mid-key charset candidate is a non-face).
    # The DIFFER is member 2: yeptris's suite-pinned plain-'?' law
    # (HM87/652Z) keeps `?!!l` ONE flat plain while libyaml's flow KEY-token
    # law consumes the '?' and binds the `!!l` tag property to the EMPTY
    # scalar key (`=VAL : <tag:yaml.org,2002:l> `), the trailing member's
    # separator giving the pair its terminator (why `[1,?!!l]` alone is the
    # classified YEPTRIS-ONLY-ACCEPT split: no follower token, no node, an
    # error). The r273 arm carried its shapes EXACT-BYTES because a
    # libyaml-side tag model would have had to un-decline the
    # yd_family_classify mutant firewall's own tag-splice case — but the
    # firewall guards the models, not this walk: the qgh differ walk gains
    # the TAGGED-EMPTY-KEY WRAPPER PAIR — yeptris's killed member line
    # against the whole four-line block, the annotation byte-exact the
    # token's resolution (secondary `!!name` -> `tag:yaml.org,2002:name`,
    # verbatim `!suffix` -> `<!suffix>`, bare `!` drops — the r273 m0 row;
    # a kept non-empty tail rides byte-exact after the '>' as the KEY's
    # content with its ONE leading blank eaten; every escape/control/
    # non-ASCII byte and %-handle rebinding declines). Floor: the 10-byte
    # `[1,?!!l,?]` (greedy ddmin to a fixed point under the DIFFER +
    # dump-pair shape bar; the trailing `,?` terminator member is
    # load-bearing). A/B (probed byte-exact, both modes): libfyaml 04e0b58
    # reads the floor AND the artifact byte-semantically yeptris's way in
    # strict-1.2 AND 1.1 (`?!!l` and `?##m` flat plains, `11>0e1` the
    # agreeing map) — the engine defect is refuted, comparator carry only,
    # no engine change (the r317/r343/r366/r375 precedent). Probe matrix:
    # 30+ shapes in the session log (secondary tails `!!abc`/`!!a-b`/
    # `!!a.b`/`!!_`/`!!9`/`!!`(empty name), the verbatim/bare rows, the
    # content-tail rows single+multi-word+folded, the `?!!l:` separator
    # face EQUAL pre-patch, the `?!!l x` tail row newly carried, the
    # double-blank tail and the %TAG-rebound `---\n%TAG !! x\n[1,?!!l,?]`
    # (libyaml `<xl>`) declining conservative). 50-mutant paired battery
    # (seed 378, 30 single + 20 double per shape): see the round issue.
    add_test(
        NAME fuzz-diff-r378-flow-qsoup-tagresolve
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qtag-glue-r378.bin
    )
    add_test(
        NAME fuzz-diff-r378-flow-qsoup-tagresolve-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qtag-glue-r378-artifact.bin
    )

    # r379 (the r378 drill's bounded comparator soak, issue #1636, main
    # dfb30cac, crash-4ca2e9109a803dac5edf48bc8a76229f89bbfce9): the
    # 26-byte artifact `[0=,?=,{?#,?\t*L\t ,\n\t }\n,8]` -- a root flow
    # seq whose members glue `0=` / `?=`, then a nested flow map carrying
    # the glued `?#` (comment-eat) and the `?\t*L\t ` member (the `?` +
    # TAB separator + the glued alias), the `\n\t }` blank-only-line
    # close, and the trailing `,8`. The DIFFER composes THREE
    # comparator-side faces, none engine-suspicious (libfyaml 04e0b58
    # reads byte-semantically yeptris's way in BOTH strict-1.2 and 1.1
    # on the artifact AND the floor -- probed byte-exact):
    # (1) the bare-`?=` member: libyaml's flow KEY-token law (the 1.1
    #     quirk the qflow family has carried since r58; the suite pins
    #     the plain reading -- HM87 "[?x]" and 652Z "{ ?foo: bar }", the
    #     ledger lines yeptris follows) makes `?=` the implicit
    #     single-pair map (key `=`), yeptris's plain-'?' law keeps it
    #     ONE flat plain (yd_qflow_divergence's single-pair branch, the
    #     classic [?x] shape);
    # (2) the glued-`?#` comment-eat member: libyaml's post-'?'-token
    #     comment-glue runs the glued '#' to the BREAK (eating
    #     `#,\t*L\t ,` wholesale -- the pending KEY re-pairs with the
    #     closer) while the suite pins a '#' as content without a blank
    #     lead (the HM87/r85 law), so yeptris reads `?#` ONE plain key
    #     and the explicit-key alias member (`?` + TAB separator =
    #     separated, so BOTH engines agree the alias `*L` is the key --
    #     probed) that follows is yeptris's ALONE (the qgh walk's
    #     glue-marked emptied-key pair + the ate-alias member skip);
    # (3) the glued-`?*L` alias-key face: libyaml's KEY-token law
    #     consumes the '?' and the glued `*L` RE-LEXES as the alias
    #     token riding the KEY SLOT ("=ALI *L" -- the alias EVENT in
    #     the key slot, the dump shape no qflow arm carried; the r199
    #     qflow-bareq-alias-ride and r355 anchor-blank-alias families
    #     cover the separated/`:`-glued forms) while yeptris's plain-'?'
    #     law keeps `?*L` ONE flat plain (yd_qflow_divergence's
    #     ALIAS-KEY line twin, the r178/r163 plain-first-'?' shape:
    #     libyaml's rendering is yeptris's content's byte-exact suffix
    #     minus the ONE '?').
    # Floor: the 12-byte `[=,?=,{?*L}]` (greedy ddmin to a fixed point
    # under the DIFFER + exact dump-pair bar; every single-byte deletion
    # declines -- single-byte fixed-point re-verified on the drill; the
    # from-artifact greedy path lands a 17-byte sibling
    #     `[0,?=,{?#,*L\n},8]` carrying face (2) alone -- the artifact is
    # genuinely multi-face). The mutant battery (seed 379, 30 single +
    # 20 double per shape): see the round issue.
    add_test(
        NAME fuzz-diff-r379-flow-qalias-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qalias-key-r379.bin
    )
    add_test(
        NAME fuzz-diff-r379-flow-qalias-key-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qalias-key-r379-artifact.bin
    )

    # r331 (run 44697552012, main 311da7ef): the family's SECOND
    # mutant-of-own-pin red — the artifact is r330's own pinned seed
    # `&2?: '<PS><CR>'` with the CR ChangeBit'd to a TAB
    # (`&2?: '<PS><TAB>'`, 11 bytes). The r238 arm's blanket no-tab
    # gate declined it; the tab delta is the QUOTED-BODY exotic-tab
    # law and it is CONTAINER-INDEPENDENT: the dual-bar floor drops
    # clean out of the anchor family to the 8-byte `? '<PS><TAB>'`
    # (greedy ddmin — `&`/`2`/`:` all die), where BOTH engines agree
    # on the structure and the ONLY delta is the tab itself. The law:
    # yeptris (1.2) keeps a tab run behind a kept-raw LS/PS break as
    # CONTENT (one escaped \t per tab — the exotic break arms no fold
    # state), libyaml's blank-state swallows the whole run and emits
    # NOTHING (no fold space behind the exotic join — probed
    # `<PS><TAB>a` reads `<PS>a`); mid-content tabs stay content on
    # BOTH engines, and behind a clean CR/LF fold the run vanishes on
    # BOTH. libfyaml 04e0b58 sides with yeptris byte-exact (probed: it
    # re-emits `&2?: '` + the raw PS + the tab, the whole riding anchor
    # name kept) — comparator carry only, no engine change (the
    # r318/r330 precedent). THE CARRY: (1) the r238 arm's no-tab gate
    # becomes body-region-scoped and its body walkers gain the
    # measured post-break tab law for both sub-shapes (the riding
    # anchor + the anchored-empty-key) — the artifact's path; (2) a
    # new differ-only arm owns the DIRECT-QUOTED containers: the
    # floor form (`? '<...>'` — the quoted scalar is the KEY, the
    # value empty) and the plain-key value form (`<run>: '<...>'`,
    # keys `2`, `2?`, `?`, `?2`, `ab`, `2_`, `3&2`, `o&2` — the
    # glued-& anchor rides mid-plain CONTENT on both engines, never
    # leading — all byte-equal on both
    # engines), both dumps derived from the input and required
    # byte-exact. The NEL spellings are SPLITS, not differs (probed
    # `2: '<NEL>'`, `2: '<NEL><TAB>'`, `2: '<LS><CR>'` — yeptris
    # REJECTS while libyaml accepts — the differ-only routing
    # declines, the r170 law); the bare-exotic and CR/LF+tab
    # spellings AGREE (no claim needed). Floor: 8 of 11 bytes survive
    # the dual bar (greedy ddmin to a fixed point; pre-patch DIFFER
    # exit 77 + NOT classified, post-patch carry exit 0, on BOTH the
    # floor `? '<PS><TAB>'` and the full artifact; no further
    # single-byte deletion keeps the dual bar). The 50-mutant paired
    # battery (30 single + 20 double, seed 331): see the round issue.
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r331-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r331.bin
    )
    add_test(
        NAME fuzz-diff-r331-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r331-artifact.bin
    )

    # r332 (run 44721335253, main e00d1138): r331's own next-round
    # pointer fires — the artifact is the 11-byte `...: '<PS><TAB>'`,
    # the quoted-body exotic-tab law riding the family's DOTTED-KEY
    # container: `.` glued to the riding ':' denies libyaml's `...`
    # doc-end cut (that one needs the blank behind the run), so yeptris
    # keeps the whole dotted run as the key's plain content and libyaml
    # 90a56d4 reads the SAME key byte-equal — probed `.`, `..`, `...`,
    # `.x`, `x.y`, `2.2`, `...x`, `..?`, `..&2`, `x..`, `?.`, `._`,
    # `.-`, `..2`, `2..`, `2._3`, `.-2` — all key-equal, all DIFFER
    # only in the body (the r331 pointer's "dotted keys ... a red
    # there means the container law needs its own measurement round").
    # THE WIDENING (all measured this round): (1) the blank run joins
    # the tab run and takes the exotic join's ADJACENCY law — a blank
    # run (tabs AND spaces, mixed) directly BEHIND a kept-raw LS/PS
    # break or DIRECTLY BEFORE one is swallowed whole on libyaml
    # (rendered NOTHING — probed `<PS><TAB><SP>a`, `a<SP><PS><TAB>b`,
    # the leading `<SP><PS><TAB>a`, the mixed
    # `<PS><SP><TAB><SP><TAB><SP>b`, and the anchor family's own
    # `&2?: '<PS><SP>a'`/`&2?: '<PS><TAB><SP>a'`/
    # `&2?: '<PS><SP><TAB><SP>b'` — all read `<PS>…` on libyaml); a
    # run with NO exotic adjacency stays content on BOTH engines
    # (space literal, tab as the escaped \t — probed `<PS>a<SP>`,
    # `<SP>a`, `<TAB>a`, `'<PS>a<TAB>'` all byte-equal). yeptris
    # keeps every blank as content everywhere (the exotic break arms
    # no fold state — the r331 law, unchanged). (2) the r331 arm's
    # plain-key charset admits '.', and the '?' lead rides the run
    # (`?.` probed); the NEL/CR/LF/LS+CR/PS+CR spellings stay SPLITS
    # (probed `.: '<NEL><TAB>'`, `.: '<LS><CR>'`, `.: '<PS><CR>'`,
    # `.: '<CR><LF><TAB>'` — yeptris REJECTS while libyaml accepts,
    # the r170 law; the bare-exotic and NEL+space spellings are EQUAL,
    # no claim) and the empty-key `: '<PS><TAB>'` form is a
    # YEPTRIS-ONLY-ACCEPT split (libyaml rejects the empty key with a
    # quoted body — declined, unmeasured as a differ shape). The
    # shared r238 walkers gain the same measured adjacency law for
    # the anchor sub-shapes (the pre-break tab run joins walk_y, the
    # space run joins walk_l; the fold-adjacent composition — a clean
    # break behind, an exotic ahead — stays unmeasured and declines).
    # libfyaml 04e0b58 sides with yeptris byte-exact (probed: it
    # re-emits the dotted key + the raw PS + every blank of the body,
    # `x.y: '<PS><TAB>'`, `.: '<LS><TAB>'`, the mixed blankrun, and
    # `.: 'a<SP><PS><TAB>b'` all kept whole) — comparator carry only,
    # no engine change (the r318/r330/r331 precedent).
    # Floor: 9 of 11 bytes survive the dual bar (greedy ddmin to a
    # fixed point; pre-patch DIFFER exit 77 + NOT classified, post-
    # patch carry exit 0, on BOTH the floor `.: '<PS><TAB>'` and the
    # full artifact; no further single-byte deletion keeps the dual
    # bar — the third dot dies into the empty-key split, not into a
    # carry). The 50-mutant paired battery (30 single + 20 double,
    # seed 332): see the round issue. Both the floor and the full
    # artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r332-dotted-key-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/dotted-key-exotic-tab-r332.bin
    )
    add_test(
        NAME fuzz-diff-r332-dotted-key-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/dotted-key-exotic-tab-r332-artifact.bin
    )

    # r340 (the nightly's run 44871442691, main 1bcebd79): the
    # quoted-body exotic-tab family's THIRD composition red — the
    # 26-byte artifact `  # Comment\n ? '<PS><TAB>'  \n\n\n` is the
    # r331 law riding a fresh three-family container: the #949
    # comment-eat (the indented `  # Comment` line is inert on BOTH
    # engines), the r331 explicit-key slot at INDENT 1 (the ` ? ` line
    # opens the block map, quoted scalar the KEY, value empty), and
    # the r331/r332 quoted-body exotic-tab law itself (yeptris keeps
    # the post-exotic-break tab as content, libyaml's blank state
    # swallows it — dumps `=VAL : <PS>\t` vs `=VAL : <PS>`, the value
    # empty on both). THE A/B: libfyaml 04e0b58 reads the artifact
    # AND the floor byte-exact as yeptris (the key scalar `<PS><TAB>`
    # raw on both — the probe above) — comparator carry only, no
    # engine change (the r318/r330/r331/r332 precedent). THE
    # WIDENING (floor-form only): the r331 arm's gate gains a
    # comment-led PREFIX (full blank/comment lines, spaces-only
    # indents, LF-terminated; CR and tabs decline) and a space/LF
    # TRAILING RIDE behind the quote (CR/content bytes decline); the
    # prefix and the trailer are inert on both engines and the dumps
    # keep the r331 floor shape byte-exact; the plain-key form keeps
    # its EOF law. Floor: 9 of 26 bytes survive the dual bar (greedy
    # single-byte ddmin to a fixed point; pre-patch DIFFER exit 77 +
    # NOT classified, post-patch carry exit 0, on BOTH the floor
    # `? '<PS><TAB>'\n` and the full artifact; no further single-byte
    # deletion keeps the dual bar — the lone trailing LF defeats the
    # old EOF gate). The 50-mutant paired battery (30 single + 20
    # double, seed 340): see the round issue. Both the floor and the
    # full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r340-quoted-exotic-tab-comment-led
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r340.bin
    )
    add_test(
        NAME fuzz-diff-r340-quoted-exotic-tab-comment-led-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r340-artifact.bin
    )

    # r341 (the nightly's run 44880972632, main 6faef227): the quoted-
    # exotic-tab family's FOURTH mutation-treadmill red — the 9-byte
    # artifact `.:<TAB>'<PS><TAB>'` is the r332 floor's OWN next
    # mutation (the SPACE behind the value colon ChangeBit'd to a
    # TAB): the bare-colon sibling of the r332 dotted-key container,
    # riding the same r331/r332 quoted-body law (yeptris keeps the
    # post-exotic-break tab as content, libyaml's blank state swallows
    # it — dumps `=VAL : <PS>\t` vs `=VAL : <PS>`, the key `.` byte-
    # equal on both). THE MECHANISM (this round's measurement): the
    # ':' closed a SAVED SIMPLE KEY, and libyaml's fetch_value
    # simple-key branch leaves simple_key_allowed = 0 — the exact
    # state scan_to_next_token exempts from the no-tab rule — so the
    # WHOLE blank run (spaces AND tabs, mixed) rides behind the value
    # colon (probed `.:<TAB>'`, `.:<TAB><TAB>'`, `.:<TAB> '`,
    # `.: <TAB>'`, `.:<TAB> <TAB> '`, the dotted `..:<TAB>'`, the
    # pre-colon composition `.<TAB>:<TAB>'`, and the `?:` glue faces
    # `?:<TAB>'`/`?:<TAB><TAB>'`/`?: <TAB>'`/`?:<TAB> '` — keys
    # `.`/`..`/`?`/`N`, all key-equal, all DIFFER only in the body;
    # the key-form trailing-LF trailer is inert too). THE SPLITS THAT
    # STAY OUT (both re-measured): the floor `? ` tab faces
    # (`?<TAB>'<PS><TAB>'` — the '?' IS the key indicator, it leaves
    # simple_key_allowed = 1, the tab errors: YEPTRIS-ONLY-ACCEPT)
    # and the empty-key face (`:<TAB>'<PS><TAB>'` — the complex-value
    # ':' keeps simple_key_allowed = 1: YEPTRIS-ONLY-ACCEPT). THE A/B:
    # libfyaml 04e0b58 sides with yeptris on the fresh faces (probed
    # `.:<TAB>'<PS><TAB>'`, `..:<TAB>'<PS><TAB>'`,
    # `?:<TAB><TAB>'<PS><TAB>'`, `.:<TAB>'a'` — key byte-equal, PS
    # raw + tab kept whole) — comparator carry only, no engine change
    # (the r318/r330/r331/r332/r340 precedent). Floor: 9 of 9 bytes —
    # the artifact is ALREADY the dual-bar floor (greedy single-byte
    # ddmin to a fixed point: all nine deletions die pre-patch —
    # EQUAL, splits, the classified r332 floor behind the space, or
    # unterminated; pre-patch DIFFER exit 77 + NOT classified,
    # post-patch carry exit 0). The 50-mutant paired battery (30
    # single + 20 double, seed 341): see the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law;
    # byte-identical this round).
    add_test(
        NAME fuzz-diff-r341-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r341.bin
    )
    add_test(
        NAME fuzz-diff-r341-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r341-artifact.bin
    )

    # r342 (the nightly's run 44891943987, main 56b542af): the
    # soup/alias family's member-end ride red — the 33-byte artifact
    # `[11<CR><PS> 7.tag<CR> ,?##|,?<CR>*00:<LF>  m,]` composes the
    # doubled CR+PS break fold (the law-b/g dance, derived byte-exact
    # on both models pre-patch), the #949 glued-`?` member `?##|`, and
    # the NEW shape: the split-`?` explicit-key pair whose alias name
    # rides a colon closed by a BREAK (`*00:` + LF is ONE name `00:` —
    # the engine's prop walk has no break exception on the colon, the
    # 2SXE member-end law probed again this round), with the post-break
    # plain `m` binding as the pair's VALUE with no VALUE token ever
    # firing. libyaml's IS_ALPHA scan_anchor cut hands the glued ':'
    # to its flow VALUE token, so the dumps agree but for the NAME
    # bytes (`=ALI *00:` vs `=ALI *00`, the r311/r318 charset law).
    # THE A/B: libfyaml 04e0b58 corroborates the riding name event
    # (`=ALI *00:` on the artifact AND the floor) and declines the
    # binding itself (its strict grammar inserts the empty value and
    # errors "missing comma" on the post-run plain — the documented
    # third reading) — comparator carry only, no engine change (the
    # r318/r339/r340/r341 precedent). THE WIDENING: (1) fc_alias_name
    # drops the break from its colon-split set (probes: `? *00:<LF>,`
    # -> `=ALI *00:`, `[*00:<LF>,]` -> `=ALI *00:` bare-member twin,
    # `&a:<LF>` -> `&a:` anchor twin; `*00: x` / `*00:,` still cut);
    # (2) the split-key slot's post-name run may reach CONTENT — the
    # colon-less explicit-key VALUE binding (`? *a<LF>  m,` ->
    # pair(*a, m), `? *a:<LF>  m, n` -> pair(*a:, m) + the member n,
    # the separator's empty value included; an alias, an anchor, a
    # nested map, or any ':' across the run stays declined); (3) the
    # SEQ single-pair wrapper renders the alias key event exactly as
    # the MAP branch does. Floor: 9 of 33 bytes — `[?<CR>*0:<LF>m]`
    # (greedy single-byte ddmin under the shape bar — the naive dual
    # bar drifts to the PS-fold and bare-alias families; no further
    # single-byte deletion keeps the dual bar + the pair shape —
    # pre-patch DIFFER exit 77 + NOT classified, post-patch carry
    # exit 0, on BOTH the floor and the full artifact). The 50-mutant
    # paired battery (30 single + 20 double, seed 342): 22 classified,
    # 0 declined-DIFFER, 28 honest split/error flips — see the round
    # issue. Both the floor and the full artifact ride the corpus (the
    # r321 seed law).
    add_test(
        NAME fuzz-diff-r342-alias-memberend-ride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-memberend-ride-r342.bin
    )
    add_test(
        NAME fuzz-diff-r342-alias-memberend-ride-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-memberend-ride-r342-artifact.bin
    )

    # the r343 folded-header round (the nightly's run 44895282842, main
    # 56b542af; the 7-byte artifact crash-f5d8fce2a23a0c73ab015a3ef9614ff73a6eb810
    # "SP SP > NEL # SP", the 5-byte dual-bar floor "SP > NEL #" --
    # greedy ddmin to a fixed point; pre-patch DIFFER exit 77,
    # post-patch carry exit 0, on BOTH shapes, plain and lf harnesses;
    # no landmark-preserving single-unit deletion keeps the dual bar --
    # the 12th composition of the folded-header family, the r333 law's
    # neighborhood: the NEL-led body x the blank-ledger walk x the '#'
    # content byte, with NO lead material ahead of the '#'): the
    # indented bare '>' header is NEL-terminated (the r283
    # commented-header shape's zero-comment twin; the gate now admits
    # the header's own leading indentation -- presentation indent, the
    # blank-preceded-# signal becomes a flag so glued-# still
    # declines) and the '#' rides the body's FIRST byte at COLUMN 0 --
    # below the header indent. libyaml's 1.1 reader breaks AT the NEL
    # (the 1.1 IS_BREAK set; 1.2's b-break is CRLF|CR|LF only -- the
    # NEL is printable content, docs/spec/yaml-1.2.2 [21]), its
    # breaks-scan lands the column-0 line below the block indent, the
    # content loop ends the block, and the '#' re-lexes as the
    # blank-preceded document-level comment eaten to EOF -- the clip
    # joining the EMPTY leading_break, the value EMPTY (the r333
    # comment-eaten mechanism, zero lead). yeptris's 1.2 whole-route
    # keeps the NEL-led line as the block's own material (the r317
    # kept-NEL law) and the clip appends the ONE LF escape -- the
    # value reads ['#'][glued plain run][ONE clip ESC][line end]. THE
    # A/B: libfyaml 04e0b58 ERRORS the 1.2 pass ("block scalar no
    # linebreak found" -- its header law demands a 1.2 break, and the
    # NEL is not one) while its 1.1 pass reads the EMPTY value
    # byte-exact as libyaml (re-measured on THIS composition's own
    # floor, the r325 lesson) -- no comparator sides with the kept
    # content, the libyaml-is-the-compat-bar line carries, comparator
    # carry only, no engine change (the r317 precedent). The gate
    # holds: the space/LS-PS lead stays with the r333 kept-exotic arm
    # (disjoint), the glued-# header still declines, and the no-exotic
    # twins ("SP > LF #", "SP > CR #") stay DIFFER-declined outside
    # the law's NEL signature -- the next round's material if the
    # nightly escapes there. Both shapes' 50-mutant paired battery
    # (30 single + 20 double, seed 343): see the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r343-folded-header-direct-hash
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-direct-hash-r343.bin
    )
    add_test(
        NAME fuzz-diff-r343-folded-header-direct-hash-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-direct-hash-r343-artifact.bin
    )

    # the r363 escape-ride round (the post-r362 Fuzz Nightly, run
    # 45094381057, main c02ff708; the 16-byte artifact
    # crash-dc904f6dc4a435df2e95e665c8bc8927e497ee0d "SP SP > NEL
    # #[\"\\u0821\" SP" -- the DQ-escape SPELLING "ࠡ" glued behind the
    # '#' as the block's own plain text, trailing space, unclosed; the
    # 7-byte harness-bar floor "SP > NEL # \u" -- greedy ddmin to a
    # fixed point under the classified-DIFFER + dump-pair shape bar;
    # pre-patch DIFFER exit 77, post-patch carry exit 0, on BOTH
    # shapes, plain and lf harnesses; the r343 direct-hash law's
    # escape-bearing composition): the r343 eat walk stopped at the
    # input backslash's ESCAPED dump pair '\\' -- the first byte of
    # the pair, so every escape-bearing run declined. The ride: the
    # dump '\\' pair IS one input backslash -- plain material, the
    # walk skips the pair and keeps walking; every OTHER escape shape
    # (\n \r \t \xNN from raw breaks/tabs/controls), the raw exotic
    # leads and the dump-line breaks still stop the walk, and the
    # claim is untouched (the ONE clip escape, the dump-line end, the
    # r161 strcmp). Verdict-preserving: the pair's second byte can
    # never be the claim's 'n', so every shape the widened walk
    # reaches FURTHER on previously declined at this arm -- no
    # existing member's path changes. THE A/B: libfyaml 04e0b58
    # re-measured on THIS composition's own floor AND artifact (the
    # r325 lesson): its --yaml-1.2 pass ERRORS both ("block scalar no
    # linebreak found" -- the NEL is not a 1.2 break) while its
    # --yaml-1.1 pass reads the EMPTY folded scalar byte-exact as
    # libyaml on both -- no comparator sides with the kept content,
    # the libyaml-is-the-compat-bar line carries, comparator carry
    # only, no engine change (the r317/r343 precedent). Probes: the
    # literal-text twin "SP > NEL # \n" and the EOF-backslash twin
    # "SP > NEL # \" carry with the law; the raw-TAB twin
    # "SP > NEL # <TAB>" stays DIFFER-declined (the \t escape still
    # stops the walk mid-run; the raw-control twin
    # "SP > NEL # <0x01>" rides the pre-existing YEPTRIS-ONLY-ACCEPT
    # control split, outside this law) -- next round's material if
    # the nightly escapes there. Both shapes' 50-mutant paired
    # battery (30 single + 20 double, seed 363): see the round issue.
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r363-folded-header-direct-hash-escride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-direct-hash-escride-r363.bin
    )
    add_test(
        NAME fuzz-diff-r363-folded-header-direct-hash-escride-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-direct-hash-escride-r363-artifact.bin
    )

    # the r344 folded-header round (the nightly's run 44905293937, main
    # d5980c44; the 75-byte artifact crash-4e0e552d710e457aa24099c2274be50bd800663c
    # "> LF SP -!<NEL> SP SP !w…`w0e17.tag CR SP ,?bar}w…xt8text9", the
    # 17-byte dual-bar floor "> LF SP -!<NEL> SP SP .tag CR SP 9" --
    # greedy ddmin to a fixed point; pre-patch DIFFER abort, post-patch
    # carry exit 0, on BOTH shapes, plain and lf harnesses; no
    # landmark-preserving single-byte deletion keeps the dual bar --
    # the 13th composition of the folded-header family, the first past
    # the r343 direct-hash entry, the family CROSSING INTO the
    # verbatim-tag face): the body's content line opens "-!<" (dash +
    # the VERBATIM-tag opener glued) and carries a kept NEL mid-line, a
    # ".tag" tag-shaped run, and a bare CR boundary. libyaml's 1.1
    # reader breaks AT the NEL (the 1.1 IS_BREAK set; 1.2's b-break is
    # CRLF|CR|LF only -- the NEL is printable content,
    # docs/spec/yaml-1.2.2 [21]) and its continuation line ("SP SP
    # .tag", two spaces over the auto-indent 1) reads MORE-INDENTED --
    # the keep emits the line break literally and POISONS THE NEXT
    # BOUNDARY: after a kept line the fold branch's gate
    # (`*trailing_breaks.start == '\0'`, scanner.c -- the r177/r328
    # documented mechanism) suppresses the fold space at the CR site
    # and the break rides as the ONE kept escape, the "SP 9" line's
    # leading space eaten as the continuation's indent (the r237
    # flavor) -- b spells [ESC][9] where the break alone (no NEL
    # upstream -- the cr-only probe) folds to the same ONE space BOTH
    # engines spell. yeptris's 1.2 whole-route never broke at the NEL
    # (the r317 kept-NEL law: the byte rides raw as content), both
    # lines sit at the auto-indent 1, and the CR boundary folds to the
    # ONE space -- a spells [SP][9]. The arm sits LAST in the pair
    # walk's chain (fires only when every existing arm declined) and
    # eats exactly ONE byte per side behind the plain-byte coincidence
    # gate (a[1] == b[2], non-space/non-escape/non-exotic-lead/
    # non-frame-end, the r323 coincidence law) -- the coincidence
    # AUTOMATICALLY bounds b's escape run and a's space run to exactly
    # one, so the walk rejoins byte-bound at an agreement byte (the
    # r176 clip tail's strcmp completes the floor's binding). Both the
    # NEL-only and the CR-only sub-shapes classify clean on the
    # pre-patch walk (the r208/r176 pairing and the r140 clip carry
    # own them) -- the DIFFER is the JOINT face only. THE A/B: libfyaml
    # 04e0b58 re-measured on THIS composition's own floor (the r325
    # lesson) -- its --yaml-1.2 pass reads the value byte-exact as
    # yeptris ("=VAL >-!<\N  .tag 9\n": the NEL kept raw rendered \N,
    # the CR boundary folded to the ONE space, the clip escape) while
    # its --yaml-1.1 pass matches libyaml 90a56d4 escape-for-escape
    # ("=VAL >-!<\n .tag\n9\n") -- the comparators split along the
    # version line exactly as the r317 precedent did, comparator carry
    # only, no engine change. Both shapes' 50-mutant paired battery
    # (30 single + 20 double, seed 344): see the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r344-folded-header-verbatim-tag-cr-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-verbatim-tag-cr-fold-r344.bin
    )
    add_test(
        NAME fuzz-diff-r344-folded-header-verbatim-tag-cr-fold-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-verbatim-tag-cr-fold-r344-artifact.bin
    )

    # the r345 folded-header round (the nightly's run 44911747812, main
    # d8647d47; the 37-byte artifact crash-ac657c873c5add6f8105374fd9f31c34902752f8
    # "> #<PS> :\n  <NEL><NEL> :\n <PS><NEL>#0 :\n <PS><NEL>#0" -- the
    # VALUE-COLON face meeting the exotic-doubled body, the 7-byte dual-bar
    # floor "> LF SP <NEL>#0" -- greedy ddmin to a fixed point under the
    # DUMP-PAIR SHAPE bar (the r343 lesson: the naive dual bar drifts -- it
    # drifted here to the bare exotic-break fold face, so the bar carried
    # the family signature: single-VAL pair, raw NEL content on yeptris's
    # side and none on libyaml's, the '#0'-led tail INSIDE yeptris's value
    # and OUTSIDE libyaml's, and libyaml's value an in-order subsequence of
    # yeptris's under the one raw-NEL-to-escape substitution; no
    # landmark-preserving single-unit deletion keeps the dual bar; pre-patch
    # DIFFER exit 77, post-patch carry exit 0, on BOTH shapes, plain and lf
    # harnesses -- the r317/r320/r321/r323/r324/r325/r327/r328/r329/r333/
    # r334/r343/r344 folded-header family's FOURTEENTH composition, the
    # first past the r343 direct-hash entry): the body's content line is
    # "SP <NEL>#0" -- the NEL rides AFTER the indent space (mid-line, not
    # line-led and not header-terminated -- the r333 kept-exotic arm owns
    # the space/LS-PS leads, the r343 direct-hash arm the direct '#').
    # libyaml's 1.1 reader breaks AT the NEL (the 1.1 IS_BREAK set; 1.2's
    # b-break is CRLF|CR|LF only -- the NEL is printable content,
    # docs/spec/yaml-1.2.2 [21]), the line is a blank, and the '#'-led tail
    # sits at COLUMN 0 behind the exotic-break blank -- below the block
    # indent: the content loop ends the block, the "#0" re-lexes as the
    # blank-preceded document-level comment eaten to EOF, and the clip
    # joins the EMPTY leading_break -- the value EMPTY (the r333/r343
    # comment-eaten mechanism). yeptris's 1.2 whole-route keeps the NEL as
    # the block's own content (the r317 kept-NEL law) with the '#0' glued
    # behind it -- the value reads [NEL]['#0'][ONE clip ESC]. On the
    # ARTIFACT the same face arrives at the walk's tail AFTER the head's
    # kept-NEL pairing, at a raw-PS lead with a '#0'-colon-'#0' tail (the
    # VALUE-COLON content riding INSIDE the eaten region -- the r333 arm's
    # inner walk stops its run at the mid-tail raw triple). The r345 arm
    # eats exactly the ONE deterministic interval: a's remainder up to the
    # FIRST escape or raw dump-line break (spaces / plain bytes / kept NEL
    # pairs / kept LS-PS triples -- choice-free), demanding at least ONE
    # '#' inside it (the comment signature IS the mechanism; a non-'#' tail
    # opens libyaml's second document and stays out -- and a second
    # document would break the strcmp anyway), then the ONE clip escape and
    # the dump-line end, the remainders behind the claim coinciding
    # completely with b's (the r161 strcmp law -- b-side bytes are never
    # eaten). Sits behind the r333 kept-exotic and r343 direct-hash arms
    # (disjoint leads), disjoint from every escape-oriented arm
    # (r176/r325/r197/r320/r283/r288/r290/r216) and from the r303
    # EOF-tail arm (which demands the escape behind the NEL -- this
    # demands the glued '#'). THE A/B: libfyaml 04e0b58 re-measured on
    # THIS composition's own floor (the r325 lesson): its --yaml-1.2 pass
    # sides with yeptris ("+DOC\n=VAL >\\N#0\\n\n-DOC" -- the raw NEL kept
    # as block content, the \\N its render) while its --yaml-1.1 pass
    # ERRORS on the floor ("block scalar with wrong indented line after
    # spaces only") and matches libyaml 90a56d4's value byte-exact on the
    # artifact ("=VAL >:\\n \\n\\n:\\n" -- its folded style mark, libyaml's
    # plain) -- the comparators split along the version line exactly as the
    # r317/r343 precedent, no comparator sides with either side outright,
    # the libyaml-is-the-compat-bar line carries, comparator carry only,
    # no engine change. Both shapes' 50-mutant paired battery (30 single +
    # 20 double, seed 345, unit-aware palette mutations guarding the header
    # lead; solo runs): floor 0 newly-carried / 50 stable-carried / 0
    # stable-red / 0 regressions; artifact 0 / 49 / 1 stable-red (mutant
    # 30 -- line 2's tail colon deleted + the last '#'->'*': its pair dies
    # at the NEL-site r<k cap, the r290/r320 ledger decline direction,
    # pre-existing on main, out of round scope) / 0 regressions; the
    # floor's own tail-twin ("> LF SP <NEL>#0 LF <NEL>#0", pre-77) is a
    # manual newly-carried face -- the tail line document-level on BOTH
    # sides. See the round issue. Both the floor and the full artifact ride
    # the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r345-folded-header-value-colon-hash-eof-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-value-colon-hash-eof-tail-r345.bin
    )
    add_test(
        NAME fuzz-diff-r345-folded-header-value-colon-hash-eof-tail-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-value-colon-hash-eof-tail-r345-artifact.bin
    )

    # the r346 quoted-exotic-tab round (the nightly's run 44918966386, main
    # 2fed1cb1; the 12-byte artifact crash-7a29d0923479791b85abe71fcf388b1e7247cdc2
    # "CR CR :.: TAB '<PS> TAB '" -- the QUOTED-PS/TAB body (the r331/r332/
    # r340/r341 law, byte-identical here) riding a COLON-LED plain key behind
    # a doubled-CR blank lead, the quoted-exotic-tab family's FIFTH
    # composition -- the 9-byte dual-bar floor ":: TAB '<PS> TAB '" -- greedy
    # single-byte ddmin to a fixed point under the DUMP-PAIR SHAPE bar (the
    # r343 lesson; the bar carried the family signature: single-VAL pair, the
    # key byte-equal on both engines, the raw PS triple kept on BOTH sides,
    # the TAB rendered as the escaped \\t INSIDE yeptris's value and ABSENT
    # from libyaml's, libyaml's value an in-order subsequence of yeptris's
    # under the one tab-to-escape substitution; both CRs die, the '.' dies
    # into the ':'-alone key; no further single-byte deletion keeps the dual
    # bar; pre-patch DIFFER exit 77 + NOT classified, post-patch carry exit
    # 0, on BOTH shapes, plain and lf harnesses). THE MECHANISM (this
    # round's measurement): the ':' is the KEY PLAIN's head -- a plain
    # scalar starts with ':' when a NON-BLANK follows (the ns-plain-first
    # law; ':' in indicator position only when blank follows -- that face is
    # the empty-key split, still declined), so the second ':' is the VALUE
    # colon and fetch_value's simple-key branch leaves simple_key_allowed =
    # 0 -- the exact state libyaml's scan_to_next_token exempts from the
    # no-tab rule (the r341 law) -- so the separator TAB rides, and the
    # DIFFER itself is the r331/r332/r340/r341 quoted-body law (yeptris
    # keeps the post-exotic-break tab as content, libyaml's blank state
    # swallows it -- dumps "=VAL : <PS>\\t" vs "=VAL : <PS>"). The r346
    # widening: (1) the ':' plain HEAD (probed byte-equal on BOTH engines,
    # all DIFFER only in the body: keys `:`, `:.`, `:x`, `:2`, `:-`, `:?`,
    # `:&` (the '&' rides mid-plain, never the head), `:.x`, and the
    # GLUED-COLON composition keys `::2`/`:::x` -- the r332 glued-colon law
    # GENERALIZES to the stated ':' + non-blank = plain-content rule; the
    # old `..::`/`..':` spellings keep their exact keys); (2) the inert
    # break LEAD (full lines of CR / LF / CRLF pairs and spaces-only runs,
    # any order, any repetition -- the dumps byte-identical to the
    # lead-less form; probed CR x1, CR x3, CRLF, LF x1, LF x2, the LF+CR
    # and CR+LF+CR mixes, the SP-indent ` ::` line, the CR-led `?:`-glue
    # face; TAB pieces decline -- the CR-TAB lead and the tab-indent line
    # are splits/others -- and '#'-comment lines stay floor-form-only, the
    # r340 law); (3) the trailer law gains the CR byte (`...'\\r` and
    # `...'\\r\\n` are final blank breaks, inert on BOTH engines, measured
    # on the key form AND the floor form -- the r340 "CR declines" wall was
    # the unmeasured spelling). THE DECLINES THAT STAY OUT (re-measured):
    # the empty-key faces (`:<TAB>'<PS><TAB>'`, `: '<PS><TAB>'`,
    # `: : '<PS><TAB>'` -- YEPTRIS-ONLY-ACCEPT, the r332/r341 law) and the
    # glued-run-then-quote faces (`:::'<PS><TAB>'`, `:.::'<PS><TAB>'` -- no
    # value colon ever fires, EQUAL/other, never a differ). THE STABLE-REDS
    # (all pre-existing on main, the pre-patch binary DIFFs identically,
    # the escalation clause's territory): the ';'-led and '/'-led key
    # charset escapes (`;:<TAB>'<PS><TAB>'`, `:/...`), the ',' mid-key
    # splice, the '?' content byte inside the lead, and the unmeasured
    # interior-space key (`:x y:`) / mid-plain-quote key (`:'x:`) faces --
    # all DIFFER with the family shape pre- AND post-patch, declined by
    # the TIGHTEST gate. THE A/B: libfyaml 04e0b58 re-measured on THIS
    # composition's own floor (the r325 lesson): its --yaml-1.2 pass sides
    # with yeptris BYTE-EXACT on the floor AND the artifact (it re-emits
    # the colon-led key + the raw PS + the kept tab, "3a 3a 20 27 e2 80 a9
    # 09 27" on the floor) while its --yaml-1.1 pass ERRORS ("invalid tab
    # used as indentation" at the separator -- the documented third
    # reading) -- comparator carry only, no engine change (the
    # r318/r330/r331/r332/r340/r341 precedent). Both shapes' 50-mutant
    # paired battery (30 single + 20 double, seed 346, unit-aware palette
    # mutations, LS/PS triples atomic; solo runs -- the r345 lesson): floor
    # 9 newly-carried / 40 stable-carried / 1 stable-red (';' key-head
    # ChangeBit, pre-existing) / 0 regressions; artifact 16 newly-carried /
    # 31 stable-carried / 3 stable-red ('.'->'/' ChangeBit, the TAB->','
    # splice, the '?'-in-lead double, all pre-existing) / 0 regressions.
    # See the round issue. Both the floor and the full artifact ride the
    # corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r346-quoted-exotic-tab-colon-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r346.bin
    )
    add_test(
        NAME fuzz-diff-r346-quoted-exotic-tab-colon-head-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r346-artifact.bin
    )

    # the r347 quoted-exotic-tab round (the nightly's run 44927174304, main
    # 5488d800; the 10-byte artifact crash-a6c7bf66f7669eb53667e23c415fcfe55bc981c2
    # "$.: '<PS><TAB>'" -- the QUOTED-PS/TAB body (the r331/r332/r340/r341/r346
    # law, byte-identical here) riding a DOLLAR-LED plain key behind a STANDARD
    # space separator, the quoted-exotic-tab family's SIXTH composition and
    # fourth key-form container -- the 9-byte dual-bar floor "$: '<PS><TAB>'"
    # drops the dot ('$' is no 1.2 indicator, plain content byte-equal on both
    # engines; the '$' itself cannot die -- its deletion ".: '<PS><TAB>'" is the
    # ALREADY-CLASSIFIED r332 dotted-key territory, exit 0 pre-patch, so the
    # dual bar denies it; no further single-byte deletion keeps the dual bar;
    # pre-patch DIFFER exit 77 + NOT classified, post-patch carry exit 0, on
    # BOTH shapes, plain and lf harnesses). THE MECHANISM (this round's
    # measurement): the DIFFER itself is the r331/r346 quoted-body law (yeptris
    # keeps the post-exotic-break tab as content, libyaml's blank state
    # swallows it -- dumps "=VAL : <PS>\\t" vs "=VAL : <PS>"); the r347
    # widening admits the '$' byte into the plain-key charset (head faces probed
    # byte-equal on BOTH engines, all DIFFER only in the body: `$`, `$.`, `$x`,
    # `$2`, `$_`, `$-`, `$-k`, `$?`, `$.x`, `$$`, `$$.`, `$2_3`, `$-2`, `$?2`,
    # `$&2` (the '&' rides mid-plain, never the head -- the r238 arm), the
    # GLUED-COLON composition `$:x`, the GLUED-QUOTE spelling `$':` (key `$'`,
    # the r332 law), and the r346 COLON-HEAD composition `:$`; the lead law
    # composes unchanged (CR x1/x2, CRLF, LF, spaces probed) and the trailer
    # law rides (LF, CR, CRLF probed)). THE DECLINES THAT STAY OUT
    # (re-measured): the interior-space key (`$ x`), the mid-plain-quote key
    # (`$'x`), and the ';'-/'/'-led charset escapes and the ',' mid-key splice
    # -- all DIFFER with the family shape pre- AND post-patch, stable-reds,
    # the escalation clause's territory; the '&' HEAD is EQUAL on both engines
    # (no differ at all). THE A/B: libfyaml 04e0b58 re-measured on THIS
    # composition's own floor (the r325 lesson): its --yaml-1.2 pass sides
    # with yeptris BYTE-EXACT on the floor AND the artifact (it re-emits the
    # dollar-led key + the raw PS + the kept tab, "24 3a 20 27 e2 80 a9 09 27"
    # on the floor) while its --yaml-1.1 pass ERRORS ("invalid tab used as
    # indentation" -- under 1.1 the PS is a break, so the quoted body's tab
    # rides it as an indentation tab -- the documented third reading) --
    # comparator carry only, zero src/ change, no budget raises. Both shapes'
    # 50-mutant paired battery (30 single + 20 double, seed 347, unit-aware
    # palette mutations, LS/PS triples atomic; solo runs -- the r345 lesson):
    # floor 1 newly-carried / 49 stable-carried / 0 stable-red / 0
    # regressions; artifact 3 newly-carried / 47 stable-carried / 0
    # stable-red / 0 regressions -- the $-container faces are rare among
    # random mutants (the body/lead mutations were classified by the
    # earlier rounds), and the battery's value is the 0-regression gate. See the round issue. Both the floor and the
    # full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r347-quoted-exotic-tab-dollar-plain-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r347.bin
    )
    add_test(
        NAME fuzz-diff-r347-quoted-exotic-tab-dollar-plain-key-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r347-artifact.bin
    )

    # the r348 quoted-exotic-tab round (the nightly's run 44934770024, main
    # 8b10ff33; the 11-byte artifact crash-8371179c0e73967d004d14345187590e1e475180
    # "..,: '<PS><TAB>'" -- the QUOTED-PS/TAB body (the r331/r332/r340/r341/r346/
    # r347 law, byte-identical here) riding a DOT-DOT-COMMA plain key behind a
    # STANDARD space separator, the quoted-exotic-tab family's SEVENTH
    # composition and FIFTH key-form container -- the comma is the r339
    # alias-rider comma-glue law's neighbor, here INSIDE plain key content
    # (',' is a flow indicator only: plain content in BLOCK context on BOTH
    # engines, and no plain-FIRST on EITHER engine -- the comma-LEAD face
    # ",: '<PS><TAB>'" is a BOTH-ERROR, so the 10-byte dual-bar floor
    # ".,: '<PS><TAB>'" drops the leading dot (plain content byte-equal; the
    # SECOND dot cannot die -- its deletion ",: '...'" is the comma-lead
    # BOTH-ERROR face; the comma itself cannot die -- its deletion "..: '...'"
    # is the ALREADY-CLASSIFIED r341/r346 territory, exit 0 pre-patch; the
    # space behind the colon dies into the glued-quote/EQUAL face "..,:'...'"
    # -- no value colon ever fires); pre-patch DIFFER exit 77 + NOT classified,
    # post-patch carry exit 0, on BOTH shapes, plain and lf harnesses). THE
    # MECHANISM: the DIFFER is the r331/r346 quoted-body law (yeptris keeps
    # the post-exotic-break tab as content, libyaml's blank state swallows it
    # -- dumps "=VAL : <PS>\\t" vs "=VAL : <PS>", the KEY byte-equal on both
    # engines). The widening admits the ',' byte into the plain-key charset
    # (head faces probed byte-equal on BOTH engines, all DIFFER only in the
    # body: `.,`, `..,`, `.x,`, `x,`, `2,2`, `.,.`, `..,2`, `.,_`, `.,-`,
    # `.-,`, `.?,`, `.&,`, `$.`, `..,$`, `.,$x`, `.,$` (the r347 '$'
    # compositions ride), the GLUED-COLON compositions `.,:x` / `..,:x`, the
    # GLUED-QUOTE spelling `.,':` (key `.,'`, the r332 law), and the r346
    # COLON-HEAD compositions `:.,` / `:.x,`; the lead law composes unchanged
    # (CR x1/x2, CRLF, LF, spaces probed), the trailer law rides (LF, CR,
    # CRLF probed), and the r341 separator-TAB ride composes). THE DECLINES
    # THAT STAY OUT (re-measured): the comma-LEAD is the BOTH-ERROR wall
    # (never a differ), the interior-space key (`.., x`), the mid-plain-quote
    # key (`.,'x`), the ';'-led charset escape, and the quote-mid-glue face
    # (`x,y'<TAB>:`) -- all DIFFER with the family shape pre- AND post-patch,
    # stable-reds, the escalation clause's territory. THE A/B: libfyaml
    # 04e0b58 re-measured on THIS composition's own floor (the r325 lesson):
    # its --yaml-1.2 pass sides with yeptris BYTE-EXACT on the floor AND the
    # artifact (it re-emits the comma-bearing key + the raw PS + the kept tab,
    # "2e 2c 3a 20 27 e2 80 a9 09 27 0a" on the floor) while its --yaml-1.1
    # pass ERRORS ("invalid tab used as indentation" -- the documented third
    # reading) -- comparator carry only, zero src/ change, no budget raises.
    # Both shapes' 50-mutant paired battery (30 single + 20 double, seed 348,
    # unit-aware palette mutations, LS/PS triples atomic; solo runs -- the
    # r345 lesson): floor and artifact counts in the round issue -- the
    # battery's value is the 0-regression gate. See the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r348-quoted-exotic-tab-comma-plain-key
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r348.bin
    )
    add_test(
        NAME fuzz-diff-r348-quoted-exotic-tab-comma-plain-key-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r348-artifact.bin
    )

    # the r349 soup round (the nightly's run 44941446715, main e1771de9;
    # the 10740-byte artifact crash-3114175b7b973b0b87f34f5e77a83abf63a61c97
    # "---\n---\n[11 ,?##|,?<CR>/..." -- the r306-lineage soup body
    # byte-identical to the r307/r319 artifact line, riding a DOUBLE
    # document-marker lead: two consecutive "---" lines before the "["
    # flow-open, the doc-marker structural dimension's DOUBLE arrival
    # state (the single marker was never classifier territory -- the
    # entry gate declined every non-blank lead byte -- and every prior
    # soup escape carried had a single-or-no document lead). The 34-byte
    # dual-bar floor "---\n---\n[1,?#<CR>/<LF><CR><CR><NEL><CR><CR><CR>
    # <CR><CR>?,?/<NEL>]" (greedy ddmin to a fixed point under the
    # dump-pair shape bar: both dumps keep the "+STR\n=VAL : \n" empty
    # -first-document fingerprint and the first divergence is the law-a
    # split -- a naive DIFFER-only bar cannot drop the second marker:
    # the single-marker spelling renders "+STR\n+SEQ" on both engines,
    # a DIFFERENT document shape, so the byte deletions die at the
    # marker lines; pre-patch DIFFER exit 77 + NOT classified, post-
    # patch carry exit 0, on BOTH shapes, plain and lf harnesses).
    # THE MECHANISM (this round's measurement): the doc-marker lead is
    # INERT and byte-equal on BOTH engines -- each marker but the last
    # opens an EMPTY first document ("---\n[...]" -> "+STR\n+SEQ",
    # "---\n---\n[...]" -> "+STR\n=VAL : \n+SEQ", three markers -> two
    # empties; probes: spaces/tabs padding after the triple, LF/CR/CRLF
    # breaks, blank lines between markers (pure and space-carrying),
    # whole-line comments, the inline root spelling "--- [...]", and
    # four markers -- all byte-equal), and the DIFFER itself is the
    # r306-lineage body law: the glued "?#" member stays ONE flat plain
    # on yeptris (the 1.2 fold: LF CR CR NEL CRx5 -> eight newlines)
    # while libyaml fires the flow KEY token, comment-eats from the
    # '#', and rebinds the post-break bytes as the key with an empty
    # value (the law-a split + the law-b fold arithmetic). THE r349
    # WIDENING (the new law l2 in family_classify.h): the doc-marker
    # LEAD walk -- fc_doc_lead admits exactly the verified marker-line
    # shapes (the first marker at input pos 0 exactly -- an indented
    # ' ---' is plain CONTENT on both engines, the probes' "=VAL : ---"
    # doc, and stays OUT; blanks pad the marker line; LF/CR/CRLF or the
    # inline root follow; between units: spaces, LF/CR/CRLF breaks, and
    # whole-line comments; anything unverified -- a tab or exotic break
    # in the intervening run, a doc-end '...' line, the same-line
    # '--- ---' pair -- ends the lead at the last committed marker and
    # the entry gate declines the tail exactly as before). THE DECLINES
    # THAT STAY OUT (re-measured): the indented-marker lead (" ---\n---
    # \n" renders "=VAL : ---" byte-equal then the body diverges --
    # DIFFER pre- AND post-patch, stable-red, the escalation clause's
    # territory), the tab-carrying blank line between markers (never a
    # differ on the probes), the doc-end lead ("...\n---\n" -- equal),
    # and the glued "---[" fetch-law face (no blankz after the triple).
    # THE A/B: libfyaml 04e0b58 re-measured on THIS composition's own
    # floor (the r325 lesson): BOTH passes corroborate the lead --
    # "+DOC ---" + "=VAL :" empty + "+DOC ---" + the seq on --yaml-1.2
    # AND --yaml-1.1 alike -- while its body reading joins yeptris's
    # flat glued-? plain (the documented third reading; the full
    # artifact's depth errors its 1.2 pass at member ~30) -- comparator
    # carry only, zero src/ change, no budget raises. Both shapes' probe
    # matrix rides this block's history; see the round issue. Both the
    # floor and the full artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r349-flow-qsoup-doclead
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-doclead-r349.bin
    )
    add_test(
        NAME fuzz-diff-r349-flow-qsoup-doclead-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-doclead-r349-artifact.bin
    )

    # the r350 quoted-exotic-tab round (the nightly's run 44950120928,
    # main e7ef0a4a; the 26-byte artifact
    # crash-c49a3640eb8b53891f0d7053c823ba4a9616ba59
    # "  # Comment\n?  '<PS><TAB>'  \n\n\n" -- the quoted-exotic-tab
    # family's SIXTH container: the r340 comment-led explicit-key
    # container's variant where the key line is BARE `?` (zero indent --
    # the r340 artifact's own spelling was the column-1 " ? "; the
    # zero-indent face itself is already carried, probed rc=0 pre-patch
    # with the one-space gap behind the comment lead) and the post-?
    # gap is TWO spaces (the r340 gate note's exactly-one-space floor
    # was the measured boundary). The 9-byte dual-bar floor
    # "?  '<PS><TAB>'" (greedy single-byte ddmin to a fixed point under
    # the dump-pair shape bar: both dumps keep the r331 floor
    # fingerprint "=VAL : <PS>\t" vs "=VAL : <PS>", the value empty;
    # pre-patch DIFFER exit 77 + NOT classified, post-patch carry exit
    # 0, on BOTH shapes, plain and lf harnesses -- either space's
    # deletion is the ALREADY-CLASSIFIED r340 territory exit 0
    # pre-patch, so the dual bar denies both; the comment lead and the
    # "  \n\n\n" trailer die as inert). THE MECHANISM: the gap is INERT
    # on both engines -- the two-, three-, and eight-space faces all
    # DIFFER with dumps byte-exact to the r331 floor shape, identical
    # to the one-space form's. THE WIDENING: the floor form's
    # separator widens from the r340 EXACTLY-ONE-SPACE law to a
    # SPACES-RUN (the first byte after the ? must still be a space,
    # further spaces ride to the quote). THE FACES THAT STAY OUT
    # (re-measured): the space+tab, tab+space, and two-space+tab gap
    # faces surface rc=0 on the current harness (no divergence -- the
    # r341 yeptris-only-accept face's current reading; no claim
    # needed) and the gate keeps them out regardless (spaces only);
    # the glued-quote no-gap face "?'..." is rc=0 too and stays
    # declined by the first-byte-must-be-a-space law. THE A/B:
    # libfyaml 04e0b58 re-measured on THIS composition's own floor
    # (the r325 lesson): its --yaml-1.2 pass sides with yeptris
    # BYTE-EXACT on the floor AND the artifact (it re-emits the quoted
    # key with the raw PS + the kept tab,
    # "27 e2 80 a9 09 27 3a 20 0a") while its --yaml-1.1 pass ERRORS
    # ("invalid tab used as indentation" -- the documented third
    # reading) -- comparator carry only, no engine change (the
    # r318/r330/r331/r332/r340/r341/r346/r347/r348 precedent).
    # Anything the gate cannot pin declines (the escalation clause
    # stays live). Both the floor and the full artifact ride the
    # corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r350-quoted-exotic-tab-double-space-key-gap
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r350.bin
    )
    add_test(
        NAME fuzz-diff-r350-quoted-exotic-tab-double-space-key-gap-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r350-artifact.bin
    )

    # the r351 glued-hash round (the nightly's run 44956787130, main
    # 41f81ab7; the 45-byte artifact
    # crash-75907871e7b02cf327e039852c75eb9a48b715ab
    # "{?#?#,?##|#|,`|,` \n:\t , ?\t[]: #|#|+` \n,< \n\t }", 27-byte
    # dual-bar floor "{?#?#,?##|#|,`|,`\n,[] #\n,<}" — greedy single-byte
    # ddmin to a fixed point under the dump-pair shape bar (both dumps
    # byte-exact to the artifact's: yeptris keeps the four glued-hash
    # lead members as content-key pairs (`?#?#`, `?##|#|`, `` `| ``,
    # `` ` `` — the law-a/d split), libyaml comment-eats them into ONE
    # empty pair, and BOTH engines render the `[]` empty-flow-seq member
    # identically (+SEQ/-SEQ as the key, the parser's inserted empty
    # value); pre-patch DIFFER exit 77, post-patch carry exit 0, on BOTH
    # shapes): the glued-hash family's empty-flow-collationToken
    # composition. THE MECHANISM: the `[]` members are AGREEING bytes —
    # the bare `,[] #\n,` member (the comment eats inertly between the
    # closer and the separator on both engines) and the artifact's
    # `?\t[]: #|#|+` ` explicit-key member (the nested EMPTY sequence
    # hangs off the split explicit key, the GLUED ':' is the VALUE
    # boundary, the comment ride in the value slot is the r309 law) —
    # the divergence stays entirely in the lead, the documented
    # glued-hash law set (a #949 comment-eat vs the suite-pinned
    # plain-?/glued-# content law d). THE WIDENING (yd_family_classify,
    # two gates): (1) the split-explicit-key slot's nested-collection
    # carve gains the SEQ twin — the r310 `{` law's mechanics at `[`
    # (the prelude's flow-indicator carve-out covered `{` only, so the
    # `[` died as a bare terminator before the key-slot walk; the r351
    # arm mirrors the r310 gates: the GLUED or run-to colon boundary
    # with a blank/break/flow-ind follower, recursion under the depth
    # budget); (2) the bare-collection member arm rides a BLANK-preceded
    # comment between the closer and the member terminator (the
    # y_members loop-top law's own; the value slot stays EMPTY — a
    # post-comment ':' and a GLUED `]#` decline, the untested and
    # split-zone compositions). THE FACES (yepdiff-measured): every
    # probed neighbor (`[] #\n` SEQ-root twin, glued `]#`,
    # post-comment `:c`, `{}` twin, space-only, glued `:`, run-to-colon,
    # `:`+flow-ind, bare `?\t[]`, SEQ-root `?\t[]:`, `?\t{}:`, the
    # artifact's own tail) is EQUAL or a yeptris/libyaml-ONLY-ACCEPT
    # split (the gray zone the ledger documents by input) — zero
    # unclaimed-abort faces; the DIFFER twins (`{\?#?#,[] #\n,<}`,
    # `{a,?:,}`) ride the existing arms quietly. THE BATTERY: 50
    # seeded floor mutants (flip/delete/insert), paired engine-verdict x
    # harness-claim: 22 classifier-claimed, 1 arm-claimed, 18 gray-zone
    # splits, 9 both-error, ZERO unclaimed aborts. THE A/B: libfyaml
    # 04e0b58 re-measured on THIS composition's own floor (the r325
    # lesson): it ERRORS both shapes outright ("plain scalar cannot
    # start with '`'", both --yaml-1.1 and --yaml-1.2 passes) — no
    # third vote; the comparators split along the libyaml-is-the-compat-
    # bar line exactly as the r317/r350 precedent (comparator carry
    # only, no engine change). The engine-level cores are honest
    # neighbors, not this family: the DIFFER-class core `{?#,[].}` (8B)
    # and the yeptris-only-accept backtick split `{`,[]}` (6B — the
    # suite-gray zone) both stay OUT of the dual-bar floor (the shape
    # bar denies them). Seeds ride the corpus (the r321 seed law) and
    # join the nightly's comparator-snapshot exclusion (the family is
    # dense — the exclusion is the r269/r271/r298 discipline). */
    add_test(
        NAME fuzz-diff-r351-emptyflowkey-glued-hash
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-emptyflowkey-r351.bin
    )
    add_test(
        NAME fuzz-diff-r351-emptyflowkey-glued-hash-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qhash-emptyflowkey-r351-artifact.bin
    )

    # the r352 quoted-exotic-tab round (the nightly's run 44996391690,
    # main 1d179811; the 12-byte artifact
    # crash-018b47aaea96e2cc3f016b526e4bc4894e87a45e
    # "..*.: '<PS><TAB>'" -- the quoted-exotic-tab family's EIGHTH
    # composition and SEVENTH key-form container: the quoted-PS/TAB body
    # (the r331/r332/r340/r341/r346/r347/r348 law, byte-identical here)
    # riding a DOT-DOT-STAR-DOT plain key behind a STANDARD space
    # separator; the star is the alias-indicator law's neighbor -- here
    # it rides INSIDE plain key CONTENT. '*' is a plain-FIRST indicator
    # on BOTH engines, but only at the token HEAD: a star glued INTO a
    # plain run never opens an alias token, so the mid-plain/trailing
    # star faces are plain content byte-equal on both engines (probed
    # keys "..*.", ".*.", "..*", ".*", "x*x", "2*2", ".*x", ".**." --
    # all key-equal, all DIFFER only in the body). The 10-byte dual-bar
    # floor ".*: '<PS><TAB>'" (greedy single-byte ddmin under the
    # dump-pair shape bar: dot one dies, dot four dies; the STAR cannot
    # die -- its deletion ".: '<PS><TAB>'" is the ALREADY-CLASSIFIED
    # r332 territory exit 0 pre-patch (from the artifact the
    # "..: '<PS><TAB>'" deletion likewise); the dot-one deletion's own
    # star-LEAD face "*: '...'" is the BOTH-ERROR wall (re-measured:
    # yeptris errors the empty-name alias, libyaml errors too -- never
    # a differ, the dual bar denies); the space behind the colon dies
    # into the glued-quote/EQUAL face ".*:'...'" (exit 0 pre-patch --
    # no value colon ever fires); the body TAB dies into the EQUAL
    # ".*: '<PS>'" face (exit 0 -- no exotic-adjacent blank, no claim);
    # per-byte PS deletions are invalid-UTF-8 error faces and the quote
    # deletion is the unterminated-quote error; pre-patch DIFFER exit
    # 77 + NOT classified, post-patch carry exit 0, on BOTH shapes).
    # THE WIDENING: the '*' byte joins the plain-key charset, never the
    # HEAD -- the new head-guard declines the star-led key line (the
    # '&' head-guard's twin, the r238 arm owns every prop-entry shape):
    # the empty-name face is BOTH-ERROR and the named-alias face
    # "*L: '<PS><TAB>'" rides "=ALI *L" KEYS byte-equal on both engines
    # with the same body delta -- a DIFFERENT derivation ("=ALI", not
    # "=VAL"), the gate declines it, and it stays a stable-red (the
    # escalation clause's territory). Compositions ride: the
    # GLUED-COLON "..*:x", the GLUED-QUOTE "..*':" (key "..*'", the
    # r332 law), the r346 COLON-HEAD ":.*.", the r347 '$' "$.*", the
    # r348 ',' ".*,", the CR lead, the LF trailer, and the r341
    # separator-TAB ride "..*.:\t'<PS><TAB>'". THE DECLINES THAT STAY
    # OUT (re-measured): the star-LEAD both faces, the interior-space
    # key ("..* x" -- a stable-red), and the tame-body face
    # ("..*.: 'a<TAB>b'" -- EQUAL, no claim). THE A/B: libfyaml 04e0b58
    # re-measured on THIS composition's own floor (the r325 lesson):
    # its --yaml-1.2 pass sides with yeptris BYTE-EXACT on the floor
    # AND the artifact (it re-emits the star-bearing key + the raw PS +
    # the kept tab, "2e 2a 3a 20 27 e2 80 a9 09 27 0a" on the floor)
    # while its --yaml-1.1 pass ERRORS ("invalid tab used as
    # indentation" -- the documented third reading) -- comparator carry
    # only, no engine change (the r318/r330/r331/r332/r340/r341/r346/
    # r347/r348 precedent). Anything the gate cannot pin declines (the
    # escalation clause stays live). Both the floor and the full
    # artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r352-dot-star-plain-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r352.bin
    )
    add_test(
        NAME fuzz-diff-r352-dot-star-plain-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r352-artifact.bin
    )

    # the r353 paren-lead round (the nightly's run 45002991060, main
    # b197e2eb; the 13-byte artifact
    # crash-de9d378cbb8374742f6265d0f9eb2f3f64e346ea ")..*.: '<PS><TAB>'",
    # 9-byte dual-bar floor "): '<PS><TAB>'" — greedy single-byte ddmin
    # under the dump-pair shape bar; pre-patch DIFFER exit 77, post-patch
    # carry exit 0, on BOTH shapes; no landmark-preserving deletion keeps
    # the dual bar below 9 bytes — the paren's own deletion is the
    # classified r352 territory, the colon deletion loses the value
    # colon, the space deletion dies into the glued face, the quote
    # deletions are the unterminated-quote error, the per-byte PS
    # deletions are invalid-UTF-8 error faces and the body-TAB deletion
    # is the EQUAL no-adjacent-blank face): the QUOTED-EXOTIC-TAB
    # family's TENTH member and the key-form family's EIGHTH container —
    # the r352 dot-star key-form with a LEADING `)` (one InsertByte from
    # the r352 artifact pin: the seed-ride treadmill the comparator
    # snapshot law names). THE WIDENING: the ')' byte joins the
    # plain-key charset with NO head-guard — ')' is in no engine's
    # c-indicator set, so a paren HEAD is a legal plain head on BOTH
    # engines (the '&'/'*' prop-entry twins own their heads; the paren
    # owns none): the floor key `)` and the artifact key `)..*.` dump
    # BYTE-EQUAL across the engines, every DIFFER rides the body alone.
    # Head faces probed byte-equal on BOTH engines, all DIFFER only in
    # the body: `)x`, `x)x`, `))`, the GLUED-QUOTE spelling `)':` (key
    # `)'`, the r332 law), the GLUED-COLON composition `):x:` (key
    # `):x`), the r341 separator-TAB ride, the r350 two-space gap, the
    # CR lead, the LF trailer, and the dot compositions `.).` and
    # `).*.`. THE DECLINES THAT STAY OUT (re-measured): the '&' and '*'
    # heads (the prop-entry arms; the empty-name face the BOTH-ERROR
    # wall), the empty-key split (`: '<PS><TAB>'` — rc=0), the
    # tame-body face (`): 'a<TAB>b'` — EQUAL, no claim needed), and the
    # two STABLE-REDS that stay out with their pre- and post-patch 77s
    # re-measured (the interior-space key `) x: '...'` and the
    # stuck-quote face `):': '...'`, key `):'` — the quote after the
    # glued colon is outside the charset walk; the escalation clause's
    # territory, carried documented-red like the r352 interior-space
    # twin). THE A/B:
    # libfyaml 04e0b58 re-measured on THIS composition's OWN floor (the
    # r325 lesson): its --yaml-1.2 pass sides with yeptris BYTE-EXACT on
    # the floor AND the artifact (it re-emits the paren key + the raw PS
    # + the kept tab, "29 3a 20 27 e2 80 a9 09 27 0a" on the floor)
    # while its --yaml-1.1 pass ERRORS ("invalid tab used as
    # indentation" — the documented third reading) — comparator carry
    # only, no engine change (the r318/r330/r331/r332/r340/r341/r346/
    # r347/r348/r352 precedent). Anything the gate cannot pin declines
    # (the escalation clause stays live). Both the floor and the full
    # artifact ride the corpus (the r321 seed law) and join the nightly
    # comparator-snapshot exclusion.
    add_test(
        NAME fuzz-diff-r353-paren-lead-plain-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r353.bin
    )
    add_test(
        NAME fuzz-diff-r353-paren-lead-plain-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r353-artifact.bin
    )

    # the r354 alnum-with-bracket round (the nightly's run 45010144045,
    # main e90b43fe; the 11-byte artifact
    # crash-b472e1d7865a62ebd2139ac37ef0721aa13361c4 "A[.: '<PS><TAB>'",
    # 10-byte dual-bar floor "A[: '<PS><TAB>'" — greedy single-byte ddmin
    # under the dump-pair shape bar: only the dot dies (key "A[" byte-
    # equal throughout); the bracket's deletion is the ALREADY-CLASSIFIED
    # r350 alnum territory, the A's deletion is the bracket-LEAD wall
    # (exit 0, no divergence), the colon deletion loses the value colon,
    # the space deletion dies into the glued face, the quote deletions
    # are the unterminated-quote error, the per-byte PS deletions are
    # YEPTRIS-ONLY-ACCEPT invalid-UTF-8 split faces and the body-TAB
    # deletion is the EQUAL no-adjacent-blank face; pre-patch DIFFER
    # exit 77, post-patch carry exit 0, on BOTH shapes): the
    # QUOTED-EXOTIC-TAB family's ELEVENTH member and the key-form
    # family's NINTH container — the FLOW-SEQ-OPEN indicator riding
    # INSIDE an ALNUM-LED plain key (the r353 paren-lead was a CLOSING
    # char at the line START; this is an OPENING bracket mid-key). THE
    # WIDENING: the '[' byte joins the plain-key charset, never the
    # HEAD — the new head-guard declines the bracket-led key line (the
    # r352 star twin): '[' at the first byte opens a FLOW COLLECTION, a
    # derivation this arm does not own — the unterminated faces
    # "[: '<PS><TAB>'" and "[A: '<PS><TAB>'" are the BOTH-ERROR wall
    # (never a differ) and the CLOSED flow face "[A: '<PS><TAB>']"
    # reads EQUAL (the flow-context quoted body does not diverge); the
    # dual bar denies every bracket-lead shape, so the bracket rides
    # mid-plain CONTENT only (the 1.2 ns-plain-char law excludes the
    # flow indicators only in flow context — key byte-equal on both
    # engines, every DIFFER rides the body alone). Compositions ride:
    # "A[", "x[x", "2[2", "A[A", "A[[", the GLUED-COLON "A[:x:" (key
    # "A[:x"), the GLUED-QUOTE "A[':" (key "A['", the r332 law), the
    # r346 COLON-HEAD ":[A:", the r341 separator-TAB ride
    # "A[.:<TAB>'<PS><TAB>'", the CR x1 lead, and the LF trailer. THE
    # DECLINES THAT STAY OUT (re-measured): the bracket-LEAD wall
    # faces, the interior-space key ("A[ x" — a stable-red), the ']'
    # mid-key face ("A]." — key "A].", DIFFER body-only, stays OUT of
    # the charset — the closing bracket is the NEXT treadmill round's
    # byte, carried documented-red), and the tame-body face
    # ("A[.: 'a<TAB>b'" — EQUAL, no claim). THE A/B: libfyaml 04e0b58
    # re-measured on THIS composition's OWN floor (the r325 lesson):
    # its --yaml-1.2 pass sides with yeptris BYTE-EXACT on the floor
    # AND the artifact (it re-emits the bracket key + the raw PS + the
    # kept tab, "41 5b 3a 20 27 e2 80 a9 09 27 0a" on the floor) while
    # its --yaml-1.1 pass ERRORS ("invalid tab used as indentation" —
    # the documented third reading) — comparator carry only, no engine
    # change (the r318/r330/r331/r332/r340/r341/r346/r347/r348/r352/
    # r353 precedent). Anything the gate cannot pin declines (the
    # escalation clause stays live). Both the floor and the full
    # artifact ride the corpus (the r321 seed law) and join the nightly
    # comparator-snapshot exclusion.
    add_test(
        NAME fuzz-diff-r354-alnum-bracket-plain-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r354.bin
    )
    add_test(
        NAME fuzz-diff-r354-alnum-bracket-plain-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r354-artifact.bin
    )

    # the r375b plus round (the r375 drill session's own bounded
    # comparator soak, 45s from the nightly's exact corpus discipline;
    # NOT a nightly red): the 15-byte artifact `+MAP...: '<PS><TAB>'`
    # rides the QUOTED-PS/TAB body (the r331 family law, byte-identical)
    # behind a PLUS-LED plain key — '+' is in NO engine's c-indicator
    # set (a legal plain head on both engines, no head-guard — the
    # r353 paren law's twin), and the pre-patch charset walk read no
    # key bytes at the '+' head (ke == ko, the empty-key decline), so
    # the DIFFER stayed unexplained. The dual-bar floor under the
    # family SHAPE bar (key rows byte-equal + the body delta; the
    # r343 lesson) is the 9-byte `+: '<PS><TAB>'`. Head faces probed
    # byte-equal on both engines, all DIFFER only in the body: `+`,
    # `+x`, `x+`, `2+2`, `+.`, `+,`, `+)`, `+;`, `+[`, `+'` (glued
    # quote, the r332 law), `+\t:` (separator TAB, the r341 law); the
    # `+:x` glued-colon and `+$:` faces are EQUAL (never a consult).
    # THE A/B: libfyaml 04e0b58's --yaml-1.2 pass sides with yeptris
    # BYTE-EXACT on floor and artifact (the raw PS + the kept tab)
    # while its --yaml-1.1 pass errors "invalid tab used as
    # indentation" — comparator carry only, no engine change. Both
    # seeds join the quoted-exotic-tab comparator-snapshot exclusion
    # (the family's mutation density is the treadmill this snapshot
    # exists to stop).
    add_test(
        NAME fuzz-diff-r375b-plus-plain-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r375b.bin
    )
    add_test(
        NAME fuzz-diff-r375b-plus-plain-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r375b-artifact.bin
    )

    # the r360 semicolon round (the nightly's run 45056549705, main
    # 336bbaeb; the 12-byte artifact
    # crash-0d590a8d210c3fb74d70a683a3b16b6db10b3fb6 "...;: '<PS><TAB>'",
    # 9-byte dual-bar floor ";: '<PS><TAB>'" — greedy single-byte ddmin
    # under the dump-pair shape bar: the three dots die one at a time
    # (key-equal throughout); the semicolon's deletion is the
    # ALREADY-DOCUMENTED empty-key YEPTRIS-ONLY-ACCEPT split, the colon
    # deletion loses the value colon, the space deletion dies into the
    # glued face, the quote deletions are the unterminated-quote error,
    # the per-byte PS deletions are YEPTRIS-ONLY-ACCEPT invalid-UTF-8
    # split faces and the body-TAB deletion is the EQUAL
    # no-adjacent-blank face; pre-patch DIFFER exit 77, post-patch
    # carry exit 0, on BOTH shapes): the QUOTED-EXOTIC-TAB family's
    # TWELFTH member and the key-form family's TENTH container — the
    # SEMICOLON riding INSIDE plain key CONTENT (the r348 comma twin;
    # the ';'-lead spelling itself was the r347 decline note's
    # stable-red, now measured and carried). THE WIDENING: the ';'
    # byte joins the plain-key charset, never needing a HEAD-GUARD —
    # ';' is in NO engine's c-indicator set and is plain-FIRST on
    # NEITHER engine (the r353 paren law's twin: a semicolon head is a
    # legal plain head on BOTH engines, the '&'/'*' prop twins and the
    # r354 '[' flow twin own their heads, the paren and the semicolon
    # own none) — the artifact's key "...;" and the floor's key ";"
    # dump BYTE-EQUAL across the engines, every DIFFER rides the body
    # alone. Compositions ride: ";x", "x;x", ";;", ";.", ".;", "2;2",
    # the r347 '$' composition ";$", the r352 '*' composition ".*;",
    # the r353 ')' composition ");", the r354 '[' composition "A[;",
    # the GLUED-COLON ";:x:" (key ";:x"), the GLUED-QUOTE ";':" (key
    # ";'", the r332 law), the r346 COLON-HEAD ":;:" (key ":;"), the
    # r341 separator-TAB ride ";:<TAB>'<PS><TAB>'", the CR x1 lead, and
    # the LF + CRLF trailers. THE DECLINES THAT STAY OUT (re-measured):
    # the interior-space key ("; x" — a stable-red), the mid-plain-quote
    # key (";'x" — a stable-red), the '/'-led charset escape ("/:"
    # — DIFFER with the family shape pre- AND post-patch — the NEXT
    # treadmill round's byte, carried documented-red like the r354
    # ']'), the ']' mid-key face ("A]." — the r354 note), the comma-LEAD
    # composition (",;" — the BOTH-ERROR wall, never a differ), the
    # tame-body face (";: 'a<TAB>b'" — EQUAL, no claim), and the
    # empty-key split (": '<PS><TAB>'" — the YEPTRIS-ONLY-ACCEPT
    # banner, unchanged). THE A/B: libfyaml 04e0b58 re-measured on THIS
    # composition's OWN floor (the r325 lesson): its --yaml-1.2 pass
    # sides with yeptris BYTE-EXACT on the floor AND the artifact (it
    # re-emits the semicolon key + the raw PS + the kept tab, "3b 3a 20
    # 27 e2 80 a9 09 27 0a" on the floor) while its --yaml-1.1 pass
    # ERRORS ("invalid tab used as indentation" — the documented third
    # reading) — comparator carry only, no engine change (the r318/
    # r330/r331/r332/r340/r341/r346/r347/r348/r352/r353/r354
    # precedent). Anything the gate cannot pin declines (the escalation
    # clause stays live). Both the floor and the full artifact ride the
    # corpus (the r321 seed law) and join the nightly
    # comparator-snapshot exclusion.
    add_test(
        NAME fuzz-diff-r360-semicolon-plain-quoted-exotic-tab
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r360.bin
    )
    add_test(
        NAME fuzz-diff-r360-semicolon-plain-quoted-exotic-tab-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-exotic-tab-r360-artifact.bin
    )

    # the r361 tag-soup round (the nightly's run 45066041954, main
    # d90ffbc4; the 148,798-byte artifact
    # crash-1f709c41ac13d656c4c6a641201adc458d0a4892 — the biggest
    # classifier-era nightly artifact; the 73,133-byte floor
    # bd8d4d466a98183e450d48f493de7078d2d9a5b54db682e2fd4a6df4f7344dee
    # — the dual bar (unclassified DIFFER, run-twice confirmed) under
    # prefix/suffix/window/prefix+closer/middle-blank-fill/line-shape
    # thinning probes, which ALL leave the band, then byte-class ddmin:
    # 30 byte classes thin, every class keeps exactly one copy ('.'
    # -6094, 'a' -5087, ':' -4930, '|' -3010, 'e' -2908, '7' -2900,
    # 'y' -2834, 'r' -2271, 'b' -1904, 'i' -1856, '&' -938, 'l' -879,
    # 'o' -634, '-' -569, '"' -184, 'M' -76, TAB -61, '0' -2580 of the
    # head classes; CRs/commas/'#'/'?'/'1's keep one each — the tail
    # classes are load-bearing), so the floor is a SIZE-BOUND face: the
    # engines' flow readings of the early members depend on the
    # collection's full extent, and the composition is the r190
    # nightly's OWN artifact family grown past every arm's reach (the
    # r190 pin IS this soup truncated at 32,772 bytes — its spell's
    # all-hits rewrite at r361 scale is 3,942 deletions that no longer
    # re-parse on either engine, the byte-strict verify cannot
    # arbitrate, measured BOTH-ERROR): the faces are all carried laws
    # (the 2SXE/HM87 plain-'?', the r132/r187/r224 unconditional '#'
    # comment-eat, the r56/r240 PS-raw-ride vs PS-normalized fold, the
    # r76/r318 anchor-charset cut — the CR-GLUED-HASH member
    # "51:<CR>#|,?" is the fresh spelling no tail grammar models), the
    # composition at artifact scale is the suite-gray zone, documented
    # by input per the r129 exact-bytes law, scaled by SIZE + SHA-256
    # content hash (the bytes ride the corpus pins; a hash stands in
    # for the r129 inline array because the shapes are 220KB combined).
    # THE A/B: libfyaml 04e0b58 declines BOTH passes on the floor
    # candidate ("plain scalar cannot start with '>'" at the '>-led
    # member, --yaml-1.2 AND --yaml-1.1 alike) while the 18-byte
    # classified core "[11 ,?##|,?<CR>/<LF>&m,]" re-emits yeptris's
    # reading BYTE-SHAPED in both passes ("[11, ?##|, {/ &m: }]") —
    # comparator carry only, no engine change (the
    # r318/r330/r331/r340/r346/r356 precedent). Arm pin: the floor and
    # the artifact run deterministically (every reduction probe ran
    # twice). pre-patch DIFFER exit 77 / abort, post-patch carry
    # exit 0, on BOTH shapes.
    add_test(
        NAME fuzz-diff-r361-tag-soup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-tag-soup-r361.bin
    )
    add_test(
        NAME fuzz-diff-r361-tag-soup-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-tag-soup-r361-artifact.bin
    )

    # the r362 round (the nightly's run 45078029712, main 22bf9231; the
    # mutation crash-1814d75b207ad9f267bacaaf430467140f36681a, sha256
    # fc2222e3...): the r361 floor's ONE-BYTE mutation (offset 56680,
    # '5' -> '4' in the "m,15t" member) escaped the SIZE + SHA-256 pin
    # and the nightly aborted unclassified. THE DIG (this round's
    # measured finding): the engines' flow readings are SIZE-
    # INDEPENDENT on this family — the glued-`?` face ("?##|" plain on
    # yeptris, "+MAP" on libyaml) is byte-identical at 8 bytes
    # ("[11,?##]") and at 73KB, and no engine budget consults the
    # collection's extent. The binding gates were the CLASSIFIER's
    # conservatism budgets (the floor's 8,243 members vs
    # FC_MAX_MEMBERS 8192; the artifact's 148,798 bytes vs
    # FC_MAX_INPUT 131072 and its ~25K-token libyaml walk vs
    # FC_MAX_TOKENS 16384). THE CARRY: the r362 budget raise — the
    # family classifier now derives BOTH engines' actual dumps
    # byte-exact on the floor, the mutation, AND the artifact, so the
    # whole composition rides the LAW at arbitrary scale (the
    # exact-byte pins above stay as belt-and-braces; no new pin for
    # the mutation). pre-patch DIFFER exit 77 / abort, post-patch law
    # carry exit 0.
    add_test(
        NAME fuzz-diff-r362-tag-soup-mutation
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-tag-soup-r362-mutation.bin
    )

    # the r355 anchor-blank-alias round (the nightly's run 45016455586,
    # main b3b8b07f; the 18-byte artifact
    # crash-87fa092da7c94269ad4500d91d33fc49bb928276 "{?*L:<LF>,&m:=ALI *(}",
    # 16-byte dual-bar floor "{?*L,&m:=ALI *(}" — greedy single-byte ddmin
    # under the artifact dump-pair shape bar: only the key colon and the
    # break die (both engines fill the missing value with the inserted
    # empty, dumps byte-identical throughout); the 13-byte role-bar twin
    # "{?*L,&m:I *(}" drops the "=AL" dump-vocabulary prefix (any
    # non-empty plain tail keeps the face role); pre-patch DIFFER exit 77
    # + abort, post-patch carry exit 0, on ALL THREE shapes): the
    # FLOW-SOUP family's anchor-charset arm grown past a BLANK — the r318
    # key-map artifact "{?*L:<LF>,&m:(}" (pinned) with the dump-vocabulary
    # bytes "=ALI " inserted before the trailing "(": one InsertByte from
    # a pinned seed, the per-round arm-extension treadmill this carry
    # exists to stop. THE WIDENING (family_classify.c, the r318 MAP-key
    # arm's sibling branch): the engine's prop cut ends at the
    # blank/break run (`m:=ALI` — the r318 suite-pinned charset keeps the
    # glued ':'), and the next token decides the node: the `*` fires the
    # engine's alias walk, whose name cut rides to the member terminator
    # ("(" rides the alias charset — the r342/r353 paren law's charset
    # neighbor, probed "{*(: 1}" -> "=ALI *("); the anchor prop does NOT
    # render on the alias event, so the member derives as the bare
    # "=ALI *<name>" plus the parser's inserted empty, while libyaml's
    # walk derives its own reading independently (the IS_ALPHA cut at
    # "m", the glued ':' re-lexed as the VALUE token, the tail as ONE
    # flat plain — mid-plain "*(" is content) and the entry's dual
    # byte-equality is the firewall. Probes byte-exact both sides:
    # "{&m:=ALI *(}", "{&m:I *(}", "{&m:ALI *(}" (all DIFFER with this
    # shape), the ALIAS-START twin "{&m: *x}" (the same y-side; libyaml's
    # value is an ALIAS event there, its valid IS_ALPHA name), and the
    # plain-leading twins that keep the alias INSIDE the plain ("{a:I
    # *(}", "{a:*(}" — EQUAL, the anchored-plain ride "{&m:  I *(}" /
    # "{&m:<LF>I *(}" already classifies through the plain arms). THE
    # DECLINES THAT STAY OUT (re-measured): the star-first "("-name face
    # "{&m: *(}" is libyaml's scan_anchor ERROR (a split, the ledger's
    # territory), the alias-with-value faces "{&m: *x: 1}" (libyaml
    # errors after the alias value), and the SEQ slot "[&m: I *(]"
    # (libyaml opens a nested MAP — a different derivation). THE A/B:
    # libfyaml 04e0b58 errors the floor and the artifact outright
    # ("unexpected alias" at the star, both --yaml-1.2 and --yaml-1.1
    # passes) — no third vote, the comparators split along the
    # libyaml-compat-bar line exactly as the r335/r317 precedent did,
    # and the underlying charset laws are suite-pinned (2SXE/W5VH;
    # libyaml is behind the suite, the r311 law) — comparator carry
    # only, no engine change (the r318/r330/r331/r332/r340/r341/r346/
    # r347/r348/r352/r353/r354 precedent). Anything the gate cannot pin
    # declines (the escalation clause stays live). Both the floor and
    # the full artifact ride the corpus (the r321 seed law) and join the
    # nightly comparator-snapshot exclusion (the flow-qsoup* pattern).
    add_test(
        NAME fuzz-diff-r355-anchor-blank-alias-flow-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-anchor-blank-alias-r355.bin
    )
    add_test(
        NAME fuzz-diff-r355-anchor-blank-alias-flow-qsoup-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-anchor-blank-alias-r355-artifact.bin
    )

    # the r357 alias-glued-colon round (the nightly's run 45033480083,
    # main 1e53f6b7; the 9-byte artifact
    # crash-b328cda046b9de5a068400380805697a5e5c985b "{*L:,&-:}", which
    # IS its own floor — greedy single-byte ddmin under the artifact
    # dump-pair shape bar kept every byte: each drop-one either exits 0
    # (classified/equal — the brace drop falls to block context, the
    # comma drop merges the members, the '&' drop leaves a plain "-:"
    # key) or dies the shape bar): the r318 MAP-key anchor-charset face
    # ("&-:" — the engine's prop charset keeps the glued ':' in the
    # name, "=VAL : &-: ", while libyaml's IS_ALPHA cut hands the ':' to
    # the VALUE token, "=VAL : &- " + the inserted empty) composed with
    # an ALIAS first member whose glued ':' is the VALUE boundary — the
    # face the r318/r355 members never had. THE WIDENING
    # (family_classify.c, the bare-alias member arm): the engine's
    # alias-name walk closes BEFORE a colon that a flow indicator
    # follows (the r342 probed cut, "*00:," — the artifact's own y-dump
    # "=ALI *L" + "=VAL : " pins the same face at the flow MAP key
    # slot), and the glued ':' fires the VALUE token on BOTH engines
    # (libyaml's flow fetch_value on the alias's saved simple key;
    # probed "{*L:,}" EQUAL pre-patch), so the member derives as the
    # alias key + the parser's inserted empty and the walk reaches the
    # divergent second member, which the r318 arm already derives. The
    # entry's dual byte-equality is the firewall. THE DECLINES THAT STAY
    # OUT (re-measured): the content follower "{*L:x}" and the SEQ slot
    # "[*L:,]" are EQUAL — never a DIFFER, no claim exists; the break
    # follower rides the r342 law INTO the alias name (the split-key
    # family's territory). THE A/B: libfyaml 04e0b58 reads the
    # artifact's divergent member byte-exact yeptris's way — the key
    # node's anchor is "<-:" glued, empty value, BOTH --yaml-1.2 and
    # --yaml-1.1 passes (the r318 seed "{&m:(}" corroborates, anchor
    # "<m:(>"); its alias member is the documented third reading — the
    # charset law suite-pinned (2SXE/W5VH; libyaml behind the suite,
    # the r311 law) — comparator carry only, no engine change (the
    # r318/r330/r331/r332/r340/r341/r346/r347/r348/r352/r353/r354/r355/
    # r356 precedent). The artifact rides the corpus (the r321 seed
    # law; floor == artifact) and joins the nightly comparator-snapshot
    # exclusion (the flow-qsoup* pattern).
    add_test(
        NAME fuzz-diff-r357-alias-glued-colon-key-flow-qsoup
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-alias-glued-colon-key-r357.bin
    )

    # the r333 fold-header round (the nightly's run 44774423077, main
    # feb599b5; the 24-byte artifact crash-501717408099949c4c94a58f39949f40b6922ce3
    # "> #<PS> :\n  <NEL> :\n <PS><NEL>#0", 14-byte dual-bar floor
    # "> #<PS>\n<NEL><PS>#0" — greedy ddmin to a fixed point; pre-patch
    # DIFFER exit 77, post-patch carry exit 0, on BOTH shapes; no
    # landmark-preserving single-unit deletion keeps the dual bar — the
    # 10th composition of the folded-header family, the first where
    # yeptris's WHOLE value reads normalized): the document-level break
    # normalizer (#638) turned the all-blank-per-1.2 body's exotic
    # breaks into the LF escapes the fold ledger spells literally, the
    # glued "#0" line rides as the block's one content line at the
    # auto-indent (yeptris), while libyaml's 1.1 reader ends the block
    # at the column-0 line and re-lexes the "#0" as the document-level
    # comment eaten to EOF (the clip joins the EMPTY leading_break, the
    # value is EMPTY). The artifact's mid-body face rides the r317/r208
    # pairing class (the more-indented kept-NEL line) and the new
    # comment-eaten EOF-tail arm; the floor rides the new
    # normalized-blank arm (behind the r321 gate, disjoint). THE A/B:
    # libfyaml 04e0b58 reads BOTH shapes an EMPTY STREAM
    # (fy-tool --testsuite --yaml-1.2: "+STR\n-STR", zero documents —
    # re-measured on this composition's own floor after the r325
    # lesson), so the dual-engine consensus law does not carry the
    # family here either — comparator carry only, no engine change (the
    # r317 precedent). The over-broad guard holds: the non-'#' tail
    # twins ("x0", the libyaml second-document class) and the
    # structural tails ("#0\n|\n y") still decline; both shapes' 50
    # -mutant paired battery (30 single + 20 double, seed 333): see the
    # round issue. Both the floor and the full artifact ride the corpus
    # (the r321 seed law).
    add_test(
        NAME fuzz-diff-r333-folded-header-comment-tail
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-comment-tail-r333.bin
    )
    add_test(
        NAME fuzz-diff-r333-folded-header-comment-tail-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-comment-tail-r333-artifact.bin
    )

    # the r334 folded-header round (the nightly's run 44793963941, main
    # af1bacea; the 45693-byte artifact -- the folded-header family
    # CROSSING INTO the tag-soup composition: the first document's
    # "> " fold carries the kept-NEL delta while the trailing tag soup
    # ("!!int :app-" / "!intege" / digit-run plains behind "---"
    # markers) byte-matches on both engines -- and its 28-byte dual-bar
    # floor "> \n<NEL><NEL>\n <NEL><NEL><NEL><NEL>\n <NEL><NEL><NEL>
    # <NEL> <" -- greedy ddmin to a fixed point, UTF-8-validity
    # constrained; pre-patch DIFFER abort, post-patch carry exit 0, on
    # BOTH shapes, plain and lf harnesses): the r216-class
    # surplus-escape mirror widens y==1 -> y>=1 -- the blank-line
    # ledger. libyaml's join spells one escape per break it read (the
    # r320 ledger law), every blank-per-1.1 line riding a kept-NEL site
    # adds exactly one surplus escape on its side while yeptris's 1.2
    # fold collapsed that run into the single LF the byte-match paired
    # -- so the surplus accumulates per blank-adjacent NEL site (the
    # floor's decline sites hold y=2, byte-traced on this composition's
    # own floor; the r216 floor's y==1 member keeps its exact path --
    # the widening admits shapes that previously declined
    # unconditionally, so no existing member's path changes; unbounded
    # in principle -- blank runs are unbounded, the r321 all-blank
    # precedent -- and bounded in fact by the coincidence + plain-byte
    # + frame guards with the byte-bound continuation binding the
    # rest). THE A/B: libfyaml 04e0b58 reads the artifact's first
    # document byte-exact as yeptris (the kept-NEL 1.2 reading,
    # fy-tool --testsuite --yaml-1.2) while its --yaml-1.1 pass matches
    # libyaml 90a56d4 escape-for-escape -- the comparators split along
    # the version line exactly as the r317 precedent did -- comparator
    # carry only, no engine change. Floor exhaustion: no
    # landmark-preserving single-unit deletion keeps the dual bar (pre
    # DIFFER + post classify) under the UTF-8 validity constraint; the
    # unconstrained byte minimizer drifts to the 4-byte "\n<c2>" twin,
    # a different (encoding-face) divergence that stays out. Battery
    # (100 mutants, 30 single + 20 double per shape, seed 334): floor
    # 24 admitted/equal + 26 ledger-enforced splits + 0 declines + 0
    # crashes; artifact 27 newly-admitted (pre DIFFER -> post
    # classified) + 22 splits + 1 admitted/equal + 0 declines + 0
    # crashes; 0 over-restrictions (no mutant the old walk admitted
    # turned red). Both the floor and the full artifact ride the corpus
    # (the r321 seed law).
    add_test(
        NAME fuzz-diff-r334-folded-header-blank-ledger
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-blank-ledger-r334.bin
    )
    add_test(
        NAME fuzz-diff-r334-folded-header-blank-ledger-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/folded-header-blank-ledger-r334-artifact.bin
    )

    # the r335 non-specific-tag round (the nightly's run 44808502141,
    # main af1bacea; the 18-byte artifact [!,?#,"\rg",?g?,""] -- the
    # glued-?/# comment-piece family CROSSING INTO the tag-member
    # composition: a flow SEQ whose members are the bare `!` tag, the
    # glued-`?#` piece, a double-quoted body carrying a RAW CR, the
    # bare `?g?`, and the empty "" tail). The 15-byte dual-bar floor
    # [!,?#,"\rg",?g?] (greedy ddmin to a fixed point under the dual
    # bar; pre-patch DIFFER abort, post-patch carry exit 0, BOTH
    # shapes, plain and lf harnesses): the y-side NON-SPECIFIC-TAG
    # MEMBER arm. The bare `!` riding a flow-SEQ member slot resolves
    # to the non-specific tag; the member's flow-indicator terminator
    # leaves the tagged node EMPTY, the tag drops from the dump
    # (resolves by style), and BOTH engines render `=VAL : ` --
    # byte-exact, the classifier's l-walk already models the bare-`!`
    # drop in l_render_node, so the y-side arm is the last model gap.
    # Gates: the bare `!` EXACTLY (a `!name`/`!!` handle or a
    # `!<verbatim>` form carries dump props and a scalar payload --
    # untested, the single-byte adjacency keeps them out), the flow
    # indicator GLUED after the tag (a blank/break run opens the
    # tagged scalar-value slot the probes never cover -- the r315
    # anchor-arm run shape, declined on the tag slot; the battery's
    # `[! ,?...` mutant proves the gate), and the SEQ slot (a MAP key
    # slot renders the tagged-empty KEY plus the parser's inserted
    # empty -- untested). The other candidate faces ride existing law:
    # the quoted-CR body via the r308 member-led quoted-span fold
    # (CR is a break like any other; the ONE-break fold joins the
    # close... here the interior fold renders ` g` plain on yeptris
    # byte-exact) and the r330 law's CR-as-comment-terminator (the
    # law-a comment-eat stops at the CR via fc_break_len, the post-CR
    # bytes rebind as libyaml's explicit key `g"`), the `?g?` tail
    # via the law-d plain-`?` walk (fc_flow_ind's break list has no
    # `?`, so the trailing indicator rides content on BOTH engines).
    # THE A/B: libfyaml 04e0b58 errors the artifact outright
    # ("invalid tag terminator", both --yaml-1.1 and --yaml-1.2
    # passes) -- no third vote; the comparators split along the
    # libyaml-is-the-compat-bar line exactly as the r317 precedent
    # did -- comparator carry only, no engine change. Floor
    # exhaustion: the ddmin drops exactly the `,""` tail member (the
    # quoted-empty arm carries it on the artifact); no other
    # single-unit deletion keeps the dual bar. Battery (100 mutants,
    # 30 single + 20 double per shape, seed 335): floor 36
    # admitted/equal + 13 newly-admitted + 1 declined + 0
    # over-restrictions + 0 crashes; artifact 39 admitted/equal + 10
    # newly-admitted + 1 declined + 0 over-restrictions + 0 crashes
    # (the declines: the battery's `[! ,?...` blank-after-tag shape
    # and the double-CR quoted body -- both correctly gated).
    # Both the floor and the full artifact ride the corpus (the r321
    # seed law).
    add_test(
        NAME fuzz-diff-r335-tag-member
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tag-r335.bin
    )
    add_test(
        NAME fuzz-diff-r335-tag-member-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tag-r335-artifact.bin
    )

    # r336 (run 44822270874, main 71939ad1): the r238 root-anchor-
    # quoted-follow family's third composition of the r330
    # neighborhood -- the artifact `&2:: '<PS><CR>'` (11 bytes) is the
    # r330 seed with the name tail's trailing colon DOUBLED. yeptris
    # keeps the whole 1.2 ns-anchor-char name `2::` (colons are legal
    # anchor chars; the quoted PS/CR fold follows the r308 g2 law --
    # the PS kept raw, the CR folded to the one space) and reads ONE
    # root anchored scalar, while libyaml 90a56d4 cuts the name at
    # the FIRST out-of-charset byte (the ':'), the cut residual `:`
    # re-lexes as the key's PLAIN content (a ':' glued to a non-blank
    # is content -- the block plain-first gate at scannerc.c's
    # fetch_next_token), the freed ': ' fires the VALUE token, and
    # the quoted scalar parses as the map value (`+MAP / =VAL : &2 :
    # / =VAL : <PS>\n`). The r330 next-round pointer fires verbatim:
    # "a residual containing ':' ... stays DECLINED, a red there
    # means the plain-content law needs its own measurement round."
    # The single-break bodies (PS-only `&2:: '<PS>'`, CR-only
    # `&2:: '<CR>'`) are already classified by the anchor-name-
    # charset arm (its split copy agrees there); under the PS+CR body
    # the split copy is yeptris-rejected (the indent law -- the PS is
    # a 1.2 line break and the map-value continuation sits at column
    # 0) so the charset arm's verify leg dies, exactly as at r330.
    # THE A/B (probed): libfyaml 04e0b58 reads yeptris's artifact
    # stream byte-exact (SCALAR anchor `2::` single-quoted value
    # `<PS>+<SP>` on both; the CR-only floor reads the bare fold
    # space) -- comparator carry only, no engine change (the
    # r318/r330 precedent). THE CARRY: the r238 arm's residual law
    # gains the PURE glued-colon run beside the r330 '?'-led
    # plain-safe run (byte-exact walks on both sides; every interior
    # ':' glues to the next non-blank byte -- the rest of the run or
    # the name's riding ':'). Floor: the artifact is its own dual-bar
    # floor (11 bytes, greedy ddmin fixed point -- dropping the PS or
    # the CR lands on charset-arm-classified single-break bodies
    # (probed pre-patch: exit 0) and dropping either colon lands on
    # the r238 root arm, so no single-byte deletion keeps the
    # pre-patch bar's exit 77). Both the floor and the artifact ride
    # the corpus (the r321 seed law; here they are byte-identical).
    # Next-round pointer: a residual MIXING ':' into the '?'-led run
    # (the `?:x`-class) or led by '*'/'.' stays DECLINED (unmeasured
    # shapes); a red there means the mixed plain-content law needs
    # its own measurement round.
    add_test(
        NAME fuzz-diff-r336-root-anchor-quoted-follow-colonrun
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-colonrun-r336.bin
    )
    add_test(
        NAME fuzz-diff-r336-root-anchor-quoted-follow-colonrun-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-colonrun-r336-artifact.bin
    )

    # r337 (run 44832174407, main ffc37f20): the r330/r331 quoted-body
    # family's next composition of the #949 KEY-token comment-glue. The
    # artifact `[1,?#,"+S:(<LF><LF>T |U:(<LF><LF>T |U %- |V|r<LF>- 42
    # <LF>T |U %<CR>g",?g:m]` (60 bytes) hangs a double-quoted member
    # (LF escapes, a raw CR fold, a `- 42` block-entry token inside the
    # body) between the glued `?#` member and the `?g:m` member.
    # yeptris's 1.2 laws keep `?#` ONE plain (HM87/652Z: a flow '?' with
    # a non-blank follower rides as plain content; a '#' comments only
    # behind a blank), parse the span normally (the `\n` escapes decode,
    # the CR folds to the one space, the `- 42` is body text) and keep
    # `?g:m` a second glued plain; libyaml's flow-'?' KEY-token law
    # fires on ANY follower and its scan-to-next-token then comment-eats
    # the glued '#' to the next break — the run swallows `,"+S:...%-`
    # up to the interior raw CR, crossing the span opener and the `- 42`
    # token alike, and the pending KEY re-pairs with the post-break
    # plain `g"` (the 1.1 plain-scan rides the span CLOSER as plain
    # content — a quote is not a plain stop). The dumps: two single-pair
    # salvage wrappers ({g": empty}, {g:m: empty}) against yeptris's
    # quoted scalar and the `?g:m` plain. libfyaml 04e0b58 reads the
    # artifact stream matching yeptris's reading (three members, the
    # escaped continuation dropped) — comparator carry only, no engine
    # change (the r317/r330/r336 precedent). THE CARRY: the qflow walk's
    # salvage-block arm gains the r337 quoted-swan reading — the pass-1
    # marker pins a glued `?#` whose comment-run carries a '"' and
    # terminates at a break; a well-formed salvage block
    # (+MAP / "=VAL : k"" / "=VAL : " / -MAP) is claimed against
    # yeptris's marker member plus the swallowed span member when the
    # KEY minus its closing quote is the byte-exact suffix of the span's
    # content; the family_classify model gains the matching y-side arms
    # (the `\n` escape decode and the escaped-break continuation, both
    # under tight no-pending-fold/no-pending-blank gates; every other
    # escape follower stays UNKNOWN). Dual-bar floor: 17 bytes
    # (`[1,?#,"\<CR>g",?g:m]`, greedy ddmin fixed point — the swallow
    # must cross the span opener AND terminate at a break for both
    # engines to parse; dropping the CR lands on libyaml-error shapes,
    # dropping the trailing `?g:m` member rides the existing r167
    # empty-wrapper arm). Both the floor and the artifact ride the
    # corpus (the r321 seed law). Next-round pointer: a swallow crossing
    # TWO span openers, or a KEY riding a doubled continuation
    # (`\<CR><CR>g`), stays DECLINED (unmeasured shapes); a red there
    # means the multi-continuation KEY law needs its own measurement
    # round.
    add_test(
        NAME fuzz-diff-r337-key-glue-quoted-swan
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/key-glue-quoted-swan-r337.bin
    )
    add_test(
        NAME fuzz-diff-r337-key-glue-quoted-swan-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/key-glue-quoted-swan-r337-artifact.bin
    )

    # r338 (run 44847828414, main 794aa99e): the BOM-content family's
    # INDENTED composition of the r306 BOM-ride and the r156 prop-split.
    # The 27-byte artifact `<BOM><BOM>'                 &U:#'` inserts a
    # 17-space root-indentation run between the content BOM and the
    # anchor name: yeptris keeps the whole run as plain content (the
    # leading BOM anchors the plain head -- catch-121; the spaces become
    # interior), reading `=VAL : <BOM>'                 &U:#'`; libyaml
    # consumes the run as root indentation (position-free whitespace at
    # the document root) and cuts the anchor at the 1.1 charset
    # boundary, reading props `&U` + the separator blank + `:#` as
    # glued plain content (the catch-85 law -- no blank follower, the
    # glued '#' opens no comment). THE CARRY: the r156 prop-split twin's
    # guard widens by the maximal space-run skip (the r156 no-space form
    # is the run-empty special case); the filing strips the BOM and the
    # run and inserts the separator blank at the 1.1-charset cut,
    # byte-exact on both dumps. The A/B: libfyaml 04e0b58 keeps the BOM
    # + run + glued tail as CONTENT (the double-quoted render is
    # yeptris's own reading) -- comparator carry only, no engine change
    # (the r318/r330/r336/r337 precedent). Dual-bar floor: 11 bytes
    # (`<BOM><BOM> &U:#`, single-space run -- the no-space 10-byte
    # `&U:` member rides the same arm and the bare `&U` form is already
    # carried by the r154 line-start removal probe); 50-mutant battery:
    # 26 pass pre-patch, 46 pass post (20 widened -- the space-run
    # lengths, the name charset members, the glued tails, the trailing
    # BOM/CR/CRLF/doc-end riders), 4 declined-both (the structural
    # block-entry tail, the doubled anchor line, and the two multi-line
    # continuations -- the single-line tight gate holds). Both the floor
    # and the artifact ride the corpus (the r321 seed law). Next-round
    # pointer: a space-run composition with a STRUCTURAL follower
    # (block-entry `- x`, a doubled anchor line, or a multi-line plain
    # continuation) stays DECLINED -- unmeasured shapes; a red there
    # means the multi-line/follower law needs its own measurement round.
    add_test(
        NAME fuzz-diff-r338-bom-anchored-key-indent
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-anchored-key-indent-r338.bin
    )
    add_test(
        NAME fuzz-diff-r338-bom-anchored-key-indent-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-anchored-key-indent-r338-artifact.bin
    )

    # r364 (run 45102166994, main 30fe527d): the r338 pointer's own
    # next round -- the BOM-led composition now rides a STRUCTURAL
    # follower. The 28-byte artifact
    # `<BOM><BOM>SPx7-SPx10&U:#` (12-byte dual-bar floor
    # `<BOM><BOM>- &U:#`) puts a block-sequence entry indicator after
    # the double line-start BOM: yeptris strips ONLY the stream-start
    # mark (the catch-121 law) and keeps the second as CONTENT, so the
    # `-` never opens an entry -- the whole line spells as ONE plain
    # scalar `=VAL : <BOM>- &U:#`; libyaml strips BOTH (its
    # column-0 scan_to_next_token, the r226 law), reaches the `-` as a
    # real entry indicator and opens `+SEQ` with the anchor riding,
    # `=VAL : &U :#`. THE CATCH: the r226 removal probe cannot reach
    # it -- the BOM-less twin `- &U:#` itself SPLITS (yeptris's
    # 2SXE/Y2GN-riding charset walk absorbs `U:` then refuses the
    # unspaced `#`, an error at column 6, while libyaml cuts the
    # anchor at the colon -- its ledgered Y2GN deviation -- and
    # accepts with the plain `:#`). THE CARRY: the BOM line-start arm
    # grows the composition probe -- libyaml's strip must be PURE
    # (its dump of the original equals its dump of the removal) and
    # yeptris's dump must equal the derived one-plain-scalar spelling
    # `+STR\n=VAL : <input minus the stream-start BOM>\n-STR` (the
    # value being the raw bytes verbatim); any other yeptris reading
    # moves a byte and declines. The A/B: libfyaml 04e0b58 reads the
    # same one-scalar shape on BOTH its 1.2 and 1.1 passes (the r318
    # dual-consensus face); the suite is silent (no BOM tests at the
    # da267a5c pin); spec 5.1 allows a BOM at the start of any
    # document and example 5.2 errors only mid-document -- comparator
    # carry only, no engine change (the r318/r330/r336/r337/r338
    # precedent). Floor re-verified: greedy ddmin to a fixed point,
    # all 12 single-byte deletions break the dual+shape bar. Both the
    # floor and the artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r364-bom-line-start-anchor-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-anchor-colon-r364.bin
    )
    add_test(
        NAME fuzz-diff-r364-bom-line-start-anchor-colon-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-anchor-colon-r364-artifact.bin
    )

    # r365 (run 45114990688, main bff9b53e): the r364 floor's own
    # map-follower twin -- the r338 pointer's predicted round, now
    # measured. The 23-byte artifact
    # `<BOM><BOM>- &U:#<BOM><BOM>- &U:` (11-byte dual-bar floor
    # `<BOM><BOM>- &U:`) closes the follower line on an END-OF-INPUT
    # COLON: yeptris strips only the stream-start mark (catch-121),
    # the second BOM opens the plain as CONTENT so the `&` loses its
    # indicator power and rides verbatim, and the end-of-input `:`
    # closes the opening line as ONE implicit ROOT-MAP key --
    # `+STR/+MAP/=VAL : <BOM>- &U/=VAL : /-MAP/-STR`; libyaml strips
    # BOTH (the r226 law), opens `+SEQ` whose entry is an implicit MAP
    # with the anchored empty key (the Y2GN colon-cut,
    # `+SEQ/+MAP/=VAL : &U /=VAL : /-MAP/-SEQ`). BOTH carried probes
    # decline: the r226 both-accept bar misses because the removal
    # twin (`- &U:`) itself SPLITS -- yeptris reads a seq entry whose
    # absorbed anchor rides the colon as plain content (`=VAL : &U: `,
    # the 2SXE/Y2GN riding law) while libyaml opens the anchored empty
    # key -- and the r364 composition gate (yok2 == 0) misses the
    # floor face because the removal twin PARSES on yeptris there (it
    # only errors on the artifact's longer tail). THE CARRY: the BOM
    # line-start arm grows the map-follower probe -- libyaml's strip
    # must be PURE (its removal dump equals its original) and
    # yeptris's dump must equal the derived root-map spelling
    # `+STR\n+MAP\n=VAL : <input minus the stream-start BOM minus the
    # final colon>\n=VAL : \n-MAP\n-STR` (key bytes verbatim, empty
    # value); any other reading moves a byte and declines. The A/B:
    # libfyaml 04e0b58 reads the same root-map shape on BOTH its 1.2
    # and 1.1 passes (the quoted key `<FEFF>- &U` with the empty
    # value -- the r318 dual-consensus face); the suite is silent (no
    # BOM tests at the da267a5c pin); spec 5.1 allows the BOM at the
    # document start and libyaml's line-start strip is its own
    # ledgered r226 deviation -- comparator carry only, no engine
    # change (the r318/r330/r336/r337/r338/r364 precedent). Floor
    # re-verified: greedy ddmin to a fixed point, 11 bytes, all
    # single-byte deletions break the dual+shape bar. Both the floor
    # and the artifact ride the corpus (the r321 seed law).
    add_test(
        NAME fuzz-diff-r365-bom-line-start-map-follower
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-map-follower-r365.bin
    )
    add_test(
        NAME fuzz-diff-r365-bom-line-start-map-follower-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-map-follower-r365-artifact.bin
    )

    # r380 (the post-r379 Fuzz Nightly round, run 45451584443, main
    # 850d4d91, plus issue #1638's r379-drill soak finds). TWO carries:
    #
    # (1) the BOM-stripped VALUE twin (issue #1638, the r379 drill's
    #     own bounded comparator soak): the 8-byte unit `[?:\r<BOM>]`
    #     (crash-7c39da3fc484a16216f9c87fb9ef8d1be74e7fe1 -- libFuzzer's
    #     minimizer walked the soak's raw `?##|` soup down to it in one
    #     bounded pass; the unit is its own floor: all 8 single-byte
    #     deletions either classify or leave the YD_DIFFER class for a
    #     YEPTRIS-ONLY-ACCEPT split). The DIFFER composes TWO carried
    #     laws no single arm modeled: libyaml's flow KEY-token law
    #     consumes the glued `?` (the key side pairs via yd_qflow_
    #     divergence's empty-key twin, the r58/r163 law -- its KEY
    #     scans EMPTY because the line-start mark strip ate the
    #     post-CR BOM that opened the line) while yeptris's suite-pinned
    #     plain-'?' law (HM87/652Z) keeps `?` ONE plain KEY with the
    #     `:` as the pair separator, and a MID-STREAM BOM rides the
    #     value as CONTENT byte-exact (the catch-121/r92 family;
    #     libyaml's scan_to_next_token strips the line-start mark at
    #     column 0 whatever break opened the line -- the r226 law).
    #     The suite is silent (no BOM-after-break rows at the da267a5c
    #     pin -- the r364/r365 composition-arm precedent). The A/B:
    #     libfyaml 04e0b58 reads the unit byte-semantically YEPTRIS's
    #     way in BOTH strict-1.2 and 1.1 (key `?`, the BOM riding the
    #     value -- probed byte-exact with the event API) -- the engine
    #     defect is refuted, comparator carry only, no engine change
    #     (the r317/r343/r366/r375/r378/r379 precedent). THE CARRY:
    #     yd_qflow_divergence grows the BOM-STRIPPED VALUE twin -- the
    #     state law: yeptris's value line is libyaml's line with the
    #     THREE mark bytes inserted at the content head, the tail
    #     byte-equal (a BOM-led yeptris line against a non-empty
    #     libyaml value claims only the byte-equal tail; a yeptris line
    #     without the mark, or a libyaml line carrying content the
    #     mark's line never had, declines -- a dropped yeptris value
    #     cannot claim). 50-mutant paired battery (seed 380, 30 single
    #     + 20 double, against the r379-carry build f3cf69ec): 0
    #     regressions (the b8 unit's 50: 17 stable-clean, 33 stable in
    #     the split zone -- the unit's neighborhood is mostly
    #     yeptris-only-accept splits).
    add_test(
        NAME fuzz-diff-r380-flow-qglue-bom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qglue-bom-r380.bin
    )
    #
    # (2) the ATE-RUN form of the qgh adjacency (the nightly's round
    #     380, run 45451584443, main 850d4d91, crash-bc2987bf3a60b375574
    #     e24a351494c1263993365, the 68-byte artifact; the 14-byte
    #     dual-bar floor `{?#,*|,- |LF,<}`, greedy libFuzzer ddmin, all
    #     14 single-byte deletions either classify or leave the face --
    #     the five that still DIFFER leave the CLASS: YEPTRIS-ONLY/
    #     LIBYAML-ONLY splits or an EOF-swallow skeleton). The floor's
    #     eaten line is `#,*|,- |`: libyaml's post-'?'-token
    #     comment-glue law (the r379 adjacency) runs the glued '#' to
    #     the NEXT BREAK -- ONE line, line-local by construction (the
    #     r337 law) -- so EVERY member frame whose bytes ride that line
    #     dies WHOLE in libyaml's reading (yeptris's suite-pinned
    #     plain-'?' law keeps `?#` ONE plain key, the bare-`*|` alias
    #     member and the dash-plain `- |` member both ride; libyaml's
    #     pending KEY re-pairs with the post-break continuation's `<`).
    #     The r379 ate-alias arm modeled the run length of ONE; the
    #     r380 floor's run carries TWO frames (the alias AND the
    #     dash-plain). The A/B: libfyaml 04e0b58 REJECTS the floor in
    #     both modes ("plain scalar cannot start with '|'" at `- |` --
    #     the strict-1.2 plain-charset law) while libyaml 90a56d4's
    #     1.1 posture accepts -- the comparator split rides the
    #     engines' own tables (the r335/r317/r369 precedent), comparator
    #     carry only, no engine change. THE CARRY: the qgh walk's
    #     ate-alias adjacency grows the ATE-RUN form -- the run walk
    #     consumes frame shapes ONLY (a content VAL, an ALI with a
    #     non-empty name, or a double-empty, EACH with the exact empty
    #     "=VAL : " partner), bounded by the r180 neighbor budget (16:
    #     the comment cannot cross a break), ending at the SAME
    #     byte-exact re-alignment pin the r379 one-frame form pinned
    #     (yeptris's next line IS libyaml's current line; the alias
    #     never emerges in libyaml's dump -- the r138-r141
    #     no-libyaml-alias shape whose masking ruling the branch still
    #     honors); ONE shared-cap count per frame (the r146 unit).
    #     50-mutant paired battery (seed 380, against the r379-carry
    #     build f3cf69ec): 0 regressions (the f14/a68 units' 100: 21
    #     DIFFER->CLEAN shifts are the widened arms claiming their own
    #     family's mutants -- 4 on the floor, 17 on the artifact; 25
    #     +29 mutants ride stable in the split zone).
    add_test(
        NAME fuzz-diff-r380-flow-qate-run
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qate-run-r380.bin
    )
    add_test(
        NAME fuzz-diff-r380-flow-qate-run-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qate-run-r380-artifact.bin
    )

    # r381 (the post-r380 Fuzz Nightly round): the anchor-led deep-flow
    # soup, 4106 bytes "&al:o&a:&2?*: '"":[[...". yeptris rides the 1.2
    # ns-anchor-char name `al:o&a:&2?*:` (the 2SXE/W5VH colon-ride law;
    # the interior '&' per the r356 row, the glued '?'/'*' per the r330
    # row) into ONE root single-quoted scalar running to the artifact's
    # LAST byte (the col-0 fold lines are the NP9H root law -- spec
    # example 7.5; the suite's QB6E pins the in-map twin as an ERROR,
    # fail: true). libyaml 90a56d4 cuts at the ':' after `al` (in
    # scan_anchor's follow set -- the anchor is ACCEPTED, unlike the
    # r370 %/@ leads that error) and the ':'-led residual `:o&a:&2?*`
    # re-lexes as the key's plain content (1.1 colon-plain leniency,
    # the r174-documented re-lex) under a root map. A/B (probed, event
    # level): libfyaml 04e0b58 reads yeptris's stream byte-exact in
    # BOTH modes on the artifact, and rejects `k: 'a\nb\nc'`
    # identically ("wrongly indented single-quoted scalar", both
    # modes) -- comparator carry only, no engine change (the
    # r318/r330/r336/r356/r366/r368/r370/r376 precedent). The carry:
    # the r238 arm gains the ':'-LED MIXED residual branch, and the
    # r368 clamp detection reads its own prefix (4100/4099 - the
    # stream prefix) instead of the r366-only 4082/4081 constants --
    # the giant value cuts both dumps at the 4095-char line budget
    # (yeptris fused with `-STR` at region 4074, libyaml fused with
    # `-MAP` at 4088), which strands the r76 keymap-splice arm's
    # value-equality law (measured: the splice claims every SHORT
    # member of this shape -- `'a\nb'`, `'a\nb\nc'`, `'[[{"": x\ny'`
    # all ride yd_anchor_keymap_splice pre-patch -- and declines only
    # the clamped giant). The charset arm's split copy declines its
    # own family again: `&al :o&a:&2?*: '...'` opens the ROOT MAP on
    # yeptris, whose quoted value carries the col-0 continuation --
    # the QB6E law yeptris enforces -- so the copy is yeptris-rejected
    # ("inconsistent indentation", measured) while libyaml accepts it.
    # Dual-bar floor: the 4090-byte greedy single-byte ddmin fixed
    # point (the 16-byte chain chunk deleted at offset 16; the value
    # rides exactly at the fused-clamp boundary -- every further
    # deletion drops below the band and lands classified pre-patch,
    # exit 0). 50-mutant paired battery (seed 381, against the
    # pre-patch build): 0 regressions. The alias face is DEAD -- the
    # `*a:` byte run sits INSIDE the quoted body on BOTH engines
    # (no =ALI event either side); the deep chain is quoted content,
    # carried (r361/r368/r380).
    add_test(
        NAME fuzz-diff-r381-root-anchor-colon-ride
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-colon-ride-clamp-floor-r381.bin
    )
    add_test(
        NAME fuzz-diff-r381-root-anchor-colon-ride-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-colon-ride-qsplat-clamp-r381.bin
    )

    # r382 (the r381 drill's bounded soak find, issue #1642): the
    # flow-'?'+TAB soup, 202 bytes, `{?##|#|+` #|#|+  \t \n\t ,?##|#|,`
    # ,?\t*L:\t ,?#` ...` -- a flow map whose members are the
    # r244/r265-style `?#`/hash-cr plain runs TAB-glued (the blank
    # runs before `*L:`, between members, inside the blank lines), a
    # thrice-repeated member block (the mutation walk's CopyPart), and
    # closing `}`. yeptris keeps the glued-`?#` members as PLAIN keys
    # (the JR7V law -- the suite pins `?`+content as plain; the
    # interior `#` is glued, never a comment) with the TAB-riding
    # alias `=ALI *L` member; libyaml's flow dispatch fires the `?` as
    # the KEY token unconditionally and its scan_to_next_token then
    # comment-eats from the glued `#` -- its dump is the all-empty
    # scalar collapse (12 members vs yeptris's 17). The artifact's
    # key-slot face (member `?\t#`\n:\t),`): the TAB after the `?`
    # splits the explicit key, the blank-preceded `#` at the key slot
    # fires the r277 comment-steal on both engines, and the stolen key
    # region opens `:` + TAB -- the COLON-LED VALUE boundary at the
    # stolen-key slot (the r382 classifier widening, law (f)).
    # Adjudication: the steal probes are all-EQUAL (`{?\t#`\n:\t),}` ->
    # (e,))`, `{?\t#\n:,}` -> (e,e)`, `{?\t#\n:\t),}` -> (e,))`,
    # `{?\t#\n:}` -> (e,e)` -- the r286 law's slot mechanics one slot
    # over); the artifact's steal member renders `(e,))` inside the
    # DIFFER. Floor: the 14-byte greedy ddmin fixed point
    # `{?,?\t#\n:,?:#\n}` (the artifact's repeated block collapses;
    # every one of the 14 single-byte deletions breaks the dual+shape
    # bar -- no DIFFER, or an unclaimed DIFFER). The floor's own split
    # face is the GLUED `?:#` member: yeptris reads ONE plain `?:#`
    # (the 1.2 ns-plain-first followed-by arm; the suite's JR7V pins
    # the `?`+content law, 2EBW pins the indicator-head key law) while
    # libyaml's flow fetch dispatch splits the glued `?:` into KEY +
    # VALUE tokens and comment-eats the tail (its scanner.c: flow `?`
    # and `:` are unconditional indicators -- the documented 1.1
    # leniency). A/B (probed, event level): libfyaml 04e0b58 reads
    # yeptris's floor byte-exact in BOTH --yaml-1.2 and --yaml-1.1
    # (`=VAL :?:#` plain kept) -- comparator carry only, no engine
    # change (the r318/r330/r336/r356/r366/r368/r370/r376/r381
    # precedent). The artifact additionally rides the LENIENT
    # backtick-led plain composition (members `` ` ``-led after the
    # `,`): the suite is silent (2EBW/FBC9 pin the backtick only
    # MID-plain), libfyaml 04e0b58 strict-1.2 ERRORS the shape ("plain
    # scalar cannot start with '`'" -- the reserved-indicator law) and
    # both engines accept it leniently -- the lenient composition rides
    # the artifact pin, and the classifier's dual model reproduces both
    # actual dumps byte-exact on floor and artifact (the classify
    # firewall: 108 dump-tamper cases declined on the pair). The carry:
    # the r277 steal arm gains the colon-led VALUE-boundary branch (the
    # r286 law's follower set: blank/break/FLOW INDICATOR) at the
    # stolen-key slot; the glued colon-plain twin keeps the r277 plain
    # walk and the slot's-EOF colon stays declined. 50-mutant paired
    # battery (seed 382, against the pre-patch build): 31 pass-pass,
    # 18 claims (the widening claims modeled family members), 1
    # red-red (an unclaimed mutation outside the family), 0
    # regressions. The comparator-snapshot exclusion chain gains
    # `root-anchor-colon-ride-*` this round too (issue #1643 -- the
    # r381 pins' own treadmill step).
    add_test(
        NAME fuzz-diff-r382-flow-qsoup-tabsteal-colon
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tabsteal-colon-r382.bin
    )
    add_test(
        NAME fuzz-diff-r382-flow-qsoup-tabsteal-colon-artifact
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-tabsteal-colon-r382-artifact.bin
    )

    # the r383 drill (issues #1645 and #1646). TWO comparator-carry
    # widenings, no engine change (the r226/r375/r368 precedent).
    #
    # Issue #1645 (the r382 soak's proactive find, artifact
    # crash-e8479c97d512d617f13c7f1d75e7aca72c316df4, 2736 bytes):
    # the pinned r226 NEL member grown a giant body -- the whole input
    # is ONE UTF-16BE root plain ("mark<NEL><BOM><NEL><BOM><NEL>" +
    # 1362 fused-BMP units; a byte-level ddmin cannot shrink it, the
    # r382 soak's own pass floored 2736 -> 2730). The law face is the
    # r226/r375 carry unchanged (yeptris keeps the interior line-start
    # marks as CONTENT with the folds spelling "<BOM> <BOM> <body>",
    # libyaml strips them; libfyaml 04e0b58 reads yeptris's shape on
    # the UTF-8 twin in BOTH modes -- the r318 dual-consensus) -- but
    # the yeptris-side dump crosses the shared vocabulary's per-event
    # line budget (yd_line's char line[4096]): the value renders 4094
    # bytes, the escape-append clamps at the 4095th char, swallows the
    # trailing newline, and the -STR event's line GLUES onto the cut,
    # so the r226 law's strip comparison saw 4080-of-4086 body bytes
    # and declined its own family. The engine carries the WHOLE value
    # (probed: the scalar event's value_len is 4094, the tail bytes
    # the body's final fused units). THE CARRY: the r226 UTF-16 law
    # gains the cap-cut arm -- the cut geometry pinned exactly (one
    # 4095-char scalar line, glued "-STR", 4105-byte dump) against the
    # derived content spelling, with every content unit past the cut
    # required to be a plain byte-carried unit (cp >= 0xA0, not a
    # break/mark/surrogate -- the r100 skeleton law); a tampered dump
    # loses the exact-length gate, a mutated break/mark unit past the
    # cut loses the unit law. Floor: the 2734-byte 1361-filler
    # synthetic (value 4091, visible 4088, one past-cut unit) -- the
    # smallest law-head member whose yeptris value crosses the budget.
    #
    # Issue #1646 (the nightly's run 45498839224, artifact
    # crash-eaec426c22dd5205b71cdaa966f5d88b7db7ef59, 13594 bytes):
    # the `&2?*:` PS-led composition's 4-tail sibling -- byte-identical
    # to the pinned r368 clamp artifact outside a +2704-byte chain
    # growth, PRE_RC=77 on the pre-patch build (pre-existing), and the
    # seed exclusion CANNOT gate it (the family has no seeds to
    # exclude -- the r1306 law; the walker re-derives the shape from
    # general corpus material every round). The face that escaped: the
    # mutation walk's CopyPart junk past the clamp cut -- the sibling
    # embeds a dump fragment's mark triple "\xef\xbb\xbf" (a
    # "=VAL : <BOM>" paste) ~9.3KB past the y-side cut, where the
    # r368 clamp-tail law's "multibyte lead: unmeasured" decline
    # rejected its own family (the fragment VISIBLE classifies: the
    # pre-cut walkers already ride it). THE CARRY: the clamp-tail law
    # gains the multibyte-content branch -- a well-formed UTF-8
    # sequence rides iff its code point is none of U+0085/U+2028/
    # U+2029 (the r231 break set, engine-divergent: yeptris keeps
    # them raw, libyaml folds); malformed/truncated/overlong/
    # surrogate encodings decline. MEASURED at the r366 floor scale:
    # the mark triple MID-BODY and at a post-break LINE START render
    # verbatim on BOTH engines (libyaml's line-start strip is a
    # scan_to_next_token law and does not reach inside a quoted body),
    # the dump delta exactly the 3 bytes on both sides. libfyaml
    # 04e0b58 reads yeptris's shape on the artifact and the floor in
    # BOTH modes (one root anchored scalar, anchor `2?*:`, the chain +
    # fragment + `U:ba:` tail inside the quoted body) -- the r318
    # dual-consensus. THE STRUCTURAL FINDING (for the main session):
    # the composition family is mutation-dense along its whole
    # past-cut vocabulary (three reds: the r368 artifact, #1643's
    # member, this sibling) -- the exclusion chain cannot stop it and
    # the clamp-tail safe set is an open-ended alphabet; whether the
    # family graduates to a dedicated gate stays the main session's
    # call. The comparator-snapshot exclusion list already covers the
    # family (`root-anchor-quoted-follow-*`) and the utf16 seed family
    # (`utf16-*`, which these pins' names join).
    add_test(
        NAME fuzz-diff-r383-bom-utf16-rootplain-capcut
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-bom-nel-rootplain-capcut-r383.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/utf16-bom-nel-rootplain-capcut-r383-artifact.bin
    )
    add_test(
        NAME fuzz-diff-r383-raf-bomfrag-capcut
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-bomfrag-capcut-r383.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-bomfrag-capcut-r383-artifact.bin
    )

    # the r384 drill (issues #1648 and #1649). TWO comparator-carry
    # widenings, no engine change (the r226/r331/r366/r383 precedent).
    #
    # Issue #1648 (the r383 soak's proactive find, artifact
    # crash-39d29a0c498a951972d9306a8af64b8aa2c4ebc7, 15 bytes,
    # `+M!P...: '<PS><TAB>'`; the greedy-ddmin floor 10 bytes,
    # `M!: '<PS><TAB>'`): inside a QUOTED scalar body a raw LS/PS
    # triple followed by s-white splits the engines on the
    # 1.1-vs-1.2 break reading. Spec 1.2 (5.4) makes LS/PS NON-BREAK
    # characters -- b-char is LF/CR only, and a 1.2 processor treats
    # the non-ASCII breaks as non-breaks even for a 1.1 document --
    # so yeptris keeps the triple RAW (the r231 class) and the
    # following blanks as MID-BODY content cells on one logical line,
    # the closer closing at its own byte (`=VAL : <PS>\t`); libyaml's
    # 1.1 READ_LINE folds at the triple (kept VERBATIM for LS/PS,
    # scanner.c's LS|PS->LS|PS) and STRIPS the continuation line's
    # leading whitespace (the fold's line-prefix law), so its value
    # is yeptris's minus exactly the blank run (`=VAL : <PS>`).
    # libfyaml 04e0b58 strict-1.2 reads yeptris's shape byte-exact on
    # every probe (`x<PS>\ty`, `x<PS> y`, the double-quoted twin, the
    # multi-site body, the floor's trailing-blank body); its 1.1 pass
    # REJECTS the TAB members outright ("invalid tab used as
    # indentation") -- the r318 dual-consensus, comparator carry
    # only. THE CARRY: the r384 quoted-PS-blank arm -- a raw LS/PS
    # triple followed by s-white inside an r145-tracked quoted span,
    # claimed by the spell transform in the adjudicated direction
    # (delete the s-white run after every in-quote triple, re-dump
    # BOTH engines, require ldump(original) == ydump(rewrite) ==
    # ldump(rewrite) byte-exact -- libyaml's own reading must not
    # move, yeptris's must lose exactly the run). The r366 CR member
    # stays out by construction: CR is a REAL 1.2 break (b-char),
    # both engines fold it, and the r238 quoted-follow arm owns that
    # face; the PS+CR/PS+LF col-0-continuation twins are
    # YEPTRIS-REJECTED (the indent law) and never reach the arm.
    #
    # Issue #1649 (the nightly's run 45511622229, artifact
    # crash-954b8ddd220b324cbd239b36719eb3ca30bdac2b, 1824 bytes; the
    # whole-member ddmin floor 82 bytes,
    # `{c,?#,\r0,?#\nm,y,...,g,l}`): the qflow-qhash-cr composition's
    # glue members grown the FOLD-GLUE pairing -- at `,\r?#<LF>m,`
    # yeptris reads ONE plain (`?#` + the 1.2 break fold joining the
    # post-break `m`, the plain-'?' law keeping the member flat) while
    # libyaml reads the glued-`#` comment-eat law's KEY token with the
    # post-break content ALONE as the key (the '#'s eat with it --
    # `?##\r\n&m` keys the post-break `&m` ANCHOR, its prop render
    # trailing one space, the r315 row). The dumps pair one-to-one,
    # but the r180 MARKER-FRAME PRIORITY branch PREEMPTED the r117
    # SPLIT arm on glue-marked content, consumed only yeptris's two
    # lines and KEPT libyaml's line -- whose bytes already RODE
    # yeptris's line -- so the streams ran one member off and the tail
    # members burned 2 shared-cap skips each (the floor's frame pass
    # flooded 75 > 64 + 82/8 at the `&m` tail; frame 0 flooded 65 >
    # 64 off-sync; the artifact flooded 293 > 64 + 228/8 at its ~30
    # `?## &m` anchors; the ddmin proved the escape needs the whole
    # composition -- every prefix, suffix and single-member cut
    # classifies, the skip arithmetic binds only at the full length).
    # THE CARRY: the r384 FOLD-GLUE tell -- a paired route (the r117
    # form) gated BEFORE the marker-frame priority, consuming BOTH
    # pairs as one diff, no skip, both pointers advance. Two
    # byte-tight tails: the PLAIN tail (yeptris's content is EXACTLY
    # '?' + '#'+ + one fold space + libyaml's content -- the floor's
    # `?# m` / `?# /` members) and the PROP tail (libyaml's content is
    # the anchor/alias prop render '&'/'*'+name+' ', the r315 row, and
    # yeptris's is the glued run + the fold space + the same prop name
    # bare -- the artifact's `?## &m` members). A multi-break fold
    # spelling (the n-1 LF-escape form) is unmeasured and declines;
    # the memcmp is the firewall. libfyaml 04e0b58 strict-1.2 reads
    # the artifact's head byte-exact libyaml's way (the empty-key
    # separate-value reading, the r106 family) and folds the PS
    # identically -- the glue members are the documented #949 family,
    # comparator carry only (the r244/r382 precedent; the family stays
    # mutation-dense -- this composition re-derived from general
    # corpus material in the nightly, the r383 structural finding).
    # The PS mid-body faces ride the r150 lsps branches unchanged (the
    # artifact's `/\n\n\n51` vs `/\n\n<PS>51` pairs claim through
    # them); the head's `:cic +SEQ##|` empty-key split rides the r116/
    # r117 skip routes.
    add_test(
        NAME fuzz-diff-r384-quoted-ps-blank
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-ps-blank-r384.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/quoted-ps-blank-r384-artifact.bin
    )
    add_test(
        NAME fuzz-diff-r384-qflow-gluefold-ps
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-gluefold-ps-r384.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/qflow-gluefold-ps-r384-artifact.bin
    )

    # the r385 drill (issue #1652, the r384 soak's proactive find).
    # Comparator carry, no engine change (the r318/r330/r336/r356/
    # r366/r370/r376/r381 precedent). Artifact
    # crash-59f81b58e85ad9e632820873255a685c7bc58bcc, 13 bytes,
    # `&2?/:: '<PS><CR>'` -- the pinned r330 seed grown the tail
    # colon DOUBLED over the r370 charset row's `/`. The three-way
    # split (byte-exact, /tmp/r385-art/split-59f81b58.txt): yeptris
    # ONE root anchored plain (the full name `2?/::` rides -- the
    # 2SXE/W5VH/Y2GN anchor-name law, probed: the alias `*2?/::`
    # resolves; value raw PS + the CR folded to the one space), libyaml
    # the anchored-key MAP (cut at the '?', the residual `?/:` the
    # key's plain content -- every interior ':' glues to the next
    # non-blank byte, the boundary fires at the riding ':' behind the
    # separator blank; the quoted value PS verbatim + the CR as the
    # `\n` escape), libfyaml 04e0b58 strict-1.2 yeptris's stream
    # BYTE-EXACT (the full anchor riding, strip-labels drops it; value
    # `<PS>+<SP>`; its 1.1 pass value `\n`, matching libyaml's value
    # spelling) -- the r318 dual-consensus, the issue's "whole line
    # absorbed as quoted" third reading REFUTED at DOM level (it was
    # the re-EMIT style of the multi-break value). THE CARRY: the
    # r238 arm's `?`-led residual walk admits the INTERIOR ':' -- the
    # mixed `?x:` face the r370 round left unmeasured (the r385
    # widening note in yd_r238_root_anchor_quoted_follow_gray). THE
    # ROW (measured pre-patch DIFFER, the identical MAP structure,
    # all claimed post-patch): `&2?::` (the floor, the in-family
    # del[3] twin), `&2?:::`, `&2?:/:`, the artifact `&2?/::`, and
    # the r366 body over the new residual `&2?:: '<PS><CR>\'`; the
    # PS-only short member `&2?:: '<PS>'` stays the r76 splice's
    # claim. Floor: the 12-byte `&2?:: '<PS><CR>'` -- the FAMILY-
    # SCOPED greedy ddmin fixed point (the `?` deletion dies into the
    # pinned r336 seed, the colon deletions into the classified
    # `&2?:` solo -- both exit 0 pre-patch). THE r385 STRUCTURAL
    # FINDING (feeding the owner gate the r383 round left pending):
    # the artifact's one-byte neighborhood is DENSE across THREE
    # other verdict classes -- 9 of 13 single-byte deletions abort
    # unclaimed (the no-'&' plain-head faces `2?/: '/...`: yeptris
    # REJECTS the map-value col-0 continuation while libyaml accepts,
    # LIBYAML-ONLY-ACCEPT; the '&'-lead `&?...` and malformed-UTF-8
    # faces, YEPTRIS-ONLY-ACCEPT; the no-separator face) -- the RAW
    # greedy ddmin escapes through del[0] and floors at the 6-byte
    # plain-head `:: '<CR>'`; the neighbors are NOT this family and
    # stay unclaimed. 50-mutant paired battery (seed 385, 20 flips +
    # 10 deletions + 20 doubles per shape, against the pre-patch
    # build): 37 pass-pass, 18 claims, 45 red-red (the density
    # finding quantified; both builds agree red), 0 regressions. The
    # pin names join the snapshot's `root-anchor-quoted-follow-*`
    # exclusion (r370) -- no workflow change.
    add_test(
        NAME fuzz-diff-r385-root-anchor-quoted-follow-colonres
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-colonres-r385.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-colonres-r385-artifact.bin
    )

    # the r386 drill (issue #1656 + the r385 soak's evolved unit; the
    # fourth re-derivation the family needed its gate for). THE
    # LEDGERED-WAIVER (law w in family_classify.h, the PR #1647 owner
    # call with four data points): the `?#`-class glue-fold composition
    # family declines as a DOCUMENTED SPLIT instead of another
    # per-spelling arm — the four red runs (45317828972/r377,
    # 45498839224/#1646, 45511622229/#1649, 45540139415/#1656) each
    # escaped the previous round's arm as a fresh mutation spelling of
    # the SAME composition (the glued-`?#` hash-run heads, the split-`?`
    # fold-glue PS keys, the `?##\r\n&m` prop tails, the `+SEQ` splices
    # and `tag:ytag:yam2l.org,200#=` tag-soup mutations). THE GATE
    # (yd_family_waiver): the measured member grammar + the BYTE-EXACT
    # yeptris-side derivation + the head-split fingerprint (yeptris's
    # flat `?#` lines absent from libyaml's stream — 558/685 vs 0 on
    # the two artifacts) + the multiset-exact content accounting (the
    # libyaml side maps onto the yeptris side through exactly four
    # transforms: the verbatim ride, the `?`-lead drop, the `?#`-head
    # line-break eat, the `>`-lead drop — anything else red). THE A/B
    # (libfyaml 04e0b58, probed per member class this round): the HEAD
    # faces read yeptris's way byte-exact in BOTH passes; the fold
    # faces are the standing law-b split (fy keeps PS raw — the third
    # reading); the `>` members ERROR fy (the reserved-indicator law).
    # No member class exists where libfyaml sides WITH libyaml against
    # yeptris. THE FIREWALL (the fuzz-diff-family-classify self-test's
    # W/N phases): the waiver corpus's dump-tamper battery (108 cases)
    # must decline every single-byte change, the law-violating input
    # mutants must never waive outside the grammar, and the negative
    # controls (other-family DIFFERs + unmeasured-shape splices: the
    # nested-seq/5-hash/quote/backtick/BOM/alias/rootplain faces) must
    # NEVER waive — 0 waived. The waiver is a DECLINE, not a claim: the
    # comparator logs `ledgered-waiver` and moves on; ctest never
    # consults it (these pins assert the engine behavior directly).
    # The artifacts: crash-922561ba0ca8e65e174fc4823c3c7065678f0b1e
    # (24,870B, the nightly's run 45540139415 — the `+SEQ` splices and
    # the tag-soup mutations) and crash-67a59a94557ef71df2babe2e7568b5
    # 411c74c105 (20,486B, the r385 drill's own bounded soak — the
    # `?##|` triple-hash pipe spelling, the PS joined mid-key). Both
    # PRE_RC=77 pre-existing, comparator-only, no engine change (the
    # r318/r330/r336/r356/r366/r370/r376/r381/r385 precedent).
    add_test(
        NAME fuzz-diff-r386-gluefold-waiver
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r386-plusseq-splice.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r386-evolved-soak.bin
    )

    # the r387 drill (issue #1660, the post-r386 nightly's run
    # 45565730060). Comparator carry on the r238 arm — no engine change
    # (the r226/r375/r368/r376/r383 precedent). Artifact
    # crash-4c8ce34fbe3d8fc3b51abdc60dc356c2cb941754, 10,955 bytes:
    # the r330-family head `&2?*: '` over the PS/CR-led quoted body
    # grown the `[[{"":[{"" ` chain (three chain lines, ~552 reps
    # each) plus the mutation walk's CopyPart paste junk
    # (`\u0060\u012a\u12`-class dump-fragment runs with `A`/` b, `/`B]`
    # /`c ]`/`al-` fragments) ~1.5KB past the clamp cut. The three-way
    # split: yeptris ONE root anchored plain (the anchor `2?*:` riding
    # whole -- the 2SXE/W5VH/Y2GN law; the PS kept raw, the CR folded
    # to the one space), libyaml the anchored-key MAP (cut at the '?',
    # the residual `?*` the key's plain content, the quoted body PS
    # raw + the CR as the `\n` escape), libfyaml 04e0b58 --testsuite
    # strict-1.2 reads yeptris's stream BYTE-EXACT (`+DOC` +
    # `=VAL &2?*: '\P "\\u1:...` -- the whole body one scalar, the
    # pastes riding as content); its --yaml-1.1 pass keeps the scalar
    # structure with libyaml's value spelling (the r330 1.1 precedent)
    # -- the r318 dual-consensus, engine divergence REFUTED. THE
    # ESCAPE was NOT the chain (the chain bytes ride the printable
    # units -- the r386 waiver's documented scope left the anchor-ride
    # lead on the r238 arm and the arm covered everything else): the
    # pastes carry LITERAL BACKSLASHES past the clamp cut, where
    # yd_r238_clamp_tail_safe declined the byte (measured at s=5544,
    # 0x5c, on the artifact). THE r387 WIDENING: the clamp-tail unit
    # row admits the literal backslash as a FORCED-EQUAL content unit
    # -- one dump `\\` pair per body byte on BOTH engines through the
    # shared yd_escape_append table, state-clearing, position-
    # independent (the r366 pre-cut law; the suite pins the literal
    # face: 4V8U plain, 6H3V single-quoted; a single-quoted scalar has
    # no escapes, 1.2 8.1.2) -- the same extension principle as the
    # r383 multibyte law and the r376 content-tab cells. THE FLOOR:
    # greedy ddmin under the dual+shape bar to its 4,113-byte plateau
    # (the artifact's head + PS/CR fold faces + the `~foo:` fold lines
    # + two chain runs + the `&mapping`/`&key` leads + the paste
    # backslash cluster straddling the cut at 4076 (pre-cut, the r366
    # pair) / 4082 / 4088 (past the y cut; 4088 past both)) -- the
    # giant-class pin rides the r383 precedent (the r368 artifact's
    # own round pinned the artifact + the structure-derived floor, not
    # a full single-byte matrix: at this size the matrix is the
    # engine's, not the gate's, work). Every deletion class probes
    # CLAIMED post-patch (the `\u1` backslash at 12, the paste
    # backslashes, the chain interiors); the negative controls ride
    # their pre-patch verdicts unchanged (LS/PS past the cut stays
    # the r231 engine-divergent decline -- probed red on both builds;
    # the 0x7f/control/malformed-multibyte/trailing-blank shapes ride
    # the other arms' claims identically pre- and post-patch).
    add_test(
        NAME fuzz-diff-r387-root-anchor-quoted-follow-pastebs-clampcut
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-pastebs-clampcut-r387.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-pastebs-clampcut-r387-artifact.bin
    )

    # the r388 drill (issue #1665, the post-r387 nightly's run
    # 45610560196). Comparator carry on the r386 waiver — no engine
    # change (the r318/r330/r336/r356/r366/r370/r376/r381/r385/r386
    # precedent). Artifact crash-587608b0003905ab3686acf77ca2e1cb8afbd2fa,
    # 15,783 bytes: the `?#`-class GLUE-FOLD composition family's 5th
    # re-derivation and the FIRST post-waiver one — the waiver HELD its
    # contract (~30 in-grammar members claimed as `ledgered-waiver`, the
    # fresh member correctly declined). THE ESCAPE was NOT a member-shape
    # gap: the fresh spellings decompose to (a) EQUAL faces the measured
    # grammar already derives byte-exact (the `211` split-pair members
    # x81, the `1.0e57.tag:y---|` dash-run plains x25, the `1&m`
    # content-value slot — both engines render them identically) and (b)
    # ONE underivable face: the folded prop tail's anchor NAME grew from
    # the measured single-byte `m` to the multi-byte IS_ALPHA `mq` (the
    # artifact's lone `?##\r\n&mq,` member — yeptris renders the folded
    # plain `?## &mq`, libyaml's post-break rebind renders the
    # anchored-empty `&mq `). THE r388 EXTENSION: the content-set
    # ADMISSION check gains the prop-render twin its own ALLOWANCE rule
    # (b) always carried — the eaten head's rebind name rides libyaml's
    # prop render (trailing space) even when that name appears NOWHERE
    # else in the yeptris stream. The firewall is unchanged: the byte-
    # exact yeptris-side derivation gates the input first, the twin maps
    # INTO an existing yeptris-side form, every other transform stays
    # enumerated, the negative controls keep declining. THE A/B:
    # libfyaml 04e0b58 reads the isolated `?##\r\n&mq` member byte-exact
    # yeptris's way (`?## &mq`) in BOTH strict-1.2 and --yaml-1.1 passes
    # and reads the artifact's head prefix (the flat `?##|` plains, the
    # `{211: }` split pairs, the folded `?## &m` tails) yeptris's way
    # until erroring at the artifact's `>` member mid-stream
    # (the reserved-indicator law, the r386 A/B unchanged). THE FLOOR:
    # greedy ddmin under the dual claim bar (PRE red + POST waiver-claims)
    # to its 54-byte plateau `[1,?#\r2002:binary,1,?# ,?#,
    # ?\r2002:binary,?.,?\r!i,?#\n&e]` — the fresh-face member's anchor
    # name rode down to the single IS_ALPHA byte `e` (no bare `&e` member
    # anywhere in the input — the admission twin is what carries it), the
    # measured faces riding it. PRE_RC=134 comparator-only, no engine
    # change.
    add_test(
        NAME fuzz-diff-r388-gluefold-waiver-anchor-name
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r388-anchor-name-mq.bin
    )

    # the r389 drill (issue #1669, the post-r388 nightly's run
    # 45625878936). Comparator carry on the r386 waiver — no engine
    # change (the r318/r330/r336/r356/r366/r370/r376/r381/r385/r386/r388
    # precedent). Artifact
    # crash-8e44de52f13da2b780278f4893b884479d77deb3, 24,883 bytes: the
    # glue-fold composition family's 6th re-derivation and the SECOND
    # post-waiver one — the waiver HELD its contract (the fresh member
    # correctly declined). THE CAUSE: the mutation walk's CopyPart
    # PASTED a comparator dump fragment (`=VAL : ?#?\r2002:binary\r\r`)
    # into the input AS A MEMBER (input offset 3947). OWNERSHIP
    # ADJUDICATED to the waiver (not the r238 arm): the paste sits at a
    # MEMBER START of a flow-rooted `{` composition whose every other
    # member is r386-measured vocabulary riding clean; the r238 arm's
    # paste rows are anchor-led quoted-follower bodies past a clamp cut
    # — a different position class. The A/B (libfyaml 04e0b58) met the
    # r318 dual-consensus bar on the isolated pasted member (key
    # `=VAL`, the folded plain `?#? 2002:binary`, BOTH passes) — engine
    # divergence refuted. TWO underivable faces, resolved by
    # MEASUREMENT: (1) the fold/blank model — a blank-only run padding
    # a `:` VALUE boundary is EATEN, not ridden (probes `{=VAL : x,}` /
    # `{=VAL  : x,}` / `{x  : y,}` / `{x  y : z,}` byte-EQUAL; the
    # non-boundary `{x :2002:binary,}` rides verbatim) — the old model
    # rode the run into the key (`=VAL ` vs the actual `=VAL`, the
    # one-byte gap at dump offset 7587); the r386 grammar never carried
    # a blank-before-boundary member, so no claimed face changes. (2)
    # the content-set prop-render twin at the GLUED-`?` slot (measured
    # `{?&m,}`: yeptris `?&m` flat, libyaml `&m ` prop + empty) — the
    # floor carried this face as the probe `&m ` exceeding its
    # allowance 2 > 1; the twin is anchors-only (the alias twin renders
    # `=ALI *<name>`, never a VAL line). THE FIREWALL also gained its
    # precise tamper contract: the two invariance domains the r386
    # accounting cannot see (the PS<->LS canon-fold flip the artifact's
    # sampled flip hit; the del-into-empty tolerance the floor's
    # sampled dels hit — both domains predate r389, the r386/r388 seeds
    # missed them by sampling luck) are computed independently
    # (yd_family_tamper_in_domain): admission-relevant tampers still
    # must decline, in-domain hits are counted and asserted to waive.
    # THE FLOOR: greedy ddmin under the dual+shape bar (DIFFER + the
    # paste signature `=VAL : =VAL` in the yeptris dump) to its
    # 109-byte plateau
    # `{?,?l,?I,?#\r a,?#,=VAL:\r0,?&m,?#,\r1,2,&m,g,<PS>0:\r1,?#,\r y,m,1,<PS>1:\r7,g:\r.,1,<PS>5:\r1,i,m,-|,g,y,m,y|,7,0,1,1,0}`
    # (`\r` = CR bytes, `<PS>` = the raw E2 80 A9 trinity — the r386
    # measured PS-led key class) — the paste member's ` : ` spacing
    # rode down to the glued form (`=VAL:\r0`), the `?&m` member
    # carrying the twin face. PRE_RC=77
    # comparator-only, no engine change. The negative control
    # waiver-neg-paste-member-split.bin pins the ISOLATED paste member
    # (`{=VAL : ?#?\r2002:binary\r\r ,}`): libyaml ERRORS it
    # (YEPTRIS-ONLY-ACCEPT — the flow-`?` fires the KEY token inside
    # the value slot with no node) — a split the waiver must NEVER
    # claim; only the in-composition reading (the artifact's, where
    # libyaml's KEY-token law eats the pasted head) rides.
    add_test(
        NAME fuzz-diff-r389-gluefold-waiver-paste-member
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r389-paste-member.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r389-paste-member-artifact.bin
    )

    # the r390 drill (issue #1671, the post-r389 nightly's run
    # 45643869326). Comparator carry on the r386 waiver — no engine
    # change (the r318/r330/r336/r356/r366/r370/r376/r381/r385/r386/
    # r388/r389 precedent). Artifact
    # crash-89b18e7f2d90c25c4af351b15fa1e3c81167c320, 16 bytes: the
    # glue-fold composition family's 7th re-derivation and the THIRD
    # post-waiver one — the waiver HELD its contract (the fresh member
    # correctly declined at the exact offset, the r389 debug walk's
    # `STOP: fw_member declined at 7 (member 2)`). THE CAUSE: the
    # bare-anchor member's name rides the GLUED-CHARSET cut
    # (`&m:~y` ONE name — the engine's prop walk keeps `:` `~` `(`
    # inside the name per the r318 suite law, 2SXE/W5VH; the waiver
    # grammar's name cut was fc_alpha-only). MEASURED this round at
    # the bare-anchor MEMBER slot (`&m:~y,` / `&m:(,` / `&m:x,` /
    # `&a:b1,` — yeptris renders `=VAL : &<name> ` + the parser's
    # inserted empty; libyaml splits at the first glued colon into the
    # anchored-empty key `&<n> ` + the plain value); the break ride
    # re-measured (`{&m:~y\r,}` / `{&m:~y\r  ,}` identical); the
    # SEQ-root twin measured (single line, unwrapped). The SECOND face
    # (member 1 `?*2:`, the glued-`?` plain whose colon is a
    # `:`+BREAK VALUE boundary — derives fine on the yeptris side)
    # rides the same basis: libyaml's `=ALI *2` render (the IS_ALPHA
    # alias cut at the glued-`?` slot) maps in via the alias-cut
    # source rule (`=ALI *<name>` INTO the flat `?*<name>` plain,
    # fc_alpha names only, its own \x01 bucket — the event kinds
    # differ, a `=VAL : *...` probe never rides the alias source).
    # The content-set also gained the r318 SPLIT rule (a prop render
    # `&<n>:<s> ` sources the `&<n> ` key render + the `<s>` value
    # plain) and the MAP-root glued-`?` colon render law (the flat
    # pair inside the root map; the single-pair wrapper fires only on
    # the SEQ root — probed `[?*2:\n,s]`). The head-split fingerprint
    # widened from `?#` to ANY flat `?`-led yeptris line libyaml
    # lacks (the flow-`?` KEY-token law consumes the `?` at every
    # member-start spelling). THE A/B: libfyaml 04e0b58 reads the
    # artifact and every probe face byte-consistently yeptris's way
    # in BOTH strict-1.2 and --yaml-1.1 passes — the r318
    # dual-consensus bar, met; engine divergence REFUTED.
    # comparator-only, no engine change. THE FLOOR: greedy ddmin
    # under the dual+shape bar (pre-patch abort + post-patch
    # ledgered-waiver + the glued-charset prop line AND the `=ALI`
    # line in the dumps) to its 13-byte plateau
    # `{?*2:\n,&m:y\r}` (`\r` = the CR byte) — the `~` and the
    # trailing plain fell away, both faces intact. FIREWALL
    # boundaries kept OUT (all probed): the colon-END name
    # (`{&m:\r,s}` rides `m:` on yeptris; the render depends on the
    # FOLLOWER via the engine's un-consume law — the
    # waiver-neg-anchor-colon-end control), the props-led second
    # anchor (`{&m:~y\r\r&s:~t,}` YEPTRIS-ONLY-ERRORS on the
    # duplicate-anchor law — the waiver-neg-anchor-props-led
    # control), and the anchor-only compositions without a `?`-led
    # head (the fingerprint keeps them on the r318 ARM — the
    # waiver-neg-anchor-nohead control).
    add_test(
        NAME fuzz-diff-r390-gluefold-waiver-glued-charset
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r390-glued-charset-anchor.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/waiver-gluefold-r390-glued-charset-anchor-artifact.bin
    )

    # r392 (the post-r391 Fuzz Nightly round, run 45675392491, headSha
    # verified = the r391 merge fd8e74d4). Comparator carry on the BOM
    # line-start arm -- no engine change, NO WAIVER WIDENING, NO
    # entry-law surgery (the r318/r330/r336/r337/r338/r364/r365
    # precedent). Artifact crash-ae0a21fb…30fac79, 80 bytes: the
    # block-level plain at the document root
    # `<BOM><BOM>&U:#` + 8sp + `text` + 58 trailing spaces (13-byte
    # dual-bar floor `<BOM><BOM>&U:# t `). The red is OUTSIDE the
    # r386 waiver's domain twice over -- the entry byte gates decline
    # at offset 0 (a line-start BOM, the classifier's own documented
    # entry law, untouched this round) and there is no flow root
    # (law j) -- so the r391 glued-#-tail extension HELD its
    # contract and the face lives in the BOM-LEAD ARM FAMILY: the
    # r364 composition twin's verbatim spelling copies the raw bytes
    # while BOTH engines strip the trailing blank run (the plain
    # law: trailing blanks at the scalar's end-of-line are
    # position-free whitespace; the 58-space run is gone from BOTH
    # dumps byte-identically), so `want` never matches and the
    # r156/r338 prop-split twin cannot rescue it (the residual
    # `&U:#        text` carries interior blanks, the invented-key
    # guard declines). MEASURED this round: the BOM-free twin
    # `&U:#        text` is a YEPTRIS-ONLY-REJECT split (the
    # 2SXE/Y2GN charset walk refuses the unspaced `#`, the r364
    # removal-twin's own ledgered reading), so the BOM is what turns
    # yeptris's `&` into content (catch-121) and the composition
    # face exists only under the BOM lead; the trailing blank is
    # what turns the claimed 74-byte shape (`<BOM><BOM>&U:#` + 8sp +
    # `text`, claimed by the r364 verbatim spelling) into the red
    # 80-byte one. THE CARRY: the composition spelling strips the
    # trailing blank run (spaces/tabs) from the copied value bytes
    # -- the plain law's own strip, exactly what both dumps already
    # left out; interior blanks ride verbatim; any other yeptris
    # reading still moves a byte and declines. The A/B: libfyaml
    # 04e0b58 renders the artifact and every trailing-blank probe
    # face as the ONE double-quoted plain `<FEFF>&U:#<blanks>text`
    # byte-consistently yeptris's way on BOTH strict-1.2 and
    # --yaml-1.1 passes (the r318 dual-consensus bar, met; probed
    # this round on the artifact and 23 faces); the suite is silent
    # (no BOM tests at the da267a5c pin, the r364/r365 precedent).
    # Floor re-verified: ddmin chunked descent 80->30 under the
    # dual+shape bar, then all single-byte deletions break the bar
    # (interior blank, `t`, trailing blank each load-bearing). FIREWALL
    # boundaries kept OUT (probed): the BOM-free lead (t01/t02/t05 --
    # yeptris-only-reject, unmeasured), the one-BOM lead (t03 --
    # yeptris errors after the mark strip, unmeasured), and the
    # flow-root `{&U:#<LF>,s}` spelling (t10 -- yeptris errors, the
    # r318 law's territory, unmeasured). Growth ledger: family
    # nightly re-derivations now 9 (r377, #1646, #1649, #1656,
    # #1665/r388, #1669/r389, #1671/r390, #1675/r391, this one);
    # this round's red was OUTSIDE the extended grammar at the entry
    # gates and the waiver itself needed NO widening -- an
    # arm-classification carry, not a grammar extension.
    add_test(
        NAME fuzz-diff-r392-bom-line-start-trailing-blank
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-trailing-blank-r392.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-trailing-blank-r392-artifact.bin
    )

    # r396 (the post-r395 Fuzz Nightly round, run 45725924584, main
    # 52af78a6, issue #1691): the anchor-PS composition at the BLOCK
    # root, comment-led — the realworld-suite spec example 2.2 (SYW4's
    # baseball comments) spliced by the walker's CopyPart with the
    # `---` marker bytes, raw PS breaks, and the `&--02:in~---` anchor:
    # the 32-byte floor "#<PS>&--02:in~---<PS><PS>? I &--nn~". yeptris
    # rides the Y2GN suite law (the FULL colon-riding 1.2 anchor name
    # `--02:in~---`; the explicit-key `?` fires — PS honors the line
    # structure) and opens the anchored MAP with the `I &--nn~` key and
    # the empty value; libyaml cuts the name at the colon (its ledgered
    # 1.1 IS_ALPHA deviation) and its plain scan treats the PS as raw
    # CONTENT — the colon-led residual plain `:in~---` never sees a
    # break, swallows the `?` member, and renders ONE anchored plain.
    # The charset split reconciles (the space insert makes both engines
    # read the cut name; the PS render differs by the carried LS/PS
    # fold relation only) — but the arm's gate never armed: its comment
    # clause swallowed past the raw PS to the next LF/CR, consuming the
    # WHOLE input as comment (red IDENTICALLY on the pre-patch build —
    # the face predates r395; the r395 carry held). THE CARRY: the
    # charset arm's comment clauses (gate + transform walks) stop at
    # the r87 raw NEL/LS/PS breaks — the comment-led face then arms,
    # and the split+verify legs (libyaml byte-invariance, the copy
    # agreement mod the fold relation, the r95 solo ladder) keep every
    # unmeasured shape red. The A/B (libfyaml 04e0b58, probed on the
    # artifact + floor + all 19 composition-row faces): fy strict-1.2
    # errors or truncates at the glued `---` (rendering NO reading of
    # the member — never libyaml's one-plain), fy --yaml-1.1 rides
    # yeptris's explicit-key structure byte-exactly on every face —
    # NO fy mode sides with libyaml. The composition row (PS run
    # length 1/2/3, LF/LS twins, the blank/space/glued `?` followers,
    # the `#`-content lead, the marker mid-stream/leading/trailing/
    # glued) measured claim-stable on every row; the same-line `?`
    # follower is yeptris-only-reject (not a DIFFER, stays out). The
    # suite: SYW4 is the splice source (a clean comment example, no
    # anchor); Y2GN governs the name bytes (the colon RIDES —
    # libyaml's cut is the ledgered deviation); the suite has NO
    # LS/PS row (the r183 family carries the render). Floor
    # re-verified: ddmin 116->32 under the dual+shape bar, red
    # identically on the r394-era build. Growth ledger: family nightly
    # re-derivations now 13 (r377, #1646, #1649, #1656, #1665/r388,
    # #1669/r389, #1671/r390, #1675/r391, #1677/r392, #1682/r393,
    # #1684/r394, #1687/r395, this one) — the FIRST in the
    # realworld-splice territory: the walker reaches the composition
    # by CopyPart of the realworld corpus, not by mutation of a family
    # seed or a dict entry.
    add_test(
        NAME fuzz-diff-r396-anchor-ps-comment
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-ps-comment-floor.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/anchor-charset-ps-comment-artifact.bin
    )

    # r397 (the post-r396 Fuzz Nightly round, run 45736164715, headSha
    # verified = the r396 merge 68ae4f2a, issue #1694): the flow
    # VALUE-slot RIDER face — the `?*2` head, the `&m:` anchored value,
    # the tag-riding plain, and the mid-collection `*VAL` alias tail —
    # the 38-byte artifact `{?*2,&m:tag:yamlabcdefgh==ALI *VAL\r,s}`
    # and its 35-byte dual-bar floor `{*2,&m:tag:yamlabcdefgh==ALI
    # *VAL\r}` (greedy ddmin under the exact-nightly-pair bar). yeptris
    # fires the tail as a REAL ALIAS EVENT at the member slot
    # (`=ALI *VAL` + the empty pair) — the carried r318 glued-name law
    # swallows `m:tag:yamlabcdefgh==ALI` into the anchor name, the
    # separating space puts `*VAL` at a NODE position, and the alias
    # indicator fires there ([104]); libyaml rides the whole tail as
    # ONE plain value (`tag:yamlabcdefgh==ALI *VAL`) THROUGH its own
    # ledgered r390 IS_ALPHA cut + flow-`:` quirk (once the bytes are a
    # plain, spec [130] admits the mid-plain `*`). Red IDENTICALLY on
    # the pre-patch build (/tmp/ywt-r394: exit 77) — the face predates
    # r395 AND r396; the r396 carry held. THE RIDER-ROW MEASUREMENT
    # (15-row matrix over the four follower axes): the alias-tail lead
    # (glued/spaced/member-boundary/absent), the terminator
    # (CR/LF/CRLF/none/trailing member), the tag-plain content
    # (tag:-URI/bare/minimal), and the head (`?*2`/bare `*2`) — the
    # face manifests on EVERY row where a SPACE leads the `*VAL` (9 of
    # 15 rows, all DIFFER), regardless of lead, terminator, plain
    # content, or head; the member-boundary twin (`,*VAL` — a real
    # alias member both engines agree on) and the no-terminator twin
    # are already admitted (family-classified), and the r390-class
    # siblings (the spaced-`==ALI`/glued-name rows where the star
    # never reaches a node position) ride fy dual-consensus but stay
    # unmeasured waiver shapes. THE CONSENSUS BASIS: NONE. fy 04e0b58
    # ERRORS "unexpected alias" in BOTH strict-1.2 and --yaml-1.1 on
    # EVERY row where the face manifests (13 error rows; it never
    # renders libyaml's one-plain, and under the carried r318 name
    # ride the tail is PROPS-ON-ALIAS — a shape the strict grammar
    # has no production for, [159]/7.1 "an alias node must not
    # specify any properties"); the suite is SILENT on the mid-plain
    # `*` (the alias rows it has — SR86/SU74/26DV/6M2F/LE5A — are all
    # token-start aliases; no row governs a space-led `*` after plain
    # content); and the two engines' renders rest on OPPOSITE
    # resolutions of the already-ledgered r390 bucket (the glued
    # `&m:` colon: yeptris+fy ride the name, libyaml cuts it), so
    # neither engine's tail reading has an independent basis. THE
    # ADJUDICATION: DECLINED-BY-DESIGN — the face stays red in the
    # comparator (documented, unreached-by-consensus), the waiver is
    # NOT widened, NO arm is added, NO engine change. OWNER-ESCALATION
    # DATUM (recorded, for the owner to resolve): the tail's fate
    # follows mechanically from the r390 bucket split — resolving
    # whether yeptris's plain/alias scanner should ride the tail as
    # content (spec [130]) or keep firing the node-position alias is
    # an ENGINE decision, not comparator-classifier work. THE CARRY:
    # the floor + artifact bytes pin as the waiver's negative-control
    # set entries waiver-neg-alias-tail-ride{,-artifact}.bin — the
    # family-classify N-phase requires the waiver to DECLINE them
    # forever (a DIFFER-shaped declined control, the first in the
    # set: the older neg controls are one-engine-error or
    # arms-claimed DIFFERs) — and because a live DIFFER cannot ride
    # the nightly's comparator seed set, the snapshot step excludes
    # these pins AND the mutation-dense waiver-gluefold-* family (the
    # r1306/r361/r379/r381 treadmill law: the r397 floor is bytes
    # away from the r394/r393 pins, proven by this round's own
    # red); the pins stay under the full ctest (the family-classify
    # entry walks them). Firewall unchanged: the r396 anchor-PS row,
    # the r395 empty-slot row, the r390 alias-cut bucket, and the
    # r394 prop-charset row keep their arms. Growth ledger: family
    # nightly re-derivations now 14 (r377, #1646, #1649, #1656,
    # #1665/r388, #1669/r389, #1671/r390, #1675/r391, #1677/r392,
    # #1682/r393, #1684/r394, #1687/r395, #1691/r396, this one) —
    # the rider family's own vocabulary (the r389-r395 laws' terms
    # recombined); the consensus-basis question DID change the
    # disposition: every prior rider carry met the r318 dual-
    # consensus bar, this one cannot, and the face declines by
    # design instead of widening past the bar.
    # 50-mutant paired battery (30 single + 20 double, seed 397,
    # size-asserted mutator) over the floor + artifact + the r361/
    # r384/r387/r389/r390/r391/r393/r394/r395-law/r396 floors and
    # artifacts: 1100 mutants, 1051 ok-clean, 49 ok-differ (the
    # declined face's own neighborhood stays red), 0 regressions.

    # r398 (the post-r397 Fuzz Nightly round, run 45748018830, headSha
    # verified = the r397 merge 6728c3b9, issue #1697): the BLOCK-root
    # FOLDED-HEADER exotic-break face — the folded `>` scalar header,
    # the space-`#` comment lead, the raw PS/NEL/LF break run, and the
    # trailing NEL-glued `#` — the 21-byte artifact `> #<PS><PS>
    # <NEL><LF><PS>  <NEL>#` and its 16-byte dual+shape-bar floor
    # `> #<PS><PS><NEL>  <NEL>#` (greedy ddmin under the
    # exact-nightly-pair bar: unclassified YD_DIFFER + yeptris renders
    # the multi-line plain with a raw NEL pair in its scalar content +
    # libyaml renders the scalar EMPTY; the artifact's lone interior LF
    # and one separation space were droppable). yeptris reads the
    # folded block scalar with the PSes folded to leading break LFs and
    # the NELs KEPT RAW as first-class content (the r220 kept-NEL
    # block-content law), so the trailing `#` is glued MID-CONTENT and
    # rides as content — the value is a multi-line plain
    # (`\n\n<NEL>#\n` on the floor, `\n<NEL>\n\n <NEL>#\n` on the
    # artifact). libyaml (its 1.1 reader BREAKS at NEL) sees the line
    # after the trailing NEL start with `#` at the content column — a
    # comment — and eats the whole body: the scalar renders EMPTY. Red
    # IDENTICALLY on the pre-patch build (this worktree IS the
    # pre-patch tree: exit 77, byte-identical dump pair) — the face
    # predates r397; the r397 carry held its contract (the declined
    # rider face is nowhere in the red; the comparator corpus read all
    # 1602 files clean; the red came by MUTATION ~106s into the diff
    # budget, never a seed). THE FOLD-ROW MEASUREMENT (18-row matrix
    # over the header/comment/break/tail axes): the face manifests
    # (10 of 18 rows, all with the byte-identical EMPTY-vs-kept-NEL
    # split) on every row that has BOTH the space-`#` comment lead
    # (dropping it — `>` with no comment, or the glued `>#` — renders
    # both engines agreeing, CLEAN) AND a NEL-glued trailing `#`
    # (dropping the second NEL so `#` is space-led after the PS run —
    # CLEAN, both engines empty); the PS run length is free (1/2/3
    # PSes all DIFFER, the run folds to the same leading LF count on
    # yeptris's side), the NEL position is free (first or second),
    # the LF placement is free (interior or trailing, the lf-mid row
    # DIFFERs identically), the trailing `#` may be spaced (the
    # spaced-hash row keeps the split with a content space), and the
    # LITERAL twin (`| #...`) DIFFERs byte-identically — the header
    # style is free. The plain-root twin (no block header) is CLEAN
    # and the header-alone baseline is CLEAN. THE CONSENSUS BASIS:
    # NONE — fy strict-1.2 renders the scalar EMPTY (`>` on every
    # probed DIFFER row, `|-` on the literal twin) SIDING WITH
    # libyaml; fy --yaml-1.1 ERRORS outright ("block scalar with
    # wrong indented line after spaces only") on every probed DIFFER
    # row; the suite carries NO raw NEL/PS row (grep over the src/
    # tree: zero hits for C2 85 and E2 80 A9), so no row governs.
    # yeptris's kept-NEL reading is the strict-1.2-printable reading
    # ([95] c-printable admits 0x85 and nb-char excludes only the
    # 1.2 breaks CR/LF/CRLF — NEL is content, the `#` is glued
    # mid-content, no comment production fires), but BOTH comparators
    # ride the 1.1 break-at-NEL legacy, so no consensus basis sides
    # with yeptris and the r318 dual-consensus bar cannot be met.
    # THE ADJUDICATION: DECLINED-BY-DESIGN — the face stays red in
    # the comparator (documented, unreached-by-consensus), the r386
    # waiver is NOT widened, NO arm is added to the classifier chain,
    # NO engine change. OWNER-ESCALATION DATUM (recorded, for the
    # owner to resolve): the split is the 1.1-legacy break-at-NEL
    # (libyaml, fy) vs the strict-1.2 printable-content reading
    # (yeptris) — yeptris's render is arguably the spec-correct 1.2
    # one; whether the emitter/scanner keeps it or the comparators'
    # de-facto reading wins is an ENGINE decision, not
    # comparator-classifier work. THE CARRY: the floor + artifact
    # bytes pin as the waiver's negative-control set entries
    # waiver-neg-foldheader-nel-comment{,-artifact}.bin — the
    # family-classify N-phase requires the waiver to DECLINE them
    # forever — and because a live DIFFER cannot ride the nightly's
    # comparator seed set, the snapshot step excludes these pins (the
    # r1306/r361/r379/r381 treadmill law; the r398 red itself was
    # reached by mutation, never a seed). Firewall unchanged: the
    # r397 declined rider row, the r396 anchor-PS row, the r395
    # empty-slot row, the r390 alias-cut bucket, and the r394
    # prop-charset row keep their arms. Growth ledger: family nightly
    # re-derivations now 15 (r377, #1646, #1649, #1656, #1665/r388,
    # #1669/r389, #1671/r390, #1675/r391, #1677/r392, #1682/r393,
    # #1684/r394, #1687/r395, #1691/r396, #1694/r397, this one) — the
    # NINTH by-mutation re-derivation, and the second consecutive
    # consensus-basis decline: the comparator-snapshot exclusion
    # worked exactly as designed (the gluefold family and the
    # alias-tail-ride live-DIFFER pins never seeded the diff leg), so
    # the red is a DIFFERENT family — the block-root folded-header
    # exotic-break composition, the r317/r333 law-b break-fold +
    # comment-eat family's vocabulary. The pins ride the
    # fuzz-diff-family-classify N-phase ONLY (a declined live DIFFER
    # cannot carry a standalone fuzz_diff add_test — the walker
    # aborts on every unclaimed split by design).

    # r399 (the post-r398 Fuzz Nightly round, run 45759021837, headSha
    # verified = the r398 merge 17b2fd99, issue #1702): the FLOW-root
    # plain-`?` member face — the 18-byte artifact `[1or: "\rg",?g?,""]`
    # (a raw CR inside the head pair's double-quoted value) and its
    # 14-byte dual+shape-bar floor `[r: "",?g?,""]` (greedy ddmin under
    # the unclassified-YD_DIFFER + yeptris-renders-the-plain-member +
    # libyaml-renders-the-`?`-opened-map bar; the CR and the head
    # spelling `1or` were both droppable). Both engines ACCEPT; the
    # streams split on the SECOND member: yeptris reads `?g?` as a
    # PLAIN scalar (the carried law-d plain-`?` rule), libyaml opens
    # its flow-`?` KEY wrapper (key `g?`, the parser's empty value) and
    # the trailing member rides after the wrapper. THE CONSENSUS BASIS
    # IS PRESENT, UNLIKE r398: the suite's HM87 row ("Scalars in flow
    # start with syntax char") pins `[?x]` -> `=VAL :?x` PLAIN — the
    # suite GOVERNS the member row (yeptris's reading IS the suite
    # bar); fy strict-1.2 AND fy --yaml-1.1 render `?g?` plain on the
    # floor, the artifact, and every probed DIFFER row (both engines
    # byte-equal on the head pairs themselves — plain-led, quoted-led,
    # empty-quoted, escape-carrier — probed via yepdiff). The member-row
    # measurement: the split needs only the flow map head + the `?g?`
    # member (the CR carrier is free: LF/CRLF/none all split
    # identically; the terminator member is free: `"x"`/none both
    # CLEAN — the classification, not the engines, needed the third
    # member for the pre-widening derivation). THE GAP WAS CLASSIFIER
    # COVERAGE, not an engine defect: the #1310 derivation declined at
    # the value walk's outright `"`/`'`/`!` refusal (y_value_node),
    # so any DIFFER sharing the seq with a DQ-valued head pair stayed
    # red. THE CARRY: the DQ value slot joins the derivation
    # (fc_quoted_value — the r337/r374/r308 gates — derives the span;
    # the quoted VALUE's glued `:` and the SQ/`!` slots stay declined,
    # unmeasured) and the r314 quoted-KEY member's unconditional-colon
    # law takes the SEQ-root single-pair wrapper twin (the probed
    # `["1or": "\rg"]` / `["r": "",t]` heads render EQUAL on both
    # engines). Pins: the floor + artifact + the quoted-key head
    # composition ride the flow-qsoup classify N-phase AND the named
    # walk below (claimed faces can carry a standalone fuzz_diff
    # add_test). Firewall unchanged: the r398 foldheader-nel-comment
    # row, the r397 alias-tail-ride row, the r396 anchor-PS row, the
    # r395 empty-slot row, the r390 alias-cut bucket, the r394
    # prop-charset row keep their arms. Growth ledger: family nightly
    # re-derivations now 16 (r377, #1646, #1649, #1656, #1665/r388,
    # #1669/r389, #1671/r390, #1675/r391, #1677/r392, #1682/r393,
    # #1684/r394, #1687/r395, #1691/r396, #1694/r397, #1697/r398,
    # this one) — the TENTH by-mutation re-derivation, and the FIRST
    # governing-row carry: a suite row (HM87) sides with yeptris and
    # fy rides it on both 1.2/1.1 passes.
    add_test(
        NAME fuzz-diff-r399-dqvalue-head
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqvalue-head-r399.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqvalue-head-r399-artifact.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/flow-qsoup-dqvalue-quotedkey-r399.bin
    )

    # r400 (the post-r399 Fuzz Nightly round, run 45768696397, headSha
    # verified = the r399 merge 7d923d69, issue #1704): the BLOCK-root
    # root-block-scalar content-indent face through a SPACE-INDENTED
    # header — the 60-byte artifact `#!##!!%#!ga#!##!!%#!ga␠␠\n␠|\n
    # ##%#!g#\n###\n##2#21%b\n#\n###\n#A` (a `#`-comment lead, the ` |`
    # literal header at column 1, and six `#`-led lines at column 0)
    # and its 4-byte dual+shape-bar floor ` |\n#` (greedy ddmin under
    # the exact-nightly-pair bar: unclassified YD_DIFFER + yeptris
    # renders the block literal + libyaml renders it EMPTY + the
    # pre-patch harness aborts; the comment lead and the body
    # spellings were all droppable — the header's own space was NOT:
    # dropping it re-enters the already-claimed col-0 family). Both engines ACCEPT; yeptris auto-detects the
    # content indent from the first non-empty line (spec 8.1.1.1, no
    # floor) so the `#`-led lines ride as literal content with the
    # breakless-EOF clip break, libyaml's unconditional
    # min-indent-1 clamp (scan_block_scalar_breaks: `if (*indent < 1)
    # *indent = 1`) ends the scalar at the first unindented line and
    # the `#`-led lines double as comments — the EMPTY render. THE
    # CONSENSUS BASIS IS PRESENT: the arm's law is already ledgered —
    # M7A3 "Spec Example 9.3" (root `|` + col-0 `%!PS-Adobe-2.0` line
    # RIDES, `=VAL |%!PS...\n`), DK3J (zero-indented folded body
    # RIDES; libyaml REJECTS it outright — measured YEPTRIS-ONLY-ACCEPT
    # on this comparator), FP8R (same shape) — all suite-VALID/
    # libyaml-rejects ledger rows with citations, and yeptris holds
    # the suite at 100% (395/395 with these rows in). fy 2.6.0 rides
    # the col-0 faces byte-exact in BOTH modes (strict-1.2 and
    # --yaml-1.1 render `|\n#` as `|` with content `#`, probed), then
    # emptying ONLY the space-indented-header composition (` |\n#A\n`
    # renders `|-`) — fy's own header-column floor, the comparator
    # split the arm's ledger lines already document (libfyaml rides /
    # PyYAML sides with libyaml's clamp, the r317 version-line
    # precedent). THE GAP WAS CLASSIFIER COVERAGE, not an engine or
    # law defect: yd_root_block_content_divergence's header detector
    # only fired at column 0, so the ` |` header never entered block
    # mode and the family walked past its own arm. THE CARRY: the
    # header detection skips leading blanks (the same header-spell
    # gate bounds it; a face the skip wrongly enters fails the
    # rebuild verification and stays red), and the
    # breakless-EOF clip residual gate the arm already anticipates
    # resolves the indented copy (measured: the artifact's indented
    # copy splits on exactly yeptris's trailing break, the break-fixed
    # copy EQUALS). Pins: the floor + artifact + the minimal
    # space-indented-header composition ride the arm as the named walk
    # below (claimed faces can carry a standalone fuzz_diff add_test).
    # Firewall unchanged: the r398 foldheader-nel-comment row, the
    # r397 alias-tail-ride row, the r396 anchor-PS row, the r395
    # empty-slot row, the r390 alias-cut bucket, the r394 prop-charset
    # row keep their arms. Growth ledger: family nightly re-derivations
    # now 17 (r377, #1646, #1649, #1656, #1665/r388, #1669/r389,
    # #1671/r390, #1675/r391, #1677/r392, #1682/r393, #1684/r394,
    # #1687/r395, #1691/r396, #1694/r397, #1697/r398, #1702/r399,
    # this one) — the ELEVENTH by-mutation re-derivation, and the
    # SECOND governing-row carry: the suite's `#`-comment block rows
    # (M7A3/DK3J/FP8R, the ledger lines the arm already cites) govern
    # the law, confirming the r399 HM87 pattern.
    add_test(
        NAME fuzz-diff-r400-block-bsi-root
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-bsi-root-r400.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-bsi-root-r400-artifact.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/block-bsi-root-r400-hdr1.bin
    )

    # r401 (the post-r400 Fuzz Nightly round, run 45782078711, headSha
    # verified = the r400 merge 93972085, issue #1708): the r238
    # quoted-body family's NEL RUN — the 10,910-byte artifact is ONE
    # 20-byte CopyPart insert (offset 7019, two raw NEL pairs and a
    # U+2129) into the r368 clamp-tail artifact 418cc31a itself, deep
    # past the 4095-char clamp cut where both dumps stay byte-identical
    # to the carried pin's; the r383 clamp-tail walk declined its own
    # family at the NEL's "engine-divergent" line. The r369 law already
    # measured the single NEL (READ_LINE's LF on libyaml, #638's LF on
    # yeptris); this round measures the RUN at engine level through the
    # r330 head — every cell forced-equal on BOTH engines (pairs spell
    # (n-1) newlines, mixed NEL/LF/CR runs count every member, blank
    # runs ride, the clean-LF twins byte-identical) — and the span
    # scanner admits the NEL into the r376 clean-break-run machinery.
    # The raw-LS/PS-facing compositions keep their declines (the
    # raw-join spell split is unmeasured); the clamp tail gains the
    # exotic-facing gate. The 20-byte ddmin floor
    # `&2?*: '<PS><CR>:<NEL><NEL><U+2129>'` (greedy chunk+byte ddmin
    # under the exact-nightly-pair bar: unclassified YD_DIFFER, yeptris
    # ONE root plain, libyaml the `&2 ?*`-keyed map) and the
    # mixed-run row `&2?*: 'x<NEL><NEL>\n<NEL>y'` (4 breaks -> 3
    # newlines on both engines) ride the arm as the named walk below.
    # THE SUITE GOVERNS THE SPLIT (the third governing-row carry): the
    # anchor-name rows 2SXE ("Anchors With Colon in Name") and W5VH
    # ("Allowed characters in alias" — the name charset literally
    # carries `?`, `*`, `!`, `"`) pin the riding-name law the family
    # reads; fy 1.0.0 (04e0b58) renders yeptris's structure byte-exact
    # on floor + artifact + every probe face in BOTH modes (strict-1.2
    # and --yaml-1.1). The NEL spell itself is comparator-internal
    # (yeptris == libyaml forced-equal; fy is a third reading — raw
    # /`\\N`-escaped in 1.2, verbatim breaks in 1.1; the suite carries
    # no raw-NEL row). Firewall unchanged: the r398 foldheader-nel-
    # comment row, the r397 alias-tail-ride row, the r396 anchor-PS
    # row, the r395 empty-slot row, the r390 alias-cut bucket, the
    # r394 prop-charset row, and the r400 block-bsi-root row keep
    # their arms. Growth ledger: family nightly re-derivations now 18
    # — the TWELFTH by-mutation re-derivation, and the THIRD
    # governing-row carry (r399 HM87, r400 M7A3/DK3J/FP8R, this one
    # 2SXE/W5VH).
    add_test(
        NAME fuzz-diff-r401-anchor-quoted-follow-nelrun
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-nelrun-r401.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-nelrun-r401-artifact.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-quoted-follow-nelrun-mixed-r401.bin
    )

    # r402 (the post-r401 Fuzz Nightly round, run 45822172084, headSha
    # verified = the r401 merge 4b0f0be1, issue #1711): the charset
    # arm's STREAM-START BOM LEAD — the 13-byte artifact
    # `<BOM>&U::U:{}t ` is a fresh composition of the measured
    # anchor-name-charset row (yeptris munches the riding name `U::U`,
    # value `:{}t`; libyaml cuts at `U`, its IS_ALPHA 1.1 set, the
    # residual `::U:{}` re-lexing as the plain value) with a BOM lead
    # and the trailing-blank-at-breakless-EOF cell. The mark's raw
    # bytes plain-opened a phantom plain in the arm's gate walk (the
    # 0xEF is content to every state law), the `&` never armed, and
    # the family declined its own BOM-led compositions. MEASURED: the
    # red set is EXACTLY the BOM-led faces (the 7-byte ddmin floor
    # `<BOM>&U:U` under the exact-red bar — every single-byte deletion
    # breaks the bar; the BOM+`&U::U` head, the BOM+`&U::Ut `, and the
    # BOM+`&U:U:{}t ` probes all red pre-patch), while EVERY no-BOM
    # twin classifies pre-patch through this very arm (`&U::U`,
    # `&U::Ut `, `&U:U:{}t `, `&a::b:c `, `&U::U:x ` — the r336/r381/
    # r385 residual laws hold). THE TRAILING BLANK at breakless EOF is
    # INERT: both engines strip it byte-identically in every probe
    # (the plain law, position-free whitespace) — measured, no law of
    # its own. The `{}` glyphs ride as block-context plain content on
    # both engines (the `{` never reaches a node position: the value
    # plain opens at the riding `:`). THE CARRY: the gate walk and the
    # copy walk skip a stream-start BOM zero-width (spec 5.1; the
    # catch-121 law — both engines strip the mark at the stream head),
    # so the `&` arms and the split-copy verify runs; a mark at any
    # LATER line start stays the r226/r364 family
    # (yd_bom_line_start_divergence). THE SUITE GOVERNS THE SPLIT
    # (the FOURTH governing-row carry): Y2GN ("Anchor with colon in
    # the middle" — `&an:chor` anchors `an:chor`, the mid-name colon
    # the floor rides), 2SXE ("Anchors With Colon in Name") and W5VH
    # ("Allowed characters in alias") pin the riding-name law;
    # libfyaml 04e0b58 sides WITH yeptris on every boundary face in
    # BOTH modes, alias-resolution-verified (`k: &U:U` + `*U:U`
    # resolves, `*U` errors alias-not-found; the artifact's munch
    # `U::U` resolves the same way; the floor `<BOM>&U:U` parses
    # clean in strict-1.2 AND --yaml-1.1) — fy's own munch extends
    # one colon further on the `:{}`-adjacent cell (`U::U:`), a
    # suite-silent comparator-internal free cell, and fy's full-
    # artifact pass takes its own `{}`-at-node-start recovery
    # (missing-document-start error at the `t`), a third reading of
    # the residual, not of the anchor axis. Comparator carry only, no
    # engine change (the r142/r76/r318/r330/r336/r337/r338/r364/r365/
    # r370/r381/r385 precedent). Firewall unchanged: the r401 nelrun
    # row, the r400 block-bsi-root row, the r398 foldheader-nel-
    # comment row, the r397 alias-tail-ride row, the r396 anchor-PS
    # row, the r395 empty-slot row, the r390 alias-cut bucket, the
    # r394 prop-charset row keep their arms. Growth ledger: family
    # nightly re-derivations now 19 — the THIRTEENTH by-mutation
    # re-derivation, and the FOURTH governing-row carry (r399 HM87,
    # r400 M7A3/DK3J/FP8R, r401 2SXE/W5VH, this one Y2GN/2SXE/W5VH).
    add_test(
        NAME fuzz-diff-r402-anchor-charset-bom
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-charset-bom-r402.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-charset-bom-r402-artifact.bin
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/root-anchor-charset-bom-r402-pair.bin
    )

    # r403 (the post-r402 Fuzz Nightly round, run 45843890557, headSha
    # verified = the r402 merge 1a3e93b6, issue #1715): the r392
    # double-line-start-BOM family's FOURTH live shape — the 81-byte
    # artifact `<BOM><BOM>&U:#` + blank runs + `text` + `1` +
    # `=VAL : ` + `<BOM>` + trailing blanks, red by MUTATION ~24s into
    # the diff leg's walk (INITED over all 1602 comparator-snapshot
    # files clean — never a seed), both engines ACCEPT. The dump pair:
    # yeptris `+STR +MAP =VAL : <BOM>&U:# … =VAL =VAL : <BOM> -MAP
    # -STR` (the stream-start mark stripped, the SECOND mark kept as
    # CONTENT, the whole head riding ONE plain key — the glued `#`
    # mid-token content, the `=VAL : ` paste splitting the map, the
    # value plain `<BOM>`), libyaml `+STR +MAP =VAL : &U :# … =VAL
    # =VAL : <BOM> -MAP -STR` (BOTH head marks stripped — its
    # column-0 skip, the r226 law; the anchor cut at `U` — its IS_ALPHA
    # 1.1 set; the `:#…` residual re-lexing as the plain; the same map
    # split, the same content-BOM value). THE r402 CARRY HELD ITS
    # CONTRACT — the red is OUTSIDE it (the offset-0 skip is nowhere
    # in the face). THE FLOOR: greedy ddmin under the exact-nightly
    # bar (rc 77 unclassified) to the 17-byte plateau
    # `<BOM><BOM>&U:# L: <BOM>` — every single-byte deletion breaks
    # the bar (the double mark, the `&U:#` glue, the ` L`, the split
    # colon, and the trailing mark are each load-bearing). THE AXES
    # (all measured): the trailing `<BOM>` value rides CONTENT
    # byte-exact on BOTH engines (`=VAL : <BOM>` in both dumps —
    # libyaml keeps non-line-start marks); the ` : `/`: ` map split
    # agrees on both engines at the same colon; the anchor-charset cut
    # is a PASSENGER of the mark fate (without the offset-3 mark the
    # `&` arms on both sides and the face joins the carried
    # anchor-charset family). THE CONSENSUS BASIS: fy 04e0b58 sides
    # WITH yeptris on EVERY probed face in BOTH its strict-1.2 and
    # --yaml-1.1 passes — the floor renders `"<FEFF>&U:# L":
    # "<FEFF>"` (mark-content key, glued plain, byte-exact
    # mark-content value), the artifact renders the full one-entry map
    # with the interior blank runs verbatim and the split at `=VAL :
    # `; the suite carries ZERO BOM rows at the da267a5c pin — no
    # declined-by-design basis exists (the r402 precedent), but THE
    # TREADMILL DECISION (the measurement issue #1715 owes): each
    # prior arm patch on this family bought exactly one round
    # (r364 anchor-colon -> r365 map-follower -> r392 trailing-blank
    # -> this map-split), the mutation source rode the comparator
    # snapshot UNEXCLUDED (the r392 pins seeded the diff leg), the
    # floor is 4 edits from the r392 pin (`t`->`L`, the `: ` insert,
    # the trailing mark — the artifact's `=VAL : ` is the harness's
    # own dump vocabulary pasted into the plain), and the map-aware
    # widening's gate surface (deriving the plain-law split colon from
    # raw bytes) is itself mutation-dense — so the r370/r375/r380/r397
    # law applies and the WHOLE bom-line-start family JOINS THE
    # EXCLUSION (`bom-line-start-*` + the new neg-control names in the
    # nightly's snapshot step) instead of widening again. Comparator
    # carry only, no engine change, NO arm change — the face stays red
    # by design in the comparator. THE PINS: the floor + artifact
    # bytes pin as the live-DIFFER negative controls
    # waiver-neg-bom-line-start-map-split-r403{,-artifact}.bin (the
    # family-classify N-phase requires the waiver to decline them
    # forever — measured this round: 18 negative controls, 13
    # differing, 0 waived, 0 failures), and the arms-claimed map-split
    # pair `<BOM><BOM>L: <BOM>` pins green below (the removal twin
    # claims it — the regression gate for the mark-fate x map axis the
    # arms already cover). Firewall unchanged: the r402 BOM-led
    # charset row, the r401 nelrun row, the r400 block-bsi-root row,
    # the r398 foldheader-nel-comment row, the r397 alias-tail-ride
    # row, the r396 anchor-PS row, the r395 empty-slot row, the r390
    # alias-cut bucket, the r394 prop-charset row keep their arms.
    # Growth ledger: family nightly re-derivations now 20 (r377,
    # #1646, #1649, #1656, #1665/r388, #1669/r389, #1671/r390,
    # #1675/r391, #1677/r392, #1682/r393, #1684/r394, #1687/r395,
    # #1691/r396, #1694/r397, #1697/r398, #1702/r399, #1704/r400,
    # #1708/r401, #1711/r402, this one) — the FOURTEENTH by-mutation
    # re-derivation, the r392 family's THIRD live shape after its
    # carry; the treadmill status: a family on exclusion stops
    # re-deriving (verified by the next nightly's INITED walk).
    add_test(
        NAME fuzz-diff-r403-bom-line-start-map-split
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/bom-line-start-map-split-r403-pair.bin
    )

    # r404 (the post-r403 Fuzz Nightly round, run 45858435542, headSha
    # verified = the r403 merge 5f6e81f5, issue #1718): the r387
    # root-anchor-quoted-follow vocabulary re-derived by MUTATION
    # (ShuffleBytes-EraseBytes-CrossOver, post-INITED — never a seed)
    # on a 4056-byte face — the anchor-ride head `&2?*: '` over the
    # `[[{""` quoted-body chain grown to ~398 reps, a folded-comment
    # rider `> #<PS> :> #<CARON> :<LF> <PS><NEL> 0` INSIDE the chain
    # (the r398 foldheader vocabulary composed into the r387 chain),
    # and the r381 law's `U:ba:`-tail. Both engines ACCEPT. The dump
    # pair: yeptris `+STR =VAL : &2?*: <chain-as-one-scalar-value>
    # -STR` (the spec anchor munch riding `?*` per the 2SXE/W5VH/Y2GN
    # law), libyaml `+STR +MAP =VAL : &2 ?* =VAL : <chain> -MAP -STR`
    # (its IS_ALPHA cut at `2`, the `?*` residual as the key plain) —
    # the same r387 three-way split, arm-carried. THE r403 CARRY HELD
    # AND VERIFIED: the red carries ZERO bom-line-start vocabulary and
    # the diff leg's INITED walk over the exclusion-extended snapshot
    # (1279 seeds, `bom-line-start-*` + `waiver-neg-bom-line-start-*`
    # deleted) was CLEAN — the r392 family stopped re-deriving exactly
    # as the r370 law predicts. THE BREEDING SEEDS (the FIRST round
    # identifying them by measurement): the mutant bred from NON-family
    # seeds still riding the comparator snapshot —
    # `qflow-depth-clamp-r368-artifact.bin`, the r368-era pin
    # near-byte-identical to the red (the same `&2?*: '` head, chain,
    # `U:ba:`-tail) whose name matches NO exclusion glob (r370 excluded
    # `root-anchor-quoted-follow-*`; the same face family's r368 carry
    # sat outside it). THE DENSITY MEASUREMENT (one 45s budget each,
    # fixed seed 404, the nightly bar): the r368 artifact seed alone
    # re-derives a face-matching red (3270 bytes,
    # `&2?*: '<PS><CR>"\u1:…` — its own shape shrunk);
    # `qflow-gluefold-ps-r384.bin` re-derives its own gluefold face
    # (`{c,?#,\r{c,?#,\r0,?#…`, 35 bytes);
    # `waiver-neg-anchor-quoted-body.bin` re-derives a 1-byte hop out
    # of its waiver's model (`&2?/:\r '<PS><CR>'`, 13 bytes);
    # `catch101-colon-quoted-follower.bin`, `flow-q-quoted-follower.bin`,
    # and the non-artifact `qflow-depth-clamp-r368.bin` stayed CLEAN —
    # the measured breeders (`qflow-depth-clamp-*`, `qflow-gluefold-*`,
    # `waiver-neg-anchor-quoted-body*`) join the snapshot exclusion,
    # the clean candidates ride. THE FLOOR: greedy chunk+byte ddmin
    # under the exact nightly bar (rc 77 unclassified + the map-split
    # shape check) walks the 4056 bytes to the 11-byte plateau
    # `&2: '<PS><NEL>'` — every single-byte deletion breaks the bar
    # (the anchor, the split colon, and EACH break byte are
    # load-bearing). THE CELL LAW (the probe matrix, all measured
    # through the harness + fy): PS and NEL are CONTENT singly on both
    # engines (`a<PS>b` / `a<NEL>b` agree — libyaml's READ_LINE folds a
    # lone NEL to one space, keeps PS verbatim); the ADJACENT `PS NEL`
    # order splits the fold join — yeptris `a<PS> b`, libyaml
    # `a<PS><LF>b` (READ_LINE normalizes NEL to LF but keeps LS/PS
    # VERBATIM, and the fold branch joins the verbatim leading break
    # with the trailing LF instead of folding to a space), the reverse
    # `NEL PS` order AGREES on both (the trailing verbatim PS joined
    # whole). THE CONSENSUS BASIS: fy 1.0.0 keeps BOTH break bytes RAW
    # in strict-1.2 on the floor and every probe face (the 1.2
    # c-printable reading — PS/NEL are printable content, b-break is
    # CR/LF/CRLF only) — siding with NEITHER engine's fold; fy
    # --yaml-1.1 folds all break runs (its own 1.1 reading); the suite
    # carries ZERO raw PS/NEL rows across all 351 src dirs — no
    # declined-by-design basis exists (the r402 precedent), and the
    # treadmill decision (the r370/r403 law) holds: the face is a
    # composition over seeds the exclusion should never have seeded, so
    # the measured breeders JOIN THE EXCLUSION instead of widening an
    # arm whose gate surface (deriving the PSNEL fold join from raw
    # bytes) is itself mutation-dense. Comparator carry only, no engine
    # change, NO arm change — the face stays red by design in the
    # comparator. THE PINS: the floor + artifact bytes pin as the
    # live-DIFFER negative controls
    # waiver-neg-psnel-quoted-fold-r404{,-artifact}.bin (the
    # family-classify N-phase requires the waiver to decline them
    # forever), and the AGREED reverse-order pair `&2: 'a<NEL><PS>b'`
    # pins green below (the fold-join regression gate for the axis
    # both engines already share). The psnel-quoted-fold pins stay out
    # of the snapshot (they are live DIFFERs / live-approved shapes the
    # diff leg must not seed from). Firewall unchanged: the r403
    # bom-line-start exclusion row (now VERIFIED), the r402 BOM-led
    # charset row, the r401 nelrun row, the r400 block-bsi-root row,
    # the r398 foldheader-nel-comment row, the r397 alias-tail-ride
    # row, the r396 anchor-PS row, the r395 empty-slot row, the r390
    # alias-cut bucket, the r394 prop-charset row keep their arms.
    # Growth ledger: family nightly re-derivations now 21 (r377,
    # #1646, #1649, #1656, #1665/r388, #1669/r389, #1671/r390,
    # #1675/r391, #1677/r392, #1682/r393, #1684/r394, #1687/r395,
    # #1691/r396, #1694/r397, #1697/r398, #1702/r399, #1704/r400,
    # #1708/r401, #1711/r402, #1715/r403, this one) — the FIFTEENTH
    # by-mutation re-derivation and the FIRST whose breeding seeds are
    # identified as an exclusion-chain GAP; the treadmill status: the
    # exclusion extension stops the r368/gluefold/waiver-anchor
    # vocabulary from re-deriving (verify by the next nightly's INITED
    # walk).
    add_test(
        NAME fuzz-diff-r404-psnel-quoted-fold-nelps-order
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/psnel-quoted-fold-nelps-order-r404-pair.bin
    )

    # r405 (the post-r404 Fuzz Nightly round, run 45880924803, headSha
    # verified = 0ee85036, the v0.6.89 tag, issue #1723): the r404
    # soak's own prediction landed — the diff leg passed INITED CLEAN
    # (the #1722 glob fix verified; the r404 psnel defect is CLOSED)
    # and red by MUTATION 52s in on a 113-byte `{?#?#|#|+`-led
    # flow-soup face bred by ONE CopyPart from
    # `flow-qate-run-r380-artifact.bin`, an r380-era pin riding the
    # comparator snapshot under a name matching NO exclusion glob.
    # Zero engine src/ change since 0.6.88 — the mutation walk
    # reaching a long-riding pool, not a regression. THE DENSITY
    # MEASUREMENT (the r404 census carried forward to its pool; one
    # 45s budget each, fixed seed 405, single-file seed dirs, the
    # nightly bar, over the WHOLE 74-seed `?#`-vocabulary pool): 64 of
    # 74 seeds re-derive a red within one budget (42 face-matching
    # gluefold, 22 neighboring documented-family faces); 10 stayed
    # clean for one budget — the pool re-derives wholesale, so the
    # EXCLUSION EXTENSION is the answer (the r375/r397 whole-family
    # law; the family globs sweep 104 seeds including 33 lineage
    # siblings, closing the naming-gap class forward). THE FACE:
    # DECLINED-BY-DESIGN — the glued `?#` cell inside a flow
    # collection splits the plain-vs-indicator resolution (yeptris
    # keeps `?#...` as plain content per the 1.2 ns-plain-first
    # reading — a `#` never starts a comment without preceding
    # whitespace; libyaml treats `?` as the complex-key indicator and
    # the glued `#...` as a comment run that swallows the member and
    # its separator tail); all single cells AGREE (p1-p9 probe matrix:
    # `{?#}`, `{a?#}`, `{?#|#|}`, the `*L:` alias rider, the `20:`
    # colon-ride); fy 04e0b58 accepts the single cells but REJECTS the
    # composed floor and artifact in BOTH strict-1.2 and --yaml-1.1
    # passes (strict-1.2 "did not find expected node content" at the
    # `,?#` cell) — siding with NEITHER engine; the suite carries ZERO
    # glued rows at the da267a5c pin and the alias-charset axis stays
    # governed by 2SXE/W5VH/Y2GN (the r397/r398 no-basis precedent).
    # THE PINS: the 15-byte greedy-ddmin floor
    # `{?,*L:<LF>,?#,-<LF>0}` (every single-byte deletion breaks the
    # bar) + the 113-byte artifact are the live-DIFFER negative
    # controls waiver-neg-gluefold-qhash-r405{,-artifact}.bin (the
    # family-classify N-phase requires the waiver to decline them
    # forever — and NO other classifier family may claim them), and
    # the engines-agreed closed-map double-cell shape `{?#,?#}`
    # (fy-clean in both passes) pins green below. The SAME cell
    # composes to three engine-relations by tail: closed compositions
    # agree, the open-tail floor-head `{?,*L:<LF>,?#}` is
    # YEPTRIS-ONLY-ACCEPT (libyaml errors outright, fy sides with
    # yeptris), and the full break-tail floor is the live DIFFER. The
    # gluefold-qhash names stay
    # out of the snapshot with the pool; six breeding green firewall
    # controls (alias-member, backtick, bom, nested-seq,
    # paste-member-split, squote) join the exclusion by name; the
    # measured-clean riders (rootplain, mid-token-hash-prop-line,
    # block-nel-escless-fold-r294) stay in the snapshot.
    add_test(
        NAME fuzz-diff-r405-gluefold-qhash-closed
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/gluefold-qhash-closed-r405-pair.bin
    )




    add_executable(gen_emitter_golden port/libyaml/gen_emitter_golden.c)
    target_include_directories(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_emitter_golden)
endif()
