# test/ — Google Test binaries. One binary per suite directory as they grow
# (unit, conformance, port, flow, fuzz, concurrency — TODO.impl/01/16/17/19).

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
    unit/test_emit.cpp
    unit/test_float.cpp
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
    unit/test_mutate.cpp
    unit/test_mapindex.cpp
    unit/test_limits.cpp
    unit/test_yaml_compat.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(test_unit PRIVATE yeptris_static Threads::Threads
    $<TARGET_NAME_IF_EXISTS:yeptris_jsonc> $<TARGET_NAME_IF_EXISTS:yeptris_yajl> GTest::gtest_main)
target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(test_unit)

include(GoogleTest)
gtest_discover_tests(test_unit)

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

add_executable(fuzz_transcode fuzz/fuzz_transcode.c)
target_link_libraries(fuzz_transcode PRIVATE yeptris_static)
target_include_directories(fuzz_transcode PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
target_compile_definitions(fuzz_transcode PRIVATE YEP_FUZZ_STANDALONE)
yeptris_set_warnings(fuzz_transcode)
yeptris_set_warnings(fuzz_roundtrip)

add_test(
    NAME fuzz-corpus
    COMMAND fuzz_parse ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
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
    NAME fuzz-transcode-corpus
    COMMAND fuzz_transcode ${CMAKE_CURRENT_SOURCE_DIR}/port/libyaml/snapshots
)

if(YEPTRIS_ENABLE_FUZZING)
    add_executable(fuzz_parse_lf fuzz/fuzz_parse.c)
    target_link_libraries(fuzz_parse_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_parse_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_parse_lf)

    add_executable(fuzz_roundtrip_lf fuzz/fuzz_roundtrip.c)
    target_link_libraries(fuzz_roundtrip_lf PRIVATE yeptris_static)
    yeptris_set_warnings(fuzz_roundtrip_lf)

    add_executable(fuzz_feed_lf fuzz/fuzz_feed.c)
    target_link_libraries(fuzz_feed_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_feed_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_feed_lf)

    add_executable(fuzz_transcode_lf fuzz/fuzz_transcode.c)
    target_link_libraries(fuzz_transcode_lf PRIVATE yeptris_static)
    target_include_directories(fuzz_transcode_lf PRIVATE ${CMAKE_SOURCE_DIR}/src/include)
    yeptris_set_warnings(fuzz_transcode_lf)

    if(YEPTRIS_LIBYAML_ROOT)
        # differential under fuzzing: libyaml linked into the harness
        add_executable(fuzz_diff_lf fuzz/fuzz_diff.c)
        target_link_libraries(fuzz_diff_lf PRIVATE diff_core)
        yeptris_set_warnings(fuzz_diff_lf)
    endif()
endif()

# ---- JSONTestSuite under YAML semantics (TODO.impl/08C) ---------------
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

    add_executable(gen_emitter_golden port/libyaml/gen_emitter_golden.c)
    target_include_directories(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/include)
    target_link_libraries(gen_emitter_golden PRIVATE ${YEPTRIS_LIBYAML_ROOT}/build/libyaml.a)
    yeptris_set_warnings(gen_emitter_golden)
endif()
