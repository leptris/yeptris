/* simd_text_scalar.c — the reference truth for every kernel.
 *
 * Deliberately naive: the differential suite compares the active ISA table
 * AND these functions against independently-written naive references in
 * the test, so a shared bug between ISA and scalar code cannot hide.
 */

#include <string.h>

#include "simd_text.h"

int yep_text_contains_scalar(const char* s, size_t len, char c) {
    return yep_text_find_scalar(s, len, c) >= 0;
}

ptrdiff_t yep_text_find_scalar(const char* s, size_t len, char c) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] == c) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ptrdiff_t yep_text_find3_scalar(const char* s, size_t len, char c0, char c1, char c2) {
    if (len < 3) {
        return -1;
    }
    for (size_t i = 0; i + 2 < len; i++) {
        if (s[i] == c0 && s[i + 1] == c1 && s[i + 2] == c2) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

size_t yep_text_count_char_scalar(const char* s, size_t len, char c) {
    size_t n = 0;
    for (size_t i = 0; i < len; i++) {
        n += (s[i] == c);
    }
    return n;
}

void yep_text_count3_scalar(const char* s, size_t len, char c0, char c1, char c2, size_t* n0,
                            size_t* n1, size_t* n2) {
    size_t a = 0, b = 0, d = 0;
    for (size_t i = 0; i < len; i++) {
        a += (s[i] == c0);
        b += (s[i] == c1);
        d += (s[i] == c2);
    }
    *n0 = a;
    *n1 = b;
    *n2 = d;
}

void yep_text_copy_count3_scalar(char* dst, const char* src, size_t len, char c0, char c1, char c2,
                                 size_t* n0, size_t* n1, size_t* n2) {
    /* len 0 with NULL dst (count-only callers) must not reach memcpy:
     * the nonnull attribute makes the call UB even for size 0 */
    if (len == 0) {
        *n0 = *n1 = *n2 = 0;
        return;
    }
    memcpy(dst, src, len);
    yep_text_count3_scalar(dst, len, c0, c1, c2, n0, n1, n2);
}

ptrdiff_t yep_text_find_not_scalar(const char* s, size_t len, char c) {
    for (size_t i = 0; i < len; i++) {
        if (s[i] != c) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ptrdiff_t yep_text_stopset_find_scalar(const char* s, size_t len, const unsigned char set[32]) {
    for (size_t i = 0; i < len; i++) {
        if (yep_stopset_test(set, (unsigned char)s[i])) {
            return (ptrdiff_t)i;
        }
    }
    return -1;
}

ptrdiff_t yep_text_quote_scan_scalar(const char* s, size_t len, char q, int* has_escape) {
    int esc = 0;
    for (size_t i = 0; i < len; i++) {
        if (q == '"' && s[i] == '\\') {
            esc = 1;
            i++; /* skip the escaped byte (a trailing lone backslash ends scanning) */
            continue;
        }
        if (s[i] == q) {
            if (q == '\'' && i + 1 < len && s[i + 1] == '\'') {
                esc = 1;
                i++; /* doubled quote inside a single-quoted scalar */
                continue;
            }
            if (has_escape != NULL) {
                *has_escape = esc;
            }
            return (ptrdiff_t)i;
        }
    }
    if (has_escape != NULL) {
        *has_escape = esc;
    }
    return -1;
}

const yep_text_kernels yep_text_kernels_scalar = {
    yep_text_contains_scalar,   yep_text_find_scalar,         yep_text_find3_scalar,
    yep_text_count_char_scalar, yep_text_count3_scalar,       yep_text_copy_count3_scalar,
    yep_text_find_not_scalar,   yep_text_stopset_find_scalar, yep_text_quote_scan_scalar,
};
