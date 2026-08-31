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

TEST(Events, AnchorsTagsAndAliasesSurviveTheModels) {
    const char* in = "a: &x !!str tagged\nb: *x\n";
    auto events = collect_pull(in, strlen(in));
    /* SS +DOC +MAP =VAL a =VAL &x <...> tagged =VAL b =ALI *x -MAP -DOC -STR */
    ASSERT_EQ(events.size(), 10u);
    EXPECT_EQ(events[4], "=VAL : &x <tag:yaml.org,2002:str> tagged");
    EXPECT_EQ(events[6], "=ALI *x");
}
