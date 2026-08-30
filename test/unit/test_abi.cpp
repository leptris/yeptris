/* test_abi.cpp — ABI pinning (TODO.impl/01).
 *
 * Bindings hard-code handle sizes and enum values; these assertions make
 * an accidental ABI change fail the build/run instead of silently breaking
 * a binding. Changing a pinned value requires a major version bump
 * (docs/ABI.md, TODO.impl/20).
 */

#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include <yeptris.h>

/* ---- Handle sizes: opaque, pointer-sized — by construction and by test. */
static_assert(sizeof(YeptrisDocument) == sizeof(void*), "YeptrisDocument must be pointer-sized");
static_assert(sizeof(YeptrisNode) == sizeof(void*), "YeptrisNode must be pointer-sized");
static_assert(sizeof(YeptrisParser) == sizeof(void*), "YeptrisParser must be pointer-sized");
static_assert(sizeof(YeptrisPullParser) == sizeof(void*),
              "YeptrisPullParser must be pointer-sized");
static_assert(sizeof(YeptrisRecorder) == sizeof(void*), "YeptrisRecorder must be pointer-sized");
static_assert(sizeof(YeptrisIterparse) == sizeof(void*), "YeptrisIterparse must be pointer-sized");
static_assert(sizeof(YeptrisEmitter) == sizeof(void*), "YeptrisEmitter must be pointer-sized");

/* ---- Status enum values: pinned, stable, 0-based, contiguous. */
static_assert(YEPTRIS_OK == 0, "pinned ABI value");
static_assert(YEPTRIS_ERROR_PARSE == 1, "pinned ABI value");
static_assert(YEPTRIS_ERROR_MEMORY == 2, "pinned ABI value");
static_assert(YEPTRIS_ERROR_DEPTH == 3, "pinned ABI value");
static_assert(YEPTRIS_ERROR_ENCODING == 4, "pinned ABI value");
static_assert(YEPTRIS_ERROR_IO == 5, "pinned ABI value");
static_assert(YEPTRIS_ERROR_ARG == 6, "pinned ABI value");
static_assert(YEPTRIS_ERROR_UNSUPPORTED == 7, "pinned ABI value");
static_assert(YEPTRIS_ERROR_INTERNAL == 8, "pinned ABI value");

TEST(Abi, VersionStringMatchesMacros) {
    const char* v = yeptris_version();
    ASSERT_NE(v, nullptr);
    EXPECT_STREQ(v, YEPTRIS_VERSION_STRING);

    char expected[32];
    snprintf(expected, sizeof(expected), "%d.%d.%d", YEPTRIS_VERSION_MAJOR, YEPTRIS_VERSION_MINOR,
             YEPTRIS_VERSION_PATCH);
    EXPECT_STREQ(v, expected);
}

TEST(Abi, VersionIsStableAcrossCalls) {
    const char* a = yeptris_version();
    const char* b = yeptris_version();
    EXPECT_EQ(a, b) << "version() must return static storage";
}
