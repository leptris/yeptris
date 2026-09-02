/* test_mapindex.cpp — O(1) mapping lookup (TODO.impl/11).
 *
 * The lazy per-map index must be semantically identical to the
 * linear scan it replaces: present/absent keys, first-wins on
 * duplicates, collection keys never match, and mutation (add/set/
 * del) is reflected after the table was already built. Concurrent
 * first-lookups serialize on the document mutex (read-sharing). */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <yeptris.h>

namespace {

std::string v(YeptrisNode n) {
    size_t len = 0;
    const char* p = yeptris_node_value(n, &len);
    return std::string(p ? p : "", len);
}

} // namespace

TEST(MapIndex, LargeMapHitsAndMisses) {
    std::string yaml = "k0: v0\n";
    for (int i = 1; i < 20000; i++) {
        yaml += "k" + std::to_string(i) + ": v" + std::to_string(i) + "\n";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);

    /* first lookup builds the table; hits across the whole range */
    for (int i = 0; i < 20000; i += 997) {
        std::string k = "k" + std::to_string(i);
        YeptrisNode n = yeptris_node_map_get(root, k.data(), k.size());
        ASSERT_NE(n, nullptr) << k;
        EXPECT_EQ(v(n), "v" + std::to_string(i));
    }
    /* misses */
    EXPECT_EQ(yeptris_node_map_get(root, "nope", 4), nullptr);
    EXPECT_EQ(yeptris_node_map_get(root, "k", 1), nullptr);
    EXPECT_EQ(yeptris_node_map_get(root, "", 0), nullptr);
    /* prefix/suffix near-misses must not match */
    EXPECT_EQ(yeptris_node_map_get(root, "k20000", 6), nullptr); /* one past the range */
    EXPECT_EQ(yeptris_node_map_get(root, "k19999x", 7), nullptr);
    EXPECT_EQ(yeptris_node_map_get(root, "xk1", 3), nullptr);
    yeptris_document_free(doc);
}

TEST(MapIndex, FirstKeyWinsOnDuplicates) {
    YeptrisStatus st = YEPTRIS_OK;
    const char* dup = "dup: first\ndup: second\n";
    YeptrisDocument doc = yeptris_parse(dup, strlen(dup), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    EXPECT_EQ(v(yeptris_node_map_get(root, "dup", 3)), "first");
    yeptris_document_free(doc);
}

TEST(MapIndex, CollectionKeysNeverMatchAStringLookup) {
    const char* yaml = "? [a, b]\n: seqkey\n? {x: 1}\n: mapkey\nplain: text\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml, strlen(yaml), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    EXPECT_EQ(yeptris_node_map_get(root, "[a, b]", 6), nullptr);
    EXPECT_EQ(yeptris_node_map_get(root, "{x: 1}", 6), nullptr);
    EXPECT_EQ(v(yeptris_node_map_get(root, "plain", 5)), "text");
    yeptris_document_free(doc);
}

TEST(MapIndex, MutationSeenAfterIndexWasBuilt) {
    const char* base = "a: 1\nb: 2\n";
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(base, strlen(base), &st);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    /* build the index */
    EXPECT_EQ(v(yeptris_node_map_get(root, "b", 1)), "2");

    /* add */
    YeptrisNode c = yeptris_node_new_scalar(doc, "3", 1, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "c", 1, c), YEPTRIS_OK);
    EXPECT_EQ(v(yeptris_node_map_get(root, "c", 1)), "3");
    EXPECT_EQ(v(yeptris_node_map_get(root, "a", 1)), "1");

    /* set replaces */
    YeptrisNode b2 = yeptris_node_new_scalar(doc, "22", 2, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_set(root, "b", 1, b2), YEPTRIS_OK);
    EXPECT_EQ(v(yeptris_node_map_get(root, "b", 1)), "22");

    /* del removes */
    ASSERT_EQ(yeptris_node_map_del(root, "a", 1), YEPTRIS_OK);
    EXPECT_EQ(yeptris_node_map_get(root, "a", 1), nullptr);

    /* a rebuilt-after-del set works */
    YeptrisNode a2 = yeptris_node_new_scalar(doc, "11", 2, YEPTRIS_STYLE_PLAIN);
    ASSERT_EQ(yeptris_node_map_add(root, "a", 1, a2), YEPTRIS_OK);
    EXPECT_EQ(v(yeptris_node_map_get(root, "a", 1)), "11");
    yeptris_document_free(doc);
}

TEST(MapIndex, ConcurrentFirstLookupsBuildOnce) {
    /* 8 threads racing the FIRST lookup on the same mapping: builds
     * serialize on the document mutex; every thread sees the answer */
    std::string yaml;
    for (int i = 0; i < 2000; i++) {
        yaml += "key" + std::to_string(i) + ": " + std::to_string(i) + "\n";
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(yaml.data(), yaml.size(), &st);
    ASSERT_NE(doc, nullptr);

    std::vector<std::thread> threads;
    std::vector<int> bad(8, 0);
    for (int t = 0; t < 8; t++) {
        threads.emplace_back([doc, t, &bad] {
            YeptrisNode root = yeptris_document_root(doc, 0);
            for (int r = 0; r < 100; r++) {
                int i = (t * 100 + r) % 2000;
                std::string k = "key" + std::to_string(i);
                YeptrisNode n = yeptris_node_map_get(root, k.data(), k.size());
                if (n == nullptr || v(n) != std::to_string(i)) {
                    bad[t]++;
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    for (int t = 0; t < 8; t++) {
        EXPECT_EQ(bad[t], 0) << "thread " << t;
    }
    yeptris_document_free(doc);
}
