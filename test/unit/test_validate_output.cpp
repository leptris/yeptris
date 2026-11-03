// test_validate_output.cpp — the structured output face (#906 slice 5).
//
// Pins: the report -> JSON transform's exact bytes — the published
// BASIC output shape (json-schema.org core spec section 12; the
// meta-schema at https://json-schema.org/draft/2020-12/output/schema,
// vendored verbatim as output/schema.json) — against a fixture derived
// from the spec's section 12.4.4 verbose example (the same
// schema/instance; our flat report carries the object's path and the
// property name in the error text, per the walk's additionalProperties
// law), the measure/write buffer contract (emit-options convention),
// JSON string escaping, determinism, NULL handling, and a self-check:
// the transform's output validates against the PUBLISHED meta-schema
// through our own validator.
#include <gtest/gtest.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <yeptris/validate.h>

#ifndef TEST_JSONSCHEMA_OUTPUT_DIR
#define TEST_JSONSCHEMA_OUTPUT_DIR "."
#endif

namespace {

// The spec 12.4.4 example's schema and instance (verbatim objects).
const char* const kSpecSchema = "{\"$id\":\"https://example.com/polygon\","
                                "\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
                                "\"type\":\"object\",\"properties\":{\"validProp\":true},"
                                "\"additionalProperties\":false}";
const char* const kSpecInstance = "{\"validProp\":5,\"disallowedProp\":\"value\"}";

std::string read_fixture(const char* name) {
    std::string path = TEST_JSONSCHEMA_OUTPUT_DIR;
    path += "/";
    path += name;
    FILE* f = fopen(path.c_str(), "rb");
    EXPECT_NE(f, nullptr) << "fixture " << name << " must be readable";
    if (f == NULL) {
        return "";
    }
    std::string out;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    return out;
}

// Validates the instance against the schema and returns the report.
yeptris_validate_report* validate(const char* schema, const char* instance) {
    yeptris_validate_report* r = NULL;
    YeptrisStatus st =
        yeptris_json_schema_validate(schema, strlen(schema), instance, strlen(instance), &r);
    EXPECT_EQ(st, YEPTRIS_ERROR_SCHEMA) << "the fixture scenario is invalid by construction";
    EXPECT_NE(r, nullptr);
    return r;
}

} // namespace

// ---- the byte-for-byte fixture (derived from the spec's 12.4.4 output) --

TEST(ValidateOutput, SpecExampleBytesArePinned) {
    yeptris_validate_report* r = validate(kSpecSchema, kSpecInstance);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);

    const std::string fixture = read_fixture("spec-12-4-4-basic.json");
    ASSERT_FALSE(fixture.empty());

    // measure form: the exact byte count (no NUL)
    EXPECT_EQ(yeptris_validate_report_write(r, NULL, 0), fixture.size());

    std::string out(fixture.size() + 1, '\0');
    size_t wrote = yeptris_validate_report_write(r, &out[0], out.size());
    out.resize(wrote);
    EXPECT_EQ(out, fixture) << "the transform must reproduce the fixture byte for byte";
    yeptris_validate_report_free(r);
}

// ---- the buffer contract (the emit-options convention) ------------------

TEST(ValidateOutput, BufferContractMeasureRefuseWrite) {
    yeptris_validate_report* r = validate(kSpecSchema, kSpecInstance);
    size_t need = yeptris_validate_report_write(r, NULL, 0);
    EXPECT_GT(need, (size_t)0);

    // too small: nothing is written, the needed size comes back
    std::string small(need, '\0');
    memset(&small[0], 'x', need);
    EXPECT_EQ(yeptris_validate_report_write(r, &small[0], need), need);
    for (size_t i = 0; i < need; i++) {
        ASSERT_EQ(small[i], 'x') << "byte " << i << " was written despite the short cap";
    }
    // cap == need is still too small: the NUL needs one more byte
    EXPECT_EQ(yeptris_validate_report_write(r, &small[0], need), need);

    // cap == need + 1: the exact bytes, NUL-terminated
    std::string fit(need + 1, '\0');
    EXPECT_EQ(yeptris_validate_report_write(r, &fit[0], fit.size()), need);
    EXPECT_EQ(fit[need], '\0');

    yeptris_validate_report_free(r);
}

TEST(ValidateOutput, ValidReportIsTheFlagShape) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_json_schema_validate("{\"type\":\"integer\"}",
                                           strlen("{\"type\":\"integer\"}"), "42", 2, &r),
              YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    char buf[128];
    size_t wrote = yeptris_validate_report_write(r, buf, sizeof(buf));
    EXPECT_STREQ(buf, "{\"valid\":true,\"keywordLocation\":\"\",\"instanceLocation\":\"\"}");
    EXPECT_EQ(wrote, strlen(buf));
    EXPECT_EQ(yeptris_validate_report_write(r, NULL, 0), strlen(buf));
    yeptris_validate_report_free(r);
}

TEST(ValidateOutput, NullReportWritesNothing) {
    char buf[8];
    EXPECT_EQ(yeptris_validate_report_write(NULL, buf, sizeof(buf)), (size_t)0);
    EXPECT_EQ(yeptris_validate_report_write(NULL, NULL, 0), (size_t)0);
}

TEST(ValidateOutput, Deterministic) {
    yeptris_validate_report* r = validate(kSpecSchema, kSpecInstance);
    char a[512], b[512];
    size_t na = yeptris_validate_report_write(r, a, sizeof(a));
    size_t nb = yeptris_validate_report_write(r, b, sizeof(b));
    ASSERT_EQ(na, nb);
    EXPECT_EQ(memcmp(a, b, na), 0) << "the same report always produces the same bytes";
    yeptris_validate_report_free(r);
}

// ---- JSON string escaping ----------------------------------------------

TEST(ValidateOutput, StringsAreJsonEscaped) {
    // a property whose name carries a quote and a backslash: the detail
    // text embeds both raw — the output must re-escape them
    const char* schema = "{\"additionalProperties\":false}";
    const char* instance = "{\"a\\\"b\\\\c\": 1}"; // YAML flow: key is a"b\c
    yeptris_validate_report* r = validate(schema, instance);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    const yeptris_diagnostic* d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(strstr(d->detail, "a\"b\\c"), nullptr) << d->detail;

    char buf[512];
    size_t wrote = yeptris_validate_report_write(r, buf, sizeof(buf));
    ASSERT_GT(wrote, (size_t)0);
    EXPECT_NE(strstr(buf, "\"error\":\"additional property \\\"a\\\"b\\\\c\\\"\""), nullptr) << buf;
    yeptris_validate_report_free(r);
}

// ---- the self-check: our output satisfies the PUBLISHED meta-schema -----

TEST(ValidateOutput, OutputValidatesAgainstThePublishedMetaSchema) {
    const std::string meta = read_fixture("schema.json");
    ASSERT_FALSE(meta.empty());

    // an invalid report's output: a valid output unit with errors
    {
        yeptris_validate_report* r = validate(kSpecSchema, kSpecInstance);
        char buf[512];
        size_t n = yeptris_validate_report_write(r, buf, sizeof(buf));
        ASSERT_GT(n, (size_t)0);
        yeptris_validate_report* o = NULL;
        ASSERT_EQ(yeptris_json_schema_validate(meta.data(), meta.size(), buf, n, &o), YEPTRIS_OK)
            << "the transform's output must satisfy the published meta-schema";
        EXPECT_EQ(yeptris_validate_report_count(o), (size_t)0);
        yeptris_validate_report_free(o);
        yeptris_validate_report_free(r);
    }
    // a valid report's output: the flag shape
    {
        yeptris_validate_report* r = NULL;
        ASSERT_EQ(yeptris_json_schema_validate("{\"type\":\"integer\"}",
                                               strlen("{\"type\":\"integer\"}"), "42", 2, &r),
                  YEPTRIS_OK);
        char buf[128];
        size_t n = yeptris_validate_report_write(r, buf, sizeof(buf));
        ASSERT_GT(n, (size_t)0);
        yeptris_validate_report* o = NULL;
        ASSERT_EQ(yeptris_json_schema_validate(meta.data(), meta.size(), buf, n, &o), YEPTRIS_OK);
        EXPECT_EQ(yeptris_validate_report_count(o), (size_t)0);
        yeptris_validate_report_free(o);
        yeptris_validate_report_free(r);
    }
    // the keyword rail: every keyword-bearing code maps to its pointer
    {
        // one schema object, one miss per keyword family — a sample of
        // the mapping (TYPE above, MIN_ITEMS here)
        yeptris_validate_report* r = NULL;
        ASSERT_EQ(yeptris_json_schema_validate("{\"minItems\":3}", strlen("{\"minItems\":3}"), "[]",
                                               2, &r),
                  YEPTRIS_ERROR_SCHEMA);
        char buf[512];
        size_t n = yeptris_validate_report_write(r, buf, sizeof(buf));
        ASSERT_GT(n, (size_t)0);
        EXPECT_NE(strstr(buf, "\"keywordLocation\":\"/minItems\""), nullptr) << buf;
        yeptris_validate_report* o = NULL;
        ASSERT_EQ(yeptris_json_schema_validate(meta.data(), meta.size(), buf, n, &o), YEPTRIS_OK);
        yeptris_validate_report_free(o);
        yeptris_validate_report_free(r);
    }
}
