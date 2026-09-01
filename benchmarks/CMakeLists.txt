# benchmarks — the matrix harness (TODO.impl/18).
#
# libyaml is the mandatory reference (the mission): point
# -DYEPTRIS_BENCH_LIBYAML_ROOT at a built checkout to include its
# columns; without it the harness still runs, ratios read n/a.

set(YEPTRIS_BENCH_LIBYAML_ROOT "" CACHE PATH "built libyaml checkout (with build/libyaml.a)")

add_executable(bench_matrix matrix/bench_matrix.cpp)
target_link_libraries(bench_matrix PRIVATE yeptris_static)
target_compile_definitions(bench_matrix PRIVATE
    YEP_BENCH_SNAPSHOT_DIR="${CMAKE_SOURCE_DIR}/test/port/libyaml/snapshots")
target_compile_features(bench_matrix PRIVATE cxx_std_11)

if(YEPTRIS_BENCH_LIBYAML_ROOT)
    target_compile_definitions(bench_matrix PRIVATE YEP_BENCH_LIBYAML=1)
    target_include_directories(bench_matrix PRIVATE ${YEPTRIS_BENCH_LIBYAML_ROOT}/include)
    if(EXISTS ${YEPTRIS_BENCH_LIBYAML_ROOT}/build/libyaml.a)
        target_link_libraries(bench_matrix PRIVATE ${YEPTRIS_BENCH_LIBYAML_ROOT}/build/libyaml.a)
    else()
        message(FATAL_ERROR "YEPTRIS_BENCH_LIBYAML_ROOT has no build/libyaml.a — build it first")
    endif()
endif()
