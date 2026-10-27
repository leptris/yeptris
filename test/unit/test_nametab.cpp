/* test_nametab.cpp — the anchor/key interner primitive (TODO.impl/18A).
 * Growth across rehash thresholds is the historically buggy path (the
 * linear scans it replaced made anchor-heavy documents quadratic).
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "common/nametab.h"

namespace {

yep_view view(const std::string& s) {
    return yep_view{s.data(), (uint32_t)s.size()};
}

} // namespace

TEST(Nametab, InitFreeAndEmptyGet) {
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    EXPECT_EQ(yep_nametab_get(&t, view("")), YEP_NAMETAB_NIL);
    EXPECT_EQ(yep_nametab_get(&t, view("anything")), YEP_NAMETAB_NIL);
    yep_nametab_free(&t);
}

TEST(Nametab, SetGetOverwrite) {
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    /* keys are borrowed: stable storage, never temporaries */
    std::string a = "a", ab = "ab", zero = "zero";
    std::string probe_abc = "abc";
    EXPECT_EQ(yep_nametab_set(&t, view(a), 7), 1);
    EXPECT_EQ(yep_nametab_get(&t, view(a)), 7u);
    /* last definition wins */
    EXPECT_EQ(yep_nametab_set(&t, view(a), 9), 1);
    EXPECT_EQ(yep_nametab_get(&t, view(a)), 9u);
    /* value 0 is storable and distinct from NIL */
    EXPECT_EQ(yep_nametab_set(&t, view(zero), 0), 1);
    EXPECT_EQ(yep_nametab_get(&t, view(zero)), 0u);
    /* prefix names never collide-match */
    EXPECT_EQ(yep_nametab_set(&t, view(ab), 1), 1);
    EXPECT_EQ(yep_nametab_get(&t, view(a)), 9u);
    EXPECT_EQ(yep_nametab_get(&t, view(ab)), 1u);
    EXPECT_EQ(yep_nametab_get(&t, view(probe_abc)), YEP_NAMETAB_NIL);
    yep_nametab_free(&t);
}

TEST(Nametab, ClearDropsEntries) {
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    /* keys are borrowed: stable storage, never temporaries */
    std::string a = "a", b = "b";
    EXPECT_EQ(yep_nametab_set(&t, view(a), 1), 1);
    yep_nametab_clear(&t);
    EXPECT_EQ(yep_nametab_get(&t, view(a)), YEP_NAMETAB_NIL);
    /* reusable after clear */
    EXPECT_EQ(yep_nametab_set(&t, view(b), 2), 1);
    EXPECT_EQ(yep_nametab_get(&t, view(b)), 2u);
    EXPECT_EQ(t.count, 1u);
    yep_nametab_free(&t);
}

TEST(Nametab, GrowthAcrossRehashes) {
    /* Crosses the 64-slot table's rehash many times over; every key
     * must stay resolvable — catches slot-mapping and finalizer bugs. */
    const int n = 50000;
    std::vector<std::string> keys;
    keys.reserve(n);
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    for (int i = 0; i < n; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "anchor_%d_x", i);
        keys.push_back(buf);
        EXPECT_EQ(yep_nametab_set(&t, view(keys.back()), (uint32_t)i), 1) << i;
    }
    EXPECT_EQ(t.count, (uint32_t)n);
    for (int i = 0; i < n; i++) {
        EXPECT_EQ(yep_nametab_get(&t, view(keys[i])), (uint32_t)i) << keys[i];
    }
    EXPECT_EQ(yep_nametab_get(&t, view("anchor_0_x-")), YEP_NAMETAB_NIL);
    yep_nametab_free(&t);
}

TEST(Nametab, CollisionHeavyKeys) {
    /* Same prefix, one differing char: adversarial for weak hashes
     * under power-of-two masking. */
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    /* keys are borrowed views: reserve so the vector never reallocates
     * (16-char SSO strings live inside the objects, which would move) */
    std::vector<std::string> keys;
    keys.reserve(256);
    for (int i = 0; i < 256; i++) {
        std::string k(16, 'k');
        k[7] = (char)i;
        keys.push_back(k);
        EXPECT_EQ(yep_nametab_set(&t, view(keys.back()), (uint32_t)i), 1);
    }
    for (int i = 0; i < 256; i++) {
        EXPECT_EQ(yep_nametab_get(&t, view(keys[i])), (uint32_t)i);
    }
    yep_nametab_free(&t);
}

TEST(Nametab, RepeatedUpsertKeepsCount) {
    yep_nametab t;
    ASSERT_EQ(yep_nametab_init(&t, yep_system_allocator()), 1);
    std::string same = "same";
    for (int i = 0; i < 1000; i++) {
        EXPECT_EQ(yep_nametab_set(&t, view(same), (uint32_t)i), 1);
    }
    EXPECT_EQ(t.count, 1u);
    EXPECT_EQ(yep_nametab_get(&t, view(same)), 999u);
    yep_nametab_free(&t);
}
