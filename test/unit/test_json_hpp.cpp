/* test_json_hpp.cpp — the C++ wrapper (TODO.impl/21). */

#include <gtest/gtest.h>

#include <yeptris/json.hpp>

using yeptris::json;
using yeptris::parse_error;
using yeptris::type_error;

TEST(JsonHpp, ParseQueryDump) {
    json j = json::parse(R"({"name": "yeptris", "n": 42, "list": [1, 2, 3]})");
    EXPECT_TRUE(j.is_object());
    EXPECT_EQ(j.size(), 3u);
    EXPECT_TRUE(j.contains("name"));
    EXPECT_FALSE(j.contains("absent"));
    EXPECT_EQ(j["name"].get_string(), "yeptris");
    EXPECT_TRUE(j["n"].is_number_integer());
    EXPECT_EQ(j["n"].get_int(), 42);
    EXPECT_TRUE(j["list"].is_array());
    EXPECT_EQ(j["list"].size(), 3u);
    EXPECT_EQ(j["list"][1].get_int(), 2);
    EXPECT_THROW(j["absent"], std::out_of_range);
    EXPECT_THROW(j["name"]["x"], type_error);
    std::string out = j.dump();
    EXPECT_FALSE(out.empty());
    json j2 = json::parse(out);
    EXPECT_EQ(j2["n"].get_int(), 42);
}

TEST(JsonHpp, TypesAndMoves) {
    json a = json::parse(R"({"b": true, "f": 0.5, "s": "x", "z": null})");
    EXPECT_TRUE(a["b"].is_boolean());
    EXPECT_TRUE(a["b"].get_bool());
    EXPECT_TRUE(a["f"].is_number_float());
    EXPECT_DOUBLE_EQ(a["f"].get_double(), 0.5);
    EXPECT_TRUE(a["s"].is_string());
    EXPECT_TRUE(a["z"].is_null());
    json b = std::move(a);
    EXPECT_EQ(b["s"].get_string(), "x");
    EXPECT_NO_THROW(b.dump());
}

TEST(JsonHpp, Errors) {
    EXPECT_THROW(json::parse("{bad}"), parse_error);
    EXPECT_THROW(json::parse(""), parse_error);
    json arr = json::parse("[1,2]");
    EXPECT_THROW(arr[5], std::out_of_range);
    EXPECT_THROW(arr["key"], type_error);
    /* json-c semantics: get_string returns the raw text of any scalar */
    EXPECT_EQ(arr[0].get_string(), "1");
}
