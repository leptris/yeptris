// test_tape.cpp — the compact JSON tape (TODO.restructure/85). Pins:
// record kinds and OPEN/CLOSE links, zero-copy spans, inline number
// conversion (int64 exact, beyond-int64 flagged), scalar roots,
// strict-JSON rejects, error parity with yeptris_parse_json, and the
// free/reuse contract.
#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include <yeptris.h>
#include <yeptris/json.h>
#include <yeptris/tape.h>

namespace {

struct TapeGuard {
    yeptris_json_tape t{};

    ~TapeGuard() {
        yeptris_tape_free(&t);
    }

    YeptrisStatus parse(const char* s) {
        return yeptris_parse_json_tape(s, strlen(s), &t);
    }
};

double val_double(const yeptris_json_tape& t, size_t i) {
    double d = 0;
    memcpy(&d, &t.vals[i], sizeof(d));
    return d;
}

TEST(JsonTape, SeqRecordsSpansAndLinks) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("[1, \"a\", true, null, 2.5]"), YEPTRIS_OK);
    ASSERT_EQ(tp.t.count, 8u); // DOC OPEN INT STR BOOL NULL FLOAT CLOSE
    EXPECT_EQ(tp.t.kinds[0], YEP_T_DOC);
    EXPECT_EQ(tp.t.kinds[1], YEP_T_SEQ_OPEN);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_INT);
    EXPECT_EQ(tp.t.kinds[3], YEP_T_STR);
    EXPECT_EQ(tp.t.kinds[4], YEP_T_BOOL);
    EXPECT_EQ(tp.t.kinds[5], YEP_T_NULL);
    EXPECT_EQ(tp.t.kinds[6], YEP_T_FLOAT);
    EXPECT_EQ(tp.t.kinds[7], YEP_T_CLOSE);

    EXPECT_EQ((int64_t)tp.t.vals[2], 1);
    // the STR span borrows the input: offset of 'a' in the literal
    EXPECT_EQ(tp.t.offs[3], 5u);
    EXPECT_EQ(tp.t.lens[3], 1u);
    EXPECT_EQ(tp.t.vals[4], 1u); // true
    EXPECT_DOUBLE_EQ(val_double(tp.t, 6), 2.5);

    EXPECT_EQ(tp.t.vals[1], 7u); // OPEN -> matching CLOSE
    EXPECT_EQ(tp.t.vals[7], 1u); // CLOSE -> matching OPEN
}

TEST(JsonTape, MapKeysAreStrRecordsInDocumentOrder) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("{\"id\": 7, \"ok\": false}"), YEPTRIS_OK);
    ASSERT_EQ(tp.t.count, 7u); // DOC OPEN STR INT STR BOOL CLOSE
    EXPECT_EQ(tp.t.kinds[1], YEP_T_MAP_OPEN);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_STR); // "id"
    // the span starts after the opening quote
    EXPECT_EQ(tp.t.offs[2], 2u);
    EXPECT_EQ(tp.t.lens[2], 2u);
    EXPECT_EQ(tp.t.kinds[3], YEP_T_INT);
    EXPECT_EQ((int64_t)tp.t.vals[3], 7);
    EXPECT_EQ(tp.t.kinds[4], YEP_T_STR); // "ok"
    EXPECT_EQ(tp.t.lens[4], 2u);
    EXPECT_EQ(tp.t.kinds[5], YEP_T_BOOL);
    EXPECT_EQ(tp.t.vals[5], 0u); // false
    EXPECT_EQ(tp.t.vals[1], 6u);
    EXPECT_EQ(tp.t.vals[6], 1u);
}

TEST(JsonTape, NestedContainersLinkTheirOwnCloses) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("{\"a\": [1, {\"b\": null}]}"), YEPTRIS_OK);
    // DOC MAP STR SEQ INT MAP STR NULL CLOSE CLOSE CLOSE
    ASSERT_EQ(tp.t.count, 11u);
    EXPECT_EQ(tp.t.vals[1], 10u); // outer map  1 <-> 10
    EXPECT_EQ(tp.t.vals[10], 1u);
    EXPECT_EQ(tp.t.vals[3], 9u); // seq        3 <-> 9
    EXPECT_EQ(tp.t.vals[9], 3u);
    EXPECT_EQ(tp.t.vals[5], 8u); // inner map  5 <-> 8
    EXPECT_EQ(tp.t.vals[8], 5u);
}

TEST(JsonTape, ScalarRootsConvertThroughTheKernels) {
    struct {
        const char* src;
        uint8_t kind;
        int64_t ival;
        double dval;
    } cases[] = {
        {"42", YEP_T_INT, 42, 0},       {"  -7  ", YEP_T_INT, -7, 0},
        {"-3.5", YEP_T_FLOAT, 0, -3.5}, {"1e3", YEP_T_FLOAT, 0, 1000.0},
        {"true", YEP_T_BOOL, 1, 0},     {"false", YEP_T_BOOL, 0, 0},
        {"null", YEP_T_NULL, 0, 0},
    };
    for (const auto& c : cases) {
        TapeGuard tp;
        ASSERT_EQ(tp.parse(c.src), YEPTRIS_OK) << c.src;
        ASSERT_EQ(tp.t.count, 2u) << c.src; // DOC + the root record
        EXPECT_EQ(tp.t.kinds[1], c.kind) << c.src;
        if (c.kind == YEP_T_FLOAT) {
            EXPECT_DOUBLE_EQ(val_double(tp.t, 1), c.dval) << c.src;
        } else {
            EXPECT_EQ((int64_t)tp.t.vals[1], c.ival) << c.src;
        }
    }

    TapeGuard str;
    ASSERT_EQ(str.parse("\"hi\""), YEPTRIS_OK);
    ASSERT_EQ(str.t.count, 2u);
    EXPECT_EQ(str.t.kinds[1], YEP_T_STR);
    EXPECT_EQ(str.t.offs[1], 1u); // inside the quotes
    EXPECT_EQ(str.t.lens[1], 2u);
}

TEST(JsonTape, NumbersConvertInlineWithInt64Contract) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("[9223372036854775807, -9223372036854775808]"), YEPTRIS_OK);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_INT);
    EXPECT_EQ((int64_t)tp.t.vals[2], INT64_MAX);
    EXPECT_EQ((int64_t)tp.t.vals[3], INT64_MIN);
    EXPECT_EQ(tp.t.int_min, 0); // both fit exactly

    TapeGuard over;
    ASSERT_EQ(over.parse("[9223372036854775808, -9223372036854775809]"), YEPTRIS_OK);
    EXPECT_EQ(over.t.kinds[2], YEP_T_INT); // integer TEXT stays INT
    EXPECT_EQ(over.t.kinds[3], YEP_T_INT);
    EXPECT_EQ(over.t.int_min, 1); // flagged beyond int64
    EXPECT_DOUBLE_EQ(val_double(over.t, 2), 9223372036854775808.0);
    EXPECT_DOUBLE_EQ(val_double(over.t, 3), -9223372036854775808.0);
}

TEST(JsonTape, EscapedStringsKeepTheRawSpan) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("[\"a\\\"b\\\\c\\n\"]"), YEPTRIS_OK);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_STR);
    EXPECT_EQ(tp.t.lens[2], 9u); // a\"b\\c\n — escapes untouched
}

TEST(JsonTape, EmptyContainersAndWhitespace) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("  {  }  \n"), YEPTRIS_OK);
    ASSERT_EQ(tp.t.count, 3u); // DOC OPEN CLOSE
    EXPECT_EQ(tp.t.vals[1], 2u);
    EXPECT_EQ(tp.t.vals[2], 1u);
}

TEST(JsonTape, StrictRejectionsAreParseErrors) {
    const char* rejects[] = {
        "",           " ",           "{a: 1}",    "{'a': 1}", "[1,]",
        "[1 2]",      "[01]",        "\"open",    "[1] x",    "{\"a\":1} trailing",
        "{\"a\":1}{", "[undefined]", "{\"a\" 1}", "[--1]",    "[1.]",
    };
    for (const char* s : rejects) {
        TapeGuard tp;
        EXPECT_EQ(tp.parse(s), YEPTRIS_ERROR_PARSE) << s;
        EXPECT_EQ(tp.t.count, 0u) << s; // freed to zero on error
        EXPECT_EQ(tp.t._block, nullptr) << s;
    }
}

TEST(JsonTape, RawControlByteInsideStringIsAReject) {
    TapeGuard tp;
    const char bad[] = {'"', '\x01', '"'};
    EXPECT_EQ(yeptris_parse_json_tape(bad, sizeof(bad), &tp.t), YEPTRIS_ERROR_PARSE);
}

TEST(JsonTape, IllFormedUtf8IsAnEncodingError) {
    TapeGuard tp;
    const char bad[] = {'"', (char)(unsigned char)0xFF, '"'}; // gate trips, grammar passes
    EXPECT_EQ(yeptris_parse_json_tape(bad, sizeof(bad), &tp.t), YEPTRIS_ERROR_ENCODING);
}

TEST(JsonTape, DepthCapMatchesTheWalkers) {
    std::string deep(300, '[');
    deep.append(300, ']');
    TapeGuard tp;
    EXPECT_EQ(tp.parse(deep.c_str()), YEPTRIS_ERROR_PARSE);

    std::string at_cap(255, '[');
    at_cap.append(255, ']');
    TapeGuard ok;
    ASSERT_EQ(ok.parse(at_cap.c_str()), YEPTRIS_OK);
    EXPECT_EQ(ok.t.count, 1u + 2u * 255u); // DOC + open/close pairs
    EXPECT_EQ(ok.t.vals[1], ok.t.count - 1u);
}

TEST(JsonTape, ErrorParityWithParseJson) {
    const char* inputs[] = {
        "[1, 2.5, \"x\", true, null]",
        "{\"a\": {\"b\": [1, 2]}}",
        "42",
        "\"s\"",
        "true",
        "{a: 1}",
        "[1,]",
        "[1] tail",
        "",
        "   ",
        "[\"\\u00e9\\u65e5\"]",
        "{\"k\": [0, -0, 1e-3, 1E+2, 3.14159]}",
    };
    for (const char* s : inputs) {
        YeptrisStatus dom_st = YEPTRIS_OK;
        yeptris_document_free(yeptris_parse_json(s, strlen(s), &dom_st));
        TapeGuard tp;
        EXPECT_EQ(tp.parse(s), dom_st) << s;
    }
}

TEST(JsonTape, ArgContractAndFreeReuse) {
    yeptris_json_tape t;
    memset(&t, 0, sizeof(t));
    EXPECT_EQ(yeptris_parse_json_tape("[1]", 3, NULL), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_parse_json_tape(NULL, 3, &t), YEPTRIS_ERROR_ARG);

    yeptris_tape_free(NULL); // no-op
    ASSERT_EQ(yeptris_parse_json_tape("[1]", 3, &t), YEPTRIS_OK);
    EXPECT_EQ(t.count, 4u);
    yeptris_tape_free(&t);
    EXPECT_EQ(t.count, 0u);
    EXPECT_EQ(t._block, nullptr);
    yeptris_tape_free(&t); // double free is a no-op (zeroed)

    EXPECT_EQ(yeptris_parse_json_tape("[2]", 3, &t), YEPTRIS_OK); // reuse
    EXPECT_EQ((int64_t)t.vals[2], 2);
    yeptris_tape_free(&t);
}

} // namespace
