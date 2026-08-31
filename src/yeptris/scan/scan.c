/* scan.c — line facts and span location (TODO.impl/06).
 *
 * Byte classification truth lives in chartype; SIMD kernels accelerate
 * the span scans; this module owns YAML's line/span semantics only.
 */

#include <string.h>

#include "common/chartype.h"
#include "common/simd_text.h"
#include "scan.h"

size_t yep_scan_break_len(const char* p, size_t len, size_t pos) {
    if (pos >= len) {
        return 0;
    }
    if (p[pos] == '\n') {
        return 1;
    }
    if (p[pos] == '\r') {
        return (pos + 1 < len && p[pos + 1] == '\n') ? 2 : 1;
    }
    return 0;
}

yep_line_info yep_scan_line(const char* p, size_t len, size_t pos) {
    yep_line_info li;
    li.offset = (uint32_t)pos;
    li.indent = 0;
    li.flags = 0;
    li.first = 0;

    /* Find the line end (any break form). */
    size_t i = pos;
    while (i < len && p[i] != '\n' && p[i] != '\r') {
        i++;
    }
    li.end = (uint32_t)i;

    /* Indentation: spaces only; a tab in the indent is flagged. */
    size_t j = pos;
    while (j < li.end && p[j] == ' ') {
        j++;
    }
    li.indent = (uint16_t)(j - pos);
    if (j < li.end && p[j] == '\t') {
        /* a whitespace-only line (spaces AND tabs) is blank; a tab that
         * precedes content is tab-in-indentation */
        size_t k = j;
        while (k < li.end && (p[k] == ' ' || p[k] == '\t')) {
            k++;
        }
        if (k >= li.end) {
            li.flags |= YEP_LF_BLANK;
            return li;
        }
        li.flags |= YEP_LF_TAB;
    }

    if (j >= li.end) {
        li.flags |= YEP_LF_BLANK;
        return li;
    }

    li.first = (unsigned char)p[j];
    if (li.first == '#') {
        li.flags |= YEP_LF_COMMENT;
        return li;
    }
    if (li.first == '%' && li.indent == 0) {
        li.flags |= YEP_LF_DIRECTIVE;
        return li;
    }

    /* "---" / "..." at column 0, followed by EOL/space/tab/comment —
     * content may follow on the same line ("--- > folded"). */
    if (li.indent == 0 && li.end - j >= 3 && memcmp(p + j, "---", 3) == 0 &&
        (li.end - j == 3 || p[j + 3] == ' ' || p[j + 3] == '\t')) {
        li.flags |= YEP_LF_DOC_START;
    } else if (li.indent == 0 && li.end - j >= 3 && memcmp(p + j, "...", 3) == 0 &&
               (li.end - j == 3 || p[j + 3] == ' ' || p[j + 3] == '\t')) {
        li.flags |= YEP_LF_DOC_END;
    }
    return li;
}

/* Terminator check for ':' — blank, EOL, or (in flow) a flow indicator. */
static int yep_colon_terminates(const char* p, size_t len, size_t colon, int flow) {
    size_t next = colon + 1;
    if (next >= len) {
        return 1; /* ':' at EOF terminates */
    }
    unsigned char c = (unsigned char)p[next];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        return 1;
    }
    if (flow && yep_ct_is(c, YEP_CT_FLOW_IND)) {
        return 1;
    }
    return 0;
}

yep_span yep_scan_plain(const char* p, size_t len, size_t pos, int flow) {
    const yep_text_kernels* k = yep_text_active();
    unsigned char stop[32];
    yep_stopset_clear(stop);
    yep_stopset_add(stop, '\n');
    yep_stopset_add(stop, '\r');
    yep_stopset_add(stop, ':');
    yep_stopset_add(stop, '#');
    if (flow) {
        static const unsigned char flow_stops[5] = {',', '[', ']', '{', '}'};
        for (size_t f = 0; f < 5; f++) {
            yep_stopset_add(stop, flow_stops[f]);
        }
    }

    yep_span s;
    s.start = (uint32_t)pos;
    s.end = (uint32_t)pos;
    s.term = YEP_TERM_EOF;

    size_t i = pos;
    while (i < len) {
        ptrdiff_t hit = k->stopset_find(p + i, len - i, stop);
        size_t at = (hit < 0) ? len : i + (size_t)hit;
        unsigned char c = (at < len) ? (unsigned char)p[at] : 0;

        if (at == len) {
            i = len;
            s.term = YEP_TERM_EOF;
            break;
        }
        if (c == '\n' || c == '\r') {
            i = at;
            s.term = YEP_TERM_EOL;
            break;
        }
        if (c == ':') {
            if (yep_colon_terminates(p, len, at, flow)) {
                i = at;
                s.term = YEP_TERM_COLON;
                break;
            }
            i = at + 1;
            continue;
        }
        if (c == '#') {
            /* '#' only starts a comment after a blank or at span start. */
            if (at > s.start && (p[at - 1] == ' ' || p[at - 1] == '\t')) {
                i = at;
                s.term = YEP_TERM_COMMENT;
                break;
            }
            i = at + 1;
            continue;
        }
        /* flow indicator (flow context only) */
        i = at;
        s.term = YEP_TERM_FLOW;
        break;
    }

    /* Trim trailing spaces/tabs from the span. */
    size_t e = i;
    while (e > s.start && (p[e - 1] == ' ' || p[e - 1] == '\t')) {
        e--;
    }
    s.end = (uint32_t)e;
    if (e == s.start && s.term == YEP_TERM_EOF) {
        s.term = YEP_TERM_EOF;
    }
    return s;
}

yep_span yep_scan_quoted(const char* p, size_t len, size_t pos, int* has_escape) {
    const yep_text_kernels* k = yep_text_active();
    unsigned char q = (unsigned char)p[pos];
    int esc = 0;
    ptrdiff_t r = k->quote_scan(p + pos + 1, len - pos - 1, (char)q, &esc);
    if (has_escape != NULL) {
        *has_escape = esc;
    }
    yep_span s;
    s.start = (uint32_t)(pos + 1);
    if (r < 0) {
        s.end = (uint32_t)len;
        s.term = YEP_TERM_EOL; /* unterminated — caller reports the error */
    } else {
        s.end = (uint32_t)(pos + 1 + (size_t)r);
        s.term = YEP_TERM_EOF;
    }
    return s;
}

int yep_scan_is_key_start(unsigned char c) {
    /* Quotes and flow openers always start a potential key; plain-first
     * excludes most indicators. */
    if (c == '\'' || c == '"' || c == '[' || c == '{' || c == '?') {
        return 1;
    }
    if (c == '&' || c == '!' || c == '*') {
        return 1; /* properties or an alias may open a key node */
    }
    if (yep_ct_is(c, YEP_CT_INDICATOR)) {
        return 0; /* the remaining indicators cannot start a plain scalar */
    }
    return yep_ct_is_ns(c);
}
