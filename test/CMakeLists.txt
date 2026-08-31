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
    unit/test_chartype.cpp
    unit/test_cpu.cpp
    unit/test_encoding.cpp
    unit/test_error.cpp
    unit/test_memory.cpp
    unit/test_parse.cpp
    unit/test_simd_text.cpp
    unit/test_string_view.cpp
)
target_link_libraries(test_unit PRIVATE yeptris_static GTest::gtest_main)
target_include_directories(test_unit PRIVATE ${CMAKE_SOURCE_DIR}/src/yeptris)
yeptris_set_warnings(test_unit)

include(GoogleTest)
gtest_discover_tests(test_unit)

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
endif()
