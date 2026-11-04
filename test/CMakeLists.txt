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
        add_executable(fuzz_diff_lf fuzz/fuzz_diff.c)
        target_link_libraries(fuzz_diff_lf PRIVATE diff_core)
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

    add_executable(fuzz_diff fuzz/fuzz_diff.c)
    target_include_directories(fuzz_diff PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(fuzz_diff PRIVATE diff_core)
    target_compile_definitions(fuzz_diff PRIVATE YEP_FUZZ_STANDALONE)
    yeptris_set_warnings(fuzz_diff)

    add_test(
        NAME fuzz-diff-corpus
        COMMAND fuzz_diff
                --ledger=${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/ledger.txt
                ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
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

    add_executable(gen_emitter_golden port/libyaml/gen_emitter_golden.c)
    target_include_directories(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_emitter_golden)
endif()
