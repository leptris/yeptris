/* test_parse.cpp — end-to-end parse smoke tests (TODO.impl/07/09/11).
 * Representative documents → public API queries; errors → status + line.
 */

#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include <yeptris.h>

#include "common/chartype.h"
#include "common/simd_text.h"
#include "parse/engine.h"
#include "scan/scan.h"
#include <yeptris/json.h>

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

TEST(Parse, FlowPropsAtEofExactBuffer) {
    /* the nightly fuzz_parse crasher (2026-10-08): flow properties
     * consumed to EOF, then the node read ran one byte past the
     * input. Pinned with an EXACT-SIZE heap buffer (no NUL slack) so
     * the ASAN build sees any regression of the bound. */
    const char bytes[8] = {'[', '!', '<', '<', '\r', '\n', '!', ']'};
    char* y = (char*)malloc(sizeof bytes);
    ASSERT_NE(y, nullptr);
    memcpy(y, bytes, sizeof bytes);
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, sizeof bytes, &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    free(y);
}

TEST(Parse, RunnerRejectsIndicatorFirstPlain) {
    /* the nightly fuzz_roundtrip trap (2026-10-08), "u: }": the fused
     * block runner accepted plain scalars opening with a reserved
     * indicator (c-indicator set) while the engine chain, the emitter,
     * Psych and PyYAML all reject. Every form must fail cleanly. */
    const char* bad[] = {"u: }", "u: ]", "u: ,", "u: @x", "u: `x", "u: %x", "k: }", "}: v", "- }"};
    for (const char* y : bad) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        EXPECT_EQ(doc, nullptr) << y;
        EXPECT_NE(st, YEPTRIS_OK) << y;
        if (doc != NULL) {
            yeptris_document_free(doc);
        }
    }
}

TEST(Parse, FlowQuoteEscapeAtEofExactBuffer) {
    /* the nightly fuzz_parse crasher (2026-10-08): a flow double-quoted
     * scalar whose final input byte is a lone backslash drove the AVX2
     * quote-scan tail past the buffer end. Exact-size heap buffer (no
     * NUL slack) so ASAN builds see any regression of the bound; the
     * parse must reject cleanly. */
    static const unsigned char bytes[] = {
        0x5b, 0x22, 0x5c, 0x75, 0x30, 0x79, 0x3a, 0x20, 0x2d, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c,
        0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x30, 0x36, 0x30, 0x68, 0x68, 0x68, 0x36, 0x30,
        0x68, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x68, 0x68,
        0x68, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x2c, 0x20, 0x68, 0x68,
        0x68, 0x68, 0x68, 0x30, 0x31, 0x62, 0x0a, 0x32, 0x60, 0x5c};
    char* y = (char*)malloc(sizeof bytes);
    ASSERT_NE(y, nullptr);
    memcpy(y, bytes, sizeof bytes);
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, sizeof bytes, &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_NE(st, YEPTRIS_OK);
    free(y);
}

TEST(Parse, AnchorNeedsName) {
    /* the nightly fuzz_roundtrip trap (2026-10-08), input "&": an
     * anchor with an EMPTY name was accepted (root, value, flow), the
     * serializer emitted an empty document, and its re-parse returned
     * NULL — parseable-implies-marshalable broken. libyaml/Psych
     * reject all of these ("did not find expected alphabetic or
     * numeric character"); a bare '!' tag stays legal. */
    const char* bad[] = {"&", "&\n", "& x", "k: & v", "- & x", "k: &\n", "[&]", "[& ]", "{a: &}"};
    for (const char* y : bad) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        EXPECT_EQ(doc, nullptr) << y;
        EXPECT_NE(st, YEPTRIS_OK) << y;
        if (doc != NULL) {
            yeptris_document_free(doc);
        }
    }
    /* the legal forms still round-trip: serialize then re-parse */
    const char* ok[] = {"!", "&a", "k: &a v", "[&a]", "a: &b c"};
    for (const char* y : ok) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        ASSERT_NE(doc, nullptr) << y;
        size_t l1 = 0;
        char* s1 = yeptris_serialize(doc, &l1);
        ASSERT_NE(s1, nullptr) << y;
        YeptrisStatus st2 = YEPTRIS_OK;
        YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
        EXPECT_NE(doc2, nullptr) << y << " -> " << s1;
        if (doc2 != NULL) {
            size_t l2 = 0;
            char* s2 = yeptris_serialize(doc2, &l2);
            EXPECT_TRUE(s2 == NULL || (l1 == l2 && memcmp(s1, s2, l1) == 0)) << y;
            free(s2);
            yeptris_document_free(doc2);
        }
        free(s1);
        yeptris_document_free(doc);
    }
}

TEST(Parse, VerbatimTagClosesSameLine) {
    /* the nightly fuzz_roundtrip byte-instability trap (2026-10-08),
     * input "!<": an unterminated/empty/spaced verbatim tag was
     * accepted, then serialize/re-parse grew a blank line per round.
     * libyaml requires the '>' to close on the same line with no
     * interior space and non-empty content. */
    const char* bad[] = {"!<", "!<\n", "!<>", "!<a", "k: !<", "!<a b>", "[!<]"};
    for (const char* y : bad) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        EXPECT_EQ(doc, nullptr) << y;
        EXPECT_NE(st, YEPTRIS_OK) << y;
        if (doc != NULL) {
            yeptris_document_free(doc);
        }
    }
    const char* ok[] = {"!<a>", "!<a> v", "!", "!foo v", "!!str v", "a: !!int 1"};
    for (const char* y : ok) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        ASSERT_NE(doc, nullptr) << y;
        size_t l1 = 0;
        char* s1 = yeptris_serialize(doc, &l1);
        ASSERT_NE(s1, nullptr) << y;
        YeptrisStatus st2 = YEPTRIS_OK;
        YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
        ASSERT_NE(doc2, nullptr) << y << " -> " << s1;
        size_t l2 = 0;
        char* s2 = yeptris_serialize(doc2, &l2);
        EXPECT_TRUE(s2 != NULL && l1 == l2 && memcmp(s1, s2, l1) == 0) << y;
        free(s2);
        yeptris_document_free(doc2);
        free(s1);
        yeptris_document_free(doc);
    }
}

TEST(Parse, FlowNullStyleRoundTrips) {
    /* the nightly fuzz_roundtrip byte-instability trap (2026-10-08),
     * input "? - :": a synthesized flow null carried style ANY, so the
     * emitter chose double-quotes and each serialize/re-parse round
     * drifted ("{"k": }" -> "{"k": ""}"). Synthesized nulls are PLAIN:
     * Psych resolves every one of these to nil. */
    const char* ins[] = {"{\"k\": }", "{\"k\":}", "{\"k\": , \"j\": v}",
                         "? - :",     "[a, ]",    "{a: , b: 1}"};
    for (const char* y : ins) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        ASSERT_NE(doc, nullptr) << y;
        size_t l1 = 0;
        char* s1 = yeptris_serialize(doc, &l1);
        ASSERT_NE(s1, nullptr) << y;
        YeptrisStatus st2 = YEPTRIS_OK;
        YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
        ASSERT_NE(doc2, nullptr) << y << " -> " << s1;
        size_t l2 = 0;
        char* s2 = yeptris_serialize(doc2, &l2);
        EXPECT_TRUE(s2 != NULL && l1 == l2 && memcmp(s1, s2, l1) == 0) << y;
        free(s2);
        yeptris_document_free(doc2);
        free(s1);
        yeptris_document_free(doc);
    }
}

TEST(Parse, Utf16RoundTripThroughContinuationByte) {
    /* the nightly fuzz_roundtrip re-parse trap (2026-10-08): a
     * UTF-16LE document decoding to U+315B (E3 85 9B). The NEL fix's
     * lone-0x85 escape fired on the CONTINUATION byte, corrupting the
     * sequence into E3 + the TEXT "\\x85" + 9B — un-re-parseable.
     * NEL is the C2 85 PAIR only; a lone 0x85 byte is a continuation
     * byte that must ride untouched. */
    static const unsigned char bytes[] = {0xff, 0xfe, 0x6f, 0x59, 0x26, 0x2b, 0x5b, 0x31};
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse((const char*)bytes, sizeof bytes, &st);
    ASSERT_NE(doc, nullptr);
    size_t l1 = 0;
    char* s1 = yeptris_serialize(doc, &l1);
    ASSERT_NE(s1, nullptr);
    EXPECT_EQ(l1, 10u) << "plain UTF-8, no quoting";
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
    ASSERT_NE(doc2, nullptr) << s1;
    size_t l2 = 0;
    char* s2 = yeptris_serialize(doc2, &l2);
    EXPECT_TRUE(s2 != NULL && l1 == l2 && memcmp(s1, s2, l1) == 0);
    free(s2);
    yeptris_document_free(doc2);
    free(s1);
    yeptris_document_free(doc);
}

TEST(Parse, BomCharEscapesAsUFEFF) {
    /* the nightly fuzz_roundtrip re-parse trap (2026-10-08, run
     * 37833916837): a UTF-32LE document whose content decodes to a
     * lone U+FEFF. Emitted raw, those three bytes ARE the UTF-8 BOM —
     * the re-parse strips them and the document vanishes. Psych
     * escapes U+FEFF everywhere: "\\uFEFF". */
    static const unsigned char bytes[] = {0xff, 0xfe, 0x00, 0x00, 0xff, 0xfe, 0x00, 0x00};
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse((const char*)bytes, sizeof bytes, &st);
    ASSERT_NE(doc, nullptr);
    size_t l1 = 0;
    char* s1 = yeptris_serialize(doc, &l1);
    ASSERT_NE(s1, nullptr);
    static const char want[] = "\"\\uFEFF\"\n";
    EXPECT_EQ(l1, sizeof want - 1);
    if (l1 == sizeof want - 1) {
        EXPECT_EQ(0, memcmp(s1, want, l1));
    }
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
    ASSERT_NE(doc2, nullptr) << s1;
    size_t l2 = 0;
    char* s2 = yeptris_serialize(doc2, &l2);
    EXPECT_TRUE(s2 != NULL && l1 == l2 && memcmp(s1, s2, l1) == 0);
    free(s2);
    yeptris_document_free(doc2);
    free(s1);
    yeptris_document_free(doc);
}

TEST(Parse, PropsChainRecursionCapped) {
    /* the nightly fuzz_parse STACK OVERFLOW (2026-10-08, run
     * 37839718900): the e_node <-> e_parse_value props/value-line
     * chain opens NO container frame, so e->depth never grew and the
     * C recursion ran unbounded (anchor-only lines at document root).
     * A recursion counter capped at max_depth now rejects with
     * YEPTRIS_ERROR_DEPTH — the depth-guard law: error, never crash. */
    static const char rep[] = "&a\n";
    size_t n = strlen(rep) * 20000;
    char* y = (char*)malloc(n);
    ASSERT_NE(y, nullptr);
    for (size_t i = 0; i < 20000; i++) {
        memcpy(y + i * strlen(rep), rep, strlen(rep));
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, n, &st);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_DEPTH);
    free(y);
    /* ordinary nesting still parses: the cap rides the same 1000 */
    static const char nested[] = "a:\n";
    size_t m = strlen(nested) * 50;
    char* z = (char*)malloc(m + 2);
    ASSERT_NE(z, nullptr);
    for (size_t i = 0; i < 50; i++) {
        memcpy(z + i * strlen(nested), nested, strlen(nested));
    }
    z[m] = ' ';
    z[m + 1] = '1';
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse(z, m + 2, &st2);
    EXPECT_NE(doc2, nullptr);
    if (doc2 != NULL) {
        yeptris_document_free(doc2);
    }
    free(z);
}

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

TEST(Parse, PlainStopSetsMatchRuntimeBuild) {
    /* the constants must equal what the old per-call build produced */
    unsigned char b[32], f[32];
    yep_stopset_clear(b);
    yep_stopset_add(b, '\n');
    yep_stopset_add(b, '\r');
    yep_stopset_add(b, ':');
    yep_stopset_add(b, '#');
    yep_stopset_clear(f);
    memcpy(f, b, 32);
    const unsigned char fs[5] = {',', '[', ']', '{', '}'};
    for (int i = 0; i < 5; i++) {
        yep_stopset_add(f, fs[i]);
    }
    yep_stopset ss;
    yep_stopset_init(&ss, b);
    EXPECT_EQ(0, memcmp(&yep_plain_stop_block, &ss, sizeof(ss)));
    yep_stopset_init(&ss, f);
    EXPECT_EQ(0, memcmp(&yep_plain_stop_flow, &ss, sizeof(ss)));

    /* the break set (\n/\r) — the engine's quote path shares it */
    unsigned char brk[32];
    yep_stopset_clear(brk);
    yep_stopset_add(brk, '\n');
    yep_stopset_add(brk, '\r');
    yep_stopset_init(&ss, brk);
    EXPECT_EQ(0, memcmp(&yep_break_stopset, &ss, sizeof(ss)));
}

namespace {
int ref_colon_terminates(const char* p, size_t len, size_t colon, int flow) {
    size_t next = colon + 1;
    if (next >= len) {
        return 1;
    }
    unsigned char c = (unsigned char)p[next];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return 1;
    }
    if (flow && yep_ct_is(c, YEP_CT_FLOW_IND)) {
        return 1;
    }
    return 0;
}

/* The byte-loop scan_plain the SWAR tiny path must match span for span. */
yep_span scan_plain_reference(const char* p, size_t len, size_t pos, int flow) {
    const yep_stopset* stop = flow ? &yep_plain_stop_flow : &yep_plain_stop_block;
    yep_span s;
    s.start = (uint32_t)pos;
    s.end = (uint32_t)pos;
    s.term = YEP_TERM_EOF;
    size_t i = pos;
    while (i < len) {
        size_t at = i;
        while (at < len && !yep_stopset_test(stop->bitmap, (unsigned char)p[at])) {
            at++;
        }
        unsigned char c = (at < len) ? (unsigned char)p[at] : 0;
        if (at == len) {
            i = len;
            s.term = YEP_TERM_EOF;
            break;
        }
        if (c == '\n' || c == '\r') {
            i = at;
            s.term = YEP_TERM_EOL;
            break;
        }
        if (c == ':') {
            if (ref_colon_terminates(p, len, at, flow)) {
                i = at;
                s.term = YEP_TERM_COLON;
                break;
            }
            i = at + 1;
            continue;
        }
        if (c == '#') {
            if (at == s.start || (at > s.start && (p[at - 1] == ' ' || p[at - 1] == '\t'))) {
                i = at;
                s.term = YEP_TERM_COMMENT;
                break;
            }
            i = at + 1;
            continue;
        }
        i = at;
        s.term = YEP_TERM_FLOW;
        break;
    }
    size_t e = i;
    while (e > s.start && (p[e - 1] == ' ' || p[e - 1] == '\t')) {
        e--;
    }
    s.end = (uint32_t)e;
    return s;
}
} /* namespace */

TEST(Parse, ScanPlainSwarTinyMatchesByteReference) {
    uint64_t seed = 0x9E3779B9ull;
    auto next = [&seed]() {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return (uint32_t)(seed >> 33);
    };
    auto check = [&](const std::string& s, size_t pos, int flow) {
        yep_span got = yep_scan_plain(s.data(), s.size(), pos, flow);
        yep_span want = scan_plain_reference(s.data(), s.size(), pos, flow);
        ASSERT_EQ(got.start, want.start) << "pos=" << pos << " flow=" << flow << " s='" << s << "'";
        ASSERT_EQ(got.end, want.end) << "pos=" << pos << " flow=" << flow << " s='" << s << "'";
        ASSERT_EQ(got.term, want.term) << "pos=" << pos << " flow=" << flow << " s='" << s << "'";
    };
    /* exhaustive over a 4-char interaction core (colon/hash/space/lit),
     * lengths crossing the 8-byte word boundary, both stop sets */
    const std::string core = "a:#,";
    for (int flow = 0; flow <= 1; flow++) {
        for (uint32_t len = 0; len <= 24; len++) {
            uint64_t variants = 1ull << (2 * (len > 8 ? 8 : len));
            for (uint64_t v = 0; v < variants; v++) {
                std::string s;
                uint64_t bits = v;
                for (uint32_t j = 0; j < len; j++) {
                    s += core[bits & 3u];
                    bits >>= 2;
                    if (j == 7 && len > 8) {
                        s += core[(v >> 13) & 3u]; /* word-boundary flavors */
                    }
                }
                check(s, 0, flow);
            }
            /* random strings over the full member + filler alphabet */
            const std::string alphabet = "ab: #\n\t\r,[]{}X\x01\x7f";
            for (int r = 0; r < 300; r++) {
                std::string s;
                uint32_t n = 1 + next() % 100;
                for (uint32_t j = 0; j < n; j++) {
                    s += alphabet[next() % alphabet.size()];
                }
                size_t pos = (r % 3 == 0) ? (next() % s.size()) : 0;
                check(s, pos, flow);
            }
        }
    }
    /* directed spans: multi-hit words, mid-token '#', non-terminating ':' */
    check("aaaa:bbbb:cccc\nd", 0, 0);
    check("aaa#bbb: c", 0, 0);
    check("a:b:c ", 0, 0);
    check("k:  v ", 3, 0);
    check("a,b]c{d}e:f", 0, 1);
    check("x:a[b]c{d}e,f g", 1, 1);
    check("trailing spaces   ", 0, 0);
    check("a{b}c", 0, 0); /* flow members do not stop block scans */
}

TEST(Parse, NormBreakLeadsMatchRuntimeBuild) {
    /* the literal must equal what yep_stopset_init builds for {C2, E2}
     * (the Unicode break lead bytes) — a drift would silently skip the
     * NEL/LS/PS normalization on every parse */
    unsigned char b[32];
    yep_stopset_clear(b);
    yep_stopset_add(b, 0xC2);
    yep_stopset_add(b, 0xE2);
    yep_stopset ss;
    yep_stopset_init(&ss, b);
    EXPECT_EQ(0, memcmp(&yep_norm_break_leads, &ss, sizeof(ss)));
}

TEST(Parse, UnicodeBreaksNormalizeLookalikesStay) {
    /* real NEL/LS/PS normalize to a line break (folded plain picks up
     * a space); the lookalike byte runs must ride through untouched */
    /* NEL/LS/PS normalize to a line break BEFORE parsing, so each one
     * below is exactly a '\n' in libyaml terms: an indented plain
     * continuation folds with a space, and a break between pairs
     * creates a second pair. The lookalike byte runs must ride through
     * untouched (the detector's real predicate). */
    struct {
        const char* y;
        const char* want;
    } cases[] = {
        {"k: a\xC2\x85"
         " b\n",
         "a b"}, /* NEL fold */
        {"k: a\xE2\x80\xA8"
         " b\n",
         "a b"}, /* LS fold */
        {"k: a\xE2\x80\xA9"
         " b\n",
         "a b"}, /* PS fold */
        {"k: 012345678\xC2\x85"
         " x\n",
         "012345678 x"},                           /* past the byte loop's word */
        {"k: \xC2\xA0x\n", "\xC2\xA0x"},           /* NBSP: C2 lead, not a break */
        {"k: v\xE2\x80\x93x\n", "v\xE2\x80\x93x"}, /* en dash: E2 80, not A8/A9 */
    };
    for (const auto& c : cases) {
        YeptrisStatus st;
        YeptrisDocument doc = yeptris_parse(c.y, strlen(c.y), &st);
        ASSERT_NE(doc, nullptr) << "input=" << c.y << " err=" << yeptris_last_error(NULL, NULL);
        YeptrisNode root = yeptris_document_root(doc, 0);
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(map_str(root, "k"), c.want) << c.y;
        yeptris_document_free(doc);
    }
    /* the pair-splitting shape: the NEL line break ends the 'k' pair */
    {
        const char* y = "k: v\xC2\x85"
                        "j: w\n";
        YeptrisStatus st;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
        YeptrisNode root = yeptris_document_root(doc, 0);
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(yeptris_node_map_count(root), 2u);
        EXPECT_EQ(map_str(root, "k"), "v");
        EXPECT_EQ(map_str(root, "j"), "w");
        yeptris_document_free(doc);
    }
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

/* A props-only line followed by its node: the pend anchor VIEW and its
 * ordinal id must ride together (same-line-wins) into block sequences,
 * flow collections, and scalars — the id alone reaching the DOM was the
 * self-referencing-structures bug (empty error, found by the Ruby port
 * spec: "--- &id001\n- *id001"). */
TEST(Parse, PendAnchorIdRidesTheView) {
    struct {
        const char* yaml;
        const char* key;
        YeptrisNodeKind kind;
    } cases[] = {
        {"--- &id001\n- *id001\n", NULL, YEPTRIS_NODE_SEQUENCE},
        {"k: &a\n  [b, 1]\nr: *a\n", "k", YEPTRIS_NODE_SEQUENCE},
        {"k: &a\n  {b: 1}\nr: *a\n", "k", YEPTRIS_NODE_MAPPING},
        {"k: &a\n  word\nr: *a\n", "k", YEPTRIS_NODE_SCALAR},
    };
    for (const auto& c : cases) {
        YeptrisStatus st;
        YeptrisDocument doc = yeptris_parse(c.yaml, strlen(c.yaml), &st);
        ASSERT_NE(doc, nullptr) << c.yaml << " -> " << yeptris_last_error(NULL, NULL);
        YeptrisNode root = yeptris_document_root(doc, 0);
        ASSERT_NE(root, nullptr);
        YeptrisNode node = c.key ? yeptris_node_map_get(root, c.key, strlen(c.key)) : root;
        ASSERT_NE(node, nullptr) << c.yaml;
        EXPECT_EQ(yeptris_node_kind(node), c.kind) << c.yaml;
        size_t alen = 0;
        const char* an = yeptris_node_anchor(node, &alen);
        ASSERT_NE(an, nullptr) << c.yaml;
        EXPECT_EQ(std::string(an, alen), c.key ? "a" : "id001") << c.yaml;
        const char* ref_name = c.key ? "r" : NULL;
        YeptrisNode ref = ref_name ? yeptris_node_map_get(root, ref_name, 1) : nullptr;
        if (ref_name) {
            ASSERT_NE(ref, nullptr) << c.yaml;
            EXPECT_EQ(yeptris_node_kind(ref), YEPTRIS_NODE_ALIAS) << c.yaml;
            YeptrisNode target = yeptris_node_alias_target(ref);
            ASSERT_NE(target, nullptr) << c.yaml;
            EXPECT_EQ(yeptris_node_id(target), yeptris_node_id(node)) << c.yaml;
        } else {
            /* the self-referencing sequence: element 0 aliases the root */
            YeptrisNode el = yeptris_node_seq_at(node, 0);
            ASSERT_NE(el, nullptr) << c.yaml;
            EXPECT_EQ(yeptris_node_kind(el), YEPTRIS_NODE_ALIAS) << c.yaml;
            EXPECT_EQ(yeptris_node_id(yeptris_node_alias_target(el)), yeptris_node_id(node))
                << c.yaml;
        }
        yeptris_document_free(doc);
    }
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
    EXPECT_EQ(std::string(t, tlen), "tag:yaml.org,2002:str");
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

/* The block-level flow indent floor (9C9N/VJP3) is a yaml-test-suite
 * rule libyaml never enforced: real-world locale files close multi-line
 * flow collections at their parent's column. The strict 1.2 engine
 * keeps the floor (the conformance suite demands the rejection); the
 * 11-COMPAT surface (the psych/libyaml parity entry every Ruby adapter
 * rides) must accept what libyaml accepts — isodoc's i18n-en.yaml is
 * the real-world breaker (metanorma-standoc CI, 2026-09-26). */
TEST(Parse, CompatGrammarFlowFloorParity) {
    struct {
        const char* y;
        int core_ok;     /* the strict engine's verdict */
        int root_is_map; /* compat: the root node's kind */
    } cases[] = {
        /* close at the parent's column — libyaml accepts, suite rejects */
        {"a: {\n  x: 1\n}\nb: 3\n", 0, 1},
        /* 9C9N: continuation line at the parent's column */
        {"flow: [a,\nb,\nc]", 0, 1},
        /* VJP3#1: every line at column 0 */
        {"k: {\nk\n:\nv\n}", 0, 1},
        /* shapes both engines accept */
        {"a: {x: 1}\n", 1, 1},
        {"a: {x: 1,\n y: 2}\n", 1, 1},
        {"[\n1,\n2\n]", 1, 0},
    };
    for (const auto& c : cases) {
        YeptrisParseOptions opts;
        memset(&opts, 0, sizeof(opts));

        opts.schema = YEPTRIS_SCHEMA_12_CORE;
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_ex(c.y, strlen(c.y), &opts, &st);
        EXPECT_EQ(st == YEPTRIS_OK, c.core_ok != 0) << "core input: " << c.y;
        yeptris_document_free(doc);

        opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
        st = YEPTRIS_OK;
        doc = yeptris_parse_ex(c.y, strlen(c.y), &opts, &st);
        ASSERT_EQ(st, YEPTRIS_OK) << "compat input: " << c.y;
        YeptrisNode root = yeptris_document_root(doc, 0);
        ASSERT_NE(root, nullptr);
        EXPECT_EQ(yeptris_node_kind(root) == YEPTRIS_NODE_MAPPING, c.root_is_map != 0) << c.y;
        yeptris_document_free(doc);
    }

    /* the compat parse is libyaml's, not just accepted: the outdented
     * continuation is flow content ("a", "b", "c" — not block keys) */
    const char* y = "flow: [a,\nb,\nc]";
    YeptrisParseOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_ex(y, strlen(y), &opts, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    YeptrisNode root = yeptris_document_root(doc, 0);
    YeptrisNode seq = yeptris_node_map_get(root, "flow", 4);
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(yeptris_node_kind(seq), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_seq_count(seq), 3u);
    EXPECT_EQ(val(yeptris_node_seq_at(seq, 2)), "c");
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
        {"[1,,2]", YEPTRIS_ERROR_PARSE},              /* doubled comma */
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

/* A properties-only document ("---\n!\n") ends at the boundary: the
 * props belong to its empty scalar and must never swallow the next
 * document as their value (found by the realworld bench corpus). */
TEST(Parse, PropsOnlyDocumentThenBoundary) {
    struct {
        const char* y;
        const char* tag;
        const char* anchor;
    } cases[] = {
        {"---\n!\n---\nfoo: bar\n", "!", ""},
        {"---\n!tag\n---\nfoo: bar\n", "!tag", ""},
        {"---\n&a\n---\nfoo: bar\n", "", "a"},
        {"---\n!\n...\nfoo: bar\n", "!", ""},
        {"---\n!\n# comment\n---\nfoo: bar\n", "!", ""},
    };
    for (const auto& c : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(c.y, strlen(c.y), &st);
        ASSERT_NE(doc, nullptr) << c.y << ": " << yeptris_last_error(NULL, NULL);
        ASSERT_EQ(yeptris_document_count(doc), 2u) << c.y;
        YeptrisNode first = yeptris_document_root(doc, 0);
        ASSERT_NE(first, nullptr) << c.y;
        EXPECT_EQ(yeptris_node_kind(first), YEPTRIS_NODE_SCALAR) << c.y;
        EXPECT_EQ(val(first), "") << c.y;
        size_t tl = 0;
        const char* tp = yeptris_node_tag(first, &tl);
        std::string tag = tp ? std::string(tp, tl) : std::string();
        EXPECT_EQ(tag, c.tag) << c.y;
        YeptrisNode second = yeptris_document_root(doc, 1);
        ASSERT_NE(second, nullptr) << c.y;
        EXPECT_EQ(yeptris_node_kind(second), YEPTRIS_NODE_MAPPING) << c.y;
        EXPECT_EQ(map_str(second, "foo"), "bar") << c.y;
        yeptris_document_free(doc);
    }
}

/* 2JQS: two empty-key pairs — suite-valid, libyaml-rejected; the bench
 * corpus keeps it out of the shared race but parse behavior is pinned. */
TEST(Parse, EmptyKeyPairs) {
    const char* y = ": a\n: b\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_kind(root), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(yeptris_node_map_count(root), 2u);
    EXPECT_EQ(map_str(root, ""), "a");
    yeptris_document_free(doc);
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

/* ---- strict JSON mode (TODO.impl/08C) -------------------------------- */

TEST(JsonMode, StrictAcceptsAndRejects) {
    struct {
        const char* j;
        int accept;
    } cases[] = {
        {"{\"a\": 1}", 1}, {"[1, 2, 3]", 1}, {"42", 1},      {"\"x\"", 1},       {"true", 1},
        {"null", 1},       {"-0.5e+10", 1},  {" [ 1 ] ", 1}, {"{\"a\": 1,}", 0}, {"[1,,2]", 0},
        {"01", 0},         {"+1", 0},        {".5", 0},      {"{'a': 1}", 0},    {"[unquoted]", 0},
        {"{\"a\" 1}", 0},  {"[1 2]", 0},     {"", 0},        {"[1] x", 0},       {"[", 0},
        {"\"\\uDd\"", 0},  {"\"\\a\"", 0},   {"nan", 0},     {"[1.0e]", 0},
    };
    for (const auto& c : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_json(c.j, strlen(c.j), &st);
        EXPECT_EQ(doc != nullptr, c.accept != 0) << c.j;
        yeptris_document_free(doc);
    }
}

TEST(JsonMode, DomQueries) {
    const char* j = "{\"name\": \"yeptris\", \"n\": 42, \"pi\": 3.5, \"ok\": true, \"no\": null}";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_json(j, strlen(j), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_kind(root), YEPTRIS_NODE_MAPPING);
    EXPECT_EQ(map_str(root, "name"), "yeptris");
    int64_t n = 0;
    EXPECT_EQ(yeptris_node_int(yeptris_node_map_get(root, "n", 1), &n), YEPTRIS_OK);
    EXPECT_EQ(n, 42);
    double pi = 0;
    EXPECT_EQ(yeptris_node_float(yeptris_node_map_get(root, "pi", 2), &pi), YEPTRIS_OK);
    EXPECT_DOUBLE_EQ(pi, 3.5);
    int b = 0;
    EXPECT_EQ(yeptris_node_bool(yeptris_node_map_get(root, "ok", 2), &b), YEPTRIS_OK);
    EXPECT_EQ(b, 1);
    /* JSON null: the literal text with the null tag (Psych: nil) */
    YeptrisNode no = yeptris_node_map_get(root, "no", 2);
    EXPECT_EQ(val(no), "null");
    yeptris_document_free(doc);
}

TEST(JsonMode, EscapesAndDeep) {
    const char* j = "{\"k\": \"a\\nb\\u00e9\\ud834\\udd1e\", \"arr\": [[[[1]]]]}";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_json(j, strlen(j), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    EXPECT_EQ(map_str(root, "k"), "a\nb\xC3\xA9\xF0\x9D\x84\x9E");
    yeptris_document_free(doc);
    std::string deep = "[[[[[[[[[[10]]]]]]]]]]";
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse_json(deep.c_str(), deep.size(), &st2);
    EXPECT_NE(doc2, nullptr);
    yeptris_document_free(doc2);
}

/* ruby #168: the bulk children drain — one walk instead of the
 * per-index O(i) seq_at/map_at loops (the 80k-row relaton index paid
 * n^2/2 sibling steps). */
TEST(NodeChildren, BulkDrainMatchesIndexedAccess) {
    const char* y = "top:\n"
                    "  - one\n"
                    "  - [a, b]\n"
                    "  - k: v\n"
                    "    k2: v2\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);

    /* count-only: no handles allocated, same count as seq_count */
    EXPECT_EQ(yeptris_node_children(root, NULL, 0), 2u);

    /* mapping: key,value interleaved — one walk equals map_at pairs */
    YeptrisNode kids[2];
    EXPECT_EQ(yeptris_node_children(root, kids, 2), 2u);
    EXPECT_EQ(val(kids[0]), "top");
    YeptrisNode seq = kids[1];
    ASSERT_NE(seq, nullptr);
    EXPECT_EQ(yeptris_node_kind(seq), YEPTRIS_NODE_SEQUENCE);

    EXPECT_EQ(yeptris_node_children(seq, NULL, 0), 3u);
    YeptrisNode items[3];
    EXPECT_EQ(yeptris_node_children(seq, items, 3), 3u);
    EXPECT_EQ(val(items[0]), "one");
    EXPECT_EQ(yeptris_node_kind(items[1]), YEPTRIS_NODE_SEQUENCE);
    EXPECT_EQ(yeptris_node_kind(items[2]), YEPTRIS_NODE_MAPPING);

    /* differential: the drain's values equal seq_at's, in order */
    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(yeptris_node_id(items[i]), yeptris_node_id(yeptris_node_seq_at(seq, i)));
    }

    /* the nested mapping: pairs interleaved — the map_at equivalence */
    YeptrisNode m = items[2];
    YeptrisNode pairs[4];
    EXPECT_EQ(yeptris_node_children(m, pairs, 4), 4u);
    for (size_t i = 0; i < 2; i++) {
        YeptrisNode k, v;
        ASSERT_EQ(yeptris_node_map_at(m, i, &k, &v), 0);
        EXPECT_EQ(yeptris_node_id(pairs[2 * i]), yeptris_node_id(k));
        EXPECT_EQ(yeptris_node_id(pairs[2 * i + 1]), yeptris_node_id(v));
    }
    yeptris_document_free(doc);
}

TEST(NodeChildren, ScalarsAndShortBuffers) {
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse_json("[10, 20, 30]", strlen("[10, 20, 30]"), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);

    /* a scalar has no children */
    YeptrisNode ten = yeptris_node_seq_at(root, 0);
    ASSERT_NE(ten, nullptr);
    EXPECT_EQ(yeptris_node_children(ten, NULL, 0), 0u);

    /* a short buffer fills what fits and still returns the total */
    YeptrisNode one[1];
    EXPECT_EQ(yeptris_node_children(root, one, 1), 3u);
    EXPECT_EQ(val(one[0]), "10");

    /* NULL handle */
    EXPECT_EQ(yeptris_node_children(NULL, NULL, 0), 0u);
    yeptris_document_free(doc);
}

TEST(NodeChildren, LargeSequenceDrainsOnce) {
    /* the repro shape from ruby #168: 80k top-level rows */
    std::string y = "- k: 1\n";
    y.reserve(y.size() * 80000);
    for (int i = 1; i < 80000; i++) {
        y += "- k: 1\n";
    }
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y.c_str(), y.size(), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    EXPECT_EQ(yeptris_node_children(root, NULL, 0), 80000u);
    std::vector<YeptrisNode> kids(80000);
    EXPECT_EQ(yeptris_node_children(root, kids.data(), kids.size()), 80000u);
    EXPECT_EQ(yeptris_node_kind(kids[79999]), YEPTRIS_NODE_MAPPING);
    yeptris_document_free(doc);
}

/* #377: the lazy child-index — indexed access must reflect mutations
 * (every link/unlink invalidates the cache) and stay differential
 * with the drain. */
TEST(NodeChildren, IndexedAccessSurvivesMutation) {
    const char* y = "- one\n- two\n- three\n";
    YeptrisStatus st;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);

    /* warm the cache */
    EXPECT_EQ(val(yeptris_node_seq_at(root, 2)), "three");

    /* mutate: append + delete, then re-index */
    YeptrisNode four = yeptris_node_new_scalar(doc, "four", 4, YEPTRIS_STYLE_PLAIN);
    ASSERT_NE(four, nullptr);
    ASSERT_EQ(yeptris_node_seq_add(root, four), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_seq_count(root), 4u);
    EXPECT_EQ(val(yeptris_node_seq_at(root, 3)), "four");
    EXPECT_EQ(val(yeptris_node_seq_at(root, 2)), "three");

    ASSERT_EQ(yeptris_node_seq_del(root, 0), YEPTRIS_OK);
    EXPECT_EQ(val(yeptris_node_seq_at(root, 0)), "two");
    EXPECT_EQ(val(yeptris_node_seq_at(root, 2)), "four");

    /* differential: the drain agrees with the index after mutation */
    YeptrisNode kids[3];
    ASSERT_EQ(yeptris_node_children(root, kids, 3), 3u);
    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(yeptris_node_id(kids[i]), yeptris_node_id(yeptris_node_seq_at(root, i)));
    }
    yeptris_document_free(doc);
}
