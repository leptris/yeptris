/* test_marshal_json.cpp — the marshal READER's C-side test gate
 * (#322): Ruby Marshal.dump bytes IN, JSON text OUT.
 *
 * The fixtures are hex-locked Ruby 3.4 dumps. The registration law
 * they pin (derived empirically, see the mj_value comment in
 * emit/writer.c): '[', '{', '"', 'f', 'l' and anything 'I'-wrapped
 * register once per construct in 0-based stream order — the root
 * included, wrappers transparent — and every '@' indexes that
 * sequence. The linked-fixture group exists because a random-tree
 * corpus never produces repeated keys: the users-style rows (every
 * field key an '@' link after the first row) are what caught the
 * registration shifts. */

#include <gtest/gtest.h>
#include <string.h>

#include <yeptris/marshal.h>

namespace {

void expect_marshal_json(const char* hex, const char* want) {
    std::string bytes;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned b;
        sscanf(hex + i, "%2x", &b);
        bytes.push_back((char)b);
    }
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_json_from_marshal(bytes.data(), bytes.size(), 1, &out, &olen);
    ASSERT_EQ(st, YEPTRIS_OK) << "hex=" << hex;
    ASSERT_NE(out, nullptr);
    std::string got(out, olen);
    yeptris_marshal_free(out);
    EXPECT_EQ(got, want) << "hex=" << hex;
}

void expect_unsupported(const char* hex) {
    std::string bytes;
    for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
        unsigned b;
        sscanf(hex + i, "%2x", &b);
        bytes.push_back((char)b);
    }
    char* out = nullptr;
    size_t olen = 0;
    YeptrisStatus st = yeptris_json_from_marshal(bytes.data(), bytes.size(), 1, &out, &olen);
    EXPECT_EQ(st, YEPTRIS_ERROR_UNSUPPORTED) << "hex=" << hex;
    EXPECT_EQ(out, nullptr);
}

TEST(MarshalJson, ScalarsAndCollections) {
    // Marshal.dump({})
    expect_marshal_json("04087b00", "{}");
    // Marshal.dump([1, "two", 3.5, nil, true])
    expect_marshal_json("04085b0a690649220874776f063a0645546608332e353054",
                        "[1,\"two\",3.5,null,true]");
}

TEST(MarshalJson, SharedStringLink) {
    // ["dup", "dup"]: the second occurrence is '@'-linked to the first
    expect_marshal_json("04085b07492208647570063a064554492208647570063b0054", "[\"dup\",\"dup\"]");
}

TEST(MarshalJson, IvarNameSymlink) {
    // {"a"=>1,"b"=>2}: the second key's :E ivar name rides ';' 00
    expect_marshal_json("04087b0749220661063a064554690649220662063b00546907", "{\"a\":1,\"b\":2}");
}

TEST(MarshalJson, BignumSlotShiftsLaterLinks) {
    // {"b"=>2**40,"arr"=>["x","x"]}: the bignum holds a registry slot
    // even though no link ever targets one — without it the "x" link
    // lands one early and emits garbage
    expect_marshal_json("04087b0749220662063a0645546c2b08000000000001492208617272063b0054"
                        "5b0749220678063b005449220678063b0054",
                        "{\"b\":1099511627776,\"arr\":[\"x\",\"x\"]}");
}

TEST(MarshalJson, SharedHashKeysAcrossRows) {
    // [{"k"=>1},{"k"=>2}]: user2's "k" key is an '@' link to user1's
    expect_marshal_json("04085b077b064922066b063a06455469067b0640076907", "[{\"k\":1},{\"k\":2}]");
}

TEST(MarshalJson, UsersRowsAllLinksAfterFirst) {
    // three users: every field key and repeated value after row 1 is
    // an '@' link — the shape that exposed the registration shifts
    expect_marshal_json("04085b087b0b4922076964063a06455469064922096e616d65063b005449220b"
                        "557365722031063b005449220b616374697665063b00544649220a73636f7265"
                        "063b00546608312e3549220974616773063b00545b0849220661063b00544922"
                        "0662063b005449220663063b00544922096d657461063b00547b0649220964656"
                        "570063b00547b0649220678063b005469067b0b40076907400849220b5573657"
                        "22032063b0054400a54400b660633400d5b0849220661063b005449220662063"
                        "b005449220663063b005440127b0640147b06401669077b0b400769084008492"
                        "20b557365722033063b0054400a46400b6608342e35400d5b0849220661063b0"
                        "05449220662063b005449220663063b005440127b0640147b0640166908",
                        "[{\"id\":1,\"name\":\"User 1\",\"active\":false,\"score\":1.5,"
                        "\"tags\":[\"a\",\"b\",\"c\"],\"meta\":{\"deep\":{\"x\":1}}},"
                        "{\"id\":2,\"name\":\"User 2\",\"active\":true,\"score\":3.0,"
                        "\"tags\":[\"a\",\"b\",\"c\"],\"meta\":{\"deep\":{\"x\":2}}},"
                        "{\"id\":3,\"name\":\"User 3\",\"active\":false,\"score\":4.5,"
                        "\"tags\":[\"a\",\"b\",\"c\"],\"meta\":{\"deep\":{\"x\":3}}}]");
}

TEST(MarshalJson, UnsupportedPaths) {
    // self-referential array (a = []; a << a)
    expect_unsupported("04085b064000");
    // 2**100: a bignum beyond i64
    expect_unsupported("04086c2b0c0000000000000000000000001000");
    // not a marshal stream
    expect_unsupported("0000");
}

} // namespace
