/* test_ytape.cpp — the packed YAML record tape's gates (#378).
 *
 * Differential law first: replay ≡ direct build. Every corpus input
 * parses BOTH ways — the eager route (the engine building into the
 * DOM) and the tape route (the recorder packing, replay unpacking) —
 * and the built trees must be node-for-node identical (shape, spans,
 * tags, values). Then the lazy lifecycle: parse-only carries the
 * tape, first access materializes, free without access stays free,
 * and error precedence matches the eager route exactly.
 */

#include <string>
#include <vector>

#ifndef _WIN32
#include <dirent.h>
#endif

#include "doc.h"
#include "dom/dom.h"
#include "dom/ytape.h"
#include "tape_in.h"

#include <gtest/gtest.h>
#include <yeptris.h>

namespace {

const char* sv_bytes(const yep_dom* d, yep_sview sv, uint32_t* len) {
    *len = sv.len;
    if (sv.len == 0) {
        return "";
    }
    return (sv.off & YEP_SV_INPUT ? d->str : d->input_base) + (sv.off & YEP_SV_OFF);
}

::testing::AssertionResult doms_equal(const yep_dom* a, const yep_dom* b) {
    if (a->dcount != b->dcount) {
        return ::testing::AssertionFailure() << "dcount " << a->dcount << " vs " << b->dcount;
    }
    if (a->ncount != b->ncount) {
        return ::testing::AssertionFailure() << "ncount " << a->ncount << " vs " << b->ncount;
    }
    for (uint32_t i = 0; i < a->dcount; i++) {
        if (a->docs[i] != b->docs[i]) {
            return ::testing::AssertionFailure() << "doc[" << i << "]";
        }
    }
    for (uint32_t i = 0; i < a->ncount; i++) {
        const yep_dnode* x = &a->nodes[i];
        const yep_dnode* y = &b->nodes[i];
        if (x->kind != y->kind || x->style != y->style || x->tag_id != y->tag_id ||
            x->implicit != y->implicit || x->flow != y->flow || x->first_child != y->first_child ||
            x->next_sibling != y->next_sibling || x->target != y->target || x->count != y->count) {
            return ::testing::AssertionFailure() << "node[" << i << "] shape";
        }
        uint32_t la = 0, lb = 0;
        const char* pa = sv_bytes(a, x->value, &la);
        const char* pb = sv_bytes(b, y->value, &lb);
        if (la != lb || memcmp(pa, pb, la) != 0) {
            return ::testing::AssertionFailure()
                   << "node[" << i << "] value '" << std::string(pa, la).substr(0, 40) << "' vs '"
                   << std::string(pb, lb).substr(0, 40) << "'";
        }
    }
    return ::testing::AssertionSuccess();
}

} // namespace

TEST(YTape, ReplayedTreeMatchesDirectParse) {
    /* the corpus: every block shape the engine classifies (pairs,
     * opens, items), flow at both roots and nested, anchors and alias
     * targets across the four carriers (event, scalar, pair, flow),
     * quoted/escaped/folded/literal content (pool spans), explicit
     * tags, multi-document streams, empty collections, comments */
    const char* docs[] = {
        "a: 1\nb: two\n",
        "- 1\n- two\n- three\n",
        "- name: x\n  values: [1, 2, 3]\n- name: y\n  values: []\n",
        "outer:\n  inner:\n    deep: {k: v, k2: [a, b]}\n",
        "flow: {a: 1, b: [x, y, {c: d}]}\n",
        "[1, 2.5, -3, true, false, null, plain]\n",
        "{a: 1}\n",
        "empty-map: {}\nempty-seq: []\n",
        "\"double quoted\": value\n'single': value2\n",
        "esc: \"a\\\"q\\\\b\\ne\\u00e9\"\n",
        "folded: >\n  line one\n  line two\n\n  after blank\n",
        "literal: |\n  raw line 1\n  raw line 2\n",
        "plain multiline: this is\n  one long scalar\n",
        "anchored: &a value\nref: *a\n",
        "base: &b\n  x: 1\nuser:\n  <<: *b\n  y: 2\n",
        "list_anchor: &L\n  - 1\n  - 2\nagain: *L\n",
        "!!str tagged: value\nexplicit: !!int 42\n",
        "tagged anchor: &t !!str hello\ntarget: *t\n",
        "key anchor: &k v\npair: {&ik ik: *k}\n",
        "---\na: 1\n---\nb: 2\n",
        /* the flow-root-with-doc-start gap (found by the canonical
         * round-trip after the flip): DOCUMENT_START then a FLOW build */
        "---\n{\"a\": 1, \"b\": [\"x\", \"y\"], \"c\": {\"d\": 2}}\n",
        "--- [1, 2, 3]\n",
        "---\n---\n",
        "---\n- a\n--- \n- b\n...\n",
        "# leading comment\nkey: value # trailing\n# end\n",
        "? complex\n: key\n",
        "nested sequences:\n  - - a\n    - b\n  - - c\n",
        "deep:\n"
        "  -\n"
        "    -\n"
        "      - bottom\n",
        "num: 42\nneg: -7\nflt: 3.25\nexp: 1e3\n",
        "empty: \ntab_free: ok\n",
        "%YAML 1.2\n---\ndoc: after directive\n",
        "a: &x [1, 2]\nb: *x\nc: &y {m: 1}\nd: *y\n",
    };
    for (const char* doc : docs) {
        size_t len = strlen(doc);
        YeptrisStatus st = YEPTRIS_OK, st2 = YEPTRIS_OK;
        YeptrisDocument direct = yeptris_parse_ex(doc, len, NULL, &st);
        ASSERT_NE(direct, nullptr) << doc;
        ASSERT_EQ(st, YEPTRIS_OK) << doc;
        YeptrisDocument lazy = yeptris_parse_ytape_ex(doc, len, NULL, &st2);
        ASSERT_NE(lazy, nullptr) << doc;
        ASSERT_EQ(st2, YEPTRIS_OK) << doc;

        /* parse-only: no tree built */
        EXPECT_EQ(((yeptris_document*)lazy)->dom, nullptr) << doc;
        EXPECT_NE(((yeptris_document*)lazy)->lazy_tape, nullptr) << doc;

        const yep_dom* a = yep_doc_dom((yeptris_document*)direct);
        const yep_dom* b = yep_doc_dom((yeptris_document*)lazy);
        ASSERT_NE(a, nullptr) << doc;
        ASSERT_NE(b, nullptr) << doc;
        EXPECT_TRUE(doms_equal(a, b)) << doc;
        yeptris_document_free(direct);
        yeptris_document_free(lazy);
    }
}

#ifndef _WIN32
TEST(YTape, PsychPureCorpusMatches) {
    /* the in-tree raw-YAML corpus: every file parses both ways, trees
     * identical — the breadth arm of the differential gate */
    const char* dir = TEST_YTAPE_CORPUS_DIR "/psych-pure";
    DIR* dp = opendir(dir);
    ASSERT_NE(dp, nullptr) << dir;
    struct dirent* de = nullptr;
    unsigned tried = 0;
    while ((de = readdir(dp)) != nullptr) {
        size_t n = strlen(de->d_name);
        if (n < 5 || strcmp(de->d_name + n - 5, ".yaml") != 0) {
            continue;
        }
        std::string path = std::string(dir) + "/" + de->d_name;
        FILE* f = fopen(path.c_str(), "rb");
        ASSERT_NE(f, nullptr) << path;
        std::string body;
        char buf[4096];
        size_t got = 0;
        while ((got = fread(buf, 1, sizeof(buf), f)) > 0) {
            body.append(buf, got);
        }
        fclose(f);

        YeptrisStatus st1 = YEPTRIS_OK, st2 = YEPTRIS_OK;
        YeptrisDocument direct = yeptris_parse_ex(body.data(), body.size(), NULL, &st1);
        YeptrisDocument lazy = yeptris_parse_ytape_ex(body.data(), body.size(), NULL, &st2);
        SCOPED_TRACE(path);
        tried++; /* every file parity-checks; error fixtures too */
        ASSERT_EQ(st1, st2) << path;
        ASSERT_EQ((direct != nullptr), (lazy != nullptr)) << path;
        if (direct == nullptr) {
            continue; /* an expected-parse-error fixture: parity holds */
        }
        EXPECT_TRUE(doms_equal(yep_doc_dom((yeptris_document*)direct),
                               yep_doc_dom((yeptris_document*)lazy)))
            << path;
        yeptris_document_free(direct);
        yeptris_document_free(lazy);
        tried++;
    }
    closedir(dp);
    EXPECT_GT(tried, 10u) << "the corpus sweep ran";
}
#endif

TEST(YTape, MaterializedTreeSurvivesMutationAndSerialize) {
    const char* doc = "a: 1\nb:\n  - x\n  - y\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse_ytape_ex(doc, strlen(doc), NULL, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(d, nullptr);

    size_t len = 0;
    char* out = yeptris_serialize(d, &len);
    ASSERT_NE(out, nullptr);
    const char* want = "a: 1\nb:\n- x\n- y\n";
    ASSERT_EQ(len, strlen(want));
    EXPECT_EQ(memcmp(out, want, len), 0);
    yeptris_free(out);
    yeptris_document_free(d);
}

TEST(YTape, FreeWithoutAccessNeverMaterializes) {
    const char* doc = "a: [1, 2, 3]\nb: {c: d}\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse_ytape_ex(doc, strlen(doc), NULL, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(d, nullptr);
    yeptris_document* doc_p = (yeptris_document*)d;
    EXPECT_EQ(doc_p->dom, nullptr);
    EXPECT_EQ(doc_p->lazy_kind, 1);
    yeptris_document_free(d); /* no tree access at all */
}

TEST(YTape, ErrorParityWithEagerParse) {
    const char* bad[] = {
        "a: [1, 2\n",        /* unclosed flow */
        "key: *undefined\n", /* alias without anchor */
        "{a: 1\n",           /* broken flow map */
        "a:\n- b\n  c: d\n", /* bad indentation */
        "\"unterminated\n",
    };
    for (const char* s : bad) {
        YeptrisStatus st1 = YEPTRIS_OK, st2 = YEPTRIS_OK;
        YeptrisDocument e1 = yeptris_parse_ex(s, strlen(s), NULL, &st1);
        YeptrisDocument e2 = yeptris_parse_ytape_ex(s, strlen(s), NULL, &st2);
        EXPECT_EQ(st1, st2) << s;
        EXPECT_EQ((e1 != nullptr), (e2 != nullptr)) << s;
        yeptris_document_free(e1);
        yeptris_document_free(e2);
    }
}

TEST(YTape, EmptyStreamReturnsNullBothWays) {
    YeptrisStatus st1 = YEPTRIS_OK, st2 = YEPTRIS_OK;
    YeptrisDocument e1 = yeptris_parse_ex("", 0, NULL, &st1);
    YeptrisDocument e2 = yeptris_parse_ytape_ex("", 0, NULL, &st2);
    EXPECT_EQ(st1, YEPTRIS_OK);
    EXPECT_EQ(st2, YEPTRIS_OK);
    EXPECT_EQ(e1, nullptr);
    EXPECT_EQ(e2, nullptr);
    yeptris_document_free(e1);
    yeptris_document_free(e2);
}

TEST(YTape, Compat11SchemaMatches) {
    const char* doc = "y: yes\nn: no\noct: 0o17\ntruthy: on\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisParseOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.schema = YEPTRIS_SCHEMA_11_COMPAT;
    YeptrisDocument direct = yeptris_parse_ex(doc, strlen(doc), &opts, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(direct, nullptr);
    YeptrisDocument lazy = yeptris_parse_ytape_ex(doc, strlen(doc), &opts, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(lazy, nullptr);
    EXPECT_TRUE(
        doms_equal(yep_doc_dom((yeptris_document*)direct), yep_doc_dom((yeptris_document*)lazy)))
        << doc;
    yeptris_document_free(direct);
    yeptris_document_free(lazy);
}
