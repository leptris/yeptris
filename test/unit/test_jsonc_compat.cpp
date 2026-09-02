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

/* ---- building (TODO.impl/21 v2 over DOM mutation 11/3) ---- */

TEST(JsonCBuild, ObjectFromScratch) {
    json_object* root = json_object_new_object();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(json_object_object_add(root, "name", json_object_new_string("yeptris")), 0);
    ASSERT_EQ(json_object_object_add(root, "version", json_object_new_int(1)), 0);
    ASSERT_EQ(json_object_object_add(root, "fast", json_object_new_boolean(1)), 0);
    ASSERT_EQ(json_object_object_add(root, "ratio", json_object_new_double(2.5)), 0);
    ASSERT_EQ(json_object_object_add(root, "tags", json_object_new_array()), 0);

    json_object* tags = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "tags", &tags));
    ASSERT_EQ(json_object_array_add(tags, json_object_new_string("yaml")), 0);
    ASSERT_EQ(json_object_array_add(tags, json_object_new_int64(42)), 0);

    const char* out = json_object_to_json_string(root);
    ASSERT_NE(out, nullptr);
    EXPECT_STREQ(out,
                 R"({"name":"yeptris","version":1,"fast":true,"ratio":2.5,"tags":["yaml",42]})");

    /* read back through the json-c surface */
    json_object* v = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "name", &v));
    EXPECT_STREQ(json_object_get_string(v), "yeptris");
    ASSERT_TRUE(json_object_object_get_ex(root, "ratio", &v));
    EXPECT_DOUBLE_EQ(json_object_get_double(v), 2.5);
    ASSERT_TRUE(json_object_object_get_ex(root, "tags", &v));
    EXPECT_EQ(json_object_array_length(v), 2u);
    EXPECT_EQ(json_object_get_int(json_object_array_get_idx(v, 1)), 42);

    /* the emitted bytes reparse identically */
    json_object* back = json_tokener_parse(out);
    ASSERT_NE(back, nullptr);
    EXPECT_STREQ(json_object_to_json_string(back), out);
    json_object_put(back);
    json_object_put(root);
}

TEST(JsonCBuild, ReplaceDeleteAndNullLegacy) {
    json_object* root = json_object_new_object();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(json_object_object_add(root, "k", json_object_new_int(1)), 0);
    /* duplicate add returns 1 (replaced) and keeps position */
    EXPECT_EQ(json_object_object_add(root, "k", json_object_new_int(2)), 1);
    json_object* v = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "k", &v));
    EXPECT_EQ(json_object_get_int(v), 2);
    EXPECT_EQ(json_object_object_length(root), 1u);
    /* legacy: NULL val deletes the key */
    EXPECT_EQ(json_object_object_add(root, "k", nullptr), 0);
    EXPECT_FALSE(json_object_object_get_ex(root, "k", &v));
    EXPECT_EQ(json_object_object_length(root), 0u);
    EXPECT_NE(json_object_object_del(root, "k"), 0);
    json_object_put(root);
}

TEST(JsonCBuild, ArrayDelAndStandaloneScalars) {
    json_object* arr = json_object_new_array();
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 4; i++) {
        ASSERT_EQ(json_object_array_add(arr, json_object_new_int(i)), 0);
    }
    ASSERT_EQ(json_object_array_del_idx(arr, 1, 2), 0);
    EXPECT_EQ(json_object_array_length(arr), 2u);
    EXPECT_EQ(json_object_get_int(json_object_array_get_idx(arr, 1)), 3);
    EXPECT_NE(json_object_array_del_idx(arr, 1, 5), 0);
    EXPECT_STREQ(json_object_to_json_string(arr), "[0,3]");
    json_object_put(arr);

    /* a standalone scalar queried before any attach materializes its
     * own document */
    json_object* s = json_object_new_string_len("hi", 2);
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(json_object_is_type(s, json_type_string));
    EXPECT_STREQ(json_object_get_string(s), "hi");
    json_object_put(s);

    json_object* d = json_object_new_double(0.1);
    ASSERT_NE(d, nullptr);
    EXPECT_DOUBLE_EQ(json_object_get_double(d), 0.1);
    /* shortest round-trip text */
    EXPECT_STREQ(json_object_to_json_string(d), "0.1");
    json_object_put(d);
}

TEST(JsonCBuild, AttachAfterStandaloneQuery) {
    /* the rare path: a value materialized standalone (via a query)
     * then attached — duplicated into the parent, pointer stays valid */
    json_object* inner = json_object_new_object();
    ASSERT_NE(inner, nullptr);
    ASSERT_EQ(json_object_object_add(inner, "x", json_object_new_int(7)), 0);
    EXPECT_EQ(json_object_object_length(inner), 1u); /* query: materializes */

    json_object* root = json_object_new_object();
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(json_object_object_add(root, "inner", inner), 0);
    EXPECT_STREQ(json_object_to_json_string(root), R"({"inner":{"x":7}})");
    /* the caller's pointer is retargeted at the attached copy */
    EXPECT_EQ(json_object_object_length(inner), 1u);
    json_object* v = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(inner, "x", &v));
    EXPECT_EQ(json_object_get_int(v), 7);
    json_object_put(root); /* frees inner with it (json-c lifetime) */
}

TEST(JsonCBuild, MixedParseAndMutate) {
    /* mutation applies to PARSED documents too */
    json_object* root = json_tokener_parse(R"({"a":1,"b":[true]})");
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(json_object_object_add(root, "c", json_object_new_string("z")), 0);
    json_object* b = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "b", &b));
    ASSERT_EQ(json_object_array_add(b, json_object_new_double(1.5)), 0);
    EXPECT_STREQ(json_object_to_json_string(root), R"({"a":1,"b":[true,1.5],"c":"z"})");
    json_object_put(root);
}

TEST(JsonCBuild, PrettyOutput) {
    json_object* root = json_tokener_parse(R"({"a":1,"b":[true,null],"c":{"d":"x"}})");
    ASSERT_NE(root, nullptr);
    const char* out = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
    ASSERT_NE(out, nullptr);
    /* json-c's pretty layout: every entry on its own line, 2 spaces
     * per nesting level, arrays exploded too */
    EXPECT_STREQ(out, "{\n  \"a\": 1,\n  \"b\": [\n    true,\n    null\n  ],\n  \"c\": {\n"
                      "    \"d\": \"x\"\n  }\n}");
    /* empty containers stay compact, like json-c */
    json_object* empty = json_object_new_object();
    EXPECT_STREQ(json_object_to_json_string_ext(empty, JSON_C_TO_STRING_PRETTY), "{}");
    json_object_put(empty);
    json_object_put(root);
}

TEST(JsonCBuild, ArrayPutIdx) {
    json_object* arr = json_object_new_array();
    ASSERT_NE(arr, nullptr);
    for (int i = 0; i < 3; i++) {
        ASSERT_EQ(json_object_array_add(arr, json_object_new_int(i)), 0);
    }
    /* replace in place */
    ASSERT_EQ(json_object_array_put_idx(arr, 1, json_object_new_string("x")), 0);
    EXPECT_STREQ(json_object_to_json_string(arr), "[0,\"x\",2]");
    /* append at len */
    ASSERT_EQ(json_object_array_put_idx(arr, 3, json_object_new_int(9)), 0);
    EXPECT_STREQ(json_object_to_json_string(arr), "[0,\"x\",2,9]");
    /* beyond len errors; the caller keeps ownership of the value on
     * failure (json-c contract) — put it or the wrapper leaks */
    json_object* beyond = json_object_new_int(1);
    EXPECT_NE(json_object_array_put_idx(arr, 9, beyond), 0);
    json_object_put(beyond);
    json_object_put(arr);
}

/* ---- json-c suite breadth (ports of test_parse/test_basic classes) ---- */

TEST(JsonCCompat, IntegerEdges) {
    /* values from json-c's own parsing tests */
    struct {
        const char* in;
        int64_t want;
    } cases[] = {
        {"0", 0},
        {"-1", -1},
        {"2147483647", 2147483647},
        {"-2147483648", -2147483648},
        {"9223372036854775807", 9223372036854775807LL},
        {"-9223372036854775808", INT64_MIN},
        {"1e2", 100}, /* exponent form is a double; json-c truncates */
        {"-0", 0},
    };
    for (const auto& c : cases) {
        json_object* o = json_tokener_parse(c.in);
        ASSERT_NE(o, nullptr) << c.in;
        EXPECT_EQ(json_object_get_int64(o), c.want) << c.in;
        json_object_put(o);
    }
}

TEST(JsonCCompat, DoubleEdges) {
    struct {
        const char* in;
        double want;
    } cases[] = {
        {"0.0", 0.0},
        {"-1.5", -1.5},
        {"1e10", 1e10},
        {"1E-10", 1e-10},
        {"1.7976931348623157e308", 1.7976931348623157e308},
        {"4.9406564584124654e-324", 4.9406564584124654e-324},
    };
    for (const auto& c : cases) {
        json_object* o = json_tokener_parse(c.in);
        ASSERT_NE(o, nullptr) << c.in;
        EXPECT_DOUBLE_EQ(json_object_get_double(o), c.want) << c.in;
        json_object_put(o);
    }
}

TEST(JsonCCompat, NestedAccess) {
    json_object* root = json_tokener_parse(R"({"a":{"b":[{"c":1},{"c":2}]}})");
    ASSERT_NE(root, nullptr);
    json_object* a = nullptr;
    json_object* arr = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(root, "a", &a));
    ASSERT_TRUE(json_object_object_get_ex(a, "b", &arr));
    ASSERT_EQ(json_object_array_length(arr), 2u);
    json_object* second = json_object_array_get_idx(arr, 1);
    ASSERT_NE(second, nullptr);
    json_object* c = nullptr;
    ASSERT_TRUE(json_object_object_get_ex(second, "c", &c));
    EXPECT_EQ(json_object_get_int(c), 2);
    /* deep misses stay clean */
    EXPECT_FALSE(json_object_object_get_ex(root, "z", nullptr));
    EXPECT_FALSE(json_object_object_get_ex(a, "c", nullptr));
    json_object_put(root);
}

TEST(JsonCCompat, Utf8StringsRoundTrip) {
    const char* cases[] = {
        R"("héllo")",
        R"("日本語")",
        R"("emoji 👍 here")",
        R"("tab\tnewline\nquote\"backslash\\")",
    };
    for (const char* c : cases) {
        json_object* o = json_tokener_parse(c);
        ASSERT_NE(o, nullptr) << c;
        const char* back = json_object_to_json_string(o);
        ASSERT_NE(back, nullptr);
        EXPECT_STREQ(back, c) << c; /* escapes preserved verbatim */
        json_object_put(o);
    }
}

TEST(JsonCCompat, MutationOnParsedDocuments) {
    json_object* root = json_tokener_parse(R"({"keep":1,"drop":2})");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(json_object_object_del(root, "drop"), 0);
    EXPECT_EQ(json_object_object_length(root), 1u);
    EXPECT_EQ(json_object_object_add(root, "added", json_object_new_string("v")), 0);
    EXPECT_EQ(json_object_object_length(root), 2u);
    EXPECT_STREQ(json_object_to_json_string(root), R"({"keep":1,"added":"v"})");
    /* replace */
    EXPECT_EQ(json_object_object_add(root, "keep", json_object_new_int(9)), 1);
    EXPECT_STREQ(json_object_to_json_string(root), R"({"keep":9,"added":"v"})");
    json_object_put(root);
}

TEST(JsonCCompat, ArrayDelIdxOnParsed) {
    json_object* root = json_tokener_parse("[0,1,2,3,4]");
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(json_object_array_del_idx(root, 1, 2), 0); /* remove 1,2 */
    EXPECT_EQ(json_object_array_length(root), 3u);
    EXPECT_EQ(json_object_get_int(json_object_array_get_idx(root, 1)), 3);
    EXPECT_EQ(json_object_array_del_idx(root, 3, 5), -1); /* out of range */
    EXPECT_STREQ(json_object_to_json_string(root), "[0,3,4]");
    json_object_put(root);
}
