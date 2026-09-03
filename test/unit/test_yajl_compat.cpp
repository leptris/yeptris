/* test_yajl_compat.cpp — the yajl generator drop-in (TODO.impl/21).
 *
 * yajl's contract: call-order state machine (maps alternate
 * string-key/value; keys must be strings; nothing after the root
 * closes until reset), get_buf returns the JSON (compact default,
 * beautify pretty), and the buffer survives until the next
 * generating call. */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

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

/* ---- SAX parser ------------------------------------------------------- */

namespace {

/* every callback routes through tick: uniform log ("what;" or
 * "name:value;") and a cancel point at cancel_at */
struct SaxLog {
    std::string ev;
    int cancel_at = -1;
    int calls = 0;
};

int tick(SaxLog* s, const std::string& what) {
    s->ev += what;
    int idx = s->calls++;
    return idx != s->cancel_at;
}

int cb_null(void* ctx) {
    return tick((SaxLog*)ctx, "null;");
}
int cb_boolean(void* ctx, int b) {
    return tick((SaxLog*)ctx, b ? "bool:1;" : "bool:0;");
}
int cb_integer(void* ctx, long long v) {
    return tick((SaxLog*)ctx, "int:" + std::to_string(v) + ";");
}
int cb_double(void* ctx, double d) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "dbl:%g;", d);
    return tick((SaxLog*)ctx, tmp);
}
int cb_number(void* ctx, const char* n, size_t len) {
    return tick((SaxLog*)ctx, "num:" + std::string(n, len) + ";");
}
int cb_string(void* ctx, const unsigned char* str, size_t len) {
    return tick((SaxLog*)ctx, "str:" + std::string((const char*)str, len) + ";");
}
int cb_start_map(void* ctx) {
    return tick((SaxLog*)ctx, "{;");
}
int cb_map_key(void* ctx, const unsigned char* k, size_t len) {
    return tick((SaxLog*)ctx, "key:" + std::string((const char*)k, len) + ";");
}
int cb_end_map(void* ctx) {
    return tick((SaxLog*)ctx, "};");
}
int cb_start_array(void* ctx) {
    return tick((SaxLog*)ctx, "[;");
}
int cb_end_array(void* ctx) {
    return tick((SaxLog*)ctx, "];");
}

void full_cbs(yajl_callbacks* c) {
    memset(c, 0, sizeof(*c));
    c->yajl_null = cb_null;
    c->yajl_boolean = cb_boolean;
    c->yajl_integer = cb_integer;
    c->yajl_double = cb_double;
    c->yajl_string = cb_string;
    c->yajl_start_map = cb_start_map;
    c->yajl_map_key = cb_map_key;
    c->yajl_end_map = cb_end_map;
    c->yajl_start_array = cb_start_array;
    c->yajl_end_array = cb_end_array;
}

yajl_status run_sax(const char* json, size_t len, SaxLog* log, yajl_callbacks* cbs) {
    yajl_callbacks c;
    full_cbs(&c);
    if (cbs != NULL) {
        c = *cbs;
    }
    yajl_handle h = yajl_alloc(&c, NULL, log);
    if (h == NULL) {
        return yajl_status_error;
    }
    yajl_status st = yajl_parse(h, (const unsigned char*)json, len);
    if (st == yajl_status_ok) {
        st = yajl_complete_parse(h);
    }
    yajl_free(h);
    return st;
}

} // namespace

TEST(YajlSax, NestedEventOrder) {
    SaxLog log;
    const char* j = "{\"a\": [1, {\"b\": null}], \"c\": \"x\"}";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_ok);
    EXPECT_EQ(log.ev, "{;key:a;[;int:1;{;key:b;null;};];key:c;str:x;};");
}

TEST(YajlSax, ScalarTypes) {
    SaxLog log;
    const char* j = "[true, false, null, \"s\", 42, -7, 2.5, 1e2]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_ok);
    EXPECT_EQ(log.ev, "[;bool:1;bool:0;null;str:s;int:42;int:-7;dbl:2.5;dbl:100;];");
}

TEST(YajlSax, QuotedNumberIsString) {
    SaxLog log;
    const char* j = "[\"12\", 12]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_ok);
    EXPECT_EQ(log.ev, "[;str:12;int:12;];");
}

TEST(YajlSax, NumberCallbackPrecedence) {
    SaxLog log;
    yajl_callbacks c;
    full_cbs(&c);
    c.yajl_number = cb_number;
    const char* j = "[1, 2.5, 1e400]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, &c), yajl_status_ok);
    /* the number callback carries every number as text — even one
     * that overflows both long long and double */
    EXPECT_EQ(log.ev, "[;num:1;num:2.5;num:1e400;];");
}

TEST(YajlSax, OversizedNumberErrorsWithoutNumberCallback) {
    SaxLog log;
    const char* j = "[1e400]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_error);
    unsigned char* err = NULL;
    yajl_handle h = yajl_alloc(NULL, NULL, NULL);
    ASSERT_NE(h, nullptr);
    ASSERT_EQ(yajl_parse(h, (const unsigned char*)j, strlen(j)), yajl_status_ok);
    ASSERT_EQ(yajl_complete_parse(h), yajl_status_error);
    err = yajl_get_error(h, 0, NULL, 0);
    ASSERT_NE(err, nullptr);
    EXPECT_GT(strlen((char*)err), 0u);
    yajl_free_error(h, err);
    err = yajl_get_error(h, 1, (const unsigned char*)j, strlen(j));
    ASSERT_NE(err, nullptr);
    EXPECT_NE(strchr((char*)err, '^'), nullptr) << (char*)err;
    yajl_free_error(h, err);
    yajl_free(h);
}

TEST(YajlSax, ClientCancel) {
    SaxLog log;
    log.cancel_at = 3; /* the 4th callback bails */
    const char* j = "[1, 2, 3, 4, 5]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_client_canceled);
    EXPECT_EQ(log.ev, "[;int:1;int:2;int:3;");
}

TEST(YajlSax, ChunkedFeeds) {
    SaxLog log;
    const char* j = "{\"key\": [1, 2, 3], \"done\": true}";
    yajl_callbacks c;
    full_cbs(&c);
    yajl_handle h = yajl_alloc(&c, NULL, &log);
    ASSERT_NE(h, nullptr);
    for (size_t i = 0; i < strlen(j); i++) {
        ASSERT_EQ(yajl_parse(h, (const unsigned char*)j + i, 1), yajl_status_ok);
    }
    ASSERT_EQ(yajl_complete_parse(h), yajl_status_ok);
    EXPECT_EQ(log.ev, "{;key:key;[;int:1;int:2;int:3;];key:done;bool:1;};");
    EXPECT_EQ(yajl_get_bytes_consumed(h), strlen(j));
    yajl_free(h);
}

TEST(YajlSax, StrictRejections) {
    const char* bad[] = {
        "{a: 1}",       /* unquoted key */
        "{'a': 1}",     /* single quotes */
        "[1, 2,]",      /* trailing comma */
        "[1] [2]",      /* two values */
        "[1] trailing", /* trailing garbage */
        "{\"a\" 1}",    /* missing colon */
        "[01]",         /* leading zero */
        "[+1]",         /* plus sign */
        "[.5]",         /* bare fraction */
        "[NaN]",        /* not a word */
        "",             /* empty: premature EOF */
    };
    for (const char* j : bad) {
        SaxLog log;
        EXPECT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_error) << j;
    }
}

static yajl_status run_comments(SaxLog* log, const char* j) {
    yajl_callbacks c;
    full_cbs(&c);
    yajl_handle h = yajl_alloc(&c, NULL, log);
    if (h == NULL) {
        return yajl_status_error;
    }
    EXPECT_EQ(yajl_config(h, yajl_allow_comments, 1), 1);
    yajl_status st = yajl_parse(h, (const unsigned char*)j, strlen(j));
    if (st == yajl_status_ok) {
        st = yajl_complete_parse(h);
    }
    yajl_free(h);
    return st;
}

TEST(YajlSax, AllowComments) {
    SaxLog log;
    ASSERT_EQ(run_comments(&log, "[1, /* two */ 2]"), yajl_status_ok);
    EXPECT_EQ(log.ev, "[;int:1;int:2;];");
}

TEST(YajlSax, AllowCommentsLineAndEdges) {
    SaxLog log;
    ASSERT_EQ(run_comments(&log, "[1,\n// c\n2]"), yajl_status_ok);
    EXPECT_EQ(log.ev, "[;int:1;int:2;];");

    /* slashes inside strings are content */
    SaxLog log2;
    ASSERT_EQ(run_comments(&log2, "{\"u\": \"http://x//y/*z*\"}"), yajl_status_ok);
    EXPECT_EQ(log2.ev, "{;key:u;str:http://x//y/*z*;};");

    /* comments still off by default: strict rejects the block form */
    SaxLog log3;
    ASSERT_EQ(run_sax("[1, /* c */ 2]", strlen("[1, /* c */ 2]"), &log3, NULL), yajl_status_error);
}

TEST(YajlSax, ConfigContract) {
    yajl_handle h = yajl_alloc(NULL, NULL, NULL);
    ASSERT_NE(h, nullptr);
    /* validation-only parse: NULL callbacks, pure grammar check */
    EXPECT_EQ(yajl_config(h, yajl_dont_validate_strings, 1), 1); /* no-op ok */
    EXPECT_EQ(yajl_config(h, yajl_allow_trailing_garbage, 1), 0);
    EXPECT_EQ(yajl_config(h, yajl_allow_multiple_values, 1), 0);
    EXPECT_EQ(yajl_config(h, yajl_allow_partial_values, 1), 0);
    const char* j = "{\"x\": [1]}";
    EXPECT_EQ(yajl_parse(h, (const unsigned char*)j, strlen(j)), yajl_status_ok);
    EXPECT_EQ(yajl_complete_parse(h), yajl_status_ok);
    /* completed handles park: further parse is a no-op ok */
    EXPECT_EQ(yajl_parse(h, (const unsigned char*)"junk", 4), yajl_status_ok);
    EXPECT_EQ(yajl_complete_parse(h), yajl_status_ok);
    yajl_free(h);
}

TEST(YajlSax, ErrorSticks) {
    const char* j = "{a: 1}";
    yajl_handle h = yajl_alloc(NULL, NULL, NULL);
    ASSERT_NE(h, nullptr);
    ASSERT_EQ(yajl_parse(h, (const unsigned char*)j, strlen(j)), yajl_status_ok);
    ASSERT_EQ(yajl_complete_parse(h), yajl_status_error);
    /* the handle stays in the error state */
    EXPECT_EQ(yajl_parse(h, (const unsigned char*)"[1]", 3), yajl_status_error);
    EXPECT_EQ(yajl_complete_parse(h), yajl_status_error);
    unsigned char* err = yajl_get_error(h, 0, NULL, 0);
    EXPECT_NE(err, nullptr);
    yajl_free_error(h, err);
    yajl_free(h);
}

TEST(YajlSax, StatusToString) {
    EXPECT_STREQ(yajl_status_to_string(yajl_status_ok), "ok, no error");
    EXPECT_STREQ(yajl_status_to_string(yajl_status_client_canceled), "client canceled parse");
    EXPECT_NE(yajl_status_to_string(yajl_status_error), nullptr);
}

TEST(YajlSax, UnicodeEscapesAndRawUTF8) {
    SaxLog log;
    const char* j = "[\"\\u00e9\", \"caf\xc3\xa9\"]";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_ok);
    EXPECT_EQ(log.ev, "[;str:\xc3\xa9;str:caf\xc3\xa9;];");
}

TEST(YajlSax, InvalidUTF8Rejected) {
    SaxLog log;
    const char* j = "\"\xff\xfe\"";
    ASSERT_EQ(run_sax(j, strlen(j), &log, NULL), yajl_status_error);
}

TEST(YajlSax, DeepNesting) {
    /* 200-deep arrays: the walk recurses within the parse depth cap */
    std::string j;
    for (int i = 0; i < 200; i++) {
        j += "[";
    }
    j += "1";
    for (int i = 0; i < 200; i++) {
        j += "]";
    }
    SaxLog log;
    ASSERT_EQ(run_sax(j.data(), j.size(), &log, NULL), yajl_status_ok);
    EXPECT_NE(log.ev.find("int:1"), std::string::npos);
}
