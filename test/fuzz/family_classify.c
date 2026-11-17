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
 *    scalar); `?`+blank/break splits the explicit key (the split slot's
 *    alias rides the same blank/break run to the separator/closer the
 *    bare-alias member walks — the r301 widening; a ':' across the run
 *    stays out); an alias at a `:`-boundary VALUE slot rides the same
 *    name cut and terminator walk (the r302 widening); a `:`-led
 *    member opens the EMPTY key (the r281 floor; the split-? key slot
 *    takes `:` + blank/break/flow-indicator — the r286 flow
 *    VALUE-boundary widening, the glued `?:` twin stays out); plain
 *    folds ride the
 *    1.2 law (1 break -> space, n -> n-1 newlines, interior blanks
 *    exact, leading/trailing runs trimmed); a `{` at the value slot
 *    opens a nested flow map derived recursively by the same member
 *    laws (law i);
 *  - the libyaml model: the 0.2.x scanner/parser contract the corpus
 *    walks adjudicated — scan_to_next_token eats blanks/tabs and every
 *    break kind in flow and comment-eats from any `#` to the line
 *    break; every flow `?` is the KEY token; every flow `:` is the
 *    VALUE token with the simple-key insertion (the simple-key slots
 *    ride PER FLOW LEVEL — the nested `{` pushes a fresh slot, and a
 *    line cross stales every level's saved key); the `!` tag is the
 *    shorthand scan (the alpha-run head rides into the suffix, %XX
 *    octet escapes decode per the UTF-8 discipline, only `!`/`!!` are
 *    default-resolvable handles, and the bare `!` resolves to the
 *    non-specific tag the adapter drops) attaching to the node that
 *    follows or riding the tagged empty scalar (the r292 law); plains
 *    fold per
 *    READ_LINE (CR/LF/CRLF/NEL -> LF, LS/PS ride RAW) with the
 *    leading/trailing-break dance (a leading folded break with no
 *    trailing joins one space; with trailing, the trailing string
 *    joins raw); the parser inserts empty scalars for KEY-without-node
 *    and value-without-token; a KEY inside a flow sequence opens the
 *    single-pair mapping wrapper (whose mapping-key state CONSUMES a
 *    following VALUE/FLOW-ENTRY token — the `[?#,x]` error shape); the
 *    flow-mapping body recurses at FLOW-MAPPING-START nodes (law i).
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

/* ---- budget (conservatism law n) ------------------------------------ */

#define FC_MAX_INPUT                                                                               \
    131072u                       /* the family's members are <1KB; the                            \
                                   * largest corpus seed is 10732; the                             \
                                   * 18.5KB r284 artifact rides whole; the                         \
                                   * 32.8KB r285 artifact rides whole; the                         \
                                   * 48.5KB r293 artifact rides whole; the                         \
                                   * 81.9KB r295 artifact rides whole */
#define FC_MAX_OUT (256u * 1024u) /* derived-dump cap */
#define FC_MAX_PIECES 256u        /* plain fold pieces per node */
#define FC_MAX_MEMBERS                                                                             \
    8192u /* flow members per collection (the                                                      \
           * 10KB r258 artifact rides ~300; the                                                    \
           * r284 soup rides ~1400; the r285 soup                                                  \
           * rides 2324; the r293 soup rides under                                                 \
           * 4096; the r295 soup rides just past                                                   \
           * 4096) */
#define FC_MAX_TOKENS                                                                              \
    16384u              /* libyaml tokens per stream (the r284 soup                                \
                         * rides ~5600; the r285 soup rides 5477; the                              \
                         * r293 soup rides just past 8192) */
#define FC_MAX_NEST 32u /* the modeled nesting depth (past it: UNKNOWN) */

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

static int fc_hex(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static unsigned fc_hexval(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return (unsigned)(c - '0');
    }
    if (c >= 'A' && c <= 'F') {
        return (unsigned)(c - 'A' + 10);
    }
    return (unsigned)(c - 'a' + 10);
}

/* scan_tag_uri's shorthand charset: alpha plus ;/?:@&=+$.!~*'()% — the
 * verbatim form's ,/[/] ride only inside !<...> (not modeled). '%' is
 * in the set but never rides raw: it opens the %XX octet escape. */
static int fc_uri_char(unsigned char c) {
    return fc_alpha(c) || c == ';' || c == '/' || c == '?' || c == ':' || c == '@' || c == '&' ||
           c == '=' || c == '+' || c == '$' || c == '.' || c == '!' || c == '~' || c == '*' ||
           c == '\'' || c == '(' || c == ')';
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
    size_t alias_at; /* an alias value (the r302 value-slot law; the
                      * engine's e_alias name cut renders verbatim) */
    size_t alias_len;
    char* nest_buf; /* a nested-collection render stashed by the value
                     * walk (fc_render_val consumes it at the caller's
                     * render point, after the key line; owned) */
    size_t nest_len;
} fcnode;

/* Scan one plain content piece inside [q, end): interior blanks ride
 * byte-exact; the piece stops at a break, a `:`-VALUE boundary, a flow
 * separator/close, or blanks that lead into one of those. A blank run
 * that leads into `#` is a COMMENT on both engines — the walk ends the
 * plain there (sets *ended_comment, the caller derives the run's ride;
 * the r279 law) instead of refusing. Returns the stop position and
 * whether it is a separator/close. */
static int fc_piece(const uint8_t* p, size_t n, size_t q, size_t end, int qm, size_t* piece_at,
                    size_t* piece_len, size_t* stop, int* stop_sep, int* ended_comment) {
    size_t k = q;
    size_t blank_n = 0;
    *ended_comment = 0;
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
                if (!qm) {
                    return 0; /* ':' + flow indicator: libyaml's "unexpected
                               * ':'" scanner error — outside the family */
                }
                /* the glued-`?` member (the r272 floor): the engine's
                 * KEY-implicit walk treats the colon + closer as the
                 * VALUE boundary, not a scanner error */
                break;
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
            *ended_comment = 1; /* the plain ends at the comment (the
                                 * r279 law); the tail-trim below drops
                                 * the blanks before the run */
            break;
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
static int fc_plain_pieces(const uint8_t* p, size_t n, size_t i, size_t end, int qm, fcnode* node,
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
            /* a whole-line comment run between pieces (the r277
             * members: `<` + a line-start `\t#...` eat + the `,`): the
             * comment ends the plain on BOTH engines and rides inert,
             * so the plain terminates at the next flow indicator.
             * Only the separator/closer follow-on is derived — a plain
             * byte resuming after the comment is a fresh member where
             * a separator was expected (the engines' "did not find
             * expected ',' or '}'" both-error, never consulted); a
             * run that eats to EOF never terminates. Everything else
             * stays UNKNOWN. The r307 mutant's widening: CONSECUTIVE
             * whole-line comments ride the same inert walk (`1.0e17
             * .tag\r #,|#\r#,??2002:binary\r\r,` — the first comment's
             * break is followed by a line-start `#` that opens another
             * comment line; each rides to its own break, both engines
             * byte-identically) before the terminator is demanded. */
            size_t e2 = q;
            for (;;) {
                e2 = q + 1;
                while (e2 < end && fc_break_len(p, n, e2) == 0) {
                    e2++;
                }
                if (e2 >= end) {
                    return 0;
                }
                /* the break run (and any further blanks/breaks) rides
                 * inert past the comment — the plain already ended */
                for (;;) {
                    size_t bl = fc_break_run_len(p, n, e2);
                    if (bl > 0) {
                        e2 += (size_t)bl;
                        continue;
                    }
                    if (e2 < end && fc_blank(p[e2])) {
                        e2++;
                        continue;
                    }
                    break;
                }
                if (e2 < end && p[e2] == '#') {
                    q = e2; /* the next line-start comment: ride it too */
                    continue;
                }
                break;
            }
            if (e2 >= end || !fc_flow_ind(p[e2])) {
                return 0;
            }
            *stop_at = e2;
            *stop_sep = 1;
            return 1;
        }
        if (node->n > 0 && br > 0 && q + 1 < end && p[q] == ':' && fc_flow_ind(p[q + 1])) {
            /* the post-break VALUE boundary (the r291 member: plain +
             * SP-TAB-SP-LF + `:` + `,`): the blank/break run ends the
             * plain and the re-lexed `:` + flow indicator is the 1.2
             * flow VALUE boundary (the r286 law, now at the post-break
             * continuation position) — the pair rides (plain, empty).
             * libyaml can only see these bytes when its comment-eat
             * swallows the whole member (the probe: `{b,` SP TAB SP LF
             * `:,}` is YEPTRIS-ONLY — libyaml's still-open plain hits
             * the `:`+flow-ind "unexpected ':'" scanner error), so the
             * twin derivation below never consults a libyaml-visible
             * instance. The glued no-break position keeps the fc_piece
             * !qm decline; a plain-led run (no break) and a first-piece
             * colon (a blank-led member) stay declined. */
            node->colon = 1;
            node->colon_at = q;
            *stop_at = q;
            *stop_sep = 0;
            return 1;
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
        int commented = 0;
        if (!fc_piece(p, n, q, end, qm, &pat, &plen, &stop, &sep, &commented)) {
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
        if (commented) {
            /* the plain-tail comment run (the r279 law, run
             * 43688038100): a blank-preceded `#` INSIDE a member's plain
             * ends the plain on BOTH engines (the run eats inertly to
             * the break — yeptris's comment-at-content rule, libyaml's
             * scan-to-token eat), so the member boundary after the
             * break must derive: a flow indicator (the plain terminates
             * there — the r277 whole-line law's ride) or a
             * `:`+blank/break VALUE boundary (the colon re-lexes as the
             * VALUE token — the r279 floor's `?#`+sp-`#`+LF+`:`+TAB).
             * Content resuming after the run, another comment line, or
             * an eat to EOF stays UNKNOWN. */
            size_t e2 = stop + 1; /* stop sits ON the '#' */
            while (e2 < end && fc_break_len(p, n, e2) == 0) {
                e2++;
            }
            if (e2 >= end) {
                return 0; /* the run eats to EOF: never terminates */
            }
            e2 += (size_t)fc_break_run_len(p, n, e2);
            while (e2 < end && fc_blank(p[e2])) {
                e2++;
            }
            if (e2 >= end) {
                return 0;
            }
            if (fc_flow_ind(p[e2])) {
                *stop_at = e2;
                *stop_sep = 1;
                return 1;
            }
            if (p[e2] == ':' &&
                (e2 + 1 >= end || fc_blank(p[e2 + 1]) || fc_break_len(p, n, e2 + 1) > 0)) {
                node->colon = 1;
                node->colon_at = e2;
                *stop_at = e2;
                *stop_sep = 0;
                return 1;
            }
            return 0;
        }
        if (stop < end && p[stop] == ':' &&
            (stop + 1 >= end || fc_blank(p[stop + 1]) || fc_break_len(p, n, stop + 1) > 0 ||
             (qm && stop + 1 < end && fc_flow_ind(p[stop + 1])))) {
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

static void fc_render_val(fout* o, const uint8_t* p, fcnode* nd) {
    if (nd->nest_buf != NULL) {
        /* the nested-collection render (the r281 law): the stashed
         * +MAP/-MAP block rides the node's slot and is consumed here */
        fo_bytes(o, nd->nest_buf, nd->nest_len);
        free(nd->nest_buf);
        nd->nest_buf = NULL;
        nd->nest_len = 0;
        return;
    }
    if (nd->n == 0) {
        if (nd->alias_len > 0) {
            /* the alias value (the r302 law): the event rides the name
             * verbatim (`=ALI *<name>`, the same render the key-slot
             * alias fires) */
            fo_str(o, "=ALI *");
            fo_bytes(o, p + nd->alias_at, nd->alias_len);
            fo_str(o, "\n");
            return;
        }
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

/* the stash drop for the member walks that decline AFTER a successful
 * value scan (the render path consumes it; a decline path must free) */
static void fc_val_drop(fcnode* nd) {
    free(nd->nest_buf);
    nd->nest_buf = NULL;
    nd->nest_len = 0;
}

/* ---- the yeptris model ---------------------------------------------- */

typedef struct {
    const uint8_t* p;
    size_t n;
    int is_map; /* the root collection kind */
    fout out;
    size_t members;
    size_t pos_trace;
    int depth;     /* the open nested-collection depth (FC_MAX_NEST cap) */
    int pair_open; /* the #741 colon-led SEQ single-pair stays open: the
                    * following members render as map entries (key +
                    * empty value; the bare colon member keeps its
                    * empty-key + value slot) until the closer */
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
 * blanks at a break boundary drop into the fold; a break-only span
 * folds to ONE space at the close (the r274 law). Gates: a backslash
 * (escapes), an exotic break, a leading run >1, any trailing
 * blank/break, a blank-led start (a blank run after a SINGLE leading
 * break joins the fold — both engines strip the continuation blanks,
 * the r287 arm; every other blank-led start declines). */
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
            /* the r308 widening (run 44160636260): an EXOTIC break inside
             * the span is a break like any other — yeptris's 1.2 fold law
             * counts NEL/LS/PS in the run and folds the whole run (the
             * artifact's `",?g??...` member renders its `/\n\r\r<LS>`
             * site as n-1 newlines with the LS folded; the actual dumps
             * carry zero raw LS bytes inside quoted values). The old
             * outright decline (untested) hid exactly this site. */
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
            if (lead_run > 1) {
                return 0; /* a multi-break-led span: untested */
            }
            if (lead_run == 1) {
                /* the leading single fold keeps one space; a leading
                 * blank run joins the fold — BOTH engines strip the
                 * continuation blanks (the r287 arm: the break folds,
                 * the blanks never ride) */
                fo_str(o, " ");
                blank_n = 0;
            } else if (blank_n > 0) {
                return 0; /* a blank-led span: untested */
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
    if (!first_content && blank_n > 0) {
        return 0; /* trailing blanks (pure or riding the tail run):
                   * libyaml's whitespaces join rides them RAW while
                   * yeptris trims the member's tail — an unmodeled split,
                   * stays UNKNOWN (the r278 twin) */
    }
    if (!first_content && run > 1) {
        return 0; /* a multi-break tail at the close: unprobed (the
                   * artifact's tail is the ONE-break fold) */
    }
    if (!first_content && run == 1) {
        /* the pending run joins the close: ONE break folds to the ONE
         * space (the r278 floor `?"2<CR>"` -> `2 ` — the artifact's
         * `...1.0e17.tag<CR>"` ends the value with the fold space) */
        fo_str(o, " ");
    }
    if (first_content && lead_run > 0) {
        if (lead_run > 1 || blank_n > 0) {
            return 0; /* a multi-break-only span or a break+blank span:
                       * the trailing join rides raw — untested */
        }
        /* the break-only span folds to ONE space at the close (the
         * engines' scan_flow_scalar end-join; both agree byte-exact —
         * the r274 law, ["<CR>"] -> " " on both) */
        fo_str(o, " ");
    }
    *close_at = i;
    return i < e && p[i] == '"';
}

/* The member loop shared by the root and the nested map walk (the r281
 * law): members to the matching `closer`, the byte after it in *after.
 * The wrong-kind closer, an empty/comment member, and a missing
 * terminator decline. Defined after y_member; forward-declared here. */
static int y_members(yctx* c, unsigned char closer, size_t i, size_t* after);

/* A value slot after a `:`-boundary: plain, empty, anchored-empty,
 * alias (the r302 law), or a nested flow map (a quoted / nested-seq
 * value slot is untested). The r309 widening (run 44178312079): a
 * whole-line comment reached ACROSS the blank/break run rides the
 * value slot — the `#` is blank/break-preceded (the engine's comment
 * condition), so both engines eat it to its break and the value
 * resumes after the run (the r309 artifact's
 * `50:<CR>##|,?<CR>2002:binary` site pairs
 * (/\n\n\n50, 2002:binary) on both engines; a GLUED colon-`#` is the
 * documented yeptris-rejects/libyaml-accepts split, never a comment,
 * and stays out). The ride loops (the r307 multi-comment law, one
 * slot deeper). */
static int y_value_node(yctx* c, size_t i, size_t* stop, fcnode* val) {
    const uint8_t* p = c->p;
    size_t q = fc_skip_blank_break(p, c->n, i);
    while (p[q] == '#' && q > i) {
        /* the blank/break-preceded value-slot comment ride: eat to the
         * break, then the run to the next content (the r295 loop-top
         * ride's mechanics, one slot deeper) */
        while (q < c->n && fc_break_len(p, c->n, q) == 0) {
            q++;
        }
        if (q >= c->n) {
            break; /* an eat to EOF: the caller's terminator gates fail */
        }
        q = fc_skip_blank_break(p, c->n, q);
    }
    if (q >= c->n || p[q] == ',' || p[q] == '}' || p[q] == ']') {
        memset(val, 0, sizeof(*val));
        *stop = q;
        return 1; /* the empty value (a separator/closer terminator) */
    }
    if (p[q] == '*') {
        /* the alias value slot (the r302 widening, run 44034382404: the
         * floor's `?#:<TAB>*% ,` member -- the fold-colon VALUE boundary
         * followed by the TAB-riding alias `*%`; the artifact's
         * `L:<TAB>*% <sp>,` plain-key twin rides the same slot). The
         * engine's e_alias name cut (the prop-char walk, the law-c
         * charset) renders the name verbatim, and a blank/break run
         * then rides to the member terminator -- the same walk the
         * bare-alias member and the r301 split-key slot take. Only a
         * separator/closer leaves the alias as the bare value: a ':'
         * VALUE boundary after the alias (a `alias:value` composition
         * the probes never cover) and any other byte decline. */
        size_t as = q + 1;
        size_t al = fc_alias_name(p, c->n, as, c->n);
        if (al == 0) {
            return y_fail(c); /* a name cut that dies at once */
        }
        size_t ve = fc_skip_blank_break(p, c->n, as + al);
        if (ve >= c->n || !fc_flow_ind(p[ve])) {
            return y_fail(c); /* a non-boundary byte after the name */
        }
        memset(val, 0, sizeof(*val));
        val->alias_at = as;
        val->alias_len = al;
        *stop = ve;
        return 1;
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
    if (p[q] == '{') {
        /* the nested flow map (the r281 law, run 43705128492): the
         * value node IS the nested collection — the same member laws
         * derive its content recursively (+MAP/-MAP around them; a
         * nested `[` declines, maps only). The render is stashed on
         * the node — the caller renders the key line first, then the
         * stash. The blank/break run after the close skips so the
         * caller's member-terminator check sees the post-value byte. */
        if (c->depth >= (int)FC_MAX_NEST) {
            return y_fail(c); /* past the nesting budget */
        }
        fout nf = {NULL, 0, 0, 0};
        fo_str(&nf, "+MAP\n");
        fout outer = c->out;
        c->out = nf;
        c->depth++;
        size_t after = 0;
        int ok = y_members(c, '}', q + 1, &after);
        c->depth--;
        nf = c->out;
        c->out = outer;
        if (!ok || nf.bad) {
            free(nf.buf);
            return 0;
        }
        fo_str(&nf, "-MAP\n");
        if (nf.bad) {
            free(nf.buf);
            return y_fail(c);
        }
        memset(val, 0, sizeof(*val));
        val->nest_buf = nf.buf;
        val->nest_len = nf.len;
        *stop = fc_skip_blank_break(p, c->n, after);
        return 1;
    }
    if (p[q] == '"' || p[q] == '\'' || p[q] == '!' || p[q] == '[') {
        return y_fail(c);
    }
    int sep;
    if (!fc_plain_pieces(p, c->n, q, c->n, 0, val, stop, &sep)) {
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
        /* the `?` directly followed by a flow indicator (the r275
         * minimal's `?,` member): the plain cannot start there (the
         * follower is not a plain-safe char), so both engines drop the
         * `?` -- the INDICATOR form, key and value both empty (the
         * probes: `{?,a}` -> (e,e)+(a,e), `{?}` -> (e,e), `{?\t}` ->
         * (e,e), SEQ wraps the single pair). Only the comma /
         * matching-closer forms are probed byte-exact; the
         * wrong-closer combos are BOTH-ERROR shapes the comparator
         * never consults. */
        if (nxt < n && (p[nxt] == ',' || p[nxt] == (c->is_map ? '}' : ']'))) {
            memset(&key, 0, sizeof(key));
            memset(&val, 0, sizeof(val));
            *term_at = nxt;
            if (c->is_map) {
                fc_render_val(&c->out, p, &key);
                fc_render_val(&c->out, p, &val);
            } else {
                fo_str(&c->out, "+MAP\n");
                fc_render_val(&c->out, p, &key);
                fc_render_val(&c->out, p, &val);
                fo_str(&c->out, "-MAP\n");
            }
            return 1;
        }
        /* GLUED: the `?` is content; the plain scan starts at it. The
         * SEQ-rooted walk rides the qm colon+flow-indicator boundary
         * (the r272 `[?::]` floor's KEY-implicit cut); the MAP-rooted
         * walk keeps the plain law. */
        if (!fc_plain_pieces(p, n, i, n, !c->is_map, &key, &stop, &sep)) {
            return y_fail(c);
        }
        if (key.colon) {
            if (!c->is_map) {
                /* the SEQ single-pair wrapper (the r272-mined `[?::]`
                 * floor): the engine opens the KEY-implicit map for the
                 * glued-`?` member and cuts key/value at the member's
                 * terminating colon (the piece walk rides interior
                 * colons and stops at the one a blank/break/member-end
                 * follows), the value slot empty when the colon sits at
                 * the member end. Anything but a flow-indicator
                 * terminator after the value scan declines. */
                if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
                    return 0;
                }
                if (stop >= n || !fc_flow_ind(p[stop])) {
                    fc_val_drop(&val);
                    return y_fail(c);
                }
                *term_at = stop;
                fo_str(&c->out, "+MAP\n");
                fc_render_val(&c->out, p, &key);
                fc_render_val(&c->out, p, &val);
                fo_str(&c->out, "-MAP\n");
                return 1;
            }
            if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
                return 0;
            }
        }
        if (stop >= n || !fc_flow_ind(p[stop])) {
            fc_val_drop(&val);
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
            /* the r301 widening (run 44006575967): a blank/break run
             * between the alias name and its boundary (the floor's
             * `? *L ,` member -- the nightly artifact's `?\t*%L\t\t ,`).
             * The bare-alias member's walk already rides this exact run
             * (fc_skip_blank_break) to the separator/closer, leaving the
             * map value slot to the parser's empty scalar; the split
             * explicit-key slot rides the same walk. Only the
             * separator/closer follow-on derives: a ':' reached across
             * the run (a VALUE boundary the probes never cover) is not
             * a flow indicator and any other byte fails the gate --
             * both decline. */
            size_t ve = fc_skip_blank_break(p, n, ae);
            if (ve >= n || !fc_flow_ind(p[ve])) {
                return y_fail(c); /* bytes after the alias name that are
                                   * not a blank/break-then-boundary ride:
                                   * outside the family */
            }
            stop = ve;
            sep = 1;
        }
    } else if (p[ks] == '#') {
        /* the comment at the key position (the r245 `?\r###|` members):
         * yeptris fires a comment at the content-start `#`; the run eats
         * to the break — the member's `,` separator AND any following
         * `?`+break ride inside it inertly — and whatever content
         * follows becomes the STOLEN KEY of this explicit-key member.
         * Two probed rides:
         *  - the r245 SEQ steal: exactly one `,` inside the run, SEQ
         *    root, the post-break content rides the single-pair
         *    wrapper (the m7/m8/m9/m10 probes: `[a,?\r#x,b\nc]` ->
         *    +MAP(c,), `[?##|,?\r###|,?\rb]` -> +MAP(b,));
         *  - the r277 MAP steal: ZERO separators inside the run, MAP
         *    root, the post-break content is the member's key slot.
         *    A GLUED `:` leading it (the r277 floor's `{?\t#\n:#\n}`:
         *    `:` + non-blank) is plain content on yeptris (the 1.2
         *    ns-plain-first rule; the glued-colon-plain ride) while
         *    libyaml's flow scan splits ANY colon as the VALUE token
         *    and the run's tail comment-eats — so the yeptris dump
         *    keeps the whole plain as the key (value empty) and the
         *    libyaml dump renders the empty-key pair. A non-colon-led
         *    stolen key agrees on both engines (never consulted).
         * Gates: at most one separator inside the run, a terminator
         * exists, the r245 steal stays SEQ-rooted, a clean eat stays
         * MAP-rooted, and the stolen key is flat content (structural
         * and colon+blank-led keys decline). */
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
        if (seps >= 2 || e >= n) {
            return y_fail(c);
        }
        if (seps == 1 && c->is_map) {
            return y_fail(c); /* the r245 steal stays SEQ-rooted */
        }
        if (seps == 0 && !c->is_map) {
            return y_fail(c); /* a clean eat on the SEQ root: untested */
        }
        e += (size_t)fc_break_run_len(p, n, e);
        ks = fc_skip_blank_break(p, n, e);
        if (ks >= n) {
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
        if (!fc_plain_pieces(p, n, ks, n, 0, &key, &stop, &sep)) {
            return y_fail(c);
        }
    } else if (p[ks] == '"' || p[ks] == '\'' || p[ks] == '&' || p[ks] == '!' || p[ks] == '[' ||
               p[ks] == '{') {
        return y_fail(c);
    } else {
        if (p[ks] == ':' && ks + 1 < n &&
            (fc_blank(p[ks + 1]) || fc_break_len(p, n, ks + 1) > 0 || fc_flow_ind(p[ks + 1]))) {
            /* the colon-led key slot (the r275 artifact's `?`+break+`:`
             * +blank members; the r286 widening: the colon may be
             * followed by a FLOW INDICATOR too — the 1.2 flow
             * VALUE-boundary law, `?\t:\,` reads the empty key and the
             * empty value on BOTH engines while libyaml's 1.1 token
             * dispatch fires KEY, VALUE, FLOW-ENTRY on the same bytes).
             * The engines agree byte-exact -- the `?`
             * drops, the key is EMPTY, and the colon boundary rides the
             * common colon path below (the probes: `?\n:\t ,b` ->
             * (e,e)+(b,e), `?\n:\tb` -> (e,b), `{a,?\n:\t }` ->
             * (a,e)+(e,e), SEQ wraps the single pair; the r286 probes:
             * `{?\t:,}`, `{?\t:}`, `{?\t:\t}`, `{a,?\t:,}`, `[?\t:,]`,
             * `{?\t:{a}}` all EQUAL — the GLUED `?:,` twin stays
             * red/DIFFER, out of this arm). The colon at EOF still
             * declines. */
            memset(&key, 0, sizeof(key)); /* the empty key node */
            key.colon = 1;
            key.colon_at = ks;
        } else {
            if (!fc_plain_pieces(p, n, ks, n, 0, &key, &stop, &sep)) {
                return y_fail(c);
            }
        }
    }
    if (key.colon) {
        if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
            return 0;
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        fc_val_drop(&val);
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
        /* bounded by the remaining bytes: a fixed %.24s precision reads
         * past the input buffer when the member sits near the end */
        size_t left = n - i;
        if (left > 24) {
            left = 24;
        }
        fprintf(stderr, "member at %zu (%c): %.*s\n", (size_t)i, ch, (int)left, (const char*)p + i);
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
                fc_val_drop(&val);
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
    if (ch == '&') {
        /* the bare anchor member (the r284 floor's `&m` between two
         * glued-`?` members): the engine's e_anchor walk cuts the name
         * and the member's flow-indicator terminator leaves the anchored
         * node EMPTY — both engines render the prop, the separator
         * blank, and the empty value (`=VAL : &m `, the r154 props
         * law's agreeing shape). Gates: a non-empty all-IS_ALPHA name
         * (the one charset where the engine's prop cut and libyaml's
         * scan_anchor cut agree byte-exact — wider names ride the Y2GN
         * interior-`:` law and diverge), the terminator glued right
         * after the name (blank/break opens a value slot: untested), and
         * the SEQ root (a MAP key slot rides the r76 anchor-keymap
         * arm). */
        size_t as = i + 1;
        size_t al = 0;
        while (as + al < n && fc_alpha(p[as + al])) {
            al++;
        }
        size_t ae = as + al;
        if (al == 0 || c->is_map || ae >= n || !fc_flow_ind(p[ae])) {
            return y_fail(c);
        }
        *term_at = ae;
        fo_str(&c->out, "=VAL : &");
        fo_bytes(&c->out, p + as, al);
        fo_str(&c->out, " \n");
        return 1;
    }
    if (ch == '\'' || ch == '!' || ch == '#' || ch == '[' || ch == '{') {
        return y_fail(c); /* outside the family's flat-member shape */
    }
    if (ch == '-' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0 ||
                      fc_flow_ind(p[i + 1]))) {
        return y_fail(c); /* a block entry / the bare-dash entry error */
    }
    fcnode key, val;
    memset(&val, 0, sizeof(val));
    size_t stop;
    int sep;
    int colon_led = 0;
    if (ch == ':' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
        /* the colon-led member (the r281 floor's second member): the
         * `?` drops-and-empties twin — the key is EMPTY (colon=1, the
         * SEQ single-pair wrapper keeps the pair) and the value node
         * rides the common slot below. libyaml can only see this
         * member when its `:` rode a comment run (a raw flow `:`
         * after a separator errors the parser's node state), so the
         * branch is reachable exactly on the family's comment-eat
         * shapes — the floor's `#,:` run. */
        colon_led = 1;
        memset(&key, 0, sizeof(key));
        key.colon = 1;
        key.colon_at = i;
        if (!y_value_node(c, i + 1, &stop, &val)) {
            return 0;
        }
    } else {
        if (!fc_plain_pieces(p, n, i, n, 0, &key, &stop, &sep)) {
            return y_fail(c);
        }
        if (key.colon) {
            if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
                return 0;
            }
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        fc_val_drop(&val);
        return y_fail(c);
    }
    *term_at = stop;
    if (c->is_map) {
        fc_render_val(&c->out, p, &key);
        fc_render_val(&c->out, p, &val);
    } else if (colon_led) {
        /* the SEQ wrapper opens and STAYS OPEN (the #741 law: closing
         * at the comma spilled the value indicator into a second
         * entry — the pair survives the separator, so every following
         * member is a map entry until the closer; y_members renders
         * the -MAP when it consumes the closer) */
        fo_str(&c->out, "+MAP\n");
        fc_render_val(&c->out, p, &key);
        fc_render_val(&c->out, p, &val);
        c->pair_open = 1;
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

/* The fold-in member walk (the #741 law's map-entry side, probed on
 * the r306 floor): with the colon-led pair open, every following
 * member renders as a MAP ENTRY — the plain member (glued colons and
 * questions ride as content, P5/P7), the quoted span (P6), and the
 * anchored-empty member (`=VAL : &<name> `, P8) all take the key slot
 * with the parser's empty-scalar value; the bare-colon member keeps
 * its empty key with the value node riding (P4). Everything else
 * declines (the alias member, an explicit `?`-key member, a nested
 * collection: unprobed under the open pair). */
static int y_pair_member(yctx* c, size_t i, size_t* term_at) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    c->pos_trace = i;
    unsigned char ch = p[i];
    if (ch == ':' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
        /* the bare-colon fold-in member: the empty key and the value
         * node (P4's `:\rn` pair) */
        fcnode val;
        memset(&val, 0, sizeof(val));
        size_t stop;
        if (!y_value_node(c, i + 1, &stop, &val)) {
            return 0;
        }
        if (stop >= n || !fc_flow_ind(p[stop])) {
            fc_val_drop(&val);
            return y_fail(c);
        }
        *term_at = stop;
        fo_str(&c->out, "=VAL : \n");
        fc_render_val(&c->out, p, &val);
        return 1;
    }
    if (ch == '"') {
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
        fo_str(&c->out, "\n=VAL : \n");
        free(tmp.buf);
        return 1;
    }
    if (ch == '&' || ch == '*') {
        /* the prop-char walk is shared: an anchor takes the key slot as
         * the anchored-empty render (`=VAL : &<name> `, P8); an alias
         * member declines (unprobed under the open pair) */
        if (ch == '*') {
            return y_fail(c);
        }
        size_t as = i + 1;
        size_t al = 0;
        while (as + al < n && fc_alpha(p[as + al])) {
            al++;
        }
        size_t ae = as + al;
        if (al == 0 || ae >= n || !fc_flow_ind(p[ae])) {
            return y_fail(c);
        }
        *term_at = ae;
        fo_str(&c->out, "=VAL : &");
        fo_bytes(&c->out, p + as, al);
        fo_str(&c->out, " \n=VAL : \n");
        return 1;
    }
    if (ch == '!') {
        /* the non-specific tag (the m47 mutant's law): `!` followed by
         * a blank/break run DROPS on yeptris (t4's `!\rm` renders the
         * plain key `m`, t5's `!\rK ?#` renders `K ?#`) — the node
         * rides the run as the plain key; a GLUED local tag stays
         * unmodeled (T1's `<!m> ` tagged-empty render needs the tag
         * machinery) and declines */
        if (!(i + 1 < n && (fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0))) {
            return y_fail(c);
        }
        i = fc_skip_blank_break(p, n, i + 1);
        if (i >= n) {
            return y_fail(c);
        }
        ch = p[i];
        if (ch == ':' || ch == '&' || ch == '*' || ch == '"' || ch == '\'' || ch == '!' ||
            ch == '#' || ch == '[' || ch == '{' || ch == '}' || ch == ',' || ch == ']') {
            return y_fail(c); /* a tagged non-plain node: unprobed */
        }
    }
    if (ch == '?' || ch == '\'' || ch == '!' || ch == '#' || ch == '[' || ch == '{') {
        return y_fail(c); /* the explicit-key member and the rest of the
                           * non-flat spell: unprobed under the open pair */
    }
    if (ch == '-' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0 ||
                      fc_flow_ind(p[i + 1]))) {
        return y_fail(c);
    }
    /* the plain member: glued colons and questions ride as content (P5's
     * `a:b`, P7's `?a`) — a `:`-VALUE boundary declines (the pair value
     * under an open pair is unprobed) */
    fcnode key;
    memset(&key, 0, sizeof(key));
    size_t stop;
    int sep;
    if (!fc_plain_pieces(p, n, i, n, 0, &key, &stop, &sep)) {
        return y_fail(c);
    }
    if (key.colon) {
        return y_fail(c);
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        return y_fail(c);
    }
    *term_at = stop;
    fc_render_val(&c->out, p, &key);
    fo_str(&c->out, "=VAL : \n");
    return 1;
}

/* The member loop shared by the root and the nested map walk: members
 * to the matching `closer`, the byte after it in *after. The wrong-kind
 * closer, an empty member, and a missing terminator decline (the nested
 * walk passes `}`; the root passes its own kind's closer — a `]` inside
 * a map (and vice versa) hits the decline). A loop-top comment rides:
 * both engines eat `#` to the break between members (the r295 artifact's
 * `, #` site — verified identical on both engines, so the comment
 * contributes no events and the walk continues after the break). */
static int y_members(yctx* c, unsigned char closer, size_t i, size_t* after) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    for (;;) {
        size_t pre = i;
        i = fc_skip_blank_break(p, n, i);
        if (i >= n) {
            return y_fail(c); /* never closed */
        }
        unsigned char ch = p[i];
        if (ch == closer) {
            if (c->pair_open) {
                /* the #741 open pair closes with the collection */
                fo_str(&c->out, "-MAP\n");
                c->pair_open = 0;
            }
            i++;
            break;
        }
        if (ch == '#' && i > pre) {
            /* the comment ride: a blank/break preceded the `#` (the
             * engine's comment condition — `,`- or `[`-glued `#` is the
             * documented yeptris-rejects/libyaml-accepts split, never a
             * comment), so both engines eat to the break and the walk
             * re-enters (the tail gate's own law, moved between
             * members — the r295 artifact's `, #` site) */
            while (i < n && fc_break_len(p, n, i) == 0) {
                i++;
            }
            continue;
        }
        if (ch == ',' || ch == (c->is_map ? ']' : '}')) {
            c->pos_trace = i;
            if (getenv("YD_FC_TRACE") != NULL) {
                fprintf(stderr, "loop-top byte at %zu: %02x (prev 24: %.*s)\n", i, ch,
                        (int)(i > 24 ? 24 : i), (const char*)p + i - (i > 24 ? 24 : i));
            }
            return y_fail(c); /* an empty member: outside */
        }
        if (c->members >= FC_MAX_MEMBERS) {
            return y_fail(c);
        }
        c->members++;
        size_t term;
        int ok;
        if (c->pair_open) {
            ok = y_pair_member(c, i, &term); /* the #741 map-entry side */
        } else {
            ok = y_member(c, i, &term);
        }
        if (!ok) {
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
        if (p[i] == closer) {
            if (c->pair_open) {
                fo_str(&c->out, "-MAP\n");
                c->pair_open = 0;
            }
            i++;
            break;
        }
        return y_fail(c);
    }
    *after = i;
    return 1;
}

/* The root walk: members to the matching close, then the tail gate. */
static int y_root(yctx* c) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    c->pos_trace = 1;
    /* the leading blank/break run before the root bracket (the r296
     * artifact's `\n [` head): both engines scan-to-token before the
     * root, so the members start past the run (the caller's gate
     * rejects any other lead byte) */
    size_t lead = fc_skip_blank_break(p, n, 0);
    fo_str(&c->out, "+STR\n");
    fo_str(&c->out, c->is_map ? "+MAP\n" : "+SEQ\n");
    size_t after = 0;
    if (!y_members(c, c->is_map ? (unsigned char)'}' : (unsigned char)']', lead + 1, &after)) {
        return 0;
    }
    fo_str(&c->out, c->is_map ? "-MAP\n" : "-SEQ\n");
    /* the tail: blanks, breaks, comments, then end of input */
    size_t i = fc_skip_blank_break(p, n, after);
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
    LT_TAG,
    LT_EOS,
    LT_ERROR
} ltkind;

typedef struct {
    ltkind kind;
    size_t line;
    char* text;          /* SCALAR: the folded value (already dump-escaped);
                          * TAG: the RESOLVED event tag (the handle prefix +
                          * the URI-decoded suffix); owned */
    size_t tlen;         /* SCALAR: the folded length; TAG: the tag length */
    const uint8_t* name; /* ALIAS/ANCHOR: the name span (input view) */
    size_t name_len;
} ltok;

typedef struct {
    const uint8_t* p;
    size_t n;
    size_t pos;
    size_t line;
    int depth;                               /* the open flow level (0 = outside; the root = 1) */
    unsigned char open_map[FC_MAX_NEST + 2]; /* per level: `{` opened it */
    int sk_possible[FC_MAX_NEST + 2];        /* libyaml's simple_keys: one slot
                                              * PER FLOW LEVEL */
    size_t sk_index[FC_MAX_NEST + 2];
    size_t sk_line[FC_MAX_NEST + 2];
    int sk_allowed; /* the single parser-wide flag */
    ltok* toks;
    size_t ntoks;
    size_t captoks;
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
    c->sk_possible[c->depth] = 1;
    c->sk_index[c->depth] = c->ntoks; /* the token ABOUT to be pushed */
    c->sk_line[c->depth] = line;      /* the token's START line (staleness law) */
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
                /* interior: byte-exact, dump-escaped (the r289 repair:
                 * the raw ride leaked TAB bytes into the derived dump —
                 * the actual driver escapes TAB as \t; SP is
                 * escape-neutral either way, so no classified seed
                 * changes) */
                fo_escape(o, p + blanks_at, blanks_n);
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

/* The DQ scan (libyaml's scan_flow_scalar: interior blanks ride
 * byte-exact, boundary blanks join the fold — see fc_quoted_value's
 * twin gates). The in-span break runs join per libyaml's
 * leading_break/trailing_breaks dance over READ_LINE (the r278 law):
 * the run's blanks drop; a run whose FIRST break is CR/LF/CRLF/NEL
 * folds to ONE space when it is the only break, else the leading
 * break discards and the REST join (CR/LF/NEL -> '\n', LS/PS raw); a
 * run led by LS/PS joins EVERY break (LS/PS raw, the others '\n').
 * The probes: "a<CR>b" -> `a b`, "a<CR><CR>b" -> `a\nb`,
 * "a<PS>b" -> `a<PS>b`, "a<CR><PS>b" -> `a<PS>b`,
 * "a<PS><CR>b" -> `a<PS>\nb`, "a<CR><PS><CR>b" -> `a<PS>\nb`. */

#define FC_MAX_BRK_RUN                                                                             \
    64u /* break kinds remembered per run; past it:                                                \
         * decline */

typedef struct {
    uint8_t raw;   /* 0: joins as '\n'; 1: the LS/PS bytes ride */
    uint8_t b3[2]; /* the raw bytes after the E2 lead (80 A8/A9) */
} fcbrk;

static void fc_brk_join(fout* o, const fcbrk* run, uint32_t run_n) {
    uint32_t k = 0;
    if (!run[0].raw) {
        if (run_n == 1) {
            fo_str(o, " ");
            return;
        }
        k = 1; /* the leading NL break discards into the fold */
    }
    for (; k < run_n; k++) {
        if (run[k].raw) {
            fo_str(o, "\xe2");
            fo_bytes(o, run[k].b3, 2);
        } else {
            fo_str(o, "\\n"); /* the dump-escaped LF (yd_escape_append) */
        }
    }
}

static int l_dq_scan(lctx* c, fout* o, size_t* stop) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    size_t i = c->pos + 1; /* past the open quote */
    int first = 1;
    fcbrk kinds[FC_MAX_BRK_RUN];
    uint32_t run_n = 0;
    size_t blank_at = 0;
    size_t blank_n = 0;
    for (;;) {
        if (i >= n) {
            return l_fail(c); /* unterminated */
        }
        int bl = fc_break_run_len(p, n, i);
        if (bl > 0) {
            if (run_n >= FC_MAX_BRK_RUN) {
                return l_fail(c); /* a run past the cap: unmodeled */
            }
            if (p[i] == 0xE2) {
                /* LS/PS ride RAW (READ_LINE); remember the exact bytes */
                kinds[run_n].raw = 1;
                kinds[run_n].b3[0] = p[i + 1];
                kinds[run_n].b3[1] = p[i + 2];
            } else {
                kinds[run_n].raw = 0; /* CR/LF/CRLF/NEL join as '\n' */
            }
            run_n++;
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
            if (blank_n > 0 && (run_n != 1 || kinds[0].raw)) {
                return l_fail(c); /* a blank-led span with no single
                                   * leading CR/LF/NEL break: untested */
            }
            if (run_n > 0) {
                fc_brk_join(o, kinds, run_n); /* the leading fold (the
                                               * r274 NL twin; PS-led
                                               * rides raw); a following
                                               * blank run joins the fold —
                                               * libyaml's leading_blanks
                                               * loop eats it (the r287
                                               * arm, fc_quoted_value's
                                               * twin) */
                run_n = 0;
                blank_n = 0;
            }
        } else if (run_n > 0) {
            fc_brk_join(o, kinds, run_n); /* the boundary blanks on BOTH
                                           * sides of the run ride the
                                           * join */
            run_n = 0;
            blank_n = 0;
        } else if (blank_n > 0) {
            fo_escape(o, p + blank_at, blank_n); /* interior: byte-exact,
                                                  * dump-escaped */
            blank_n = 0;
        }
        fo_escape(o, p + i, 1);
        i++;
    }
    if (!first && blank_n > 0 && run_n == 0) {
        return l_fail(c); /* pure trailing blanks: libyaml's whitespaces
                           * join rides them RAW while yeptris trims the
                           * member's tail — an unmodeled split, stays
                           * UNKNOWN */
    }
    if (run_n > 0) {
        /* the pending run joins the close (the r278 floor:
         * `?"2<CR>"` -> `2 `; the break-only span is the r274 NL twin,
         * `?"<CR>"` -> ` ` — PS-led rides raw) */
        fc_brk_join(o, kinds, run_n);
    }
    *stop = i + 1; /* past the close */
    return 1;
}

/* The tokenizer (libyaml's fetch dispatch over the flow contract). */
/* libyaml's scan_tag (the shorthand forms; the verbatim !<...> form is
 * outside the modeled family). Fills `tag` with the RESOLVED event tag
 * bytes and advances past the tag; returns 0 on every error shape:
 *  - the handle forms: '!h!' with a nonempty h is the "undefined tag
 *    handle" parser error (the default directives carry only '!' and
 *    '!!'), and '!!' resolves to tag:yaml.org,2002: — neither rides;
 *  - the suffix form: handle '!', the suffix starts with the alpha run
 *    (scan_tag_uri copies the head minus the leading '!') and continues
 *    over the URI charset, decoding %XX octet escapes per the UTF-8
 *    leading/trailing-octet discipline (scan_uri_escapes);
 *  - the end check: in flow only ',' may end the tag without a blank
 *    ('did not find expected whitespace or line break');
 *  - the bare '!' is the special case (handle '' + suffix '!'): the
 *    resolved tag is the bare '!' — the adapter drops it ("resolves by
 *    style", diff_core.c). */
static int l_scan_tag(lctx* c, fout* tag) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    size_t i = c->pos + 1; /* past the '!' */
    if (i < n && p[i] == '<') {
        return 0; /* the verbatim form */
    }
    size_t h = i;
    while (h < n && fc_alpha(p[h])) {
        h++;
    }
    if (h < n && p[h] == '!') {
        return 0; /* a '!h!'/ '!!' handle: unmodeled */
    }
    fo_str(tag, "!");            /* the '!' handle's default prefix */
    fo_bytes(tag, p + i, h - i); /* the head minus the leading '!' */
    for (;;) {
        if (h >= n) {
            break; /* IS_BLANKZ at EOF: the tag ends clean */
        }
        unsigned char t = p[h];
        if (fc_blank(t) || fc_break_len(p, n, h) > 0) {
            break;
        }
        if (t == '%') {
            int width = 0;
            do {
                if (h + 2 >= n || p[h] != '%' || !fc_hex(p[h + 1]) || !fc_hex(p[h + 2])) {
                    return 0; /* 'did not find URI escaped octet' */
                }
                unsigned char octet =
                    (unsigned char)((fc_hexval(p[h + 1]) << 4) + fc_hexval(p[h + 2]));
                if (!width) {
                    width = (octet & 0x80) == 0x00   ? 1
                            : (octet & 0xE0) == 0xC0 ? 2
                            : (octet & 0xF0) == 0xE0 ? 3
                            : (octet & 0xF8) == 0xF0 ? 4
                                                     : 0;
                    if (!width) {
                        return 0; /* 'found an incorrect leading UTF-8
                                   * octet' */
                    }
                } else if ((octet & 0xC0) != 0x80) {
                    return 0; /* 'found an incorrect trailing UTF-8
                               * octet' */
                }
                fo_bytes(tag, &octet, 1);
                h += 3;
            } while (--width);
            continue;
        }
        if (!fc_uri_char(t)) {
            if (t == ',' && c->depth > 0) {
                break; /* the flow tag-end carve-out */
            }
            return 0; /* 'did not find expected whitespace or line
                       * break' */
        }
        fo_bytes(tag, &t, 1);
        h++;
    }
    c->pos = h;
    return 1;
}

static int l_tokenize(lctx* c) {
    const uint8_t* p = c->p;
    size_t n = c->n;
    for (;;) {
        l_scan_to_token(c);
        /* staleness: a saved simple key dies on a line cross (EVERY
         * level's slot — libyaml's stale_simple_keys loop) */
        for (size_t d = 0; d <= (size_t)c->depth; d++) {
            if (c->sk_possible[d] && c->sk_line[d] < c->line) {
                c->sk_possible[d] = 0;
            }
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
            if (c->depth > 0) {
                return l_fail(c); /* a nested flow SEQUENCE: outside the
                                   * modeled family (maps only) */
            }
            l_save_sk(c, line); /* the collection may be a simple key */
            c->depth++;
            c->open_map[c->depth] = 0;
            c->sk_possible[c->depth] = 0; /* the fresh level's slot */
            c->sk_allowed = 1;
            if (!l_push(c, LT_FLSEQ, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case '{':
            if (c->depth >= (int)FC_MAX_NEST) {
                return l_fail(c); /* past the nesting budget */
            }
            l_save_sk(c, line); /* the collection may be a simple key */
            c->depth++;
            c->open_map[c->depth] = 1;
            c->sk_possible[c->depth] = 0;
            c->sk_allowed = 1;
            if (!l_push(c, LT_FLMAP, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ']':
        case '}':
            if (c->depth == 0) {
                return l_fail(c);
            }
            if ((ch == '}') != (c->open_map[c->depth] != 0)) {
                return l_fail(c); /* a mismatched closer */
            }
            c->sk_possible[c->depth] = 0; /* remove the level's key */
            c->depth--;
            c->sk_allowed = 0;
            if (!l_push(c, ch == ']' ? LT_FLSEQ_END : LT_FLMAP_END, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ',':
            c->sk_possible[c->depth] = 0;
            c->sk_allowed = 1;
            if (!l_push(c, LT_ENTRY, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case '?':
            c->sk_possible[c->depth] = 0;
            c->sk_allowed = 0;
            if (!l_push(c, LT_KEY, line)) {
                return l_fail(c);
            }
            c->pos++;
            break;
        case ':': {
            if (c->sk_possible[c->depth]) {
                /* the simple-key insertion: a KEY before the saved token */
                size_t ski = c->sk_index[c->depth];
                if (!l_push(c, LT_KEY, c->sk_line[c->depth])) {
                    return l_fail(c);
                }
                memmove(&c->toks[ski + 1], &c->toks[ski], (c->ntoks - ski - 1) * sizeof(ltok));
                c->toks[ski].kind = LT_KEY;
                c->toks[ski].line = c->sk_line[c->depth];
                c->toks[ski].text = NULL;
                c->toks[ski].tlen = 0;
                c->toks[ski].name = NULL;
                c->toks[ski].name_len = 0;
                c->sk_possible[c->depth] = 0;
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
            return l_fail(c); /* block scalars: outside the family */
        case '!': {
            /* the tag (the r292 member: the glued-`?` KEY slot's
             * `!(4%21bz!` shorthand — the %21 decodes to '!'). The
             * resolved tag rides the token's text. */
            fout tag = {NULL, 0, 0, 0};
            if (!l_scan_tag(c, &tag)) {
                free(tag.buf);
                return l_fail(c);
            }
            l_save_sk(c, line); /* fetch_tag's save_simple_key (flow keeps
                                 * the slot live) */
            if (!l_push(c, LT_TAG, line)) {
                free(tag.buf);
                return l_fail(c);
            }
            c->toks[c->ntoks - 1].text = tag.buf; /* owned */
            c->toks[c->ntoks - 1].tlen = tag.len;
            break;
        }
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

static int l_render_map(lctx* c, fout* o, size_t* ip);

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
        if (t->kind == LT_TAG) {
            return l_fail(c); /* an anchor-then-tag node: untested */
        }
    }
    if (t->kind == LT_TAG) {
        /* the tag attaches to the node that follows (parse_node's
         * properties walk); the next token decides: a scalar carries the
         * tag in the props slot, any non-node token leaves the tagged
         * EMPTY scalar (the parser's anchor||tag branch). The bare '!'
         * resolves to the non-specific tag and drops (diff_core.c:
         * "resolves by style"). The multi-prop and tagged-collection
         * combos stay outside. */
        const char* tagb = (const char*)t->text;
        size_t tl = t->tlen;
        int dropped = (tl == 1 && tagb[0] == '!');
        (*ip)++;
        if (*ip >= c->ntoks) {
            return l_fail(c);
        }
        ltok* t2 = &c->toks[*ip];
        if (t2->kind == LT_TAG || t2->kind == LT_ANCHOR || t2->kind == LT_ALIAS ||
            t2->kind == LT_FLMAP) {
            return l_fail(c);
        }
        fo_str(o, "=VAL :");
        if (!dropped) {
            fo_str(o, " <");
            fo_bytes(o, tagb, tl);
            fo_str(o, ">");
        }
        if (t2->kind == LT_SCALAR) {
            fo_str(o, " ");
            fo_bytes(o, t2->text, t2->tlen);
            (*ip)++;
        } else {
            fo_str(o, " "); /* the node-less position */
        }
        fo_str(o, "\n");
        return 1;
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
    if (t->kind == LT_FLMAP) {
        if (anchor != NULL) {
            return l_fail(c); /* an anchored nested map: outside */
        }
        return l_render_map(c, o, ip);
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

/* The flow-mapping body (libyaml's flow_mapping_key/value states,
 * recursive for the r281 nested-map law): *ip sits ON the FLMAP-START
 * token. KEY-without-node and value-without-token insert the empty
 * scalar; a keyless member renders the node into the key slot; the
 * separator re-peek keeps the trailing-comma shape. */
static int l_render_map(lctx* c, fout* o, size_t* ip) {
    (*ip)++; /* past FLMAP-START */
    fo_str(o, "+MAP\n");
    int first = 1;
    for (;;) {
        if (*ip >= c->ntoks) {
            return l_fail(c);
        }
        ltok* e = &c->toks[*ip];
        if (e->kind == LT_EOS || e->kind == LT_ERROR) {
            return l_fail(c); /* the collection never closed */
        }
        if (e->kind == LT_FLMAP_END) {
            (*ip)++;
            break;
        }
        if (!first) {
            if (e->kind != LT_ENTRY) {
                return l_fail(c);
            }
            (*ip)++;
            if (*ip >= c->ntoks) {
                return l_fail(c);
            }
            e = &c->toks[*ip];
            /* libyaml re-peeks after the separator: the collection may
             * end right here (the trailing-comma shape) */
            if (e->kind == LT_FLMAP_END) {
                (*ip)++;
                break;
            }
            if (e->kind == LT_EOS || e->kind == LT_ERROR) {
                return l_fail(c);
            }
        }
        first = 0;
        if (e->kind == LT_KEY) {
            (*ip)++;
            if (*ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* k = &c->toks[*ip];
            if (k->kind == LT_VALUE || k->kind == LT_ENTRY || k->kind == LT_FLMAP_END) {
                fo_str(o, "=VAL : \n");
            } else {
                if (!l_render_node(c, o, ip)) {
                    return l_fail(c);
                }
            }
            if (*ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* v = &c->toks[*ip];
            if (v->kind == LT_VALUE) {
                (*ip)++;
                if (*ip >= c->ntoks) {
                    return l_fail(c);
                }
                v = &c->toks[*ip];
                if (v->kind == LT_ENTRY || v->kind == LT_FLMAP_END) {
                    fo_str(o, "=VAL : \n");
                } else {
                    if (!l_render_node(c, o, ip)) {
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
        if (!l_render_node(c, o, ip)) {
            return l_fail(c);
        }
        fo_str(o, "=VAL : \n");
    }
    fo_str(o, "-MAP\n");
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
    case LT_TAG:
        return "TAG";
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
        /* the flow mapping (recursive body — the r281 nested-map law) */
        if (!l_render_map(c, o, &ip)) {
            return l_fail(c);
        }
        if (ip >= c->ntoks || c->toks[ip].kind != LT_EOS) {
            return l_fail(c); /* anything after the root: not this family */
        }
        fo_str(o, "-STR\n");
        return 1;
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
        if (e->kind == LT_FLSEQ_END) {
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
            if (e->kind == LT_FLSEQ_END) {
                ip++;
                break;
            }
            if (e->kind == LT_EOS || e->kind == LT_ERROR) {
                return l_fail(c);
            }
        }
        first = 0;
        if (e->kind == LT_KEY) {
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
        /* the key-less entry: the node is the SEQ member */
        if (!l_render_node(c, o, &ip)) {
            return l_fail(c);
        }
    }
    fo_str(o, "-SEQ\n");
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
    c.sk_allowed = 1; /* libyaml's fetch_stream_start arms the flag */
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
        return 0; /* the budget gate (law n) */
    }
    size_t root = fc_skip_blank_break(data, size, 0);
    if (root >= size || (data[root] != '[' && data[root] != '{')) {
        return 0; /* the family is flow-rooted (law j): a leading
                   * blank/break run is scan-to-token territory on both
                   * engines (the r296 artifact's `\n [` head) — any
                   * other lead byte (a comment, a directive, a document
                   * marker) still declines */
    }
    int is_map = data[root] == '{';
    /* the r306 BOM-ride gate: a MID-LINE BOM is inert content — both
     * engines keep the mark inside the plain byte-exact (the r306
     * artifact's `?\r2<BOM><BOM><BOM>` site renders the three marks
     * into BOTH actual dumps), so the soup models ride it as plain
     * bytes; a LINE-START BOM still rides other families (libyaml's
     * scan_to_next_token strips it at mark column 0 whatever break
     * opened the line — the r226 law — while yeptris keeps it as
     * content, so the byte-exact derivation would decline it anyway;
     * the gate keeps the routing explicit) */
    int line_start = 1;
    for (size_t i = 0; i < size;) {
        unsigned char ch = data[i];
        if (ch == 0x00) {
            return 0; /* NUL: libyaml's stream end — outside the family */
        }
        if (ch == 0xEF && i + 2 < size && data[i + 1] == 0xBB && data[i + 2] == 0xBF) {
            if (line_start) {
                return 0; /* a line-start BOM rides other families */
            }
            i += 3; /* the mid-line mark rides as content */
            continue;
        }
        size_t bl = fc_break_len(data, size, i);
        if (bl > 0) {
            i += bl;
            line_start = 1;
            continue;
        }
        line_start = 0;
        i++;
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
