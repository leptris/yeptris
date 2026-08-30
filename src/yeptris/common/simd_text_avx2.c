/* simd_text_avx2.c — AVX2 kernels (TODO.impl/04).
 *
 * Compiled with -mavx2 -mpopcnt on x86 (src/CMakeLists.txt). Chunked
 * 32-byte processing with scalar tails: reads never pass len. The
 * differential suite proves equivalence with the scalar reference.
 *
 * Deferred (recorded in the item ledger): SIMD stopset_find — the runtime
 * bitmap defeats cmpeq-style classification (needs nibble-table tricks);
 * the scalar 256-bit lookup is L1-resident, and item 06's profiles will
 * decide whether the vector version earns its complexity.
 */

#include "port.h" /* defines YEP_ARCH_* — must precede the guard below */

#if defined(YEP_ARCH_X86)

#include <immintrin.h>
#include <stdint.h>
#include <string.h>

#include "simd_text.h"

#define YEP_AVX2_CHUNK 32

static inline uint32_t yep_avx2_eq_mask(const char* p, char c) {
    __m256i v = _mm256_loadu_si256((const __m256i*)(const void*)p);
    __m256i eq = _mm256_cmpeq_epi8(v, _mm256_set1_epi8(c));
    return (uint32_t)_mm256_movemask_epi8(eq);
}

static int yep_avx2_contains(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        if (yep_avx2_eq_mask(s + i, c)) {
            return 1;
        }
    }
    return yep_text_contains_scalar(s + i, len - i, c);
}

static ptrdiff_t yep_avx2_find(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        uint32_t m = yep_avx2_eq_mask(s + i, c);
        if (m) {
            return (ptrdiff_t)(i + (size_t)__builtin_ctz(m));
        }
    }
    ptrdiff_t tail = yep_text_find_scalar(s + i, len - i, c);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static size_t yep_avx2_count(const char* s, size_t len, char c) {
    size_t n = 0, i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        n += (size_t)__builtin_popcount(yep_avx2_eq_mask(s + i, c));
    }
    return n + yep_text_count_char_scalar(s + i, len - i, c);
}

static void yep_avx2_count3(const char* s, size_t len, char c0, char c1, char c2, size_t* n0,
                            size_t* n1, size_t* n2) {
    size_t a = 0, b = 0, d = 0, i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        uint32_t m0 = yep_avx2_eq_mask(s + i, c0);
        uint32_t m1 = yep_avx2_eq_mask(s + i, c1);
        uint32_t m2 = yep_avx2_eq_mask(s + i, c2);
        a += (size_t)__builtin_popcount(m0);
        b += (size_t)__builtin_popcount(m1);
        d += (size_t)__builtin_popcount(m2);
    }
    size_t ta = 0, tb = 0, td = 0;
    yep_text_count3_scalar(s + i, len - i, c0, c1, c2, &ta, &tb, &td);
    *n0 = a + ta;
    *n1 = b + tb;
    *n2 = d + td;
}

static void yep_avx2_copy_count3(char* dst, const char* src, size_t len, char c0, char c1, char c2,
                                 size_t* n0, size_t* n1, size_t* n2) {
    size_t a = 0, b = 0, d = 0, i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        __m256i v = _mm256_loadu_si256((const __m256i*)(const void*)(src + i));
        _mm256_storeu_si256((__m256i*)(void*)(dst + i), v);
        __m256i s0 = _mm256_set1_epi8(c0), s1 = _mm256_set1_epi8(c1), s2 = _mm256_set1_epi8(c2);
        a += (size_t)__builtin_popcount((uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, s0)));
        b += (size_t)__builtin_popcount((uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, s1)));
        d += (size_t)__builtin_popcount((uint32_t)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, s2)));
    }
    if (i < len) {
        memcpy(dst + i, src + i, len - i);
    }
    size_t ta = 0, tb = 0, td = 0;
    yep_text_count3_scalar(dst + i, len - i, c0, c1, c2, &ta, &tb, &td);
    *n0 = a + ta;
    *n1 = b + tb;
    *n2 = d + td;
}

static ptrdiff_t yep_avx2_find_not(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        uint32_t m = yep_avx2_eq_mask(s + i, c) ^ 0xFFFFFFFFu;
        if (m) {
            return (ptrdiff_t)(i + (size_t)__builtin_ctz(m));
        }
    }
    ptrdiff_t tail = yep_text_find_not_scalar(s + i, len - i, c);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static ptrdiff_t yep_avx2_find3(const char* s, size_t len, char c0, char c1, char c2) {
    if (len < 3) {
        return -1;
    }
    size_t i = 0;
    for (; i + YEP_AVX2_CHUNK <= len; i += YEP_AVX2_CHUNK) {
        uint32_t m = yep_avx2_eq_mask(s + i, c0);
        while (m) {
            size_t k = (size_t)__builtin_ctz(m);
            size_t at = i + k;
            if (at + 2 < len && s[at + 1] == c1 && s[at + 2] == c2) {
                return (ptrdiff_t)at;
            }
            m &= m - 1;
        }
    }
    ptrdiff_t tail = yep_text_find3_scalar(s + i, len - i, c0, c1, c2);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static ptrdiff_t yep_avx2_quote_scan(const char* s, size_t len, char q, int* has_escape) {
    int esc = 0;
    size_t i = 0;
    while (i < len) {
        for (; i + YEP_AVX2_CHUNK <= len;) {
            uint32_t m = yep_avx2_eq_mask(s + i, q) | yep_avx2_eq_mask(s + i, '\\');
            if (!m) {
                i += YEP_AVX2_CHUNK;
                continue;
            }
            size_t k = (size_t)__builtin_ctz(m);
            if (s[i + k] == '\\') {
                esc = 1;
                i += k + 2; /* skip the escaped byte */
                break;
            }
            if (has_escape != NULL) {
                *has_escape = esc;
            }
            return (ptrdiff_t)(i + k);
        }
        if (i + YEP_AVX2_CHUNK > len) {
            int tail_esc = 0;
            ptrdiff_t r = yep_text_quote_scan_scalar(s + i, len - i, q, &tail_esc);
            esc |= tail_esc;
            if (r >= 0) {
                if (has_escape != NULL) {
                    *has_escape = esc;
                }
                return (ptrdiff_t)i + r;
            }
            break;
        }
    }
    if (has_escape != NULL) {
        *has_escape = esc;
    }
    return -1;
}

const yep_text_kernels yep_text_kernels_avx2 = {
    yep_avx2_contains,   yep_avx2_find,
    yep_avx2_find3,      yep_avx2_count,
    yep_avx2_count3,     yep_avx2_copy_count3,
    yep_avx2_find_not,   yep_text_stopset_find_scalar, /* deferred — see file header */
    yep_avx2_quote_scan,
};

#endif /* YEP_ARCH_X86 */
