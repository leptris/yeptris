/* test_parse.cpp — end-to-end parse smoke tests (TODO.impl/07/09/11).
 * Representative documents → public API queries; errors → status + line.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>

namespace {

std::string val(YeptrisNode n) {
    size_t len = 0;
    const char* p = yeptris_node_value(n, &len);
    return p ? std::string(p, len) : std::string();
}

/* First value for a mapping key (shortcut). */
std::string map_str(YeptrisNode map, const char* key) {
    YeptrisNode v = yeptris_node_map_get(map, key, strlen(key));
    return val(v);
}

} // namespace

TEST(Parse, ScalarRoot) {
    const char* y = "hello";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(yeptris_document_count(doc), 1u);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_kind(root), YEPTRIS_NODE_SCALAR);
    EXPECT_EQ(val(root), "hello");
    EXPECT_EQ(yeptris_node_style(root), YEPTRIS_STYLE_PLAIN);
    yeptris_document_free(doc);
}

TEST(Parse, BlockMapping) {
    const char* y = "name: yeptris\nlang: c\nfast: true";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_kind(root), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(yeptris_node_map_count(root), 3u);
    EXPECT_EQ(map_str(root, "name"), "yeptris");
    EXPECT_EQ(map_str(root, "lang"), "c");
    EXPECT_EQ(map_str(root, "fast"), "true");
    yeptris_document_free(doc);
}

TEST(Parse, NestedBlock) {
    const char* y = "outer:\n"
                    "  middle:\n"
                    "    inner: 1\n"
                    "  sibling: 2\n"
                    "after: 3\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_map_count(root), 2u);
    YeptrisNode outer = yeptris_node_map_get(root, "outer", 5);
    ASSERT_NE(outer, nullptr);
    EXPECT_EQ(yeptris_node_kind(outer), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(map_str(root, "after"), "3");
    YeptrisNode middle = yeptris_node_map_get(outer, "middle", 6);
    ASSERT_NE(middle, nullptr);
    EXPECT_EQ(map_str(middle, "inner"), "1");
    EXPECT_EQ(map_str(outer, "sibling"), "2");
    yeptris_document_free(doc);
}

TEST(Parse, SequencesAndCompact) {
    const char* y = "- one\n"
                    "- two\n"
                    "-\n"
                    "  - nested\n"
                    "  - items\n"
                    "- key: value\n"
                    "  k2: v2\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_kind(root), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_seq_count(root), 4u);
    EXPECT_EQ(val(yeptris_node_seq_at(root, 0)), "one");
    EXPECT_EQ(val(yeptris_node_seq_at(root, 1)), "two");
    YeptrisNode nested = yeptris_node_seq_at(root, 2);
    ASSERT_NE(nested, nullptr);
    EXPECT_EQ(yeptris_node_kind(nested), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_seq_count(nested), 2u);
    EXPECT_EQ(val(yeptris_node_seq_at(nested, 1)), "items");
    YeptrisNode compact = yeptris_node_seq_at(root, 3);
    ASSERT_NE(compact, nullptr);
    EXPECT_EQ(yeptris_node_kind(compact), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(map_str(compact, "key"), "value");
    EXPECT_EQ(map_str(compact, "k2"), "v2");
    yeptris_document_free(doc);
}

TEST(Parse, IndentlessSequenceValue) {
    const char* y = "key:\n"
                    "- a\n"
                    "- b\n"
                    "other: 1\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_map_count(root), 2u);
    YeptrisNode seq = yeptris_node_map_get(root, "key", 3);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(yeptris_node_kind(seq), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_seq_count(seq), 2u);
    EXPECT_EQ(val(yeptris_node_seq_at(seq, 0)), "a");
    EXPECT_EQ(map_str(root, "other"), "1");
    yeptris_document_free(doc);
}

TEST(Parse, FlowCollections) {
    const char* y = "list: [1, two, \"three\", [4, 5]]\n"
                    "map: {a: 1, b: [x, y], c: {d: 2}}\n"
                    "empty_list: []\n"
                    "empty_map: {}\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_map_count(root), 4u);

    YeptrisNode list = yeptris_node_map_get(root, "list", 4);
    ASSERT_NE(list, nullptr);
    EXPECT_EQ(yeptris_node_kind(list), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_seq_count(list), 4u);
    EXPECT_EQ(val(yeptris_node_seq_at(list, 0)), "1");
    EXPECT_EQ(val(yeptris_node_seq_at(list, 1)), "two");
    EXPECT_EQ(val(yeptris_node_seq_at(list, 2)), "three");
    YeptrisNode inner = yeptris_node_seq_at(list, 3);
    ASSERT_NE(inner, nullptr);
    EXPECT_EQ(yeptris_node_seq_count(inner), 2u);
    EXPECT_EQ(val(yeptris_node_seq_at(inner, 0)), "4");

    YeptrisNode map = yeptris_node_map_get(root, "map", 3);
    ASSERT_NE(map, nullptr);
    EXPECT_EQ(yeptris_node_kind(map), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(yeptris_node_map_count(map), 3u);
    EXPECT_EQ(map_str(map, "a"), "1");
    YeptrisNode bseq = yeptris_node_map_get(map, "b", 1);
    ASSERT_NE(bseq, nullptr);
    EXPECT_EQ(yeptris_node_seq_count(bseq), 2u);
    EXPECT_EQ(val(yeptris_node_seq_at(bseq, 1)), "y");
    YeptrisNode cmap = yeptris_node_map_get(map, "c", 1);
    ASSERT_NE(cmap, nullptr);
    EXPECT_EQ(map_str(cmap, "d"), "2");

    YeptrisNode el = yeptris_node_map_get(root, "empty_list", 10);
    ASSERT_NE(el, nullptr);
    EXPECT_EQ(yeptris_node_seq_count(el), 0u);
    YeptrisNode em = yeptris_node_map_get(root, "empty_map", 9);
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(yeptris_node_map_count(em), 0u);
    yeptris_document_free(doc);
}

TEST(Parse, QuotedScalars) {
    const char* y = "single: 'it''s here'\n"
                    "double: \"line\\nbreak\\ttab\"\n"
                    "unicode: \"\\x41\\u00e9\"\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(map_str(root, "single"), "it's here");
    std::string d = map_str(root, "double");
    EXPECT_EQ(d, std::string("line\nbreak\ttab"));
    EXPECT_EQ(map_str(root, "unicode"), "Aé");
    yeptris_document_free(doc);
}

TEST(Parse, PlainMultilineFold) {
    const char* y = "key: this is\n"
                    "  a folded\n"
                    "  plain scalar\n"
                    "other: 1\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(map_str(root, "key"), "this is a folded plain scalar");
    EXPECT_EQ(map_str(root, "other"), "1");
    yeptris_document_free(doc);
}

TEST(Parse, BlockScalars) {
    const char* y = "lit: |\n"
                    "  line one\n"
                    "  line two\n"
                    "folded: >\n"
                    "  folds\n"
                    "  into one\n"
                    "strip: |-\n"
                    "  no newline\n"
                    "keep: |+\n"
                    "  keep\n"
                    "\n"
                    "\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(map_str(root, "lit"), "line one\nline two\n");
    EXPECT_EQ(map_str(root, "folded"), "folds into one\n");
    EXPECT_EQ(map_str(root, "strip"), "no newline");
    EXPECT_EQ(map_str(root, "keep"), "keep\n\n\n");
    yeptris_document_free(doc);
}

TEST(Parse, AnchorsAndAliases) {
    const char* y = "base: &b\n"
                    "  x: 1\n"
                    "same: *b\n"
                    "scalar_anchor: &s hello\n"
                    "ref: *s\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    YeptrisNode base = yeptris_node_map_get(root, "base", 4);
    ASSERT_NE(base, nullptr);
    EXPECT_EQ(yeptris_node_kind(base), YEPTRIS_NODE_MAPPING);
    size_t alen = 0;
    const char* a = yeptris_node_anchor(base, &alen);
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(std::string(a, alen), "b");
    YeptrisNode same = yeptris_node_map_get(root, "same", 4);
    ASSERT_NE(same, nullptr);
    EXPECT_EQ(yeptris_node_kind(same), YEPTRIS_NODE_ALIAS);
    YeptrisNode target = yeptris_node_alias_target(same);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(yeptris_node_kind(target), YEPTRIS_NODE_MAPPING);
    /* *s aliases a scalar: the value comes from the target. */
    YeptrisNode ref = yeptris_node_map_get(root, "ref", 3);
    ASSERT_NE(ref, nullptr);
    EXPECT_EQ(yeptris_node_kind(ref), YEPTRIS_NODE_ALIAS);
    YeptrisNode starget = yeptris_node_alias_target(ref);
    ASSERT_NE(starget, nullptr);
    EXPECT_EQ(val(starget), "hello");
    yeptris_document_free(doc);
}

TEST(Parse, Tags) {
    const char* y = "a: !!str 123\nb: !custom v\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    YeptrisNode a = yeptris_node_map_get(root, "a", 1);
    ASSERT_NE(a, nullptr);
    size_t tlen = 0;
    const char* t = yeptris_node_tag(a, &tlen);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(std::string(t, tlen), "!!str");
    EXPECT_EQ(val(a), "123");
    YeptrisNode b = yeptris_node_map_get(root, "b", 1);
    t = yeptris_node_tag(b, &tlen);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(std::string(t, tlen), "!custom");
    yeptris_document_free(doc);
}

TEST(Parse, MultipleDocuments) {
    const char* y = "---\nfirst: 1\n---\nsecond: 2\n...\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    EXPECT_EQ(yeptris_document_count(doc), 2u);
    YeptrisNode r0 = yeptris_document_root(doc, 0);
    ASSERT_NE(r0, nullptr);
    EXPECT_EQ(map_str(r0, "first"), "1");
    YeptrisNode r1 = yeptris_document_root(doc, 1);
    ASSERT_NE(r1, nullptr);
    EXPECT_EQ(map_str(r1, "second"), "2");
    yeptris_document_free(doc);
}

TEST(Parse, CommentsAndBlanks) {
    const char* y = "# leading comment\n"
                    "\n"
                    "key: value # trailing\n"
                    "\n"
                    "# another\n"
                    "next: 2\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_map_count(root), 2u);
    EXPECT_EQ(map_str(root, "key"), "value");
    EXPECT_EQ(map_str(root, "next"), "2");
    yeptris_document_free(doc);
}

TEST(Parse, NullValues) {
    const char* y = "empty:\nother: ~\nnul: null\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(map_str(root, "empty"), "");
    EXPECT_EQ(map_str(root, "other"), "~");
    EXPECT_EQ(map_str(root, "nul"), "null");
    yeptris_document_free(doc);
}

TEST(Parse, Errors) {
    struct {
        const char* y;
        YeptrisStatus want;
    } cases[] = {
        {"key: [1, 2", YEPTRIS_ERROR_PARSE},          /* unterminated flow */
        {"key: \"unterminated", YEPTRIS_ERROR_PARSE}, /* unterminated quote */
        {"bad: *missing", YEPTRIS_ERROR_PARSE},       /* undefined alias */
        {"a: 1\n\tb: 2\n", YEPTRIS_ERROR_PARSE},      /* tab indent */
        {"[1,]", YEPTRIS_ERROR_PARSE},                /* trailing comma */
        {"a: b: c", YEPTRIS_ERROR_PARSE},             /* mapping values not allowed */
    };
    for (const auto& c : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(c.y, strlen(c.y), &st);
        EXPECT_EQ(doc, nullptr) << "input: " << c.y;
        EXPECT_EQ(st, c.want) << "input: " << c.y;
        if (doc == nullptr) {
            uint32_t line = 0, col = 0;
            const char* msg = yeptris_last_error(&line, &col);
            EXPECT_STRNE(msg, "") << "input: " << c.y;
            EXPECT_GE(line, 1u) << "error position expected: " << c.y;
        }
        yeptris_document_free(doc);
    }
}

TEST(Parse, EmptyAndWhitespace) {
    for (const char* y : {"", "\n", "   \n", "# only a comment\n"}) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        EXPECT_EQ(doc, nullptr) << "input: [" << y << "]";
        EXPECT_EQ(st, YEPTRIS_OK) << "empty input is not an error: [" << y << "]";
    }
}

TEST(Parse, DeepNestingGuard) {
    std::string y;
    for (int i = 0; i < 1200; i++) {
        y += "a:\n";
        for (int j = 0; j <= i; j++) {
            y += " ";
        }
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y.c_str(), y.size(), &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
}
