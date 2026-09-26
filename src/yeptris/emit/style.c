/* style.c — the style rule table (TODO.impl/13). */

#include "style.h"

#include <string.h>

static int is_blank(char c) {
    return c == ' ' || c == '\t';
}

static int is_break(char c) {
    return c == '\n' || c == '\r';
}

int yep_style_plain_safe(const char* p, uint32_t len) {
    if (len == 0) {
        return 0; /* empty needs quotes or an empty-value slot */
    }
    if (is_blank(p[0]) || is_blank(p[len - 1])) {
        return 0;
    }
    switch (p[0]) {
    case ':':
        /* libyaml (Psych symbol dumps ":name"): a leading colon only
         * indicates when followed by a blank/EOL — the interior rule
         * below covers it; ":name" stays plain */
        break;
    case '-':
    case '?':
        /* indicators only before a blank or EOL: "-5" and "?x" are
         * plain (libyaml emits negative numbers plain; Psych, too) —
         * but fall through: the document-marker check below owns
         * the bare "---"/"..." (libyaml quotes those) */
        if (len == 1 || is_blank(p[1])) {
            return 0;
        }
        break;
    case ',':
    case '[':
    case ']':
    case '{':
    case '}':
    case '#':
    case '&':
    case '*':
    case '!':
    case '|':
    case '>':
    case '\'':
    case '"':
    case '%':
    case '@':
    case '`':
        return 0;
    default:
        break;
    }
    /* The interior scan in SWAR words: every reject condition is a
     * byte-class adjacency, so one masked word test is exact —
     * break/tab bytes reject directly; ':' rejects with a blank at its
     * NEXT lane (or the span's EOL); '#' with a blank at its PREVIOUS
     * lane (the span's first byte is exempt — the head switch already
     * handled a leading '#'). Cross-word adjacency rides one carried
     * byte per boundary; the tail runs the byte loop, whose rules read
     * real neighbors and need no carries. */
    {
        const uint64_t ONES = 0x0101010101010101ull;
        const uint64_t HIGH = 0x8080808080808080ull;
#define YST_ZDET(w, ch)                                                                            \
    ((((w) ^ (uint64_t)(ch) * ONES) - ONES) & ~((w) ^ (uint64_t)(ch) * ONES) & HIGH)
        uint32_t i = 0;
        int prev_blank7 = 0;
        while (i + 8 <= len) {
            uint64_t w;
            memcpy(&w, p + i, 8);
            uint64_t space = YST_ZDET(w, ' ');
            uint64_t tab = YST_ZDET(w, '\t');
            uint64_t blank = space | tab;
            uint64_t colon = YST_ZDET(w, ':');
            uint64_t hash = YST_ZDET(w, '#');
            /* tab and the breaks reject outright */
            if (tab | YST_ZDET(w, '\n') | YST_ZDET(w, '\r')) {
                return 0;
            }
            /* ':' + blank next: (blank >> 8) pulls lane i+1's flag down
             * into lane i; the word's last lane takes the peeked byte
             * (EOL at the span end counts as blank — the rule rejects
             * there) */
            if (colon) {
                int next_blank = i + 8 < len ? is_blank(p[i + 8]) : 1;
                if (colon & ((blank >> 8) | (next_blank ? (uint64_t)0x80 << 56 : 0))) {
                    return 0;
                }
            }
            /* '#' + blank prev: (blank << 8) lifts lane i-1's flag up
             * into lane i; lane 0 takes the carried last-lane flag */
            if (hash && (hash & ((blank << 8) | (prev_blank7 ? 0x80 : 0)))) {
                return 0;
            }
            prev_blank7 = (int)((blank >> 63) & 1); /* lane 7's flag is its top bit */
            i += 8;
        }
#undef YST_ZDET
        for (; i < len; i++) {
            char c = p[i];
            if (is_break(c) || c == '\t') {
                return 0;
            }
            if (c == ':') {
                if (i + 1 >= len || is_blank(p[i + 1])) {
                    return 0;
                }
            }
            if (c == '#') {
                if (i > 0 && is_blank(p[i - 1])) {
                    return 0;
                }
            }
        }
    }
    /* document markers */
    if ((len == 3 && (memcmp(p, "---", 3) == 0 || memcmp(p, "...", 3) == 0))) {
        return 0;
    }
    return 1;
}

int yep_style_plain_key_safe(const char* p, uint32_t len) {
    /* plain_safe's colon rule is the key rule too: a colon only ends a
     * key when followed by a blank/EOL, so "a:b" and ":name" are plain
     * keys (libyaml emits Psych's symbol dumps exactly this way) */
    return yep_style_plain_safe(p, len);
}
