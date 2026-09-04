/* core12.c — the YAML 1.2 core schema (TODO.impl/10).
 *
 * null: "", "~", "null", "Null", "NULL"
 * bool: true/True/TRUE, false/False/FALSE
 * int:  [-+]?[0-9]+ | [-+]?0o[0-7]+ | [-+]?0x[0-9a-fA-F]+
 * float: core float production, [-+]?(\.inf|\.Inf|\.INF), \.nan/\.NaN/\.NAN
 * else str
 * One pass, no allocations; spec-table vectors in test_resolve.cpp.
 */

#include <stdint.h>
#include <string.h>

#include "resolver.h"

/* Word-class checks use constant-size memcmp only: a per-word strlen
 * loop (tag_is) cost measurable percent of scalar-heavy parse — the
 * resolver runs once per scalar event. */
#define TAG_IS4(p, a, b, c) (memcmp(p, a, 4) == 0 || memcmp(p, b, 4) == 0 || memcmp(p, c, 4) == 0)
#define TAG_IS5(p, a, b, c) (memcmp(p, a, 5) == 0 || memcmp(p, b, 5) == 0 || memcmp(p, c, 5) == 0)

static yep_tag_id core12(void* ctx, const char* p, uint32_t n) {
    (void)ctx;
    if (n == 0) {
        return 4; /* null */
    }
    if (n == 1) {
        if (p[0] == '~') {
            return 4; /* null */
        }
    } else if (n == 4) {
        if (TAG_IS4(p, "null", "Null", "NULL")) {
            return 4; /* null */
        }
        if (TAG_IS4(p, "true", "True", "TRUE")) {
            return 3; /* bool */
        }
    } else if (n == 5) {
        if (TAG_IS5(p, "false", "False", "FALSE")) {
            return 3; /* bool */
        }
    }
    uint32_t i = 0;
    if (p[0] == '-' || p[0] == '+') {
        i = 1;
    }
    if (i >= n) {
        return 0; /* lone sign */
    }
    /* .inf / .nan family (sign allowed on inf) */
    {
        const char* r = p + i;
        uint32_t rn = n - i;
        if (rn == 4 && r[0] == '.') {
            if (TAG_IS4(r, ".inf", ".Inf", ".INF") ||
                (i == 0 && TAG_IS4(r, ".nan", ".NaN", ".NAN"))) {
                return 2; /* float */ /* NaN carries no sign */
            }
        }
    }
    /* 0x hex */
    if (n - i > 2 && p[i] == '0' && (p[i + 1] == 'x' || p[i + 1] == 'X')) {
        for (uint32_t k = i + 2; k < n; k++) {
            char c = p[k];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                return 0;
            }
        }
        return 1; /* int */
    }
    /* 0o octal */
    if (n - i > 2 && p[i] == '0' && p[i + 1] == 'o') {
        for (uint32_t k = i + 2; k < n; k++) {
            if (p[k] < '0' || p[k] > '7') {
                return 0;
            }
        }
        return 1; /* int */
    }
    /* decimal int / float */
    int digits = 0, dot = 0, e = 0, edigits = 0;
    for (uint32_t k = i; k < n; k++) {
        char c = p[k];
        if (c >= '0' && c <= '9') {
            if (e) {
                edigits = 1;
            } else {
                digits = 1;
            }
            continue;
        }
        if (c == '.' && !e) {
            if (dot) {
                return 0;
            }
            dot = 1;
            continue;
        }
        if ((c == 'e' || c == 'E') && digits && !e) {
            e = 1;
            if (k + 1 < n && (p[k + 1] == '-' || p[k + 1] == '+')) {
                k++;
            }
            continue;
        }
        return 0;
    }
    if (digits && (dot || e) && (!e || edigits)) {
        return 2; /* float */
    }
    if (digits && !dot && !e) {
        return 1; /* int */
    }
    return 0; /* str */
}

static const yep_resolver k_core12 = {core12, NULL};

const yep_resolver* yep_resolver_core12(void) {
    return &k_core12;
}
