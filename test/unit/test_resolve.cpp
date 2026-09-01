/* test_resolve.cpp — schema resolver vectors (TODO.impl/10).
 *
 * core12: the YAML 1.2 core-schema spec tables. compat11: vectors
 * mirroring psych's scalar_scanner.rb (the 1.1 oracle; 15's ported
 * Psych suite enforces the same table from the Ruby side).
 */

#include <gtest/gtest.h>

#include <cstring>

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
    EXPECT_EQ(i, 90);
    double f = 0;
    EXPECT_EQ(yeptris_node_float(yeptris_node_map_get(root, "sexf", 4), &f), YEPTRIS_OK);
    EXPECT_DOUBLE_EQ(f, 90.5);
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
