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

static int tag_is(const char* p, uint32_t n, const char* s) {
    uint32_t m = 0;
    while (s[m] != '\0') {
        m++;
    }
    return n == m && (n == 0 || memcmp(p, s, n) == 0);
}

static yep_tag_id core12(void* ctx, const char* p, uint32_t n) {
    (void)ctx;
    if (n == 0 || tag_is(p, n, "~") || tag_is(p, n, "null") || tag_is(p, n, "Null") ||
        tag_is(p, n, "NULL")) {
        return 4; /* null */
    }
    if (tag_is(p, n, "true") || tag_is(p, n, "True") || tag_is(p, n, "TRUE") ||
        tag_is(p, n, "false") || tag_is(p, n, "False") || tag_is(p, n, "FALSE")) {
        return 3; /* bool */
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
        if (rn > 1 && r[0] == '.') {
            int inf = tag_is(r, rn, ".inf") || tag_is(r, rn, ".Inf") || tag_is(r, rn, ".INF");
            int nan = tag_is(r, rn, ".nan") || tag_is(r, rn, ".NaN") || tag_is(r, rn, ".NAN");
            if (inf || (nan && i == 0)) { /* NaN carries no sign */
                return 2;                 /* float */
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
