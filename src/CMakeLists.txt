# libyeptris — core library sources.
#
# Subsystems land with their TODO.impl items (common/ 02, memory/ 03,
# simd/ 04, encoding/ 05, scan/ 06, parse/ 07+). The SIMD per-ISA TUs get
# their own -m flags here when item 04 lands.

set(YEPTRIS_SOURCES
    yeptris/version.c
    yeptris/common/cpu.c
    yeptris/common/chartype.c
    yeptris/common/string_view.c
    yeptris/common/error.c
    yeptris/common/nametab.c
    yeptris/common/simd_text.c
    yeptris/common/simd_text_scalar.c
    yeptris/memory/allocator.c
    yeptris/memory/pool.c
    yeptris/memory/arena.c
    yeptris/encoding/bom.c
    yeptris/encoding/transcode.c
    yeptris/encoding/utf8_validate.c
    yeptris/scan/scan.c
    yeptris/parse/scalars.c
    yeptris/parse/engine.c
    yeptris/dom/dom.c
    yeptris/parse.c
    yeptris/events/capture.c
    yeptris/events/push.c
    yeptris/events/pull.c
    yeptris/events/recorder.c
    yeptris/events/iterparse.c
    yeptris/resolve/tags.c
    yeptris/resolve/core12.c
    yeptris/resolve/compat11.c
    yeptris/emit/style.c
    yeptris/emit/writer.c
    yeptris/emit/emit.c
)

# AOT SIMD TUs (TODO.impl/04): per-ISA compile flags; the TUs and the
# dispatch share architecture guards, so links resolve on every platform.
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|i[3-6]86)$" AND NOT MSVC)
    list(APPEND YEPTRIS_SOURCES yeptris/common/simd_text_avx2.c)
    set_source_files_properties(yeptris/common/simd_text_avx2.c
        PROPERTIES COMPILE_OPTIONS "-mavx2;-mpopcnt")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    list(APPEND YEPTRIS_SOURCES yeptris/common/simd_text_neon.c)
endif()

set(YEPTRIS_PUBLIC_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/api.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/dom.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/error.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/events.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/emit.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/resolve.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/parse.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/yeptris/types.h
)

set(YEPTRIS_INCLUDE_DIRS
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/generated>
)

if(YEPTRIS_BUILD_STATIC)
    add_library(yeptris_static STATIC ${YEPTRIS_SOURCES})
    add_library(yeptris::yeptris ALIAS yeptris_static)
    set_target_properties(yeptris_static PROPERTIES OUTPUT_NAME yeptris)
    target_include_directories(yeptris_static
        PUBLIC ${YEPTRIS_INCLUDE_DIRS}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/yeptris) # internal root-relative includes
    target_compile_definitions(yeptris_static PRIVATE YEPTRIS_BUILDING)
    yeptris_set_warnings(yeptris_static)
endif()

if(YEPTRIS_BUILD_SHARED)
    add_library(yeptris_shared SHARED ${YEPTRIS_SOURCES})
    set_target_properties(yeptris_shared
        PROPERTIES OUTPUT_NAME yeptris
                    SOVERSION ${PROJECT_VERSION_MAJOR})
    target_include_directories(yeptris_shared
        PUBLIC ${YEPTRIS_INCLUDE_DIRS}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/yeptris)
    target_compile_definitions(yeptris_shared PRIVATE YEPTRIS_BUILDING)
    yeptris_set_warnings(yeptris_shared)
endif()
