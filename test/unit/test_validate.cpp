// test_validate.cpp — the validation face (issue #882). Pins: the
// 12_CORE/12_JSON family walks (tag consistency, key stringness, the
// spec 10.2.2 Error row, duplicate keys by canonical form, line/col
// and JSON-pointer paths, multi-document streams) and the JSON Schema
// draft 2020-12 keyword subset (type/required/enum/const/pattern/
// numeric+string+array bounds, uniqueItems, properties/additional,
// items/prefixItems, allOf/anyOf/oneOf/not, local $ref, boolean
// schemas, unknown keywords ignored, the depth/ref guards), plus the
// bounded regex kernel behind `pattern`.
#include <gtest/gtest.h>

#include <cstring>
#include <string>

#include <yeptris/dom.h>
#include <yeptris/parse.h>
#include <yeptris/validate.h>

#include "validate/internal.h" /* yep_re_search */

namespace {

void free_report(yeptris_validate_report* r) {
    yeptris_validate_report_free(r);
}

// Runs the family validation and returns the first diagnostic (or
// NULL when valid).
const yeptris_diagnostic* validate_first(const char* src, YeptrisSchema schema,
                                         yeptris_validate_report** out) {
    *out = NULL;
    YeptrisStatus st = yeptris_validate(src, strlen(src), schema, out);
    if (st == YEPTRIS_OK) {
        EXPECT_EQ(*out != nullptr, true) << "valid documents still get a report";
        return nullptr;
    }
    EXPECT_EQ(st, YEPTRIS_ERROR_SCHEMA) << "family violations ride ERROR_SCHEMA";
    EXPECT_NE(*out, nullptr);
    EXPECT_GE(yeptris_validate_report_count(*out), (size_t)1);
    return yeptris_validate_report_at(*out, 0);
}

} // namespace

// ---- the schema-family walk --------------------------------------------

TEST(Validate, ValidCoreDocumentIsClean) {
    yeptris_validate_report* r = NULL;
    const char* src = "name: yeptris\n"
                      "counts: [1, 2, 3]\n"
                      "nested:\n  deep: true\n";
    ASSERT_EQ(yeptris_validate(src, strlen(src), YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    EXPECT_EQ(yeptris_validate_report_at(r, 0), nullptr);
    free_report(r);
}

TEST(Validate, TagMismatchCarriesPathAndPosition) {
    yeptris_validate_report* r = NULL;
    const char* src = "n: !!int abc\n";
    const yeptris_diagnostic* d = validate_first(src, YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    EXPECT_STREQ(d->path, "/n");
    EXPECT_EQ(d->line, (uint32_t)1);
    EXPECT_EQ(d->col, (uint32_t)10); /* the content bytes start at column 10 */
    EXPECT_STREQ(d->detail, "explicit int tag on string content");
    free_report(r);
}

TEST(Validate, ExplicitTagsThatMatchAreFine) {
    yeptris_validate_report* r = NULL;
    const char* src = "a: !!str 42\n"
                      "b: !!int \"42\"\n"
                      "c: !!int 0x1A\n"
                      "d: !!bool true\n"
                      "e: !!null\n";
    ASSERT_EQ(yeptris_validate(src, strlen(src), YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    free_report(r);
}

TEST(Validate, SeqMapTagsCheckTheNodeKind) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d =
        validate_first("x: !!seq not-a-seq\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    free_report(r);

    const yeptris_diagnostic* d2 = validate_first("- !!map [a]\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    free_report(r);
}

TEST(Validate, JsonFamilyFlagsPlainNonProductionScalars) {
    // spec 10.2.2: a plain scalar matching no JSON production is the
    // Error row — under 12_JSON, not under 12_CORE (str default). The
    // key is quoted so the VALUE diagnostic is the one observed.
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = validate_first("\"name\": hello\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    EXPECT_STREQ(d->path, "/name");
    free_report(r);

    r = NULL;
    ASSERT_EQ(yeptris_validate("name: hello\n", strlen("name: hello\n"), YEPTRIS_SCHEMA_12_CORE,
                               &r),
              YEPTRIS_OK);
    free_report(r);

    // quoted scalars are strings: fine under the JSON family
    r = NULL;
    ASSERT_EQ(yeptris_validate("\"name\": \"hello\"\n", strlen("\"name\": \"hello\"\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);

    // an unquoted plain KEY matches no production either (the Error row
    // applies to keys): the key diagnostic rides the mapping's path
    const yeptris_diagnostic* dk = validate_first("y: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(dk, nullptr);
    EXPECT_EQ(dk->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    EXPECT_STREQ(dk->path, "");
    free_report(r);
}

TEST(Validate, JsonFamilyFlagsNonStringKeysButAcceptsStringOnes) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = validate_first("123: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    EXPECT_STREQ(d->detail, "mapping key is int (expected a string)");
    free_report(r);

    // core-12 allows keys of any type
    r = NULL;
    ASSERT_EQ(yeptris_validate("123: \"v\"\n", strlen("123: \"v\"\n"), YEPTRIS_SCHEMA_12_CORE,
                               &r),
              YEPTRIS_OK);
    free_report(r);

    // bool/null/float productions resolve too: keys of those types fail
    const yeptris_diagnostic* d2 = validate_first("true: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    free_report(r);
    const yeptris_diagnostic* d3 = validate_first("~: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE); /* ~ matches no production */
    free_report(r);
    const yeptris_diagnostic* d4 = validate_first("null: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d4, nullptr);
    EXPECT_EQ(d4->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    free_report(r);

    // quoted keys are strings
    r = NULL;
    ASSERT_EQ(yeptris_validate("\"123\": \"v\"\n", strlen("\"123\": \"v\"\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);
}

TEST(Validate, JsonFamilyRejectsNonJsonTags) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d =
        validate_first("\"made\": !!timestamp 2001-12-14\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    free_report(r);

    const yeptris_diagnostic* d2 =
        validate_first("\"x\": !vendor thing\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    free_report(r);

    // core-12 leaves consumer tags alone
    r = NULL;
    ASSERT_EQ(yeptris_validate("x: !vendor thing\nmade: !!timestamp 2001-12-14\n",
                               strlen("x: !vendor thing\nmade: !!timestamp 2001-12-14\n"),
                               YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);
}

TEST(Validate, JsonFamilyTagConsistencyUsesJsonProductions) {
    // !!int on 0x1A: core-resolvable, but 0x1A matches no JSON production
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = validate_first("\"a\": !!int 0x1A\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    free_report(r);

    r = NULL;
    ASSERT_EQ(yeptris_validate("a: !!int 0x1A\n", strlen("a: !!int 0x1A\n"),
                               YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);
}

TEST(Validate, DuplicateKeysBothFamiliesCanonicalForm) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = validate_first("a: 1\na: 2\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);

    // spec 3.2.1.3: 0o13 and 0xB are the same int — ONE key
    const yeptris_diagnostic* d2 =
        validate_first("k: 0o13\nk: 0xB\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);

    // the same KEY twice with differently-typed values is the issue's
    // "multiple types for one key": still one duplicate
    const yeptris_diagnostic* dm =
        validate_first("k: 1\nk: 1.0\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(dm, nullptr);
    EXPECT_EQ(dm->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);

    // int 1 vs float 1.0 AS keys carry different tags: distinct keys
    r = NULL;
    ASSERT_EQ(yeptris_validate("1: a\n1.0: b\n", strlen("1: a\n1.0: b\n"), YEPTRIS_SCHEMA_12_CORE,
                               &r),
              YEPTRIS_OK);
    free_report(r);

    // nested duplicate: the diagnostic rides the inner mapping's path
    const yeptris_diagnostic* d3 =
        validate_first("x:\n  a: 1\n  a: 2\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    EXPECT_STREQ(d3->path, "/x");
    free_report(r);
}

TEST(Validate, PathsEscapeJsonPointerTokens) {
    yeptris_validate_report* r = NULL;
    const char* src = "a/b: !!int x\n";
    const yeptris_diagnostic* d = validate_first(src, YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->path, "/a~1b");
    free_report(r);
}

TEST(Validate, MultiDocumentStreamsValidatePerDocument) {
    yeptris_validate_report* r = NULL;
    const char* src = "--- 1\n--- !!int x\n";
    const yeptris_diagnostic* d = validate_first(src, YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    EXPECT_STREQ(d->path, ""); /* root-relative per document */
    EXPECT_EQ(d->line, (uint32_t)2);
    free_report(r);
}

TEST(Validate, EmptyAndEdgeInputs) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_validate("", 0, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    free_report(r);

    // a bare `---` document is an empty plain scalar: fine under core,
    // the Error row under the strict JSON family
    r = NULL;
    ASSERT_EQ(yeptris_validate("---\n", 4, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    free_report(r);
    const yeptris_diagnostic* de = validate_first("---\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(de, nullptr);
    EXPECT_EQ(de->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    free_report(r);

    // NULL with a nonzero length is an argument error
    EXPECT_EQ(yeptris_validate(NULL, 5, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_ERROR_ARG);
    // 11_COMPAT is a parse resolver, not a validation family
    EXPECT_EQ(yeptris_validate("a: 1\n", 5, YEPTRIS_SCHEMA_11_COMPAT, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(r, nullptr);
}

TEST(Validate, DocumentEntryValidatesAnExistingParse) {
    yeptris_validate_report* r = NULL;
    const char* src = "n: !!int abc\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(src, strlen(src), &st);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(yeptris_validate_document(doc, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    EXPECT_EQ(yeptris_validate_report_at(r, 0)->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    free_report(r);
    yeptris_document_free(doc);
}

TEST(Validate, TheIssueExamplePath) {
    // /resources/andale32/sha256 — the shape from the issue text
    yeptris_validate_report* r = NULL;
    const char* src = "resources:\n"
                      "  andale32:\n"
                      "    sha256: !!int nope\n";
    const yeptris_diagnostic* d = validate_first(src, YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->path, "/resources/andale32/sha256");
    EXPECT_EQ(d->line, (uint32_t)3);
    free_report(r);
}

// ---- the JSON Schema walk (draft 2020-12 subset) ------------------------

namespace {

// schema+doc -> first diagnostic (NULL when valid)
const yeptris_diagnostic* js_first(const char* schema, const char* doc,
                                   yeptris_validate_report** out) {
    *out = NULL;
    YeptrisStatus st =
        yeptris_json_schema_validate(schema, strlen(schema), doc, strlen(doc), out);
    if (st == YEPTRIS_OK) {
        return nullptr;
    }
    EXPECT_EQ(st, YEPTRIS_ERROR_SCHEMA);
    return yeptris_validate_report_at(*out, 0);
}

} // namespace

TEST(ValidateJsonSchema, TypeRequiredProperties) {
    const char* schema = "{\"type\":\"object\","
                         "\"required\":[\"name\"],"
                         "\"properties\":{\"name\":{\"type\":\"string\"}}}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "name: yeptris\n", &r), nullptr);
    free_report(r);

    const yeptris_diagnostic* d = js_first(schema, "other: 1\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_REQUIRED);
    EXPECT_STREQ(d->path, "/name"); /* the missing leaf's pointer */
    EXPECT_NE(strstr(d->detail, "name"), nullptr);
    free_report(r);

    const yeptris_diagnostic* d2 = js_first(schema, "name: [x]\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d2->path, "/name");
    free_report(r);
}

TEST(ValidateJsonSchema, NestedPathIsTheInstancePointer) {
    const char* schema = "{\"type\":\"object\",\"properties\":{"
                         "\"resources\":{\"type\":\"object\",\"properties\":{"
                         "\"andale32\":{\"type\":\"object\",\"properties\":{"
                         "\"sha256\":{\"type\":\"string\"}}}}}}}";
    const char* doc = "resources:\n  andale32:\n    sha256: 6\n";
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = js_first(schema, doc, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d->path, "/resources/andale32/sha256");
    EXPECT_NE(strstr(d->detail, "string"), nullptr);
    EXPECT_NE(strstr(d->detail, "integer"), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, EnumConst) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"enum\":[1, 2.5, \"a\", null, true]}", "'a'", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first("{\"enum\":[1, 2]}", "3\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_ENUM);
    EXPECT_NE(strstr(d->detail, "2 enum"), nullptr);
    free_report(r);

    ASSERT_EQ(js_first("{\"const\":\"x\"}", "\"x\"", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first("{\"const\":\"x\"}", "\"y\"", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_CONST);
    free_report(r);

    // structural enum members: arrays and objects
    ASSERT_EQ(js_first("{\"enum\":[[1, 2], {\"k\": true}]}", "[1, 2]", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d3 = js_first("{\"enum\":[[1, 2]]}", "[2, 1]", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_ENUM);
    free_report(r);
}

TEST(ValidateJsonSchema, NumericBoundsAndExclusiveForms) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d =
        js_first("{\"type\":\"number\",\"minimum\":3}", "2\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MINIMUM);
    free_report(r);

    ASSERT_EQ(js_first("{\"type\":\"number\",\"minimum\":3}", "3\n", &r), nullptr);
    free_report(r);

    const yeptris_diagnostic* d2 =
        js_first("{\"type\":\"integer\",\"exclusiveMinimum\":3}", "3\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_EXCLUSIVE_MINIMUM);
    free_report(r);

    const yeptris_diagnostic* d3 =
        js_first("{\"type\":\"number\",\"maximum\":10}", "10.5\n", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_MAXIMUM);
    free_report(r);

    const yeptris_diagnostic* d4 =
        js_first("{\"type\":\"number\",\"exclusiveMaximum\":10}", "10\n", &r);
    ASSERT_NE(d4, nullptr);
    EXPECT_EQ(d4->code, (uint32_t)YEPTRIS_DIAG_EXCLUSIVE_MAXIMUM);
    free_report(r);

    // 2020-12: 1.0 IS an integer (zero fractional part)
    ASSERT_EQ(js_first("{\"type\":\"integer\"}", "1.0\n", &r), nullptr);
    free_report(r);
    // core data model: "y" is boolean — not a number
    const yeptris_diagnostic* d5 = js_first("{\"type\":\"number\"}", "y\n", &r);
    ASSERT_NE(d5, nullptr);
    EXPECT_EQ(d5->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    free_report(r);
}

TEST(ValidateJsonSchema, StringBoundsUseCodePoints) {
    yeptris_validate_report* r = NULL;
    // "éé" is 4 UTF-8 bytes but 2 code points
    const yeptris_diagnostic* d = js_first("{\"type\":\"string\",\"minLength\":3}",
                                           "\"\xC3\xA9\xC3\xA9\"", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MIN_LENGTH);
    free_report(r);

    ASSERT_EQ(js_first("{\"type\":\"string\",\"maxLength\":2}", "\"\xC3\xA9\xC3\xA9\"", &r),
              nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, PatternMatchesThroughTheBoundedKernel) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"type\":\"string\",\"pattern\":\"^[a-z]+$\"}", "\"abc\"", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"type\":\"string\",\"pattern\":\"^[a-z]+$\"}", "\"ABC\"", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_PATTERN);
    free_report(r);
    // an invalid pattern (backreference) is IGNORED, never a guess
    ASSERT_EQ(js_first("{\"type\":\"string\",\"pattern\":\"(a)\\\\1\"}", "\"zz\"", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, ArrayBoundsUniqueItems) {
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = js_first("{\"minItems\":2}", "[1]", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MIN_ITEMS);
    free_report(r);

    const yeptris_diagnostic* d2 = js_first("{\"maxItems\":1}", "[1, 2]", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_MAX_ITEMS);
    free_report(r);

    const yeptris_diagnostic* d3 = js_first("{\"uniqueItems\":true}", "[1, 1]", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_UNIQUE_ITEMS);
    free_report(r);

    // structurally equal objects are duplicates too
    const yeptris_diagnostic* d4 = js_first("{\"uniqueItems\":true}", "[{a: 1}, {a: 1}]", &r);
    ASSERT_NE(d4, nullptr);
    EXPECT_EQ(d4->code, (uint32_t)YEPTRIS_DIAG_UNIQUE_ITEMS);
    free_report(r);

    ASSERT_EQ(js_first("{\"uniqueItems\":true}", "[{a: 1}, {a: 2}]", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, ItemsAndPrefixItems) {
    yeptris_validate_report* r = NULL;
    // items: one schema over every element
    const yeptris_diagnostic* d = js_first("{\"items\":{\"type\":\"integer\"}}", "[1, \"x\", 3]",
                                           &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d->path, "/1");
    free_report(r);

    // prefixItems per index; items covers the tail
    const char* schema = "{\"prefixItems\":[{\"type\":\"string\"},{\"type\":\"integer\"}],"
                         "\"items\":{\"type\":\"boolean\"}}";
    ASSERT_EQ(js_first(schema, "[\"a\", 1, true]", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first(schema, "[\"a\", 1, \"x\"]", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d2->path, "/2");
    free_report(r);

    // the draft-07 array form of items rides the prefixItems semantics
    const char* draft07 = "{\"items\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}";
    ASSERT_EQ(js_first(draft07, "[\"a\", 1]", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d3 = js_first(draft07, "[1, \"a\"]", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d3->path, "/0");
    free_report(r);
}

TEST(ValidateJsonSchema, AdditionalPropertiesBoolAndSchema) {
    const char* schema = "{\"type\":\"object\",\"properties\":{\"a\":{}},"
                         "\"additionalProperties\":false}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "a: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "a: 1\nextra: 2\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_ADDITIONAL_PROPERTY);
    EXPECT_NE(strstr(d->detail, "extra"), nullptr);
    free_report(r);

    // as a schema: unknown values validate against it
    const char* schema2 = "{\"additionalProperties\":{\"type\":\"string\"}}";
    ASSERT_EQ(js_first(schema2, "any: text\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first(schema2, "any: [1]\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d2->path, "/any");
    free_report(r);
}

TEST(ValidateJsonSchema, AllOfAnyOfOneOfNot) {
    yeptris_validate_report* r = NULL;
    // allOf: every branch applies
    const char* all = "{\"allOf\":[{\"type\":\"object\"},{\"properties\":{\"a\":{\"type\":"
                      "\"integer\"}}}]}";
    ASSERT_EQ(js_first(all, "a: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(all, "a: x\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    free_report(r);

    // anyOf: at least one
    const char* any = "{\"anyOf\":[{\"type\":\"integer\"},{\"type\":\"string\"}]}";
    ASSERT_EQ(js_first(any, "\"s\"\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first(any, "[1]\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_ANY_OF);
    free_report(r);

    // oneOf: exactly one
    const char* one = "{\"oneOf\":[{\"type\":\"integer\"},{\"minimum\":5}]}";
    ASSERT_EQ(js_first(one, "1\n", &r), nullptr); /* int only */
    free_report(r);
    ASSERT_EQ(js_first(one, "5.5\n", &r), nullptr); /* float >= 5: minimum only */
    free_report(r);
    const yeptris_diagnostic* d3 = js_first(one, "7\n", &r); /* both */
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_ONE_OF);
    EXPECT_NE(strstr(d3->detail, "2 of"), nullptr);
    free_report(r);
    const yeptris_diagnostic* d4 = js_first(one, "2.5\n", &r); /* neither branch */
    ASSERT_NE(d4, nullptr);
    EXPECT_EQ(d4->code, (uint32_t)YEPTRIS_DIAG_ONE_OF);
    free_report(r);

    // not
    const yeptris_diagnostic* d5 = js_first("{\"not\":{\"type\":\"integer\"}}", "1\n", &r);
    ASSERT_NE(d5, nullptr);
    EXPECT_EQ(d5->code, (uint32_t)YEPTRIS_DIAG_NOT);
    free_report(r);
    ASSERT_EQ(js_first("{\"not\":{\"type\":\"integer\"}}", "\"x\"\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, LocalRefsResolveAndCarryPaths) {
    yeptris_validate_report* r = NULL;
    const char* schema = "{\"$defs\":{\"int\":{\"type\":\"integer\"}},"
                         "\"properties\":{\"n\":{\"$ref\":\"#/$defs/int\"}}}";
    ASSERT_EQ(js_first(schema, "n: 4\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "n: \"x\"\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d->path, "/n");
    free_report(r);

    // the draft-07 definitions keyword resolves identically
    const char* schema2 = "{\"definitions\":{\"s\":{\"type\":\"string\"}},"
                          "\"$ref\":\"#/definitions/s\"}";
    const yeptris_diagnostic* d2 = js_first(schema2, "1\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    free_report(r);

    // an unresolvable pointer is a REF diagnostic at the instance
    const yeptris_diagnostic* d3 =
        js_first("{\"$ref\":\"#/nowhere\"}", "\"x\"\n", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_REF);
    free_report(r);
}

TEST(ValidateJsonSchema, RefCyclesAreGuardedNeverCrash) {
    yeptris_validate_report* r = NULL;
    // a direct self-reference over a shallow instance: the chain cap
    // fires (never a hang, never a crash)
    const yeptris_diagnostic* d = js_first("{\"$ref\":\"#\"}", "{a: {b: 1}}\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_REF);
    free_report(r);

    // a mutual cycle through two definitions
    const char* schema = "{\"definitions\":{\"a\":{\"$ref\":\"#/definitions/b\"},"
                         "\"b\":{\"$ref\":\"#/definitions/a\"}},\"$ref\":\"#/definitions/a\"}";
    const yeptris_diagnostic* d2 = js_first(schema, "\"x\"\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_REF);
    free_report(r);
}

TEST(ValidateJsonSchema, BooleanSchemasAndUnknownKeywords) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("true", "\"anything\"\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first("false", "\"anything\"\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_FALSE_SCHEMA);
    free_report(r);

    // unknown keywords are ignored (2020-12); known keywords with
    // unreadable values are ignored too
    const char* schema = "{\"x-nonsense\":42,\"type\":5,\"minimum\":\"no\",\"properties\":7}";
    ASSERT_EQ(js_first(schema, "anything: here\n", &r), nullptr);
    free_report(r);

    // an empty schema constrains nothing
    ASSERT_EQ(js_first("{}", "[1, 2]\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, BadInputs) {
    yeptris_validate_report* r = NULL;
    // the schema bytes must parse
    EXPECT_EQ(yeptris_json_schema_validate("{unbalanced", 11, "1\n", 2, &r), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
    // the schema must hold a document
    EXPECT_EQ(yeptris_json_schema_validate("", 0, "1\n", 2, &r), YEPTRIS_ERROR_ARG);
    // the instance must parse
    EXPECT_EQ(yeptris_json_schema_validate("true", 4, "{oops", 5, &r), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
    // argument gates
    EXPECT_EQ(yeptris_json_schema_validate("true", 4, NULL, 3, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_json_schema_validate(NULL, 4, "1", 1, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_json_schema_validate("true", 4, "1", 1, NULL), YEPTRIS_ERROR_ARG);
}

// ---- the bounded regex kernel (internal.h) -------------------------------

TEST(ValidateRegex, SearchSemantics) {
    // unanchored search
    EXPECT_EQ(yep_re_search("bc", (uint32_t)strlen("bc"), "abcd", 4), 1);
    EXPECT_EQ(yep_re_search("xyz", (uint32_t)strlen("xyz"), "abcd", 4), 0);
    // anchors
    EXPECT_EQ(yep_re_search("^ab", (uint32_t)strlen("^ab"), "abcd", 4), 1);
    EXPECT_EQ(yep_re_search("^cd", (uint32_t)strlen("^cd"), "abcd", 4), 0);
    EXPECT_EQ(yep_re_search("cd$", (uint32_t)strlen("cd$"), "abcd", 4), 1);
    EXPECT_EQ(yep_re_search("^abcd$", (uint32_t)strlen("^abcd$"), "abcd", 4), 1);
    // classes, dot, escapes
    EXPECT_EQ(yep_re_search("^[a-z0-9_]+$", (uint32_t)strlen("^[a-z0-9_]+$"), "a_9z", 4), 1);
    EXPECT_EQ(yep_re_search("^[a-z]+$", (uint32_t)strlen("^[a-z]+$"), "a9z", 3), 0);
    EXPECT_EQ(yep_re_search("a.c", (uint32_t)strlen("a.c"), "abc", 3), 1);
    EXPECT_EQ(yep_re_search("a\\.c", (uint32_t)strlen("a\\.c"), "abc", 3), 0);
    EXPECT_EQ(yep_re_search("\\d+", (uint32_t)strlen("\\d+"), "x42y", 4), 1);
    EXPECT_EQ(yep_re_search("\\D\\D", (uint32_t)strlen("\\D\\D"), "42", 2), 0);
    // alternation and groups
    EXPECT_EQ(yep_re_search("^(cat|dog)s?$", (uint32_t)strlen("^(cat|dog)s?$"), "cats", 4), 1);
    EXPECT_EQ(yep_re_search("^(cat|dog)s?$", (uint32_t)strlen("^(cat|dog)s?$"), "dogs", 4), 1);
    EXPECT_EQ(yep_re_search("^(cat|dog)s?$", (uint32_t)strlen("^(cat|dog)s?$"), "cows", 4), 0);
    EXPECT_EQ(yep_re_search("^(?:ab)+$", (uint32_t)strlen("^(?:ab)+$"), "abab", 4), 1);
    // quantifiers incl. {n,m}
    EXPECT_EQ(yep_re_search("^a{2,3}$", (uint32_t)strlen("^a{2,3}$"), "aa", 2), 1);
    EXPECT_EQ(yep_re_search("^a{2,3}$", (uint32_t)strlen("^a{2,3}$"), "aaaa", 4), 0);
    EXPECT_EQ(yep_re_search("^a*b+$", (uint32_t)strlen("^a*b+$"), "aaab", 4), 1);
    EXPECT_EQ(yep_re_search("^a?b?$", (uint32_t)strlen("^a?b?$"), "ab", 2), 1);
    // a literal '{' is not a quantifier
    EXPECT_EQ(yep_re_search("a{x", (uint32_t)strlen("a{x"), "ax", 2), 0);
    // word boundary
    EXPECT_EQ(yep_re_search("\\bcat\\b", (uint32_t)strlen("\\bcat\\b"), "a cat sat", 9), 1);
    EXPECT_EQ(yep_re_search("\\bcat\\b", (uint32_t)strlen("\\bcat\\b"), "scatter", 7), 0);
    // the NFA is linear: the classic blowup pattern answers instantly
    const char* blowup = "aaaaaaaaaaaaaaaaaaaaaaaaaaaab";
    EXPECT_EQ(yep_re_search("(a+)+$", (uint32_t)strlen("(a+)+$"), blowup,
                            (uint32_t)strlen(blowup)), 0);
    // SyntaxErrors
    EXPECT_EQ(yep_re_search("(a", (uint32_t)strlen("(a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("[a", (uint32_t)strlen("[a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("a\\q", (uint32_t)strlen("a\\q"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("*a", (uint32_t)strlen("*a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("(a)\\1", (uint32_t)strlen("(a)\\1"), "aa", 2), -1);
    EXPECT_EQ(yep_re_search("", (uint32_t)strlen(""), "a", 1), -1);
}
