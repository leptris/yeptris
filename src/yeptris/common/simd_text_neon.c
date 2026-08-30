/* simd_text_neon.c — NEON kernels (TODO.impl/04).
 *
 * NEON is architectural on AArch64, so this TU needs no extra -m flags.
 * 16-byte chunks, scalar tails. Counts use UADDV over 0/1 lanes (the
 * sizing ops are the ≥8×-vs-scalar acceptance target); position queries
 * reduce through a stack movemask helper — correct first, and the perf
 * ledger records it as a refinement candidate if 06's profiles care.
 * stopset_find is deliberately scalar here too (see the AVX2 header note).
 */

#include "port.h" /* defines YEP_ARCH_* — must precede the guard below */

#if defined(YEP_ARCH_AARCH64)

#include <arm_neon.h>
#include <stdint.h>
#include <string.h>

#include "simd_text.h"

#define YEP_NEON_CHUNK 16

static inline uint8x16_t yep_neon_load(const char* p) {
    return vld1q_u8((const uint8_t*)(const void*)p);
}

static inline uint8x16_t yep_neon_eq(const char* p, uint8_t c) {
    return vceqq_u8(yep_neon_load(p), vdupq_n_u8(c));
}

/* 0x00/0xFF lanes -> per-lane bitmask (stack reduce; lanes are already
 * powers of two after the AND with the pow2 table). */
static inline uint16_t yep_neon_bits(uint8x16_t m) {
    static const uint8_t pow2[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    uint8_t tmp[16];
    vst1q_u8(tmp, vandq_u8(m, vld1q_u8(pow2)));
    uint16_t lo = (uint16_t)(tmp[0] | tmp[1] | tmp[2] | tmp[3] | tmp[4] | tmp[5] | tmp[6] | tmp[7]);
    uint16_t hi =
        (uint16_t)(tmp[8] | tmp[9] | tmp[10] | tmp[11] | tmp[12] | tmp[13] | tmp[14] | tmp[15]);
    return (uint16_t)(lo | (uint16_t)(hi << 8));
}

/* Occurrences of c in one chunk: UADDV over 0/1 lanes. */
static inline size_t yep_neon_chunk_count(const char* p, uint8_t c) {
    uint8x16_t one = vandq_u8(yep_neon_eq(p, c), vdupq_n_u8(1));
    return (size_t)vaddvq_u8(one);
}

static int yep_neon_contains(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        if (yep_neon_chunk_count(s + i, (uint8_t)c) != 0) {
            return 1;
        }
    }
    return yep_text_contains_scalar(s + i, len - i, c);
}

static ptrdiff_t yep_neon_find(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        uint16_t m = yep_neon_bits(yep_neon_eq(s + i, (uint8_t)c));
        if (m) {
            return (ptrdiff_t)(i + (size_t)__builtin_ctz(m));
        }
    }
    ptrdiff_t tail = yep_text_find_scalar(s + i, len - i, c);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static size_t yep_neon_count(const char* s, size_t len, char c) {
    size_t n = 0, i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        n += yep_neon_chunk_count(s + i, (uint8_t)c);
    }
    return n + yep_text_count_char_scalar(s + i, len - i, c);
}

static void yep_neon_count3(const char* s, size_t len, char c0, char c1, char c2, size_t* n0,
                            size_t* n1, size_t* n2) {
    size_t a = 0, b = 0, d = 0, i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        a += yep_neon_chunk_count(s + i, (uint8_t)c0);
        b += yep_neon_chunk_count(s + i, (uint8_t)c1);
        d += yep_neon_chunk_count(s + i, (uint8_t)c2);
    }
    size_t ta = 0, tb = 0, td = 0;
    yep_text_count3_scalar(s + i, len - i, c0, c1, c2, &ta, &tb, &td);
    *n0 = a + ta;
    *n1 = b + tb;
    *n2 = d + td;
}

static void yep_neon_copy_count3(char* dst, const char* src, size_t len, char c0, char c1, char c2,
                                 size_t* n0, size_t* n1, size_t* n2) {
    size_t a = 0, b = 0, d = 0, i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        uint8x16_t v = yep_neon_load(src + i);
        vst1q_u8((uint8_t*)(void*)(dst + i), v);
        a += (size_t)vaddvq_u8(vandq_u8(vceqq_u8(v, vdupq_n_u8((uint8_t)c0)), vdupq_n_u8(1)));
        b += (size_t)vaddvq_u8(vandq_u8(vceqq_u8(v, vdupq_n_u8((uint8_t)c1)), vdupq_n_u8(1)));
        d += (size_t)vaddvq_u8(vandq_u8(vceqq_u8(v, vdupq_n_u8((uint8_t)c2)), vdupq_n_u8(1)));
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

static ptrdiff_t yep_neon_find_not(const char* s, size_t len, char c) {
    size_t i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        uint16_t m = yep_neon_bits(vmvnq_u8(yep_neon_eq(s + i, (uint8_t)c)));
        if (m) {
            return (ptrdiff_t)(i + (size_t)__builtin_ctz(m));
        }
    }
    ptrdiff_t tail = yep_text_find_not_scalar(s + i, len - i, c);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static ptrdiff_t yep_neon_find3(const char* s, size_t len, char c0, char c1, char c2) {
    if (len < 3) {
        return -1;
    }
    size_t i = 0;
    for (; i + YEP_NEON_CHUNK <= len; i += YEP_NEON_CHUNK) {
        uint16_t m = yep_neon_bits(yep_neon_eq(s + i, (uint8_t)c0));
        while (m) {
            size_t k = (size_t)__builtin_ctz(m);
            size_t at = i + k;
            if (at + 2 < len && s[at + 1] == c1 && s[at + 2] == c2) {
                return (ptrdiff_t)at;
            }
            m = (uint16_t)(m & (m - 1));
        }
    }
    ptrdiff_t tail = yep_text_find3_scalar(s + i, len - i, c0, c1, c2);
    return tail < 0 ? -1 : (ptrdiff_t)i + tail;
}

static ptrdiff_t yep_neon_quote_scan(const char* s, size_t len, char q, int* has_escape) {
    int esc = 0;
    size_t i = 0;
    while (i < len) {
        int did_break = 0;
        for (; i + YEP_NEON_CHUNK <= len;) {
            uint16_t mq = yep_neon_bits(yep_neon_eq(s + i, (uint8_t)q));
            uint16_t mb = yep_neon_bits(yep_neon_eq(s + i, (uint8_t)'\\'));
            if (mq == 0 && mb == 0) {
                i += YEP_NEON_CHUNK;
                continue;
            }
            size_t kq = mq ? (size_t)__builtin_ctz(mq) : SIZE_MAX;
            size_t kb = mb ? (size_t)__builtin_ctz(mb) : SIZE_MAX;
            if (kb < kq) {
                esc = 1;
                i += kb + 2; /* skip the escaped byte */
                did_break = 1;
                break;
            }
            if (q == '\'' && i + kq + 1 < len && s[i + kq + 1] == '\'') {
                esc = 1;
                i += kq + 2; /* doubled quote inside a single-quoted scalar */
                did_break = 1;
                break;
            }
            if (has_escape != NULL) {
                *has_escape = esc;
            }
            return (ptrdiff_t)(i + kq);
        }
        if (did_break) {
            continue;
        }
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
    if (has_escape != NULL) {
        *has_escape = esc;
    }
    return -1;
}

const yep_text_kernels yep_text_kernels_neon = {
    yep_neon_contains,   yep_neon_find,
    yep_neon_find3,      yep_neon_count,
    yep_neon_count3,     yep_neon_copy_count3,
    yep_neon_find_not,   yep_text_stopset_find_scalar, /* deferred — see AVX2 header note */
    yep_neon_quote_scan,
};

#endif /* YEP_ARCH_AARCH64 */
