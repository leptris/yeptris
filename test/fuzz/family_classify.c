/* family_classify.c — the law-verifying classifier (see family_classify.h).
 *
 * Two independent mini-models derive the expected event dump per side,
 * sharing one renderer whose byte format is the comparator's SSOT
 * (event_dump.h: yd_escape_append + yd_line):
 *
 *  - the yeptris model: flow members split at top-level commas outside
 *    quoted spans; glued-`?` members stay one flat plain (law a/d);
 *    bare alias members ride the engine's e_alias name cut (law c —
 *    the alias event renders the name verbatim, a ':'-blank boundary
 *    opens the VALUE slot, a separator/closer leaves the empty
 *    scalar); `?`+blank/break splits the explicit key; plain folds
 *    ride the 1.2 law (1 break -> space, n -> n-1 newlines, interior
 *    blanks exact, leading/trailing runs trimmed);
 *  - the libyaml model: the 0.2.x scanner/parser contract the corpus
 *    walks adjudicated — scan_to_next_token eats blanks/tabs and every
 *    break kind in flow and comment-eats from any `#` to the line
 *    break; every flow `?` is the KEY token; every flow `:` is the
 *    VALUE token with the simple-key insertion; plains fold per
 *    READ_LINE (CR/LF/CRLF/NEL -> LF, LS/PS ride RAW) with the
 *    leading/trailing-break dance (a leading folded break with no
 *    trailing joins one space; with trailing, the trailing string
 *    joins raw); the parser inserts empty scalars for KEY-without-node
 *    and value-without-token; a KEY inside a flow sequence opens the
 *    single-pair mapping wrapper (whose mapping-key state CONSUMES a
 *    following VALUE/FLOW-ENTRY token — the `[?#,x]` error shape).
 *
 * Any byte either model cannot place -> UNKNOWN (0). The comparison is
 * byte-exact strcmp against the caller's actual dumps; a single flipped
 * dump byte on a family-shaped input therefore stays red.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "family_classify.h"

/* ---- budget (conservatism law v) ----------------------------------- */

#define FC_MAX_INPUT                                                                               \
    16384u                        /* the family's members are <1KB; the                            \
                                   * largest corpus seed is 10732 */
#define FC_MAX_OUT (256u * 1024u) /* derived-dump cap */
#define FC_MAX_PIECES 256u        /* plain fold pieces per node */
#define FC_MAX_MEMBERS                                                                             \
    1024u                   /* flow members per collection (the                                    \
                             * 10KB r258 artifact rides ~300) */
#define FC_MAX_TOKENS 4096u /* libyaml tokens per stream */

/* ---- byte facts ------------------------------------------------------ */

static int fc_blank(unsigned char c) {
    return c == ' ' || c == '\t';
}

static int fc_flow_ind(unsigned char c) {
    return c == ',' || c == '[' || c == ']' || c == '{' || c == '}';
}

/* Break length in bytes: LF, CR, CRLF, NEL (C2 85), LS/PS (E2 80 A8/A9).
 * Returns 0 when the byte does not open a break. */
static int fc_break_len(const uint8_t* p, size_t n, size_t i) {
    if (i >= n) {
        return 0;
    }
    unsigned char c = p[i];
    if (c == '\n' || c == '\r') {
        return 1;
    }
    if (c == 0xC2 && i + 1 < n && p[i + 1] == 0x85) {
        return 2;
    }
    if (c == 0xE2 && i + 2 < n && p[i + 1] == 0x80 && (p[i + 2] == 0xA8 || p[i + 2] == 0xA9)) {
        return 3;
    }
    return 0;
}

/* CRLF counts as ONE break for fold runs. */
static int fc_break_run_len(const uint8_t* p, size_t n, size_t i) {
    int l = fc_break_len(p, n, i);
    if (l == 1 && p[i] == '\r' && i + 1 < n && p[i + 1] == '\n') {
        return 2;
    }
    return l;
}

/* Advance past blanks and whole breaks. A single-byte advance would land
 * mid-sequence on the multi-byte breaks (NEL C2 85, LS/PS E2 80 A8/A9),
 * so every skip walk must move by the break's byte length. */
static size_t fc_skip_blank_break(const uint8_t* p, size_t n, size_t i) {
    while (i < n) {
        if (fc_blank(p[i])) {
            i++;
            continue;
        }
        int bl = fc_break_len(p, n, i);
        if (bl == 0) {
            break;
        }
        i += (size_t)bl;
    }
    return i;
}

/* libyaml IS_ALPHA: [0-9A-Za-z_-] (the alias/anchor name cut). */
static int fc_alpha(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
           c == '-';
}

/* ---- output buffer + the dump renderer (event_dump.h's byte format) -- */

typedef struct {
    char* buf;
    size_t len;
    size_t cap;
    int bad; /* overflow/allocation: the derivation declines */
} fout;

static void fo_need(fout* o, size_t extra) {
    if (o->bad || o->len + extra + 1 <= o->cap) {
        return;
    }
    size_t nc = o->cap ? o->cap : 256;
    while (nc < o->len + extra + 1) {
        nc *= 2;
        if (nc > FC_MAX_OUT) {
            o->bad = 1;
            return;
        }
    }
    char* nb = realloc(o->buf, nc);
    if (nb == NULL) {
        o->bad = 1;
        return;
    }
    o->buf = nb;
    o->cap = nc;
}

static void fo_bytes(fout* o, const void* s, size_t n) {
    if (o->bad || n == 0) {
        return;
    }
    fo_need(o, n);
    if (o->bad) {
        return;
    }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
    o->buf[o->len] = '\0';
}

static void fo_str(fout* o, const char* s) {
    fo_bytes(o, s, strlen(s));
}

/* yd_escape_append's byte format: backslash, LF, CR, TAB escaped; other
 * C0/DEL as \xNN; every other byte rides raw. */
static void fo_escape(fout* o, const uint8_t* v, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = v[i];
        switch (c) {
        case '\\':
            fo_str(o, "\\\\");
            break;
        case '\n':
            fo_str(o, "\\n");
            break;
        case '\r':
            fo_str(o, "\\r");
            break;
        case '\t':
            fo_str(o, "\\t");
            break;
        default:
            if (c < 0x20 || c == 0x7f) {
                char hex[8];
                snprintf(hex, sizeof(hex), "\\x%02x", (unsigned)c);
                fo_str(o, hex);
            } else {
                fo_bytes(o, &c, 1);
            }
            break;
        }
        if (o->bad) {
            return;
        }
    }
}

/* The 1.2 plain fold join for a run of n breaks: 1 -> one space,
 * n>1 -> n-1 newlines ("[0\r\r)]" is "0\n"). */
static void fo_fold_run(fout* o, uint32_t n) {
    if (n == 1) {
        fo_str(o, " ");
        return;
    }
    for (uint32_t b = 0; b + 1 < n; b++) {
        fo_str(o, "\\n");
    }
}

/* ---- shared plain-piece walk ---------------------------------------- */

typedef struct {
    size_t at;
    size_t len;
    uint32_t breaks_before; /* the break-run count BEFORE this piece */
    int has_blanks_before;  /* an exact blank run rode before the piece
                             * (rendered byte-exact; 0 for the first) */
    size_t blanks_at;
    size_t blanks_n;
} fcpiece;

typedef struct {
    fcpiece pieces[FC_MAX_PIECES];
    size_t n;
    int colon; /* a `:`-VALUE boundary ended the node */
    size_t colon_at;
    size_t anchor_at; /* an anchored empty value (the props law) */
    size_t anchor_len;
} fcnode;

/* Scan one plain content piece inside [q, end): interior blanks ride
 * byte-exact; the piece stops at a break, a `:`-VALUE boundary, a flow
 * separator/close, or blanks that lead into one of those. Returns the
 * stop position and whether it is a separator/close. A blank run that
 * leads into `#` is a COMMENT on both engines — the walk refuses it
 * (the family's `#` only rides glued to the `?`-led run, law a). */
static int fc_piece(const uint8_t* p, size_t n, size_t q, size_t end, size_t* piece_at,
                    size_t* piece_len, size_t* stop, int* stop_sep) {
    size_t k = q;
    size_t blank_n = 0;
    for (;;) {
        if (k >= end) {
            break;
        }
        unsigned char c = p[k];
        if (fc_break_len(p, n, k) > 0) {
            break;
        }
        if (c == ':') {
            if (k + 1 >= end) {
                break; /* ':' at the member end terminates */
            }
            unsigned char nx = p[k + 1];
            if (fc_blank(nx) || fc_break_len(p, n, k + 1) > 0) {
                break; /* the VALUE boundary */
            }
            if (fc_flow_ind(nx)) {
                return 0; /* ':' + flow indicator: libyaml's "unexpected
                           * ':'" scanner error — outside the family */
            }
        }
        if (fc_flow_ind(c)) {
            break;
        }
        if (fc_blank(c)) {
            blank_n++;
            k++;
            continue;
        }
        if (c == '#' && blank_n > 0) {
            return 0; /* a comment: both engines end the plain here */
        }
        /* content: any pending blanks ride INSIDE the piece */
        blank_n = 0;
        k++;
    }
    /* trailing blanks trim (yep_scan_plain stops before them; libyaml's
     * whitespaces buffer drops when no content follows) */
    while (k > q && fc_blank(p[k - 1])) {
        k--;
    }
    *piece_at = q;
    *piece_len = k - q;
    *stop = k;
    *stop_sep = (k < end && fc_flow_ind(p[k]));
    return 1;
}

/* The full plain-node walk: pieces separated by break runs. Returns 0
 * on any gate (comment lead-in, ':' + flow indicator, budget). */
static int fc_plain_pieces(const uint8_t* p, size_t n, size_t i, size_t end, fcnode* node,
                           size_t* stop_at, int* stop_sep) {
    memset(node, 0, sizeof(*node));
    size_t pos = i;
    for (;;) {
        /* skip the blank/break run to the next content */
        uint32_t br = 0;
        size_t q = pos;
        for (;;) {
            while (q < end && fc_blank(p[q])) {
                q++;
            }
            if (q < end) {
                int bl = fc_break_run_len(p, n, q);
                if (bl > 0) {
                    br++;
                    q += (size_t)bl;
                    continue;
                }
            }
            break;
        }
        if ((node->n > 0 || br > 0) && q < end && p[q] == '#') {
            return 0; /* a comment line/lead-in: outside the family */
        }
        if (q >= end || fc_flow_ind(p[q])) {
            *stop_at = q;
            *stop_sep = 1;
            return 1;
        }
        if (node->n >= FC_MAX_PIECES) {
            return 0;
        }
        size_t pat, plen, stop;
        int sep;
        if (!fc_piece(p, n, q, end, &pat, &plen, &stop, &sep)) {
            return 0;
        }
        if (plen > 0) {
            node->pieces[node->n].at = pat;
            node->pieces[node->n].len = plen;
            node->pieces[node->n].breaks_before = (node->n == 0) ? 0 : br;
            node->pieces[node->n].has_blanks_before = 0;
            node->pieces[node->n].blanks_at = 0;
            node->pieces[node->n].blanks_n = 0;
            node->n++;
        }
        if (stop < end && p[stop] == ':' &&
            (stop + 1 >= end || fc_blank(p[stop + 1]) || fc_break_len(p, n, stop + 1) > 0)) {
            node->colon = 1;
            node->colon_at = stop;
            *stop_at = stop;
            *stop_sep = 0;
            return 1;
        }
        if (sep || stop >= end) {
            *stop_at = stop;
            *stop_sep = 1;
            return 1;
        }
        pos = stop; /* a break stopped the piece: loop for the next */
    }
}

/* Render the folded plain (the 1.2 fold law; interior blanks exact). */
static void fc_render_plain(fout* o, const uint8_t* p, const fcpiece* pieces, size_t np) {
    for (size_t k = 0; k < np; k++) {
        if (k > 0) {
            fo_fold_run(o, pieces[k].breaks_before);
        }
        fo_escape(o, p + pieces[k].at, pieces[k].len);
    }
}

static void fc_render_val(fout* o, const uint8_t* p, const fcnode* nd) {
    if (nd->n == 0) {
        if (nd->anchor_len > 0) {
            fo_str(o, "=VAL : &");
            fo_bytes(o, p + nd->anchor_at, nd->anchor_len);
            fo_str(o, " \n");
            return;
        }
        fo_str(o, "=VAL : \n");
        return;
    }
    fo_str(o, "=VAL : ");
    fc_render_plain(o, p, nd->pieces, nd->n);
    fo_str(o, "\n");
}

/* ---- the yeptris model ---------------------------------------------- */

typedef struct {
    const uint8_t* p;
    size_t n;
    int is_map; /* the root collection kind */
    fout out;
    size_t members;
    size_t pos_trace;
} yctx;

static int y_fail_at(yctx* c, int line) {
    c->out.bad = 1;
    if (getenv("YD_FC_TRACE") != NULL) {
        fprintf(stderr, "yeptris gate at line %d (member pos %zu of %zu)\n", line, c->pos_trace,
                c->n);
    }
    return 0;
}
#define y_fail(c) y_fail_at((c), __LINE__)

/* The alias name run (the engine's e_alias walk): prop chars = not
 * blank, not break, not flow indicator, ',', or '#'; an interior ':'
 * rides unless a blank or flow indicator follows it ("*an:chor" is one
 * name — Y2GN; at the member end it rides too — 2SXE). */
static size_t fc_alias_name(const uint8_t* p, size_t n, size_t i, size_t end) {
    size_t s = i;
    while (i < end) {
        unsigned char c = p[i];
        if (fc_blank(c) || c == ',' || c == '#' || fc_flow_ind(c) || fc_break_len(p, n, i) > 0) {
            break;
        }
        if (c == ':') {
            size_t j = i + 1;
            if (j < end && (fc_blank(p[j]) || fc_flow_ind(p[j]) || fc_break_len(p, n, j) > 0)) {
                break; /* ':' separates before blank/flow indicator */
            }
        }
        i++;
    }
    return i - s;
}

/* The DQ content fold (the e_quoted_floor law; libyaml's
 * scan_flow_scalar folds CR/LF/NEL identically, so quoted spans agree
 * byte-exact across the engines — the law-b raw-ride split is
 * plain-only). Interior blanks ride BYTE-EXACT when content follows
 * directly (the dump keeps the raw TAB — the r251 seed's `##!%b\t##`);
 * blanks at a break boundary drop into the fold. Gates: a backslash
 * (escapes), an exotic break, a leading run >1, any trailing
 * blank/break, a blank-led start. */
static int fc_quoted_value(const uint8_t* p, size_t n, size_t s, size_t e, fout* o,
                           size_t* close_at) {
    size_t i = s;
    uint32_t lead_run = 0;
    int first_content = 1;
    uint32_t run = 0;
    size_t blank_at = 0;
    size_t blank_n = 0;
    while (i < e) {
        int bl = fc_break_run_len(p, n, i);
        if (bl > 0) {
            if (p[i] == 0xE2 || p[i] == 0xC2) {
                return 0; /* an exotic break in a quoted span */
            }
            if (first_content) {
                lead_run++;
            } else {
                run++;
            }
            blank_n = 0; /* the boundary blanks join the fold */
            i += (size_t)bl;
            continue;
        }
        unsigned char c = p[i];
        if (c == '"') {
            break; /* the close */
        }
        if (c == '\\') {
            return 0; /* escapes are outside the family */
        }
        if (fc_blank(c)) {
            if (blank_n == 0) {
                blank_at = i;
            }
            blank_n++;
            i++;
            continue;
        }
        if (first_content) {
            first_content = 0;
            if (blank_n > 0 || lead_run > 1) {
                return 0; /* a blank-led span / a leading run: untested */
            }
            if (lead_run == 1) {
                fo_str(o, " "); /* the leading single fold keeps one space */
            }
        } else if (run > 0) {
            fo_fold_run(o, run); /* the boundary blanks on BOTH sides of
                                  * the break ride the fold */
            run = 0;
            blank_n = 0;
        } else if (blank_n > 0) {
            fo_escape(o, p + blank_at, blank_n); /* interior: byte-exact,
                                                  * dump-escaped */
            blank_n = 0;
        }
        fo_escape(o, p + i, 1);
        i++;
    }
    if (!first_content && (run > 0 || blank_n > 0)) {
        return 0; /* trailing blanks/breaks: untested */
    }
    if (first_content && lead_run > 0) {
        return 0; /* a break-only span: untested */
    }
    *close_at = i;
    return i < e && p[i] == '"';
}

/* A value slot after a `:`-boundary: plain or empty only (an alias /
 * quoted value slot is untested). */
static int y_value_node(yctx* c, size_t i, size_t* stop, fcnode* val) {
    const uint8_t* p = c->p;
    size_t q = fc_skip_blank_break(p, c->n, i);
    if (q >= c->n || fc_flow_ind(p[q])) {
        memset(val, 0, sizeof(*val));
        *stop = q;
        return 1; /* the empty value */
    }
    if (p[q] == '*') {
        return y_fail(c); /* an alias value slot: untested */
    }
    if (p[q] == '&') {
        /* the anchored empty value (the engine's props-then-no-node
         * law): the anchor name rides the prop-char set; only a
         * separator/close may follow */
        size_t as = q + 1;
        size_t al = fc_alias_name(p, c->n, as, c->n);
        if (al == 0) {
            return y_fail(c); /* an anchor needs a name */
        }
        size_t ae = as + al;
        while (ae < c->n && fc_blank(p[ae])) {
            ae++;
        }
        if (ae >= c->n || !fc_flow_ind(p[ae])) {
            return y_fail(c);
        }
        memset(val, 0, sizeof(*val));
        val->anchor_at = as;
        val->anchor_len = al;
        *stop = ae;
        return 1;
    }
    if (p[q] == '"' || p[q] == '\'' || p[q] == '!' || p[q] == '[' || p[q] == '{') {
        return y_fail(c);
    }
    int sep;
    if (!fc_plain_pieces(p, c->n, q, c->n, val, stop, &sep)) {
        return y_fail(c);
    }
    return 1;
}

/* The `?`-led member: GLUED (laws a/d) or SPLIT (`?` + blank/break). */
static int y_question_member(yctx* c, size_t i, size_t* term_at) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    size_t nxt = i + 1;
    int split = 0;
    if (nxt < n && (fc_blank(p[nxt]) || fc_break_len(p, n, nxt) > 0)) {
        split = 1;
    }
    fcnode key, val;
    memset(&val, 0, sizeof(val));
    size_t stop;
    int sep;
    if (!split) {
        /* GLUED: the `?` is content; the plain scan starts at it */
        if (!fc_plain_pieces(p, n, i, n, &key, &stop, &sep)) {
            return y_fail(c);
        }
        if (key.colon) {
            if (!c->is_map) {
                return y_fail(c); /* a glued SEQ entry never splits
                                   * key/value (untested shape) */
            }
            if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
                return 0;
            }
        }
        if (stop >= n || !fc_flow_ind(p[stop])) {
            return y_fail(c);
        }
        *term_at = stop;
        if (c->is_map) {
            fc_render_val(&c->out, p, &key);
            fc_render_val(&c->out, p, &val);
        } else {
            fc_render_val(&c->out, p, &key);
        }
        return 1;
    }
    /* SPLIT: the `?` drops; the key node starts after the blank/break */
    size_t ks = fc_skip_blank_break(p, n, nxt);
    int key_is_alias = 0;
    size_t alias_at = 0;
    size_t alias_len = 0;
    if (ks >= n || fc_flow_ind(p[ks])) {
        memset(&key, 0, sizeof(key)); /* the empty key node */
        stop = ks;
        sep = (ks < n && fc_flow_ind(p[ks]));
    } else if (p[ks] == '*') {
        alias_at = ks + 1;
        alias_len = fc_alias_name(p, n, alias_at, n);
        key_is_alias = 1;
        size_t ae = alias_at + alias_len;
        if (alias_len == 0 || ae >= n) {
            if (ae >= n) {
                return y_fail(c);
            }
            return y_fail(c);
        }
        if (p[ae] == ':') {
            key.colon = 1;
            key.colon_at = ae;
            stop = ae;
            sep = 0;
        } else if (fc_flow_ind(p[ae])) {
            stop = ae;
            sep = 1;
        } else {
            return y_fail(c); /* bytes after the alias name that are not
                               * a boundary: outside the family */
        }
    } else if (p[ks] == '#') {
        /* the comment at the key position (the r245 `?\r###|` members):
         * yeptris fires a comment at the content-start `#`; the run eats
         * to the break — the member's `,` separator AND any following
         * `?`+break ride inside it inertly — and whatever content
         * follows becomes the STOLEN KEY of this explicit-key member,
         * rendered as the single-pair wrapper (the m7/m8/m9/m10 probes:
         * `[a,?\r#x,b\nc]` -> +MAP(c,), `[?##|,?\r###|,?\rb]` ->
         * +MAP(b,)). Gates: exactly one separator inside the run, a
         * terminator exists, SEQ root, flat content start. */
        size_t e = ks + 1;
        int seps = 0;
        while (e < n && fc_break_len(p, n, e) == 0) {
            unsigned char cc = p[e];
            if (fc_flow_ind(cc)) {
                if (cc != ',') {
                    return y_fail(c);
                }
                seps++;
            }
            e++;
        }
        if (seps != 1 || e >= n) {
            return y_fail(c);
        }
        e += (size_t)fc_break_run_len(p, n, e);
        ks = fc_skip_blank_break(p, n, e);
        if (ks >= n || c->is_map) {
            return y_fail(c);
        }
        if (p[ks] == '*' || p[ks] == '&' || p[ks] == '"' || p[ks] == '\'' || p[ks] == '!' ||
            p[ks] == '[' || p[ks] == '{') {
            return y_fail(c); /* a structural stolen key: untested */
        }
        if (p[ks] == ':' &&
            (ks + 1 >= n || fc_blank(p[ks + 1]) || fc_break_len(p, n, ks + 1) > 0)) {
            return y_fail(c); /* a colon-led stolen key: untested */
        }
        if (!fc_plain_pieces(p, n, ks, n, &key, &stop, &sep)) {
            return y_fail(c);
        }
    } else if (p[ks] == '"' || p[ks] == '\'' || p[ks] == '&' || p[ks] == '!' || p[ks] == '[' ||
               p[ks] == '{') {
        return y_fail(c);
    } else {
        if (p[ks] == ':' &&
            (ks + 1 >= n || fc_blank(p[ks + 1]) || fc_break_len(p, n, ks + 1) > 0)) {
            return y_fail(c); /* a colon-led key node: untested */
        }
        if (!fc_plain_pieces(p, n, ks, n, &key, &stop, &sep)) {
            return y_fail(c);
        }
    }
    if (key.colon) {
        if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
            return 0;
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        return y_fail(c);
    }
    *term_at = stop;
    if (c->is_map) {
        if (key_is_alias) {
            fo_str(&c->out, "=ALI *");
            fo_bytes(&c->out, p + alias_at, alias_len);
            fo_str(&c->out, "\n");
        } else {
            fc_render_val(&c->out, p, &key);
        }
        fc_render_val(&c->out, p, &val);
    } else {
        /* the SEQ single-pair wrapper (the engine's KEY-implicit map) */
        fo_str(&c->out, "+MAP\n");
        fc_render_val(&c->out, p, &key);
        fc_render_val(&c->out, p, &val);
        fo_str(&c->out, "-MAP\n");
    }
    return 1;
}

/* One member of the root collection. */
static int y_member(yctx* c, size_t i, size_t* term_at) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    c->pos_trace = i;
    unsigned char ch = p[i];
    if (getenv("YD_FC_TRACE") != NULL) {
        fprintf(stderr, "member at %zu (%c): %.24s\n", (size_t)i, ch, (const char*)p + i);
    }
    if (ch == '?') {
        return y_question_member(c, i, term_at);
    }
    if (ch == '"') {
        /* the member IS the quoted span; only blanks may follow; and a
         * quoted MAP member (key with an empty value) is untested */
        if (c->is_map) {
            return y_fail(c);
        }
        fout tmp = {NULL, 0, 0, 0};
        size_t close = 0;
        int ok = fc_quoted_value(p, n, i + 1, n, &tmp, &close);
        if (!ok) {
            free(tmp.buf);
            return y_fail(c);
        }
        size_t j = fc_skip_blank_break(p, n, close + 1);
        if (j >= n || !fc_flow_ind(p[j])) {
            free(tmp.buf);
            return y_fail(c);
        }
        *term_at = j;
        fo_str(&c->out, "=VAL : ");
        fo_bytes(&c->out, tmp.buf, tmp.len);
        fo_str(&c->out, "\n");
        free(tmp.buf);
        return 1;
    }
    /* `|`, `>`, `%`, `@` ride as plain CONTENT at a yeptris member
     * start (the r258 ">\r2002:binary" members) — the libyaml walk
     * gates them where its dispatch would really fire; `&`, `*`, `!`,
     * quotes and collections stay structural gates */
    if (ch == '*') {
        /* the bare alias member (the r270 `{?#,*<name>\n,(}` soup): the
         * engine's e_alias name cut fires at the member start and the
         * alias event rides the name verbatim (`=ALI *<name>`, the Y2GN
         * interior-`:` law: the name cut stops before a ':' that a
         * blank/break follows). Two terminations: a ':'-blank/break
         * boundary is the VALUE token (MAP only — the value node rides
         * y_value_node, the same gates as the plain-key path); a
         * separator/closer after the blank/break run leaves the map
         * value slot to the parser's empty-scalar insertion (SEQ: the
         * alias is a bare node). Anything else — a ':' glued to a flow
         * indicator, a SEQ alias key with a value, a non-boundary byte
         * — declines. */
        size_t as = i + 1;
        size_t al = fc_alias_name(p, n, as, n);
        if (al == 0) {
            return y_fail(c); /* a name cut that dies at once */
        }
        size_t ae = as + al;
        if (c->is_map && p[ae] == ':' &&
            (ae + 1 >= n || fc_blank(p[ae + 1]) || fc_break_len(p, n, ae + 1) > 0)) {
            fcnode val;
            size_t vstop;
            if (!y_value_node(c, ae + 1, &vstop, &val)) {
                return 0;
            }
            if (vstop >= n || !fc_flow_ind(p[vstop])) {
                return y_fail(c);
            }
            *term_at = vstop;
            fo_str(&c->out, "=ALI *");
            fo_bytes(&c->out, p + as, al);
            fo_str(&c->out, "\n");
            fc_render_val(&c->out, p, &val);
            return 1;
        }
        size_t ve = fc_skip_blank_break(p, n, ae);
        if (ve >= n || !fc_flow_ind(p[ve])) {
            return y_fail(c); /* a non-boundary byte after the name */
        }
        *term_at = ve;
        fo_str(&c->out, "=ALI *");
        fo_bytes(&c->out, p + as, al);
        fo_str(&c->out, "\n");
        if (c->is_map) {
            fo_str(&c->out, "=VAL : \n");
        }
        return 1;
    }
    if (ch == '&' || ch == '\'' || ch == '!' || ch == '#' || ch == '[' || ch == '{') {
        return y_fail(c); /* outside the family's flat-member shape */
    }
    if (ch == '-' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0 ||
                      fc_flow_ind(p[i + 1]))) {
        return y_fail(c); /* a block entry / the bare-dash entry error */
    }
    if (ch == ':' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
        return y_fail(c); /* a colon-led member: untested */
    }
    fcnode key, val;
    memset(&val, 0, sizeof(val));
    size_t stop;
    int sep;
    if (!fc_plain_pieces(p, n, i, n, &key, &stop, &sep)) {
        return y_fail(c);
    }
    if (key.colon) {
        if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
            return 0;
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        return y_fail(c);
    }
    *term_at = stop;
    if (c->is_map) {
        fc_render_val(&c->out, p, &key);
        fc_render_val(&c->out, p, &val);
    } else if (key.colon) {
        /* a SEQ entry split at the colon boundary rides the single-pair
         * wrapper on BOTH engines (the simple-key mechanism — the r245
         * m2 seed's "2002:binaryg:\r\r1.0e17.tag" members) */
        fo_str(&c->out, "+MAP\n");
        fc_render_val(&c->out, p, &key);
        fc_render_val(&c->out, p, &val);
        fo_str(&c->out, "-MAP\n");
    } else {
        fc_render_val(&c->out, p, &key);
    }
    return 1;
}

/* The root walk: members to the matching close, then the tail gate. */
static int y_root(yctx* c) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    c->pos_trace = 1;
    fo_str(&c->out, "+STR\n");
    fo_str(&c->out, c->is_map ? "+MAP\n" : "+SEQ\n");
    size_t i = 1;
    for (;;) {
        i = fc_skip_blank_break(p, n, i);
        if (i >= n) {
            return y_fail(c); /* never closed */
        }
        unsigned char ch = p[i];
        if (ch == (c->is_map ? '}' : ']')) {
            i++;
            break;
        }
        if (ch == ',' || ch == (c->is_map ? ']' : '}') || ch == '#') {
            c->pos_trace = i;
            if (getenv("YD_FC_TRACE") != NULL) {
                fprintf(stderr, "loop-top byte at %zu: %02x (prev 24: %.*s)\n", i, ch,
                        (int)(i > 24 ? 24 : i), (const char*)p + i - (i > 24 ? 24 : i));
            }
            return y_fail(c); /* an empty/comment member: outside */
        }
        if (c->members >= FC_MAX_MEMBERS) {
            return y_fail(c);
        }
        c->members++;
        size_t term;
        if (!y_member(c, i, &term)) {
            return 0;
        }
        i = term;
        if (i >= n) {
            return y_fail(c);
        }
        if (p[i] == ',') {
            i++;
            continue;
        }
        if (p[i] == (c->is_map ? '}' : ']')) {
            i++;
            break;
        }
        return y_fail(c);
    }
    fo_str(&c->out, c->is_map ? "-MAP\n" : "-SEQ\n");
    /* the tail: blanks, breaks, comments, then end of input */
    i = fc_skip_blank_break(p, n, i);
    while (i < n) {
        if (p[i] == '#') {
            while (i < n && fc_break_len(p, n, i) == 0) {
                i++;
            }
            i = fc_skip_blank_break(p, n, i);
            continue;
        }
        return y_fail(c); /* anything else after the root: not this family */
    }
    fo_str(&c->out, "-STR\n");
    return 1;
}

static int y_derive(const uint8_t* data, size_t size, int is_map, fout* out) {
    yctx c;
    memset(&c, 0, sizeof(c));
    c.p = data;
    c.n = size;
    c.is_map = is_map;
    c.out = *out;
    int ok = y_root(&c);
    if (!ok || c.out.bad) {
        free(c.out.buf);
        memset(out, 0, sizeof(*out));
        out->bad = 1;
        return 0;
    }
    *out = c.out;
    return 1;
}

/* ---- the libyaml model ---------------------------------------------- */

typedef enum {
    LT_FLSEQ,
    LT_FLMAP,
    LT_FLSEQ_END,
    LT_FLMAP_END,
    LT_ENTRY,
    LT_KEY,
    LT_VALUE,
    LT_SCALAR,
    LT_ALIAS,
    LT_ANCHOR,
    LT_EOS,
    LT_ERROR
} ltkind;

typedef struct {
    ltkind kind;
    size_t line;
    char* text;          /* SCALAR: the folded value (already dump-escaped);
                          * owned */
    size_t tlen;         /* SCALAR: the folded length */
    const uint8_t* name; /* ALIAS/ANCHOR: the name span (input view) */
    size_t name_len;
} ltok;

typedef struct {
    const uint8_t* p;
    size_t n;
    size_t pos;
    size_t line;
    int flow; /* the root collection is open */
    ltok* toks;
    size_t ntoks;
    size_t captoks;
    /* the one flow-level simple key (the depth gate is 1) */
    int sk_possible;
    size_t sk_index;
    size_t sk_line;
    int sk_allowed;
} lctx;

static int l_fail_at(lctx* c, int line) {
    if (getenv("YD_FC_TRACE") != NULL) {
        fprintf(stderr, "libyaml gate at line %d (pos %zu of %zu)\n", line, c->pos, c->n);
    }
    return 0;
}
#define l_fail(c) l_fail_at((c), __LINE__)

static int l_push(lctx* c, ltkind k, size_t line) {
    if (c->ntoks >= FC_MAX_TOKENS) {
        return l_fail(c);
    }
    if (c->ntoks == c->captoks) {
        size_t nc = c->captoks ? c->captoks * 2 : 64;
        if (nc > FC_MAX_TOKENS) {
            nc = FC_MAX_TOKENS;
        }
        ltok* nt = realloc(c->toks, nc * sizeof(ltok));
        if (nt == NULL) {
            return l_fail(c);
        }
        c->toks = nt;
        c->captoks = nc;
    }
    memset(&c->toks[c->ntoks], 0, sizeof(ltok));
    c->toks[c->ntoks].kind = k;
    c->toks[c->ntoks].line = line;
    c->ntoks++;
    return 1;
}

/* scan_to_next_token: blanks (tabs always — flow), a comment to the
 * break, EVERY break eaten (the loop re-runs), then the token. */
static void l_scan_to_token(lctx* c) {
    for (;;) {
        while (c->pos < c->n && fc_blank(c->p[c->pos])) {
            c->pos++;
        }
        if (c->pos < c->n && c->p[c->pos] == '#') {
            while (c->pos < c->n && fc_break_len(c->p, c->n, c->pos) == 0) {
                c->pos++;
            }
        }
        int bl = fc_break_run_len(c->p, c->n, c->pos);
        if (bl > 0) {
            c->pos += (size_t)bl;
            c->line++;
            continue;
        }
        break;
    }
}

static void l_save_sk(lctx* c, size_t line) {
    if (!c->sk_allowed) {
        return;
    }
    c->sk_possible = 1;
    c->sk_index = c->ntoks; /* the token ABOUT to be pushed */
    c->sk_line = line;      /* the token's START line (staleness law) */
}

/* The plain-scalar scan (libyaml's dance, dump-exact): READ_LINE folds
 * CR/LF/CRLF/NEL into LF and rides LS/PS raw; a leading folded break
 * with no trailing joins ONE space, with trailing the trailing string
 * joins raw; interior blanks join byte-exact; everything pending at
 * the plain's end drops. Renders into `o`; returns the stop position,
 * 0 on a gate/error. */
static int l_plain_scan(lctx* c, fout* o, size_t* stop) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    size_t i = c->pos;
    /* leading break: kind 0 none / 1 folded LF / 2 raw bytes */
    int lb_kind = 0;
    uint8_t lb_raw[4];
    size_t lb_len = 0;
    /* trailing breaks: the exact byte string that JOINs raw */
    uint8_t tb[1024];
    size_t tb_n = 0;
    int leading_blanks = 0;
    size_t blanks_at = 0;
    size_t blanks_n = 0;
    for (;;) {
        if (i >= n) {
            break;
        }
        unsigned char ch = p[i];
        if (ch == '#') {
            break; /* a comment (blanks preceded it: the run below keeps
                    * glued '#' as content) */
        }
        /* the non-blank run (glued '#' rides; ':' + flow indicator is
         * libyaml's "found unexpected ':'") */
        size_t k = i;
        while (k < n) {
            unsigned char cc = p[k];
            if (fc_blank(cc) || fc_break_len(p, n, k) > 0) {
                break;
            }
            if (cc == ':' && (k + 1 >= n || fc_blank(p[k + 1]) || fc_break_len(p, n, k + 1) > 0)) {
                break; /* the value boundary */
            }
            if (cc == ':' && k + 1 < n && fc_flow_ind(p[k + 1])) {
                return l_fail(c);
            }
            if (fc_flow_ind(cc)) {
                break;
            }
            k++;
        }
        /* the join at the first content byte */
        if (k > i) {
            if (leading_blanks) {
                if (lb_kind == 1) {
                    if (tb_n == 0) {
                        fo_str(o, " ");
                    } else {
                        fo_bytes(o, tb, tb_n);
                    }
                } else if (lb_kind == 2) {
                    fo_bytes(o, lb_raw, lb_len);
                    fo_bytes(o, tb, tb_n);
                }
                lb_kind = 0;
                lb_len = 0;
                tb_n = 0;
                leading_blanks = 0;
            } else if (blanks_n > 0) {
                fo_bytes(o, p + blanks_at, blanks_n);
            }
            blanks_n = 0;
            fo_escape(o, p + i, k - i);
            i = k;
        }
        /* blank/break consumption */
        int ended = 0;
        for (;;) {
            if (i >= n) {
                ended = 1;
                break;
            }
            int bl = fc_break_run_len(p, n, i);
            if (bl > 0) {
                /* READ_LINE: CR/LF/CRLF/NEL -> LF; LS/PS raw 3 bytes */
                int raw = (p[i] == 0xE2);
                if (!leading_blanks) {
                    blanks_n = 0;
                    if (raw) {
                        /* the raw ride: the EXACT bytes join the value
                         * (LS/PS ride the dump unescaped) */
                        lb_kind = 2;
                        lb_raw[0] = p[i];
                        lb_raw[1] = p[i + 1];
                        lb_raw[2] = p[i + 2];
                        lb_len = 3;
                    } else {
                        lb_kind = 1;
                        lb_len = 0;
                    }
                    leading_blanks = 1;
                } else {
                    if (tb_n + 8 > sizeof(tb)) {
                        return l_fail(c); /* a break run beyond the budget */
                    }
                    if (raw) {
                        tb[tb_n++] = p[i];
                        tb[tb_n++] = p[i + 1];
                        tb[tb_n++] = p[i + 2];
                    } else {
                        /* the folded break rides the dump as the ESCAPED
                         * LF (the value byte is LF; the renderer escapes
                         * it — here the join writes dump text) */
                        tb[tb_n++] = '\\';
                        tb[tb_n++] = 'n';
                    }
                }
                i += (size_t)bl;
                c->line++;
                continue;
            }
            unsigned char cc = p[i];
            if (fc_blank(cc)) {
                if (!leading_blanks) {
                    if (blanks_n == 0) {
                        blanks_at = i;
                    }
                    blanks_n++;
                }
                i++;
                continue;
            }
            break;
        }
        if (ended) {
            break;
        }
        /* the outer loop: a comment ends the plain (its blanks drop) */
        if (p[i] == '#') {
            break;
        }
        if (fc_flow_ind(p[i])) {
            break;
        }
        /* a post-break ':' re-lexes as the VALUE token: the plain ends */
        if (p[i] == ':' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
            break;
        }
        if (p[i] == ':' && i + 1 < n && fc_flow_ind(p[i + 1])) {
            return l_fail(c);
        }
    }
    *stop = i;
    return 1;
}

/* The DQ scan (libyaml's scan_flow_scalar fold — CR/LF/NEL fold
 * identically to yeptris; exotic breaks are a gate; interior blanks
 * ride byte-exact, boundary blanks join the fold — see
 * fc_quoted_value). */
static int l_dq_scan(lctx* c, fout* o, size_t* stop) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    size_t i = c->pos + 1; /* past the open quote */
    int first = 1;
    uint32_t lead = 0;
    uint32_t run = 0;
    size_t blank_at = 0;
    size_t blank_n = 0;
    for (;;) {
        if (i >= n) {
            return l_fail(c); /* unterminated */
        }
        int bl = fc_break_run_len(p, n, i);
        if (bl > 0) {
            if (p[i] == 0xE2 || p[i] == 0xC2) {
                return l_fail(c);
            }
            if (first) {
                lead++;
            } else {
                run++;
            }
            blank_n = 0; /* the boundary blanks join the fold */
            i += (size_t)bl;
            c->line++;
            continue;
        }
        unsigned char cc = p[i];
        if (cc == '"') {
            break;
        }
        if (cc == '\\') {
            return l_fail(c); /* escapes are outside the family */
        }
        if (fc_blank(cc)) {
            if (blank_n == 0) {
                blank_at = i;
            }
            blank_n++;
            i++;
            continue;
        }
        if (first) {
            first = 0;
            if (blank_n > 0 || lead > 1) {
                return l_fail(c);
            }
            if (lead == 1) {
                fo_str(o, " ");
            }
        } else if (run > 0) {
            fo_fold_run(o, run); /* the boundary blanks on BOTH sides of
                                  * the break ride the fold */
            run = 0;
            blank_n = 0;
        } else if (blank_n > 0) {
            fo_escape(o, p + blank_at, blank_n); /* interior: byte-exact,
                                                  * dump-escaped */
            blank_n = 0;
        }
        fo_escape(o, p + i, 1);
        i++;
    }
    if (!first && (run > 0 || blank_n > 0)) {
        return l_fail(c); /* trailing blanks/breaks: untested */
    }
    *stop = i + 1; /* past the close */
    return 1;
}

/* The tokenizer (libyaml's fetch dispatch over the flow contract). */
static int l_tokenize(lctx* c) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    for (;;) {
        l_scan_to_token(c);
        /* staleness: a saved simple key dies on a line cross */
        if (c->sk_possible && c->sk_line < c->line) {
            c->sk_possible = 0;
        }
        if (c->pos >= n) {
            return l_push(c, LT_EOS, c->line);
        }
        size_t line = c->line;
        int col0 = (c->pos == 0 || fc_break_len(p, n, c->pos - 1) > 0);
        unsigned char ch = p[c->pos];
        int plain = 0;
        switch (ch) {
        case '[':
        case '{':
            if (c->flow) {
                return l_fail(c); /* nesting is outside the family */
            }
            c->flow = 1;
            c->sk_allowed = 1;
            if (!l_push(c, ch == '[' ? LT_FLSEQ : LT_FLMAP, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ']':
        case '}':
            if (!c->flow) {
                return l_fail(c);
            }
            c->flow = 0;
            c->sk_possible = 0;
            c->sk_allowed = 0;
            if (!l_push(c, ch == ']' ? LT_FLSEQ_END : LT_FLMAP_END, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ',':
            c->sk_possible = 0;
            c->sk_allowed = 1;
            if (!l_push(c, LT_ENTRY, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case '?':
            c->sk_possible = 0;
            c->sk_allowed = 0;
            if (!l_push(c, LT_KEY, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ':': {
            if (c->sk_possible) {
                /* the simple-key insertion: a KEY before the saved token */
                if (!l_push(c, LT_KEY, c->sk_line)) {
                    return l_fail(c);
                }
                memmove(&c->toks[c->sk_index + 1], &c->toks[c->sk_index],
                        (c->ntoks - c->sk_index - 1) * sizeof(ltok));
                c->toks[c->sk_index].kind = LT_KEY;
                c->toks[c->sk_index].line = c->sk_line;
                c->toks[c->sk_index].text = NULL;
                c->toks[c->sk_index].tlen = 0;
                c->toks[c->sk_index].name = NULL;
                c->toks[c->sk_index].name_len = 0;
                c->sk_possible = 0;
            }
            c->sk_allowed = 0;
            if (!l_push(c, LT_VALUE, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        }
        case '"':
        case '\'': {
            if (ch == '\'') {
                return l_fail(c); /* single quotes: outside the family */
            }
            fout v = {NULL, 0, 0, 0};
            size_t stop = 0;
            int ok = l_dq_scan(c, &v, &stop);
            if (!ok) {
                free(v.buf);
                return l_fail(c);
            }
            l_save_sk(c, line);
            if (!l_push(c, LT_SCALAR, line)) {
                free(v.buf);
                return l_fail(c);
            }
            c->toks[c->ntoks - 1].text = v.buf; /* owned (may be NULL) */
            c->toks[c->ntoks - 1].tlen = v.len;
            c->pos = stop;
            break;
        }
        case '&':
        case '*': {
            size_t s = c->pos + 1;
            size_t e = s;
            while (e < n && fc_alpha(p[e])) {
                e++;
            }
            size_t l = e - s;
            int term_ok = 0;
            if (e >= n) {
                term_ok = 1;
            } else {
                unsigned char t = p[e];
                if (fc_blank(t) || fc_break_len(p, n, e) > 0 || t == '?' || t == ':' || t == ',' ||
                    t == ']' || t == '}' || t == '%' || t == '@' || t == '`') {
                    term_ok = 1;
                }
            }
            if (l == 0 || !term_ok) {
                return l_fail(c); /* the scan_anchor error shape */
            }
            l_save_sk(c, line);
            if (!l_push(c, ch == '&' ? LT_ANCHOR : LT_ALIAS, line)) {
                return l_fail(c);
            }
            c->toks[c->ntoks - 1].name = p + s;
            c->toks[c->ntoks - 1].name_len = l;
            c->pos = e;
            break;
        }
        case '-':
            if (c->pos + 1 >= n || fc_blank(p[c->pos + 1]) || fc_break_len(p, n, c->pos + 1) > 0) {
                return l_fail(c); /* a block entry inside flow */
            }
            plain = 1; /* a glued '-' is plain content */
            break;
        case '%':
            if (col0) {
                return l_fail(c); /* a directive line */
            }
            plain = 1;
            break;
        case '@':
            return l_fail(c); /* libyaml cannot start a token here */
        case '|':
        case '>':
        case '!':
            return l_fail(c); /* block scalars / tags: outside the family */
        case '`':
            plain = 1;
            break;
        default:
            /* the document indicators at a line start */
            if (col0 && n - c->pos >= 3 &&
                (memcmp(p + c->pos, "---", 3) == 0 || memcmp(p + c->pos, "...", 3) == 0)) {
                return l_fail(c);
            }
            plain = 1;
            break;
        }
        if (!plain) {
            continue;
        }
        /* plain scalar */
        fout v = {NULL, 0, 0, 0};
        size_t stop = 0;
        int ok = l_plain_scan(c, &v, &stop);
        if (!ok) {
            free(v.buf);
            return l_fail(c);
        }
        l_save_sk(c, line);
        if (!l_push(c, LT_SCALAR, line)) {
            free(v.buf);
            return l_fail(c);
        }
        c->toks[c->ntoks - 1].text = v.buf; /* owned (may be NULL) */
        c->toks[c->ntoks - 1].tlen = v.len;
        c->pos = stop;
    }
}

/* ---- the libyaml parser (the flow state machine, event-exact) -------- */

static int l_render_node(lctx* c, fout* o, size_t* ip) {
    ltok* t = &c->toks[*ip];
    const uint8_t* anchor = NULL;
    size_t alen = 0;
    if (t->kind == LT_ANCHOR) {
        anchor = t->name;
        alen = t->name_len;
        (*ip)++;
        if (*ip >= c->ntoks) {
            return l_fail(c);
        }
        t = &c->toks[*ip];
        if (t->kind == LT_ANCHOR) {
            return l_fail(c); /* a second anchor: outside the family */
        }
    }
    if (t->kind == LT_ALIAS) {
        fo_str(o, "=ALI *");
        fo_bytes(o, t->name, t->name_len);
        fo_str(o, "\n");
        (*ip)++;
        return 1;
    }
    if (t->kind == LT_SCALAR) {
        fo_str(o, "=VAL :");
        if (anchor != NULL) {
            fo_str(o, " &");
            fo_bytes(o, anchor, alen);
        }
        fo_str(o, " ");
        fo_bytes(o, t->text, t->tlen);
        fo_str(o, "\n");
        (*ip)++;
        return 1;
    }
    /* a node-less position: the empty scalar (the anchor rides) */
    fo_str(o, "=VAL :");
    if (anchor != NULL) {
        fo_str(o, " &");
        fo_bytes(o, anchor, alen);
    }
    fo_str(o, " \n");
    return 1;
}

static const char* ltk_name(ltkind k) {
    switch (k) {
    case LT_FLSEQ:
        return "FLSEQ";
    case LT_FLMAP:
        return "FLMAP";
    case LT_FLSEQ_END:
        return "FLSEQ_END";
    case LT_FLMAP_END:
        return "FLMAP_END";
    case LT_ENTRY:
        return "ENTRY";
    case LT_KEY:
        return "KEY";
    case LT_VALUE:
        return "VALUE";
    case LT_SCALAR:
        return "SCALAR";
    case LT_ALIAS:
        return "ALIAS";
    case LT_ANCHOR:
        return "ANCHOR";
    case LT_EOS:
        return "EOS";
    default:
        return "ERROR";
    }
}

static int l_render_stream(lctx* c, fout* o) {
    size_t ip = 0;
    if (getenv("YD_FC_DEBUG") != NULL) {
        for (size_t q = 0; q < c->ntoks; q++) {
            fprintf(stderr, "tok[%zu] %s line=%zu\n", q, ltk_name(c->toks[q].kind),
                    c->toks[q].line);
        }
    }
    fo_str(o, "+STR\n");
    ltok* t = &c->toks[ip];
    int is_seq;
    if (t->kind == LT_FLSEQ) {
        is_seq = 1;
    } else if (t->kind == LT_FLMAP) {
        is_seq = 0;
    } else {
        return l_fail(c);
    }
    ip++;
    fo_str(o, is_seq ? "+SEQ\n" : "+MAP\n");
    int first = 1;
    for (;;) {
        if (ip >= c->ntoks) {
            return l_fail(c);
        }
        ltok* e = &c->toks[ip];
        if (e->kind == LT_EOS || e->kind == LT_ERROR) {
            return l_fail(c); /* the collection never closed */
        }
        if (e->kind == (is_seq ? LT_FLSEQ_END : LT_FLMAP_END)) {
            ip++;
            break;
        }
        if (!first) {
            if (e->kind != LT_ENTRY) {
                return l_fail(c);
            }
            ip++;
            if (ip >= c->ntoks) {
                return l_fail(c);
            }
            e = &c->toks[ip];
            /* libyaml re-peeks after the separator: the collection may
             * end right here (the trailing-comma shape) */
            if (e->kind == (is_seq ? LT_FLSEQ_END : LT_FLMAP_END)) {
                ip++;
                break;
            }
            if (e->kind == LT_EOS || e->kind == LT_ERROR) {
                return l_fail(c);
            }
        }
        first = 0;
        if (is_seq && e->kind == LT_KEY) {
            /* the single-pair mapping wrapper */
            ip++;
            fo_str(o, "+MAP\n");
            if (ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* k = &c->toks[ip];
            if (k->kind == LT_VALUE || k->kind == LT_ENTRY) {
                ip++; /* the SEQ mapping-key state consumes this token */
                fo_str(o, "=VAL : \n");
            } else if (k->kind == LT_FLSEQ_END) {
                fo_str(o, "=VAL : \n");
            } else {
                if (!l_render_node(c, o, &ip)) {
                    return l_fail(c);
                }
            }
            if (ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* v = &c->toks[ip];
            if (v->kind == LT_VALUE) {
                ip++;
                if (ip >= c->ntoks) {
                    return l_fail(c);
                }
                v = &c->toks[ip];
                if (v->kind == LT_ENTRY || v->kind == LT_FLSEQ_END) {
                    fo_str(o, "=VAL : \n");
                } else {
                    if (!l_render_node(c, o, &ip)) {
                        return l_fail(c);
                    }
                }
            } else {
                fo_str(o, "=VAL : \n");
            }
            fo_str(o, "-MAP\n");
            continue;
        }
        if (!is_seq && e->kind == LT_KEY) {
            ip++;
            if (ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* k = &c->toks[ip];
            if (k->kind == LT_VALUE || k->kind == LT_ENTRY || k->kind == LT_FLMAP_END) {
                fo_str(o, "=VAL : \n");
            } else {
                if (!l_render_node(c, o, &ip)) {
                    return l_fail(c);
                }
            }
            if (ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* v = &c->toks[ip];
            if (v->kind == LT_VALUE) {
                ip++;
                if (ip >= c->ntoks) {
                    return l_fail(c);
                }
                v = &c->toks[ip];
                if (v->kind == LT_ENTRY || v->kind == LT_FLMAP_END) {
                    fo_str(o, "=VAL : \n");
                } else {
                    if (!l_render_node(c, o, &ip)) {
                        return l_fail(c);
                    }
                }
            } else {
                fo_str(o, "=VAL : \n");
            }
            continue;
        }
        /* the key-less entry: the node is the key; the value empty on a
         * mapping (the parser's EMPTY-VALUE state) */
        if (!l_render_node(c, o, &ip)) {
            return l_fail(c);
        }
        if (!is_seq) {
            fo_str(o, "=VAL : \n");
        }
    }
    fo_str(o, is_seq ? "-SEQ\n" : "-MAP\n");
    if (ip >= c->ntoks || c->toks[ip].kind != LT_EOS) {
        return l_fail(c); /* anything after the root: not this family */
    }
    fo_str(o, "-STR\n");
    return 1;
}

static int l_derive(const uint8_t* data, size_t size, fout* out) {
    lctx c;
    memset(&c, 0, sizeof(c));
    c.p = data;
    c.n = size;
    int ok = l_tokenize(&c);
    if (ok) {
        ok = l_render_stream(&c, out);
    }
    /* free the stashed folded-scalar buffers */
    for (size_t k = 0; k < c.ntoks; k++) {
        free(c.toks[k].text);
    }
    free(c.toks);
    if (!ok) {
        free(out->buf);
        memset(out, 0, sizeof(*out));
        out->bad = 1;
        return 0;
    }
    return 1;
}

/* ---- the entry point ------------------------------------------------ */

static int fc_str_eq(const fout* o, const char* s) {
    if (o->buf == NULL || o->bad) {
        return 0;
    }
    return strcmp(o->buf, s) == 0;
}

int yd_family_classify(const uint8_t* data, size_t size, const char* ydump, const char* ldump) {
    if (data == NULL || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (size < 4 || size > FC_MAX_INPUT) {
        return 0; /* the budget gate (law v) */
    }
    if (data[0] != '[' && data[0] != '{') {
        return 0; /* the family is flow-rooted (law i) */
    }
    int is_map = data[0] == '{';
    for (size_t i = 0; i < size; i++) {
        unsigned char ch = data[i];
        if (ch == 0x00) {
            return 0; /* NUL: libyaml's stream end — outside the family */
        }
        if (ch == 0xEF && i + 2 < size && data[i + 1] == 0xBB && data[i + 2] == 0xBF) {
            return 0; /* a BOM rides other families */
        }
    }
    fout yd = {NULL, 0, 0, 0};
    fout ld = {NULL, 0, 0, 0};
    int ok = y_derive(data, size, is_map, &yd);
    if (ok) {
        ok = l_derive(data, size, &ld);
    }
    int classified = ok && fc_str_eq(&yd, ydump) && fc_str_eq(&ld, ldump);
    if (getenv("YD_FC_DEBUG") != NULL) {
        fprintf(stderr, "--- fc input (%zu bytes) classified=%d:\n", size, classified);
        fwrite(data, 1, size, stderr);
        fprintf(stderr, "\n--- fc derived yeptris:\n%s\n--- fc derived libyaml:\n%s\n",
                ok && yd.buf ? yd.buf : "(declined)", ok && ld.buf ? ld.buf : "(declined)");
        fprintf(stderr, "--- actual yeptris:\n%s\n--- actual libyaml:\n%s\n", ydump, ldump);
    }
    free(yd.buf);
    free(ld.buf);
    return classified;
}
