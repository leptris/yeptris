/* json.c — the strict-JSON grammar (RFC 8259), TODO.impl/08C/21.
 *
 * MECE: this is the ONLY place that decides what strict JSON is —
 * the engine's flow fast path (parse/engine.c) and the whole-input
 * JSON mode (yeptris_parse_json) both consume these primitives; the
 * grammar never diverges between them.
 */

#include <stdint.h>
#include <string.h>

#include "common/chartype.h"
#include "common/simd_text.h"

#include "parse/scalars.h"
#include "scan/json.h"

/* ---- whole-document strict validation (JSON mode) ----
 *
 * optional ws, one JSON value, optional ws, EOF. err (may be NULL)
 * receives the offset of the first violation. Depth is bounded by the
 * shared engine limit; exceeding it fails validation like any other
 * violation. */

#define JX_MAX_DEPTH 256

enum { JV_VALUE_OR_CLOSE = 0, JV_VALUE, JV_KEY_OR_CLOSE, JV_KEY, JV_COLON, JV_COMMA_OR_CLOSE };

static int jv_ws(const char* p, size_t len, size_t* i) {
    while (*i < len) {
        char c = p[*i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            (*i)++;
            continue;
        }
        return 1;
    }
    return 0; /* EOF */
}

int yep_json_document(const char* p, size_t len, size_t* err) {
    size_t i = 0;
    if (err != NULL) {
        *err = 0;
    }
    if (!jv_ws(p, len, &i)) {
        if (err != NULL) {
            *err = i;
        }
        return 0; /* empty is not a JSON document */
    }
    uint8_t kind[JX_MAX_DEPTH];
    uint8_t expect[JX_MAX_DEPTH];
    int depth = 0;
    /* virtual root frame: expects exactly one value */
    kind[0] = 0;
    expect[0] = JV_VALUE;
    depth = 1;
    int root_done = 0;
    for (;;) {
        if (!jv_ws(p, len, &i)) {
            if (err != NULL) {
                *err = i;
            }
            return 0; /* EOF inside a value */
        }
        char c = p[i];
        if (c == ']' || c == '}') {
            int want = (c == ']') ? 0 : 1;
            if (depth == 1 || kind[depth - 1] != want ||
                (expect[depth - 1] != JV_VALUE_OR_CLOSE && expect[depth - 1] != JV_KEY_OR_CLOSE &&
                 expect[depth - 1] != JV_COMMA_OR_CLOSE)) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
            depth--;
            if (depth == 1) {
                /* closed the root collection */
                root_done = 1;
                i++;
                break;
            }
            expect[depth - 1] = JV_COMMA_OR_CLOSE;
            i++;
            continue;
        }
        if (root_done) {
            if (err != NULL) {
                *err = i;
            }
            return 0; /* trailing content */
        }
        switch (expect[depth - 1]) {
        case JV_COLON:
            if (c != ':') {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
            expect[depth - 1] = JV_VALUE;
            i++;
            continue;
        case JV_COMMA_OR_CLOSE:
            if (c != ',') {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
            expect[depth - 1] = kind[depth - 1] ? JV_KEY : JV_VALUE;
            i++;
            continue;
        default:
            break;
        }
        if (c == '{' || c == '[') {
            if (depth >= JX_MAX_DEPTH) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
            kind[depth] = (c == '[') ? 0 : 1;
            expect[depth] = kind[depth] ? JV_KEY_OR_CLOSE : JV_VALUE_OR_CLOSE;
            depth++;
            i++;
            continue;
        }
        int he = 0;
        size_t close;
        if (c == '"') {
            if (!yep_json_string(p, len, &i, &close, &he)) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
        } else if (c == '-' || (c >= '0' && c <= '9')) {
            if (!yep_json_number(p, len, &i)) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
        } else if (c == 't') {
            if (!yep_json_literal(p, len, &i, "true")) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
        } else if (c == 'f') {
            if (!yep_json_literal(p, len, &i, "false")) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
        } else if (c == 'n') {
            if (!yep_json_literal(p, len, &i, "null")) {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
        } else {
            if (err != NULL) {
                *err = i;
            }
            return 0;
        }
        if (depth == 1) {
            root_done = 1;
            break; /* scalar root */
        }
        if (kind[depth - 1] == 1 && expect[depth - 1] == JV_KEY_OR_CLOSE) {
            if (c != '"') {
                if (err != NULL) {
                    *err = i;
                }
                return 0; /* JSON map keys are strings only */
            }
            expect[depth - 1] = JV_COLON;
        } else if (kind[depth - 1] == 1 && expect[depth - 1] == JV_KEY) {
            if (c != '"') {
                if (err != NULL) {
                    *err = i;
                }
                return 0;
            }
            expect[depth - 1] = JV_COLON;
        } else {
            expect[depth - 1] = JV_COMMA_OR_CLOSE;
        }
    }
    /* trailing whitespace only, and then the input must end */
    while (i < len) {
        char c = p[i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            i++;
            continue;
        }
        if (err != NULL) {
            *err = i;
        }
        return 0;
    }
    return 1;
}

/* ---- token primitives (shared with the engine fast path) ---- */

int yep_json_ws(const char* p, size_t len, size_t* i, int* saw_tab) {
    while (*i < len) {
        char c = p[*i];
        if (c == ' ' || c == '\n' || c == '\r') {
            (*i)++;
            continue;
        }
        if (c == '\t') {
            *saw_tab = 1; /* tabs: let the general kernel rule on them */
            return 0;
        }
        return 1; /* token byte */
    }
    return -1; /* EOF */
}

/* Strict RFC 8259 number: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][-+]?[0-9]+)? */
int yep_json_number(const char* p, size_t len, size_t* i) {
    size_t k = *i;
    if (p[k] == '-') {
        k++;
    }
    if (k >= len || p[k] < '0' || p[k] > '9') {
        return 0;
    }
    if (p[k] == '0') {
        k++;
    } else {
        while (k < len && p[k] >= '0' && p[k] <= '9') {
            k++;
        }
    }
    if (k < len && p[k] == '.') {
        k++;
        if (k >= len || p[k] < '0' || p[k] > '9') {
            return 0;
        }
        while (k < len && p[k] >= '0' && p[k] <= '9') {
            k++;
        }
    }
    if (k < len && (p[k] == 'e' || p[k] == 'E')) {
        k++;
        if (k < len && (p[k] == '-' || p[k] == '+')) {
            k++;
        }
        if (k >= len || p[k] < '0' || p[k] > '9') {
            return 0;
        }
        while (k < len && p[k] >= '0' && p[k] <= '9') {
            k++;
        }
    }
    if (k < len) {
        char c = p[k];
        if (c != ' ' && c != '\n' && c != '\r' && c != ',' && c != ']' && c != '}' && c != ':') {
            return 0; /* "1x" is YAML, not JSON */
        }
    }
    *i = k;
    return 1;
}

int yep_json_literal(const char* p, size_t len, size_t* i, const char* word) {
    size_t w = 0;
    while (word[w] != '\0') {
        if (*i + w >= len || p[*i + w] != word[w]) {
            return 0;
        }
        w++;
    }
    *i += w;
    if (*i < len) {
        char c = p[*i];
        if (c != ' ' && c != '\n' && c != '\r' && c != ',' && c != ']' && c != '}' && c != ':') {
            return 0; /* "truex" is a YAML plain, not JSON */
        }
    }
    return 1;
}

/* Strict-JSON quoted scalar: no raw breaks, only JSON escapes. One
 * stopset walk {'"', '\\', '\n', '\r'} decides close/escape/break —
 * no second scan. On success *i sits just past the close, *close_out
 * is the close quote index, *has_esc reports backslashes. */
int yep_json_string(const char* p, size_t len, size_t* i, size_t* close_out, int* has_esc) {
    const yep_text_kernels* k = yep_text_active();
    unsigned char stop[32];
    yep_stopset_clear(stop);
    yep_stopset_add(stop, '"');
    yep_stopset_add(stop, '\\');
    for (unsigned c = 0; c < 0x20; c++) {
        /* RFC 8259: raw C0 controls are invalid inside strings; DEL
         * (0x7F) is NOT a control in JSON and stays legal */
        yep_stopset_add(stop, (unsigned char)c);
    }
    size_t j = *i + 1;
    int esc = 0;
    for (;;) {
        ptrdiff_t r = k->stopset_find(p + j, len - j, stop);
        if (r < 0) {
            return 0; /* unterminated */
        }
        size_t at = j + (size_t)r;
        char c = p[at];
        if (c == '"') {
            *close_out = at;
            *has_esc = esc;
            *i = at + 1;
            return 1;
        }
        if (c != '"' && c != '\\') {
            return 0; /* raw C0 control (incl. breaks) inside a string */
        }
        /* escape: validate the escaped byte inline */
        if (at + 1 >= len) {
            return 0;
        }
        esc = 1;
        char e2 = p[at + 1];
        if (e2 == 'u') {
            for (int h = 2; h <= 5; h++) {
                if (at + (size_t)h >= len ||
                    !yep_ct_is((unsigned char)p[at + (size_t)h], YEP_CT_HEXDIGIT)) {
                    return 0;
                }
            }
            uint32_t cp = 0;
            for (int h = 2; h <= 5; h++) {
                cp = (cp << 4) | (uint32_t)hexval((unsigned char)p[at + (size_t)h]);
            }
            if (cp >= 0xD800 && cp <= 0xDBFF) {
                /* high surrogate must pair; the general kernel reports */
                if (at + 12 >= len || p[at + 6] != '\\' || p[at + 7] != 'u') {
                    return 0;
                }
                uint32_t lo = 0;
                for (int h = 8; h <= 11; h++) {
                    if (!yep_ct_is((unsigned char)p[at + (size_t)h], YEP_CT_HEXDIGIT)) {
                        return 0;
                    }
                    lo = (lo << 4) | (uint32_t)hexval((unsigned char)p[at + (size_t)h]);
                }
                if (lo < 0xDC00 || lo > 0xDFFF) {
                    return 0;
                }
                j = at + 12;
                continue;
            }
            if (cp >= 0xDC00 && cp <= 0xDFFF) {
                return 0; /* lone low surrogate */
            }
            j = at + 6;
            continue;
        }
        if (e2 != '"' && e2 != '\\' && e2 != '/' && e2 != 'b' && e2 != 'f' && e2 != 'n' &&
            e2 != 'r' && e2 != 't') {
            return 0; /* YAML-only escape (\a, \x…): not JSON */
        }
        j = at + 2;
    }
}
