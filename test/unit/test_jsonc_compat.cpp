/* test_jsonc_compat.cpp — the json-c drop-in layer (TODO.impl/21).
 * Written exactly as a json-c user would write it: only json-c names.
 */

#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include <yeptris/jsonc_compat.h>
}

TEST(JsonCCompat, ParseAndQuery) {
    json_object* root = json_tokener_parse(
        "{\"name\": \"yeptris\", \"n\": 42, \"pi\": 3.5, \"ok\": true, \"none\": null, "
        "\"list\": [1, \"two\", 3.0], \"inner\": {\"x\": 1}}");
    ASSERT_NE(root, nullptr);

    EXPECT_TRUE(json_object_is_type(root, json_type_object));
    EXPECT_EQ(json_object_object_length(root), 7u);

    json_object* name = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "name", &name));
    EXPECT_TRUE(json_object_is_type(name, json_type_string));
    EXPECT_STREQ(json_object_get_string(name), "yeptris");

    json_object* n = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "n", &n));
    EXPECT_TRUE(json_object_is_type(n, json_type_int));
    EXPECT_EQ(json_object_get_int(n), 42);
    EXPECT_EQ(json_object_get_int64(n), 42);

    json_object* pi = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "pi", &pi));
    EXPECT_TRUE(json_object_is_type(pi, json_type_double));
    EXPECT_DOUBLE_EQ(json_object_get_double(pi), 3.5);

    json_object* ok = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "ok", &ok));
    EXPECT_TRUE(json_object_is_type(ok, json_type_boolean));
    EXPECT_EQ(json_object_get_boolean(ok), 1);

    json_object* none = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "none", &none));
    EXPECT_TRUE(json_object_is_type(none, json_type_null));

    json_object* list = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "list", &list));
    EXPECT_TRUE(json_object_is_type(list, json_type_array));
    EXPECT_EQ(json_object_array_length(list), 3u);
    json_object* e0 = json_object_array_get_idx(list, 0);
    ASSERT_NE(e0, nullptr);
    EXPECT_EQ(json_object_get_int(e0), 1);
    json_object* e1 = json_object_array_get_idx(list, 1);
    EXPECT_STREQ(json_object_get_string(e1), "two");
    json_object* e2 = json_object_array_get_idx(list, 2);
    EXPECT_TRUE(json_object_is_type(e2, json_type_double));
    EXPECT_EQ(json_object_array_get_idx(list, 99), nullptr);

    json_object* missing = (json_object*)0x1;
    EXPECT_FALSE(json_object_object_get_ex(root, "absent", &missing));
    EXPECT_EQ(missing, nullptr);

    EXPECT_EQ(json_object_put(root), 1);
}

TEST(JsonCCompat, StrictRejects) {
    EXPECT_EQ(json_tokener_parse("{bad}"), nullptr);
    EXPECT_EQ(json_tokener_parse("[1,]"), nullptr);
    EXPECT_EQ(json_tokener_parse(""), nullptr);
    EXPECT_EQ(json_tokener_parse(nullptr), nullptr);
}

TEST(JsonCCompat, QuotedNumbersAreStrings) {
    json_object* root = json_tokener_parse("{\"a\": \"12\", \"b\": 12}");
    ASSERT_NE(root, nullptr);
    json_object* a = nullptr;
    json_object* b = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "a", &a));
    ASSERT_TRUE(json_object_object_get_ex(root, "b", &b));
    EXPECT_TRUE(json_object_is_type(a, json_type_string));
    EXPECT_TRUE(json_object_is_type(b, json_type_int));
    json_object_put(root);
}

TEST(JsonCCompat, ToJsonStringRoundTrip) {
    json_object* root = json_tokener_parse("{\"z\": 1, \"a\": [true, null, 0.5]}");
    ASSERT_NE(root, nullptr);
    const char* out = json_object_to_json_string(root);
    ASSERT_NE(out, nullptr) << "to_json_string returned null";
    /* the output is itself strict JSON with the same content */
    json_object* reparsed = json_tokener_parse(out);
    ASSERT_NE(reparsed, nullptr) << "reparse of [" << out << "]";
    json_object *z1 = nullptr, *z2 = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "z", &z1));
    ASSERT_TRUE(json_object_object_get_ex(reparsed, "z", &z2));
    EXPECT_EQ(json_object_get_int(z1), json_object_get_int(z2));
    json_object_put(reparsed);
    json_object_put(root);
}

TEST(JsonCCompat, ArrayRoot) {
    json_object* root = json_tokener_parse("[1, 2, 3]");
    ASSERT_NE(root, nullptr);
    EXPECT_TRUE(json_object_is_type(root, json_type_array));
    EXPECT_EQ(json_object_array_length(root), 3u);
    EXPECT_EQ(json_object_get_int(json_object_array_get_idx(root, 2)), 3);
    json_object_put(root);
}
