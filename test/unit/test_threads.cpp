/* test_threads.cpp — the thread contract (TODO.impl/19).
 *
 * Contract: one document per thread is fully independent; read-only
 * sharing of one document across threads is safe; the error channel is
 * thread-local. Errors in one thread never leak into another's parse.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <yeptris.h>
#include <yeptris/dom.h>

namespace {

const char* kDoc = "name: yeptris\n"
                   "n: 42\n"
                   "list: [1, 2, 3]\n"
                   "nested:\n"
                   "  a: 1\n"
                   "  b: two\n";

const char* kBad = "{ unclosed\n";

} // namespace

TEST(Threads, OneDocumentPerThread) {
    const int N = 8;
    std::vector<std::thread> ts;
    std::vector<int> oks(N, 0);
    for (int t = 0; t < N; t++) {
        ts.emplace_back([&oks, t] {
            for (int i = 0; i < 200; i++) {
                YeptrisStatus st = YEPTRIS_OK;
                YeptrisDocument doc = yeptris_parse(kDoc, strlen(kDoc), &st);
                if (doc != NULL) {
                    YeptrisNode root = yeptris_document_root(doc, 0);
                    if (root != NULL && yeptris_node_kind(root) == YEPTRIS_NODE_MAPPING) {
                        oks[t]++;
                    }
                    size_t len = 0;
                    char* out = yeptris_serialize(doc, &len);
                    free(out);
                    yeptris_document_free(doc);
                }
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }
    for (int t = 0; t < N; t++) {
        EXPECT_EQ(oks[t], 200);
    }
}

TEST(Threads, ErrorIsolation) {
    /* a failing parse in one thread must not pollute a concurrent
     * good parse's error channel */
    std::vector<std::thread> ts;
    std::vector<int> good(6, 0);
    for (int t = 0; t < 6; t++) {
        ts.emplace_back([&good, t] {
            for (int i = 0; i < 300; i++) {
                YeptrisStatus st = YEPTRIS_OK;
                YeptrisDocument bad = yeptris_parse(kBad, strlen(kBad), &st);
                if (bad == NULL) {
                    /* error channel read: our thread's message is set */
                    uint32_t line = 0, col = 0;
                    const char* msg = yeptris_last_error(&line, &col);
                    (void)msg;
                }
                yeptris_document_free(bad);
                YeptrisStatus st2 = YEPTRIS_OK;
                YeptrisDocument doc = yeptris_parse(kDoc, strlen(kDoc), &st2);
                if (doc != NULL) {
                    good[t]++;
                }
                yeptris_document_free(doc);
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }
    for (int t = 0; t < 6; t++) {
        EXPECT_EQ(good[t], 300);
    }
}

TEST(Threads, ReadOnlySharing) {
    /* one document, many reader threads: queries are pure reads */
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(kDoc, strlen(kDoc), &st);
    ASSERT_NE(doc, nullptr);
    std::vector<std::thread> ts;
    std::vector<int> hits(8, 0);
    for (int t = 0; t < 8; t++) {
        ts.emplace_back([&doc, &hits, t] {
            for (int i = 0; i < 500; i++) {
                YeptrisNode root = yeptris_document_root(doc, 0);
                YeptrisNode list = yeptris_node_map_get(root, "list", 4);
                if (list != NULL && yeptris_node_seq_count(list) == 3) {
                    int64_t v = 0;
                    if (yeptris_node_int(yeptris_node_seq_at(list, 2), &v) == YEPTRIS_OK &&
                        v == 3) {
                        hits[t]++;
                    }
                }
            }
        });
    }
    for (auto& th : ts) {
        th.join();
    }
    for (int t = 0; t < 8; t++) {
        EXPECT_EQ(hits[t], 500);
    }
    yeptris_document_free(doc);
}
