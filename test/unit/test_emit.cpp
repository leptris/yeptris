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

/* ---- canonical mode (13B) ------------------------------------------- */

static std::string canon(const char* y) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    EXPECT_NE(doc, nullptr) << yeptris_last_error(NULL, NULL) << " [" << y << "]";
    if (doc == nullptr) {
        return "<parse-failed>";
    }
    yeptris_emit_options opts = {sizeof(yeptris_emit_options), 1, 0};
    size_t len = 0;
    char* out = yeptris_serialize_ex(doc, &opts, &len);
    yeptris_document_free(doc);
    if (out == nullptr) {
        return "<emit-failed>";
    }
    std::string r(out, len);
    free(out);
    return r;
}

TEST(EmitCanonical, Basics) {
    EXPECT_EQ(canon("a: 1\nb: 'x'\nc: [1, 2]\n"), "---\n{\"a\": 1, \"b\": \"x\", \"c\": [1, 2]}\n");
    EXPECT_EQ(canon("a: hi\n"), "---\n{\"a\": \"hi\"}\n");
    EXPECT_EQ(canon("- 1\n- 2\n"), "---\n[1, 2]\n");
    EXPECT_EQ(canon("k: plain words here\n"), "---\n{\"k\": \"plain words here\"}\n");
}

TEST(EmitCanonical, TypedWordsAndFloats) {
    EXPECT_EQ(canon("t: true\nf: false\nn: ~\ne: \n"),
              "---\n{\"t\": true, \"f\": false, \"n\": ~, \"e\": ~}\n");
    EXPECT_EQ(canon("x: True\n"), "---\n{\"x\": true}\n");
    EXPECT_EQ(canon("x: 6.8599e+5\n"), "---\n{\"x\": 685990.0}\n");
    EXPECT_EQ(canon("x: 0.1\n"), "---\n{\"x\": 0.1}\n");
    EXPECT_EQ(canon("x: 3.141592653589793\n"), "---\n{\"x\": 3.141592653589793}\n");
    EXPECT_EQ(canon("x: .inf\n"), "---\n{\"x\": .inf}\n");
    EXPECT_EQ(canon("x: 42\n"), "---\n{\"x\": 42}\n");
}

TEST(EmitCanonical, FixedPointFByteStability) {
    const char* cases[] = {
        "a: 1\nb: [x, y]\nc:\n  d: 2\n",     "- - 1\n  - 2\n- k: v\n",
        "text: |\n  line one\n  line two\n", "a: &x 1\nb: *x\n",
        "key with spaces: value\n",          "'quoted key': v\n",
        "nested: {a: {b: [1, {c: 2}]}}\n",   "e: ''\nz:\n",
    };
    for (const char* y : cases) {
        std::string c1 = canon(y);
        /* parse(c1) must re-canonicalize byte-identically */
        EXPECT_EQ(canon(c1.c_str()), c1) << "input [" << y << "] -> [" << c1 << "]";
    }
}

TEST(EmitCanonical, OptionsVersioning) {
    yeptris_emit_options opts = {4, 1, 0}; /* too small: canonical ignored */
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse("a: 1\n", 5, &st);
    ASSERT_NE(doc, nullptr);
    size_t len = 0;
    char* out = yeptris_serialize_ex(doc, &opts, &len);
    yeptris_document_free(doc);
    ASSERT_NE(out, nullptr);
    /* fidelity mode: no --- marker */
    EXPECT_EQ(std::string(out, len), "a: 1\n");
    free(out);
}
