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
    EXPECT_EQ(roundtrip("a:\n  - 1\n  - 2\n"), "a:\n- 1\n- 2\n");
    EXPECT_EQ(roundtrip("empty: {}\nlist: []\n"), "empty: {}\nlist: []\n");
}

TEST(Emit, ExplicitDocStartOption) {
    YeptrisStatus st = YEPTRIS_OK; /* parse leaves st untouched on success */
    YeptrisDocument doc = yeptris_parse("version: 1.2.3\n", 15, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);

    /* default (fidelity mode): a single document is bare */
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "version: 1.2.3\n");
    free(out);

    /* the option carries the leading marker (Psych implicit=false) */
    yeptris_emit_options opts = {sizeof(opts), 0, 0, 1};
    out = yeptris_serialize_ex(doc, &opts, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "---\nversion: 1.2.3\n");
    free(out);
    yeptris_document_free(doc);

    /* libyaml parity: a scalar root rides the marker line */
    st = YEPTRIS_OK;
    doc = yeptris_parse("42\n", 3, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    out = yeptris_serialize_ex(doc, &opts, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "--- 42\n");
    free(out);
    yeptris_document_free(doc);

    /* size-versioned: a caller with the OLD struct size keeps the
     * implicit behavior even with garbage past its fields */
    st = YEPTRIS_OK;
    doc = yeptris_parse("a: 1\n", 5, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    yeptris_emit_options old_shape = {4, 0, 0, 0}; /* too small: explicit_doc_start unread */
    out = yeptris_serialize_ex(doc, &old_shape, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "a: 1\n");
    free(out);
    yeptris_document_free(doc);
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
    /* libyaml's indicator rules (#290 family 4): the explicit indent
     * only when the first body line starts with a space or is blank */
    EXPECT_EQ(roundtrip("a: |\n  one\n  two\n"), "a: |\n  one\n  two\n");
    EXPECT_EQ(roundtrip("a: |-\n  strip\n"), "a: strip\n");
    EXPECT_EQ(roundtrip("a: |+\n  keep\n\n"), "a: |+\n  keep\n\n");
    EXPECT_EQ(roundtrip("a: |\n  one\n\n  two\n"), "a: |\n  one\n\n  two\n");
    EXPECT_EQ(roundtrip("a: |2\n   deep\n  flat\n"), "a: |2\n   deep\n  flat\n");
}

TEST(Emit, BuiltNullsRideBare) {
    /* libyaml's null rendering (#290): an empty plain scalar value
     * rides the key with no trailing space; a null seq item is a
     * bare dash; an explicitly-empty block scalar stays "" */
    YeptrisDocument doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_map_add(root, "z", 1,
                                   yeptris_node_new_scalar(doc, "", 0, YEPTRIS_STYLE_PLAIN)),
              YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_map_add(root, "s", 1,
                                   yeptris_node_new_scalar(doc, "", 0, YEPTRIS_STYLE_LITERAL)),
              YEPTRIS_OK);
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "z:\ns: \"\"\n");
    free(out);
    yeptris_document_free(doc);

    doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode seq = yeptris_node_new_sequence(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, seq), YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_seq_add(seq, yeptris_node_new_scalar(doc, "1", 1, YEPTRIS_STYLE_PLAIN)),
              YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_seq_add(seq, yeptris_node_new_scalar(doc, "", 0, YEPTRIS_STYLE_PLAIN)),
              YEPTRIS_OK);
    len = 0;
    out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "- 1\n-\n");
    free(out);
    yeptris_document_free(doc);
}

TEST(Emit, AnchorsAndAliases) {
    EXPECT_EQ(roundtrip("a: &x 1\nb: *x\n"), "a: &x 1\nb: *x\n");
    EXPECT_EQ(roundtrip("a:\n  &s\n  - 1\n"), "a: &s\n- 1\n");
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

TEST(Emit, BuiltEmptyCollectionsRideTheKeyLine) {
    /* v0.5.1 regression: an empty built sequence under a mapping key
     * took the seq-at-key-column path and emitted a bare [] on the
     * next line at the mapping's own column — invalid YAML (pyyaml
     * rejects it; yeptris's own loader was lenient). Empty
     * collections ride the key line flow, libyaml's form. */
    YeptrisDocument doc = yeptris_document_new();
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_node_new_mapping(doc);
    ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_map_add(root, "empty_list", 10, yeptris_node_new_sequence(doc)),
              YEPTRIS_OK);
    ASSERT_EQ(yeptris_node_map_add(root, "empty_map", 9, yeptris_node_new_mapping(doc)),
              YEPTRIS_OK);
    size_t len = 0;
    char* out = yeptris_serialize(doc, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "empty_list: []\nempty_map: {}\n");
    free(out);
    yeptris_document_free(doc);
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
    yeptris_emit_options opts = {sizeof(opts), 1, 0, 0};
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
    yeptris_emit_options opts = {4, 1, 0, 0}; /* too small: canonical+explicit unread */
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

/* ---- streaming writer (13C) ------------------------------------------ */

#include <yeptris/emit.h>

namespace {
struct SinkAcc {
    std::string out;
    size_t calls = 0;
};
} // namespace

static int sink_append(void* ctx, const char* bytes, size_t len) {
    SinkAcc* a = (SinkAcc*)ctx;
    a->out.append(bytes, len);
    a->calls++;
    return 0;
}

static int sink_abort(void* ctx, const char* bytes, size_t len) {
    (void)ctx;
    (void)bytes;
    (void)len;
    return 7;
}

TEST(EmitStream, IdenticalToBuffered) {
    const char* docs[] = {
        "a: 1\nb: [1, 2, 3]\nc: hello world\n",
        "---\n- one\n- two\n- three\n...\n",
        "text: |\n  a very long literal block\n  spanning several lines\n  to cross the mark\n",
    };
    for (const char* y : docs) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
        ASSERT_NE(doc, nullptr);
        size_t blen = 0;
        char* buffered = yeptris_serialize(doc, &blen);
        SinkAcc acc;
        size_t slen = yeptris_serialize_stream(doc, nullptr, sink_append, &acc);
        ASSERT_NE(buffered, nullptr);
        EXPECT_EQ(slen, blen);
        EXPECT_EQ(acc.out.size(), blen);
        EXPECT_EQ(memcmp(acc.out.data(), buffered, blen), 0);
        EXPECT_GE(acc.calls, 1u);
        free(buffered);
        yeptris_document_free(doc);
    }
}

TEST(EmitStream, LargeDocumentFlushes) {
    std::string y;
    for (int i = 0; i < 20000; i++) {
        y += "key" + std::to_string(i) + ": value number " + std::to_string(i) + "\n";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y.c_str(), y.size(), &st);
    ASSERT_NE(doc, nullptr);
    size_t blen = 0;
    char* buffered = yeptris_serialize(doc, &blen);
    SinkAcc acc;
    size_t slen = yeptris_serialize_stream(doc, nullptr, sink_append, &acc);
    EXPECT_GT(blen, (1u << 16)); /* actually crosses the watermark */
    EXPECT_EQ(slen, blen);
    EXPECT_EQ(acc.out.size(), blen);
    EXPECT_EQ(memcmp(acc.out.data(), buffered, blen), 0);
    EXPECT_GT(acc.calls, 1u); /* flushed multiple times */
    free(buffered);
    yeptris_document_free(doc);
}

TEST(EmitStream, AbortPropagates) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse("a: 1\n", 5, &st);
    ASSERT_NE(doc, nullptr);
    SinkAcc acc;
    size_t slen = yeptris_serialize_stream(doc, nullptr, sink_abort, &acc);
    EXPECT_EQ(slen, 0u); /* aborted: reported as nothing written */
    yeptris_document_free(doc);
}

/* ---- width folding (13B) --------------------------------------------- */

TEST(EmitFold, FlowWrapsPastBestWidth) {
    const char* y = "list: [aaaa, bbbb, cccc, dddd, eeee, ffff, gggg, hhhh]\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr);
    yeptris_emit_options opts = {sizeof(opts), 0, 30, 0};
    size_t len = 0;
    char* out = yeptris_serialize_ex(doc, &opts, &len);
    ASSERT_NE(out, nullptr);
    /* every line must respect the width (indent + content) */
    int over = 0;
    int col = 0;
    for (size_t i = 0; i < len; i++) {
        if (out[i] == '\n') {
            col = 0;
        } else {
            col++;
            if (col > 40) { /* 30 + bracket indent + one value slack */
                over = 1;
            }
        }
    }
    EXPECT_EQ(over, 0);
    /* must have wrapped (multiple lines) */
    EXPECT_NE(memchr(out, '\n', len), nullptr);
    /* and must re-parse to the same content */
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse(out, len, &st2);
    EXPECT_NE(doc2, nullptr);
    if (doc2 != nullptr) {
        YeptrisNode a = yeptris_document_root(doc2, 0);
        YeptrisNode b = yeptris_document_root(doc, 0);
        YeptrisNode la = yeptris_node_map_get(a, "list", 4);
        YeptrisNode lb = yeptris_node_map_get(b, "list", 4);
        EXPECT_EQ(yeptris_node_seq_count(la), yeptris_node_seq_count(lb));
    }
    yeptris_document_free(doc2);
    free(out);
    yeptris_document_free(doc);
}

TEST(EmitFold, DefaultUnfoldsSmall) {
    const char* y = "a: [1, 2, 3]\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(doc, nullptr);
    yeptris_emit_options opts = {sizeof(opts), 0, 0, 0};
    size_t len = 0;
    char* out = yeptris_serialize_ex(doc, &opts, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(std::string(out, len), "a: [1, 2, 3]\n");
    free(out);
    yeptris_document_free(doc);
}

/* #168: a TAGGED literal-styled scalar keeps the block form even
 * single-line (psych's !binary base64 body); an untagged single-line
 * literal re-emits plain (libyaml parity, pinned above). */
TEST(Emit, TaggedLiteralKeepsBlockForm) {
    YeptrisStatus st = YEPTRIS_OK;
    const char* y = "--- !binary |-\n  YbBimGM=\n";
    YeptrisDocument d = yeptris_parse(y, strlen(y), &st);
    ASSERT_NE(d, nullptr);
    size_t len = 0;
    char* out = yeptris_serialize(d, &len);
    ASSERT_NE(out, nullptr);
    EXPECT_STREQ(out, "!binary |-\n  YbBimGM=\n");
    yeptris_free(out);
    yeptris_document_free(d);
}

TEST(Emit, PlainNegativeNumbers) {
    /* '-' only indicates before a blank: -5 and -.inf are plain
     * (libyaml parity; the old table quoted every '-'-leading text) */
    struct {
        const char* value;
        const char* want;
    } cases[] = {
        {"-5", "-5"}, {"-.inf", "-.inf"}, {"-2.5e+300", "-2.5e+300"}, {"?x", "?x"},
        {"-", "'-'"}, {"- x", "'- x'"},   {"? x", "'? x'"},
    };
    for (const auto& c : cases) {
        YeptrisDocument doc = yeptris_document_new();
        ASSERT_NE(doc, nullptr);
        YeptrisNode root = yeptris_node_new_mapping(doc);
        ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
        YeptrisNode v = yeptris_node_new_scalar(doc, c.value, strlen(c.value), YEPTRIS_STYLE_PLAIN);
        ASSERT_NE(v, nullptr);
        ASSERT_EQ(yeptris_node_map_add(root, "k", 1, v), YEPTRIS_OK);
        size_t len = 0;
        char* out = yeptris_serialize(doc, &len);
        ASSERT_NE(out, nullptr);
        EXPECT_EQ(std::string(out, len), std::string("k: ") + c.want + "\n") << c.value;
        free(out);
        yeptris_document_free(doc);
    }
}

/* #182 regression: the writer's grow trigger left no terminator slot,
 * so a serialized size landing exactly on the buffer capacity wrote
 * the closing NUL one byte past the allocation — the first byte of
 * the next heap block's header. Corrupt deterministic, detected
 * later (a malloc checksum botch at an unrelated free), which is why
 * it presented as a load/GC-timing flake for years of emit changes.
 * Sweep lengths through both doubling boundaries on a BUILT document
 * (est=256: the dump path's shape — the parse route's est=input+64
 * never grows on plain round trips). */
TEST(Emit, TerminatorSlotSurvivesEveryBoundaryLength) {
    for (int n = 240; n <= 1200; n++) {
        YeptrisDocument doc = yeptris_document_new();
        ASSERT_NE(doc, nullptr);
        YeptrisNode root = yeptris_node_new_mapping(doc);
        ASSERT_NE(root, nullptr);
        ASSERT_EQ(yeptris_document_set_root(doc, root), YEPTRIS_OK);
        std::string long_scalar(n, 'x');
        YeptrisNode val =
            yeptris_node_new_scalar(doc, long_scalar.data(), (size_t)n, YEPTRIS_STYLE_PLAIN);
        ASSERT_NE(val, nullptr);
        ASSERT_EQ(yeptris_node_map_add(root, "k", 1, val), YEPTRIS_OK);

        size_t len = 0;
        char* out = yeptris_serialize(doc, &len);
        ASSERT_NE(out, nullptr) << "n=" << n;
        /* the emitter folds nothing here: one line "k: xxx...\n" */
        ASSERT_EQ(len, (size_t)n + 4) << "n=" << n;
        EXPECT_EQ(out[len], '\0') << "n=" << n; /* ASAN: in-bounds */
        std::string body(out, len);
        yeptris_free(out);

        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument back = yeptris_parse(body.data(), body.size(), &st);
        ASSERT_NE(back, nullptr) << "n=" << n;
        YeptrisNode r2 = yeptris_document_root(back, 0);
        ASSERT_NE(r2, nullptr);
        YeptrisNode v2 = yeptris_node_map_get(r2, "k", 1);
        ASSERT_NE(v2, nullptr);
        size_t bl = 0;
        const char* bv = yeptris_node_value(v2, &bl);
        ASSERT_EQ(bl, (size_t)n) << "n=" << n;
        EXPECT_EQ(memcmp(bv, long_scalar.data(), (size_t)n), 0) << "n=" << n;
        yeptris_document_free(back);
        yeptris_document_free(doc);
    }
}

/* #352: the plain-safety interior scan is SWAR — differential-pinned
 * against a reference byte loop over adversarial alphabets at every
 * length around the 8-byte word boundaries, plus the head rules. A
 * mask bug shows up as a disagreement, not as silent misquoting. */
#include "emit/style.h"

namespace {
int plain_safe_reference(const char* p, uint32_t len) {
    if (len == 0)
        return 0;
    auto blank = [](char c) { return c == ' ' || c == '\t'; };
    if (blank(p[0]) || blank(p[len - 1]))
        return 0;
    switch (p[0]) {
    case ',':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
    case '&':
    case '*':
    case '!':
    case '|':
    case '>':
    case '\'':
    case '"':
    case '%':
    case '@':
    case '`':
        return 0;
    case '-':
    case '?':
        if (len == 1 || blank(p[1]))
            return 0;
        break;
    default:
        break;
    }
    for (uint32_t i = 0; i < len; i++) {
        char c = p[i];
        if (c == '\n' || c == '\r' || c == '\t')
            return 0;
        if (c == ':' && (i + 1 >= len || blank(p[i + 1])))
            return 0;
        if (c == '#' && i > 0 && blank(p[i - 1]))
            return 0;
    }
    if (len == 3 && (memcmp(p, "---", 3) == 0 || memcmp(p, "...", 3) == 0))
        return 0;
    return 1;
}
} /* namespace */

TEST(EmitStyle, PlainSafeSwarMatchesReference) {
    const std::string alphabet = "ab: #\t-\n.?";
    uint64_t seed = 0x9E3779B9ull;
    auto next = [&seed]() {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return (uint32_t)(seed >> 33);
    };
    /* exhaustive short strings over a 3-char core, all lengths 0..24 */
    const std::string core = "a: #";
    for (uint32_t len = 0; len <= 24; len++) {
        uint64_t variants = 1ull << (2 * (len > 8 ? 8 : len));
        for (uint64_t v = 0; v < variants; v++) {
            std::string s;
            uint64_t bits = v;
            for (uint32_t j = 0; j < len; j++) {
                s += core[bits & 3u];
                bits >>= 2;
                if (j == 7 && len > 8)
                    s += core[(v >> 13) & 3u]; /* boundary flavors */
            }
            ASSERT_EQ(yep_style_plain_safe(s.data(), (uint32_t)s.size()),
                      plain_safe_reference(s.data(), (uint32_t)s.size()))
                << "len=" << len << " v=" << v << " s='" << s << "'";
        }
        /* random long strings over the full alphabet */
        for (int r = 0; r < 200; r++) {
            std::string s;
            for (uint32_t j = 0; j < len + 8; j++) {
                s += alphabet[next() % alphabet.size()];
            }
            ASSERT_EQ(yep_style_plain_safe(s.data(), (uint32_t)s.size()),
                      plain_safe_reference(s.data(), (uint32_t)s.size()))
                << "len=" << s.size() << " s='" << s << "'";
        }
    }
    /* the head rules, exact */
    EXPECT_EQ(yep_style_plain_safe("", 0), 0);
    EXPECT_EQ(yep_style_plain_safe(" x", 2), 0);
    EXPECT_EQ(yep_style_plain_safe("x ", 2), 0);
    EXPECT_EQ(yep_style_plain_safe("---", 3), 0);
    EXPECT_EQ(yep_style_plain_safe("...", 3), 0);
    EXPECT_EQ(yep_style_plain_safe("---x", 4), 1);
    EXPECT_EQ(yep_style_plain_safe("-5", 2), 1);
    EXPECT_EQ(yep_style_plain_safe("-", 1), 0);
    EXPECT_EQ(yep_style_plain_safe("?", 1), 0);
    EXPECT_EQ(yep_style_plain_safe("?x", 2), 1);
    EXPECT_EQ(yep_style_plain_safe(":name", 5), 1);
    EXPECT_EQ(yep_style_plain_safe(": name", 6), 0);
    EXPECT_EQ(yep_style_plain_safe("#x", 2), 0);
    EXPECT_EQ(yep_style_plain_safe("a#b", 3), 1);
}
