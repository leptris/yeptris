/* test_event_marks.cpp - #179: libyaml-parity event marks.
 *
 * The expectations are FROZEN from stdlib psych 5.5.0 (the marks
 * dumper in scripts/marks_oracle) - the tuples below are psych's
 * event_location pairs, 0-based, one per event in stream order. The
 * corpus spans every family: directives, ---/... markers, block and
 * flow collections, quoted/literal/folded/multiline scalars, anchors,
 * tags, aliases, same-indent sequences, chomping indicators, deferred
 * nulls, trailing spaces. */
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
    std::vector<Mark> marks; /* psych-frozen, stream order */
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
      M(11, 0, 11, 1), M(11, 3, 14, 0), M(14, 0, 14, 0), M(14, 0, 14, 0), M(14, 0, 14, 0)}},
    {"flow: {k1: v1, k2: [1, 2, {n: null}]}\nflowseq: [a, b, c]\n",
     {M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 4),   M(0, 6, 0, 7),
      M(0, 7, 0, 9),   M(0, 11, 0, 13), M(0, 15, 0, 17), M(0, 19, 0, 20), M(0, 20, 0, 21),
      M(0, 23, 0, 24), M(0, 26, 0, 27), M(0, 27, 0, 28), M(0, 30, 0, 34), M(0, 34, 0, 35),
      M(0, 35, 0, 36), M(0, 36, 0, 37), M(1, 0, 1, 7),   M(1, 9, 1, 10),  M(1, 10, 1, 11),
      M(1, 13, 1, 14), M(1, 16, 1, 17), M(1, 17, 1, 18), M(2, 0, 2, 0),   M(2, 0, 2, 0),
      M(2, 0, 2, 0)}},
    {"---\nfirst: doc\n---\nsecond: doc\n...\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 3), M(1, 0, 1, 0), M(1, 0, 1, 5), M(1, 7, 1, 10), M(2, 0, 2, 0),
      M(2, 0, 2, 0), M(2, 0, 2, 3), M(3, 0, 3, 0), M(3, 0, 3, 6), M(3, 8, 3, 11), M(4, 0, 4, 0),
      M(4, 0, 4, 3), M(5, 0, 5, 0)}},
    {"key: plain multi\n  continued\nend: yes\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 3), M(0, 5, 1, 11), M(2, 0, 2, 3),
      M(2, 5, 2, 8), M(3, 0, 3, 0), M(3, 0, 3, 0), M(3, 0, 3, 0)}},
    {"single: 'it''s here'\ndquote: \"line one\\n  line two\"\nempty:\nlater: x\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 6), M(0, 8, 0, 20), M(1, 0, 1, 6),
      M(1, 8, 1, 30), M(2, 0, 2, 5), M(2, 6, 2, 6), M(3, 0, 3, 5), M(3, 7, 3, 8), M(4, 0, 4, 0),
      M(4, 0, 4, 0), M(4, 0, 4, 0)}},
    {"keep: |+\n  a\n\n\nstrip: |-\n  b\ncustom: >2\n    indented\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 4), M(0, 6, 4, 0), M(4, 0, 4, 5),
      M(4, 7, 6, 0), M(6, 0, 6, 6), M(6, 8, 8, 0), M(8, 0, 8, 0), M(8, 0, 8, 0), M(8, 0, 8, 0)}},
    {"outer:\n  inner: [1, {deep: [two]}]\n  # comment inside\n  plain: value\nseq:\n- {a: 1}\n- "
     "[b, 2]\n",
     {M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 0),   M(0, 0, 0, 5),   M(1, 2, 1, 2),
      M(1, 2, 1, 7),   M(1, 9, 1, 10),  M(1, 10, 1, 11), M(1, 13, 1, 14), M(1, 14, 1, 18),
      M(1, 20, 1, 21), M(1, 21, 1, 24), M(1, 24, 1, 25), M(1, 25, 1, 26), M(1, 26, 1, 27),
      M(3, 2, 3, 7),   M(3, 9, 3, 14),  M(4, 0, 4, 0),   M(4, 0, 4, 3),   M(5, 0, 5, 1),
      M(5, 2, 5, 3),   M(5, 3, 5, 4),   M(5, 6, 5, 7),   M(5, 7, 5, 8),   M(6, 2, 6, 3),
      M(6, 3, 6, 4),   M(6, 6, 6, 7),   M(6, 7, 6, 8),   M(7, 0, 7, 0),   M(7, 0, 7, 0),
      M(7, 0, 7, 0),   M(7, 0, 7, 0)}},
    {"- a\n- b\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 2, 0, 3), M(1, 2, 1, 3), M(2, 0, 2, 0),
      M(2, 0, 2, 0), M(2, 0, 2, 0)}},
    {"k:\n  - a\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 1), M(1, 2, 1, 2), M(1, 4, 1, 5),
      M(2, 0, 2, 0), M(2, 0, 2, 0), M(2, 0, 2, 0), M(2, 0, 2, 0)}},
    {"k:\n- a\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 1), M(1, 0, 1, 1), M(1, 2, 1, 3),
      M(2, 0, 2, 0), M(2, 0, 2, 0), M(2, 0, 2, 0), M(2, 0, 2, 0)}},
    {"k:\n  - a\n  - b\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 1), M(1, 2, 1, 2), M(1, 4, 1, 5),
      M(2, 4, 2, 5), M(3, 0, 3, 0), M(3, 0, 3, 0), M(3, 0, 3, 0), M(3, 0, 3, 0)}},
    {"- - a\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 2, 0, 2), M(0, 4, 0, 5), M(1, 0, 1, 0),
      M(1, 0, 1, 0), M(1, 0, 1, 0), M(1, 0, 1, 0)}},
    {"k:   \nj: 1\n",
     {M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 0), M(0, 0, 0, 1), M(0, 2, 0, 2), M(1, 0, 1, 1),
      M(1, 3, 1, 4), M(2, 0, 2, 0), M(2, 0, 2, 0), M(2, 0, 2, 0)}},
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
            EXPECT_EQ(rs[i].end_line - 1, m.el) << "event " << i;
            EXPECT_EQ(rs[i].end_col - 1, m.ec) << "event " << i;
        }
        yeptris_recorder_free(rec);
    }
}
