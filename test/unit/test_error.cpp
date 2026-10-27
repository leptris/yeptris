/* test_error.cpp — error channel: set/clear/format + thread-local
 * isolation (TODO.impl/02 Phase C). The isolation test is the TSAN gate.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "common/error.h"

TEST(Error, StringsForAllCodes) {
    for (int code = 0; code < YEP_ERR_CODE_COUNT; code++) {
        const char* s = yep_error_string((yep_err_code)code);
        ASSERT_NE(s, nullptr);
        EXPECT_GT(strlen(s), 0u) << "empty string for code " << code;
    }
    EXPECT_STREQ(yep_error_string((yep_err_code)9999), "unknown error code");
}

TEST(Error, SetClearFormat) {
    yep_error e;
    yep_error_clear(&e);
    EXPECT_TRUE(yep_error_is_none(&e));

    yep_error_set(&e, YEP_ERR_TAB_IN_INDENT, 3, 7, 42, "found tab at column %d", 7);
    EXPECT_EQ(e.code, YEP_ERR_TAB_IN_INDENT);
    EXPECT_EQ(e.line, 3u);
    EXPECT_EQ(e.col, 7u);
    EXPECT_EQ(e.offset, 42u);
    EXPECT_STREQ(e.msg, "found tab at column 7");

    yep_error_clear(&e);
    EXPECT_TRUE(yep_error_is_none(&e));
    EXPECT_EQ(e.code, YEP_ERR_NONE);
}

TEST(Error, NullFormatUsesCodeString) {
    yep_error e;
    yep_error_set(&e, YEP_ERR_UNTERMINATED_QUOTE, 1, 1, 0, NULL);
    EXPECT_STREQ(e.msg, yep_error_string(YEP_ERR_UNTERMINATED_QUOTE));
}

TEST(Error, MessageTruncatesSafely) {
    yep_error e;
    std::string long_msg(YEP_ERROR_MSG_MAX * 3, 'x');
    yep_error_set(&e, YEP_ERR_BAD_INDENT, 1, 1, 0, "%s", long_msg.c_str());
    EXPECT_LT(strlen(e.msg), sizeof(e.msg));
    /* Truncated to a prefix of the original. */
    EXPECT_EQ(strncmp(e.msg, long_msg.c_str(), strlen(e.msg)), 0);
}

TEST(Error, ThreadLocalIsolation) {
    yep_error_clear(yep_error_tls());

    std::atomic<int> ready{0};
    std::atomic<bool> go{false};
    std::atomic<int> failures{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&, t] {
            yep_error_clear(yep_error_tls());
            ready.fetch_add(1);
            while (!go.load()) {
                std::this_thread::yield();
            }
            /* Each thread hammers its own slot with its own code. */
            for (int i = 0; i < 2000; i++) {
                yep_error_set(yep_error_tls(), (yep_err_code)(YEP_ERR_UNEXPECTED + t), t + 1, i, i,
                              "thread %d iter %d", t, i);
            }
            yep_error* slot = yep_error_tls();
            if (slot->code != (yep_err_code)(YEP_ERR_UNEXPECTED + t) ||
                slot->line != (uint32_t)(t + 1)) {
                failures.fetch_add(1);
            }
        });
    }

    while (ready.load() < 4) {
        std::this_thread::yield();
    }
    go.store(true);
    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(failures.load(), 0);
    EXPECT_EQ(yep_error_tls()->code, YEP_ERR_NONE) << "main thread slot untouched by workers";
}
