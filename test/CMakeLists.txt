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
        NAME fuzz-diff-r261-nel-fold
        COMMAND fuzz_diff
                ${CMAKE_CURRENT_SOURCE_DIR}/fuzz/corpus/parse/nel-fold-r261.bin
    )

    add_executable(gen_emitter_golden port/libyaml/gen_emitter_golden.c)
    target_include_directories(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_emitter_golden)
endif()
