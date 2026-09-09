/* test_line_shape.cpp — the one-pass line classifier (TODO.restructure/49).
 *
 * Part 1 pins the scan-side FACTS (kind / value class / spans) for the
 * target shapes and every bail shape. Part 2 parses the classified
 * shapes through the public API and asserts the built trees — the fast
 * arms must be event-path-identical.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>

#include "scan/scan.h"

namespace {

struct Shape {
    yep_line_info li;
    yep_line_shape sh;
};

Shape shape_of(const std::string& line) {
    Shape s;
    s.li = yep_scan_line(line.data(), line.size(), 0);
    yep_scan_shape(line.data(), line.size(), &s.li, &s.sh);
    return s;
}

std::string span(const std::string& p, uint32_t a, uint32_t b) {
    return p.substr(a, b - a);
}

std::string val(YeptrisNode n) {
    size_t len = 0;
    const char* p = yeptris_node_value(n, &len);
    return p ? std::string(p, len) : std::string();
}

std::string map_str(YeptrisNode map, const char* key) {
    YeptrisNode v = yeptris_node_map_get(map, key, strlen(key));
    return val(v);
}

struct DocHolder {
    YeptrisDocument doc = nullptr;
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisNode root = nullptr;
    explicit DocHolder(const char* y) {
        doc = yeptris_parse(y, strlen(y), &st);
        root = doc ? yeptris_document_root(doc, 0) : nullptr;
    }
    ~DocHolder() {
        if (doc) {
            yeptris_document_free(doc);
        }
    }
};

/* -------- part 1: classifier facts -------- */

TEST(LineShape, KeyPlain) {
    std::string p = "key: word";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(s.sh.val, YEP_LVAL_PLAIN);
    EXPECT_EQ(span(p, s.sh.key_start, s.sh.key_end), "key");
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "word");
    EXPECT_EQ(s.sh.val_start, 5u);
}

TEST(LineShape, KeyColonInValue) {
    std::string p = "k: a:b"; // ':' not followed by blank stays in the scalar
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(s.sh.val, YEP_LVAL_PLAIN);
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "a:b");
}

TEST(LineShape, KeyEmptyValueVariants) {
    for (std::string p : {"k:", "k: ", "k:\t", "k: # note"}) {
        Shape s = shape_of(p);
        EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY) << p;
        EXPECT_EQ(s.sh.val, YEP_LVAL_EMPTY) << p;
    }
}

TEST(LineShape, KeyAlias) {
    std::string p = "  <<: *def0";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(s.sh.val, YEP_LVAL_ALIAS);
    EXPECT_EQ(span(p, s.sh.key_start, s.sh.key_end), "<<");
    EXPECT_EQ(s.sh.val_start, 6u);
}

TEST(LineShape, KeyAnchorPlain) {
    std::string p = "  x: &x0 word";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(s.sh.val, YEP_LVAL_ANCHOR_PLAIN);
    EXPECT_EQ(s.sh.val_start, 5u);
    EXPECT_EQ(span(p, s.sh.val_start + 1, s.sh.anchor_end), "x0");
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "word");
}

TEST(LineShape, KeyFlowOpeners) {
    for (std::string p : {"k: [1, 2]", "k: {\"a\": 1}"}) {
        Shape s = shape_of(p);
        EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY) << p;
        EXPECT_EQ(s.sh.val, YEP_LVAL_FLOW) << p;
    }
}

TEST(LineShape, KeySpacesBeforeColon) {
    std::string p = "key   : word";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(span(p, s.sh.key_start, s.sh.key_end), "key");
    EXPECT_EQ(p[s.sh.colon], ':');
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "word");
}

TEST(LineShape, KeyTrailingComment) {
    std::string p = "k: word # note";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.val, YEP_LVAL_PLAIN);
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "word");
    EXPECT_EQ(s.sh.val_span.term, YEP_TERM_COMMENT);
}

TEST(LineShape, KeyCrlf) {
    std::string p = "k: word\r\n";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(s.sh.val, YEP_LVAL_PLAIN);
    EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), "word");
}

TEST(LineShape, DashShapes) {
    EXPECT_EQ(shape_of("- word").sh.val, YEP_LVAL_PLAIN);
    EXPECT_EQ(shape_of("- {\"a\": 1}").sh.val, YEP_LVAL_FLOW);
    EXPECT_EQ(shape_of("- *a").sh.val, YEP_LVAL_ALIAS);
    EXPECT_EQ(shape_of("- ").sh.val, YEP_LVAL_EMPTY);
    EXPECT_EQ(shape_of("-").sh.val, YEP_LVAL_EMPTY);
    std::string p = "- {\"a\": 1}";
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_DASH);
    EXPECT_EQ(p[s.sh.dash], '-');
    EXPECT_EQ(s.sh.val_start, 2u);
}

namespace {

/* The engine's fast arm fires iff scan produced a classified shape
 * with a value class that arm owns (KEY: all classes; DASH: everything
 * but anchored entries). */
bool fastable(const Shape& s) {
    switch (s.sh.kind) {
    case YEP_LSHAPE_KEY:
        return s.sh.val != YEP_LVAL_NONE;
    case YEP_LSHAPE_DASH:
        return s.sh.val == YEP_LVAL_EMPTY || s.sh.val == YEP_LVAL_PLAIN ||
               s.sh.val == YEP_LVAL_ALIAS || s.sh.val == YEP_LVAL_FLOW;
    default:
        return false;
    }
}

} // namespace

TEST(LineShape, BailShapes) {
    // whole classes that must stay on the general path
    for (std::string p : {
             "",            // blank
             "   ",         // blank
             "# comment",   // comment
             "---",         // doc start
             "...",         // doc end
             "%TAG",        // directive
             "\t- x",       // tab in indent
             "word1",       // bare plain scalar
             "word1 # c",   // bare plain + comment
             "k:v",         // ':' not followed by blank: a scalar
             "k:#c",        // ditto
             "? k: v",      // explicit key
             "\"k\": v",    // quoted key
             "'k': v",      // quoted key
             "&a k: v",     // anchored key
             "!t k: v",     // tagged key
             "[1]: v",      // flow key
             "{a: 1}: v",   // flow key
             "*a: v",       // alias key
             ": v",         // empty-key line (main loop arm)
             "k: \"q\"",    // quoted value
             "k: 'q'",      // quoted value
             "k: !t v",     // tagged value
             "k: |",        // literal value
             "k: >",        // folded value
             "k: &a",       // anchor, value on following lines
             "k: &a [1]",   // anchor + flow
             "k: &a \"q\"", // anchor + quoted
             "k: &b *a",    // anchor + alias: error shape (SR86)
             "k: - x",      // error shape: dash as value
             "k: ? x",      // error shape: '?' as value
             "k: ]",        // non-plain-first value
             "k: v: w",     // second terminating colon (baseline: error)
             "- k: v",      // compact mapping entry
             "- - x",       // nested sequence entry
             "- ? k",       // explicit key in entry (baseline: parses)
             "- &a v",      // anchored entry
             "- \"q\"",     // quoted entry
         }) {
        Shape s = shape_of(p);
        EXPECT_FALSE(fastable(s)) << p;
    }
}

/* Plain keys carrying indicator bytes: the scan walk already owns
 * these rules (baseline: all parse as mappings). */
TEST(LineShape, PlainKeysWithIndicators) {
    for (std::string p : {"-x: v", "?x: v", ":x: v"}) {
        Shape s = shape_of(p);
        EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY) << p;
        EXPECT_TRUE(fastable(s)) << p;
    }
    { // ':' followed by a non-blank stays in the value (baseline: parses)
        std::string p = "k: :x";
        Shape s = shape_of(p);
        EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
        EXPECT_EQ(s.sh.val, YEP_LVAL_PLAIN);
        EXPECT_EQ(span(p, s.sh.val_span.start, s.sh.val_span.end), ":x");
    }

    std::string p = "a#b: word"; // '#' not after a blank stays in the key
    Shape s = shape_of(p);
    EXPECT_EQ(s.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(span(p, s.sh.key_start, s.sh.key_end), "a#b");

    std::string q = "http://x: word"; // ':' non-terminating inside the key
    Shape t = shape_of(q);
    EXPECT_EQ(t.sh.kind, YEP_LSHAPE_KEY);
    EXPECT_EQ(span(q, t.sh.key_start, t.sh.key_end), "http://x");
}

/* -------- part 2: fast arms build identical trees -------- */

TEST(LineShapeArms, KeyEmptyThenBlock) {
    DocHolder d("defaults: &def0\n  a: 1\n  b: 2\n");
    ASSERT_NE(d.root, nullptr) << yeptris_last_error(NULL, NULL);
    EXPECT_EQ(yeptris_node_kind(d.root), YEPTRIS_NODE_MAPPING);
    YeptrisNode def = yeptris_node_map_get(d.root, "defaults", 8);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(yeptris_node_kind(def), YEPTRIS_NODE_MAPPING); // anchor on the nested map
    EXPECT_EQ(map_str(def, "a"), "1");
    EXPECT_EQ(map_str(def, "b"), "2");
}

TEST(LineShapeArms, MergeAliasValue) {
    DocHolder d("defaults: &def0\n  a: 1\nuse:\n  <<: *def0\n  b: 2\n");
    ASSERT_NE(d.root, nullptr);
    YeptrisNode use = yeptris_node_map_get(d.root, "use", 3);
    ASSERT_NE(use, nullptr);
    /* the C DOM keeps the merge pair literal (bindings materialize) */
    YeptrisNode merge = yeptris_node_map_get(use, "<<", 2);
    ASSERT_NE(merge, nullptr);
    ASSERT_EQ(yeptris_node_kind(merge), YEPTRIS_NODE_ALIAS);
    YeptrisNode target = yeptris_node_alias_target(merge);
    ASSERT_NE(target, nullptr);
    EXPECT_EQ(yeptris_node_kind(target), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(map_str(target, "a"), "1");
    EXPECT_EQ(map_str(use, "b"), "2");
}

TEST(LineShapeArms, AnchorPlainValue) {
    DocHolder d("x: &x0 word\ny: *x0\n");
    ASSERT_NE(d.root, nullptr);
    EXPECT_EQ(map_str(d.root, "x"), "word");
    YeptrisNode y = yeptris_node_map_get(d.root, "y", 1);
    ASSERT_NE(y, nullptr);
    EXPECT_EQ(yeptris_node_kind(y), YEPTRIS_NODE_ALIAS);
    EXPECT_EQ(val(y), "x0");
    YeptrisNode t = yeptris_node_alias_target(y);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(val(t), "word");
}

TEST(LineShapeArms, PlainValueFoldContinues) {
    DocHolder d("k: word\n  more words\nz: 1\n");
    ASSERT_NE(d.root, nullptr) << yeptris_last_error(NULL, NULL);
    EXPECT_EQ(map_str(d.root, "k"), "word more words");
    EXPECT_EQ(map_str(d.root, "z"), "1");
}

TEST(LineShapeArms, DashFlowEntry) {
    DocHolder d("- {\"id\": 7, \"ok\": true}\n- {\"id\": 8, \"ok\": false}\n");
    ASSERT_NE(d.root, nullptr);
    ASSERT_EQ(yeptris_node_kind(d.root), YEPTRIS_NODE_SEQUENCE);
    ASSERT_EQ(yeptris_node_seq_count(d.root), 2u);
    YeptrisNode e0 = yeptris_node_seq_at(d.root, 0);
    ASSERT_NE(e0, nullptr);
    EXPECT_EQ(map_str(e0, "id"), "7");
    EXPECT_EQ(map_str(e0, "ok"), "true");
}

TEST(LineShapeArms, DashPlainAndAliasEntries) {
    DocHolder d("- &w word\n- *w\n- 3\n");
    ASSERT_NE(d.root, nullptr);
    ASSERT_EQ(yeptris_node_seq_count(d.root), 3u);
    EXPECT_EQ(map_str(d.root, "__none__"), "");
    YeptrisNode e0 = yeptris_node_seq_at(d.root, 0);
    ASSERT_NE(e0, nullptr);
    EXPECT_EQ(val(e0), "word");
    YeptrisNode e1 = yeptris_node_seq_at(d.root, 1);
    ASSERT_NE(e1, nullptr);
    EXPECT_EQ(yeptris_node_kind(e1), YEPTRIS_NODE_ALIAS);
}

TEST(LineShapeArms, DeepNestKeys) {
    DocHolder d("l0:\n  l1:\n    l2: leaf\n");
    ASSERT_NE(d.root, nullptr);
    YeptrisNode l1 = yeptris_node_map_get(d.root, "l0", 2);
    ASSERT_NE(l1, nullptr);
    YeptrisNode l2 = yeptris_node_map_get(l1, "l1", 2);
    ASSERT_NE(l2, nullptr);
    EXPECT_EQ(map_str(l2, "l2"), "leaf");
}

TEST(LineShapeArms, ErrorShapesPreserved) {
    struct Case {
        const char* y;
        YeptrisStatus st;
    } cases[] = {
        {"k: - x\n", YEPTRIS_ERROR_PARSE},  // dash as value
        {"k: ? x\n", YEPTRIS_ERROR_PARSE},  // '?' as value
        {"k: v: w\n", YEPTRIS_ERROR_PARSE}, // second colon (baseline: error)
    };
    for (const Case& c : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(c.y, strlen(c.y), &st);
        EXPECT_EQ(st, c.st) << c.y;
        if (doc) {
            yeptris_document_free(doc);
        }
    }
}

TEST(LineShapeArms, QuotedAndTaggedStillParse) {
    DocHolder d("k: \"q\"\nt: !str word\nb: |\n  line\n");
    ASSERT_NE(d.root, nullptr) << yeptris_last_error(NULL, NULL);
    EXPECT_EQ(map_str(d.root, "k"), "q");
    EXPECT_EQ(map_str(d.root, "t"), "word");
    EXPECT_EQ(map_str(d.root, "b"), "line\n");
}

} // namespace
