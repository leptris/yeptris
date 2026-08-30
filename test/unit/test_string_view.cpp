/* test_string_view.cpp — YepView primitives (TODO.impl/02 Phase A). */

#include <gtest/gtest.h>

#include <cstring>

#include "common/string_view.h"

static yep_view v(const char* s) {
    return yep_view_from_cstr(s);
}

TEST(View, Empty) {
    yep_view e = yep_view_empty();
    EXPECT_TRUE(yep_view_is_empty(e));
    EXPECT_EQ(e.len, 0u);
    EXPECT_TRUE(yep_view_eq(e, v("")));
}

TEST(View, Eq) {
    EXPECT_TRUE(yep_view_eq(v("a"), v("a")));
    EXPECT_TRUE(yep_view_eq(v(""), v("")));
    EXPECT_FALSE(yep_view_eq(v("a"), v("ab")));
    EXPECT_FALSE(yep_view_eq(v("ab"), v("ac")));
    /* Same bytes, different pointers — equality is by content. */
    char buf[] = "hello";
    yep_view a = {buf, 5};
    yep_view b = {"hello world", 5};
    EXPECT_TRUE(yep_view_eq(a, b));
    /* Embedded NULs are fine: views are length-authoritative. */
    yep_view n1 = {"a\0b", 3};
    yep_view n2 = {"a\0c", 3};
    EXPECT_FALSE(yep_view_eq(n1, n2));
}

TEST(View, EqCstrAndStartsWith) {
    EXPECT_TRUE(yep_view_eq_cstr(v("key"), "key"));
    EXPECT_TRUE(yep_view_eq_cstr(v(""), ""));
    EXPECT_FALSE(yep_view_eq_cstr(v("key"), "ke"));
    EXPECT_FALSE(yep_view_eq_cstr(v("ke"), "key"));
    /* Embedded NULs: from_cstr cannot carry them; construct explicitly. */
    yep_view nul = {"a\0b", 3};
    EXPECT_FALSE(yep_view_eq_cstr(nul, "a"));

    EXPECT_TRUE(yep_view_starts_with(v("--- doc"), "---"));
    EXPECT_TRUE(yep_view_starts_with(v("x"), "x"));
    EXPECT_TRUE(yep_view_starts_with(v("x"), ""));
    EXPECT_FALSE(yep_view_starts_with(v("-"), "---"));
}

TEST(View, Slice) {
    yep_view s = v("0123456789");
    EXPECT_TRUE(yep_view_eq(yep_view_slice(s, 0, 10), s));
    EXPECT_TRUE(yep_view_eq(yep_view_slice(s, 2, 3), v("234")));
    EXPECT_TRUE(yep_view_eq(yep_view_slice(s, 10, 0), v("")));
    EXPECT_TRUE(yep_view_is_empty(yep_view_slice(s, 11, 0)));
    EXPECT_TRUE(yep_view_is_empty(yep_view_slice(s, 8, 5)));
    EXPECT_TRUE(yep_view_eq(yep_view_slice(s, 8, 2), v("89")));
}

TEST(View, FnvHashes) {
    /* Known FNV-1a vectors. */
    EXPECT_EQ(yep_view_hash32(v("")), 2166136261u);
    EXPECT_EQ(yep_view_hash64(v("")), 14695981039346656037ull);
    EXPECT_EQ(yep_view_hash32(v("a")), 0xE40C292Cu);
    EXPECT_EQ(yep_view_hash32(v("foobar")), 0xBF9CF968u);

    /* Deterministic and prefix-sensitive. */
    EXPECT_EQ(yep_view_hash32(v("abc")), yep_view_hash32(v("abc")));
    EXPECT_NE(yep_view_hash32(v("abc")), yep_view_hash32(v("abd")));
    EXPECT_NE(yep_view_hash32(v("ab")), yep_view_hash32(v("abc")));
    /* Hash of a slice equals the hash of the same content elsewhere. */
    EXPECT_EQ(yep_view_hash32(v("abc")), yep_view_hash32(yep_view_slice(v("xxabcxx"), 2, 3)));
    EXPECT_EQ(yep_view_hash64(v("abc")), yep_view_hash64(yep_view_slice(v("xxabcxx"), 2, 3)));
}
