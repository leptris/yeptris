/* test_yaml_compat.cpp — the libyaml event adapter (TODO.impl/12C). */

#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include "events/yaml_compat.h"
#include "memory/allocator.h"
#include "parse/engine.h"

namespace {

std::string run_first_types(const char* y, yep_ly_event* evs, int max, int* n) {
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    struct Cap {
        yep_ly_event* evs;
        int max;
        int* n;
    } cap = {evs, max, n};
    *n = 0;
    auto sink_fn = [](void* ctx, const yep_event* ev) -> int {
        Cap* c = (Cap*)ctx;
        if (*c->n < c->max) {
            yep_ly_translate(&c->evs[*c->n], ev);
        }
        (*c->n)++;
        return 0;
    };
    yep_sink sink = {sink_fn, &cap};
    int rc = yep_engine_run(eng, y, strlen(y), &sink);
    yep_engine_destroy(eng);
    return rc == 0 ? "ok" : "error";
}

} // namespace

TEST(YamlCompat, TypeOrderMatchesLibyaml) {
    /* enum values are load-bearing: drivers port their switches
     * verbatim against them */
    EXPECT_EQ(YEP_LY_NO_EVENT, 0);
    EXPECT_EQ(YEP_LY_STREAM_START_EVENT, 1);
    EXPECT_EQ(YEP_LY_STREAM_END_EVENT, 2);
    EXPECT_EQ(YEP_LY_DOCUMENT_START_EVENT, 3);
    EXPECT_EQ(YEP_LY_DOCUMENT_END_EVENT, 4);
    EXPECT_EQ(YEP_LY_ALIAS_EVENT, 5);
    EXPECT_EQ(YEP_LY_SCALAR_EVENT, 6);
    EXPECT_EQ(YEP_LY_SEQUENCE_START_EVENT, 7);
    EXPECT_EQ(YEP_LY_SEQUENCE_END_EVENT, 8);
    EXPECT_EQ(YEP_LY_MAPPING_START_EVENT, 9);
    EXPECT_EQ(YEP_LY_MAPPING_END_EVENT, 10);
    EXPECT_EQ(YEP_LY_PLAIN_STYLE, 1);
    EXPECT_EQ(YEP_LY_SINGLE_QUOTED_STYLE, 2);
    EXPECT_EQ(YEP_LY_DOUBLE_QUOTED_STYLE, 3);
    EXPECT_EQ(YEP_LY_LITERAL_STYLE, 4);
    EXPECT_EQ(YEP_LY_FOLDED_STYLE, 5);
}

TEST(YamlCompat, ScalarTranslation) {
    yep_ly_event evs[12];
    int n = 0;
    EXPECT_EQ(run_first_types("a: 'q'\nb: &x 1\nc: *x\n", evs, 12, &n), "ok");
    /* +STR +DOC +MAP =VAL(a) =VAL(q,single,quoted_implicit) ... */
    ASSERT_GE(n, 7);
    EXPECT_EQ(evs[2].type, YEP_LY_MAPPING_START_EVENT);
    EXPECT_EQ(evs[2].style, YEP_LY_BLOCK_STYLE);
    EXPECT_EQ(evs[2].implicit, 1);
    EXPECT_EQ(evs[3].type, YEP_LY_SCALAR_EVENT);
    EXPECT_EQ(evs[3].plain_implicit, 1);
    EXPECT_EQ(evs[3].quoted_implicit, 0);
    EXPECT_EQ(evs[3].style, YEP_LY_PLAIN_STYLE);
    EXPECT_EQ(std::string(evs[3].value, evs[3].value_len), "a");
    EXPECT_EQ(evs[4].style, YEP_LY_SINGLE_QUOTED_STYLE);
    EXPECT_EQ(evs[4].plain_implicit, 0);
    EXPECT_EQ(evs[4].quoted_implicit, 1);
    /* b: &x 1 — the VALUE carries the anchor (index 6) */
    ASSERT_GE(n, 9);
    EXPECT_EQ(evs[6].type, YEP_LY_SCALAR_EVENT);
    EXPECT_EQ(std::string(evs[6].anchor, evs[6].anchor_len), "x");
    /* c: *x — alias (index 8) */
    EXPECT_EQ(evs[8].type, YEP_LY_ALIAS_EVENT);
    EXPECT_EQ(std::string(evs[8].anchor, evs[8].anchor_len), "x");
}

TEST(YamlCompat, DocumentsAndMarks) {
    yep_ly_event evs[8];
    int n = 0;
    EXPECT_EQ(run_first_types("--- 1\n...\n", evs, 8, &n), "ok");
    ASSERT_GE(n, 5);
    EXPECT_EQ(evs[1].type, YEP_LY_DOCUMENT_START_EVENT);
    EXPECT_EQ(evs[1].implicit, 0); /* explicit --- */
    EXPECT_EQ(evs[2].type, YEP_LY_SCALAR_EVENT);
    EXPECT_EQ(evs[2].start_mark.line, 0);
    EXPECT_EQ(evs[2].start_mark.column, 4);
    EXPECT_EQ(evs[3].type, YEP_LY_DOCUMENT_END_EVENT);
    EXPECT_EQ(evs[3].implicit, 0); /* explicit ... */
    EXPECT_EQ(evs[4].type, YEP_LY_STREAM_END_EVENT);
    /* unmarked document: implicit */
    n = 0;
    EXPECT_EQ(run_first_types("x\n", evs, 8, &n), "ok");
    EXPECT_EQ(evs[1].type, YEP_LY_DOCUMENT_START_EVENT);
    EXPECT_EQ(evs[1].implicit, 1);
}

TEST(YamlCompat, FlowCollectionsAndTags) {
    /* views borrow the engine: capture them inside the sink (the
     * lifetime contract is the adapter's documented behavior) */
    std::string captured_tag;
    int saw_seq_start = 0, saw_flow = 0, saw_implicit = 0;
    struct Cap {
        std::string* tag;
        int* saw_seq_start;
        int* saw_flow;
        int* saw_implicit;
    } cap = {&captured_tag, &saw_seq_start, &saw_flow, &saw_implicit};
    auto sink_fn = [](void* ctx, const yep_event* ev) -> int {
        Cap* c = (Cap*)ctx;
        yep_ly_event ly;
        yep_ly_translate(&ly, ev);
        if (ly.type == YEP_LY_SEQUENCE_START_EVENT) {
            *c->saw_seq_start = 1;
            *c->saw_flow = (ly.style == YEP_LY_FLOW_STYLE);
            *c->saw_implicit = ly.implicit;
            if (ly.tag != NULL) {
                c->tag->assign(ly.tag, ly.tag_len);
            }
        }
        return 0;
    };
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    yep_sink sink = {sink_fn, &cap};
    const char* y = "!!seq [1, 2]\n";
    int rc = yep_engine_run(eng, y, strlen(y), &sink);
    yep_engine_destroy(eng);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(saw_seq_start, 1);
    EXPECT_EQ(saw_flow, 1);
    EXPECT_EQ(saw_implicit, 0); /* has a tag */
    EXPECT_EQ(captured_tag, "tag:yaml.org,2002:seq");
}
