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
