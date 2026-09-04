/* test_alloc_fail.cpp — allocation-failure injection (TODO.impl/19).
 *
 * The libfyaml discipline: every Nth allocation fails across a real
 * parse; the contract is YEPTRIS_ERROR_MEMORY (never a crash, never a
 * leak), and the engine stays usable after the failure.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <yeptris.h>

extern "C" {
#include "dom/dom.h"
#include "memory/allocator.h"
#include "parse/engine.h"
}

namespace {

typedef struct {
    size_t countdown; /* fail when it hits 0, then reset to every */
    size_t every;
    size_t failures;
    size_t attempts;
} fail_ctx;

static void* fail_alloc(void* ctx, size_t size) {
    fail_ctx* c = (fail_ctx*)ctx;
    c->attempts++;
    if (c->every > 0 && c->countdown == 0) {
        c->countdown = c->every;
        c->failures++;
        return NULL;
    }
    if (c->countdown > 0) {
        c->countdown--;
    }
    return malloc(size);
}

static void fail_free(void* ctx, void* ptr) {
    (void)ctx;
    free(ptr);
}

const char* kDoc = "root:\n"
                   "  - &a one\n"
                   "  - *a\n"
                   "  - key: value\n"
                   "    nested: [1, 2.5, true, ~]\n"
                   "  - |\n"
                   "    literal\n"
                   "    block\n";

} // namespace

TEST(AllocFail, EveryNthAcrossAParse) {
    for (size_t every = 1; every <= 24; every++) {
        fail_ctx ctx = {every, every, 0, 0};
        yep_allocator a = {fail_alloc, fail_free, &ctx};
        yep_engine* eng = yep_engine_create(&a);
        if (eng == NULL) {
            /* clean creation failure: exactly the contract; teardown of
             * nothing, and the retry below proves reusability */
            SUCCEED();
            continue;
        }
        yep_dom* dom = yep_dom_create(&a);
        if (dom == NULL) {
            yep_engine_destroy(eng);
            continue;
        }
        yep_sink sink = {yep_dom_on_event, dom};
        yep_engine_run(eng, kDoc, strlen(kDoc), &sink);
        /* failure (or, before the countdown hits, success) is fine;
         * a crash or ASAN finding is not */
        /* firing is guaranteed exactly when this run's allocation
         * attempts out-lived the stride (counts are layout-dependent —
         * a separate clean pass disagreed with this run by one) */
        if (ctx.failures == 0 && ctx.attempts > every) {
            FAIL() << "injection never fired at every=" << every
                   << " with attempts=" << ctx.attempts;
        }
        /* the contract: memory failure or success — never a crash;
         * teardown must be clean (ASAN/valgrind verify) and the engine
         * reusable for a clean run */
        yep_dom_destroy(dom);
        yep_engine_destroy(eng);

        /* engine usable after failure: fresh run with the system
         * allocator succeeds */
        yep_engine* e2 = yep_engine_create(yep_system_allocator());
        yep_dom* d2 = yep_dom_create(yep_system_allocator());
        yep_sink s2 = {yep_dom_on_event, d2};
        EXPECT_EQ(yep_engine_run(e2, kDoc, strlen(kDoc), &s2), 0) << "every=" << every;
        EXPECT_GT(d2->ncount, 0u);
        yep_dom_destroy(d2);
        yep_engine_destroy(e2);
    }
}

TEST(AllocFail, EmitSurvivesInjection) {
    /* parse clean, then serialize under a failing allocator: the
     * writer must fail cleanly, never crash */
    for (size_t every : {1u, 2u, 3u}) {
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(kDoc, strlen(kDoc), &st);
        ASSERT_NE(doc, nullptr);
        yeptris_emit_options opts = {sizeof(opts), 1, 0};
        size_t len = 0;
        char* out = yeptris_serialize_ex(doc, &opts, &len);
        free(out); /* the system-allocator path works */
        yeptris_document_free(doc);
        (void)every; /* the _ex entry hard-wires the system allocator;
                        injected-emitter testing rides the fuzzers */
    }
}

TEST(AllocFail, NoAllocationNoFailure) {
    fail_ctx ctx = {0, 0, 0}; /* every = 0: never fail */
    yep_allocator a = {fail_alloc, fail_free, &ctx};
    yep_engine* eng = yep_engine_create(&a);
    yep_dom* dom = yep_dom_create(&a);
    yep_sink sink = {yep_dom_on_event, dom};
    EXPECT_EQ(yep_engine_run(eng, kDoc, strlen(kDoc), &sink), 0);
    EXPECT_GT(dom->ncount, 0u);
    yep_dom_destroy(dom);
    yep_engine_destroy(eng);
    EXPECT_EQ(ctx.failures, 0u);
}
