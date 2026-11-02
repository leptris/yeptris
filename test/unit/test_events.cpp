/* test_events.cpp — consumption-model contracts (TODO.impl/12): event
 * shapes, validity windows, batch counts, recorder drain, iterparse
 * document boundaries, error surfacing. The corpus-wide equality of
 * all models is test_events_crosscheck; these pin the contracts. */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <yeptris.h>

namespace {

const char* kMulti = "a: 1\n---\nb: [2, 3]\n---\n# empty\nc: x\n";
const char* kOne = "k: v\n";

std::string ev_text(const YeptrisEvent* e) {
    switch (e->type) {
    case YEPTRIS_EV_STREAM_START:
        return "+STR";
    case YEPTRIS_EV_STREAM_END:
        return "-STR";
    case YEPTRIS_EV_DOCUMENT_START:
        return e->explicit_marker ? "+DOC ---" : "+DOC";
    case YEPTRIS_EV_DOCUMENT_END:
        return e->explicit_marker ? "-DOC ..." : "-DOC";
    case YEPTRIS_EV_MAPPING_START:
        return e->flow ? "+MAP {}" : "+MAP";
    case YEPTRIS_EV_MAPPING_END:
        return "-MAP";
    case YEPTRIS_EV_SEQUENCE_START:
        return e->flow ? "+SEQ []" : "+SEQ";
    case YEPTRIS_EV_SEQUENCE_END:
        return "-SEQ";
    case YEPTRIS_EV_ALIAS:
        return std::string("=ALI *") + std::string(e->value, e->value_len);
    case YEPTRIS_EV_SCALAR: {
        static const char sc[6] = {'?', ':', '\'', '"', '|', '>'};
        char c = sc[e->style < 6 ? e->style : 0];
        std::string t = std::string("=VAL ") + c;
        if (e->anchor_len > 0) {
            t += " &" + std::string(e->anchor, e->anchor_len);
        }
        if (e->tag_len > 0) {
            t += " <" + std::string(e->tag, e->tag_len) + ">";
        }
        return t + " " + std::string(e->value ? e->value : "", e->value_len);
    }
    }
    return "?";
}

std::vector<std::string> collect_push(const char* in, size_t len, YeptrisStatus* st) {
    struct Ctx {
        std::vector<std::string> out;
    } ctx;
    auto fn = [](void* v, const YeptrisEvent* e) -> int {
        ((Ctx*)v)->out.push_back(ev_text(e));
        return 0;
    };
    YeptrisStatus got = yeptris_push_parse(in, len, fn, &ctx);
    if (st != NULL) {
        *st = got;
    }
    return ctx.out;
}

std::vector<std::string> collect_pull(const char* in, size_t len) {
    std::vector<std::string> out;
    YeptrisPullParser p = yeptris_pull_new(in, len);
    const YeptrisEvent* e;
    while ((e = yeptris_pull_next(p)) != NULL) {
        out.push_back(ev_text(e));
    }
    yeptris_pull_free(p);
    return out;
}

std::vector<std::string> collect_recorder(const char* in, size_t len) {
    std::vector<std::string> out;
    YeptrisRecorder r = yeptris_recorder_new();
    EXPECT_EQ(yeptris_recorder_feed(r, in, len, 1), YEPTRIS_OK);
    size_t n = 0;
    const YeptrisEventRecord* recs = yeptris_recorder_records(r, &n);
    size_t alen = 0;
    const char* arena = yeptris_recorder_arena(r, &alen);
    for (size_t i = 0; i < n; i++) {
        YeptrisEvent e;
        memset(&e, 0, sizeof(e));
        e.type = (YeptrisEventType)recs[i].type;
        e.style = recs[i].style;
        e.flow = (recs[i].flags & YEPTRIS_EF_FLOW) != 0;
        e.explicit_marker = (recs[i].flags & YEPTRIS_EF_EXPLICIT) != 0;
        if (recs[i].value_len > 0) {
            e.value = arena + recs[i].value_off;
            e.value_len = recs[i].value_len;
        }
        if (recs[i].anchor_len > 0) {
            e.anchor = arena + recs[i].anchor_off;
            e.anchor_len = recs[i].anchor_len;
        }
        if (recs[i].tag_len > 0) {
            e.tag = arena + recs[i].tag_off;
            e.tag_len = recs[i].tag_len;
        }
        out.push_back(ev_text(&e));
    }
    yeptris_recorder_free(r);
    return out;
}

} // namespace

TEST(Events, PushMatchesPullMatchesRecorder) {
    auto a = collect_push(kMulti, strlen(kMulti), nullptr);
    auto b = collect_pull(kMulti, strlen(kMulti));
    auto c = collect_recorder(kMulti, strlen(kMulti));
    ASSERT_EQ(a.size(), b.size());
    EXPECT_EQ(a, b);
    ASSERT_EQ(a.size(), c.size());
    EXPECT_EQ(a, c);
}

TEST(Events, PullValidityWindowAndBatch) {
    YeptrisPullParser p = yeptris_pull_new(kOne, strlen(kOne));
    ASSERT_NE(p, nullptr);
    const YeptrisEvent* e = yeptris_pull_next(p);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->type, YEPTRIS_EV_STREAM_START);
    const char* v0 = e->value; /* NULL here, but the pointer rule must hold */
    (void)v0;
    /* every pointer from call N is invalid at call N+1 */
    e = yeptris_pull_next(p);
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->type, YEPTRIS_EV_DOCUMENT_START);
    /* batch drains the rest exactly */
    YeptrisEvent buf[16];
    size_t total = 0, n;
    while ((n = yeptris_pull_next_batch(p, buf, 4)) > 0) {
        total += n;
    }
    /* kOne = STR,DOC,MAP,k,v,-MAP,-DOC,-STR = 8; two already consumed */
    EXPECT_EQ(total, 6u);
    /* exhausted */
    EXPECT_EQ(yeptris_pull_next(p), nullptr);
    EXPECT_EQ(yeptris_pull_status(p, nullptr, nullptr), YEPTRIS_OK);
    yeptris_pull_free(p);
}

TEST(Events, RecorderChunkedFeedDrainsBulk) {
    YeptrisRecorder r = yeptris_recorder_new();
    ASSERT_NE(r, nullptr);
    /* feed in three chunks: nothing recorded until final */
    const char* in = kOne;
    size_t len = strlen(in);
    ASSERT_EQ(yeptris_recorder_feed(r, in, len / 2, 0), YEPTRIS_OK);
    size_t n = 99;
    yeptris_recorder_records(r, &n);
    EXPECT_EQ(n, 0u);
    ASSERT_EQ(yeptris_recorder_feed(r, in + len / 2, len - len / 2, 0), YEPTRIS_OK);
    yeptris_recorder_records(r, &n);
    EXPECT_EQ(n, 0u);
    ASSERT_EQ(yeptris_recorder_feed(r, "", 0, 1), YEPTRIS_OK);
    yeptris_recorder_records(r, &n);
    EXPECT_EQ(n, 8u); /* kOne's full stream */
    size_t alen = 0;
    EXPECT_NE(yeptris_recorder_arena(r, &alen), nullptr);
    yeptris_recorder_free(r);
}

TEST(Events, IterparseYieldsPerDocument) {
    YeptrisIterparse it = yeptris_iterparse_new(kMulti, strlen(kMulti));
    ASSERT_NE(it, nullptr);
    size_t n = 0;
    const YeptrisEvent* evs;
    int docs = 0;
    int saw_stream_start = 0, saw_stream_end = 0;
    std::vector<std::string> joined;
    while ((evs = yeptris_iterparse_next(it, &n)) != NULL) {
        for (size_t i = 0; i < n; i++) {
            joined.push_back(ev_text(&evs[i]));
            if (evs[i].type == YEPTRIS_EV_STREAM_START) {
                saw_stream_start++;
            }
            if (evs[i].type == YEPTRIS_EV_STREAM_END) {
                saw_stream_end++;
            }
            if (evs[i].type == YEPTRIS_EV_DOCUMENT_START) {
                docs++;
            }
        }
    }
    EXPECT_EQ(docs, 3);
    EXPECT_EQ(saw_stream_start, 1);
    EXPECT_EQ(saw_stream_end, 1);
    /* the concatenation equals the whole-stream push dump */
    auto push = collect_push(kMulti, strlen(kMulti), nullptr);
    ASSERT_EQ(push.size(), joined.size());
    EXPECT_EQ(push, joined);
    EXPECT_EQ(yeptris_iterparse_status(it, nullptr, nullptr), YEPTRIS_OK);
    yeptris_iterparse_free(it);
}

TEST(Events, ErrorsSurfaceInEveryModel) {
    const char* bad = "a: [1, 2\n"; /* unterminated flow */
    size_t len = strlen(bad);
    YeptrisStatus st = YEPTRIS_OK;
    (void)collect_push(bad, len, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    YeptrisPullParser p = yeptris_pull_new(bad, len);
    EXPECT_NE(p, nullptr);
    uint32_t line = 0, col = 0;
    EXPECT_EQ(yeptris_pull_status(p, &line, &col), YEPTRIS_ERROR_PARSE);
    EXPECT_GT(line, 0u);
    yeptris_pull_free(p);
    YeptrisRecorder r = yeptris_recorder_new();
    EXPECT_EQ(yeptris_recorder_feed(r, bad, len, 1), YEPTRIS_ERROR_PARSE);
    size_t n = 0;
    yeptris_recorder_records(r, &n);
    EXPECT_GT(n, 0u); /* partial events before the failure point */
    yeptris_recorder_free(r);
    YeptrisIterparse it = yeptris_iterparse_new(bad, len);
    size_t dn;
    while (yeptris_iterparse_next(it, &dn) != NULL) {}
    EXPECT_EQ(yeptris_iterparse_status(it, nullptr, nullptr), YEPTRIS_ERROR_PARSE);
    yeptris_iterparse_free(it);
}

/* yeptris-ruby#258: libyaml never validates a block-scalar header's
 * column — only the content must out-indent the parent. A "|" / ">"
 * on its own line at the key's column is the pending key's value
 * (metanorma-cli's collection fixture shape); a stray one after a
 * completed pair still rejects. */
TEST(Events, BlockScalarHeaderAtKeyColumn) {
    const char* in = "prefatory-content:\n|\n  == Clause\n";
    auto events = collect_pull(in, strlen(in));
    /* SS +DOC +MAP =VAL prefatory-content =VAL |<literal> -MAP -DOC -STR */
    ASSERT_EQ(events.size(), 8u) << "the header is the pending value";
    EXPECT_EQ(events[4], "=VAL | == Clause\n") << events[4];

    const char* folded = "prefatory-content:\n>-\n  folded text\n";
    auto fe = collect_pull(folded, strlen(folded));
    ASSERT_EQ(fe.size(), 8u) << "folded strips as libyaml does";
    EXPECT_EQ(fe[4], "=VAL > folded text") << fe[4];

    const char* stray = "k: 1\n|\n  stray\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse(stray, strlen(stray), &st);
    EXPECT_NE(st, YEPTRIS_OK) << "no pending key: still a syntax error";
    yeptris_document_free(d);
}

TEST(Events, AnchorsTagsAndAliasesSurviveTheModels) {
    const char* in = "a: &x !!str tagged\nb: *x\n";
    auto events = collect_pull(in, strlen(in));
    /* SS +DOC +MAP =VAL a =VAL &x <...> tagged =VAL b =ALI *x -MAP -DOC -STR */
    ASSERT_EQ(events.size(), 10u);
    EXPECT_EQ(events[4], "=VAL : &x <tag:yaml.org,2002:str> tagged");
    EXPECT_EQ(events[6], "=ALI *x");
}

/* #431 follow-up: the recorder's engine rode the flow indent floor on
 * the compat schema too — the py safe_load path (recorder_new_ex +
 * feed) rejected the multi-line flows libyaml accepts, the exact
 * rejection the parse_ex wiring had fixed. Compat accepts, core keeps
 * the floor (the suite's rejection). */
TEST(Events, CompatRecorderAcceptsMultiLineFlow) {
    const char* y = "a: {\n  x: 1\n}\nb: 3\n";

    YeptrisRecorder compat = yeptris_recorder_new_ex(YEPTRIS_SCHEMA_11_COMPAT);
    ASSERT_EQ(yeptris_recorder_feed(compat, y, strlen(y), 1), YEPTRIS_OK);
    size_t n = 0;
    const YeptrisEventRecord* recs = yeptris_recorder_records(compat, &n);
    size_t alen = 0;
    const char* arena = yeptris_recorder_arena(compat, &alen);
    ASSERT_GT(n, 0u);
    bool saw_scalar_b = false;
    for (size_t i = 0; i < n; i++) {
        if (recs[i].value_len == 1 && arena[recs[i].value_off] == 'b') {
            saw_scalar_b = true;
        }
    }
    EXPECT_TRUE(saw_scalar_b) << "the sibling key after the flow must parse";
    yeptris_recorder_free(compat);

    YeptrisRecorder core = yeptris_recorder_new_ex(YEPTRIS_SCHEMA_12_CORE);
    EXPECT_EQ(yeptris_recorder_feed(core, y, strlen(y), 1), YEPTRIS_ERROR_PARSE)
        << "the strict 1.2 engine keeps the yaml-test-suite floor";
    yeptris_recorder_free(core);
}

/* ---- audit round 5: the cross-model mark law and the encoding gates ---- */

namespace {

struct Mark4 {
    int type;
    uint32_t line, col, end_line, end_col;
};

std::vector<Mark4> marks_push(const char* in, size_t len) {
    std::vector<Mark4> out;
    auto fn = [](void* v, const YeptrisEvent* e) -> int {
        ((std::vector<Mark4>*)v)
            ->push_back(Mark4{(int)e->type, e->line, e->col, e->end_line, e->end_col});
        return 0;
    };
    EXPECT_EQ(yeptris_push_parse(in, len, fn, &out), YEPTRIS_OK);
    return out;
}

std::vector<Mark4> marks_pull(const char* in, size_t len) {
    std::vector<Mark4> out;
    YeptrisPullParser p = yeptris_pull_new(in, len);
    const YeptrisEvent* e;
    while ((e = yeptris_pull_next(p)) != NULL) {
        out.push_back(Mark4{(int)e->type, e->line, e->col, e->end_line, e->end_col});
    }
    yeptris_pull_free(p);
    return out;
}

int iterparse_docs(const char* in, size_t len) {
    YeptrisIterparse it = yeptris_iterparse_new(in, len);
    size_t n = 0;
    const YeptrisEvent* ev;
    int docs = 0;
    while ((ev = yeptris_iterparse_next(it, &n)) != NULL) {
        for (size_t i = 0; i < n; i++) {
            if (ev[i].type == YEPTRIS_EV_DOCUMENT_START) {
                docs++;
            }
        }
    }
    EXPECT_EQ(yeptris_iterparse_status(it, nullptr, nullptr), YEPTRIS_OK);
    yeptris_iterparse_free(it);
    return docs;
}

} // namespace

/* (c) cross-model identity: push carried only the START marks — every
 * event's end line/col read 0 there while pull/recorder carried the
 * engine's. The routes must be mark-identical on a multi-line doc. */
TEST(Events, PushMarksMatchPullMarks) {
    const char* doc = "a: 1\nb:\n  - foo\n  - bar\nc: &x val\nd: *x\n"
                      "e: \"multi\n  line\"\nf: |\n  lit\ng: {k: [1, 2]}\n";
    auto a = marks_push(doc, strlen(doc));
    auto b = marks_pull(doc, strlen(doc));
    ASSERT_GT(a.size(), 10u);
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); i++) {
        EXPECT_EQ(a[i].type, b[i].type) << "event " << i;
        EXPECT_EQ(a[i].line, b[i].line) << "event " << i;
        EXPECT_EQ(a[i].col, b[i].col) << "event " << i;
        EXPECT_EQ(a[i].end_line, b[i].end_line) << "event " << i;
        EXPECT_EQ(a[i].end_col, b[i].end_col) << "event " << i;
    }
}

/* (a) the step route's encoding gate: UTF-16 fed through the recorder
 * (yep_engine_step) used to parse garbage silently — the parse route
 * rejects with YEPTRIS_ERROR_ENCODING, and the chunked route must
 * produce the same verdict. line:col is the engine's (1:1). */
TEST(Events, RecorderFeedGatesNonUtf8Encodings) {
    const unsigned char le[] = {0xFF, 0xFE, 'a', 0, ':', 0, ' ', 0, '1', 0, '\n', 0};
    YeptrisRecorder r = yeptris_recorder_new();
    uint32_t line = 0, col = 0;
    EXPECT_EQ(yeptris_recorder_feed(r, (const char*)le, sizeof(le), 1), YEPTRIS_ERROR_ENCODING);
    EXPECT_EQ(yeptris_recorder_status(r, &line, &col), YEPTRIS_ERROR_ENCODING);
    EXPECT_EQ(line, 1u);
    EXPECT_EQ(col, 1u);
    yeptris_recorder_free(r);

    /* a BOM split across feeds: the gate holds until the head can
     * decide, then fires — never a half-mark read as content */
    YeptrisRecorder r2 = yeptris_recorder_new();
    EXPECT_EQ(yeptris_recorder_feed(r2, "\xFF", 1, 0), YEPTRIS_OK);
    EXPECT_EQ(yeptris_recorder_feed(r2, "\xFE a", 3, 1), YEPTRIS_ERROR_ENCODING);
    yeptris_recorder_free(r2);
}

/* the UTF-8 BOM on the step route: strips like the parse route's
 * front-end, including when it splits across feeds; a lone mark is an
 * empty stream (the parse route's round-122 law) */
TEST(Events, RecorderFeedStripsUtf8BomLikeParse) {
    const char* doc = "k: v\n";
    std::vector<std::string> base, marked;
    {
        YeptrisRecorder r = yeptris_recorder_new();
        ASSERT_EQ(yeptris_recorder_feed(r, doc, strlen(doc), 1), YEPTRIS_OK);
        size_t n = 0;
        const YeptrisEventRecord* rs = yeptris_recorder_records(r, &n);
        for (size_t i = 0; i < n; i++) {
            base.push_back(std::string(1, (char)rs[i].type));
        }
        yeptris_recorder_free(r);
    }
    {
        const char* bom = "\xEF\xBB\xBF"
                          "k: v\n";
        YeptrisRecorder r = yeptris_recorder_new();
        ASSERT_EQ(yeptris_recorder_feed(r, bom, strlen(bom), 1), YEPTRIS_OK);
        size_t n = 0;
        const YeptrisEventRecord* rs = yeptris_recorder_records(r, &n);
        for (size_t i = 0; i < n; i++) {
            marked.push_back(std::string(1, (char)rs[i].type));
        }
        yeptris_recorder_free(r);
    }
    ASSERT_EQ(base.size(), marked.size());
    EXPECT_EQ(base, marked);

    /* split across feeds: "\xEF\xBB" holds, "\xBF..." completes it */
    {
        YeptrisRecorder r = yeptris_recorder_new();
        ASSERT_EQ(yeptris_recorder_feed(r, "\xEF\xBB", 2, 0), YEPTRIS_OK);
        ASSERT_EQ(yeptris_recorder_feed(r,
                                        "\xBF"
                                        "k: v\n",
                                        6, 1),
                  YEPTRIS_OK);
        size_t n = 0;
        yeptris_recorder_records(r, &n);
        EXPECT_EQ(n, base.size());
        yeptris_recorder_free(r);
    }
    /* a lone mark: +STR/-STR, no document (mirror of Parse.BomOnly) */
    {
        YeptrisRecorder r = yeptris_recorder_new();
        ASSERT_EQ(yeptris_recorder_feed(r, "\xEF\xBB\xBF", 3, 1), YEPTRIS_OK);
        size_t n = 0;
        yeptris_recorder_records(r, &n);
        EXPECT_EQ(n, 2u);
        yeptris_recorder_free(r);
    }
}

/* (b) the resume offset must live in the space the engine's pos
 * counts: a head BOM the front-end strips, a UTF-16 transcode, and the
 * NEL break normalizer each used to skew it (bogus slices, a spurious
 * parse error, corrupted documents). Every document still parses. */
TEST(Events, IterparseResumeSurvivesEncodingFrontEnd) {
    const char* plain = "a: 1\n---\nb: 2\n---\nc: 3\n";
    EXPECT_EQ(iterparse_docs(plain, strlen(plain)), 3);

    const char* bom = "\xEF\xBB\xBF"
                      "a: 1\n---\nb: 2\n---\nc: 3\n";
    EXPECT_EQ(iterparse_docs(bom, strlen(bom)), 3) << "the mark is not a re-parse seed";

    /* one NEL (indented plain continuation — the shape both routes
     * accept) inside the first document: the normalizer's buffer is
     * shorter than the raw one, and the old cursor arithmetic parsed
     * from mid-document (a spurious parse error) */
    const char* nel = "a: 1\xC2\x85 z\n---\nb: 2\n---\nc: 3\n";
    EXPECT_EQ(iterparse_docs(nel, strlen(nel)), 3);

    /* units kept adjacent (a mis-paired literal sniffs as garbage) */
    const unsigned char le[] = {0xFF, 0xFE, 'a', 0, ':',  0, ' ',  0, '1', 0, '\n', 0,
                                '-',  0,    '-', 0, '-',  0, '\n', 0, 'b', 0, ':',  0,
                                ' ',  0,    '2', 0, '\n', 0, '-',  0, '-', 0, '-',  0,
                                '\n', 0,    'c', 0, ':',  0, ' ',  0, '3', 0, '\n', 0};
    EXPECT_EQ(iterparse_docs((const char*)le, sizeof(le)), 3) << "UTF-16LE transcode + resume";
}
