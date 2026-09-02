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
        /* "-?" only indicate when followed by a blank; a safe subset
         * still avoids them as FIRST bytes for re-emit simplicity */
        return 0;
    default:
        break;
    }
    for (uint32_t i = 0; i < len; i++) {
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
