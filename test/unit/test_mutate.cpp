/* test_mutate.cpp — from-scratch construction (TODO.impl/11 phase 3).
 *
 * The contract: synthesized documents behave exactly like parsed ones
 * (queries, typing, serialization), links reject double-parents,
 * duplicates, cross-document misuse, and depth overflow, and teardown
 * is clean (ASAN). */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>
#include <yeptris/json.h>

namespace {

std::string ser(YeptrisDocument doc) {
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    EXPECT_NE(out, nullptr);
    std::string s(out ? out : "", len);
    free(out);
    return s;
}

std::string ser_json(YeptrisDocument doc) {
    size_t len = 0;
    char* out = yeptris_serialize_json(doc, &len);
    EXPECT_NE(out, nullptr);
    std::string s(out ? out : "", len);
    free(out);
    return s;
}

} // namespace

TEST(Mutate, BuildMapSerializeRoundtrip) {
    YeptrisDocument doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_NE(root, nullptr);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);

    YeptrisNode name = yeptris_node_new_scalar(doc, "yeptris", 7, YEPTRIS_STYLE_PLAIN);
    YeptrisNode ver = yeptris_node_new_scalar(doc, "1.0", 3, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "name", 4, name), YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_map_add(root, "version", 7, ver), YEPTRIS_OK);

    /* queries work unchanged */
    EXPECT_EQ(yeptris_document_count(doc), 1u);
    YeptrisNode got = yeptris_node_map_get(root, "name", 4);
    ASSERT_NE(got, nullptr);
    size_t len = 0;
    const char* v = yeptris_node_value(got, &len);
    EXPECT_EQ(std::string(v ? v : "", len), "yeptris");

    /* serialize -> parse -> same structure (the emitter round-trip) */
    std::string yaml = ser(doc);
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument back = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(st, YEPTRIS_OK);
    YeptrisNode r2 = yeptris_document_root(back, 0);
    YeptrisNode n2 = yeptris_node_map_get(r2, "name", 4);
    ASSERT_NE(n2, nullptr);
    v = yeptris_node_value(n2, &len);
    EXPECT_EQ(std::string(v ? v : "", len), "yeptris");
    yeptris_document_free(back);
    yeptris_document_free(doc);
}

TEST(Mutate, NestedTreeAndTypingSSOT) {
    YeptrisDocument doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);

    YeptrisNode list = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_node_map_add(root, "list", 4, list), YEPTRIS_OK);
    YeptrisNode one = yeptris_node_new_scalar(doc, "1", 1, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_seq_add(list, one), YEPTRIS_OK);
    /* quoted "2" stays a string: the resolver is the typing SSOT */
    YeptrisNode two = yeptris_node_new_scalar(doc, "2", 1, YEPTRIS_STYLE_DOUBLE_QUOTED);
    ASSERT_EQ(yeptris_node_seq_add(list, two), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_seq_count(list), 2u);

    int64_t i = 0;
    EXPECT_EQ(yeptris_node_int(yeptris_node_seq_at(list, 0), &i), YEPTRIS_OK);
    EXPECT_EQ(i, 1);
    EXPECT_NE(yeptris_node_int(yeptris_node_seq_at(list, 1), &i), YEPTRIS_OK);
    size_t len = 0;
    const char* s = yeptris_node_value(yeptris_node_seq_at(list, 1), &len);
    EXPECT_EQ(std::string(s ? s : "", len), "2");

    double d = 0;
    YeptrisNode f = yeptris_node_new_scalar(doc, "2.5e3", 5, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "f", 1, f), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_float(f, &d), YEPTRIS_OK);
    EXPECT_DOUBLE_EQ(d, 2500.0);

    yeptris_document_free(doc);
}

TEST(Mutate, DuplicateKeyRejectedSetReplaces) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    YeptrisNode a = yeptris_node_new_scalar(doc, "a", 1, YEPTRIS_STYLE_PLAIN);
    YeptrisNode b = yeptris_node_new_scalar(doc, "b", 1, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "k", 1, a), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_add(root, "k", 1, b), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(yeptris_node_map_count(root), 1u);
    /* set replaces in place: count stable, position kept */
    YeptrisNode c = yeptris_node_new_scalar(doc, "c", 1, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "z", 1, c), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_set(root, "k", 1, b), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_count(root), 2u);
    size_t len = 0;
    const char* v = yeptris_node_value(yeptris_node_map_get(root, "k", 1), &len);
    EXPECT_EQ(std::string(v ? v : "", len), "b");
    /* k is still pair 0 */
    YeptrisNode kk, kv;
    ASSERT_EQ(yeptris_node_map_at(root, 0, &kk, &kv), 0);
    v = yeptris_node_value(kv, &len);
    EXPECT_EQ(std::string(v ? v : "", len), "b");
    yeptris_document_free(doc);
}

TEST(Mutate, DeletesUnlinkSubtree) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    YeptrisNode seq = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_node_map_add(root, "arr", 3, seq), YEPTRIS_OK);
    for (int i = 0; i < 3; i++) {
        char buf[2] = {(char)('0' + i), 0};
        ASSERT_EQ(
            yeptris_node_seq_add(seq, yeptris_node_new_scalar(doc, buf, 1, YEPTRIS_STYLE_PLAIN)),
            YEPTRIS_OK);
    }
    ASSERT_EQ(yeptris_node_seq_del(seq, 1), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_seq_count(seq), 2u);
    size_t len = 0;
    const char* v = yeptris_node_value(yeptris_node_seq_at(seq, 1), &len);
    EXPECT_EQ(std::string(v ? v : "", len), "2");

    ASSERT_EQ(yeptris_node_map_del(root, "arr", 3), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_count(root), 0u);
    EXPECT_EQ(yeptris_node_map_get(root, "arr", 3), nullptr);
    /* double delete fails */
    EXPECT_NE(yeptris_node_map_del(root, "arr", 3), YEPTRIS_OK);
    yeptris_document_free(doc);
}

TEST(Mutate, RejectsDoubleAttachCrossDocAndRootReuse) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisDocument other = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    YeptrisNode seq = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_node_map_add(root, "s", 1, seq), YEPTRIS_OK);

    /* a node with a parent cannot take a second parent */
    YeptrisNode m = yeptris_node_new_mapping(doc);
    EXPECT_EQ(yeptris_node_seq_add(seq, m), YEPTRIS_OK);
    EXPECT_NE(yeptris_node_seq_add(seq, m), YEPTRIS_OK);

    /* cross-document links are invalid */
    YeptrisNode foreign = yeptris_node_new_scalar(other, "x", 1, YEPTRIS_STYLE_PLAIN);
    EXPECT_EQ(yeptris_node_seq_add(seq, foreign), YEPTRIS_ERROR_ARG);

    /* the root is attached: cannot re-root it */
    EXPECT_NE(yeptris_document_set_root(doc, root), YEPTRIS_OK);

    yeptris_document_free(doc);
    yeptris_document_free(other);
}

TEST(Mutate, DepthCapIsEnforced) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    /* chain sequences until the cap rejects: never a crash */
    YeptrisNode cur = root;
    int levels = 0;
    for (;;) {
        YeptrisNode next = yeptris_node_new_sequence(doc);
        ASSERT_NE(next, nullptr);
        if (yeptris_node_seq_add(cur, next) != YEPTRIS_OK) {
            break;
        }
        cur = next;
        levels++;
        ASSERT_LT(levels, 2000) << "depth cap never fired";
    }
    EXPECT_GT(levels, 900);
    /* the document still serializes (bounded recursion) */
    EXPECT_FALSE(ser(doc).empty());
    yeptris_document_free(doc);
}

TEST(Mutate, JsonOutputOfBuiltTree) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    YeptrisNode inner = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_node_map_add(root, "inner", 5, inner), YEPTRIS_OK);
    YeptrisNode n = yeptris_node_new_scalar(doc, "42", 2, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(inner, "n", 1, n), YEPTRIS_OK);
    YeptrisNode q = yeptris_node_new_scalar(doc, "07", 2, YEPTRIS_STYLE_DOUBLE_QUOTED);
    ASSERT_EQ(yeptris_node_map_add(inner, "q", 1, q), YEPTRIS_OK);
    /* the public JSON serializer keeps its spaced single-line form;
     * compact output is the json-c compat entry's contract (JsonCBuild) */
    std::string j = ser_json(doc);
    EXPECT_EQ(j, "{\"inner\": {\"n\": 42, \"q\": \"07\"}}\n");
    yeptris_document_free(doc);
}

TEST(Mutate, EmptyScalarsAndEmptyKeys) {
    YeptrisDocument doc = yeptris_document_new();
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    YeptrisNode empty = yeptris_node_new_scalar(doc, nullptr, 0, YEPTRIS_STYLE_PLAIN);
    ASSERT_NE(empty, nullptr);
    ASSERT_EQ(yeptris_node_map_add(root, "", 0, empty), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_count(root), 1u);
    EXPECT_NE(yeptris_node_map_get(root, "", 0), nullptr);
    /* an empty plain scalar resolves null, like at parse */
    int b = 1;
    EXPECT_NE(yeptris_node_bool(empty, &b), YEPTRIS_OK);
    yeptris_document_free(doc);
}

/* ---- bulk build ------------------------------------------------------- */

namespace {

struct Entry {
    uint8_t op;
    uint8_t style;
    uint32_t off;
    uint32_t len;
};

std::string build_dump(const std::vector<Entry>& entries, const std::string& blob,
                       YeptrisStatus* st) {
    std::vector<YeptrisBuildEntry> flat;
    flat.reserve(entries.size());
    for (const Entry& e : entries) {
        flat.push_back({e.op, e.style, 0, e.off, e.len});
    }
    YeptrisDocument doc = yeptris_document_new();
    *st = yeptris_document_build(doc, flat.data(), flat.size(), blob.data(), blob.size());
    if (*st != YEPTRIS_OK) {
        yeptris_document_free(doc);
        return "";
    }
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    std::string text(out ? out : "", out ? len : 0);
    free(out);
    yeptris_document_free(doc);
    return text;
}

} // namespace

TEST(BulkBuild, NestedDocument) {
    /* {a: [1, 2], b: {c: x}} in document order:
     * MAP, k=a, SEQ, 1, 2, END, k=b, MAP, k=c, x, END, END */
    std::vector<Entry> es = {
        {3, 0, 0, 0},  // MAP
        {1, 1, 0, 1},  // a
        {2, 0, 0, 0},  // SEQ
        {1, 1, 2, 1},  // 1
        {1, 1, 4, 1},  // 2
        {4, 0, 0, 0},  // END seq
        {1, 1, 6, 1},  // b
        {3, 0, 0, 0},  // MAP
        {1, 1, 8, 1},  // c
        {1, 1, 10, 1}, // x
        {4, 0, 0, 0},  // END inner map
        {4, 0, 0, 0},  // END root
    };
    std::string blob = "a12bcx";
    /* offsets above: a=0,1=2,2=4,b=6,c=8,x=10 */
    blob = "a\x001\x002\x00b\x00c\x00x\x00"; /* not used; simple offsets below */
    blob = "a12bcx";
    es[1] = {1, 1, 0, 1}; /* a */
    es[3] = {1, 1, 1, 1}; /* 1 */
    es[4] = {1, 1, 2, 1}; /* 2 */
    es[6] = {1, 1, 3, 1}; /* b */
    es[8] = {1, 1, 4, 1}; /* c */
    es[9] = {1, 1, 5, 1}; /* x */
    YeptrisStatus st = YEPTRIS_OK;
    std::string out = build_dump(es, blob, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(out, "a:\n  - 1\n  - 2\nb:\n  c: x\n");
}

TEST(BulkBuild, ScalarRoot) {
    std::vector<Entry> es = {{1, 1, 0, 5}};
    YeptrisStatus st = YEPTRIS_OK;
    EXPECT_EQ(build_dump(es, "hello", &st), "hello\n");
    ASSERT_EQ(st, YEPTRIS_OK);
}

TEST(BulkBuild, QuotedStyle) {
    std::vector<Entry> es = {{1, 3, 0, 3}}; /* double-quoted "yes" */
    YeptrisStatus st = YEPTRIS_OK;
    EXPECT_EQ(build_dump(es, "yes", &st), "\"yes\"\n");
    ASSERT_EQ(st, YEPTRIS_OK);
}

TEST(BulkBuild, ImbalanceRejected) {
    YeptrisStatus st = YEPTRIS_OK;
    /* END with nothing open */
    EXPECT_EQ(build_dump({{4, 0, 0, 0}}, "", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    /* entries after the root closed */
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{1, 1, 0, 1}, {1, 1, 1, 1}}, "ab", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    /* an unclosed container */
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{2, 0, 0, 0}}, "", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    /* a key without its value */
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{3, 0, 0, 0}, {1, 1, 0, 1}, {4, 0, 0, 0}}, "k", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    /* duplicate keys: kept like the parser keeps them (both pairs
     * serialize; map_get stays first-wins) — the check-free pairing
     * exists because map_add's linear dup scan was O(n^2) on wide
     * maps */
    st = YEPTRIS_OK;
    EXPECT_EQ(
        build_dump(
            {{3, 0, 0, 0}, {1, 1, 0, 1}, {1, 1, 1, 1}, {1, 1, 0, 1}, {1, 1, 1, 1}, {4, 0, 0, 0}},
            "k12", &st),
        "k: 1\nk: 1\n");
    EXPECT_EQ(st, YEPTRIS_OK);
    /* out-of-range slice */
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{1, 1, 2, 5}}, "ab", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    /* unknown op */
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{9, 0, 0, 0}}, "", &st), "");
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
}

TEST(BulkBuild, EmptyContainersAndDeepNesting) {
    YeptrisStatus st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{2, 0, 0, 0}, {4, 0, 0, 0}}, "", &st), "[]\n");
    ASSERT_EQ(st, YEPTRIS_OK);
    st = YEPTRIS_OK;
    EXPECT_EQ(build_dump({{3, 0, 0, 0}, {4, 0, 0, 0}}, "", &st), "{}\n");
    ASSERT_EQ(st, YEPTRIS_OK);

    /* depth: 999 nested sequences must build, 1001 must reject */
    std::vector<Entry> deep;
    for (int i = 0; i < 999; i++) {
        deep.push_back({2, 0, 0, 0});
    }
    deep.push_back({1, 1, 0, 1});
    for (int i = 0; i < 999; i++) {
        deep.push_back({4, 0, 0, 0});
    }
    st = YEPTRIS_OK;
    std::string out = build_dump(deep, "x", &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_NE(out.find("x"), std::string::npos);

    std::vector<Entry> over;
    for (int i = 0; i < 1001; i++) {
        over.push_back({2, 0, 0, 0});
    }
    st = YEPTRIS_OK;
    build_dump(over, "", &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
}
