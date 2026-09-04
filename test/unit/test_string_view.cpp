/* test_string_view.cpp — YepView primitives (TODO.impl/02 Phase A). */

#include <gtest/gtest.h>

#include <cstring>

#include "common/nametab.h"
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

TEST(View, Hash) {
    /* The interner hash (nametab.h): deterministic, content-only,
     * prefix-sensitive, and slice-transparent. */
    EXPECT_EQ(yep_view_hash(v("abc")), yep_view_hash(v("abc")));
    EXPECT_NE(yep_view_hash(v("abc")), yep_view_hash(v("abd")));
    EXPECT_NE(yep_view_hash(v("ab")), yep_view_hash(v("abc")));
    EXPECT_NE(yep_view_hash(v("")), yep_view_hash(v("a")));
    EXPECT_EQ(yep_view_hash(v("abc")), yep_view_hash(yep_view_slice(v("xxabcxx"), 2, 3)));
    /* Length-class boundaries: 8/16 straddle the load paths. */
    EXPECT_EQ(yep_view_hash(v("12345678")), yep_view_hash(yep_view_slice(v("x12345678x"), 1, 8)));
    EXPECT_NE(yep_view_hash(v("12345678")), yep_view_hash(v("123456789")));
    EXPECT_EQ(yep_view_hash(v("1234567890123456")),
              yep_view_hash(yep_view_slice(v("y1234567890123456y"), 1, 16)));
    EXPECT_NE(yep_view_hash(v("1234567890123456")), yep_view_hash(v("12345678901234567")));
    EXPECT_NE(yep_view_hash(v("long key with many bytes past sixteen")),
              yep_view_hash(v("long key with many bytes past sevente")));
}
