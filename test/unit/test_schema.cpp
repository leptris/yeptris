// test_schema.cpp — the fused schema-descriptor materialization
// (issue #238 / TODO.restructure/83). Pins: typed columns against a
// known document, sequence element plans, nested mappings, the
// two-names-one-slot merge (shared child slices), the CALLBACK escape
// hatch, REQUIRED enforcement, ABI gating, unknown keys ignored, and
// capacity overflow.
#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include <yeptris/schema.h>

namespace {

struct Fixture {
    std::vector<yeptris_desc_node> desc;
    std::vector<yeptris_schema_column> cols;
    std::vector<std::vector<char>> buffers;

    explicit Fixture(size_t n) : desc(n), cols(n), buffers(n) {}

    void capacity(size_t node, size_t elems, size_t elem_size) {
        buffers[node].assign(elems * elem_size, 0);
        cols[node].data = buffers[node].data();
        cols[node].capacity = (uint32_t)elems;
        cols[node].count = 0;
    }

    YeptrisStatus load(const char* src, size_t len, YeptrisSchema schema = YEPTRIS_SCHEMA_12_CORE) {
        return yeptris_schema_load(src, len, schema, desc.data(), (uint32_t)desc.size(),
                                   YEPTRIS_DESC_ABI, cols.data());
    }
};

TEST(SchemaLoad, TypedColumnsMatchTheDocument) {
    // 0 root -> children {1 one, 2 two}; both share child slice [3..7)
    // — the two-names-one-slot merge the ABI exists for
    Fixture f(7);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 2, 0};
    f.desc[1] = {"one", YEP_SK_MAPPING, 0, 0, 3, 4, 0};
    f.desc[2] = {"two", YEP_SK_MAPPING, 0, 0, 3, 4, 0};
    f.desc[3] = {"id", YEP_SK_SCALAR, YEP_ST_INT, 0, 0, 0, 0};
    f.desc[4] = {"name", YEP_SK_SCALAR, YEP_ST_STR, 0, 0, 0, 0};
    f.desc[5] = {"active", YEP_SK_SCALAR, YEP_ST_BOOL, 0, 0, 0, 0};
    f.desc[6] = {"score", YEP_SK_SCALAR, YEP_ST_FLOAT, 0, 0, 0, 0};
    f.capacity(3, 4, 8);
    f.capacity(4, 4, 16);
    f.capacity(5, 4, 1);
    f.capacity(6, 4, 8);

    const char* src = "one:\n  id: 7\n  name: alpha\n  active: true\n  score: 2.5\n"
                      "two:\n  id: 9\n  name: beta\n  active: false\n  score: 4.25\n";
    ASSERT_EQ(f.load(src, strlen(src)), YEPTRIS_OK);

    EXPECT_EQ(f.cols[3].count, 2u);
    EXPECT_EQ(((int64_t*)f.cols[3].data)[0], 7);
    EXPECT_EQ(((int64_t*)f.cols[3].data)[1], 9);
    EXPECT_EQ(f.cols[4].count, 2u);
    auto* names = (yeptris_span2*)f.cols[4].data;
    EXPECT_EQ(names[0].len, 5u);
    EXPECT_EQ(memcmp(src + names[0].off, "alpha", 5), 0);
    EXPECT_EQ(names[1].len, 4u);
    EXPECT_EQ(memcmp(src + names[1].off, "beta", 4), 0);
    EXPECT_EQ(f.cols[5].count, 2u);
    EXPECT_EQ(((uint8_t*)f.cols[5].data)[0], 1);
    EXPECT_EQ(((uint8_t*)f.cols[5].data)[1], 0);
    EXPECT_EQ(f.cols[6].count, 2u);
    EXPECT_DOUBLE_EQ(((double*)f.cols[6].data)[0], 2.5);
    EXPECT_DOUBLE_EQ(((double*)f.cols[6].data)[1], 4.25);
}

TEST(SchemaLoad, SequenceElementPlanAndNesting) {
    // 0 root -> {1 items SEQ of element 3, 2 deep MAP -> {4 inner}}
    Fixture f(5);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 2, 0};
    f.desc[1] = {"items", YEP_SK_SEQUENCE, 0, 0, 3, 0, 0};
    f.desc[2] = {"deep", YEP_SK_MAPPING, 0, 0, 4, 1, 0};
    f.desc[3] = {NULL, YEP_SK_SCALAR, YEP_ST_INT, 0, 0, 0, 0};
    f.desc[4] = {"inner", YEP_SK_SCALAR, YEP_ST_STR, 0, 0, 0, 0};
    f.capacity(3, 8, 8);
    f.capacity(4, 4, 16);

    const char* src = "items:\n  - 1\n  - 2\n  - 3\ndeep:\n  inner: gamma\n";
    ASSERT_EQ(f.load(src, strlen(src)), YEPTRIS_OK);
    EXPECT_EQ(f.cols[3].count, 3u);
    EXPECT_EQ(((int64_t*)f.cols[3].data)[0], 1);
    EXPECT_EQ(((int64_t*)f.cols[3].data)[2], 3);
    EXPECT_EQ(f.cols[4].count, 1u);
    EXPECT_EQ(memcmp(src + ((yeptris_span2*)f.cols[4].data)[0].off, "gamma", 5), 0);
}

TEST(SchemaLoad, CallbackEscapeHatchCarriesSpanAndPosition) {
    Fixture f(2);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 1, 0};
    f.desc[1] = {"custom", YEP_SK_CALLBACK, 0, 0, 0, 0, 0};
    f.capacity(1, 4, 16);

    const char* src = "custom: 12:34:56\n";
    ASSERT_EQ(f.load(src, strlen(src)), YEPTRIS_OK);
    EXPECT_EQ(f.cols[1].count, 1u);
    yeptris_span2 s = ((yeptris_span2*)f.cols[1].data)[0];
    EXPECT_EQ(s.off, strlen("custom: "));
    EXPECT_EQ(s.node_off, s.off);
    EXPECT_EQ(memcmp(src + s.off, "12:34:56", 8), 0);
}

TEST(SchemaLoad, RequiredMissingIsASchemaError) {
    Fixture f(2);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 1, 0};
    f.desc[1] = {"must", YEP_SK_SCALAR, YEP_ST_INT, YEP_SF_REQUIRED, 0, 0, 0};
    f.capacity(1, 4, 8);

    uint32_t line = 0, col = 0;
    EXPECT_EQ(f.load("other: 1\n", strlen("other: 1\n")), YEPTRIS_ERROR_SCHEMA);
    EXPECT_TRUE(strstr(yeptris_last_error(&line, &col), "node 1") != nullptr);

    EXPECT_EQ(f.load("must: 1\n", strlen("must: 1\n")), YEPTRIS_OK);
    EXPECT_EQ(f.cols[1].count, 1u);
}

TEST(SchemaLoad, AbiGuardAndUnknownKeys) {
    Fixture f(2);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 1, 0};
    f.desc[1] = {"known", YEP_SK_SCALAR, YEP_ST_INT, 0, 0, 0, 0};
    f.capacity(1, 4, 8);

    const char* src = "known: 5\nunknown: ignored\n";
    EXPECT_EQ(f.load(src, strlen(src)), YEPTRIS_OK); // unknown keys skip
    EXPECT_EQ(f.cols[1].count, 1u);
    EXPECT_EQ(((int64_t*)f.cols[1].data)[0], 5);

    EXPECT_EQ(yeptris_schema_load(src, strlen(src), YEPTRIS_SCHEMA_12_CORE, f.desc.data(),
                                  (uint32_t)f.desc.size(), 99, f.cols.data()),
              YEPTRIS_ERROR_ARG);
}

TEST(SchemaLoad, CapacityOverflowReportsMemory) {
    Fixture f(4);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 2, 0};
    f.desc[1] = {"a", YEP_SK_MAPPING, 0, 0, 3, 1, 0};
    f.desc[2] = {"b", YEP_SK_MAPPING, 0, 0, 3, 1, 0};
    f.desc[3] = {"v", YEP_SK_SCALAR, YEP_ST_INT, 0, 0, 0, 0};
    f.capacity(3, 1, 8);
    EXPECT_EQ(f.load("a:\n  v: 1\nb:\n  v: 2\n", strlen("a:\n  v: 1\nb:\n  v: 2\n")),
              YEPTRIS_ERROR_MEMORY);
}

TEST(SchemaLoad, StrictJsonInputTakesTheSameDoor) {
    Fixture f(2);
    f.desc[0] = {NULL, YEP_SK_MAPPING, 0, 0, 1, 1, 0};
    f.desc[1] = {"a", YEP_SK_SCALAR, YEP_ST_INT, 0, 0, 0, 0};
    f.capacity(1, 4, 8);
    EXPECT_EQ(f.load("{\"a\": 42}", strlen("{\"a\": 42}")), YEPTRIS_OK);
    EXPECT_EQ(((int64_t*)f.cols[1].data)[0], 42);
}

} // namespace
