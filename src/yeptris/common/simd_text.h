/* simd_text.h — SIMD-accelerated text scan primitives (TODO.impl/04).
 *
 * AOT framework (the libleptris pattern): a scalar reference TU is always
 * compiled; AVX2 / NEON TUs are compiled separately with the right -m
 * flags; dispatch picks the best table once via cpu detection. Callers go
 * through yep_text_active() — never call an ISA table directly.
 *
 * Contract (every implementation, every ISA):
 *  - functions read AT MOST len bytes; no faults past the end
 *    (length-guarded chunking, never full-width loads beyond len);
 *  - len == 0 is valid: contains/count/find_not-style predicates return
 *    their empty answer, finds return -1;
 *  - results are bit-identical to the scalar reference — enforced by the
 *    differential suite (test_simd_text.cpp), which compares against naive
 *    test-local references, not against our own scalar TU.
 */
#ifndef YEP_SIMD_TEXT_H
#define YEP_SIMD_TEXT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The kernel table. One struct = one dispatch point (OCP: a new ISA is a
 * new TU exporting a new table; nothing else changes). */
typedef struct yep_text_kernels {
    /* nonzero iff c occurs in [s, s+len) */
    int (*contains)(const char* s, size_t len, char c);

    /* offset of the first c, or -1 */
    ptrdiff_t (*find)(const char* s, size_t len, char c);

    /* offset of the first c0c1c2 run, or -1; len < 3 -> -1 */
    ptrdiff_t (*find3)(const char* s, size_t len, char c0, char c1, char c2);

    /* occurrences of c */
    size_t (*count_char)(const char* s, size_t len, char c);

    /* three counts in one memory pass (arena sizing pre-scan, 06) */
    void (*count3)(const char* s, size_t len, char c0, char c1, char c2, size_t* n0, size_t* n1,
                   size_t* n2);

    /* memcpy + count3 fused: one traversal (TODO 188 pattern) */
    void (*copy_count3)(char* dst, const char* src, size_t len, char c0, char c1, char c2,
                        size_t* n0, size_t* n1, size_t* n2);

    /* offset of the first byte != c, or -1 (indentation column scan) */
    ptrdiff_t (*find_not)(const char* s, size_t len, char c);

    /* offset of the first byte in the 256-bit bitmap set, or -1
     * (plain-scalar end detection: ": " / " #" / line-break / flow stops) */
    ptrdiff_t (*stopset_find)(const char* s, size_t len, const unsigned char set[32]);

    /* Given the content AFTER an opening quote: offset of the closing
     * quote q honoring backslash escapes, or -1 if unterminated.
     * *has_escape is set to 1 iff a backslash occurred (0 otherwise). */
    ptrdiff_t (*quote_scan)(const char* s, size_t len, char q, int* has_escape);
} yep_text_kernels;

/* The best table for this CPU (atomic-lazy, like yep_cpu_detect). */
const yep_text_kernels* yep_text_active(void);

/* Scalar reference implementations — callable directly for differential
 * testing and as fallbacks. The active() table never points here on CPUs
 * with a vector ISA we compiled. */
int yep_text_contains_scalar(const char* s, size_t len, char c);
ptrdiff_t yep_text_find_scalar(const char* s, size_t len, char c);
ptrdiff_t yep_text_find3_scalar(const char* s, size_t len, char c0, char c1, char c2);
size_t yep_text_count_char_scalar(const char* s, size_t len, char c);
void yep_text_count3_scalar(const char* s, size_t len, char c0, char c1, char c2, size_t* n0,
                            size_t* n1, size_t* n2);
void yep_text_copy_count3_scalar(char* dst, const char* src, size_t len, char c0, char c1, char c2,
                                 size_t* n0, size_t* n1, size_t* n2);
ptrdiff_t yep_text_find_not_scalar(const char* s, size_t len, char c);
ptrdiff_t yep_text_stopset_find_scalar(const char* s, size_t len, const unsigned char set[32]);
ptrdiff_t yep_text_quote_scan_scalar(const char* s, size_t len, char q, int* has_escape);

/* Stopset bitmap helpers: a 256-bit bitmap is the wire form of a byte
 * class; build with yep_stopset_clear + yep_stopset_add. */
static inline void yep_stopset_clear(unsigned char set[32]) {
    for (int i = 0; i < 32; i++) {
        set[i] = 0;
    }
}

static inline void yep_stopset_add(unsigned char set[32], unsigned char c) {
    set[c >> 3] |= (unsigned char)(1u << (c & 7));
}

static inline int yep_stopset_test(const unsigned char set[32], unsigned char c) {
    return (set[c >> 3] >> (c & 7)) & 1;
}

#ifdef __cplusplus
}
#endif

#endif /* YEP_SIMD_TEXT_H */
