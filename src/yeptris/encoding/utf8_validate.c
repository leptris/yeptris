/* utf8_validate.c — well-formedness check with a SWAR ASCII fast path
 * (TODO.impl/05). The SIMD kernel is deferred until item 06's profiles
 * justify it; the SWAR path already runs multi-GB/s on pure ASCII.
 *
 * Rules enforced (Unicode 15 well-formedness): no overlong encodings, no
 * UTF-16 surrogate code points (U+D800..U+DFFF), nothing above U+10FFFF,
 * continuation bytes only where a sequence expects them.
 */

#include <stdint.h>
#include <string.h>

#include "encoding.h"

int yep_utf8_validate(const unsigned char* p, size_t len, size_t* err_pos) {
    if (err_pos != NULL) {
        *err_pos = 0;
    }
    if (p == NULL && len != 0) {
        return 0;
    }

    size_t i = 0;

    /* ASCII fast path: 8 bytes at a time while the high bits are clear. */
    while (i + 8 <= len) {
        uint64_t chunk;
        memcpy(&chunk, p + i, 8);
        if ((chunk & 0x8080808080808080ULL) != 0) {
            break;
        }
        i += 8;
    }

    while (i < len) {
        unsigned char b = p[i];
        if (b < 0x80) {
            i++;
            continue;
        }

        size_t need;          /* continuation bytes expected */
        unsigned char lo, hi; /* allowed range of the first continuation */
        if (b >= 0xC2 && b <= 0xDF) {
            need = 1;
            lo = 0x80;
            hi = 0xBF;
        } else if (b == 0xE0) {
            need = 2;
            lo = 0xA0; /* no overlong two-byte */
            hi = 0xBF;
        } else if ((b >= 0xE1 && b <= 0xEC) || b == 0xEE || b == 0xEF) {
            need = 2;
            lo = 0x80;
            hi = 0xBF;
        } else if (b == 0xED) {
            need = 2;
            lo = 0x80;
            hi = 0x9F; /* no surrogates */
        } else if (b == 0xF0) {
            need = 3;
            lo = 0x90; /* no overlong three-byte */
            hi = 0xBF;
        } else if (b >= 0xF1 && b <= 0xF3) {
            need = 3;
            lo = 0x80;
            hi = 0xBF;
        } else if (b == 0xF4) {
            need = 3;
            lo = 0x80;
            hi = 0x8F; /* no > U+10FFFF */
        } else {
            if (err_pos != NULL) {
                *err_pos = i;
            }
            return 0;
        }

        /* The sequence spans [i, i + need]; all those bytes must exist. */
        if (i + need >= len) {
            if (err_pos != NULL) {
                *err_pos = i;
            }
            return 0;
        }

        unsigned char c1 = p[i + 1];
        if (c1 < lo || c1 > hi) {
            if (err_pos != NULL) {
                *err_pos = i + 1;
            }
            return 0;
        }
        for (size_t k = 2; k <= need; k++) {
            if (p[i + k] < 0x80 || p[i + k] > 0xBF) {
                if (err_pos != NULL) {
                    *err_pos = i + k;
                }
                return 0;
            }
        }
        i += 1 + need;
    }
    return 1;
}
