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

TEST(PlanWalk, MapRootScalarMapLeafIsOneRow) {
    /* the single-record shape (yeptris-ruby#430): the path leaf is a
     * map of scalars — the container itself is ONE row */
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":["config","database"],"children":[
        {"name":"host","kind":"str"},{"name":"port","kind":"int"},
        {"name":"name","kind":"str"},{"name":"user","kind":"str"},
        {"name":"password","kind":"str"}]})");
    TapeGuard tape;
    tape.parse(R"({"config":{"database":{"host":"localhost","port":5432,
                    "name":"myapp","user":"admin","password":"secret"}}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1u);
    const uint32_t* offs = yeptris_plan_result_str_offs(r, 0);
    const uint32_t* lens = yeptris_plan_result_str_lens(r, 0);
    ASSERT_NE(offs, nullptr);
    EXPECT_EQ(std::string((const char*)tape.t._src + offs[0], lens[0]), "localhost");
    EXPECT_EQ(yeptris_plan_result_ints(r, 1)[0], 5432);
    EXPECT_EQ(std::string((const char*)tape.t._src + yeptris_plan_result_str_offs(r, 2)[0],
                          yeptris_plan_result_str_lens(r, 2)[0]),
              "myapp");
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[0], 0);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 0);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 4)[0], 0);
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, ScalarMapLeafMissingLeavesStayNull) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"db","children":[
        {"name":"host","kind":"str"},{"name":"port","kind":"int"},
        {"name":"gone","kind":"str"}]})");
    TapeGuard tape;
    tape.parse(R"({"db":{"host":"h","port":null,"other":7}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1u);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[0], 0);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 1); /* explicit null */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 2)[0], 1); /* missing leaf */
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, ScalarMapLeafNestedValueSkipsToItsClose) {
    /* a planned leaf holding a nested object stays null AND its
     * contents never fill later columns (the OPEN-skip used to walk
     * nested keys as row keys and derailed the record fill) */
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"db","children":[
        {"name":"host","kind":"str"},{"name":"tls","kind":"int"},
        {"name":"port","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"({"db":{"host":"h","tls":{"deep":9},"port":5432}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1u);
    EXPECT_EQ(yeptris_plan_result_ints(r, 2)[0], 5432); /* the sibling after the nested map */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 1);   /* the nested object: null */
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, RowLeafNestedValueDoesNotLeakNestedKeys) {
    /* the same CLOSE-skip on the rows-of-records form: a matched key
     * whose value is a nested map must not walk the nested keys as
     * row keys (the OPEN-skip used to leak the nested value into the
     * column AND derail the depth for every row after it) */
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[
        {"name":"id","kind":"int"},{"name":"deep","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"([{"id":1,"deep":{"deep":9}},{"id":2,"deep":7}])");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2u);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 1);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 1); /* the nested "deep" does not leak */
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 2);  /* the sibling row still fills */
    EXPECT_EQ(yeptris_plan_result_ints(r, 1)[1], 7);
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, ScalarMapLeafEmptyContainerIsZeroRows) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"db","children":[
        {"name":"host","kind":"str"}]})");
    TapeGuard tape;
    tape.parse(R"({"db":{}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 0u); /* empty: no record, no row */
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, MapRootMappingValuesWinOverTheRecordForm) {
    /* a map-of-records keeps its shape even beside scalar siblings */
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"items","children":[
        {"name":"id","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"({"items":{"a":{"id":1},"b":2,"c":{"id":3}}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2u); /* the mapping values only */
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 1);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 3);
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
    yeptris_tape_columns(&t); /* item 07: the lenient columns are lazy */

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 41);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 42);
    yeptris_plan_result_free(r);
    yeptris_tape_free(&t);
}

TEST(PlanWalk, SegmentedPathWalksNestedMaps) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","path":["data","items"],"children":[
        {"name":"id","kind":"int"},
        {"name":"score","kind":"float"}]})");
    TapeGuard tape;
    tape.parse(R"({"meta":{"v":2},"data":{"items":[
                    {"id":1,"score":0.5},{"id":2,"score":1.5}],
                    "other":[{"id":9}]}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 1);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 2);
    EXPECT_DOUBLE_EQ(yeptris_plan_result_floats(r, 1)[0], 0.5);
    EXPECT_DOUBLE_EQ(yeptris_plan_result_floats(r, 1)[1], 1.5);
    yeptris_plan_result_free(r);
}

TEST(PlanWalk, SegmentedPathMidSegmentScalarIsADisagreement) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","path":["data","items"],"children":[
        {"name":"id","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"({"data":{"items":42}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
}

TEST(PlanWalk, SegmentedPathMissingSegmentIsADisagreement) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","path":["nope","items"],"children":[
        {"name":"id","kind":"int"}]})");
    TapeGuard tape;
    tape.parse(R"({"data":{"items":[{"id":1}]}})");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
}

TEST(PlanWalk, PathSegmentMustBeAString) {
    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan* p = yeptris_plan_compile(
        R"({"kind":"seq","path":[1,2],"children":[{"name":"a","kind":"int"}]})",
        strlen(R"({"kind":"seq","path":[1,2],"children":[{"name":"a","kind":"int"}]})"), &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_ARG);
    EXPECT_EQ(p, nullptr);
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

// ---- the DOM leg (the YAML form): the same plan over yeptris_parse ----

namespace {

struct DocGuard {
    YeptrisDocument d{};

    ~DocGuard() {
        yeptris_document_free(d);
    }

    void parse(const char* doc) {
        YeptrisStatus st = YEPTRIS_OK;
        d = yeptris_parse(doc, strlen(doc), &st);
        ASSERT_EQ(st, YEPTRIS_OK) << doc;
        ASSERT_NE(d, nullptr) << doc;
    }
};

} // namespace

TEST(PlanDomWalk, BlockYamlSeqRootTypedColumns) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[
        {"name":"id","kind":"int"},
        {"name":"name","kind":"str"},
        {"name":"ok","kind":"bool"},
        {"name":"score","kind":"float"}]})");
    DocGuard doc;
    doc.parse("- id: 1\n"
              "  name: one\n"
              "  ok: true\n"
              "  score: 1.5\n"
              "- id: 2\n"
              "  name: two\n"
              "  ok: false\n"
              "  score: 2\n"
              "- id: 3\n"
              "  name:\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 3);
    const int64_t* ids = yeptris_plan_result_ints(r, 0);
    EXPECT_EQ(ids[0], 1);
    EXPECT_EQ(ids[1], 2);
    EXPECT_EQ(ids[2], 3);
    const yeptris_plan_str* names = yeptris_plan_result_strs(r, 1);
    ASSERT_NE(names, nullptr);
    EXPECT_EQ(std::string(names[0].p, names[0].len), "one");
    EXPECT_EQ(std::string(names[1].p, names[1].len), "two");
    const uint8_t* name_nulls = yeptris_plan_result_nulls(r, 1);
    EXPECT_EQ(name_nulls[2], 1); /* the missing name stays null */
    const int64_t* oks = yeptris_plan_result_ints(r, 2);
    EXPECT_EQ(oks[0], 1);
    EXPECT_EQ(oks[1], 0);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 2)[2], 1);
    const double* scores = yeptris_plan_result_floats(r, 3);
    EXPECT_DOUBLE_EQ(scores[0], 1.5);
    EXPECT_DOUBLE_EQ(scores[1], 2.0); /* int text promotes into float */
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, SegmentedPathOnBlockYaml) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","path":["data","items"],"children":[
        {"name":"id","kind":"int"}]})");
    DocGuard doc;
    doc.parse("data:\n"
              "  items:\n"
              "    - id: 1\n"
              "    - id: 2\n"
              "  other:\n"
              "    - id: 9\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 1);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 2);
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, MapRootYieldsMappingRows) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"items","children":[
        {"name":"id","kind":"int"}]})");
    DocGuard doc;
    doc.parse("items:\n"
              "  first:\n"
              "    id: 1\n"
              "  second:\n"
              "    id: 2\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 2);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 1);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 2);
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, MapRootScalarMapLeafIsOneRow) {
    /* the single-record shape (yeptris-ruby#430) on the YAML leg */
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":["config","database"],"children":[
        {"name":"host","kind":"str"},{"name":"port","kind":"int"},
        {"name":"name","kind":"str"}]})");
    DocGuard doc;
    doc.parse("config:\n"
              "  database:\n"
              "    host: localhost\n"
              "    port: 5432\n"
              "    name: myapp\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1);
    const yeptris_plan_str* hosts = yeptris_plan_result_strs(r, 0);
    ASSERT_NE(hosts, nullptr);
    EXPECT_EQ(std::string(hosts[0].p, hosts[0].len), "localhost");
    EXPECT_EQ(yeptris_plan_result_ints(r, 1)[0], 5432);
    EXPECT_EQ(
        std::string(yeptris_plan_result_strs(r, 2)[0].p, yeptris_plan_result_strs(r, 2)[0].len),
        "myapp");
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[0], 0);
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, ScalarMapLeafNestedValueSkipsAndKeepsWalking) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"db","children":[
        {"name":"host","kind":"str"},{"name":"tls","kind":"str"},
        {"name":"port","kind":"int"}]})");
    DocGuard doc;
    doc.parse("db:\n"
              "  host: h\n"
              "  tls:\n"
              "    enabled: true\n"
              "  port: 5432\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1);
    EXPECT_EQ(yeptris_plan_result_ints(r, 2)[0], 5432); /* the sibling after the nested map */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 1);   /* the nested mapping: null */
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, ScalarMapLeafEmptyContainerIsZeroRows) {
    PlanGuard plan;
    plan.compile(R"({"kind":"map","path":"db","children":[
        {"name":"host","kind":"str"}]})");
    DocGuard doc;
    doc.parse("db: {}\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 0);
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, AliasesResolveAndNullsStayNull) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[
        {"name":"id","kind":"int"},
        {"name":"name","kind":"str"}]})");
    DocGuard doc;
    doc.parse("- &d\n"
              "  id: 99\n"
              "  name: base\n"
              "- *d\n"
              "- id: 2\n"
              "  name: ~\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 3);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[0], 99);
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[1], 99); /* through the alias */
    EXPECT_EQ(yeptris_plan_result_ints(r, 0)[2], 2);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 0); /* "base" */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[1], 0); /* the alias target's "base" */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[2], 1); /* ~ stays null */
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, TypeDisagreementLeavesNull) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[
        {"name":"id","kind":"int"},
        {"name":"name","kind":"str"}]})");
    DocGuard doc;
    doc.parse("- id: not-a-number\n"
              "  name: 123\n"); /* str value in an int column, int text in str */

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_rows(r), 1);
    EXPECT_EQ(yeptris_plan_result_nulls(r, 0)[0], 1); /* int col, str text */
    EXPECT_EQ(yeptris_plan_result_nulls(r, 1)[0], 0); /* str col takes any scalar */
    const yeptris_plan_str* names = yeptris_plan_result_strs(r, 1);
    EXPECT_EQ(std::string(names[0].p, names[0].len), "123");
    yeptris_plan_result_free(r);
}

TEST(PlanDomWalk, ShapeDisagreementIsAParseError) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[{"name":"id","kind":"int"}]})");
    DocGuard doc;
    doc.parse("a: 1\n");

    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_document_plan_walk(doc.d, plan.p, &st);
    EXPECT_EQ(st, YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
}

TEST(PlanDomWalk, TapeLegStrsAccessorIsNull) {
    PlanGuard plan;
    plan.compile(R"({"kind":"seq","children":[{"name":"name","kind":"str"}]})");
    TapeGuard tape;
    tape.parse(R"([{"name":"a"}])");
    YeptrisStatus st = YEPTRIS_OK;
    yeptris_plan_result* r = yeptris_tape_plan_walk(&tape.t, plan.p, &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_plan_result_strs(r, 0), nullptr); /* the tape leg rides offs/lens */
    yeptris_plan_result_free(r);
}
