/* utf8_validate.c — well-formedness check with a SWAR ASCII fast path
 * (TODO.impl/05). The SIMD kernel is deferred until item 06's profiles
 * justify it; the SWAR path already runs multi-GB/s on pure ASCII.
 *
 * Rules enforced (Unicode 15 well-formedness): no overlong encodings, no
 * UTF-16 surrogate code points (U+D800..U+DFFF), nothing above U+10FFFF,
 * continuation bytes only where a sequence expects them.
 *
 * yep_printable_validate additionally enforces the YAML 1.2 c-printable
 * charset on the stream (the charset SSOT lives here): raw C0 controls
 * except TAB/LF/CR, DEL, C1 except NEL (U+0085), and U+FFFE/U+FFFF are
 * rejected wherever they appear. Escape sequences are decoded later —
 * printable applies to source bytes only.
 */

#include <stdint.h>
#include <string.h>

#include "encoding.h"

static int ascii_ok(unsigned char b, int printable) {
    if (!printable) {
        return 1;
    }
    /* c-printable ASCII: TAB, LF, CR, 0x20..0x7E */
    if (b == '\t' || b == '\n' || b == '\r') {
        return 1;
    }
    return b >= 0x20 && b <= 0x7E;
}

/* Code point >= 0x80 allowed by c-printable? (surrogates are already
 * rejected by the well-formedness rules above). */
static int cp_printable(uint32_t cp) {
    return cp == 0x85 || (cp >= 0xA0 && cp <= 0xFFFD) || cp >= 0x10000;
}

static int validate(const unsigned char* p, size_t len, size_t* err_pos, int printable) {
    if (err_pos != NULL) {
        *err_pos = 0;
    }
    if (p == NULL && len != 0) {
        return 0;
    }

    size_t i = 0;

    /* ASCII fast path: 8 bytes at a time while the high bits are clear.
     * Printable mode also rejects C0/DEL: one SWAR test per chunk, with
     * a per-byte fallback only for chunks that contain one. */
    while (i + 8 <= len) {
        uint64_t chunk;
        memcpy(&chunk, p + i, 8);
        if ((chunk & 0x8080808080808080ULL) != 0) {
            break;
        }
        if (printable) {
            /* bytes < 0x20, or 0x7F: ((x - 0x20s) borrows) | (x == 0x7F) */
            uint64_t below = (chunk - 0x2020202020202020ULL) & 0x8080808080808080ULL & ~chunk &
                             0x8080808080808080ULL;
            uint64_t del = ~chunk & (chunk ^ 0x7F7F7F7F7F7F7F7FULL) & 0x8080808080808080ULL;
            uint64_t bad = below | del;
            if (bad != 0) {
                /* find the first flagged byte, confirm it is not 09/0A/0D */
                for (size_t k = 0; k < 8; k++) {
                    unsigned char b = p[i + k];
                    if (!ascii_ok(b, printable)) {
                        if (err_pos != NULL) {
                            *err_pos = i + k;
                        }
                        return 0;
                    }
                }
            }
        }
        i += 8;
    }

    while (i < len) {
        unsigned char b = p[i];
        if (b < 0x80) {
            if (!ascii_ok(b, printable)) {
                if (err_pos != NULL) {
                    *err_pos = i;
                }
                return 0;
            }
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
        if (printable) {
            uint32_t cp;
            if (need == 1) {
                cp = ((uint32_t)(b & 0x1F) << 6) | (uint32_t)(c1 & 0x3F);
            } else if (need == 2) {
                cp = ((uint32_t)(b & 0x0F) << 12) | ((uint32_t)(c1 & 0x3F) << 6) |
                     (uint32_t)(p[i + 2] & 0x3F);
            } else {
                cp = ((uint32_t)(b & 0x07) << 18) | ((uint32_t)(c1 & 0x3F) << 12) |
                     ((uint32_t)(p[i + 2] & 0x3F) << 6) | (uint32_t)(p[i + 3] & 0x3F);
            }
            if (!cp_printable(cp)) {
                if (err_pos != NULL) {
                    *err_pos = i;
                }
                return 0;
            }
        }
        i += 1 + need;
    }
    return 1;
}

int yep_utf8_validate(const unsigned char* p, size_t len, size_t* err_pos) {
    return validate(p, len, err_pos, 0);
}

int yep_printable_validate(const unsigned char* p, size_t len, size_t* err_pos) {
    return validate(p, len, err_pos, 1);
}
