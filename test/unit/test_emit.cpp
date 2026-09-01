/* test_emit.cpp — emitter contracts (TODO.impl/13): exact sizing
 * (serialize_into with NULL/short buffers), canonical shapes, roundtrip
 * stability on hand vectors. The corpus-wide gate is
 * test_emit_roundtrip. */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>

namespace {

std::string ser(YeptrisDocument d) {
    size_t len = 0;
    char* s = yeptris_serialize(d, &len);
    std::string out(s ? s : "", s ? len : 0);
    free(s);
    return out;
}

std::string roundtrip(const char* in) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse(in, strlen(in), &st);
    EXPECT_EQ(st, YEPTRIS_OK);
    if (d == nullptr) {
        return "(parse-failed)";
    }
    std::string s1 = ser(d);
    yeptris_document_free(d);
    return s1;
}

} // namespace

TEST(Emit, BlockShapes) {
    EXPECT_EQ(roundtrip("a: 1\nb: two\n"), "a: 1\nb: two\n");
    EXPECT_EQ(roundtrip("- x\n- y\n"), "- x\n- y\n");
    EXPECT_EQ(roundtrip("k:\n  nested: v\n"), "k:\n  nested: v\n");
    EXPECT_EQ(roundtrip("- a: 1\n  b: 2\n"), "- a: 1\n  b: 2\n");
    EXPECT_EQ(roundtrip("a:\n  - 1\n  - 2\n"), "a:\n  - 1\n  - 2\n");
    EXPECT_EQ(roundtrip("empty: {}\nlist: []\n"), "empty: {}\nlist: []\n");
}

TEST(Emit, FlowPreserved) {
    EXPECT_EQ(roundtrip("a: [1, 2, 3]\n"), "a: [1, 2, 3]\n");
    EXPECT_EQ(roundtrip("m: {x: 1, y: 2}\n"), "m: {x: 1, y: 2}\n");
}

TEST(Emit, QuotedAndEscapes) {
    EXPECT_EQ(roundtrip("a: \"dq\"\n"), "a: \"dq\"\n");
    EXPECT_EQ(roundtrip("a: 'sq'\n"), "a: 'sq'\n");
    EXPECT_EQ(roundtrip("a: \"tab\\there\"\n"), "a: \"tab\\there\"\n");
    EXPECT_EQ(roundtrip("a: 'it''s'\n"), "a: 'it''s'\n");
}

TEST(Emit, LiteralBlocks) {
    EXPECT_EQ(roundtrip("a: |\n  one\n  two\n"), "a: |2\n  one\n  two\n");
    EXPECT_EQ(roundtrip("a: |-\n  strip\n"), "a: strip\n");
    EXPECT_EQ(roundtrip("a: |+\n  keep\n\n"), "a: |+2\n  keep\n\n");
}

TEST(Emit, AnchorsAndAliases) {
    EXPECT_EQ(roundtrip("a: &x 1\nb: *x\n"), "a: &x 1\nb: *x\n");
    EXPECT_EQ(roundtrip("a:\n  &s\n  - 1\n"), "a: &s\n  - 1\n");
}

TEST(Emit, MultiDoc) {
    EXPECT_EQ(roundtrip("a: 1\n---\nb: 2\n"), "---\na: 1\n---\nb: 2\n");
}

TEST(Emit, TagsPreserved) {
    EXPECT_EQ(roundtrip("- !!str 5\n"), "- !<tag:yaml.org,2002:str> 5\n");
    EXPECT_EQ(roundtrip("!!map\na: 1\n"), "!<tag:yaml.org,2002:map>\na: 1\n");
}

TEST(Emit, UnsafePlainFallsBack) {
    EXPECT_EQ(roundtrip("a: \"1: 2\"\n"), "a: \"1: 2\"\n");
    EXPECT_EQ(roundtrip("a: \" leading\"\n"), "a: \" leading\"\n");
}

TEST(Emit, SizingIsExact) {
    const char* y = "a: [1, 2, {b: c}]\nlit: |\n  text\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse(y, strlen(y), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    size_t need = yeptris_serialize_into(d, NULL, 0);
    EXPECT_GT(need, 0u);
    std::string buf(need + 1, '\0');
    size_t wrote = yeptris_serialize_into(d, &buf[0], need);
    EXPECT_EQ(wrote, need);
    EXPECT_EQ(buf[need], '\0');
    /* short buffer: nothing written, need returned */
    EXPECT_EQ(yeptris_serialize_into(d, &buf[0], need - 1), need);
    yeptris_document_free(d);
}

TEST(Emit, ByteStableAcrossRoundtrips) {
    const char* cases[] = {
        "a: 1\nb: [x, y]\nc: \"quoted\"\n", "deep:\n  map:\n    seq:\n      - 1\n      - two\n",
        "lit: |\n  line1\n  line2\n",       "anchored: &a\n  k: v\nref: *a\n",
        "---\nx: 1\n---\ny: 2\n",
    };
    for (const char* y : cases) {
        std::string s1 = roundtrip(y);
        std::string s2 = roundtrip(s1.c_str());
        EXPECT_EQ(s1, s2) << "unstable for: " << y;
    }
}
