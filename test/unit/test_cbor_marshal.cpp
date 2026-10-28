/* test_cbor_marshal.cpp — the CBOR marshal READER's C-side gate
 * (yeptris-ruby#322's CBOR half): Ruby Marshal.dump bytes IN, ONE CBOR
 * data item OUT.
 *
 * The twin of test_marshal_json.cpp. The fixtures are hex-locked Ruby
 * 3.4 dumps paired with the bytes the per-node route emits for the
 * same host value (the binding's YAMLTree -> Document -> cbor encode
 * path), so every fixture is a byte-parity pin, not a re-derivation.
 * UNSUPPORTED fixtures pin the DECLINE side: the host's bytes have no
 * exact fast-path spelling (or the fallback raises), so the binding
 * keeps its own walk, which is exact by construction.
 *
 * The registration-link fixtures (shared strings, shared map keys
 * across rows, the bignum slot) exist because a random-tree corpus
 * never emits '@' links. */

#include <gtest/gtest.h>
#include <string.h>

#include <yeptris/cbor.h>
#include <yeptris/marshal.h>

namespace {

std::string unhex(const char* hex) {
    std::string bytes;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned b;
        sscanf(hex + i, "%2x", &b);
        bytes.push_back((char)b);
    }
    return bytes;
}

void expect_cbor(const char* hex, uint32_t opts, const char* want_hex) {
    std::string bytes = unhex(hex);
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_cbor_from_marshal(bytes.data(), bytes.size(), opts, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK) << "marshal=" << hex;
    ASSERT_NE(out, nullptr);
    std::string got(out, olen);
    yeptris_marshal_free(out);
    EXPECT_EQ(got, unhex(want_hex)) << "marshal=" << hex;
}

void expect_unsupported(const char* hex) {
    std::string bytes = unhex(hex);
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st =
        yeptris_cbor_from_marshal(bytes.data(), bytes.size(), YEPTRIS_CBOR_CANONICAL, &out, &olen);
    EXPECT_EQ(st, YEPTRIS_ERROR_UNSUPPORTED) << "marshal=" << hex;
    EXPECT_EQ(out, nullptr) << "marshal=" << hex;
}

TEST(CborMarshal, ScalarsAndCollections) {
    // {}
    expect_cbor("04087b00", YEPTRIS_CBOR_CANONICAL, "a0");
    // [1, "two", 3.5, nil, true] — 3.5 rides the preferred half
    expect_cbor("04085b0a690649220874776f063a0645546608332e353054", YEPTRIS_CBOR_CANONICAL,
                "85016374776ff94300f6f5");
}

TEST(CborMarshal, StringKeysAndLinks) {
    // {"a"=>1,"b"=>2}: the second key's :E ivar name rides ';' 00
    expect_cbor("04087b0749220661063a064554690649220662063b00546907", YEPTRIS_CBOR_CANONICAL,
                "a2616101616202");
    // ["dup","dup"]: the second string is an '@' link — the memo spans
    // do not chain, the bytes are re-spelled from the payload
    expect_cbor("04085b07492208647570063a0645544006", YEPTRIS_CBOR_CANONICAL, "826364757063647570");
    // [{"k"=>1},{"k"=>2}]: row two's "k" key is an '@' link into row one
    expect_cbor("04085b077b064922066b063a06455469067b0640076907", YEPTRIS_CBOR_CANONICAL,
                "82a1616b01a1616b02");
}

TEST(CborMarshal, BignumTakesASlotButNoLink) {
    // [2**40, "x", "x"]: Ruby registers no link for a bignum (it dumps
    // a repeated one in full) but the bignum still OCCUPIES a slot, so
    // the string is slot 2 and the link is '@' 2. Counting slots
    // wrongly would point this link at the wrong construct.
    expect_cbor("04085b086c2b0800000000000149220678063a0645544007", YEPTRIS_CBOR_CANONICAL,
                "831b000001000000000061786178");
    // {"b"=>2**40,"arr"=>["x","x"]}: canonical order puts "arr" first
    expect_cbor("04087b0749220662063a0645546c2b08000000000001492208617272063b00545b0749220678"
                "063b005449220678063b0054",
                YEPTRIS_CBOR_CANONICAL, "a261621b0000010000000000636172728261786178");
}

TEST(CborMarshal, SymbolsSpellColonName) {
    // {:a=>:b} — the per-node route's YAMLTree scalar is ":a"/":b"
    expect_cbor("04087b063a06613a0662", YEPTRIS_CBOR_CANONICAL, "a1623a61623a62");
    // [:a, :a]: the symbol table's own ';' link shares no slot with '@'
    expect_cbor("04085b073a06613b00", YEPTRIS_CBOR_CANONICAL, "82623a61623a61");
    // a symbol with invalid UTF-8 bytes: a byte string of ":name"
    expect_cbor("04085b063a06ff", YEPTRIS_CBOR_CANONICAL, "81423aff");
}

TEST(CborMarshal, ReservedWordsReproduceTheSimpleLedger) {
    // "undefined" -> simple(23); "simple(32)" -> F8 20; the <=9-byte
    // words ("simple(7)") and the 24..31 gap stay text
    expect_cbor("04085b0949220e756e646566696e6564063a06455449220f73696d706c6528333229063b005449"
                "220e73696d706c65283729063b005449220f73696d706c6528323429063b0054",
                YEPTRIS_CBOR_CANONICAL, "84f7f8206973696d706c652837296a73696d706c6528323429");
    // the ledger applies to text keys too
    expect_cbor("04087b0649220e756e646566696e6564063a0645546906", YEPTRIS_CBOR_CANONICAL, "a1f701");
    expect_cbor("04087b0649220f73696d706c6528333229063a0645546906", YEPTRIS_CBOR_CANONICAL,
                "a1f82001");
}

TEST(CborMarshal, FloatsUseThePreferredWidth) {
    // [-0.0, 1.5, 1.5e300]: half, half, double
    expect_cbor("04085b0866072d306608312e35660c312e3565333030", YEPTRIS_CBOR_CANONICAL,
                "83f98000f93e00fb7e41eb2d66005835");
    // {"k"=>[1.5, 1.5]}: a repeated float links and re-emits its span
    expect_cbor("04087b064922066b063a0645545b076608312e354008", YEPTRIS_CBOR_CANONICAL,
                "a1616b82f93e00f93e00");
}

TEST(CborMarshal, OutOfInt64IntegersRidePreferredFloats) {
    // [2**63, -(2**63)]: the positive one re-renders as a float (the
    // number kernel's ledger); the negative one is int64 min
    expect_cbor("04085b076c2b0900000000000000806c2d090000000000000080", YEPTRIS_CBOR_CANONICAL,
                "82fa5f0000003b7fffffffffffffff");
    // [2**63, -(2**63)-1]: both directions past int64 ride the float
    // ledger, and -(2**64-1) rounds to the same float32
    expect_cbor("04085b076c2b0900000000000000806c2d090100000000000080", YEPTRIS_CBOR_CANONICAL,
                "82fa5f000000fadf000000");
    expect_cbor("04085b066c2d09ffffffffffffffff", YEPTRIS_CBOR_CANONICAL, "81fadf800000");
}

TEST(CborMarshal, CanonicalSortsEncodedKeys) {
    // {"b"=>1,"a"=>2}: canonical vs insertion order
    expect_cbor("04087b0749220662063a064554690649220661063b00546907", 0, "a2616201616102");
    expect_cbor("04087b0749220662063a064554690649220661063b00546907", YEPTRIS_CBOR_CANONICAL,
                "a2616102616201");
    // {"z"=>[1,2],"a"=>{"k"=>3.25}}: nested canonical maps, half float
    expect_cbor("04087b074922067a063a0645545b076906690749220661063b00547b064922066b063b00546609"
                "332e3235",
                YEPTRIS_CBOR_CANONICAL, "a26161a1616bf94280617a820102");
    // {1=>"i","a"=>"s",2.5=>"f"}: numeric keys sort by their encoding
    expect_cbor("04087b08690649220669063a06455449220661063b005449220673063b00546608322e354922"
                "0666063b0054",
                YEPTRIS_CBOR_CANONICAL, "a301616961616173f941006166");
}

TEST(CborMarshal, BignumKeysRideTheNumberLaw) {
    // int64-range bignums encode as integers; past that the number
    // kernel's float re-render
    expect_cbor("04087b066c2b080000000000016906", YEPTRIS_CBOR_CANONICAL, "a11b000001000000000001");
    expect_cbor("04087b066c2d080000000000016906", YEPTRIS_CBOR_CANONICAL, "a13b000000ffffffffff01");
    expect_cbor("04087b066c2b0900000000000000806906", YEPTRIS_CBOR_CANONICAL, "a1fa5f00000001");
    expect_cbor("04087b066c2d0901000000000000806906", YEPTRIS_CBOR_CANONICAL, "a1fadf00000001");
    // past four limbs the fast path is out (the fallback re-renders
    // through the number kernel), in either direction
    expect_unsupported("04087b066c2b0a000000000000000040006906");
    expect_unsupported("04087b066c2d0a000000000000000040006906");
}

TEST(CborMarshal, StringValueEncodingLaw) {
    // unwrapped ASCII-8BIT and ASCII-only US-ASCII are text
    expect_cbor("04085b06220678", YEPTRIS_CBOR_CANONICAL, "816178");
    expect_cbor("04085b06492208616263063a064546", YEPTRIS_CBOR_CANONICAL, "8163616263");
    // empty, and UTF-8 with its :E ivar
    expect_cbor("04085b06492200063a064554", YEPTRIS_CBOR_CANONICAL, "8160");
    expect_cbor("04085b0649221268c3a96c6c6f2077c3b6726c64063a064554", YEPTRIS_CBOR_CANONICAL,
                "816d68c3a96c6c6f2077c3b6726c64");
    // a non-ASCII ASCII-8BIT value takes the !binary tag (the encoder
    // refuses); invalid UTF-8 trips the plain-safety regexp; a named
    // encoding is not the fast path's business (even ASCII-only)
    expect_unsupported("04085b06220b68c3a96c6c6f");
    expect_unsupported("04085b06492207ff01063a064554");
    expect_unsupported("04085b06492208616263063a0d656e636f64696e67220b4555432d4a50");
    expect_unsupported("04085b064922088fabb1063a0d656e636f64696e67220b4555432d4a50");
}

TEST(CborMarshal, StringKeyResolutionLaw) {
    // map_add stores the raw key BYTES; the document's compat-11
    // resolver retypes them. Numbers, bools, null and the empty key are
    // the fallback's spellings only.
    expect_unsupported("04087b064922073432063a0645546906");     // "42"
    expect_unsupported("04087b06492208312e35063a0645546906");   // "1.5"
    expect_unsupported("04087b0649220974727565063a0645546906"); // "true"
    expect_unsupported("04087b06492208796573063a0645546906");   // "yes"
    expect_unsupported("04087b064922067e063a0645546906");       // "~"
    expect_unsupported("04087b06492200063a0645546906");         // ""
    expect_unsupported("04087b06492208303132063a0645546906");   // octal
    // text keys ride the key law, whatever their Ruby encoding said
    expect_cbor("04087b064922073a61063a0645546906", YEPTRIS_CBOR_CANONICAL, "a1623a6101");
    expect_cbor("04087b06220b68c3a96c6c6f6906", YEPTRIS_CBOR_CANONICAL, "a16668c3a96c6c6f01");
    expect_cbor("04087b062206ff6906", YEPTRIS_CBOR_CANONICAL, "a141ff01");
    expect_cbor("04087b064922088fabb1063a0d656e636f64696e67220b4555432d4a506906",
                YEPTRIS_CBOR_CANONICAL, "a1438fabb101");
    expect_cbor("04087b06492208616263063a0d656e636f64696e67220b4555432d4a506906",
                YEPTRIS_CBOR_CANONICAL, "a16361626301");
    // invalid-UTF-8 symbol: ":name" as a byte string
    expect_cbor("04087b063a06ff6906", YEPTRIS_CBOR_CANONICAL, "a1423aff01");
}

TEST(CborMarshal, ContainerAndBoolKeysKeepTheFallback) {
    // arrays/hashes stringify in the fallback ("[1]", "{...}") — a
    // sequence or map key is a different wire shape
    expect_unsupported("04087b065b0649220678063a0645546906");
    expect_unsupported("04087b065b063a06616906");
    expect_unsupported("04087b067b0649220661063a06455469066907");
    expect_unsupported("04087b06495b066906063a07407869066906");
    expect_unsupported("04087b06497b0649220661063a0645546906063a07407869076907");
    // booleans sort by their TEXT but emit f4/f5 in canonical mode
    expect_unsupported("04087b06546906");
    expect_unsupported("04087b06306906");
}

TEST(CborMarshal, DuplicateKeyTextsKeepTheFallback) {
    // ":a" (String) vs :a (Symbol) both store the key text ":a"
    expect_unsupported("04087b074922073a61063a06455469063a06616907");
    // a UTF-8 key and its ASCII-8BIT twin store the same bytes
    expect_unsupported("04087b0749220b68c3a96c6c6f063a0645546906220b68c3a96c6c6f6907");
}

TEST(CborMarshal, LinkedStringsKeepTheLandingRole) {
    // a linked string re-spells by the position it lands in: here a
    // frozen "abc" lands as a value (text) and as a key (text)
    expect_cbor("04087b074922066b063a064554220861626340076906", YEPTRIS_CBOR_CANONICAL,
                "a2616b636162636361626301");
    // ... and a frozen "42" lands as a value (text) then a key (the
    // resolver's INT) — no single span can be both
    expect_unsupported("04087b074922066b063a0645542207343240076906");
    expect_unsupported("04087b074922066b063a064554220b68c3a96c6c6f40076906");
    // a key first, then a value whose encoding law refuses the bytes
    expect_unsupported("04087b072206ff690649220676063a0645544006");
}

TEST(CborMarshal, UnsupportedHostsKeepTheFallback) {
    // nil map key (the per-node route raises), invalid-UTF-8 string
    // (it tags !binary), NaN/inf, a bignum past four words, a Set
    expect_unsupported("04087b06306906");
    expect_unsupported("04082206ff");
    expect_unsupported("040866086e616e");
    expect_unsupported("04086608696e66");
    expect_unsupported("04086c2b0a00000000000000000100");
    expect_unsupported("04086f3a08536574063a0a40686173687d0769065469075446");
    // not a 4.8 stream, or trailing bytes after the root
    expect_unsupported("0409a0");
    {
        std::string bytes = unhex("04087b00");
        bytes.push_back('\0');
        char* out = nullptr;
        size_t olen = 0;
        EXPECT_EQ(yeptris_cbor_from_marshal(bytes.data(), bytes.size(), YEPTRIS_CBOR_CANONICAL,
                                            &out, &olen),
                  YEPTRIS_ERROR_UNSUPPORTED);
        EXPECT_EQ(out, nullptr);
    }
}

TEST(CborMarshal, ContainerLinksAreUnsupported) {
    // Marshal.dump(a = [1]; [a, a]) — the per-node route anchors the
    // array and the CBOR encoder refuses the alias
    expect_unsupported("04085b075b0669064006");
    // {"q"=>[a=["n"], a]}
    expect_unsupported("04087b0649220671063a0645545b075b064922066e063b00544008");
}

TEST(CborMarshal, CyclesAreUnsupported) {
    // Marshal.dump(a = []; a << a) — the link targets the array still
    // being emitted (no span yet): UNSUPPORTED, the binding keeps its
    // own walk (which raises on the cycle)
    expect_unsupported("04085b064000");
}

} // namespace
