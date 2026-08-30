/* test_memory.cpp — allocator / pool / arena (TODO.impl/03).
 *
 * The counting allocator is the injection seam: live==0 after every test
 * is the zero-leak gate; fail_after exercises allocation-failure paths.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <string>
#include <vector>

#include "memory/allocator.h"
#include "memory/arena.h"
#include "memory/pool.h"

namespace {

struct CountingAlloc {
    yep_allocator base;
    std::atomic<int> live{0};
    std::atomic<int> total{0};
    std::atomic<int> fail_after{-1}; /* nth allocation fails (1-based); -1 = never */

    CountingAlloc() {
        base.ctx = this;
        base.alloc = [](void* ctx, size_t size) -> void* {
            CountingAlloc* self = static_cast<CountingAlloc*>(ctx);
            int n = self->total.fetch_add(1) + 1;
            if (self->fail_after.load() >= 0 && n >= self->fail_after.load()) {
                return NULL;
            }
            self->live.fetch_add(1);
            return malloc(size);
        };
        base.free = [](void* ctx, void* ptr) {
            CountingAlloc* self = static_cast<CountingAlloc*>(ctx);
            if (ptr != NULL) {
                self->live.fetch_sub(1);
            }
            free(ptr);
        };
    }
};

} // namespace

TEST(Allocator, SystemDefaultRoundTrip) {
    const yep_allocator* sys = yep_system_allocator();
    void* p = yep_alloc(sys, 128);
    ASSERT_NE(p, nullptr);
    memset(p, 0xAB, 128);
    yep_free(sys, p);
    yep_free(sys, NULL); /* accepts NULL */
}

TEST(Pool, AllocAlignsAndDistinct) {
    CountingAlloc ca;
    yep_pool* pool = yep_pool_create(&ca.base, 0);
    ASSERT_NE(pool, nullptr);

    std::vector<void*> ptrs;
    for (int i = 0; i < 1000; i++) {
        size_t size = 1 + (i * 7) % 200;
        size_t align = (i % 4 == 0) ? 64 : 16;
        void* p = yep_pool_alloc(pool, size, align);
        ASSERT_NE(p, nullptr) << "i=" << i;
        ASSERT_EQ((uintptr_t)p % align, 0u) << "alignment broken at i=" << i;
        ptrs.push_back(p);
    }
    for (size_t i = 0; i < ptrs.size(); i++) {
        for (size_t j = i + 1; j < ptrs.size(); j++) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }

    yep_pool_destroy(pool);
    EXPECT_EQ(ca.live.load(), 0) << "pool destroy leaked blocks";
}

TEST(Pool, OversizeGetsItsOwnBlock) {
    CountingAlloc ca;
    yep_pool* pool = yep_pool_create(&ca.base, 256);
    ASSERT_NE(pool, nullptr);
    ASSERT_EQ(yep_pool_block_count(pool), 1u);

    void* big = yep_pool_alloc(pool, 64 * 1024, 16);
    ASSERT_NE(big, nullptr);
    EXPECT_EQ(yep_pool_block_count(pool), 2u);

    void* small = yep_pool_alloc(pool, 16, 16); /* back in the dedicated block */
    ASSERT_NE(small, nullptr);

    yep_pool_destroy(pool);
    EXPECT_EQ(ca.live.load(), 0);
}

TEST(Pool, RejectsBadArgs) {
    CountingAlloc ca;
    yep_pool* pool = yep_pool_create(&ca.base, 0);
    ASSERT_NE(pool, nullptr);
    EXPECT_EQ(yep_pool_alloc(NULL, 8, 8), nullptr);
    EXPECT_EQ(yep_pool_alloc(pool, 0, 8), nullptr);
    EXPECT_EQ(yep_pool_alloc(pool, 8, 3), nullptr); /* not a power of two */
    EXPECT_EQ(yep_pool_alloc(pool, 8, 0), nullptr);
    yep_pool_destroy(pool);
    yep_pool_destroy(NULL); /* NULL-safe */
    EXPECT_EQ(ca.live.load(), 0);
}

TEST(Arena, HintsPreventGrowth) {
    CountingAlloc ca;
    yep_sizing_hints hints = {1024 * 1024, 0, 0, 0};
    yep_arena* arena = yep_arena_create(&ca.base, &hints);
    ASSERT_NE(arena, nullptr);

    /* Everything fits the reserved block: struct + block = 2 sys allocs. */
    for (int i = 0; i < 256; i++) {
        void* p = yep_arena_alloc(arena, 1024, 16);
        ASSERT_NE(p, nullptr) << "i=" << i;
    }
    yep_arena_stats s = yep_arena_get_stats(arena);
    EXPECT_EQ(s.blocks, 1u);
    EXPECT_EQ(s.sys_allocs, 2u) << "arena grew despite sufficient hints";

    yep_arena_destroy(arena);
    EXPECT_EQ(ca.live.load(), 0);
}

TEST(Arena, GrowthPreservesEarlierDataAndDoubles) {
    CountingAlloc ca;
    yep_sizing_hints hints = {0, 0, 0, 0}; /* 16 KiB default */
    yep_arena* arena = yep_arena_create(&ca.base, &hints);
    ASSERT_NE(arena, nullptr);

    std::vector<std::pair<void*, std::string>> saved;
    size_t capacity_before = 0;
    for (int i = 0; i < 4096; i++) {
        std::string tag = "payload-" + std::to_string(i);
        char* p = (char*)yep_arena_alloc(arena, tag.size() + 1, 16);
        ASSERT_NE(p, nullptr);
        memcpy(p, tag.c_str(), tag.size() + 1);
        saved.push_back({p, tag});
        if (i == 0) {
            capacity_before = yep_arena_get_stats(arena).capacity;
        }
    }

    yep_arena_stats s = yep_arena_get_stats(arena);
    EXPECT_GT(s.blocks, 1u) << "expected growth past the default block";
    EXPECT_GE(s.capacity - capacity_before, capacity_before)
        << "growth must at least double the initial capacity";

    for (auto& kv : saved) {
        EXPECT_STREQ((char*)kv.first, kv.second.c_str());
    }

    yep_arena_destroy(arena);
    EXPECT_EQ(ca.live.load(), 0);
}

TEST(Arena, FailureInjectionIsClean) {
    { /* arena_create fails on the struct allocation */
        CountingAlloc ca;
        ca.fail_after = 1;
        EXPECT_EQ(yep_arena_create(&ca.base, NULL), nullptr);
        EXPECT_EQ(ca.live.load(), 0);
    }
    { /* first block allocation fails */
        CountingAlloc ca;
        ca.fail_after = 2;
        EXPECT_EQ(yep_arena_create(&ca.base, NULL), nullptr);
        EXPECT_EQ(ca.live.load(), 0);
    }
    { /* growth allocation fails mid-stream: earlier data intact, no leaks */
        CountingAlloc ca;
        yep_arena* arena = yep_arena_create(&ca.base, NULL);
        ASSERT_NE(arena, nullptr);
        char* first = (char*)yep_arena_alloc(arena, 32, 16);
        ASSERT_NE(first, nullptr);
        strcpy(first, "keep me");

        ca.fail_after = ca.total.load() + 1; /* next sys alloc fails */
        size_t live_before = ca.live.load();
        EXPECT_EQ(yep_arena_alloc(arena, 8 * 1024 * 1024, 16), nullptr);
        EXPECT_STREQ(first, "keep me") << "failed alloc must not corrupt data";

        yep_arena_destroy(arena);
        EXPECT_EQ(ca.live.load(), 0);
        EXPECT_GE(live_before, 0u);
    }
}

TEST(Arena, StatsTrackUsage) {
    CountingAlloc ca;
    yep_arena* arena = yep_arena_create(&ca.base, NULL);
    ASSERT_NE(arena, nullptr);

    size_t total = 0;
    for (size_t n : {16u, 32u, 64u, 128u}) {
        ASSERT_NE(yep_arena_alloc(arena, n, 16), nullptr);
        total += n;
    }
    yep_arena_stats s = yep_arena_get_stats(arena);
    EXPECT_EQ(s.used, total);

    yep_arena_destroy(arena);
    EXPECT_EQ(ca.live.load(), 0);
}
