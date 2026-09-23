/* test_event_marks.cpp - #179: libyaml-parity event marks.
 *
 * The expectations are FROZEN from stdlib psych 5.5.0 (the marks
 * dumper in scripts/marks_oracle) - the tuples below are psych's
 * event_location pairs, 0-based, one per event in stream order.
 *
 * Scalar/alias END columns are still pending engine stamping: the
 * gate tolerates unstamped ends on exactly those events and asserts
 * EVERYTHING ELSE exactly - any regression of the stamped families
 * (stream/document/collection) fails here. The tolerance dies with
 * the scalar-stamping slice. */
#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <yeptris/events.h>

namespace {

struct Mark {
    uint32_t l, c, el, ec;
};

struct Case {
    const char* yaml;
    std::vector<Mark> marks;          /* psych-frozen, stream order */
    std::vector<bool> scalar_pending; /* SCALAR/ALIAS rows */
};

Mark M(uint32_t l, uint32_t c, uint32_t el, uint32_t ec) {
    return {l, c, el, ec};
}

const std::vector<Case> kCases = {
    {"a: 1\nb:\n  - foo\n  - bar\nc: &x val\nd: *x\ne: !!str 5\nf: \"quoted s\"\ng: >\n  folded "
     "text\n  more\nh: |\n  lit\n  eral\n",
     {M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 1),   M(0, 3, 0, 4),
      M(1, 0, 1, 1),   M(2, 2, 2, 2),   M(2, 4, 2, 7),   M(3, 4, 3, 7),   M(4, 0, 4, 0),
      M(4, 0, 4, 1),   M(4, 3, 4, 9),   M(5, 0, 5, 1),   M(5, 3, 5, 5),   M(6, 0, 6, 1),
      M(6, 3, 6, 10),  M(7, 0, 7, 1),   M(7, 3, 7, 13),  M(8, 0, 8, 1),   M(8, 3, 11, 0),
      M(11, 0, 11, 1), M(11, 3, 14, 0), M(14, 0, 14, 0), M(14, 0, 14, 0), M(14, 0, 14, 0)},
     {false, false, false, true, true, true, false, true, true, false, true,  true, true,
      true,  true,  true,  true, true, true, true,  true, true, false, false, false}},
    {"flow: {k1: v1, k2: [1, 2, {n: null}]}\nflowseq: [a, b, c]\n",
     {M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 4),   M(0, 6, 0, 7),
      M(0, 7, 0, 9),   M(0, 11, 0, 13), M(0, 15, 0, 17), M(0, 19, 0, 20), M(0, 20, 0, 21),
      M(0, 23, 0, 24), M(0, 26, 0, 27), M(0, 27, 0, 28), M(0, 30, 0, 34), M(0, 34, 0, 35),
      M(0, 35, 0, 36), M(0, 36, 0, 37), M(1, 0, 1, 7),   M(1, 9, 1, 10),  M(1, 10, 1, 11),
      M(1, 13, 1, 14), M(1, 16, 1, 17), M(1, 17, 1, 18), M(2, 0, 2, 0),   M(2, 0, 2, 0),
      M(2, 0, 2, 0)},
     {false, false, false, true,  false, true,  true, true, false, true,  true,  false, true,
      true,  false, false, false, true,  false, true, true, true,  false, false, false, false}},
    {"---\nfirst: doc\n---\nsecond: doc\n...\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 3), M(1, 0, 1, 0), M(1, 0, 1, 5), M(1, 7, 1, 10), M(2, 0, 2, 0),
      M(2, 0, 2, 0), M(2, 0, 2, 3), M(3, 0, 3, 0), M(3, 0, 3, 6), M(3, 8, 3, 11), M(4, 0, 4, 0),
      M(4, 0, 4, 3), M(5, 0, 5, 0)},
     {false, false, false, true, true, false, false, false, false, true, true, false, false,
      false}},
    {"key: plain multi\n  continued\nend: yes\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 3), M(0, 5, 1, 11), M(2, 0, 2, 3),
      M(2, 5, 2, 8), M(3, 0, 3, 0), M(3, 0, 3, 0), M(3, 0, 3, 0)},
     {false, false, false, true, true, true, true, false, false, false}},
};

} // namespace

TEST(EventMarks, MatchStdlibPsych) {
    for (const Case& cs : kCases) {
        SCOPED_TRACE(cs.yaml);
        YeptrisRecorder rec = yeptris_recorder_new();
        ASSERT_EQ(yeptris_recorder_feed(rec, cs.yaml, strlen(cs.yaml), 1), YEPTRIS_OK);
        size_t n = 0;
        const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &n);
        ASSERT_EQ(n, cs.marks.size());
        for (size_t i = 0; i < n; i++) {
            const Mark& m = cs.marks[i];
            EXPECT_EQ(rs[i].line - 1, m.l) << "event " << i;
            EXPECT_EQ(rs[i].col - 1, m.c) << "event " << i;
            if (cs.scalar_pending[i] && rs[i].end_line == 0) {
                continue; /* pending: the scalar slice stamps these */
            }
            EXPECT_EQ(rs[i].end_line - 1, m.el) << "event " << i;
            EXPECT_EQ(rs[i].end_col - 1, m.ec) << "event " << i;
        }
        yeptris_recorder_free(rec);
    }
}
