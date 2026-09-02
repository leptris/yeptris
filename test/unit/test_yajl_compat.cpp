/* test_yajl_compat.cpp — the yajl generator drop-in (TODO.impl/21).
 *
 * yajl's contract: call-order state machine (maps alternate
 * string-key/value; keys must be strings; nothing after the root
 * closes until reset), get_buf returns the JSON (compact default,
 * beautify pretty), and the buffer survives until the next
 * generating call. */

#include <gtest/gtest.h>

#include <cstring>

#include <yeptris/yajl_compat.h>

namespace {

std::string buf(yajl_gen g) {
    size_t len = 0;
    const unsigned char* b = yajl_gen_get_buf(g, &len);
    return std::string(b ? (const char*)b : "", len);
}

} // namespace

TEST(YajlGen, SimpleObject) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_map_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"name", 4), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"yeptris", 7), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"answer", 6), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_integer(g, 42), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_close(g), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), R"({"name":"yeptris","answer":42})");
    yajl_gen_free(g);
}

TEST(YajlGen, NestedArraysAndScalars) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_array_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_integer(g, 1), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_double(g, 2.5), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_bool(g, 1), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_bool(g, 0), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_null(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"inner", 5), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_close(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_close(g), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), R"([1,2.5,true,false,null,["inner"]])");
    yajl_gen_free(g);
}

TEST(YajlGen, Beautify) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_config(g, yajl_gen_beautify, 1), 1);
    ASSERT_EQ(yajl_gen_map_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"a", 1), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_integer(g, 1), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_integer(g, 2), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_close(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_close(g), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), "{\n  \"a\": [\n    1,\n    2\n  ]\n}");
    yajl_gen_free(g);
}

TEST(YajlGen, RawNumber) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_array_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_number(g, "1.5e+3", 6), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_close(g), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), "[1500.0]"); /* typed through the resolver */
    yajl_gen_free(g);
}

TEST(YajlGen, StateMachineErrors) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    /* keys must be strings */
    ASSERT_EQ(yajl_gen_map_open(g), yajl_gen_status_ok);
    EXPECT_EQ(yajl_gen_integer(g, 3), yajl_gen_keys_must_be_strings);
    /* close on an open map with a pending key is yajl's error */
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"k", 1), yajl_gen_status_ok);
    /* a dangling key at close is yajl's incomplete-pair error */
    EXPECT_EQ(yajl_gen_map_close(g), yajl_gen_keys_must_be_strings);
    /* complete the pair properly */
    ASSERT_EQ(yajl_gen_null(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_close(g), yajl_gen_status_ok);
    /* nothing after the root closes */
    EXPECT_EQ(yajl_gen_integer(g, 9), yajl_gen_generation_complete);
    EXPECT_EQ(buf(g), R"({"k":null})");

    /* reset restores a working generator */
    yajl_gen_reset(g, NULL);
    ASSERT_EQ(yajl_gen_integer(g, 7), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), "7");

    /* mismatched close */
    EXPECT_EQ(yajl_gen_array_close(g), yajl_gen_in_error_state);
    yajl_gen_free(g);
}

TEST(YajlGen, KeyOrderAndEmptyContainers) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_map_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"empty_map", 9), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_close(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"empty_seq", 9), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_open(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_array_close(g), yajl_gen_status_ok);
    ASSERT_EQ(yajl_gen_map_close(g), yajl_gen_status_ok);
    EXPECT_EQ(buf(g), R"({"empty_map":{},"empty_seq":[]})");
    yajl_gen_free(g);
}

TEST(YajlGen, EscapedStrings) {
    yajl_gen g = yajl_gen_alloc(NULL);
    ASSERT_NE(g, nullptr);
    ASSERT_EQ(yajl_gen_string(g, (const unsigned char*)"a\"b\\c\n", 6), yajl_gen_status_ok);
    /* the newline is the two-character \n escape, never a raw break */
    EXPECT_EQ(buf(g), "\"a\\\"b\\\\c\\n\"");
    yajl_gen_free(g);
}
