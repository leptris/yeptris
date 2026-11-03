/* test_resolve.cpp — schema resolver vectors (TODO.impl/10).
 *
 * core12: the YAML 1.2 core-schema spec tables. compat11: vectors
 * mirroring psych's scalar_scanner.rb (the 1.1 oracle; 15's ported
 * Psych suite enforces the same table from the Ruby side).
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>

namespace {

YeptrisTagId tag_of(const char* yaml, const YeptrisParseOptions* opts) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_ex(yaml, strlen(yaml), opts, &st);
    EXPECT_EQ(st, YEPTRIS_OK);
    if (doc == nullptr) {
        return YEPTRIS_TAG_CUSTOM;
    }
    YeptrisNode root = yeptris_document_root(doc, 0);
    EXPECT_NE(root, nullptr);
    YeptrisTagId id = yeptris_node_tag_id(root);
    yeptris_document_free(doc);
    return id;
}

struct Vec {
    const char* in;
    YeptrisTagId want;
};

void run_vecs(const Vec* v, size_t n, const YeptrisParseOptions* opts) {
    for (size_t i = 0; i < n; i++) {
        EXPECT_EQ(tag_of(v[i].in, opts), v[i].want) << "input: " << v[i].in;
    }
}

} // namespace

TEST(Core12, SpecTables) {
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_CORE;
    const Vec v[] = {
        {"null", YEPTRIS_TAG_NULL},      {"Null", YEPTRIS_TAG_NULL},
        {"NULL", YEPTRIS_TAG_NULL},      {"~", YEPTRIS_TAG_NULL},
        {"''", YEPTRIS_TAG_STR},         {"\"x\"", YEPTRIS_TAG_STR},
        {"true", YEPTRIS_TAG_BOOL},      {"True", YEPTRIS_TAG_BOOL},
        {"TRUE", YEPTRIS_TAG_BOOL},      {"false", YEPTRIS_TAG_BOOL},
        {"False", YEPTRIS_TAG_BOOL},     {"FALSE", YEPTRIS_TAG_BOOL},
        {"yes", YEPTRIS_TAG_STR},        {"no", YEPTRIS_TAG_STR}, /* 1.2: not bool */
        {"on", YEPTRIS_TAG_STR},         {"off", YEPTRIS_TAG_STR},
        {"0", YEPTRIS_TAG_INT},          {"7", YEPTRIS_TAG_INT},
        {"-3", YEPTRIS_TAG_INT},         {"+27", YEPTRIS_TAG_INT},
        {"0x1A", YEPTRIS_TAG_INT},       {"-0x10", YEPTRIS_TAG_INT},
        {"0X1A", YEPTRIS_TAG_STR}, /* lowercase 0x only — the core regexp */
        {"0o17", YEPTRIS_TAG_INT},       {"1_000", YEPTRIS_TAG_STR}, /* no _ in 1.2 */
        {"0", YEPTRIS_TAG_INT},          {"08", YEPTRIS_TAG_INT},    /* dec */
        {"1.", YEPTRIS_TAG_FLOAT},       {"1e3", YEPTRIS_TAG_FLOAT},
        {"1.5e-3", YEPTRIS_TAG_FLOAT},   {"-.inf", YEPTRIS_TAG_FLOAT},
        {".inf", YEPTRIS_TAG_FLOAT},     {"+.Inf", YEPTRIS_TAG_FLOAT},
        {".NAN", YEPTRIS_TAG_FLOAT},     {".nan", YEPTRIS_TAG_FLOAT},
        {"-.NaN", YEPTRIS_TAG_STR},      /* NaN carries no sign */
        {"1:30", YEPTRIS_TAG_STR},       /* no sexagesimal in 1.2 */
        {"1900-01-01", YEPTRIS_TAG_STR}, /* no timestamp in core */
        {"y", YEPTRIS_TAG_STR},          {"n", YEPTRIS_TAG_STR},
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);
}

TEST(Core12, QuotedAndTagged) {
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_CORE;
    const Vec v[] = {
        {"'1'", YEPTRIS_TAG_STR},         /* quoted: never implicit */
        {"\"true\"", YEPTRIS_TAG_STR},    //
        {"!!int 5", YEPTRIS_TAG_INT},     /* explicit core tag */
        {"!!bool yes", YEPTRIS_TAG_BOOL}, /* tag wins over bytes */
        {"!foo 5", YEPTRIS_TAG_CUSTOM},   /* custom */
        {"!!str 5", YEPTRIS_TAG_STR},     //
        {"[1, 2]", YEPTRIS_TAG_SEQ},      /* collections */
        {"{a: 1}", YEPTRIS_TAG_MAP},      //
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);
}

TEST(Compat11, PsychScalarScanner) {
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
    const Vec v[] = {
        {"yes", YEPTRIS_TAG_BOOL},
        {"Y", YEPTRIS_TAG_BOOL},
        {"on", YEPTRIS_TAG_BOOL},
        {"YES", YEPTRIS_TAG_BOOL},
        {"no", YEPTRIS_TAG_BOOL},
        {"n", YEPTRIS_TAG_BOOL},
        {"off", YEPTRIS_TAG_BOOL},
        {"Off", YEPTRIS_TAG_BOOL},
        {"null", YEPTRIS_TAG_NULL},
        {"~", YEPTRIS_TAG_NULL},
        {"NULL", YEPTRIS_TAG_NULL},
        {"Null", YEPTRIS_TAG_NULL},
        {"=", YEPTRIS_TAG_VALUE},
        {"<<", YEPTRIS_TAG_MERGE},
        {"0b101", YEPTRIS_TAG_INT},
        {"0x1A", YEPTRIS_TAG_INT},
        {"017", YEPTRIS_TAG_INT},   /* leading-0 octal */
        {"1_000", YEPTRIS_TAG_INT}, /* underscores */
        {"-0x2f", YEPTRIS_TAG_INT},
        {"0X1A", YEPTRIS_TAG_STR}, /* psych keeps it a String too */
        {".inf", YEPTRIS_TAG_FLOAT},
        {"-.Inf", YEPTRIS_TAG_FLOAT},
        {".NAN", YEPTRIS_TAG_FLOAT},
        {"1:30", YEPTRIS_TAG_INT}, /* sexagesimal */
        {"1:30.5", YEPTRIS_TAG_FLOAT},
        {"82:30:15", YEPTRIS_TAG_INT},
        {"1901-12-14", YEPTRIS_TAG_TIMESTAMP}, /* psych dates */
        {"2001-12-14t21:59:43.10-05:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-12-14 21:59:43.10 Z", YEPTRIS_TAG_TIMESTAMP},
        {"true", YEPTRIS_TAG_BOOL},
        {"maybe", YEPTRIS_TAG_STR},
        {":symbol", YEPTRIS_TAG_STR}, /* symbols are a Ruby concern */
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);
}

TEST(Compat11, PsychSexagesimalWeights) {
    /* Psych's fold is weight-based (60 ** |e - 2|): a 2-component
     * value is H:M with seconds implicitly zero -- 1:30 -> 5400, not
     * 90. 3-component already matches the positional fold. Pinned
     * against Psych's scanner verbatim (issue #30,
     * TODO.restructure/43). */
    const struct {
        const char* text;
        int64_t want;
    } ints[] = {
        {"1:30", 5400}, {"-1:30", -1800}, {"0:30", 1800}, {"190:20:30", 685230}, {"1:2:3", 3723},
    };
    const struct {
        const char* text;
        double want;
    } floats[] = {
        {"1:30.5", 5430.0},
        {"190:20:30.15", 685230.15},
    };
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
    for (size_t k = 0; k < sizeof(ints) / sizeof(ints[0]); k++) {
        std::string y = std::string("k: ") + ints[k].text + "\n";
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_ex(y.c_str(), y.size(), &opts, &st);
        ASSERT_EQ(st, YEPTRIS_OK) << ints[k].text;
        int64_t got = 0;
        EXPECT_EQ(
            yeptris_node_int(yeptris_node_map_get(yeptris_document_root(doc, 0), "k", 1), &got),
            YEPTRIS_OK)
            << ints[k].text;
        EXPECT_EQ(got, ints[k].want) << ints[k].text;
        yeptris_document_free(doc);
    }
    for (size_t k = 0; k < sizeof(floats) / sizeof(floats[0]); k++) {
        std::string y = std::string("k: ") + floats[k].text + "\n";
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_ex(y.c_str(), y.size(), &opts, &st);
        ASSERT_EQ(st, YEPTRIS_OK) << floats[k].text;
        double got = 0;
        EXPECT_EQ(
            yeptris_node_float(yeptris_node_map_get(yeptris_document_root(doc, 0), "k", 1), &got),
            YEPTRIS_OK)
            << floats[k].text;
        EXPECT_DOUBLE_EQ(got, floats[k].want) << floats[k].text;
        yeptris_document_free(doc);
    }
}

TEST(Resolve, TypedAccessors) {
    const char* y = "i: 42\nf: 3.5\nb: true\ns: hi\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    int64_t i = 0;
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "i", 1), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 42);
    double f = 0;
    EXPECT_EQ(yeptris_node_float(yeptris_node_map_get(root, "f", 1), &f), YEPTRIS_OK);
    EXPECT_DOUBLE_EQ(f, 3.5);
    int b = 0;
    EXPECT_EQ(yeptris_node_bool(yeptris_node_map_get(root, "b", 1), &b), YEPTRIS_OK);
    EXPECT_EQ(b, 1);
    /* wrong-accessor: the tag decides */
    int64_t bad = 0;
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "s", 1), &bad), YEPTRIS_ERROR_PARSE);
    /* int bases + compat forms */
    const char* y2 = "a: 0x1A\nb: 0o17\nc: -7\n";
    yeptris_document_free(doc);
    doc = yeptris_parse(y2, strlen(y2), &st);
    root = yeptris_document_root(doc, 0);
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "a", 1), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 26);
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "b", 1), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 15);
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "c", 1), &i), YEPTRIS_OK);
    EXPECT_EQ(i, -7);
    yeptris_document_free(doc);
    (void)bad;
}

TEST(Resolve, CompatTypedConversion) {
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
    const char* y = "oct: 017\nsex: 1:30\nsexf: 1:30.5\nbig: 1_000\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_ex(y, strlen(y), &opts, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    int64_t i = 0;
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "oct", 3), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 15);
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "sex", 3), &i), YEPTRIS_OK);
    /* Psych's H:M weights (issue #30): 1:30 is 5400s, 1:30.5 is 5430.0s */
    EXPECT_EQ(i, 5400);
    double f = 0;
    EXPECT_EQ(yeptris_node_float(yeptris_node_map_get(root, "sexf", 4), &f), YEPTRIS_OK);
    EXPECT_DOUBLE_EQ(f, 5430.0);
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "big", 3), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 1000);
    yeptris_document_free(doc);
}

TEST(Resolve, TagUriTable) {
    EXPECT_STREQ(yeptris_tag_uri(YEPTRIS_TAG_INT), "tag:yaml.org,2002:int");
    EXPECT_STREQ(yeptris_tag_uri(YEPTRIS_TAG_MERGE), "tag:yaml.org,2002:merge");
    EXPECT_EQ(yeptris_tag_uri(YEPTRIS_TAG_CUSTOM), nullptr);
}

TEST(Resolve, MaxDepthOption) {
    /* nesting beyond the cap errors with YEPTRIS_ERROR_DEPTH */
    std::string deep = "a: ";
    for (int i = 0; i < 30; i++) {
        deep += "[";
    }
    deep += "1";
    for (int i = 0; i < 30; i++) {
        deep += "]";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(deep.c_str(), deep.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    yeptris_document_free(doc);

    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_CORE;
    opts.max_depth = 16;
    doc = yeptris_parse_ex(deep.c_str(), deep.size(), &opts, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
    EXPECT_EQ(doc, nullptr);
}

/* ---- the FULL 1.2 family (#906 slice 3) --------------------------------- */
//
// full12 = the core productions (spec 10.4: interoperable schemas are
// "strongly recommended ... based on the core schema") + the yaml.org
// type repository carried forward in practice: RFC3339-era timestamps
// (spec example 2.22's canonical/iso8601/spaced/date forms) and the
// merge/value words. PyYAML 6.0.2's SafeLoader is the practice oracle
// for the timestamp surface; scripts/pyyaml-timestamp-diff.py
// regenerates the bounded corpus below from a live PyYAML.

TEST(Full12, SpecExample22Forms) {
    // spec 2.22 "Timestamps", verbatim
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_FULL;
    const Vec v[] = {
        {"2001-12-15T02:59:43.1Z", YEPTRIS_TAG_TIMESTAMP},       /* canonical */
        {"2001-12-14t21:59:43.10-05:00", YEPTRIS_TAG_TIMESTAMP}, /* iso8601 */
        {"2001-12-14 21:59:43.10 -5", YEPTRIS_TAG_TIMESTAMP},    /* spaced */
        {"2002-12-14", YEPTRIS_TAG_TIMESTAMP},                   /* date */
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);

    // the same plains under 12_CORE stay strings: core has no timestamp
    // production (spec 10.3), deliberately — full is the extension lane
    YeptrisParseOptions core = {};
    core.schema = YEPTRIS_SCHEMA_12_CORE;
    const Vec c[] = {
        {"2001-12-15T02:59:43.1Z", YEPTRIS_TAG_STR},
        {"2001-12-14t21:59:43.10-05:00", YEPTRIS_TAG_STR},
        {"2001-12-14 21:59:43.10 -5", YEPTRIS_TAG_STR},
        {"2002-12-14", YEPTRIS_TAG_STR},
    };
    run_vecs(c, sizeof(c) / sizeof(c[0]), &core);
}

TEST(Full12, PyYamlTimestampDifferential) {
    // PyYAML 6.0.2 verdicts (yaml.compose -> node.tag) for the bounded
    // corpus. Every entry is a lone plain-scalar document, so the
    // engine's scanner delivers the bytes the resolver saw.
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_FULL;
    const Vec v[] = {
        // accepted: month/day/hour are 1-2 digits once a time part exists
        {"2001-01-01T1:00:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01t1:00:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01  1:00:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01 1:00:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00", YEPTRIS_TAG_TIMESTAMP},
        // fraction: dot then ZERO or more digits
        {"2001-01-01T01:00:00.", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00.5", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00.123456789", YEPTRIS_TAG_TIMESTAMP},
        // timezone: optional; Z or a signed 1-2 digit offset, :mm optional
        {"2001-01-01T01:00:00Z", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00+5", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00-5", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00+05", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00+05:30", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00-05:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T01:00:00 +5", YEPTRIS_TAG_TIMESTAMP},   /* space before tz */
        {"2001-01-01T01:00:00    Z", YEPTRIS_TAG_TIMESTAMP}, /* a space run */
        {"2001-01-01T01:00:00 Z", YEPTRIS_TAG_TIMESTAMP},
        // range is NOT a resolution concern: the oracle tags these, its
        // constructor raises later (full12 is spec-refereed resolution)
        {"2001-13-14", YEPTRIS_TAG_TIMESTAMP},
        {"2001-00-00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T25:99:99", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-32", YEPTRIS_TAG_TIMESTAMP},
        {"2001-12-14 24:00:00", YEPTRIS_TAG_TIMESTAMP},
        // other in-range shapes
        {"2001-12-14 21:59:43", YEPTRIS_TAG_TIMESTAMP},
        {"2001-12-14 21:59:43.0 -05:00", YEPTRIS_TAG_TIMESTAMP},
        {"0000-01-01", YEPTRIS_TAG_TIMESTAMP},
        {"9999-12-31", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T00:00:00-00:00", YEPTRIS_TAG_TIMESTAMP},
        {"2001-01-01T00:00:00+00:00", YEPTRIS_TAG_TIMESTAMP},
        // rejected: date-only demands EXACTLY two-digit month and day
        {"2001-1-1", YEPTRIS_TAG_STR},
        {"2001-01-1", YEPTRIS_TAG_STR},
        {"2001-1-01", YEPTRIS_TAG_STR},
        {"2001-12-14x", YEPTRIS_TAG_STR},
        // rejected: the time part is mandatory once the date is narrow
        {"2001-01-01T001:00:00", YEPTRIS_TAG_STR}, /* hour is 1-2 digits */
        {"2001-01-01T01:0:00", YEPTRIS_TAG_STR},   /* mm/ss exactly two */
        {"2001-01-01T01:00:0", YEPTRIS_TAG_STR},
        {"2001-01-01T01:00", YEPTRIS_TAG_STR},
        {"2001-01-01T01:00:00:00", YEPTRIS_TAG_STR},
        {"2001-01-01T01:00:00,5", YEPTRIS_TAG_STR}, /* comma fraction */
        {"2001-01-01T01:00:00.5.5", YEPTRIS_TAG_STR},
        // rejected: tz casings and widths
        {"2001-01-01T01:00:00z", YEPTRIS_TAG_STR},
        {"2001-01-01T01:00:00+0530", YEPTRIS_TAG_STR},
        {"2001-01-01T01:00:00+05:30x", YEPTRIS_TAG_STR},
        // rejected: year shape and sign
        {"-2001-12-14", YEPTRIS_TAG_STR},
        {"+2001-12-14", YEPTRIS_TAG_STR},
        {"20011-12-14", YEPTRIS_TAG_STR},
        {"201-12-14", YEPTRIS_TAG_STR},
        {"2002-12-14T00:00:00Z extra", YEPTRIS_TAG_STR},
        // the oracle's scanner rejects a TAB separator before resolution;
        // the oracle's REGEX accepts [ \t]+ and this engine's scanner
        // delivers the tab, so the regex answer is the pinned one
        {"2001-01-01\t1:00:00", YEPTRIS_TAG_TIMESTAMP},
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);
}

TEST(Full12, CoreBaseAndRepositoryWords) {
    YeptrisParseOptions opts = {};
    opts.schema = YEPTRIS_SCHEMA_12_FULL;
    const Vec v[] = {
        // the core productions ride verbatim (full is core-based)
        {"null", YEPTRIS_TAG_NULL},
        {"~", YEPTRIS_TAG_NULL},
        {"true", YEPTRIS_TAG_BOOL},
        {"FALSE", YEPTRIS_TAG_BOOL},
        {"0x1A", YEPTRIS_TAG_INT},
        {"0o17", YEPTRIS_TAG_INT},
        {"-19", YEPTRIS_TAG_INT},
        {"1e3", YEPTRIS_TAG_FLOAT},
        {"1.5e-3", YEPTRIS_TAG_FLOAT},
        {"+.Inf", YEPTRIS_TAG_FLOAT},
        {"0X1A", YEPTRIS_TAG_STR},  /* core's lowercase 0x */
        {"yes", YEPTRIS_TAG_STR},   /* 1.1 bool words stay compat's */
        {"1:30", YEPTRIS_TAG_STR},  /* no sexagesimal in 1.2 */
        {"1_000", YEPTRIS_TAG_STR}, /* no underscores in 1.2 */
        // the 1.1 full-schema words PyYAML also resolves implicitly
        {"=", YEPTRIS_TAG_VALUE},
        {"<<", YEPTRIS_TAG_MERGE},
        // quoted scalars are never implicit; explicit tags win
        {"'2001-12-14'", YEPTRIS_TAG_STR},
        {"\"2001-12-14\"", YEPTRIS_TAG_STR},
        {"!!timestamp 2001-12-14", YEPTRIS_TAG_TIMESTAMP},
        {"!!str 2001-12-14", YEPTRIS_TAG_STR},
        {"!!binary aGVsbG8=", YEPTRIS_TAG_BINARY},
        // collections keep their kind tags
        {"[2001-12-14]", YEPTRIS_TAG_SEQ},
        {"{d: 2001-12-14}", YEPTRIS_TAG_MAP},
    };
    run_vecs(v, sizeof(v) / sizeof(v[0]), &opts);
}

TEST(Full12, FullVsCoreDivergence) {
    // The divergence table's non-timestamp rows: full is SPEC-refereed
    // (core-based), NOT PyYAML-refereed — PyYAML's implicit typing is
    // the 1.1 schema verbatim (underscores/sexagesimal/octal-leading-0
    // ints, yes/no bools, dot-required floats). These stay with
    // 11_COMPAT; a 1.2 interoperable schema keeps the core productions
    // (spec 10.4's recommendation).
    YeptrisParseOptions full = {};
    full.schema = YEPTRIS_SCHEMA_12_FULL;
    YeptrisParseOptions core = {};
    core.schema = YEPTRIS_SCHEMA_12_CORE;
    const Vec c[] = {
        {"0o17", YEPTRIS_TAG_INT},    /* PyYAML: str (no 0o production) */
        {"08", YEPTRIS_TAG_INT},      /* PyYAML: str (octal-only prefix) */
        {"1e3", YEPTRIS_TAG_FLOAT},   /* PyYAML: str (dot required) */
        {"1.5e2", YEPTRIS_TAG_FLOAT}, /* PyYAML: str (signed exponent) */
        {"1_000", YEPTRIS_TAG_STR},   /* PyYAML: int (underscores) */
        {"1:30", YEPTRIS_TAG_STR},    /* PyYAML: int (sexagesimal) */
        {"yes", YEPTRIS_TAG_STR},     /* PyYAML: bool (1.1 words) */
        {"=", YEPTRIS_TAG_VALUE},     /* full's repository word */
        {"<<", YEPTRIS_TAG_MERGE},    /* full's repository word */
    };
    run_vecs(c, sizeof(c) / sizeof(c[0]), &full);
    const Vec k[] = {
        {"0o17", YEPTRIS_TAG_INT},    {"08", YEPTRIS_TAG_INT},    {"1e3", YEPTRIS_TAG_FLOAT},
        {"1.5e2", YEPTRIS_TAG_FLOAT}, {"1_000", YEPTRIS_TAG_STR}, {"1:30", YEPTRIS_TAG_STR},
        {"yes", YEPTRIS_TAG_STR},     {"=", YEPTRIS_TAG_STR},     {"<<", YEPTRIS_TAG_STR},
    };
    run_vecs(k, sizeof(k) / sizeof(k[0]), &core);
}
