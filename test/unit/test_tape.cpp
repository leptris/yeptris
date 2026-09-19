// test_tape.cpp — the compact JSON tape (TODO.restructure/85, v2
// records). Pins: record kinds and OPEN/CLOSE links, zero-copy spans,
// LAZY numbers (span records at parse; yeptris_tape_convert at
// materialize — int64 exact, beyond-int64 flagged), scalar roots,
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

    // convert every record; arrays sized to count
    size_t convert_all(int64_t* iv, double* dv) {
        return yeptris_tape_convert(&t, 0, t.count, iv, dv);
    }
};

TEST(JsonTape, SeqRecordsSpansAndLinks) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("[1, \"a\", true, null, 2.5]"), YEPTRIS_OK);
    ASSERT_EQ(tp.t.count, 8u); // DOC OPEN INT STR TRUE NULL FLOAT CLOSE
    EXPECT_EQ(tp.t.kinds[0], YEP_T_DOC);
    EXPECT_EQ(tp.t.kinds[1], YEP_T_SEQ_OPEN);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_INT);
    EXPECT_EQ(tp.t.kinds[3], YEP_T_STR);
    EXPECT_EQ(tp.t.kinds[4], YEP_T_TRUE);
    EXPECT_EQ(tp.t.kinds[5], YEP_T_NULL);
    EXPECT_EQ(tp.t.kinds[6], YEP_T_FLOAT);
    EXPECT_EQ(tp.t.kinds[7], YEP_T_CLOSE);

    // the STR span borrows the input: offset of 'a' in the literal
    EXPECT_EQ(tp.t.offs[3], 5u);
    EXPECT_EQ(tp.t.lens[3], 1u);
    // lazy numbers: the spans point at the digit text
    EXPECT_EQ(tp.t.offs[2], 1u);
    EXPECT_EQ(tp.t.lens[2], 1u); // "1"
    EXPECT_EQ(tp.t.lens[6], 3u); // "2.5"

    EXPECT_EQ(tp.t.offs[1], 7u); // OPEN -> matching CLOSE
    EXPECT_EQ(tp.t.offs[7], 1u); // CLOSE -> matching OPEN
}

TEST(JsonTape, MapKeysAreStrRecordsInDocumentOrder) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("{\"id\": 7, \"ok\": false}"), YEPTRIS_OK);
    ASSERT_EQ(tp.t.count, 7u); // DOC OPEN STR INT STR FALSE CLOSE
    EXPECT_EQ(tp.t.kinds[1], YEP_T_MAP_OPEN);
    EXPECT_EQ(tp.t.kinds[2], YEP_T_STR); // "id"
    // the span starts after the opening quote
    EXPECT_EQ(tp.t.offs[2], 2u);
    EXPECT_EQ(tp.t.lens[2], 2u);
    EXPECT_EQ(tp.t.kinds[3], YEP_T_INT);
    EXPECT_EQ(tp.t.offs[3], 7u);
    EXPECT_EQ(tp.t.lens[3], 1u);         // "7"
    EXPECT_EQ(tp.t.kinds[4], YEP_T_STR); // "ok"
    EXPECT_EQ(tp.t.lens[4], 2u);
    EXPECT_EQ(tp.t.kinds[5], YEP_T_FALSE);
    EXPECT_EQ(tp.t.offs[1], 6u);
    EXPECT_EQ(tp.t.offs[6], 1u);
}

TEST(JsonTape, NestedContainersLinkTheirOwnCloses) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("{\"a\": [1, {\"b\": null}]}"), YEPTRIS_OK);
    // DOC MAP STR SEQ INT MAP STR NULL CLOSE CLOSE CLOSE
    ASSERT_EQ(tp.t.count, 11u);
    EXPECT_EQ(tp.t.offs[1], 10u); // outer map  1 <-> 10
    EXPECT_EQ(tp.t.offs[10], 1u);
    EXPECT_EQ(tp.t.offs[3], 9u); // seq        3 <-> 9
    EXPECT_EQ(tp.t.offs[9], 3u);
    EXPECT_EQ(tp.t.offs[5], 8u); // inner map  5 <-> 8
    EXPECT_EQ(tp.t.offs[8], 5u);
}

TEST(JsonTape, ScalarRootsRecordSpans) {
    struct {
        const char* src;
        uint8_t kind;
        const char* span;
    } cases[] = {
        {"42", YEP_T_INT, "42"},      {"  -7  ", YEP_T_INT, "-7"},  {"-3.5", YEP_T_FLOAT, "-3.5"},
        {"1e3", YEP_T_FLOAT, "1e3"},  {"true", YEP_T_TRUE, "true"}, {"false", YEP_T_FALSE, "false"},
        {"null", YEP_T_NULL, "null"},
    };
    for (const auto& c : cases) {
        TapeGuard tp;
        ASSERT_EQ(tp.parse(c.src), YEPTRIS_OK) << c.src;
        ASSERT_EQ(tp.t.count, 2u) << c.src; // DOC + the root record
        EXPECT_EQ(tp.t.kinds[1], c.kind) << c.src;
        EXPECT_EQ(tp.t.lens[1], strlen(c.span)) << c.src;
        EXPECT_EQ(memcmp((const char*)tp.t._src + tp.t.offs[1], c.span, strlen(c.span)), 0)
            << c.src;
    }

    TapeGuard str;
    ASSERT_EQ(str.parse("\"hi\""), YEPTRIS_OK);
    ASSERT_EQ(str.t.count, 2u);
    EXPECT_EQ(str.t.kinds[1], YEP_T_STR);
    EXPECT_EQ(str.t.offs[1], 1u); // inside the quotes
    EXPECT_EQ(str.t.lens[1], 2u);
}

TEST(JsonTape, ConvertMaterializesTheNumberContract) {
    TapeGuard tp;
    ASSERT_EQ(tp.parse("[9223372036854775807, -9223372036854775808, 2.5, -0.5e1]"), YEPTRIS_OK);
    int64_t iv[8] = {0x7E7E7E7E7E7E7E7E, 0x7E7E7E7E7E7E7E7E, 0x7E7E7E7E7E7E7E7E,
                     0x7E7E7E7E7E7E7E7E, 0x7E7E7E7E7E7E7E7E, 0x7E7E7E7E7E7E7E7E,
                     0x7E7E7E7E7E7E7E7E, 0x7E7E7E7E7E7E7E7E};
    double dv[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    ASSERT_EQ(tp.convert_all(iv, dv), 4u);
    EXPECT_EQ(iv[2], INT64_MAX);
    EXPECT_EQ(iv[3], INT64_MIN);
    EXPECT_DOUBLE_EQ(dv[4], 2.5);
    EXPECT_DOUBLE_EQ(dv[5], -5.0);
    EXPECT_EQ(tp.t.int_min, 0); // both ints fit exactly
    // non-number slots untouched
    EXPECT_EQ(iv[1], (int64_t)0x7E7E7E7E7E7E7E7E);
    EXPECT_EQ(tp.t.kinds[1], YEP_T_SEQ_OPEN);

    // beyond int64: the INT span converts to the approximate double
    TapeGuard over;
    ASSERT_EQ(over.parse("[9223372036854775808, -9223372036854775809]"), YEPTRIS_OK);
    EXPECT_EQ(over.t.kinds[2], YEP_T_INT); // integer TEXT stays INT
    EXPECT_EQ(over.t.kinds[3], YEP_T_INT);
    EXPECT_EQ(over.t.int_min, 0); // parse-time: not yet discovered
    int64_t oiv[4] = {};
    double odv[4] = {};
    ASSERT_EQ(over.convert_all(oiv, odv), 2u);
    EXPECT_EQ(over.t.int_min, 1); // flagged at materialize
    EXPECT_DOUBLE_EQ(odv[2], 9223372036854775808.0);
    EXPECT_DOUBLE_EQ(odv[3], -9223372036854775808.0);

    // NULL out arrays: count and int_min still work
    TapeGuard n;
    ASSERT_EQ(n.parse("[3, 4]"), YEPTRIS_OK);
    EXPECT_EQ(n.convert_all(NULL, NULL), 2u);

    // contract violations
    EXPECT_EQ(yeptris_tape_convert(NULL, 0, 1, NULL, NULL), SIZE_MAX);
    TapeGuard bad;
    ASSERT_EQ(bad.parse("[5]"), YEPTRIS_OK);
    EXPECT_EQ(yeptris_tape_convert(&bad.t, 2, 1, NULL, NULL), SIZE_MAX); // from > to
    EXPECT_EQ(yeptris_tape_convert(&bad.t, 0, bad.t.count + 1, NULL, NULL), SIZE_MAX);
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
    EXPECT_EQ(tp.t.offs[1], 2u);
    EXPECT_EQ(tp.t.offs[2], 1u);
}

TEST(JsonTape, StrictRejectionsAreParseErrors) {
    const char* rejects[] = {
        "",
        " ",
        "{a: 1}",
        "{'a': 1}",
        "[1,]",
        "[1 2]",
        "[01]",
        "\"open",
        "[1] x",
        "{\"a\":1} trailing",
        "{\"a\":1}{",
        "[undefined]",
        "{\"a\" 1}",
        "[--1]",
        "[1.]",
        /* the pinned gap classes (tape-diff found both) */
        "{\"a\": \"a\" 123}",
        "{\"a\" \"b\"}",
        "[\"a\" \"b\"]",
        "[\"a\" 1]",
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
    EXPECT_EQ(ok.t.offs[1], ok.t.count - 1u);
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
        /* tabs are legal RFC 8259 ws: the walker route rejects them,
         * the document fallback accepts — both engines must agree */
        "[\t1,\t2\t]",
        "{\"a\"\t:\t1\t}",
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
    int64_t iv[4] = {};
    ASSERT_EQ(yeptris_tape_convert(&t, 0, t.count, iv, NULL), 1u);
    EXPECT_EQ(iv[2], 2);
    yeptris_tape_free(&t);
}

} // namespace

/* ---- the lenient route (simdjson's deferred contract) ---------------- */

namespace {

// structural equality modulo NUM vs INT/FLOAT on identical spans
bool tapes_equiv(const yeptris_json_tape& a, const yeptris_json_tape& b) {
    /* item 07: the lenient tape's columns are lazy — materialize both
     * sides before comparing (the strict route's are primary) */
    yeptris_tape_columns(const_cast<yeptris_json_tape*>(&a));
    yeptris_tape_columns(const_cast<yeptris_json_tape*>(&b));
    if (a.count != b.count) {
        return false;
    }
    for (size_t i = 0; i < a.count; i++) {
        uint8_t ka = a.kinds[i], kb = b.kinds[i];
        if (ka != kb && !((ka == YEP_T_INT || ka == YEP_T_FLOAT) && kb == YEP_T_NUM)) {
            return false;
        }
        if (a.offs[i] != b.offs[i] || a.lens[i] != b.lens[i]) {
            return false;
        }
    }
    return true;
}

} // namespace

TEST(JsonTapeLenient, MatchesStrictOnValidDocuments) {
    const char* docs[] = {
        "[1, \"a\", true, null, 2.5]",
        "{\"id\": 7, \"name\": \"alpha\", \"vals\": [1, 2, 3], \"ok\": true}",
        "{\"a\": [1, {\"b\": null}]}",
        "[[[[1, 2]]], [[3, 4]]]",
        "[0, -0, 1e-3, 1E+2, 3.14159, -2.5e10]",
        "[]",
        "{}",
        "42",
        "\"hi\"",
        "-3.5",
    };
    for (const char* d : docs) {
        TapeGuard s, l;
        ASSERT_EQ(s.parse(d), YEPTRIS_OK) << d;
        yeptris_json_tape& lt = l.t;
        ASSERT_EQ(yeptris_parse_json_tape_lenient(d, strlen(d), &lt), YEPTRIS_OK) << d;
        EXPECT_TRUE(tapes_equiv(s.t, lt)) << d;
    }
}

TEST(JsonTapeLenient, NumberGrammarDefersToConvert) {
    TapeGuard l;
    yeptris_json_tape& lt = l.t;
    /* "1.2.3" is charset-clean but grammar-invalid: the run records
     * whole and convert rejects it at drain */
    EXPECT_EQ(yeptris_parse_json_tape_lenient("[1.2.3]", 7, &lt), YEPTRIS_OK);
    yeptris_tape_columns(&lt);
    ASSERT_EQ(lt.count, 4u); /* DOC OPEN NUM CLOSE */
    EXPECT_EQ(lt.kinds[2], YEP_T_NUM);
    EXPECT_EQ(lt.offs[2], 1u);
    EXPECT_EQ(lt.lens[2], 5u); /* "1.2.3" */
    int64_t iv[8];
    EXPECT_EQ(yeptris_tape_convert(&lt, 0, lt.count, iv, NULL), SIZE_MAX);

    /* the valid arm still converts */
    TapeGuard ok;
    yeptris_json_tape& ot = ok.t;
    ASSERT_EQ(yeptris_parse_json_tape_lenient("[42, 2.5]", 9, &ot), YEPTRIS_OK);
    yeptris_tape_columns(&ot);
    int64_t iv2[8];
    double dv2[8];
    EXPECT_EQ(yeptris_tape_convert(&ot, 0, ot.count, iv2, dv2), 2u);
    EXPECT_EQ(iv2[2], 42);
    EXPECT_DOUBLE_EQ(dv2[3], 2.5);
}

TEST(JsonTapeLenient, StructuralErrorsStayParseErrors) {
    const char* bad[] = {
        "[1, 2",    "[1 2]",     "{\"a\": 1,}", "[1,]", "{\"a\" 1}", "[\"a\",]",  "tru",
        "[1] tail", "{\"k\": }", "[,]",         "[",    "{",         "[12ab, 1]", /* charset-invalid
                                                                                     run: the
                                                                                     bare-scalar
                                                                                     reject */
    };
    for (const char* b : bad) {
        yeptris_json_tape t;
        EXPECT_EQ(yeptris_parse_json_tape_lenient(b, strlen(b), &t), YEPTRIS_ERROR_PARSE) << b;
        yeptris_tape_free(&t);
    yeptris_tape_columns(&t);
    }
}

TEST(JsonTapeLenient, TabWhitespaceIsLegal) {
    /* RFC 8259 ws includes tabs; the strict tape walk rejects them,
     * the lenient route accepts (simdjson semantics) */
    TapeGuard l;
    yeptris_json_tape& lt = l.t;
    ASSERT_EQ(yeptris_parse_json_tape_lenient("[\t1\t]", 5, &lt), YEPTRIS_OK);
    yeptris_tape_columns(&lt);
    ASSERT_EQ(lt.count, 4u);
    EXPECT_EQ(lt.kinds[2], YEP_T_NUM);
    EXPECT_EQ(lt.offs[2], 2u);
    EXPECT_EQ(lt.lens[2], 1u);
}

TEST(JsonTapeLenient, ScalarRootDefersLikeTheWalk) {
    TapeGuard l;
    yeptris_json_tape& lt = l.t;
    ASSERT_EQ(yeptris_parse_json_tape_lenient("12x", 3, &lt), YEPTRIS_OK);
    yeptris_tape_columns(&lt);
    EXPECT_EQ(lt.count, 2u);
    EXPECT_EQ(lt.kinds[1], YEP_T_NUM);
    int64_t iv[4];
    EXPECT_EQ(yeptris_tape_convert(&lt, 0, lt.count, iv, NULL), SIZE_MAX);
}

TEST(JsonTapeLenient, NonAsciiFallsBackToTheStrictRoute) {
    /* the encoding gate keeps strings' parse-time UTF-8 contract */
    const char* doc = "[\"\xc3\xa9\"]"; /* é */
    TapeGuard s, l;
    ASSERT_EQ(s.parse(doc), YEPTRIS_OK);
    yeptris_json_tape& lt = l.t;
    ASSERT_EQ(yeptris_parse_json_tape_lenient(doc, strlen(doc), &lt), YEPTRIS_OK);
    yeptris_tape_columns(&lt);
    EXPECT_TRUE(tapes_equiv(s.t, lt));

    const char* bad = "[\"\xff\"]"; /* ill-formed UTF-8 */
    yeptris_json_tape t;
    EXPECT_EQ(yeptris_parse_json_tape_lenient(bad, strlen(bad), &t), YEPTRIS_ERROR_ENCODING);
    yeptris_tape_columns(&t);
    yeptris_tape_free(&t);
}
