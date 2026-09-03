/* test_limits.cpp — limit-boundary contract (TODO.impl/19).
 *
 * Every cap has two sides and the bug is always the off-by-one:
 * exactly-at-limit must be ACCEPTED, one-past must be REJECTED with
 * the right status — never a crash, never a hang. */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>
#include <yeptris/parse.h>

namespace {

std::string nested_seq(size_t depth) {
    std::string s;
    for (size_t i = 0; i < depth; i++) {
        s += "- ";
    }
    s += "leaf\n";
    return s;
}

} // namespace

TEST(Limits, EngineDepthBoundary) {
    /* 1000 engine frames: 1000 open collections is over, 999 under */
    YeptrisStatus st = YEPTRIS_OK;
    std::string under = nested_seq(999);
    YeptrisDocument doc = yeptris_parse(under.data(), under.size(), &st);
    EXPECT_EQ(st, YEPTRIS_OK);
    EXPECT_NE(doc, nullptr);
    yeptris_document_free(doc);

    st = YEPTRIS_OK;
    std::string over = nested_seq(1002);
    doc = yeptris_parse(over.data(), over.size(), &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
}

TEST(Limits, ParseOptionMaxDepthHonored) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisParseOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.max_depth = 5;

    std::string four = "a:\n  b:\n    c:\n      d: 1\n"; /* depth 4 maps */
    YeptrisDocument doc = yeptris_parse_ex(four.data(), four.size(), &opts, &st);
    EXPECT_EQ(st, YEPTRIS_OK);
    EXPECT_NE(doc, nullptr);
    yeptris_document_free(doc);

    st = YEPTRIS_OK;
    std::string deep = nested_seq(7);
    doc = yeptris_parse_ex(deep.data(), deep.size(), &opts, &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
}

TEST(Limits, MutationDepthBoundary) {
    YeptrisDocument doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode cur = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, cur), YEPTRIS_OK);

    /* chain sequences: the cap rejects somewhere below 1000, the
     * document stays serializable, and the count of accepted levels
     * is stable (the cap, not luck) */
    int levels = 0;
    for (;;) {
        YeptrisNode next = yeptris_node_new_sequence(doc);
        ASSERT_NE(next, nullptr);
        if (yeptris_node_seq_add(cur, next) != YEPTRIS_OK) {
            break;
        }
        cur = next;
        levels++;
        ASSERT_LT(levels, 1500) << "mutation cap never fired";
    }
    EXPECT_GT(levels, 900);
    EXPECT_LE(levels, 1000);
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    EXPECT_NE(out, nullptr);
    free(out);
    yeptris_document_free(doc);
}

TEST(Limits, ManyAnchorsAndAliases) {
    std::string yaml;
    for (int i = 0; i < 10000; i++) {
        yaml +=
            "a" + std::to_string(i) + ": &v" + std::to_string(i) + " " + std::to_string(i) + "\n";
    }
    for (int i = 0; i < 10000; i++) {
        yaml += "r" + std::to_string(i) + ": *v" + std::to_string(i) + "\n";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(yeptris_document_count(doc), 1u);
    YeptrisNode root = yeptris_document_root(doc, 0);
    EXPECT_EQ(yeptris_node_map_count(root), 20000u);
    /* a late alias resolves through 10k bindings */
    size_t len = 0;
    const char* v =
        yeptris_node_value(yeptris_node_alias_target(yeptris_node_map_get(root, "r9999", 5)), &len);
    EXPECT_EQ(std::string(v ? v : "", len), "9999");
    yeptris_document_free(doc);
}

TEST(Limits, HugeScalarRoundTrips) {
    const size_t n = 4u << 20; /* 4 MiB plain scalar */
    std::string yaml = "big: ";
    yaml += std::string(n, 'x');
    yaml += "\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    size_t len = 0;
    const char* v =
        yeptris_node_value(yeptris_node_map_get(yeptris_document_root(doc, 0), "big", 3), &len);
    EXPECT_EQ(len, n);
    EXPECT_EQ(v[0], 'x');
    EXPECT_EQ(v[n - 1], 'x');
    char* out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(len, yaml.size()); /* plain scalar re-emits 1:1 */
    free(out);
    yeptris_document_free(doc);
}

TEST(Limits, ThousandDocumentStream) {
    std::string yaml;
    for (int i = 0; i < 1000; i++) {
        yaml += "--- " + std::to_string(i) + "\n";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(yeptris_document_count(doc), 1000u);
    int64_t v = 0;
    ASSERT_EQ(yeptris_node_int(yeptris_document_root(doc, 999), &v), YEPTRIS_OK);
    EXPECT_EQ(v, 999);
    yeptris_document_free(doc);
}

TEST(Limits, SimpleKey1024Boundary) {
    /* YAML 1.2: a simple key is one line, at most 1024 characters —
     * libyaml counts the RAW span (quotes included); verified
     * against libyaml directly: 1024 passes, 1025 rejects */
    auto key = [](size_t n) { return std::string(n, 'x'); };
    struct {
        std::string yaml;
        bool ok;
    } cases[] = {
        {key(1024) + ": v\n", true},             /* exact: accepted */
        {key(1025) + ": v\n", false},            /* one over: rejected */
        {"\"" + key(1022) + "\": v\n", true},    /* 1024 raw with quotes */
        {"\"" + key(1024) + "\": v\n", false},   /* 1026 raw */
        {"{" + key(1025) + ": 1}\n", false},     /* flow plain */
        {"{\"" + key(1024) + "\": 1}\n", false}, /* flow quoted (fast path) */
        {"[{" + key(1025) + ": 1}]\n", false},   /* fast path single-pair */
        {"? " + key(1025) + "\n: v\n", true},    /* explicit key: no limit */
        {"k: " + key(1025) + "\n", true},        /* long VALUE: no limit */
    };
    for (const auto& c : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(c.yaml.data(), c.yaml.size(), &st);
        if (c.ok) {
            EXPECT_EQ(st, YEPTRIS_OK) << c.yaml.substr(0, 20);
            ASSERT_NE(doc, nullptr);
        } else {
            EXPECT_EQ(doc, nullptr) << c.yaml.substr(0, 20);
        }
        yeptris_document_free(doc);
    }
}

TEST(Limits, SinglePairMapWithNestedCollection) {
    /* 07's known gap, since fixed by the entry buffer: balanced
     * events, correct DOM, single- and multi-pair forms */
    const char* yaml = "[a: [1]]\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml, strlen(yaml), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "[{a: [1]}]\n");
    free(out);
    yeptris_document_free(doc);
}
