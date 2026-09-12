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

void yep_scan_advance_line(const char* p, size_t* from, size_t to, uint32_t* line,
                           size_t* line_start) {
    for (size_t i = *from; i < to; i++) {
        if (p[i] == '\n') {
            (*line)++;
            *line_start = i + 1;
        }
    }
    *from = to;
}

/* ONE home for the \n/\r break class (scan owns the concept): the
 * engine's multiline walk shares it — it had rebuilt a duplicate per
 * quoted scalar. Prebuilt nibble-class tables (TODO.restructure/68);
 * the StopsetLiterals spec pins them against yep_stopset_init. */
const yep_stopset yep_break_stopset = {
    .bitmap = {0x00, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    .groups = 1,
    .lo = {{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x02,
            0x00, 0x00}},
    .hi = {{0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00}},
};

void yep_scan_facts(const char* p, size_t len, size_t pos, yep_line_facts* out) {
    yep_text_active()->line_facts(p, len, pos, out);
}

/* Line facts -> line info (the flags/marker derivation, facts-free).
 * One kernel pass feeds BOTH this and the shape classifier via the
 * engine's combined memo (TODO.restructure/76). */
void yep_scan_line_f(const char* p, size_t len, size_t pos, const yep_line_facts* f,
                     yep_line_info* out) {
    yep_line_info li;
    li.offset = (uint32_t)pos;
    li.indent = 0;
    li.flags = 0;
    li.first = 0;
    li.end = f->end;
    size_t j = f->indent;
    li.indent = (uint16_t)(j - pos);
    if (j < li.end && p[j] == '\t') {
        size_t k = j;
        while (k < li.end && (p[k] == ' ' || p[k] == '\t')) {
            k++;
        }
        if (k >= li.end) {
            li.flags |= YEP_LF_BLANK;
            if (li.indent == 0) {
                li.flags |= YEP_LF_TAB;
            }
            *out = li;
            return;
        }
        li.flags |= YEP_LF_TAB;
    }

    if (j >= li.end) {
        li.flags |= YEP_LF_BLANK;
        *out = li;
        return;
    }

    li.first = (unsigned char)p[j];
    if (li.first == '#') {
        li.flags |= YEP_LF_COMMENT;
        *out = li;
        return;
    }
    if (li.first == '%' && li.indent == 0) {
        li.flags |= YEP_LF_DIRECTIVE;
        *out = li;
        return;
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
    *out = li;
}

yep_line_info yep_scan_line(const char* p, size_t len, size_t pos) {
    yep_line_info li;
    yep_line_facts f;
    yep_scan_facts(p, len, pos, &f);
    yep_scan_line_f(p, len, pos, &f, &li);
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

int yep_plain_first_ok(unsigned char c) {
    return !(c == ',' || c == ']' || c == '}' || c == '%' || c == '@' || c == '`');
}

int yep_scan_prop_char(unsigned char c) {
    return !yep_ct_any(c, YEP_CT_BLANK | YEP_CT_LBREAK | YEP_CT_FLOW_IND) && c != ',' && c != '#';
}

size_t yep_scan_prop_end(const char* p, size_t len, size_t pos) {
    while (pos < len && yep_scan_prop_char((unsigned char)p[pos])) {
        pos++;
    }
    return pos;
}

/* ---- line-shape classification (TODO.restructure/49) ---- */

/* blank/EOL/EOF directly after p[at] (the dash / explicit-key rule). */
static int shape_blank_next(const char* p, size_t len, size_t at) {
    return at + 1 >= len || p[at + 1] == ' ' || p[at + 1] == '\t' || p[at + 1] == '\n' ||
           p[at + 1] == '\r';
}

/* Value bytes the fast arms do not own: quoted/tagged/block/flow
 * scalars and the compact-syntax leads whose semantics belong to the
 * general chain. */
static int shape_val_bail(unsigned char c, const char* p, size_t len, size_t at) {
    switch (c) {
    case '"':
    case '\'':
    case '!':
    case '|':
    case '>':
    case '[':
    case '{':
    case '*': /* an alias after an anchor is an error shape (SR86) */
        return 1;
    case '-':
    case '?':
        return shape_blank_next(p, len, at);
    default:
        return 0;
    }
}

static void shape_value(const char* p, size_t len, const yep_line_info* li, size_t vt,
                        yep_line_shape* s) {
    s->val_start = (uint32_t)vt;
    if (vt >= li->end || p[vt] == '#') { /* '#' here follows a blank by construction */
        s->val = YEP_LVAL_EMPTY;
        return;
    }
    unsigned char c = (unsigned char)p[vt];
    if (c == '*') {
        s->val = YEP_LVAL_ALIAS; /* the engine's alias walk owns name semantics */
        return;
    }
    if (c == '[' || c == '{') {
        s->val = YEP_LVAL_FLOW;
        return;
    }
    if (c == '&') {
        size_t a_end = yep_scan_prop_end(p, len, vt + 1);
        size_t t = a_end;
        while (t < len && (p[t] == ' ' || p[t] == '\t')) {
            t++;
        }
        if (t < li->end && p[t] != '#' && !shape_val_bail((unsigned char)p[t], p, len, t) &&
            yep_plain_first_ok((unsigned char)p[t])) {
            yep_span v = yep_scan_plain(p, len, t, 0);
            if (v.term != YEP_TERM_COLON && v.end > v.start) {
                s->val = YEP_LVAL_ANCHOR_PLAIN;
                s->anchor_end = (uint32_t)a_end;
                s->val_span = v;
                return;
            }
        }
        return; /* anchor with a following-lines value / odd content: bail */
    }
    if (shape_val_bail(c, p, len, vt) || !yep_plain_first_ok(c)) {
        return;
    }
    yep_span v = yep_scan_plain(p, len, vt, 0);
    if (v.term == YEP_TERM_COLON) {
        return; /* a second terminating ':' is compact/error territory */
    }
    s->val = YEP_LVAL_PLAIN;
    s->val_span = v;
}

/* The key span straight from the fused facts (TODO.restructure/76):
 * the common line's key scan becomes zero byte-walking. Rare shapes
 * (interior ':', embedded '#') fall back to the walking scan — the
 * two must agree, and the LineShape spec pins them together. */
static yep_span shape_key_span(const char* p, size_t len, size_t t, const yep_line_facts* f) {
    yep_span k;
    k.start = (uint32_t)t;
    k.term = YEP_TERM_EOF;
    if (f->stop_set && f->stop >= t) {
        unsigned char c = (unsigned char)p[f->stop];
        if (c == ':') {
            if (yep_colon_terminates(p, len, f->stop, 0)) {
                k.term = YEP_TERM_COLON;
                k.end = f->stop;
                while (k.end > k.start && (p[k.end - 1] == ' ' || p[k.end - 1] == '\t')) {
                    k.end--;
                }
                return k;
            }
        } else if (c == '#' && (f->stop == t || p[f->stop - 1] == ' ' || p[f->stop - 1] == '\t')) {
            k.term = YEP_TERM_COMMENT;
            k.end = f->stop;
            while (k.end > k.start && (p[k.end - 1] == ' ' || p[k.end - 1] == '\t')) {
                k.end--;
            }
            return k;
        }
        return yep_scan_plain(p, len, t, 0); /* non-terminating stop: the walk owns it */
    }
    /* no stop byte before EOL: the span runs to the line end */
    k.end = f->end;
    while (k.end > k.start && (p[k.end - 1] == ' ' || p[k.end - 1] == '\t')) {
        k.end--;
    }
    k.term = f->end < len ? YEP_TERM_EOL : YEP_TERM_EOF;
    return k;
}

void yep_scan_shape_f(const char* p, size_t len, const yep_line_info* li, const yep_line_facts* f,
                      yep_line_shape* s) {
    memset(s, 0, sizeof(*s));
    s->kind = YEP_LSHAPE_NONE;
    s->val = YEP_LVAL_NONE;
    if (li->flags != 0) {
        return; /* blank/comment/doc/directive/tab lines: general paths */
    }
    size_t t = li->offset + li->indent;
    unsigned char c = (unsigned char)p[t];

    if (c == '-' && shape_blank_next(p, len, t)) {
        size_t vt = t + 1;
        while (vt < len && (p[vt] == ' ' || p[vt] == '\t')) {
            vt++;
        }
        s->kind = YEP_LSHAPE_DASH;
        s->dash = (uint32_t)t;
        shape_value(p, len, li, vt, s);
        return;
    }

    /* '?' / ':' followed by a blank are the explicit-key and bare-colon
     * lines (the general paths own them); followed by content they are
     * ordinary plain-first bytes. A dash + blank returned as DASH above. */
    if (yep_plain_first_ok(c) && c != '&' && c != '!' && c != '*' && c != '\'' && c != '"' &&
        c != '[' && c != '{' && !((c == '?' || c == ':') && shape_blank_next(p, len, t))) {
        yep_span k = shape_key_span(p, len, t, f);
        if (k.term == YEP_TERM_COLON) {
            /* k.end is the TRIMMED span end: the ':' sits past the
             * trimmed blanks (or past interior stops on the fallback
             * path) — the forward walk is over spaces, a few bytes */
            size_t colon = k.end;
            while (p[colon] != ':') {
                colon++;
            }
            size_t vt = colon + 1;
            while (vt < len && (p[vt] == ' ' || p[vt] == '\t')) {
                vt++;
            }
            s->kind = YEP_LSHAPE_KEY;
            s->key_start = k.start;
            s->key_end = k.end;
            s->colon = (uint32_t)colon;
            shape_value(p, len, li, vt, s);
        }
    }
}

void yep_scan_shape(const char* p, size_t len, const yep_line_info* li, yep_line_shape* s) {
    yep_line_facts f;
    yep_scan_facts(p, len, li->offset, &f);
    yep_scan_shape_f(p, len, li, &f, s);
}

/* The two plain-scalar stop classes are constants — they were rebuilt
   on EVERY scan (~2 per line; clear + 4-9 adds ≈ 3M ops on a 100k-line
   document), then became static bitmaps, now prebuilt nibble-class
   tables (TODO.restructure/68). Generated: set[c>>3] |= 1 << (c&7) for
   the stop chars below; lo/hi pinned by the StopsetLiterals spec. */
const yep_stopset yep_plain_stop_block = {
    .bitmap = {0x00, 0x24, 0x00, 0x00, 0x08, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* members: \n \r # : */
    .groups = 1,
    .lo = {{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x02,
            0x00, 0x00}},
    .hi = {{0x03, 0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00}},
};

const yep_stopset yep_plain_stop_flow = {
    .bitmap = {0x00, 0x24, 0x00, 0x00, 0x08, 0x10, 0x00, 0x04, 0x00, 0x00, 0x00,
               0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    /* members: \n \r # , : [ ] { } */
    .groups = 2,
    .lo = {{0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x11, 0xA0, 0x08, 0x42,
            0x00, 0x00},
           {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
            0x00, 0x00}},
    .hi = {{0x03, 0x00, 0x0C, 0x10, 0x00, 0x60, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00},
           {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00}},
};

yep_span yep_scan_plain(const char* p, size_t len, size_t pos, int flow) {
    const yep_text_kernels* k = yep_text_active();
    const yep_stopset* stop = flow ? &yep_plain_stop_flow : &yep_plain_stop_block;
    /* Short spans keep the scalar walk: the kernel dispatch costs more
     * than it saves below a vector (block keys run 2-8 bytes — the
     * same gate scan_line uses for its end-find). */
    const int tiny = (len - pos) < 64;

    yep_span s;
    s.start = (uint32_t)pos;
    s.end = (uint32_t)pos;
    s.term = YEP_TERM_EOF;

    size_t i = pos;
    while (i < len) {
        ptrdiff_t hit;
        if (tiny) {
            size_t at = i;
            while (at < len && !yep_stopset_test(stop->bitmap, (unsigned char)p[at])) {
                at++;
            }
            hit = (at < len) ? (ptrdiff_t)(at - i) : -1;
        } else {
            hit = k->stopset_find(stop, p + i, len - i);
        }
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
            /* '#' starts a comment after a blank or at span start */
            if (at == s.start || (at > s.start && (p[at - 1] == ' ' || p[at - 1] == '\t'))) {
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
