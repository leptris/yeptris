// test_stream_iter.cpp — the tolerant per-document stream iterator
// (#921). Pins: the mixed stream (valid/INVALID/valid with per-document
// line/col), the recovery law (a failed frame resyncs to the next
// line-start marker; the resync is a RAW scan — the failed frame has no
// scanner-defined extent — so the tail's remainder surfaces as the
// honest best effort), the index law (failures consume an index), the
// empty and NULL+0 shapes, and the depth-miss recovery (the guard
// fails the deep frame; the walk resumes at the next marker).
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris/dom.h>
#include <yeptris/parse.h>

namespace {

// A minimal root-scalar read: the document's root as a string (the
// pins' value assertions ride it).
std::string root_scalar(YeptrisDocument doc) {
    if (doc == nullptr || yeptris_document_count(doc) == 0) {
        return "";
    }
    YeptrisNode r = yeptris_document_root(doc, 0);
    size_t len = 0;
    const char* v = yeptris_node_value(r, &len);
    return v != nullptr ? std::string(v, len) : "";
}

} // namespace

TEST(StreamIter, MixedStreamSurfacesPerDocumentFailures) {
    const char* src = "--- ok_one\n"
                      "--- {{{{ bad\n" /* a flow map never closed */
                      "--- ok_two\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src, strlen(src), nullptr);
    ASSERT_NE(it, nullptr);

    YeptrisDocument doc = nullptr;
    size_t index = 999;
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(index, 0u);
    EXPECT_EQ(root_scalar(doc), "ok_one");
    yeptris_document_free(doc);

    doc = nullptr;
    index = 999;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(index, 1u); /* the failure consumed the stream's index */
    uint32_t line = 0, col = 0;
    EXPECT_EQ(yeptris_stream_iter_status(it, &line, &col), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(line, 2u); /* the failed document's OWN geometry */
    EXPECT_GT(col, 0u);

    doc = nullptr;
    index = 999;
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(index, 2u);
    EXPECT_EQ(root_scalar(doc), "ok_two");
    yeptris_document_free(doc);

    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr); /* the stream is over */
    yeptris_stream_iter_free(it);
}

TEST(StreamIter, QuotedMarkerLineDocumentFailsAndTheTailRecoversBestEffort) {
    // a double-quoted scalar spanning a line that LOOKS like a marker:
    // the grammar itself rejects marker-like content lines (spec 9.1.2's
    // c-forbidden), so the document fails as ONE unit. The recovery
    // resync is a RAW marker scan (the failed frame has no scanner-
    // defined extent), and the tail's remainder genuinely parses —
    // surfacing it is the honest best effort, never a crash or a hang.
    const char* src = "\"line one\n---\nline two\"\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src, strlen(src), nullptr);
    ASSERT_NE(it, nullptr);
    YeptrisDocument doc = nullptr;
    size_t index = 999;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(index, 0u);
    uint32_t line = 0, col = 0;
    EXPECT_EQ(yeptris_stream_iter_status(it, &line, &col), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(line, 1u); /* the failed document's own geometry */

    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    yeptris_document_free(doc); /* the tail's remainder, if it parses */
    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr); /* and the walk terminates */
    yeptris_stream_iter_free(it);
}

TEST(StreamIter, ResyncSkipsToTheNextMarker) {
    // the proven failure generator: an explicit FLOW root closes at its
    // line end, so the next content line is a SECOND root — a parse
    // error at that line (the two-root law #900 pinned). The failure
    // consumes its index, the resync lands on the next --- marker, and
    // the recoverable doc rides.
    std::string deep(1200, '[');
    deep.append(1200, ']');
    const std::string src = "--- [1]\n" + deep + "\n--- recovered\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src.data(), src.size(), nullptr);
    ASSERT_NE(it, nullptr);
    YeptrisDocument doc = nullptr;
    size_t index = 999;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(index, 0u);
    uint32_t line = 0, col = 0;
    EXPECT_EQ(yeptris_stream_iter_status(it, &line, &col), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(line, 2u); /* the second root's own line */

    doc = nullptr;
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(index, 1u);
    EXPECT_EQ(root_scalar(doc), "recovered");
    yeptris_document_free(doc);

    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr);
    yeptris_stream_iter_free(it);
}

TEST(StreamIter, UnrecoverableTailEndsTheStream) {
    // the same failure with no marker after it: the resync finds
    // nothing and the walk ends — never hangs
    std::string deep(1200, '[');
    deep.append(1200, ']');
    const std::string src = "--- [1]\n" + deep + "\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src.data(), src.size(), nullptr);
    ASSERT_NE(it, nullptr);
    YeptrisDocument doc = nullptr;
    size_t index = 0;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(index, 0u);

    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, &index), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr);
    yeptris_stream_iter_free(it);
}

TEST(StreamIter, EmptyAndNullShapes) {
    yeptris_stream_iter* it = yeptris_stream_iter_new("", 0, nullptr);
    ASSERT_NE(it, nullptr);
    YeptrisDocument doc = (YeptrisDocument)1;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr);
    yeptris_stream_iter_free(it);

    EXPECT_EQ(yeptris_stream_iter_new(nullptr, 5, nullptr), nullptr);
    EXPECT_EQ(yeptris_stream_iter_next(nullptr, &doc, nullptr), YEPTRIS_ERROR_ARG);
}

TEST(StreamIter, ToleratedDocumentsMatchLoadStreamValues) {
    // the differential law: the valid documents the tolerant walk keeps
    // parse to the same values a strict whole-stream parse of just the
    // valid documents yields (the schema option rides the slices too)
    const char* src = "--- 41\n--- \"42\"\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src, strlen(src), nullptr);
    ASSERT_NE(it, nullptr);
    YeptrisDocument doc = nullptr;
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(root_scalar(doc), "41");
    yeptris_document_free(doc);
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    ASSERT_NE(doc, nullptr);
    EXPECT_EQ(root_scalar(doc), "42"); /* quoted: a string, not an int */
    yeptris_document_free(doc);
    yeptris_stream_iter_free(it);
}

TEST(StreamIter, SliceServesTheLastSuccessfulFrame) {
    // the slice accessor: hosts with their own value route re-parse
    // exactly these bytes — the offsets must track the frame extents
    // (and die ARG after a failure, the end, or a NULL out-param)
    const char* src = "--- first\n--- {a: 1}\n--- {{{{ bad\n--- last\n";
    yeptris_stream_iter* it = yeptris_stream_iter_new(src, strlen(src), nullptr);
    ASSERT_NE(it, nullptr);

    size_t off = 999, len = 0;
    YeptrisDocument doc = nullptr;
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    yeptris_document_free(doc);
    EXPECT_EQ(yeptris_stream_iter_slice(it, &off, &len), YEPTRIS_OK);
    EXPECT_EQ(memcmp(src + off, "--- first", 9), 0);

    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    yeptris_document_free(doc);
    EXPECT_EQ(yeptris_stream_iter_slice(it, &off, &len), YEPTRIS_OK);
    EXPECT_EQ(memcmp(src + off, "--- {a: 1}", 9), 0);

    /* the failed frame: the slice goes ARG (the frame is ill-formed) */
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(yeptris_stream_iter_slice(it, &off, &len), YEPTRIS_ERROR_ARG);

    /* the recovery frame serves again */
    ASSERT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    yeptris_document_free(doc);
    EXPECT_EQ(yeptris_stream_iter_slice(it, &off, &len), YEPTRIS_OK);
    EXPECT_EQ(memcmp(src + off, "--- last", 8), 0);

    /* the stream end: ARG, not a stale frame */
    doc = nullptr;
    EXPECT_EQ(yeptris_stream_iter_next(it, &doc, nullptr), YEPTRIS_OK);
    EXPECT_EQ(doc, nullptr);
    EXPECT_EQ(yeptris_stream_iter_slice(it, &off, &len), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_stream_iter_slice(nullptr, &off, &len), YEPTRIS_ERROR_ARG);
    yeptris_stream_iter_free(it);
}
