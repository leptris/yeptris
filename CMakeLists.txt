# yeptris — Ultra-fast YAML 1.2 parser, emitter and streamer in C
# The YAML counterpart of libleptris. Pure C library with CLI and bindings.
cmake_minimum_required(VERSION 3.20)

project(yeptris
    VERSION 0.1.0
    DESCRIPTION "Ultra-fast YAML 1.2 parser, emitter and streamer in C"
    LANGUAGES C CXX
)

# C11 (not C99): _Static_assert is not a keyword in MSVC's C99 mode; C11
# makes it standard (VS2019 16.8+). Same rationale as libleptris.
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# C++11 standard for tests and benchmarks.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------------- options --
option(BUILD_TESTING "Build tests" ON)
option(YEPTRIS_BUILD_CLI "Build the yeptris CLI tool" ON)
option(YEPTRIS_BUILD_STATIC "Build the static library" ON)
option(YEPTRIS_BUILD_SHARED "Build the shared library" OFF)
option(YEPTRIS_BUILD_BENCHMARKS "Build performance benchmarks (TODO.impl/18)" OFF)
option(YEPTRIS_ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(YEPTRIS_ENABLE_TSAN "Enable ThreadSanitizer (requires all-TSAN build)" OFF)
option(YEPTRIS_ENABLE_FUZZING "Build libFuzzer harnesses (TODO.impl/19; requires clang)" OFF)
option(YEPTRIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# LTO: ON for Release/RelWithDebInfo by default (libleptris measured
# 1.3-3.5x on parse paths); override with -DYEPTRIS_ENABLE_LTO=OFF.
if(NOT DEFINED YEPTRIS_ENABLE_LTO)
    if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(YEPTRIS_ENABLE_LTO_DEFAULT ON)
    else()
        set(YEPTRIS_ENABLE_LTO_DEFAULT OFF)
    endif()
else()
    set(YEPTRIS_ENABLE_LTO_DEFAULT ${YEPTRIS_ENABLE_LTO})
endif()
option(YEPTRIS_ENABLE_LTO "Enable link-time optimization (default ON for Release)" ${YEPTRIS_ENABLE_LTO_DEFAULT})

# ------------------------------------------------------------- sanitizers --
if(YEPTRIS_ENABLE_ASAN AND YEPTRIS_ENABLE_TSAN)
    message(FATAL_ERROR "ASAN and TSAN are mutually exclusive")
endif()

if(YEPTRIS_ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address)
    message(STATUS "AddressSanitizer: ENABLED")
endif()

if(YEPTRIS_ENABLE_TSAN)
    add_compile_options(-fsanitize=thread -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=thread)
    message(STATUS "ThreadSanitizer: ENABLED")
endif()

# libFuzzer implies ASAN (libleptris TODO 40 pattern).
if(YEPTRIS_ENABLE_FUZZING)
    add_compile_options(-fsanitize=fuzzer-no-link,address -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=fuzzer,address)
    message(STATUS "libFuzzer: ENABLED (implies ASAN)")
endif()

# -------------------------------------------------------------------- LTO --
if(YEPTRIS_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT yeptris_lto_supported OUTPUT yeptris_lto_error)
    if(yeptris_lto_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "Link-time optimization: ENABLED")
    else()
        message(WARNING "YEPTRIS_ENABLE_LTO requested but not supported: ${yeptris_lto_error}")
    endif()
endif()

# -------------------------------------------------------------- visibility --
# Position-independent code for static libraries (embeddable in DSOs).
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Hide all symbols by default; the public surface is exactly the
# YEPTRIS_API-marked functions (libleptris TODO 80 pattern).
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------- warnings --
# Scoped per-target so fetched dependencies (GTest) keep their own flags.
function(yeptris_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        if(YEPTRIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
        if(YEPTRIS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ------------------------------------------------------------- versioning --
# CMakeLists project(VERSION) is the single source of truth (TODO.impl/01);
# scripts/bump-version.sh syncs vcpkg.json and CHANGELOG.md from it.
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include/yeptris/version.h.in
    ${CMAKE_BINARY_DIR}/generated/yeptris/version.h
    @ONLY
)

# --------------------------------------------------------------- subdirs --
enable_testing()

add_subdirectory(src)

if(YEPTRIS_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(BUILD_TESTING)
    add_subdirectory(test)
endif()

if(YEPTRIS_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
