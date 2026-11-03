// test_validate_stream.cpp — the event-route validator (#902). The
// differential gate: for a corpus of multi-document inputs, the
// event-route diagnostics must equal the DOM route's EXACTLY (status,
// count, and every diagnostic's code/path/detail/line/col/offset) —
// both routes share the fact cores and the diag machinery, so this is
// an identity gate, not a re-implementation. Plus the face's own
// contract pins: the argument gates (byte-identical to
// yeptris_validate's), the empty stream, report ownership, and the
// issue's attribution shapes riding the event walk.
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <yeptris/validate.h>

namespace {

// Compares BOTH routes over one input under one family: status,
// report count, and every diagnostic field.
void pin_routes(const char* src, size_t len, YeptrisSchema schema) {
    yeptris_validate_report* dom = NULL;
    yeptris_validate_report* evt = NULL;
    YeptrisStatus sd = yeptris_validate(src, len, schema, &dom);
    YeptrisStatus se = yeptris_validate_stream(src, len, schema, &evt);
    ASSERT_EQ(sd, se) << "the routes share the parse's statuses";
    size_t nd = yeptris_validate_report_count(dom);
    size_t ne = yeptris_validate_report_count(evt);
    ASSERT_EQ(nd, ne) << "the routes agree on the diagnostic count";
    for (size_t i = 0; i < nd; i++) {
        const yeptris_diagnostic* a = yeptris_validate_report_at(dom, i);
        const yeptris_diagnostic* b = yeptris_validate_report_at(evt, i);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        EXPECT_EQ(a->code, b->code) << "diag " << i;
        EXPECT_STREQ(a->path, b->path) << "diag " << i;
        EXPECT_STREQ(a->detail, b->detail) << "diag " << i;
        EXPECT_EQ(a->line, b->line) << "diag " << i;
        EXPECT_EQ(a->col, b->col) << "diag " << i;
        EXPECT_EQ(a->offset, b->offset) << "diag " << i;
    }
    yeptris_validate_report_free(dom);
    yeptris_validate_report_free(evt);
}

void pin_routes_both_families(const char* src) {
    pin_routes(src, strlen(src), YEPTRIS_SCHEMA_12_CORE);
    pin_routes(src, strlen(src), YEPTRIS_SCHEMA_12_JSON);
}

// The corpus: every family-lane shape that orders the two walks —
// multi-document attribution, staged duplicate order, collection and
// alias keys, flow and block shapes, tags, pooled positions.
TEST(ValidateStream, DifferentialCorpus) {
    const std::vector<const char*> corpus = {
        "",
        "a: 1\n",
        "k:\n",
        "--- 1\n--- !!int x\n",
        "---\na: 1\n---\nresources:\n  andale32:\n    sha256: !!int nope\n---\nc: 3\n",
        "--- {a: 1}\n--- {b: !!int x}\n",
        "--- !!int x\n--- 2\n",
        "--- 1\n---\n--- 3\n",
        "--- 1\n--- 2\n--- 3\n--- 4\n--- !!int bad\n--- 6\n",
        "%YAML 1.2\n---\nk: !!int bad\n---\nj: 2\n",
        "%TAG !e! tag:example.com,2000:\n---\n!e!thing\nv: 1\n",
        "a: 1\na: 2\na: 3\n",
        "a: 1\na: 2\nb: 1\nb: 2\n",
        "k: 0o13\nk: 0xB\n",
        "k: 1\nk: 1.0\n",
        "1: a\n1.0: b\n",
        "1.0: a\n1e0: b\n",
        "0x1F: a\n0x1f: b\n",
        "1e2: a\n100: b\n",
        "'q': a\nq: b\n",
        "\"\\x41\": a\nA: b\n",
        "t: 1\nt: 2\nt: 3\nt: 4\nt: 5\n",
        "{a: 1, a: 2}\n",
        "x:\n  a: 1\n  a: 2\n",
        "- a: 1\n- a: 2\n",
        "[1]: x\n[1]: y\n",
        "? [a, b]\n: v\n? [a, b]\n: w\n",
        "? {k: 1}\n: v\n? {k: 1}\n: w\n",
        "? {k: 1}\n: v\n? {k: 2}\n: w\n",
        "? k\n: v\n",
        "n: !!int abc\n",
        "a/b: !!int x\n",
        "x: !!seq not-a-seq\n",
        "- !!map [a]\n",
        "a: !!str 42\nb: !!int \"42\"\nc: !!int 0x1A\nd: !!bool true\ne: !!null\n",
        "\"name\": hello\n",
        "\"name\": \"hello\"\n",
        "y: \"v\"\n",
        "123: \"v\"\ntrue: \"w\"\n[1]: x\n",
        "~: \"v\"\nnull: \"v\"\n",
        "\"made\": !!timestamp 2001-12-14\n",
        "\"x\": !vendor thing\n",
        "x: !vendor thing\nmade: !!timestamp 2001-12-14\n",
        "\"a\": !!int 0x1A\n",
        "a: !!int 0x1A\n",
        "\"k\": 0o7\n",
        "k: 0o7\n",
        "\"k\": 0x1F\n",
        "a: yes\nb: n\nc: 1:30\nd: on\n",
        "n: !!int\n",
        "k: |\n  text\n",
        "k: >\n  fold\nd: !!int nope\n",
        "k: 'single'\nj: \"with \\\"esc\\\"\"\n",
        "'it''s': \"a\\nb\"\n",
        "k: \"\\x41\"\n",
        "ml: this is\n  a multi\n  line plain\nj: !!int nope\n",
        "empty: ''\n",
        "k: |\n  text\nj: !!int nope\n",
        "k: >-\n  fold\n  more\nj: !!int nope\n",
        "k: \"a\\nb\"\nj: !!int nope\n",
        "- |\n  x\n- !!int nope\n",
        "doc: &d\n  x: 1\nuse: *d\n",
        "x: &k 1\n? *k\n: v\nj: *k\n",
        "&a 1\n...\n---\nb: *a\n",
        "&s [*s, 1]\n",
        "list: &l\n  - 1\n  - 2\nother: *l\nbad: !!int x\n",
        "a: &v [1, 2]\nb: *v\nv2: &w {p: 1}\nw2: *w\n",
        "- &i 1\n- *i\n- &i 2\n- *i\n- !!int nope\n",
        "merge:\n  <<: {a: 1}\n  b: 2\n",
        "quoted: \"123\"\nqkey: !!int \"456\"\n",
        "--- 1\n--- 2\n--- 3\n--- 4\n--- !!int bad\n--- 6\n",
        "verylongkey_abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz: !!int x\n",
        "caf\xc3\xa9: !!int x\n-\xc3\xa9: !!int y\n",
        "a: 1 # comment\nb: !!int nope # another\n",
        "!!map\nk: !!int x\n",
        "!!seq\n- !!int nope\n",
        "!!str\n- !!int nope\n",
        "k: !!seq [1, 2]\nj: ok\n",
        "k: !!map {a: 1}\nj: ok\n",
        "{a: 1, b: {c: !!int x}}\n",
        "[{a: [1, {b: !!int y}]}]\n",
        "nested: {deep: [{deeper: !!int nope}]}\n",
        "- 1\n- [2, 3]\n- {a: 4}\n- !!int nope\n",
        "a:\n- x: 1\n- y: 2\nb: {c: !!int z}\n",
        "x: !!int 1\ny: !!int 2\nz: !!int bad\nw: !!int 4\n",
        "t: !!timestamp 2001-12-14\nc: !!binary aGVsbG8=\nm: !!merge x\nv: !!value y\nu: !!custom "
        "z\n",
        "s: !!bool [1]\n",
        "- !!set\n  ? a\n  : ~\n",
        "1: a\n'1': b\n",
        "0x1A: a\n0o13: b\n",
        "true: 1\nTrue: 2\n",
        "k: !!int 0x1F\nj: !!int 0x2A\n",
        "a: !!int nope\nb: !!int nope\nc: !!int nope\n",
        "l1:\n  l2:\n    l3:\n      l4:\n        l5:\n          - 1\n          - 2\n        k: "
        "!!int x\n",
        "k: {\n  a: 1,\n  b: 2\n}\nj: !!int nope\n",
        "k: [\n  1,\n  2\n]\nj: !!int nope\n",
        "a: {}\nb: []\nc: !!int nope\n",
        "a: !!seq {}\nb: !!map []\n",
        "{a: {b: {c: {d: !!int nope}}}}\n",
        "[[[[[[!!int nope]]]]]]\n",
        "# only a comment\n",
        "---\n---\n---\n",
        "--- !!int x\n--- !!int y\n--- !!int z\n",
        "a: 1\n...\n---\nb: !!int nope\n...\n",
        "key with spaces: !!int nope\n",
        "'quoted key': !!int nope\n\"dq key\": 1\n",
        "? !!int complex\n: v\n",
        "- {x: 1, x: 2}\n- {x: 1}\n",
    };
    for (const char* src : corpus) {
        pin_routes_both_families(src);
    }
}

// The report cap across one walk and across documents: the sentinel
// rides the walk order's last slot, identically on both routes.
TEST(ValidateStream, DifferentialCap) {
    std::string one;
    for (int i = 0; i < 300; i++) {
        one += "same: " + std::to_string(i) + "\n"; /* 299 dups + key lanes */
    }
    pin_routes_both_families(one.c_str());

    std::string many;
    for (int d = 0; d < 4; d++) {
        many += "---\nd" + std::to_string(d) + ": 1\n";
        for (int i = 0; i < 70; i++) {
            many += "k" + std::to_string(i) + ": !!int nope\n";
        }
    }
    pin_routes_both_families(many.c_str());

    // every miss in ONE pair region, then more documents after
    std::string mixed = "---\na: 1\n---\n";
    for (int i = 0; i < 150; i++) {
        mixed += "k" + std::to_string(i) + ": !!int nope\n";
    }
    mixed += "---\nz: 9\n";
    pin_routes_both_families(mixed.c_str());
}

// Parse failures and guards: statuses ride the same parse, reports stay
// NULL on both routes.
TEST(ValidateStream, DifferentialStatuses) {
    const std::vector<std::string> corpus = {
        "{unbalanced\n",
        "a: 1\n b: 2\n",
        "- a\n- b: [\n",
        "a: 'unterminated\n",
        "*never_defined\n",
        "k: !!int\n",
        std::string(1100, '[') + std::string(1100, ']') + "\n",
    };
    for (const std::string& s : corpus) {
        pin_routes(s.data(), s.size(), YEPTRIS_SCHEMA_12_CORE);
        pin_routes(s.data(), s.size(), YEPTRIS_SCHEMA_12_JSON);
    }
}

// The depth guard composes through the event route too: DEPTH, no
// report — alone and mid-stream.
TEST(ValidateStream, DepthGuardComposes) {
    std::string deep(1100, '[');
    deep.append(1100, ']');
    const std::string lone = deep + "\n";
    const std::string mid = "--- 1\n--- " + deep + "\n--- 3\n";
    for (const std::string* src : {&lone, &mid}) {
        yeptris_validate_report* r = NULL;
        EXPECT_EQ(yeptris_validate_stream(src->data(), src->size(), YEPTRIS_SCHEMA_12_CORE, &r),
                  YEPTRIS_ERROR_DEPTH);
        EXPECT_EQ(r, nullptr);
        yeptris_validate_report_free(r);
    }
}

// The face's own contract: argument gates byte-identical to the DOM
// route's, the empty stream, report ownership.
TEST(ValidateStream, Contract) {
    yeptris_validate_report* r = NULL;
    EXPECT_EQ(yeptris_validate_stream(NULL, 5, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(r, nullptr);
    EXPECT_EQ(yeptris_validate_stream("a: 1\n", 5, YEPTRIS_SCHEMA_11_COMPAT, &r),
              YEPTRIS_ERROR_ARG); /* a parse resolver, not a family */
    EXPECT_EQ(r, nullptr);
    EXPECT_EQ(yeptris_validate_stream("a: 1\n", 5, YEPTRIS_SCHEMA_12_CORE, NULL),
              YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_validate_stream(NULL, 0, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    yeptris_validate_report_free(r);
    // the empty input through the (NULL, 0) shape parses as empty
    EXPECT_EQ(yeptris_validate_stream("", 0, YEPTRIS_SCHEMA_12_JSON, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    yeptris_validate_report_free(r);
}

// The issue's shapes, pinned directly on the event route: the miss's
// document is named, line/col stay input-absolute, the empty middle
// document's unpositioned Error row still attributes.
TEST(ValidateStream, StreamAttribution) {
    yeptris_validate_report* r = NULL;
    const char* src = "---\na: 1\n"
                      "---\n"
                      "resources:\n"
                      "  andale32:\n"
                      "    sha256: !!int nope\n"
                      "---\nc: 3\n";
    ASSERT_EQ(yeptris_validate_stream(src, strlen(src), YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    const yeptris_diagnostic* d = yeptris_validate_report_at(r, 0);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    EXPECT_STREQ(d->path, "/1/resources/andale32/sha256");
    EXPECT_EQ(d->line, (uint32_t)6);
    yeptris_validate_report_free(r);

    // duplicate keys order: the misses precede the pairs' content —
    // the staged flush reproduces the DOM walk's order
    r = NULL;
    ASSERT_EQ(yeptris_validate_stream("a: !!int x\na: 1\n", strlen("a: !!int x\na: 1\n"),
                                      YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)2);
    EXPECT_EQ(yeptris_validate_report_at(r, 0)->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    EXPECT_EQ(yeptris_validate_report_at(r, 0)->line, (uint32_t)2);
    EXPECT_EQ(yeptris_validate_report_at(r, 1)->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    EXPECT_EQ(yeptris_validate_report_at(r, 1)->line, (uint32_t)1);
    yeptris_validate_report_free(r);

    // a literal block scalar takes the quoted law, and a plain KEY is
    // a string (spec 10.2 example 10.8 — the #908 law both routes
    // share): the document is clean under the JSON family on BOTH
    // routes
    r = NULL;
    ASSERT_EQ(yeptris_validate_stream("k: |\n  text\n", strlen("k: |\n  text\n"),
                                      YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    yeptris_validate_report_free(r);

    // the family's teeth survive the event route: a non-string KEY
    // rides the string-key demand, positioned at its bytes on BOTH
    // routes (the differential gate pins the pair)
    r = NULL;
    ASSERT_EQ(yeptris_validate_stream("123: |\n  text\n", strlen("123: |\n  text\n"),
                                      YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    d = yeptris_validate_report_at(r, 0);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    EXPECT_STREQ(d->path, "");
    EXPECT_EQ(d->line, (uint32_t)1);
    EXPECT_EQ(d->col, (uint32_t)1);
    yeptris_validate_report_free(r);

    // the empty document's scalar is a no-production plain: a STRING
    // under the example-10.8 law — the stream stays clean on both
    // routes
    r = NULL;
    ASSERT_EQ(yeptris_validate_stream("--- 1\n---\n--- 3\n", strlen("--- 1\n---\n--- 3\n"),
                                      YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    yeptris_validate_report_free(r);
}

// BOM'd and transcoded inputs: the offsets ride the DOM route's base
// law (the engine's bytes), so even the offset field matches.
TEST(ValidateStream, EncodedInputs) {
    const char* bom = "\xEF\xBB\xBF--- 1\n--- !!int x\n";
    pin_routes_both_families(bom);
    static const unsigned char u16[] = {0xFF, 0xFE, 'k', 0, ':', 0, ' ', 0, '!',  0, '!',  0,
                                        'i',  0,    'n', 0, 't', 0, ' ', 0, 'x',  0, '\n', 0,
                                        'j',  0,    ':', 0, ' ', 0, '2', 0, '\n', 0};
    pin_routes((const char*)u16, sizeof(u16), YEPTRIS_SCHEMA_12_CORE);
    pin_routes((const char*)u16, sizeof(u16), YEPTRIS_SCHEMA_12_JSON);
}

} // namespace
