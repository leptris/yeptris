// test_plan.cpp — the compiled plan walk (TODO.restructure/87 slice
// one, #293): spec compilation, the columnar walk over a strict tape,
// null/missing/mismatch handling, and the lenient tape's NUM records.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris.h>
#include <yeptris/json.h>
#include <yeptris/plan.h>
#include <yeptris/tape.h>

namespace {

struct PlanGuard {
    yeptris_plan* p{};

    ~PlanGuard() {
        yeptris_plan_free(p);
    }

    void compile(const char* spec) {
        YeptrisStatus st = YEPTRIS_OK;
        p = yeptris_plan_compile(spec, strlen(spec), &st);
        ASSERT_EQ(st, YEPTRIS_OK) << spec;
        ASSERT_NE(p, nullptr) << spec;
    }
};

struct TapeGuard {
    yeptris_json_tape t{};

    ~TapeGuard() {
        yeptris_tape_free(&t);
    }

    void parse(const char* doc) {
        ASSERT_EQ(yeptris_parse_json_tape(doc, strlen(doc), &t), YEPTRIS_OK) << doc;
    }
};

} // namespace

TEST(PlanWalk, SeqRootTypedColumns) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[
        {"name":"id","kind":"int"},
        {"name":"name","kind":"str"},
        {"name":"ok","kind":"bool"},
        {"name":"score","kind":"float"}]})");
    TapeGuard tape;
    tape.parse(R"([{"id":1,"name":"a","ok":true,"score":1.5},
                    {"id":2,"name":"b","ok":false,"score":2},
                    {"id":3,"name":"c"}])");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 3u);

    const int64_t* ids = yeptris_plan_result_ints(r, 0);
    ASSERT_NE(ids, nullptr);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
    EXPECT_EQ(ids[2], 3);

    const uint32_t* offs = yeptris_plan_result_str_offs(r, 1);
    const uint32_t* lens = yeptris_plan_result_str_lens(r, 1);
    const uint8_t* nulls = yeptris_plan_result_nulls(r, 1);
    ASSERT_NE(offs, nullptr);
    EXPECT_EQ(std::string((const char*)tape.t._src + offs[0], lens[0]), "a");
    EXPECT_EQ(std::string((const char*)tape.t._src + offs[2], lens[2]), "c");
    EXPECT_EQ(nulls[0], 0);

    const int64_t* oks = yeptris_plan_result_ints(r, 2);
    EXPECT_EQ(oks[0], 1);
    EXPECT_EQ(oks[1], 0);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 2)[2], 1); // missing leaf

    const double* scores = yeptris_plan_result_floats(r, 3);
    EXPECT_DOUBLE_EQ(scores[0], 1.5);
    EXPECT_DOUBLE_EQ(scores[1], 2.0); // int text into a float column
    EXPECT_EQ(yeptris_plan_result_nulls(r, 3)[2], 1);
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, MapRootPathSelectsRows) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"items","children":[
        {"name":"id","kind":"int"},{"name":"tag","kind":"str"}]})");
    TapeGuard tape;
    tape.parse(R"({"meta":{"skip":1},"items":[{"id":7,"tag":"x"}],"other":9})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1u);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 7);
    const uint32_t* lens = yeptris_plan_result_str_lens(r, 1);
    const uint32_t* offs = yeptris_plan_result_str_offs(r, 1);
    EXPECT_EQ(std::string((const char*)tape.t._src + offs[0], lens[0]), "x");
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, NullsAndUnknownLeaves) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[{"name":"v","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"([{"v":null,"extra":[1,2]},{"v":5}])");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2u);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[0], 1); // explicit null
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[1], 0);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 5);
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, ShapeDisagreementIsAParseError) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[{"name":"id","kind":"int"}]})");
    TapeGuard tape; // scalar root: no rows container
    tape.parse("42");

    YeptrisStatus st = YEPTRIS_OK;
    EXPECT_EQ(yeptris_tape_plan_walk(&tape.t, plan.p, &st), nullptr);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
}

TEST(PlanWalk, LenientTapeNumRecordsConvert) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[{"name":"n","kind":"int"}]})");
    yeptris_json_tape t;
    const char* doc = "[{\"n\":41},{\"n\":42}]";
    ASSERT_EQ(yeptris_parse_json_tape_lenient(doc, strlen(doc), &t), YEPTRIS_OK);

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 41);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 42);
    yeptris_plan_result_free(r);
    yeptris_tape_free(&t);
}

TEST(PlanWalk, MalformedSpecsAreRejected) {
    const char* bad[] = {
        "{}", // no kind/children
        R"({"kind":"zzz","children":[]})",
        R"({"kind":"seq","children":[{"name":"x"}]})", // no leaf kind
        R"({"kind":"seq","children":[{"name":"x","kind":"bogus"}]})",
        R"({"kind":"seq","children":[],"extra":1})", // unknown key
        "[1,2]",                                     // not a mapping
    };
    for (const char* s : bad) {
        YeptrisStatus st = YEPTRIS_OK;
        EXPECT_EQ(yeptris_plan_compile(s, strlen(s), &st), nullptr) << s;
        EXPECT_NE(st, YEPTRIS_OK) << s;
    }
}
