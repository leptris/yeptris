/* test_json_number_scan.cpp — the fused number kernel's gate
 * (TODO.restructure/26). Verdicts must match the plain validator
 * byte-for-byte (the wrapper IS the same walk), and the converted
 * values must be exact (INT64_MIN included, overflow promoted). */

#include <gtest/gtest.h>
#include <string.h>

#include "scan/json.h"
#include <yeptris/dom.h>
#include <yeptris/json.h>

namespace {

struct Case {
    const char* in;
    int ok;       /* expected verdict */
    int is_float; /* meaningful when ok */
    int64_t iv;   /* when !is_float */
    double dv;    /* when is_float */
};

namespace {

/* TODO.restructure/46: the vector string scanner must reject raw C0
 * controls INSIDE strings deep past chunk boundaries — the exact
 * blind-spot class the old differential missed (milestone 55). */
TEST(JsonString, C0DeepInVectorPath) {
    std::string body(200, 'a'); /* > 6 chunks on every ISA */
    for (size_t pos : {(size_t)5, (size_t)33, (size_t)40, (size_t)70, (size_t)130, (size_t)199}) {
        std::string doc = "\"" + body + "\"";
        doc[pos + 1] = (char)0x0A; /* inside the string content */
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument d = yeptris_parse_json(doc.data(), doc.size(), &st);
        EXPECT_EQ(st != YEPTRIS_OK || d == nullptr, true) << "pos=" << pos;
        if (d != nullptr)
            yeptris_document_free(d);
    }
    /* the clean long string parses */
    std::string doc = "\"" + body + "\"";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse_json(doc.data(), doc.size(), &st);
    EXPECT_EQ(st, YEPTRIS_OK);
    if (d != nullptr)
        yeptris_document_free(d);
    /* DEL is legal inside JSON strings (not a C0 control) */
    std::string del = "\"" + body + "\"";
    del[50] = (char)0x7F;
    st = YEPTRIS_OK;
    d = yeptris_parse_json(del.data(), del.size(), &st);
    EXPECT_EQ(st, YEPTRIS_OK) << "DEL must stay legal (RFC 8259)";
    if (d != nullptr)
        yeptris_document_free(d);
}

} // namespace

TEST(JsonNumberScan, Table) {
    static const Case cases[] = {
        {"0", 1, 0, 0, 0},
        {"-0", 1, 0, 0, 0},
        {"7", 1, 0, 7, 0},
        {"-7", 1, 0, -7, 0},
        {"1234567890123456789", 1, 0, 1234567890123456789LL, 0},
        {"-9223372036854775808", 1, 0, INT64_MIN, 0},
        {"9223372036854775807", 1, 0, INT64_MAX, 0},
        /* integer text beyond int64: shape 2, approximate dv */
        {"9223372036854775808", 1, 2, 0, 9223372036854775808.0},
        {"-9223372036854775809", 1, 2, 0, -9223372036854775809.0},
        {"1.5", 1, 1, 0, 1.5},
        {"-0.25", 1, 1, 0, -0.25},
        {"1e5", 1, 1, 0, 100000.0},
        {"1E+5", 1, 1, 0, 100000.0},
        {"2.5e-8", 1, 1, 0, 2.5e-8},
        /* grammar rejects */
        {"01", 0, 0, 0, 0},
        {"1.", 0, 0, 0, 0},
        {".5", 0, 0, 0, 0},
        {"1e", 0, 0, 0, 0},
        {"1e+", 0, 0, 0, 0},
        {"-", 0, 0, 0, 0},
        {"x1", 0, 0, 0, 0},
    };
    for (const Case& c : cases) {
        size_t i = 0;
        int flt = -1;
        int64_t iv = 0xdeadbeef;
        double dv = -1.0;
        int ok = yep_json_number_scan(c.in, strlen(c.in), &i, &flt, &iv, &dv);
        ASSERT_EQ(ok, c.ok) << c.in;
        if (!c.ok) {
            continue;
        }
        EXPECT_EQ(flt, c.is_float) << c.in;
        if (c.is_float) {
            EXPECT_DOUBLE_EQ(dv, c.dv) << c.in;
        } else {
            EXPECT_EQ(iv, c.iv) << c.in;
        }
        /* the wrapper agrees on the verdict (same walk, no conversion) */
        size_t j = 0;
        EXPECT_EQ(yep_json_number(c.in, strlen(c.in), &j), c.ok) << c.in;
        if (c.ok) {
            EXPECT_EQ(j, i) << c.in;
        }
    }
}

TEST(JsonNumberScan, DelimiterRule) {
    /* "1x" is YAML, not JSON — the byte after a number must delimit */
    const char* bad[] = {"1x", "1true", "1null", "1\"s\""};
    for (const char* b : bad) {
        size_t i = 0;
        EXPECT_EQ(yep_json_number_scan(b, strlen(b), &i, NULL, NULL, NULL), 0) << b;
    }
    const char* good[] = {"1,", "1]", "1}", "1:", "1 ", "1\n"};
    for (const char* g : good) {
        size_t i = 0;
        EXPECT_EQ(yep_json_number_scan(g, strlen(g), &i, NULL, NULL, NULL), 1) << g;
    }
}

} // namespace
