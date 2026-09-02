/* test_stepping.cpp — resumable stepping (TODO.impl/07).
 *
 * The contract: for ANY input and ANY chunk split, the stepped event
 * stream is record-for-record identical to the whole-buffer one —
 * kinds, flags, line:col (stream-absolute), and value bytes. Cuts
 * happen only at --- boundaries that are safe: outside quoted
 * scalars, flow, comments, and never between a % directive and its
 * document. Ambiguity delays a cut (buffering), never mis-splits. */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <yeptris/events.h>

namespace {

struct Snap {
    uint8_t type, style, flags, tag_id;
    uint32_t line, col;
    std::string val, anchor, tag;
};

struct Stream {
    std::vector<Snap> recs;
    YeptrisStatus status = YEPTRIS_OK;
};

Snap snap_one(const YeptrisEventRecord* r, const char* arena) {
    Snap s;
    s.type = r->type;
    s.style = r->style;
    s.flags = r->flags;
    s.tag_id = r->tag_id;
    s.line = r->line;
    s.col = r->col;
    s.val.assign(arena + r->value_off, r->value_len);
    s.anchor.assign(arena + r->anchor_off, r->anchor_len);
    s.tag.assign(arena + r->tag_off, r->tag_len);
    return s;
}

Stream whole(const std::string& in) {
    Stream st;
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == nullptr) {
        st.status = YEPTRIS_ERROR_MEMORY;
        return st;
    }
    st.status = yeptris_recorder_feed(rec, in.data(), in.size(), 1);
    size_t n = 0, alen = 0;
    const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &n);
    const char* arena = yeptris_recorder_arena(rec, &alen);
    for (size_t i = 0; i < n; i++) {
        st.recs.push_back(snap_one(&rs[i], arena));
    }
    yeptris_recorder_free(rec);
    return st;
}

/* feed in fixed-size chunks; drain per feed (the pinned contract) */
Stream stepped_fixed(const std::string& in, size_t chunk) {
    Stream st;
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == nullptr) {
        st.status = YEPTRIS_ERROR_MEMORY;
        return st;
    }
    for (size_t i = 0; i < in.size(); i += chunk) {
        size_t take = in.size() - i < chunk ? in.size() - i : chunk;
        st.status = yeptris_recorder_feed(rec, in.data() + i, take, 0);
        size_t n = 0, alen = 0;
        const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &n);
        const char* arena = yeptris_recorder_arena(rec, &alen);
        for (size_t k = 0; k < n; k++) {
            st.recs.push_back(snap_one(&rs[k], arena));
        }
        if (st.status != YEPTRIS_OK) {
            yeptris_recorder_free(rec);
            return st;
        }
    }
    st.status = yeptris_recorder_feed(rec, "", 0, 1);
    size_t n = 0, alen = 0;
    const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &n);
    const char* arena = yeptris_recorder_arena(rec, &alen);
    for (size_t k = 0; k < n; k++) {
        st.recs.push_back(snap_one(&rs[k], arena));
    }
    yeptris_recorder_free(rec);
    return st;
}

void expect_identical(const std::string& in, size_t chunk) {
    Stream w = whole(in);
    Stream c = stepped_fixed(in, chunk);
    ASSERT_EQ(w.status, c.status);
    ASSERT_EQ(w.recs.size(), c.recs.size()) << "chunk=" << chunk;
    for (size_t i = 0; i < w.recs.size(); i++) {
        EXPECT_EQ(w.recs[i].type, c.recs[i].type) << "record " << i << " chunk=" << chunk;
        EXPECT_EQ(w.recs[i].style, c.recs[i].style) << "record " << i;
        EXPECT_EQ(w.recs[i].flags, c.recs[i].flags) << "record " << i;
        EXPECT_EQ(w.recs[i].tag_id, c.recs[i].tag_id) << "record " << i;
        EXPECT_EQ(w.recs[i].line, c.recs[i].line) << "record " << i;
        EXPECT_EQ(w.recs[i].col, c.recs[i].col) << "record " << i;
        EXPECT_EQ(w.recs[i].val, c.recs[i].val) << "record " << i;
        EXPECT_EQ(w.recs[i].anchor, c.recs[i].anchor) << "record " << i;
        EXPECT_EQ(w.recs[i].tag, c.recs[i].tag) << "record " << i;
    }
}

/* every split point and every small fixed chunk size */
void expect_identical_all_splits(const std::string& in) {
    for (size_t chunk = 1; chunk <= in.size() + 1; chunk++) {
        expect_identical(in, chunk);
    }
}

} // namespace

TEST(Stepping, MultiDocumentByteByByte) {
    expect_identical_all_splits("--- a\n--- b\n--- c\n");
}

TEST(Stepping, DocumentsClosedIncrementally) {
    /* by the time the chunk holding the second --- is fed, doc1's
     * records are already drained — the streaming property */
    const std::string in = "--- a: 1\n--- b: 2\n--- c: 3\n";
    YeptrisRecorder rec = yeptris_recorder_new();
    ASSERT_NE(rec, nullptr);
    /* doc1's closing --- has not arrived: nothing to drain */
    ASSERT_EQ(yeptris_recorder_feed(rec, in.data(), 9, 0), YEPTRIS_OK);
    size_t n = 0;
    (void)yeptris_recorder_records(rec, &n);
    EXPECT_EQ(n, 0u);
    /* the chunk holding doc2's --- closes doc1: its 7 events drain
     * (STREAM_START, DOC_START, MAP_START, a, 1, MAP_END, DOC_END) */
    ASSERT_EQ(yeptris_recorder_feed(rec, in.data() + 9, 9, 0), YEPTRIS_OK);
    n = 0;
    (void)yeptris_recorder_records(rec, &n);
    EXPECT_EQ(n, 7u);
    /* final flush: docs 2 and 3 plus STREAM_END (13 events) */
    ASSERT_EQ(yeptris_recorder_feed(rec, in.data() + 18, in.size() - 18, 1), YEPTRIS_OK);
    n = 0;
    (void)yeptris_recorder_records(rec, &n);
    EXPECT_EQ(n, 13u);
    yeptris_recorder_free(rec);
}

TEST(Stepping, QuotedScalarSpansMarkerLine) {
    /* the --- inside the single-quoted scalar is content, not a
     * boundary: no cut may land inside the quote */
    expect_identical_all_splits("msg: 'keep\n---\ninside'\nafter: 1\n--- doc2\n");
}

TEST(Stepping, DoubleQuotedEscapeSplitAcrossChunks) {
    expect_identical_all_splits("a: \"x\\\\\"\n--- b\n");
}

TEST(Stepping, PlainApostropheThenMultilineQuote) {
    /* don't must not open a quote; msg's quote must open and span
     * the marker line — the classic false-cut hazard */
    expect_identical_all_splits("note: don't do\nmsg: 'keep\n---\ninside'\n--- d2\n");
}

TEST(Stepping, DoubledQuoteEscape) {
    expect_identical_all_splits("a: 'it''s\n--- fine\nok'\n--- d2: 1\n");
}

TEST(Stepping, FlowSpansMarkerLine) {
    expect_identical_all_splits("a: [1,\n---\n, 2]\n--- b\n");
}

TEST(Stepping, CommentWithQuotesAndMarkers) {
    expect_identical_all_splits("# don't --- split here\n# 'quoted'\n--- a: 1\n--- b: 2\n");
}

TEST(Stepping, ColumnZeroComment) {
    expect_identical_all_splits("# first\n--- a\n# inner 'quote\n--- b\n");
}

TEST(Stepping, DirectiveStaysWithItsDocument) {
    expect_identical_all_splits("%YAML 1.2\n--- text\n--- more\n");
    expect_identical_all_splits("--- a\n%YAML 1.2\n--- b\n--- c\n");
}

TEST(Stepping, ExplicitEndNeverOrphaned) {
    /* cuts land at --- lines only: the ... end marker keeps its
     * explicit DOCUMENT_END flag */
    expect_identical_all_splits("--- a\n...\n--- b\n");
    expect_identical_all_splits("--- a\n...\n");
}

TEST(Stepping, BlockScalarIsIndented) {
    expect_identical_all_splits("a: |\n  text\n  --- not a marker\n--- b: 1\n");
}

TEST(Stepping, SequenceDashesAreNotMarkers) {
    expect_identical_all_splits("- a\n- b\n- ---\n--- d\n");
}

TEST(Stepping, LinesAreStreamAbsolute) {
    /* doc2's events must carry whole-buffer line numbers */
    Stream w = whole("a: 1\n# c\n\n--- b: 2\n");
    ASSERT_EQ(w.status, YEPTRIS_OK);
    ASSERT_EQ(w.recs.size(), 14u); /* two documents, one stream */
    EXPECT_EQ(w.recs[3].line, 1u); /* key a */
    EXPECT_EQ(w.recs[3].val, "a");
    EXPECT_EQ(w.recs[9].line, 4u); /* key b: whole-buffer line */
    EXPECT_EQ(w.recs[9].val, "b");
    Stream c = stepped_fixed("a: 1\n# c\n\n--- b: 2\n", 3);
    ASSERT_EQ(c.status, YEPTRIS_OK);
    ASSERT_EQ(c.recs.size(), w.recs.size());
    for (size_t i = 0; i < w.recs.size(); i++) {
        EXPECT_EQ(c.recs[i].line, w.recs[i].line) << "record " << i;
        EXPECT_EQ(c.recs[i].val, w.recs[i].val) << "record " << i;
    }
}

TEST(Stepping, TruncatedMarkerAtChunkEdge) {
    /* every prefix length of a marker line is fed as a non-final
     * chunk; none of them may cut or error */
    const std::string in = "a: 1\n--- b\n";
    for (size_t split = 4; split < in.size(); split++) {
        Stream st;
        YeptrisRecorder rec = yeptris_recorder_new();
        ASSERT_NE(rec, nullptr);
        EXPECT_EQ(yeptris_recorder_feed(rec, in.data(), split, 0), YEPTRIS_OK) << "split=" << split;
        EXPECT_EQ(yeptris_recorder_feed(rec, in.data() + split, in.size() - split, 1), YEPTRIS_OK)
            << "split=" << split;
        yeptris_recorder_free(rec);
    }
}

TEST(Stepping, ErrorIsTerminal) {
    const char* bad = "a: [1, 2\n"; /* unterminated flow */
    YeptrisRecorder rec = yeptris_recorder_new();
    ASSERT_NE(rec, nullptr);
    EXPECT_EQ(yeptris_recorder_feed(rec, bad, strlen(bad), 1), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(yeptris_recorder_feed(rec, "]", 1, 0), YEPTRIS_ERROR_ARG);
    yeptris_recorder_free(rec);
}

TEST(Stepping, ErrorInsideCompleteDocumentMidStream) {
    /* doc1 is invalid; the error surfaces on the pre-final feed that
     * completes it — matching the whole-buffer verdict */
    const char* in = "a: [1, 2\n--- b: 1\n";
    Stream w = whole(in);
    EXPECT_EQ(w.status, YEPTRIS_ERROR_PARSE);
    Stream c = stepped_fixed(in, 8);
    EXPECT_EQ(c.status, YEPTRIS_ERROR_PARSE);
}

TEST(Stepping, EmptyAndFinalOnlyFeeds) {
    expect_identical_all_splits("");
    expect_identical_all_splits("---\n");
    expect_identical_all_splits("--- null\n");
}
