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

    add_executable(gen_emitter_golden port/libyaml/gen_emitter_golden.c)
    target_include_directories(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_emitter_golden)
endif()
