// test_validate.cpp — the validation face (issue #882). Pins: the
// 12_CORE/12_JSON family walks (tag consistency, key stringness, the
// spec 10.2.2 Error row, duplicate keys by canonical form, line/col
// and JSON-pointer paths, multi-document streams) and the JSON Schema
// draft 2020-12 keyword subset (type/required/enum/const/pattern/
// numeric+string+array bounds, uniqueItems, properties/additional,
// items/prefixItems, allOf/anyOf/oneOf/not, local $ref, boolean
// schemas, unknown keywords ignored, the depth/ref guards) plus the
// #885 item 2 sweep keywords (multipleOf, min/maxProperties,
// patternProperties, propertyNames, contains family,
// dependentRequired/dependentSchemas, if/then/else) and the
// suite-exposed fixes (order-independent mapping hashes, cross-tag
// numeric const/enum equality), plus the bounded regex kernel behind
// `pattern`, the stream face's per-document diagnostics (#885 item 4:
// multi-document streams address their documents as array elements —
// "/1/k" is key k of document 1; single-document streams keep the
// bare instance pointers; the parse depth guard composes), and the
// strict lanes' per-family resolvability (#885 item 5: `0o7` under
// JSON, string keys, enumerate-every-miss duplicate keys).
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

TEST(Validate, JsonFamilyResolvesPlainNonProductionScalarsAsStrings) {
    // spec 10.2's example 10.8 resolves plain scalars matching no JSON
    // production as STRINGS — keys ("A null", "Booleans") and values
    // (the "Invalid" list: True/Null/0o7/0x3A/+12.3) alike. The table's
    // "should consider them to be an error" note governs JSON-file
    // processing warnings, not family validity; the walk's teeth are
    // the string-key demand (next test) and explicit non-JSON tags.
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_validate("\"name\": hello\n", strlen("\"name\": hello\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);

    // under 12_CORE this is the str default; same verdict
    r = NULL;
    ASSERT_EQ(
        yeptris_validate("name: hello\n", strlen("name: hello\n"), YEPTRIS_SCHEMA_12_CORE, &r),
        YEPTRIS_OK);
    free_report(r);

    // quoted scalars are strings: fine under the JSON family
    r = NULL;
    ASSERT_EQ(yeptris_validate("\"name\": \"hello\"\n", strlen("\"name\": \"hello\"\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);

    // the family is still stricter than core where a production DOES
    // match: 0o7 stays a string under 12_JSON (no radix production)
    // while core resolves it int — the divergence is in resolution,
    // not in a diagnostic
    r = NULL;
    ASSERT_EQ(yeptris_validate("\"name\": [ 0o7, +12.3, True ]\n",
                               strlen("\"name\": [ 0o7, +12.3, True ]\n"), YEPTRIS_SCHEMA_12_JSON,
                               &r),
              YEPTRIS_OK);
    free_report(r);

    // the spec's own 10.8 example document, verbatim shape: valid
    r = NULL;
    ASSERT_EQ(yeptris_validate("A null: null\nBooleans: [ true, false ]\n"
                               "Integers: [ 0, -0, 3, -19 ]\n"
                               "Floats: [ 0., -0.0, 12e03, -2E+05 ]\n"
                               "Invalid: [ True, Null, 0o7, 0x3A, +12.3 ]\n",
                               strlen("A null: null\nBooleans: [ true, false ]\n"
                                      "Integers: [ 0, -0, 3, -19 ]\n"
                                      "Floats: [ 0., -0.0, 12e03, -2E+05 ]\n"
                                      "Invalid: [ True, Null, 0o7, 0x3A, +12.3 ]\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
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
    ASSERT_EQ(yeptris_validate("123: \"v\"\n", strlen("123: \"v\"\n"), YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);

    // bool/null/float productions resolve too: keys of those types fail
    const yeptris_diagnostic* d2 = validate_first("true: \"v\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    free_report(r);
    // `~` matches no JSON production: as a KEY that makes it a string
    // (same example-10.8 law); only keys resolving to non-string
    // productions fail
    r = NULL;
    ASSERT_EQ(yeptris_validate("~: \"v\"\n", strlen("~: \"v\"\n"), YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
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
    ASSERT_EQ(
        yeptris_validate("a: !!int 0x1A\n", strlen("a: !!int 0x1A\n"), YEPTRIS_SCHEMA_12_CORE, &r),
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
    const yeptris_diagnostic* d2 = validate_first("k: 0o13\nk: 0xB\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);

    // the same KEY twice with differently-typed values is the issue's
    // "multiple types for one key": still one duplicate
    const yeptris_diagnostic* dm = validate_first("k: 1\nk: 1.0\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(dm, nullptr);
    EXPECT_EQ(dm->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);

    // int 1 vs float 1.0 AS keys carry different tags: distinct keys
    r = NULL;
    ASSERT_EQ(
        yeptris_validate("1: a\n1.0: b\n", strlen("1: a\n1.0: b\n"), YEPTRIS_SCHEMA_12_CORE, &r),
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

TEST(Validate, DuplicateKeysEnumerateEveryMiss) {
    // enumerate-all-misses: every key whose canonical form already
    // appeared is its own diagnostic — three equal keys, two misses;
    // the third occurrence matches the retained first key
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d = validate_first("a: 1\na: 2\na: 3\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)2);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    EXPECT_STREQ(d->detail, "duplicate mapping key (\"a\")");
    const yeptris_diagnostic* d2 = yeptris_validate_report_at(r, 1);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    EXPECT_EQ(d2->line, (uint32_t)3); /* the third occurrence's position */
    free_report(r);

    // two distinct duplicated keys enumerate independently
    r = NULL;
    const yeptris_diagnostic* d3 =
        validate_first("a: 1\na: 2\nb: 1\nb: 2\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d3, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)2);
    EXPECT_EQ(yeptris_validate_report_at(r, 0)->line, (uint32_t)2);
    EXPECT_EQ(yeptris_validate_report_at(r, 1)->line, (uint32_t)4);
    free_report(r);

    // FIRST occurrence over a mapping pair: keys equal across sibling
    // mappings are NOT duplicates (uniqueness scopes to one mapping)
    r = NULL;
    ASSERT_EQ(yeptris_validate("- a: 1\n- a: 2\n", strlen("- a: 1\n- a: 2\n"),
                               YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);
}

// ---- the strict lanes (#885 item 5): resolvability per family -----------

TEST(Validate, StrictLaneResolvabilityPerFamily) {
    // the mission's named case: `0o7` is a CORE int production but
    // matches no JSON production — the strict JSON lane resolves it as
    // the STRING it is (example 10.8), so the divergence from core is
    // in RESOLUTION (core says int), not in a diagnostic
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_validate("\"k\": 0o7\n", strlen("\"k\": 0o7\n"), YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);

    r = NULL;
    ASSERT_EQ(yeptris_validate("k: 0o7\n", strlen("k: 0o7\n"), YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);

    // hex: same law — 12_JSON resolves it a string, core resolves int
    r = NULL;
    ASSERT_EQ(
        yeptris_validate("\"k\": 0x1F\n", strlen("\"k\": 0x1F\n"), YEPTRIS_SCHEMA_12_JSON, &r),
        YEPTRIS_OK);
    free_report(r);

    // the core lane's str default: `yes`/`no` are plain strings under
    // 1.2 core (compat owns the bool words), and everything else
    // resolves — the core lane never flags an untagged plain scalar
    r = NULL;
    ASSERT_EQ(yeptris_validate("a: yes\nb: n\nc: 1:30\nd: on\n",
                               strlen("a: yes\nb: n\nc: 1:30\nd: on\n"), YEPTRIS_SCHEMA_12_CORE,
                               &r),
              YEPTRIS_OK);
    free_report(r);

    // key types where the family demands scalars: the strict JSON lane
    // demands STRING keys — int/bool/null/float keys ride
    // KEY_NOT_STRING (collection keys too), core allows them all
    r = NULL;
    const yeptris_diagnostic* dk =
        validate_first("123: \"v\"\ntrue: \"w\"\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(dk, nullptr);
    EXPECT_EQ(dk->code, (uint32_t)YEPTRIS_DIAG_KEY_NOT_STRING);
    free_report(r);
    r = NULL;
    ASSERT_EQ(yeptris_validate("123: v\ntrue: w\n[1]: x\n", strlen("123: v\ntrue: w\n[1]: x\n"),
                               YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
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
    EXPECT_STREQ(d->path, "/1"); /* the stream is an array of documents:
                                  * document 1 (the second) carries the miss */
    EXPECT_EQ(d->line, (uint32_t)2);
    free_report(r);
}

// ---- the stream face: per-document diagnostics (#885 item 4) ------------

TEST(Validate, StreamDiagsCarryTheDocumentIndex) {
    // doc 2 invalid at a nested key: the path names the document
    // ("/1/..."), docs 1 and 3 stay clean — exactly one diagnostic
    yeptris_validate_report* r = NULL;
    const char* src = "--- a: 1\n"
                      "---\n"
                      "resources:\n"
                      "  andale32:\n"
                      "    sha256: !!int nope\n"
                      "--- c: 3\n";
    const yeptris_diagnostic* d = validate_first(src, YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TAG_MISMATCH);
    EXPECT_STREQ(d->path, "/1/resources/andale32/sha256");
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)1);
    EXPECT_EQ(d->line, (uint32_t)5); /* line/col stay input-absolute */
    free_report(r);

    // a flow document mid-stream attributes the same way
    r = NULL;
    const yeptris_diagnostic* d2 =
        validate_first("--- {a: 1}\n--- {b: !!int x}\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_STREQ(d2->path, "/1/b");
    free_report(r);

    // document 0 spells its index too: explicit beats implicit
    r = NULL;
    const yeptris_diagnostic* d3 =
        validate_first("--- !!int x\n--- 2\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_STREQ(d3->path, "/0");
    free_report(r);
}

TEST(Validate, StreamsWithEmptyDocumentsAttributeThem) {
    // the empty middle document rides an explicit non-JSON tag here
    // (implicit empty plains are strings under 12_JSON since example
    // 10.8): the miss is unpositioned (no line/col) but still
    // attributed to document 1
    yeptris_validate_report* r = NULL;
    const yeptris_diagnostic* d =
        validate_first("--- 1\n--- !!timestamp\n--- 3\n", YEPTRIS_SCHEMA_12_JSON, &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_NON_JSON_TYPE);
    EXPECT_STREQ(d->path, "/1");
    EXPECT_EQ(d->line, (uint32_t)0);
    free_report(r);

    // the untagged empty document is a no-production plain scalar: a
    // string under 12_JSON (example 10.8), clean under core
    r = NULL;
    ASSERT_EQ(yeptris_validate("--- 1\n---\n--- 3\n", strlen("--- 1\n---\n--- 3\n"),
                               YEPTRIS_SCHEMA_12_JSON, &r),
              YEPTRIS_OK);
    free_report(r);

    r = NULL;
    ASSERT_EQ(yeptris_validate("--- 1\n---\n--- 3\n", strlen("--- 1\n---\n--- 3\n"),
                               YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_OK);
    free_report(r);
}

TEST(Validate, StreamDiagsCarryTheDocumentIndexViaDocumentEntry) {
    // the same contract through an already-parsed multi-doc forest
    yeptris_validate_report* r = NULL;
    const char* src = "--- k: !!int x\n--- j: 2\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(src, strlen(src), &st);
    ASSERT_NE(doc, nullptr);
    ASSERT_EQ(yeptris_validate_document(doc, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    EXPECT_STREQ(yeptris_validate_report_at(r, 0)->path, "/0/k");
    free_report(r);
    yeptris_document_free(doc);
}

TEST(Validate, DepthGuardComposesWithStreams) {
    // a document past the engine's depth cap fails the PARSE (the
    // guard is the engine's, before any validation runs): status
    // DEPTH, no report — for a deep document mid-stream as much as a
    // lone one, through the family walk and the compiled face alike
    std::string deep(1100, '[');
    deep.append(1100, ']');
    const std::string lone = deep + "\n";
    const std::string mid = "--- 1\n--- " + deep + "\n--- 3\n";

    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_validate(lone.data(), lone.size(), YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_ERROR_DEPTH);
    EXPECT_EQ(r, nullptr);
    r = NULL;
    ASSERT_EQ(yeptris_validate(mid.data(), mid.size(), YEPTRIS_SCHEMA_12_CORE, &r),
              YEPTRIS_ERROR_DEPTH);
    EXPECT_EQ(r, nullptr);

    // the compiled face rides the same parse guard
    const char* schema = "{\"type\":\"array\"}";
    yeptris_schema* s = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s, nullptr);
    r = NULL;
    ASSERT_EQ(yeptris_schema_validate(s, mid.data(), mid.size(), &r), YEPTRIS_ERROR_DEPTH);
    EXPECT_EQ(r, nullptr);
    yeptris_schema_free(s);
}

TEST(Validate, EmptyAndEdgeInputs) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_validate("", 0, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    free_report(r);

    // a bare `---` document is an empty plain scalar: a no-production
    // plain is a string under BOTH families (example 10.8) — clean
    r = NULL;
    ASSERT_EQ(yeptris_validate("---\n", 4, YEPTRIS_SCHEMA_12_CORE, &r), YEPTRIS_OK);
    free_report(r);
    r = NULL;
    ASSERT_EQ(yeptris_validate("---\n", 4, YEPTRIS_SCHEMA_12_JSON, &r), YEPTRIS_OK);
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
    YeptrisStatus st = yeptris_json_schema_validate(schema, strlen(schema), doc, strlen(doc), out);
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
    const yeptris_diagnostic* d = js_first("{\"type\":\"number\",\"minimum\":3}", "2\n", &r);
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

    const yeptris_diagnostic* d3 = js_first("{\"type\":\"number\",\"maximum\":10}", "10.5\n", &r);
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
    const yeptris_diagnostic* d =
        js_first("{\"type\":\"string\",\"minLength\":3}", "\"\xC3\xA9\xC3\xA9\"", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MIN_LENGTH);
    free_report(r);

    ASSERT_EQ(js_first("{\"type\":\"string\",\"maxLength\":2}", "\"\xC3\xA9\xC3\xA9\"", &r),
              nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, PatternMatchesThroughTheBoundedKernel) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"type\":\"string\",\"pattern\":\"^[a-z]+$\"}", "\"abc\"", &r), nullptr);
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
    const yeptris_diagnostic* d =
        js_first("{\"items\":{\"type\":\"integer\"}}", "[1, \"x\", 3]", &r);
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
    const yeptris_diagnostic* d3 = js_first("{\"$ref\":\"#/nowhere\"}", "\"x\"\n", &r);
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

TEST(ValidateJsonSchema, AnchorAndIdResourcesResolve) {
    yeptris_validate_report* r = NULL;
    // a plain-name fragment ($anchor) resolved against the document base
    const char* anchor_schema = "{\"$ref\":\"#foo\","
                                "\"$defs\":{\"A\":{\"$anchor\":\"foo\",\"type\":\"integer\"}}}";
    ASSERT_EQ(js_first(anchor_schema, "1\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(anchor_schema, "\"a\"\n", &r), nullptr);
    free_report(r);

    // $id composes the base: a relative $id rides its nearest enclosing $id,
    // and the pointer fragment walks from THAT resource's root
    const char* id_schema =
        "{\"$id\":\"http://example.com/a.json\","
        "\"$defs\":{\"x\":{\"$id\":\"b/c.json\","
        "\"not\":{\"$defs\":{\"y\":{\"$id\":\"d.json\",\"type\":\"number\"}}}}},"
        "\"allOf\":[{\"$ref\":\"http://example.com/b/d.json\"}]}";
    ASSERT_EQ(js_first(id_schema, "1\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(id_schema, "\"a\"\n", &r), nullptr);
    free_report(r);

    // a relative path reference resolves against the base's directory
    const char* rel_schema = "{\"$id\":\"http://example.com/ref/nested/base.json\","
                             "\"$defs\":{\"num\":{\"$id\":\"num.json\",\"type\":\"number\"}},"
                             "\"$ref\":\"num.json\"}";
    ASSERT_EQ(js_first(rel_schema, "5\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(rel_schema, "\"x\"\n", &r), nullptr);
    free_report(r);

    // an anchor under a DIFFERENT resource is not found from this base
    const char* cross_schema =
        "{\"$id\":\"http://example.com/root.json\","
        "\"$ref\":\"http://example.com/other.json#pt\","
        "\"$defs\":{\"a\":{\"$id\":\"other.json\",\"$anchor\":\"pt\",\"type\":\"string\"}}}";
    ASSERT_EQ(js_first(cross_schema, "\"s\"\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(cross_schema, "1\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, PointerFragmentsDecodeTheUriLayer) {
    yeptris_validate_report* r = NULL;
    // %25 decodes before the tilde layer: "percent%field" is reached by
    // "#/$defs/percent%25field"
    const char* schema = "{\"$defs\":{\"tilde~field\":{\"type\":\"integer\"},"
                         "\"slash/field\":{\"type\":\"integer\"},"
                         "\"percent%field\":{\"type\":\"integer\"}},"
                         "\"properties\":{\"t\":{\"$ref\":\"#/$defs/tilde~0field\"},"
                         "\"s\":{\"$ref\":\"#/$defs/slash~1field\"},"
                         "\"p\":{\"$ref\":\"#/$defs/percent%25field\"}}}";
    ASSERT_EQ(js_first(schema, "{t: 1, s: 2, p: 3}\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(schema, "{t: \"x\"}\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, DynamicRefResolvesTheOutermostScopedAnchor) {
    yeptris_validate_report* r = NULL;
    // the spec appendix C shape, self-contained: tree recurses through
    // $dynamicRef, strict-tree extends it — the recursion must land on
    // the OUTERMOST resource's $dynamicAnchor (strict-tree itself), so
    // unevaluatedProperties guards the children too
    const char* schema = "{\"$id\":\"https://example.com/strict\","
                         "\"$dynamicAnchor\":\"node\","
                         "\"type\":\"object\","
                         "\"properties\":{\"v\":{\"const\":7},"
                         "\"children\":{\"type\":\"array\",\"items\":{\"$dynamicRef\":\"#node\"}}},"
                         "\"$defs\":{"
                         "\"tree\":{\"$id\":\"https://example.com/tree\","
                         "\"$dynamicAnchor\":\"node\","
                         "\"type\":\"string\",\"required\":[\"children\"]}}}";
    ASSERT_EQ(js_first(schema, "{v: 7, children: []}\n", &r), nullptr);
    free_report(r);
    /* the child items face the OUTERMOST resource's anchor (strict's,
     * where the instance must be an object with v == 7): a static
     * fallback would apply tree's anchor, whose type string rejects
     * every child object */
    ASSERT_EQ(js_first(schema, "{children: [{v: 7}]}\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first(schema, "{children: [{v: 7, children: [{v: 7}]}]}\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "{children: [{v: 8}]}\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_CONST);
    free_report(r);
}

TEST(ValidateJsonSchema, DynamicRefScopeIsLeftAndProbesEnterNone) {
    yeptris_validate_report* r = NULL;
    // leaving a dynamic scope: the then-branch's resource scopes the
    // $dynamicRef; the if-probe's resource does NOT (first_scope is
    // entered only as a trial, and trials leave no scope)
    const char* schema =
        "{\"$id\":\"http://example.com/main\","
        "\"if\":{\"$id\":\"first_scope\",\"$defs\":{\"thingy\":{"
        "\"$dynamicAnchor\":\"thingy\",\"type\":\"number\"}}},"
        "\"then\":{\"$id\":\"second_scope\",\"$ref\":\"start\","
        "\"$defs\":{\"thingy\":{\"$dynamicAnchor\":\"thingy\",\"type\":\"null\"}}},"
        "\"$defs\":{"
        "\"start\":{\"$id\":\"start\",\"$dynamicRef\":\"inner_scope#thingy\"},"
        "\"thingy\":{\"$id\":\"inner_scope\",\"$dynamicAnchor\":\"thingy\","
        "\"type\":\"string\"}}}";
    ASSERT_EQ(js_first(schema, "null\n", &r), nullptr); /* second_scope's anchor wins */
    free_report(r);
    ASSERT_NE(js_first(schema, "\"a string\"\n", &r), nullptr);
    free_report(r);
    ASSERT_NE(js_first(schema, "42\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, UnevaluatedPropertiesReadsEveryApplicatorFamily) {
    yeptris_validate_report* r = NULL;
    // adjacent keywords: properties, patternProperties, additionalProperties
    ASSERT_EQ(js_first("{\"properties\":{\"foo\":true},\"unevaluatedProperties\":false}",
                       "{foo: 1}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"properties\":{\"foo\":true},\"unevaluatedProperties\":false}",
                 "{foo: 1, bar: 2}\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    EXPECT_STREQ(d->path, "/bar");
    free_report(r);

    ASSERT_EQ(js_first("{\"patternProperties\":{\"^f\":true},\"unevaluatedProperties\":false}",
                       "{foo: 1}\n", &r),
              nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"properties\":{\"foo\":true},\"additionalProperties\":true,"
                       "\"unevaluatedProperties\":false}",
                       "{foo: 1, bar: 2}\n", &r),
              nullptr);
    free_report(r);
    // additionalProperties false still flags its keys itself
    const yeptris_diagnostic* d2 = js_first(
        "{\"additionalProperties\":false,\"unevaluatedProperties\":false}", "{bar: 2}\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_ADDITIONAL_PROPERTY);
    free_report(r);
}

TEST(ValidateJsonSchema, UnevaluatedAnnotationsFlowThroughApplicators) {
    yeptris_validate_report* r = NULL;
    // allOf branches share the location: the sibling branch's properties count
    ASSERT_EQ(js_first("{\"properties\":{\"foo\":true},\"allOf\":[{\"properties\":{\"bar\":true}}],"
                       "\"unevaluatedProperties\":false}",
                       "{foo: 1, bar: 2}\n", &r),
              nullptr);
    free_report(r);
    // oneOf: only the SINGLE successful branch's annotations flow
    ASSERT_EQ(js_first("{\"properties\":{\"foo\":true},"
                       "\"oneOf\":[{\"properties\":{\"bar\":true},\"required\":[\"bar\"]},"
                       "{\"properties\":{\"baz\":true},\"required\":[\"baz\"]}],"
                       "\"unevaluatedProperties\":false}",
                       "{foo: 1, bar: 2}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"properties\":{\"foo\":true},"
                 "\"oneOf\":[{\"properties\":{\"bar\":true},\"required\":[\"bar\"]},"
                 "{\"properties\":{\"baz\":true},\"required\":[\"baz\"]}],"
                 "\"unevaluatedProperties\":false}",
                 "{foo: 1, bar: 2, quux: 3}\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    free_report(r);
    // anyOf: failed branches annotate nothing (a failed branch's properties
    // must NOT mask the unevaluated flag)
    ASSERT_EQ(
        js_first("{\"properties\":{\"foo\":true},"
                 "\"anyOf\":[{\"properties\":{\"baz\":{\"const\":\"baz\"}},\"required\":[\"baz\"]},"
                 "{\"properties\":{\"bar\":{\"const\":\"bar\"}},\"required\":[\"bar\"]}],"
                 "\"unevaluatedProperties\":false}",
                 "{foo: 1, bar: \"bar\"}\n", &r),
        nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 =
        js_first("{\"properties\":{\"foo\":true},"
                 "\"anyOf\":[{\"properties\":{\"baz\":{\"const\":\"baz\"}},\"required\":[\"baz\"]},"
                 "{\"properties\":{\"bar\":{\"const\":\"bar\"}},\"required\":[\"bar\"]}],"
                 "\"unevaluatedProperties\":false}",
                 "{foo: 1, bar: \"bar\", quux: 3}\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    free_report(r);
    // if/then/else: the taken branch annotates; a FAILED if-probe does not
    ASSERT_EQ(
        js_first(
            "{\"if\":{\"properties\":{\"foo\":{\"const\":\"then\"}},\"required\":[\"foo\"]},"
            "\"then\":{\"properties\":{\"bar\":true}},\"else\":{\"properties\":{\"baz\":true}},"
            "\"unevaluatedProperties\":false}",
            "{foo: \"then\", bar: 1}\n", &r),
        nullptr);
    free_report(r);
    ASSERT_EQ(
        js_first(
            "{\"if\":{\"properties\":{\"foo\":{\"const\":\"then\"}},\"required\":[\"foo\"]},"
            "\"then\":{\"properties\":{\"bar\":true}},\"else\":{\"properties\":{\"baz\":true}},"
            "\"unevaluatedProperties\":false}",
            "{baz: 1}\n", &r),
        nullptr);
    free_report(r);
    const yeptris_diagnostic* d3 =
        js_first("{\"if\":{\"properties\":{\"foo\":{\"const\":\"then\"}},\"required\":[\"foo\"]},"
                 "\"then\":{\"properties\":{\"bar\":true}},\"unevaluatedProperties\":false}",
                 "{foo: \"else\", bar: 1}\n", &r);
    ASSERT_NE(d3, nullptr); /* the failed if's annotation is dropped */
    free_report(r);
    // $ref targets annotate their site's location; a ref creates a NEW
    // scope: the site's sibling annotations do not cross the boundary
    ASSERT_EQ(js_first("{\"$ref\":\"#/$defs/bar\",\"properties\":{\"foo\":true},"
                       "\"unevaluatedProperties\":false,"
                       "\"$defs\":{\"bar\":{\"properties\":{\"bar\":true}}}}",
                       "{foo: 1, bar: 2}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d4 = js_first("{\"$ref\":\"#/$defs/a\",\"properties\":{\"foo\":true},"
                                            "\"unevaluatedProperties\":false,"
                                            "\"$defs\":{\"a\":{\"unevaluatedProperties\":false}}}",
                                            "{foo: 1}\n", &r);
    ASSERT_NE(d4, nullptr);
    EXPECT_EQ(d4->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    free_report(r);
    // dependentSchemas annotate when their trigger key is present
    ASSERT_EQ(js_first("{\"properties\":{\"foo\":true},"
                       "\"dependentSchemas\":{\"foo\":{\"properties\":{\"bar\":true}}},"
                       "\"unevaluatedProperties\":false}",
                       "{foo: 1, bar: 2}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d5 =
        js_first("{\"properties\":{\"foo\":true},"
                 "\"dependentSchemas\":{\"foo\":{\"properties\":{\"bar\":true}}},"
                 "\"unevaluatedProperties\":false}",
                 "{baz: 1}\n", &r);
    ASSERT_NE(d5, nullptr);
    free_report(r);
    // annotations do not cross cousins: the unevaluated keyword only reads
    // ITS OWN schema object's collected set
    const yeptris_diagnostic* d6 =
        js_first("{\"allOf\":[{\"properties\":{\"foo\":true}},{\"unevaluatedProperties\":false}]}",
                 "{foo: 1}\n", &r);
    ASSERT_NE(d6, nullptr);
    free_report(r);
    // a single cyclic ref composes: the recursion consumes instance
    // structure, so the guard never fires
    ASSERT_EQ(js_first("{\"properties\":{\"x\":{\"$ref\":\"#\"}},\"unevaluatedProperties\":false}",
                       "{x: {x: {}}}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d7 =
        js_first("{\"properties\":{\"x\":{\"$ref\":\"#\"}},\"unevaluatedProperties\":false}",
                 "{x: {x: {}, y: {}}}\n", &r);
    ASSERT_NE(d7, nullptr);
    EXPECT_EQ(d7->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    free_report(r);
    // `not` collects internally (its unevaluated* sees the sibling anyOf
    // branch's annotations) but never annotates outward
    ASSERT_EQ(js_first("{\"not\":{\"anyOf\":[true,{\"properties\":{\"foo\":true}}],"
                       "\"unevaluatedProperties\":false}}",
                       "{bar: 1}\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d8 =
        js_first("{\"not\":{\"anyOf\":[true,{\"properties\":{\"foo\":true}}],"
                 "\"unevaluatedProperties\":false}}",
                 "{foo: 1}\n", &r);
    ASSERT_NE(d8, nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, UnevaluatedItemsReadsItemsAndContains) {
    yeptris_validate_report* r = NULL;
    // prefixItems + items evaluate their indices
    ASSERT_EQ(
        js_first(
            "{\"prefixItems\":[{\"type\":\"string\"}],\"items\":true,\"unevaluatedItems\":false}",
            "[\"foo\", 42]\n", &r),
        nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"prefixItems\":[{\"type\":\"string\"}],\"unevaluatedItems\":false}",
                 "[\"foo\", 42]\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_ITEM);
    EXPECT_STREQ(d->path, "/1");
    free_report(r);
    // contains annotates the MATCHING indices while it holds; with
    // minContains 0 and no matches it annotates nothing
    ASSERT_EQ(js_first("{\"prefixItems\":[true],\"contains\":{\"type\":\"string\"},"
                       "\"unevaluatedItems\":false}",
                       "[1, \"foo\"]\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 =
        js_first("{\"prefixItems\":[true],\"contains\":{\"type\":\"string\"},"
                 "\"unevaluatedItems\":false}",
                 "[1, 2, \"foo\"]\n", &r);
    ASSERT_NE(d2, nullptr);
    free_report(r);
    // items evaluates what it applied: unevaluatedItems never runs
    ASSERT_EQ(
        js_first("{\"items\":{\"type\":\"string\"},\"unevaluatedItems\":{\"type\":\"boolean\"}}",
                 "[\"a\", \"b\"]\n", &r),
        nullptr);
    free_report(r);
    const yeptris_diagnostic* d3 =
        js_first("{\"items\":{\"type\":\"string\"},\"unevaluatedItems\":{\"type\":\"boolean\"}}",
                 "[\"a\", 5]\n", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    free_report(r);
    // annotations do not cross cousins
    const yeptris_diagnostic* d4 = js_first(
        "{\"allOf\":[{\"prefixItems\":[true]},{\"unevaluatedItems\":false}]}", "[1]\n", &r);
    ASSERT_NE(d4, nullptr);
    free_report(r);
    // instance-location discipline: an annotation made at /0/1 is not an
    // annotation at /1
    const yeptris_diagnostic* d5 =
        js_first("{\"prefixItems\":[{\"prefixItems\":[true,{\"type\":\"string\"}]}],"
                 "\"unevaluatedItems\":false}",
                 "[[\"foo\", \"bar\"], \"bar\"]\n", &r);
    ASSERT_NE(d5, nullptr);
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

// ---- the 2020-12 sweep (#885 item 2): the suite-pinned keywords ----------

TEST(ValidateJsonSchema, MultipleOfIsLibmFreeQuotientIntegrality) {
    yeptris_validate_report* r = NULL;
    // integer and float multiples ride the same quotient test
    ASSERT_EQ(js_first("{\"multipleOf\":2}", "10\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"multipleOf\":1.5}", "4.5\n", &r), nullptr);
    free_report(r);
    // the suite's float pair: 0.00751/0.0001 is not integral
    const yeptris_diagnostic* d = js_first("{\"multipleOf\":0.0001}", "0.00751\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MULTIPLE_OF);
    free_report(r);
    ASSERT_EQ(js_first("{\"multipleOf\":0.0001}", "0.0075\n", &r), nullptr);
    free_report(r);
    // a quotient of +inf fails, never crashes (the suite's 1e308/1e-308)
    const yeptris_diagnostic* dinf = js_first("{\"multipleOf\":1e-308}", "1e308\n", &r);
    ASSERT_NE(dinf, nullptr);
    EXPECT_EQ(dinf->code, (uint32_t)YEPTRIS_DIAG_MULTIPLE_OF);
    free_report(r);
    // non-numeric instances ignore multipleOf; unreadable values too
    ASSERT_EQ(js_first("{\"multipleOf\":2}", "text\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"multipleOf\":\"x\"}", "3\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, MinMaxPropertiesCountPairs) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"minProperties\":1,\"maxProperties\":2}", "a: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first("{\"minProperties\":2}", "a: 1\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MIN_PROPERTIES);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first("{\"maxProperties\":1}", "a: 1\nb: 2\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_MAX_PROPERTIES);
    free_report(r);
    // non-objects ignore both
    ASSERT_EQ(js_first("{\"minProperties\":2,\"maxProperties\":1}", "[1]\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, PatternPropertiesApplyByKeyName) {
    yeptris_validate_report* r = NULL;
    // the kernel runs over the KEY bytes; matching keys walk their value
    const char* schema = "{\"patternProperties\":{\"^a\":{\"type\":\"integer\"}}}";
    ASSERT_EQ(js_first(schema, "apple: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "apple: text\nother: text\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d->path, "/apple");
    free_report(r);
    // a false patternProperty schema fails matched keys only
    const yeptris_diagnostic* d2 =
        js_first("{\"patternProperties\":{\"^v\":false}}", "vroom: 1\nother: 2\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_FALSE_SCHEMA);
    EXPECT_STREQ(d2->path, "/vroom");
    free_report(r);
    // additionalProperties does not see patternProperties matches (2020-12)
    const char* both = "{\"properties\":{\"foo\":{}},\"patternProperties\":{\"^v\":{}},"
                       "\"additionalProperties\":false}";
    ASSERT_EQ(js_first(both, "foo: 1\nvroom: 2\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d3 = js_first(both, "foo: 1\nquux: 2\n", &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_ADDITIONAL_PROPERTY);
    free_report(r);
}

TEST(ValidateJsonSchema, PropertyNamesAppliesToTheKeyStrings) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"propertyNames\":{\"maxLength\":3}}", "abc: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"propertyNames\":{\"maxLength\":3}}", "abcd: 1\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->path, "/abcd");
    free_report(r);
    // a false propertyNames rejects every key; empty objects ride
    ASSERT_EQ(js_first("{\"propertyNames\":false}", "{}\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"propertyNames\":true}", "anything: 1\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, ContainsCountsHeldItems) {
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"contains\":{\"type\":\"integer\"}}", "[text, 1]\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d =
        js_first("{\"contains\":{\"type\":\"integer\"}}", "[text, more]\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_CONTAINS);
    free_report(r);
    // minContains/maxContains bound the match count; minContains 0 holds
    // on arrays the contains schema never matches
    ASSERT_EQ(js_first("{\"contains\":{\"type\":\"integer\"},\"minContains\":2}", "[1, 2]\n", &r),
              nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"contains\":{\"type\":\"integer\"},\"minContains\":0}", "[text]\n", &r),
              nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first("{\"contains\":{\"type\":\"integer\"},"
                                            "\"maxContains\":1}",
                                            "[1, 2]\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_MAX_CONTAINS);
    free_report(r);
    // contains on a non-array is ignored
    ASSERT_EQ(js_first("{\"contains\":{\"type\":\"integer\"}}", "text\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, DependentRequiredAndDependentSchemas) {
    yeptris_validate_report* r = NULL;
    const char* dr = "{\"dependentRequired\":{\"credit_card\":[\"billing_details\"]}}";
    ASSERT_EQ(js_first(dr, "credit_card: 1\nbilling_details: x\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first(dr, "name: bob\n", &r), nullptr); // the trigger absent
    free_report(r);
    const yeptris_diagnostic* d = js_first(dr, "credit_card: 1\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_DEPENDENT_REQUIRED);
    EXPECT_STREQ(d->path, "/billing_details"); /* the missing leaf */
    free_report(r);

    // dependentSchemas applies the schema when the key is present
    const char* ds = "{\"dependentSchemas\":{\"credit_card\":{\"required\":"
                     "[\"billing_details\"]}}}";
    ASSERT_EQ(js_first(ds, "credit_card: 1\nbilling_details: x\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first(ds, "name: bob\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first(ds, "credit_card: 1\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_REQUIRED);
    free_report(r);
}

TEST(ValidateJsonSchema, IfThenElseRidesASuppressedProbe) {
    yeptris_validate_report* r = NULL;
    const char* schema = "{\"if\":{\"properties\":{\"kind\":{\"const\":\"int\"}}},"
                         "\"then\":{\"required\":[\"value\"]},"
                         "\"else\":{\"required\":[\"other\"]}}";
    ASSERT_EQ(js_first(schema, "kind: int\nvalue: 1\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first(schema, "kind: str\nother: 1\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "kind: int\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_REQUIRED); /* then fired */
    EXPECT_STREQ(d->path, "/value");
    free_report(r);
    const yeptris_diagnostic* d2 = js_first(schema, "kind: str\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_REQUIRED); /* else fired */
    EXPECT_STREQ(d2->path, "/other");
    free_report(r);
    // if without then/else constrains nothing; an unreadable if is ignored
    ASSERT_EQ(js_first("{\"if\":{\"type\":\"integer\"}}", "text\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"if\":\"x\"}", "text\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, UniqueItemsIgnoresKeyOrder) {
    // the suite caught the order-dependent mapping hash: canonical form
    // of a mapping is its CONTENT, never the pair order
    yeptris_validate_report* r = NULL;
    const char* schema = "{\"uniqueItems\":true}";
    const yeptris_diagnostic* d =
        js_first(schema, "- {\"a\": 1, \"b\": 2}\n- {\"b\": 2, \"a\": 1}\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNIQUE_ITEMS);
    free_report(r);
    ASSERT_EQ(js_first(schema, "- {\"a\": 1, \"b\": 2}\n- {\"b\": 3, \"a\": 1}\n", &r), nullptr);
    free_report(r);
}

TEST(ValidateJsonSchema, ConstEnumCompareNumbersAcrossIntAndFloatTags) {
    // the suite caught the tag-strict equality: 2020-12 compares NUMBERS
    // by value — 0.0 is the constant 0; 1.0 is not the constant true
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first("{\"const\":0}", "0.0\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"const\":1.0}", "1\n", &r), nullptr);
    free_report(r);
    ASSERT_EQ(js_first("{\"const\":-2.0}", "-2\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first("{\"const\":1}", "true\n", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_CONST);
    free_report(r);
    // enum rides the same canonical equality, element-wise
    ASSERT_EQ(js_first("{\"enum\":[3, 4.5, \"x\"]}", "4.5\n", &r), nullptr);
    free_report(r);
    // the suite's [0]-vs-[0.0] shape: the member [1] equals [1.0] ...
    ASSERT_EQ(js_first("{\"enum\":[[1]]}", "[1.0]\n", &r), nullptr);
    free_report(r);
    // ... and the equality descends: [[1]] is a member of [[[1]]]
    ASSERT_EQ(js_first("{\"enum\":[[[1]]]}", "[[1.0]]\n", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d2 = js_first("{\"enum\":[[1]]}", "[[true]]\n", &r);
    ASSERT_NE(d2, nullptr);
    EXPECT_EQ(d2->code, (uint32_t)YEPTRIS_DIAG_ENUM);
    free_report(r);
    // duplicate keys ride the same canonical form: 1 and 1.0 are ONE key
    const yeptris_diagnostic* d3 = validate_first("a: 1\n\"a\": 1.0\n", YEPTRIS_SCHEMA_12_CORE, &r);
    ASSERT_NE(d3, nullptr);
    EXPECT_EQ(d3->code, (uint32_t)YEPTRIS_DIAG_DUPLICATE_KEY);
    free_report(r);
    // the JSON family keeps a plain int key a JSON production; the
    // quoted twin is still the same canonical key
    ASSERT_EQ(js_first("{\"type\":\"object\"}", "a: 1\n\"a\": 2\n", &r), nullptr)
        << "no key-stringness noise expected for JSON-production keys";
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
    // ECMA-262 \cX control escapes (X & 0x1F); a non-letter after \c is
    // the Annex B identity escape
    EXPECT_EQ(yep_re_search("^\\cC$", (uint32_t)strlen("^\\cC$"), "\x03", 1), 1);
    EXPECT_EQ(yep_re_search("^\\cc$", (uint32_t)strlen("^\\cc$"), "\x03", 1), 1);
    EXPECT_EQ(yep_re_search("^\\cC$", (uint32_t)strlen("^\\cC$"), "x", 1), 0);
    EXPECT_EQ(yep_re_search("^[\\cA]$", (uint32_t)strlen("^[\\cA]$"), "\x01", 1), 1);
    // the code-point classes: a class consumes a WHOLE UTF-8 code point,
    // so an anchor can land after one (the suite's ecmascript-regex
    // section passes 74/74 on these)
    EXPECT_EQ(yep_re_search("^\\D$", (uint32_t)strlen("^\\D$"), "\xDF\x80", 2),
              1); // NKO DIGIT ZERO
    EXPECT_EQ(yep_re_search("^\\W$", (uint32_t)strlen("^\\W$"), "\xC3\xA9", 2), 1);     // e-acute
    EXPECT_EQ(yep_re_search("^\\s$", (uint32_t)strlen("^\\s$"), "\xC2\xA0", 2), 1);     // NBSP
    EXPECT_EQ(yep_re_search("^\\s$", (uint32_t)strlen("^\\s$"), "\xE2\x80\x83", 3), 1); // EM SPACE
    EXPECT_EQ(yep_re_search("^\\s$", (uint32_t)strlen("^\\s$"), "\xEF\xBB\xBF", 3), 1); // ZWNBSP
    EXPECT_EQ(yep_re_search("^\\S$", (uint32_t)strlen("^\\S$"), "\xE2\x80\x83", 3), 0);
    EXPECT_EQ(yep_re_search("^\\S$", (uint32_t)strlen("^\\S$"), "\xE2\x80\x93", 3), 1); // en dash
    EXPECT_EQ(yep_re_search("^\\d$", (uint32_t)strlen("^\\d$"), "\xE0\xA7\xA6", 3),
              0); // Bengali 0: \d is ASCII
    // the property escapes (exact canonical spellings; any other \p{...}
    // name stays the Annex B identity escape)
    EXPECT_EQ(yep_re_search("\\p{Letter}cole", (uint32_t)strlen("\\p{Letter}cole"),
                            "l'\xC3\xA9"
                            "cole",
                            9),
              1);
    EXPECT_EQ(yep_re_search("^\\p{digit}+$", (uint32_t)strlen("^\\p{digit}+$"),
                            "\xE0\xA7\xAA\xE0\xA7\xA8", 6),
              1); // Bengali 4 2
    EXPECT_EQ(yep_re_search("^\\p{digit}+$", (uint32_t)strlen("^\\p{digit}+$"), "-%#", 3), 0);
    EXPECT_EQ(yep_re_search("\\p{Digit}", (uint32_t)strlen("\\p{Digit}"), "p{Digit}", 8), 1);
    EXPECT_EQ(yep_re_search("\\p{Digit}", (uint32_t)strlen("\\p{Digit}"), "123", 3), 0);
    // a negated bracket and the dot over a multi-byte code point
    EXPECT_EQ(yep_re_search("^[^a]$", (uint32_t)strlen("^[^a]$"), "\xC3\xA9", 2), 1);
    EXPECT_EQ(yep_re_search("^a.c$", (uint32_t)strlen("^a.c$"),
                            "a\xC3\xA9"
                            "c",
                            4),
              1);
    EXPECT_EQ(yep_re_search("^[a-z]$", (uint32_t)strlen("^[a-z]$"), "\xC3\xA9", 2), 0);
    // the NFA is linear: the classic blowup pattern answers instantly
    const char* blowup = "aaaaaaaaaaaaaaaaaaaaaaaaaaaab";
    EXPECT_EQ(yep_re_search("(a+)+$", (uint32_t)strlen("(a+)+$"), blowup, (uint32_t)strlen(blowup)),
              0);
    // SyntaxErrors
    EXPECT_EQ(yep_re_search("(a", (uint32_t)strlen("(a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("[a", (uint32_t)strlen("[a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("*a", (uint32_t)strlen("*a"), "a", 1), -1);
    EXPECT_EQ(yep_re_search("(a)\\1", (uint32_t)strlen("(a)\\1"), "aa", 2), -1);
    EXPECT_EQ(yep_re_search("", (uint32_t)strlen(""), "a", 1), -1);
    // non-unicode ECMA-262 (Annex B): unknown LETTER escapes are identity
    // escapes — \p without the u flag is the literal 'p' (pattern.json's
    // Unicode-property-escape group rides this); digits stay SyntaxError
    EXPECT_EQ(yep_re_search("a\\q", (uint32_t)strlen("a\\q"), "aq", 2), 1);
    EXPECT_EQ(yep_re_search("a\\q", (uint32_t)strlen("a\\q"), "a", 1), 0);
    EXPECT_EQ(yep_re_search("\\p{Digit}", (uint32_t)strlen("\\p{Digit}"), "p{Digit}", 8), 1);
    EXPECT_EQ(yep_re_search("\\p{Digit}", (uint32_t)strlen("\\p{Digit}"), "123", 3), 0);
}

// ---- the compiled-schema economy (#885 item 3) ---------------------------
//
// The handle compiles ONCE (schema DOM + the pattern kernel's programs)
// and validates many streams; the verdicts must equal the one-shot
// face's byte for byte (the suite port's 1298 cases are the global
// referee — these pins ride the same walk through both faces).

namespace {

// Runs BOTH faces over (schema, doc) and pins them together: status,
// diagnostic count, and the first diagnostic's code/path/detail.
void pin_faces(const char* schema, const char* doc) {
    yeptris_schema* s = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s, nullptr) << "the test schema must compile";
    yeptris_validate_report* r1 = NULL;
    yeptris_validate_report* r2 = NULL;
    YeptrisStatus st1 = yeptris_schema_validate(s, doc, strlen(doc), &r1);
    YeptrisStatus st2 = yeptris_json_schema_validate(schema, strlen(schema), doc, strlen(doc), &r2);
    ASSERT_EQ(st1, st2) << "the compiled face shares the one-shot's statuses";
    ASSERT_EQ(yeptris_validate_report_count(r1), yeptris_validate_report_count(r2));
    const yeptris_diagnostic* d1 = yeptris_validate_report_at(r1, 0);
    const yeptris_diagnostic* d2 = yeptris_validate_report_at(r2, 0);
    if (d1 == nullptr) {
        EXPECT_EQ(d2, nullptr);
    } else {
        ASSERT_NE(d2, nullptr);
        EXPECT_EQ(d1->code, d2->code);
        EXPECT_STREQ(d1->path, d2->path);
        EXPECT_STREQ(d1->detail, d2->detail);
    }
    yeptris_validate_report_free(r1);
    yeptris_validate_report_free(r2);
    yeptris_schema_free(s);
}

} // namespace

// ---- the IETF WG draft's own examples (issue #906 slice 2) ---------------
//
// draft-ietf-jsonschema-json-schema-00 carries the 2020-12 semantics with
// NO keyword-level deltas (docs/schema/ietf-wg-draft-delta.md), so these
// pins guard the draft's flagship examples — the machinery the WG text
// showcases, verbatim — rather than any behavior change.

TEST(ValidateJsonSchemaWgDraft, RefsExamplePositiveInteger) {
    // WG §4.2.2 "$defs": the canonical reference example, verbatim
    const char* schema = "{\"type\":\"array\","
                         "\"items\":{\"$ref\":\"#/$defs/positiveInteger\"},"
                         "\"$defs\":{\"positiveInteger\":"
                         "{\"type\":\"integer\",\"exclusiveMinimum\":0}}}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "[1, 2, 3]", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "[1, -2]", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_EXCLUSIVE_MINIMUM);
    EXPECT_STREQ(d->path, "/1");
    free_report(r);
}

TEST(ValidateJsonSchemaWgDraft, DereferencingExampleRootJson) {
    // WG §11.2: $id base composition + the "#item" plain-name fragment
    // resolve within the document; "other.json" resolves OUTSIDE it, and
    // the WG's own walkthrough ("which is not defined in this document")
    // is our no-fetch boundary: DIAG_REF at the property the
    // additionalProperties schema applies to
    const char* schema = "{\"$id\":\"https://example.net/root.json\","
                         "\"items\":{\"type\":\"array\",\"items\":{\"$ref\":\"#item\"}},"
                         "\"$defs\":{\"single\":{\"$anchor\":\"item\",\"type\":\"object\","
                         "\"additionalProperties\":{\"$ref\":\"other.json\"}}}}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "[[{}]]", &r), nullptr);
    free_report(r);
    const yeptris_diagnostic* d = js_first(schema, "[[{\"a\": 1}]]", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_REF);
    EXPECT_STREQ(d->path, "/0/0/a");
    free_report(r);
}

TEST(ValidateJsonSchemaWgDraft, AppendixCTreeStrictTreeDynamicScope) {
    // WG Appendix C verbatim, bundled the only way the face can hold two
    // resources (tree rides under $defs with its $id; "tree" against the
    // strict-tree base resolves to https://example.com/tree). The
    // misspelled "daat" must trip strict-tree's unevaluatedProperties —
    // so the $dynamicRef recursion inside tree's children must land on
    // the OUTERMOST resource's anchor (strict-tree), not tree's own.
    // The upstream suite pins this example only through its remote
    // corpus (the carried strict-tree group), so this is the pin.
    const char* schema = "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
                         "\"$id\":\"https://example.com/strict-tree\","
                         "\"$dynamicAnchor\":\"node\","
                         "\"$ref\":\"tree\","
                         "\"unevaluatedProperties\":false,"
                         "\"$defs\":{\"tree\":{"
                         "\"$id\":\"https://example.com/tree\","
                         "\"$dynamicAnchor\":\"node\","
                         "\"type\":\"object\","
                         "\"properties\":{\"data\":true,"
                         "\"children\":{\"type\":\"array\","
                         "\"items\":{\"$dynamicRef\":\"#node\"}}}}}}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "{children: [{data: 1}]}", &r), nullptr);
    free_report(r);
    // the misspelled field, one level deep
    const yeptris_diagnostic* d = js_first(schema, "{children: [{daat: 1}]}", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    EXPECT_STREQ(d->path, "/children/0/daat");
    free_report(r);
    // the recursion still guards THROUGH the dynamic reference
    d = js_first(schema, "{children: [{children: [{daat: 1}]}]}", &r);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_UNEVALUATED_PROPERTY);
    EXPECT_STREQ(d->path, "/children/0/children/0/daat");
    free_report(r);
    // and the compiled face's verdicts ride the same walk
    pin_faces(schema, "{children: [{data: 1}]}");
    pin_faces(schema, "{children: [{daat: 1}]}");
}

TEST(ValidateJsonSchemaWgDraft, ReadOnlyWriteOnlyAnnotationsNeverAssert) {
    // WG §10.4.1's example: readOnly/writeOnly are annotations — the
    // example instance validates (the WG's output units for it are the
    // slice-5 output face, not an assertion)
    const char* schema = "{\"$id\":\"https://example.com/schema\",\"type\":\"object\","
                         "\"properties\":{\"username\":{\"type\":\"string\",\"readOnly\":true},"
                         "\"password\":{\"type\":\"string\",\"writeOnly\":true}}}";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(js_first(schema, "{username: xyz, password: \"123\"}", &r), nullptr);
    free_report(r);
}

TEST(ValidateCompiledSchema, CompileOnceValidateMany) {
    // the pattern kernel is the XSD content-model analog: the schema's
    // four patterns compile at handle-compile time, then every stream
    // reuses them
    const char* schema =
        "{\"definitions\":{\"code\":{\"type\":\"string\",\"pattern\":\"^C-[0-9]{3}$\"}},"
        "\"type\":\"object\","
        "\"required\":[\"name\"],"
        "\"properties\":{\"name\":{\"type\":\"string\",\"pattern\":\"^[a-z]+\"},"
        "\"code\":{\"$ref\":\"#/definitions/code\"}},"
        "\"patternProperties\":{\"^x-\":{\"maximum\":10}}}";
    yeptris_schema* s = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s, nullptr);

    yeptris_validate_report* r = NULL;
    // 1: valid
    ASSERT_EQ(yeptris_schema_validate(s, "{\"name\":\"abc\",\"code\":\"C-123\"}\n",
                                      strlen("{\"name\":\"abc\",\"code\":\"C-123\"}\n"), &r),
              YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    yeptris_validate_report_free(r);

    // 2: the plain pattern misses
    ASSERT_EQ(
        yeptris_schema_validate(s, "{\"name\":\"Abc\"}\n", strlen("{\"name\":\"Abc\"}\n"), &r),
        YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    const yeptris_diagnostic* d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_PATTERN);
    EXPECT_STREQ(d->path, "/name");
    yeptris_validate_report_free(r);

    // 3: the $ref'd pattern misses (a string satisfies type; the
    // ref'd pattern rejects)
    ASSERT_EQ(yeptris_schema_validate(s, "{\"name\":\"ok\",\"code\":\"X-99\"}\n",
                                      strlen("{\"name\":\"ok\",\"code\":\"X-99\"}\n"), &r),
              YEPTRIS_ERROR_SCHEMA);
    d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_PATTERN);
    EXPECT_STREQ(d->path, "/code");
    yeptris_validate_report_free(r);

    // 4: the patternProperties schema holds
    ASSERT_EQ(yeptris_schema_validate(s, "{\"name\":\"ok\",\"x-thing\":5}\n",
                                      strlen("{\"name\":\"ok\",\"x-thing\":5}\n"), &r),
              YEPTRIS_OK);
    yeptris_validate_report_free(r);

    // 5: and misses over 10
    ASSERT_EQ(yeptris_schema_validate(s, "{\"name\":\"ok\",\"x-thing\":11}\n",
                                      strlen("{\"name\":\"ok\",\"x-thing\":11}\n"), &r),
              YEPTRIS_ERROR_SCHEMA);
    d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_MAXIMUM);
    EXPECT_STREQ(d->path, "/x-thing");
    yeptris_validate_report_free(r);

    // 6: a required miss rides the same handle untouched
    ASSERT_EQ(
        yeptris_schema_validate(s, "{\"code\":\"C-001\"}\n", strlen("{\"code\":\"C-001\"}\n"), &r),
        YEPTRIS_ERROR_SCHEMA);
    d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_REQUIRED);
    yeptris_validate_report_free(r);

    // 7: a multi-document stream — diagnostics across both docs of
    // one validate call, the miss attributed to document 1
    ASSERT_EQ(yeptris_schema_validate(s, "{\"name\":\"ok\"}\n---\n{\"name\":\"Nope\"}\n",
                                      strlen("{\"name\":\"ok\"}\n---\n{\"name\":\"Nope\"}\n"), &r),
              YEPTRIS_ERROR_SCHEMA);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_PATTERN);
    EXPECT_STREQ(d->path, "/1/name");
    yeptris_validate_report_free(r);

    // 8: an empty instance stream is valid
    ASSERT_EQ(yeptris_schema_validate(s, "", 0, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    yeptris_validate_report_free(r);

    yeptris_schema_free(s);
}

TEST(ValidateCompiledSchema, StreamDiagnosticsCarryTheDocumentIndex) {
    // the compiled stream face validates EVERY document in turn
    // (compile once / validate many) and attributes each miss: docs 1
    // and 3 are clean, doc 2 is named by the path prefix, and one
    // stream-wide report comes back
    const char* schema = "{\"type\":\"object\","
                         "\"required\":[\"name\"],"
                         "\"properties\":{\"name\":{\"type\":\"string\"}}}";
    yeptris_schema* s = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s, nullptr);
    const char* stream = "{\"name\":\"a\"}\n---\nname: 7\n---\n{\"name\":\"c\"}\n";
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_schema_validate(s, stream, strlen(stream), &r), YEPTRIS_ERROR_SCHEMA);
    ASSERT_NE(r, nullptr);
    ASSERT_EQ(yeptris_validate_report_count(r), (size_t)1);
    const yeptris_diagnostic* d = yeptris_validate_report_at(r, 0);
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_TYPE);
    EXPECT_STREQ(d->path, "/1/name");
    EXPECT_EQ(d->line, (uint32_t)3); /* input-absolute, the stream's line 3 */
    yeptris_validate_report_free(r);
    yeptris_schema_free(s);
}

TEST(ValidateCompiledSchema, WrongSchemaRejectedIdenticallyToOneShot) {
    // the compiled face and the one-shot face must agree byte for byte
    // across shapes: patterns, bounds, refs, booleans, oneOf
    pin_faces("{\"type\":\"object\",\"properties\":{\"a\":{\"type\":\"integer\"}}}",
              "{\"a\": \"str\"}\n");
    pin_faces("{\"properties\":{\"a\":{\"pattern\":\"^p\"},\"b\":{\"pattern\":\"^q\"}}}",
              "{\"a\": \"xp\", \"b\": \"yq\"}\n");
    pin_faces("{\"patternProperties\":{\"^[0-9]\":{\"minLength\":3}}}", "12: abc\n");
    pin_faces("{\"$defs\":{\"n\":{\"type\":\"number\"}},\"allOf\":[{\"$ref\":\"#/$defs/n\"},"
              "{\"maximum\":5}]}",
              "7\n");
    pin_faces("false", "1\n");
    pin_faces("{\"oneOf\":[{\"type\":\"string\"},{\"type\":\"integer\"}]}", "true\n");
    pin_faces("{\"type\":\"array\",\"items\":{\"enum\":[1,2]}}", "- 1\n- 3\n- 2\n");
    // the schema bytes hold no document / do not parse: same NULL, and
    // the one-shot's exact statuses stay reachable through the faces
    EXPECT_EQ(yeptris_schema_compile("", 0), nullptr);
    EXPECT_EQ(yeptris_schema_compile(NULL, 5), nullptr);
    yeptris_validate_report* r = NULL;
    EXPECT_EQ(yeptris_json_schema_validate("", 0, "1\n", 2, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_json_schema_validate("{unbalanced", 11, "1\n", 2, &r), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(r, nullptr);
}

TEST(ValidateCompiledSchema, InvalidPatternIsIgnoredLikeOneShot) {
    // a pattern outside the subset is not retained: every use
    // recompiles to -1 and the keyword is ignored — both faces alike
    pin_faces("{\"properties\":{\"a\":{\"pattern\":\"(a\"}}}", "{\"a\": \"x\"}\n");
    pin_faces("{\"patternProperties\":{\"[0\":{\"type\":\"integer\"}}}", "{\"[0\": \"s\"}\n");
    pin_faces("{\"properties\":{\"a\":{\"pattern\":\"\"}}}", "{\"a\": \"x\"}\n");
}

TEST(ValidateCompiledSchema, ManyPatternsSharePrograms) {
    // ten pattern nodes over one handle; verdicts identical to the
    // one-shot for documents that trip each of them
    char buf[512];
    snprintf(buf, sizeof(buf), "%s", "{\"properties\":{");
    for (int i = 0; i < 10; i++) {
        char part[64];
        snprintf(part, sizeof(part), "%s\"k%d\":{\"pattern\":\"^p%d$\"}", i ? "," : "", i, i);
        strncat(buf, part, sizeof(buf) - strlen(buf) - 1);
    }
    strncat(buf, "}}", sizeof(buf) - strlen(buf) - 1);
    yeptris_schema* s = yeptris_schema_compile(buf, strlen(buf));
    ASSERT_NE(s, nullptr);
    yeptris_validate_report* r = NULL;
    // a document tripping ALL ten at once: the report enumerates the
    // misses on one walk of one stream
    char doc[256];
    snprintf(doc, sizeof(doc), "%s", "{");
    for (int i = 0; i < 10; i++) {
        char part[32];
        snprintf(part, sizeof(part), "%s\"k%d\":\"z\"", i ? "," : "", i);
        strncat(doc, part, sizeof(doc) - strlen(doc) - 1);
    }
    strncat(doc, "}\n", sizeof(doc) - strlen(doc) - 1);
    ASSERT_EQ(yeptris_schema_validate(s, doc, strlen(doc), &r), YEPTRIS_ERROR_SCHEMA);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)10);
    for (size_t i = 0; i < 10; i++) {
        const yeptris_diagnostic* d = yeptris_validate_report_at(r, i);
        ASSERT_NE(d, nullptr);
        EXPECT_EQ(d->code, (uint32_t)YEPTRIS_DIAG_PATTERN);
    }
    yeptris_validate_report_free(r);
    yeptris_schema_free(s);
}

TEST(ValidateCompiledSchema, HandleGuards) {
    yeptris_validate_report* r = NULL;
    // argument gates: the same shapes as the one-shot's
    EXPECT_EQ(yeptris_schema_validate(NULL, "1", 1, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(r, nullptr);
    yeptris_schema* s = yeptris_schema_compile("true", 4);
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(yeptris_schema_validate(s, "1", 1, NULL), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(yeptris_schema_validate(s, NULL, 3, &r), YEPTRIS_ERROR_ARG);
    EXPECT_EQ(r, nullptr);
    // the (NULL, 0) shapes are legal empties: the parse reads them
    // through a non-null base (the macOS CI catch), so compile reads
    // no-document and validate reads an empty valid stream
    EXPECT_EQ(yeptris_schema_compile(NULL, 0), nullptr); /* empty schema: no document */
    ASSERT_EQ(yeptris_schema_validate(s, NULL, 0, &r), YEPTRIS_OK);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(yeptris_validate_report_count(r), (size_t)0);
    yeptris_validate_report_free(r);
    yeptris_schema_free(s);
    // NULL free is a no-op, like documents
    yeptris_schema_free(NULL);
}

TEST(ValidateCompiledSchema, FreedHandleContract) {
    // the documented contract: free then NO use (the handle is dead);
    // a fresh handle from the same bytes works and frees cleanly even
    // when never validated
    const char* schema = "{\"type\":\"integer\"}";
    yeptris_schema* s = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s, nullptr);
    yeptris_validate_report* r = NULL;
    ASSERT_EQ(yeptris_schema_validate(s, "5\n", 2, &r), YEPTRIS_OK);
    yeptris_validate_report_free(r);
    yeptris_schema_free(s);
    // an unvalidated handle frees clean
    yeptris_schema* s2 = yeptris_schema_compile(schema, strlen(schema));
    ASSERT_NE(s2, nullptr);
    yeptris_schema_free(s2);
}
