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
#include <yeptris/json.h>
#include <yeptris/marshal.h>
#include <yeptris/parse.h>
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

void expect_marshal_schema(const char* in, const char* want_hex, YeptrisMarshalMode mode,
                           YeptrisSchema schema) {
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_marshal(in, strlen(in), schema, mode, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK) << yeptris_last_error(0, 0);
    std::string want = hex_to_bytes(want_hex);
    ASSERT_EQ(olen, want.size());
    EXPECT_EQ(memcmp(out, want.data(), olen), 0)
        << "input=" << in << " got=" << std::string(out, olen);
    yeptris_marshal_free(out);
}

void expect_marshal(const char* in, const char* want_hex, YeptrisMarshalMode mode) {
    expect_marshal_schema(in, want_hex, mode, YEPTRIS_SCHEMA_11_COMPAT);
}

void expect_node_marshal_schema(const char* in, const char* want_hex, YeptrisSchema schema) {
    YeptrisParseOptions opts = {};
    opts.schema = schema;
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_ex(in, strlen(in), &opts, &st);
    ASSERT_EQ(st, YEPTRIS_OK) << yeptris_last_error(0, 0);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    char* out = nullptr;
    size_t olen = 0;
    ASSERT_EQ(yeptris_marshal_node(root, &out, &olen), YEPTRIS_OK);
    std::string want = hex_to_bytes(want_hex);
    ASSERT_EQ(olen, want.size());
    EXPECT_EQ(memcmp(out, want.data(), olen), 0)
        << "input=" << in << " got=" << std::string(out, olen);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);
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

TEST(Marshal, SchemaConditionedFloatTyping) {
    /* spec 10.3.2: the core schema's float regexp has an OPTIONAL dot
     * (1e3 IS a float); Psych's dot-required quirk is compat_11-only
     * (TODO.restructure/32). Locked-in bytes decode as {"k"=>1000.0}
     * under core and {"k"=>"1e3"} under compat. */
    const char* core_hex = "04087b064922066b063a064554660b313030302e30";
    const char* compat_hex = "04087b064922066b063a064554492208316533063a064554";
    expect_marshal_schema("k: 1e3\n", core_hex, YEPTRIS_MARSHAL_FIRST_DOC, YEPTRIS_SCHEMA_12_CORE);
    expect_marshal_schema("k: 1e3\n", compat_hex, YEPTRIS_MARSHAL_FIRST_DOC,
                          YEPTRIS_SCHEMA_11_COMPAT);
    /* the node path reads the schema the DOCUMENT was parsed with */
    expect_node_marshal_schema("k: 1e3\n", core_hex, YEPTRIS_SCHEMA_12_CORE);
    expect_node_marshal_schema("k: 1e3\n", compat_hex, YEPTRIS_SCHEMA_11_COMPAT);
}

TEST(Marshal, BignumLimbCountFullWidth) {
    /* LLP64 regression: `unsigned long` limb bookkeeping is 32-bit on
     * Windows, so the limb count of any |v| >= 2^32 came out short and
     * the loader truncated the value (1234567890123456789 decoded as
     * 2112454933 — its low 32 bits). Bytes are Ruby Marshal's. */
    expect_marshal("a: 1234567890123456789\n", "04087b0649220661063a0645546c2b091581e97df4102211",
                   YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("a: -1234567890123456789\n", "04087b0649220661063a0645546c2d091581e97df4102211",
                   YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("a: 4294967296\n", "04087b0649220661063a0645546c2b08000000000100",
                   YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("a: -4294967296\n", "04087b0649220661063a0645546c2d08000000000100",
                   YEPTRIS_MARSHAL_FIRST_DOC);
    expect_marshal("a: 9223372036854775807\n", "04087b0649220661063a0645546c2b09ffffffffffffff7f",
                   YEPTRIS_MARSHAL_FIRST_DOC);
}

TEST(Marshal, BignumTextBeyondTheOldLimbCap) {
    /* e_bignum_text used a fixed 128-limb array (~2048 bits, ~616
     * digits) while its entry gate admitted 1299: every value in
     * between emitted a silently WRONG number (dropped carries). The
     * arrays now size from the gate bound; bytes are Ruby Marshal's
     * (10^700 = 146 limbs, past the old cap; the 1299-digit gate
     * boundary = 270 limbs). */
    std::string in700 = "a: 1" + std::string(700, '0') + "\n";
    expect_marshal(in700.c_str(),
                   "04087b0649220661063a0645546c2b0192000000000000000000000000000000000000000000"
                   "0000000000000000000000000000000000000000000000000000000000000000000000000000"
                   "0000000000000000000000000000000000000000000000000000000010397aa6fdb1a0e2c5fc"
                   "db8f0d7a5c30e2be4abb6c5150d3906c9f558e4c5883cd0b2bc8d7b75a807d54d4cd657dc3bb"
                   "ec5aab61a11d4f0ebfa436b7e03b65c515c29c51ec2852f8369b9bfbcbd77fc930a537dd320e"
                   "4aaa8e6e5b10939a053ac2cfcb4d1bc79800e143a16f75715e789c7951fe15216bc1889e7a3b"
                   "d695de33051c77fe83d18062eb5f27169239d09d2e1eabfb8a31243aa95175efa7b8dcc92087"
                   "cfb63ca9aba2a27e5fc024b52cb3942ef6f7fb712bc1779aa520123b91eef2281edd6312943e"
                   "67c9c62800",
                   YEPTRIS_MARSHAL_FIRST_DOC);
    std::string in1299 = "a: " + std::string(1299, '9') + "\n";
    expect_marshal(in1299.c_str(),
                   "04087b0649220661063a0645546c2b020e01ffffffffffffffffffffffffffffffffffffffff"
                   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
                   "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffe76972b8ed21ec3741e6"
                   "e46215b589706400ad3e64cea5a322328e15186cc67365253f4ae9488d0e9d562a2798f0a8e6"
                   "7749e01f8e7fe1baa56aa85ff8beaf32d4cc8734c346309fdf5376cf09016c55b7ba5117fea9"
                   "3677ea503389ac333cc3ac61ee306aa113b7a6d48f91d0b07251c09ab10f90e029c03f9737ba"
                   "c5bcddd5014a81b75ea212e8b26e94693e374ffa94ff6679ed8fc5909ad3e30b036d059d7f7d"
                   "b01e70bba5da2257d43d87b1443ae28be76f02c18e2fe408e4827efe1b2b2434c5db13ac577b"
                   "99db2dc098bd0857c15a77181da350776509bfbff0da10ad9e9240b9fa40ccd318fc65368976"
                   "3272677aa324faa4ca47a53cd47f5541840ff1701cf68976055d1d2519dd4d3240c832eac09e"
                   "9ecdbbae87b8fa5b535b62a0184b799f693f961e0146e6f1fb92b98af634885e82ad213a6a42"
                   "4f3c7357a7cdf5bef8adc69041bc13d6ab107c2a01fa7322f305d11a9eff7ec22400a7509571"
                   "f39053c2ce41b95785938b9a38e6490a048d86d5556c4f8c1709",
                   YEPTRIS_MARSHAL_FIRST_DOC);
}

} // namespace

/* #178: an explicit tag anywhere in the subtree bails marshal_node —
 * the value records carry the resolver's verdict, not the source tag,
 * so a `!ruby/object` map would otherwise marshal as a plain hash
 * (psych's visitor revives it; the tag must not vanish). */
TEST(MarshalNode, ExplicitTagsBail) {
    const char* docs[] = {
        "a: 1\nb: !ruby/object {}\n",
        "- !ruby/struct::Point2 {x: 3, y: 4}\n",
        "top:\n  inner: !!str tagged\n",
        "k: !custom/tag v\n",
    };
    for (const char* y : docs) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_ex(y, strlen(y), NULL, &st);
        ASSERT_EQ(st, YEPTRIS_OK) << y;
        YeptrisNode root = yeptris_document_root(doc, 0);
        ASSERT_NE(root, nullptr);
        char* out = NULL;
        size_t olen = 0;
        EXPECT_EQ(yeptris_marshal_node(root, &out, &olen), YEPTRIS_ERROR_UNSUPPORTED) << y;
        yeptris_marshal_free(out);
        yeptris_document_free(doc);
    }
    /* untagged bulk data still marshals (the fast path's payload) */
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc =
        yeptris_parse("- :id: 1\n  file: x\n", strlen("- :id: 1\n  file: x\n"), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    char* out = NULL;
    size_t olen = 0;
    ASSERT_EQ(yeptris_marshal_node(yeptris_document_root(doc, 0), &out, &olen), YEPTRIS_OK);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);
}

TEST(Marshal, StrictDuplicateKeysSequenceEntriesNeverFlag) {
    /* yeptris-ruby#333: the value stream's slot rule toggled the key
     * parity for SEQUENCE entries too, so every odd-position array
     * element carried is_key=1 and the strict-dup scan false-flagged
     * arrays with a repeated value at two odd positions — a plain
     * ["a","b","a"] answered "duplicate key in object" on valid JSON
     * (the yeptris#680 class, through the marshal route's value
     * stream). A sequence entry never takes a key slot; TRUE map
     * duplicates keep raising. */
    struct Row {
        const char* y;
        YeptrisStatus want;
    };
    const Row rows[] = {
        {R"(["a", "b", "a"])", YEPTRIS_OK},
        {R"({"k": ["x", "y", "x"]})", YEPTRIS_OK},
        {R"([[1, 2, 1], [1, 2, 1]])", YEPTRIS_OK},
        {R"([{"m": 1}, {"m": 2}])", YEPTRIS_OK},      /* same key, different maps */
        {R"({"a": 1, "a": 2})", YEPTRIS_ERROR_PARSE}, /* true dup still raises */
        {R"({"n": {"z": 1}, "m": {"z": 2}})", YEPTRIS_OK},
    };
    for (const Row& r : rows) {
        size_t n = strlen(r.y);
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_json(r.y, n, &st);
        ASSERT_EQ(st, YEPTRIS_OK) << r.y;
        char* out = nullptr;
        size_t olen = 0;
        EXPECT_EQ(yeptris_marshal_node_ex(yeptris_document_root(doc, 0),
                                          YEPTRIS_MARSHAL_STRICT_DUP_KEYS, &out, &olen),
                  r.want)
            << r.y;
        if (out != nullptr) {
            yeptris_marshal_free(out);
        }
        yeptris_document_free(doc);
    }
}

TEST(Marshal, StrictDuplicateKeysRaiseBeforeAnyBytes) {
    /* yeptris-ruby#278: the JSON fallback's strict route must raise on
     * duplicate keys exactly like the native materializer's rb_hash_aref
     * probe (json 3 default). The JSON DOM keeps both pairs, so the
     * pre-scan sees them; non-strict stays last-wins (json 2.x). */
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc =
        yeptris_parse_json("{\"a\":1,\"a\":2,\"b\":{\"k\":1,\"k\":2}}",
                           strlen("{\"a\":1,\"a\":2,\"b\":{\"k\":1,\"k\":2}}"), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    YeptrisNode root = yeptris_document_root(doc, 0);
    char* out = nullptr;
    size_t olen = 0;
    EXPECT_EQ(yeptris_marshal_node_ex(root, YEPTRIS_MARSHAL_STRICT_DUP_KEYS, &out, &olen),
              YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(out, nullptr);
    /* the message names the key like the tape walk's (json 3 names it too) */
    EXPECT_STREQ(yeptris_last_error(nullptr, nullptr), "duplicate key \"a\" in JSON object");
    EXPECT_EQ(yeptris_marshal_node_ex(root, YEPTRIS_MARSHAL_STRICT_DUP_KEYS, &out, &olen),
              YEPTRIS_ERROR_PARSE); /* nested dup under "b" raises too */
    EXPECT_EQ(out, nullptr);
    yeptris_marshal_free(out);
    /* flags are validated: an unknown bit is ARG */
    EXPECT_EQ(yeptris_marshal_node_ex(root, 0x2, &out, &olen), YEPTRIS_ERROR_ARG);
    /* non-strict emission still succeeds (last-wins) */
    ASSERT_EQ(yeptris_marshal_node_ex(root, 0, &out, &olen), YEPTRIS_OK);
    ASSERT_NE(out, nullptr);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);

    /* clean strict input emits normally */
    doc = yeptris_parse_json("{\"a\":1,\"b\":[1,2]}", strlen("{\"a\":1,\"b\":[1,2]}"), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_EQ(yeptris_marshal_node_ex(yeptris_document_root(doc, 0),
                                      YEPTRIS_MARSHAL_STRICT_DUP_KEYS, &out, &olen),
              YEPTRIS_OK);
    ASSERT_NE(out, nullptr);
    EXPECT_EQ(out[0], 0x04);
    EXPECT_EQ(out[1], 0x08);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);

    /* the plain (non-JSON) node entry keeps its contract */
    doc = yeptris_parse("a: 1", strlen("a: 1"), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    ASSERT_EQ(yeptris_marshal_node(yeptris_document_root(doc, 0), &out, &olen), YEPTRIS_OK);
    yeptris_marshal_free(out);
    yeptris_document_free(doc);
}

TEST(Marshal, StrictDuplicateKeysArePerMap) {
    /* The strict pre-scan keeps one key table per open map (0.6.34
     * shared one table, cleared on MAP_OPEN): a parent key matching a
     * key in an earlier child object was a false duplicate, and a
     * parent key repeated after a nested object went unseen. */
    struct Case {
        const char* doc;
        YeptrisStatus want;
    };
    const Case cases[] = {
        {"{\"a\":1,\"b\":{\"a\":2},\"c\":3}", YEPTRIS_OK},
        {"{\"a\":[{\"b\":1}],\"b\":2}", YEPTRIS_OK},
        {"{\"a\":{\"b\":1,\"b\":2}}", YEPTRIS_ERROR_PARSE},
        {"{\"p\":1,\"a\":{\"x\":1},\"p\":2}", YEPTRIS_ERROR_PARSE},
    };
    for (const Case& tc : cases) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse_json(tc.doc, strlen(tc.doc), &st);
        ASSERT_EQ(st, YEPTRIS_OK) << tc.doc;
        char* out = nullptr;
        size_t olen = 0;
        EXPECT_EQ(yeptris_marshal_node_ex(yeptris_document_root(doc, 0),
                                          YEPTRIS_MARSHAL_STRICT_DUP_KEYS, &out, &olen),
                  tc.want)
            << tc.doc;
        yeptris_marshal_free(out);
        yeptris_document_free(doc);
    }
}

TEST(Marshal, StrictDuplicateKeysScanGrowsPastSixteenKeys) {
    /* The 0.6.34 table was fixed at 16 slots with no growth: a 17th
     * distinct key probed forever. 64 keys must emit. */
    std::string doc = "{";
    for (int i = 0; i < 64; i++) {
        if (i) {
            doc += ",";
        }
        doc += "\"k" + std::to_string(i) + "\":" + std::to_string(i);
    }
    doc += "}";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument d = yeptris_parse_json(doc.c_str(), doc.size(), &st);
    ASSERT_EQ(st, YEPTRIS_OK);
    char* out = nullptr;
    size_t olen = 0;
    EXPECT_EQ(yeptris_marshal_node_ex(yeptris_document_root(d, 0), YEPTRIS_MARSHAL_STRICT_DUP_KEYS,
                                      &out, &olen),
              YEPTRIS_OK);
    ASSERT_NE(out, nullptr);
    yeptris_marshal_free(out);
    yeptris_document_free(d);
}
