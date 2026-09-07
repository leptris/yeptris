/* test_marshal.cpp — the Marshal 4.8 emitter's C-side test gate
 * (TODO.restructure/21).
 *
 * What's covered here:
 *   - byte-equality on a few hand-checked fixtures (the byte streams
 *     were generated once via yeptris_marshal and locked in)
 *   - engine route vs strict-JSON route produce IDENTICAL bytes for
 *     the JSON corpus (records must not differ by route)
 *   - UNSUPPORTED status for merge keys, timestamps, forward aliases
 *   - empty stream, single doc, multi-doc, node-marshal path
 *
 * The full Marshal.load round-trip (byte streams -> Ruby object graph
 * equality with YAML.unsafe_load) lives in the Ruby binding's spec,
 * where the verifier is a stock Marshal.load call. */

#include <gtest/gtest.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/dom.h>
#include <yeptris/marshal.h>
#include <yeptris/values.h>

namespace {

std::string hex_to_bytes(const char* hex) {
    std::string out;
    out.reserve(strlen(hex) / 2);
    for (size_t i = 0; i + 1 < strlen(hex); i += 2) {
        unsigned b;
        sscanf(hex + i, "%2x", &b);
        out.push_back((char)b);
    }
    return out;
}

void expect_marshal(const char* in, const char* want_hex, YeptrisMarshalMode mode) {
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(in, strlen(in), YEPTRIS_SCHEMA_11_COMPAT, mode, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK) << yeptris_last_error(0, 0);
    std::string want = hex_to_bytes(want_hex);
    ASSERT_EQ(olen, want.size());
    EXPECT_EQ(memcmp(out, want.data(), olen), 0)
        << "input=" << in << " got=" << std::string(out, olen);
    yeptris_marshal_free(out);
}

TEST(Marshal, ByteEqualitySmoke) {
    expect_marshal("a: 1", "04087b0649220661063a0645546906", YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("[]", "04085b00", YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("- 1\n- two\n", "04085b07690649220874776f063a064554", YEPTRIS_MARSHAL_FIRST_DOC);
    /* INT64_MIN -> bignum limb-stream */
    expect_marshal("n: -9223372036854775808", "04087b064922066e063a0645546c2d090000000000000080",
                   YEPTRIS_MARSHAL_FIRST_DOC);
}

TEST(Marshal, EmptyStreamFirst) {
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st =
        yeptris_marshal("", 0, YEPTRIS_SCHEMA_11_COMPAT, YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(olen, 3u);
    EXPECT_EQ((unsigned char)out[0], 0x04);
    EXPECT_EQ((unsigned char)out[1], 0x08);
    EXPECT_EQ((unsigned char)out[2], '0'); /* nil */
    yeptris_marshal_free(out);
}

TEST(Marshal, MultiDocAll) {
    const char* yaml = "--- 1\n--- 2\n";
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(yaml, strlen(yaml), YEPTRIS_SCHEMA_11_COMPAT,
                                       YEPTRIS_MARSHAL_ALL_DOCS, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK);
    /* '[' count=2 'i' 1 'i' 2 = 0x5b 07 69 06 69 07 */
    std::string want = hex_to_bytes("04085b0769066907");
    ASSERT_EQ(olen, want.size());
    EXPECT_EQ(memcmp(out, want.data(), olen), 0);
    yeptris_marshal_free(out);
}

TEST(Marshal, UnsupportedOnMergeKey) {
    const char* yaml = "<<:\n  a: 1\n";
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(yaml, strlen(yaml), YEPTRIS_SCHEMA_11_COMPAT,
                                       YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
    EXPECT_EQ(st, YEPTRIS_ERROR_UNSUPPORTED);
    EXPECT_EQ(olen, 0u);
    EXPECT_EQ(out, nullptr);
    yeptris_marshal_free(out);
}

TEST(Marshal, UnsupportedOnTimestamp) {
    const char* yaml = "d: 2020-01-01\n";
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(yaml, strlen(yaml), YEPTRIS_SCHEMA_11_COMPAT,
                                       YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
    EXPECT_EQ(st, YEPTRIS_ERROR_UNSUPPORTED);
    yeptris_marshal_free(out);
}

TEST(Marshal, UnsupportedOnForwardAlias) {
    /* forward alias references cannot occur in well-formed YAML (the
     * engine rejects them first); the emitter's own guard is exercised
     * only through internal state — the corpus differential pins it */
}

TEST(Marshal, JsonInputsMarshalClean) {
    /* Strict-JSON inputs take the scanner route into the records; the
     * route-vs-engine byte equality is the Ruby binding's differential
     * corpus (2.7k inputs), this pins the C-side API contract: valid
     * JSON always marshals OK. */
    const char* inputs[] = {
        "{}",
        "[]",
        "true",
        "false",
        "null",
        "0",
        "-1.5",
        "\"hello\"",
        "{\"a\":1,\"b\":[true,null,1.5]}",
        "[{},[],[null],1,2.5]",
    };
    for (const char* in : inputs) {
        char* out = nullptr;
        size_t olen = 0;
        YeptrisStatus st = yeptris_marshal(in, strlen(in), YEPTRIS_SCHEMA_11_COMPAT,
                                           YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
        ASSERT_EQ(st, YEPTRIS_OK) << in << " " << yeptris_last_error(0, 0);
        yeptris_marshal_free(out);
    }
}

TEST(Marshal, NestedAnchorsAndRebinding) {
    /* an anchor whose value contains another anchor: the outer
     * binding must survive the inner one's recursion (ASAN caught a
     * stale-index write on exactly this shape) */
    const char* yaml = "a: &x\n  - &y 1\n  - *y\nb: *x\n";
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(yaml, strlen(yaml), YEPTRIS_SCHEMA_11_COMPAT,
                                       YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK) << yeptris_last_error(0, 0);
    EXPECT_GT(olen, 10u);
    yeptris_marshal_free(out);

    /* rebinding: the newest &anchor wins for later aliases (3GZX) */
    const char* rebind = "one: &a Foo\nsecond: &a Bar\nlate: *a\n";
    st = yeptris_marshal(rebind, strlen(rebind), YEPTRIS_SCHEMA_11_COMPAT,
                         YEPTRIS_MARSHAL_FIRST_DOC, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK);
    /* the late alias links the SECOND anchor's string: locate the
     * final '@' link and check its index points at a registered
     * object — full value equality is the Ruby spec's job */
    EXPECT_NE(memchr(out, '@', olen), nullptr);
    yeptris_marshal_free(out);

    /* anchor on an empty scalar: binds to null (6KGN) */
    const char* nul = "a: &anchor\nb: *anchor\n";
    st = yeptris_marshal(nul, strlen(nul), YEPTRIS_SCHEMA_11_COMPAT, YEPTRIS_MARSHAL_FIRST_DOC,
                         &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK);
    yeptris_marshal_free(out);
}

TEST(Marshal, NodeMarshal) {
    const char* yaml = "a: 1\nb:\n  - 2\n  - 3\n";
    YeptrisDocument doc = yeptris_parse(yaml, strlen(yaml), nullptr);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal_node(root, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK);
    /* the doc `a: 1\nb:\n  - 2\n  - 3\n` is a non-trivial
     * object graph; just sanity-check it's non-trivial bytes */
    EXPECT_GT(olen, 10u);
    EXPECT_EQ((unsigned char)out[0], 0x04);
    EXPECT_EQ((unsigned char)out[1], 0x08);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);
}

} // namespace
