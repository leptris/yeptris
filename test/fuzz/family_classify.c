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
 *    opens the VALUE slot, a ':'-with-flow-indicator-follower GLUED
 *    boundary inserts the empty at the terminator (the r357 widening),
 *    a separator/closer leaves the empty
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
 *    laws (law i), and so does a `{` at the split-explicit-key
 *    member's KEY slot — the blank/break run after the close rides to
 *    the `:` VALUE boundary (the r310 widening); the value slot's `[`
 *    opens the nested flow SEQUENCE the same way (the r281 map arm's
 *    SEQ twin — the r371 widening, run 45217557947: the nested
 *    `[?::]` composition, the inner `?::JJ` plain riding law d);
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
 * The entry gates: the family is flow-rooted (law j) with three
 * verified lead walks — the blank/break run (the r296 law), the `---`
 * doc-marker line run (the r349 law), and the bare-`!` tag-property
 * line head (the r375 widening, run 45281713143: the property rides
 * the ROOT collection header `<!>` byte-equal on both engines — the
 * bare tag never "resolves by style" on a collection, the r335 law;
 * `!!`/`!h!` handles and `!<verbatim>` forms carry other resolved
 * tags and stay out, the glued `![` head is a YEPTRIS-ONLY-ACCEPT
 * engine split and stays out).
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
    262144u /* the family's members are <1KB; the                                                  \
             * largest corpus seed is 10732; the                                                   \
             * 18.5KB r284 artifact rides whole; the                                               \
             * 32.8KB r285 artifact rides whole; the                                               \
             * 48.5KB r293 artifact rides whole; the                                               \
             * 81.9KB r295 artifact rides whole; the                                               \
             * 148.8KB r361 artifact rides whole (r362) */
#define FC_MAX_OUT                                                                                 \
    (1024u * 1024u)        /* derived-dump cap (the r361 artifact                                  \
                            * derives ~270KB per side, ~542KB total;                               \
                            * the r362 raise) */
#define FC_MAX_PIECES 256u /* plain fold pieces per node */
#define FC_MAX_MEMBERS                                                                             \
    16384u /* flow members per collection (the                                                     \
            * 10KB r258 artifact rides ~300; the                                                   \
            * r284 soup rides ~1400; the r285 soup                                                 \
            * rides 2324; the r293 soup rides under                                                \
            * 4096; the r295 soup rides just past                                                  \
            * 4096; the r361 floor rides 8243 — the                                              \
            * r362 raise: the one-byte mutation of                                                 \
            * the floor (run 45078029712) declined                                                 \
            * ONLY on this gate, every other gate                                                  \
            * held) */
#define FC_MAX_TOKENS                                                                              \
    65536u              /* libyaml tokens per stream (the r284 soup                                \
                         * rides ~5600; the r285 soup rides 5477; the                              \
                         * r293 soup rides just past 8192; the r361                                \
                         * artifact rides ~25K (the walk trips at pos                              \
                         * 96431 of 148798 under the old cap) — the                              \
                         * r362 raise) */
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

/* The r349 document-marker LEAD law (run 44941446715): a run of
 * document-marker lines ahead of the flow root is INERT and byte-equal
 * on both engines, every marker EXCEPT the last opening an EMPTY first
 * document (one "=VAL : \n" per prior marker; the probes:
 * `---\n[1]` -> `+STR\n+SEQ`, `---\n---\n[1]` -> `+STR\n=VAL : \n+SEQ`,
 * `---\n---\n---\n[1]` -> two empties, the spaces-padded `---  \n---\n`
 * and the comment-line `---\n# c\n---\n` forms riding the same count).
 * Each marker unit: `---` at a LINE START (the first at input pos 0
 * exactly — an indented ` ---` is plain content on both engines, the
 * probes' `=VAL : ---` doc), padded with blanks (the tab-padded
 * `---\t\n` form dump-verified byte-equal beside the spaces form), then
 * a break (LF, CR, CRLF only — the exotic breaks stay out), the inline
 * root's blanks (`--- [1]`), or the inline root directly (`---[1]` is
 * no doc indicator at all — the lead ends there and the gate declines).
 * Between units: spaces, LF/CR/CRLF breaks, and whole-line `#` comments
 * at a line start (inert on both engines). Anything else — a tab, an
 * exotic break, a non-blankz byte after the triple (`---x`, `----`,
 * `---[`) — ends the lead at the last committed marker, and the entry
 * gate declines the tail exactly as before. Returns the marker count
 * (0 = no lead; *end then rides 0 and every walk is unchanged). */
/* libyaml IS_ALPHA: [0-9A-Za-z_-] (the alias/anchor name cut). */
static int fc_alpha(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' ||
           c == '-';
}

static int fc_doc_lead(const uint8_t* p, size_t n, size_t* end) {
    size_t i = 0;
    int markers = 0;
    *end = 0;
    /* the r377 widening (the soak find 2 floor's `\n---\n[...` lead,
     * the r376b session): a blank/break RUN may precede the FIRST
     * marker line — both engines scan-to-token before the doc start,
     * byte-equal across the probe matrix (`\n---\n[1]`, ` \n---\n[1]`,
     * `\n \n---\n[1]`, `\r---\n[1]`, and the exotic-break heads
     * `NEL---\n[1]`/`LS---\n[1]`/`PS---\n[1]`/`\nLS---\n[1]` all
     * EQUAL; the marker-count law composes unchanged behind the run,
     * the `\n---\n---\n[1]` twin renders the one empty first document
     * on both engines). The run admits spaces (each space run must
     * face a break — a space run DIRECTLY before the triple is the
     * r349 indented-`---` content law, stays out) and every
     * fc_break_len break kind. A TAB before the triple is BOTH-ERROR
     * on both engines (probed) — the walk's decline there matches; a
     * comment inside the run is a separate composition, stays out. An
     * EMPTY run is the old pos-0 law; a run ending at a non-triple
     * byte keeps the pre-patch whole-lead decline. */
    for (;;) {
        size_t s = i;
        while (s < n && p[s] == ' ') {
            s++;
        }
        if (s > i) {
            size_t bl = fc_break_len(p, n, s);
            if (bl == 0) {
                break; /* the space run faces a non-break: the lead
                        * starts here (the indented `---` declines) */
            }
            i = s;
        }
        size_t bl = fc_break_len(p, n, i);
        if (bl == 0) {
            break;
        }
        i += (size_t)bl;
    }
    if (n < i + 3 || p[i] != '-' || p[i + 1] != '-' || p[i + 2] != '-') {
        return 0;
    }
    for (;;) {
        /* the marker unit: i is at the triple, at a line start */
        i += 3;
        int padded = 0;
        while (i < n && fc_blank(p[i])) {
            i++; /* blanks pad the marker line; the tab-padded form is
                  * dump-verified byte-equal beside the spaces form */
            padded = 1;
        }
        if (i >= n) {
            *end = i;
            return markers + 1;
        }
        if (padded && (p[i] == '[' || p[i] == '{')) {
            /* the INLINE root spelling (the r349 probe `--- [1]` ->
             * `+STR\n+SEQ`, zero empties): the marker is invisible, the
             * root bracket follows the padding directly. A GLUED
             * `---[` has no blankz after the triple — the engine's
             * fetch law makes it plain content, never a marker: the
             * lead ends here and the gate declines the rest */
            markers++;
            *end = i;
            return markers;
        }
        if (p[i] != '\r' && p[i] != '\n') {
            /* a non-blankz byte after the padding that is neither the
             * inline root nor a break (e.g. `--- # c`): not a doc
             * indicator tail the probes cover — the lead ends at the
             * last committed marker; the first unit failing here is no
             * lead at all */
            if (markers == 0) {
                return 0;
            }
            *end = i;
            return markers;
        }
        i += (size_t)fc_break_run_len(p, n, i);
        markers++;
        /* the intervening walk: spaces, LF/CR/CRLF breaks, whole-line
         * comments; anything unverified declines the WHOLE lead (the
         * input keeps its pre-patch decline — never a partial hop over
         * an unverified byte) */
        int at_ls = 1;
        for (;;) {
            if (i >= n) {
                *end = i;
                return markers;
            }
            unsigned char ch = p[i];
            if (ch == ' ') {
                i++;
                at_ls = 0;
                continue;
            }
            if (ch == '\r' || ch == '\n') {
                i += (size_t)fc_break_run_len(p, n, i);
                at_ls = 1;
                continue;
            }
            if (ch == '\t' || ch == 0xC2 || ch == 0xE2) {
                return 0; /* tab or exotic break: unverified, decline */
            }
            if (ch == '#' && at_ls) {
                while (i < n && p[i] != '\r' && p[i] != '\n') {
                    if (p[i] == 0xC2 || p[i] == 0xE2) {
                        return 0; /* exotic break in the comment: out */
                    }
                    i++;
                }
                continue; /* the break itself rides the loop */
            }
            if (ch == '-' && at_ls && i + 2 < n && p[i + 1] == '-' && p[i + 2] == '-') {
                break; /* the next marker unit */
            }
            *end = i;
            return markers;
        }
    }
}

/* The TAG-PROPERTY lead (the r375 artifact's `!`+LF head, run
 * 45281713143): a bare non-specific `!` tag at offset 0, a blank/break
 * run after it, then the ROOT bracket. The property rides the root
 * collection on both engines (the header `+SEQ <!>`/`+MAP <!>`
 * byte-equal across the r375 probe matrix: LF, blank, CRLF, blank+LF
 * and LF+LF runs all agree). Gates, probe-backed:
 *  - the bare `!` EXACTLY — a `!!` handle lead is a YEPTRIS-ONLY-ACCEPT
 *    split (yeptris renders `<tag:yaml.org,2002:>`, libyaml 90a56d4
 *    ERRORS the parse — the probe matrix's `!!\n[1]` dump cut), and a
 *    `!name`/`!<verbatim>` lead carries a different resolved tag (the
 *    probes' `<!x>`/`<x>` renders — agreeable but unverified by this
 *    walk, stays out);
 *  - the run NON-EMPTY — a GLUED `![` is a YEPTRIS-ONLY-ACCEPT split
 *    (yeptris accepts the tag-glued root, libyaml errors the tag-end
 *    "did not find expected whitespace or line break" — the probe
 *    matrix's `![1]` dump cut), so the run must exist;
 *  - no comment inside the run (`! # c\n[1]` agrees on both engines
 *    but the comment form is a separate composition, stays out);
 *  - the bracket DIRECTLY after the run (the probes never cover a
 *    second tag or an anchor between the run and the root);
 *  - the run admits ONLY the probe-verified bytes: the space and the
 *    LF/CR/CRLF breaks (a TAB or an exotic break — NEL/LS/PS — in the
 *    run is untested, stays out; the r349 marker walk's discipline —
 *    its tab form was dump-verified BEFORE admission).
 * `end` lands ON the root bracket (the root walks' blank/break skip
 * re-checks it; the l-tokenizer starts at offset 0 so its TAG token
 * precedes the root — l_derive threads lead_tag for that). */
static int fc_tag_lead(const uint8_t* p, size_t n, size_t* end) {
    if (n < 2 || p[0] != '!') {
        return 0;
    }
    if (p[1] == '!' || fc_alpha(p[1]) || p[1] == '<') {
        return 0; /* a handle / verbatim form: unverified, stays out */
    }
    size_t i = 1;
    int moved = 0;
    for (;;) {
        if (i < n && p[i] == ' ') {
            i++;
            moved = 1;
            continue;
        }
        if (i < n && p[i] == '\n') {
            i++;
            moved = 1;
            continue;
        }
        if (i < n && p[i] == '\r') {
            i++;
            if (i < n && p[i] == '\n') {
                i++;
            }
            moved = 1;
            continue;
        }
        break; /* a tab, an exotic break, or the next token: the run ends */
    }
    if (!moved) {
        return 0; /* the run is empty (the glued `![` engine split) */
    }
    if (i >= n || (p[i] != '[' && p[i] != '{')) {
        return 0; /* the bracket is not directly after the run */
    }
    *end = i;
    return 1;
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
static int fc_piece(const uint8_t* p, size_t n, size_t q, size_t end, int qm, int rider,
                    size_t* piece_at, size_t* piece_len, size_t* stop, int* stop_sep,
                    int* ended_comment) {
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
                if (!qm && !rider) {
                    return 0; /* ':' + flow indicator: libyaml's "unexpected
                               * ':'" scanner error — outside the family.
                               * The rider carve-out (the r339 widening, run
                               * 44864014989): a GLUED-`?` member whose
                               * follower is an ALIAS never opens a plain on
                               * libyaml — its flow `?` fires the KEY token
                               * and the `*` fires the ALIAS token
                               * (fetch_key's remove_simple_key +
                               * `simple_key_allowed = (!flow_level)` = 0,
                               * so the alias saves no key; fetch_value's
                               * complex-key path is check-free in flow) —
                               * the r291 "still-open plain" premise is
                               * false there and the glued colon + flow
                               * indicator derives as the VALUE boundary on
                               * BOTH engines (the artifact's `?*L:,&m:(}`
                               * member; libfyaml 04e0b58 reads yeptris's
                               * stream byte-exact on it). */
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
static int fc_plain_pieces(const uint8_t* p, size_t n, size_t i, size_t end, int qm, int rider,
                           fcnode* node, size_t* stop_at, int* stop_sep) {
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
        if (!fc_piece(p, n, q, end, qm, rider, &pat, &plen, &stop, &sep, &commented)) {
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
             ((qm || rider) && stop + 1 < end && fc_flow_ind(p[stop + 1])))) {
            /* the rider arm (the r339 widening, run 44864014989): the
             * GLUED-`?` alias-rider member's colon + flow indicator is the
             * VALUE boundary the same way the SEQ-rooted qm arm takes it
             * — libyaml's KEY→ALIAS path leaves no plain open, so the
             * r291 decline premise does not apply and the boundary
             * derives on both engines (the artifact's `?*L:,&m:(}`; the
             * value slot walks on from the colon). */
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
    int depth;                             /* the open nested-collection depth (FC_MAX_NEST cap) */
    unsigned char seq_at[FC_MAX_NEST + 2]; /* per level: the level is a
                                            * SEQUENCE (the r314 nested-
                                            * seq law; the nested walks
                                            * restamp their level so a
                                            * sibling branch reads fresh) */
    int pair_open;                         /* the #741 colon-led SEQ single-pair stays open: the
                                            * following members render as map entries (key +
                                            * empty value; the bare colon member keeps its
                                            * empty-key + value slot) until the closer */
    size_t lead_end;                       /* the r349 doc-marker lead's end (the root bracket's
                                            * position; 0 = no lead) */
    int lead_docs;                         /* the lead's marker count (each but the last opens an
                                            * empty first document) */
    int lead_tag;                          /* the r375 tag-property lead (the bare `!` + blank/
                                            * break run head): the tag rides the ROOT collection
                                            * header (`+SEQ <!>`/`+MAP <!>`) */
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

/* The ENCLOSING level's kind (the member walks' branch driver): the
 * root level rides c->is_map; a nested level rides its seq_at stamp —
 * the r314 nested-seq walk stamps 1, the nested-MAP walks stamp 0, so
 * a sibling branch never reads a stale level kind. */
static int y_cur_is_map(const yctx* c) {
    if (c->depth == 0) {
        return c->is_map;
    }
    return c->seq_at[c->depth] ? 0 : 1;
}

/* The alias name run (the engine's e_alias walk): prop chars = not
 * blank, not break, not flow indicator, ',', or '#'; an interior ':'
 * rides unless a blank or flow indicator follows it ("*an:chor" is one
 * name — Y2GN; at the member end it rides too — 2SXE, probed again at
 * the r342 nightly: the r342 widening removed the BREAK from the split
 * set — the engine keeps the colon when a break follows ("*0:" + LF is
 * ONE name `0:` — the split-key artifact's `*00:` site, the bare-alias
 * member's `[*00:<LF>,]` probe, and the anchor twin `&a:` render the
 * colon into BOTH events; only a blank or a flow indicator closes the
 * name — probed `*00: x` and `*00:,` cut before the colon). */
static size_t fc_alias_name(const uint8_t* p, size_t n, size_t i, size_t end) {
    size_t s = i;
    while (i < end) {
        unsigned char c = p[i];
        if (fc_blank(c) || c == ',' || c == '#' || fc_flow_ind(c) || fc_break_len(p, n, i) > 0) {
            break;
        }
        if (c == ':') {
            size_t j = i + 1;
            if (j < end && (fc_blank(p[j]) || fc_flow_ind(p[j]))) {
                break; /* ':' separates before blank/flow indicator (a
                        * break follower RIDES: the r342 widening) */
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
            /* the r337 widening (run 44832174407, main ffc37f20): two
             * escape followers join the family, everything else stays
             * outside; both ride the tight gates — no pending fold
             * run, no pending blanks at the escape (untested
             * compositions decline). The r374 widening (run
             * 45259544033, main 84150a76) takes the `\uXXXX`
             * follower ahead of those gates: ITS pending-state
             * compositions are measured (see the arm below). */
            if (i + 1 >= e) {
                return 0;
            }
            if (p[i + 1] == 'u') {
                /* the r374 widening (run 45259544033, main 84150a76):
                 * the `\uXXXX` follower joins the family. The
                 * artifact's member-led span `"\r g61クリス"`
                 * (the CR lead folds, three chained BMP escapes) and
                 * the 19-byte floor `["ク",?g?,""]` DIFFER only at
                 * a SIBLING member — the escape member renders
                 * byte-identical on both engines, but the classifier
                 * could not derive a single escape byte, so any DIFFER
                 * sharing a seq with it stayed red. THE MEASURED LAW
                 * (the 21-row matrix, this session): yeptris and
                 * libyaml decode a well-formed `\uXXXX` IDENTICALLY —
                 * mid-content, chained, at the lead, at the close
                 * edge, and across the fold compositions
                 * (`["a<CR>クb"]`); the decoded codepoint lands as
                 * content bytes the dump re-escapes
                 * (yd_escape_append's format — `\\` for a decoded
                 * backslash, `\n`/`\r`/`\t`, `\xNN` for the other
                 * C0/DEL, every non-ASCII byte raw — probed `A`, the
                 * decoded backslash, LF, C0, DEL, the C1 NEL byte,
                 * NBSP, the decoded LS, U+FFFD, U+FFFE — all EQUAL).
                 * The decline domain is BOTH-ERROR, never a consult: a
                 * surrogate half (`\ud800`/`\udfff`) and the malformed
                 * spellings (short hex, a bad hex digit) error on BOTH
                 * engines. Gates: the four hex digits must complete
                 * inside the span, and a surrogate codepoint declines
                 * (a decline is cheap; a surrogate can only be a
                 * both-error, never this family's DIFFER). The escape
                 * rides the pending state exactly as a content byte
                 * does: a pending fold run or blank run flushes first
                 * (the `["a<CR>クb"]` probe — the fold space then
                 * the escape; the `["a クb"]` probe — the blanks
                 * ride byte-exact), the lead handling matches the `\n`
                 * arm (lead_run == 1 folds to the one space;
                 * lead_run > 1 declines). */
                if (run > 0) {
                    if (first_content) {
                        if (lead_run > 1) {
                            return 0; /* a multi-break-led span */
                        }
                        fo_str(o, " "); /* the one-break lead fold */
                        lead_run = 0;
                    } else {
                        fo_fold_run(o, run);
                    }
                    run = 0;
                    first_content = 0;
                }
                if (blank_n > 0) {
                    if (first_content) {
                        return 0; /* a blank-led span: untested (the
                                   * r287 arm takes the break twin) */
                    }
                    fo_escape(o, p + blank_at, blank_n);
                    blank_n = 0;
                    first_content = 0;
                }
                if (i + 6 > e) {
                    return 0; /* the four hex digits must be inside the
                               * span (a truncated escape is a
                               * both-error, never this family) */
                }
                unsigned v = 0;
                for (int k = 2; k <= 5; k++) {
                    if (!fc_hex(p[i + (size_t)k])) {
                        return 0; /* a bad hex digit: both-error domain */
                    }
                    v = (v << 4) + fc_hexval(p[i + (size_t)k]);
                }
                if (v >= 0xD800 && v <= 0xDFFF) {
                    return 0; /* a surrogate half: both engines reject */
                }
                uint8_t utf[4];
                size_t un;
                if (v < 0x80) {
                    utf[0] = (uint8_t)v;
                    un = 1;
                } else if (v < 0x800) {
                    utf[0] = (uint8_t)(0xC0 | (v >> 6));
                    utf[1] = (uint8_t)(0x80 | (v & 0x3F));
                    un = 2;
                } else {
                    utf[0] = (uint8_t)(0xE0 | (v >> 12));
                    utf[1] = (uint8_t)(0x80 | ((v >> 6) & 0x3F));
                    utf[2] = (uint8_t)(0x80 | (v & 0x3F));
                    un = 3;
                }
                if (first_content) {
                    first_content = 0;
                    if (lead_run > 1) {
                        return 0; /* a multi-break-led span: untested */
                    }
                    if (lead_run == 1) {
                        fo_str(o, " ");
                    }
                }
                fo_escape(o, utf, un); /* the decoded bytes ride the
                                        * dump escape (backslash, LF/
                                        * CR/TAB, \xNN for the other
                                        * C0/DEL, non-ASCII raw) */
                i += 6;
                continue;
            }
            if (run > 0 || blank_n > 0) {
                return 0;
            }
            int ebl = fc_break_len(p, n, i + 1);
            if (ebl > 0) {
                /* the ESCAPED-BREAK continuation: `\` glued to a break
                 * drops BOTH (the 1.2 escaped-continuation law; the
                 * minimal's `"\<CR>g"` renders `g` byte-exact on the
                 * engine; libyaml's scan_flow_scalar twin rides its
                 * leading-blanks loop to the same). The following byte
                 * must be CONTENT — a blank (the drop's blank law is
                 * untested here), a second break, or the closer
                 * (untested compositions) decline. */
                if (i + 1 + (size_t)ebl >= e) {
                    return 0;
                }
                unsigned char f2 = p[i + 1 + (size_t)ebl];
                if (fc_blank(f2) || fc_break_len(p, n, i + 1 + (size_t)ebl) > 0 || f2 == '"') {
                    return 0;
                }
                first_content = 0;
                i += 1 + (size_t)ebl;
                continue;
            }
            if (p[i + 1] != 'n') {
                return 0; /* every other escape follower: outside */
            }
            /* the `\n` ESCAPE decodes to the LF byte — the engines
             * agree and the dump re-escapes it (`\n`, the same two
             * bytes yd_escape_append writes for LF). The artifact's
             * span body carries six of them; the pending-state gates
             * above keep the fold interaction untouched. */
            if (first_content) {
                first_content = 0;
                if (lead_run > 1) {
                    return 0; /* a multi-break-led span: untested */
                }
                if (lead_run == 1) {
                    fo_str(o, " ");
                }
                /* lead_run == 0 here: blank_n == 0 was gated above */
            }
            fo_str(o, "\\n");
            i += 2;
            continue;
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
        /* the r315 widening (run 44411162279): the run between the name
         * and the terminator rides blank/break like the alias value slot
         * (the same prop walk — the artifact's `:\r&binary\r,` value
         * renders the anchored empty byte-exact on both engines); a `#`
         * or any other non-boundary byte declines */
        size_t ve = fc_skip_blank_break(p, c->n, ae);
        if (ve >= c->n || !fc_flow_ind(p[ve])) {
            return y_fail(c);
        }
        memset(val, 0, sizeof(*val));
        val->anchor_at = as;
        val->anchor_len = al;
        *stop = ve;
        return 1;
    }
    if (p[q] == '{') {
        /* the nested flow map (the r281 law, run 43705128492): the
         * value node IS the nested collection — the same member laws
         * derive its content recursively (+MAP/-MAP around them; the
         * value-slot `[` took its own SEQ twin below, the r371
         * widening). The render is stashed on
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
        c->seq_at[c->depth] = 0; /* the level is a MAP (fresh stamp) */
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
    if (p[q] == '[') {
        /* the nested flow sequence (the r371 widening, run 45217557947):
         * the value node IS the nested collection — the r281 nested-map
         * arm's SEQ twin at the value slot (the r314 law owns the same
         * nested seq at the MEMBER slot; the r281 widening took the
         * value-slot `{` and the value-slot `[` stayed declined as an
         * untested shape — the nightly's artifact measures it now): the
         * same member laws derive its content recursively (+SEQ/-SEQ
         * around them; the artifact's inner `?::JJ` glues the `?` into
         * the plain — law d — its colons riding content, neither a blank
         * nor a flow indicator following, so no VALUE boundary fires).
         * The render is stashed on the node — the caller renders the key
         * line first, then the stash. The blank/break run after the
         * close skips so the caller's member-terminator check sees the
         * post-value byte. The libyaml side rides the l-side nested-seq
         * law (the tokenizer level slots + the recursive seq body —
         * law i): the inner member-start `?` fires the KEY token (the
         * single-pair +MAP wrapper), the key slot empty on the VALUE at
         * the mapping-key state, the SECOND VALUE consumed by the
         * mapping-value state with the node after it as the pair's
         * value — the artifact's `[?::[?::JJ]]` derives both dumps
         * byte-exact (yeptris: the outer KEY-implicit pair key `?:` +
         * the nested seq's plain `?::JJ`; libyaml: the outer
         * empty-key/double-VALUE pair + the inner empty-key/JJ wrapper;
         * libfyaml 04e0b58 reads yeptris's stream byte-exact — JSON
         * mode `{"?:": ["?::JJ"]}` — in BOTH --yaml-1.2 and --yaml-1.1
         * passes; comparator carry only, no engine change, the
         * r318/r330/r331/r332/r336/r346/r356/r366/r368/r369/r370
         * precedent). A quoted / `!` value keeps its decline (untested
         * shapes stay out). */
        if (c->depth >= (int)FC_MAX_NEST) {
            return y_fail(c); /* past the nesting budget */
        }
        fout nf = {NULL, 0, 0, 0};
        fo_str(&nf, "+SEQ\n");
        fout outer = c->out;
        c->out = nf;
        c->depth++;
        c->seq_at[c->depth] = 1; /* the level is a SEQUENCE (the r314
                                  * stamp; the member walks below ride
                                  * y_cur_is_map) */
        size_t after = 0;
        int ok = y_members(c, ']', q + 1, &after);
        c->depth--;
        nf = c->out;
        c->out = outer;
        if (!ok || nf.bad) {
            free(nf.buf);
            return 0;
        }
        fo_str(&nf, "-SEQ\n");
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
    if (p[q] == '"' || p[q] == '\'' || p[q] == '!') {
        return y_fail(c);
    }
    int sep;
    if (!fc_plain_pieces(p, c->n, q, c->n, 0, 0, val, stop, &sep)) {
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
    if (!split && nxt < n && (p[nxt] == '{' || p[nxt] == '[')) {
        /* the r377 widening (the soak find 1 floor's glued `?{` member):
         * a collection follower GLUED to the `?` rides the split
         * machinery — the engine's `?` is the explicit-key indicator
         * with the collection as the key node on BOTH engines, and the
         * plain cannot start there (a flow indicator is not a
         * plain-safe char). Probed byte-equal: `[?{a}]`, `[?[a]]`,
         * `{?{a}}`, `[? {a}]` all EQUAL (the SEQ single-pair wrapper +
         * the collection key node + the parser's inserted empty value;
         * the r310/r351 split arms' mechanics with the run skipped) */
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
        if (nxt < n && (p[nxt] == ',' || p[nxt] == (y_cur_is_map(c) ? '}' : ']'))) {
            memset(&key, 0, sizeof(key));
            memset(&val, 0, sizeof(val));
            *term_at = nxt;
            /* the r319 widening (run 44470194441): the silent comma
             * ride after the indicator member on the SEQ root — the
             * engines consume ONE stray ',' past the member terminator
             * byte-exact (the floor's `?,,g` member: `{e,e}` map then
             * the plain `g`; the probes `[1,?,,g,h]` -> (1,{e,e},g,h),
             * `[1,?,,]` -> (1,{e,e}), `[1,?,g]` -> (1,{e,e},g)). One
             * comma only: the triple-comma form is a BOTH-ERROR shape
             * (`[1,?,,,g]`) and a blank/break between the commas too
             * (`[1,?,\rg]` errors yeptris) — both stay outside. The MAP
             * root errors the ride (`{1,?,,g}` BOTH-ERROR) — SEQ-rooted
             * only. The libyaml model's member loop already skips the
             * leading FLOW-ENTRY (its flow_sequence_entry law), so this
             * is the yeptris-model side only. */
            if (!y_cur_is_map(c) && nxt + 1 < n && (p[nxt + 1] == ',' || p[nxt + 1] == ']')) {
                *term_at = nxt + 1;
            }
            if (y_cur_is_map(c)) {
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
         * walk keeps the plain law — EXCEPT the alias rider (the r339
         * widening, run 44864014989): a `*` follower never opens a
         * plain on libyaml (the KEY token fires, then the ALIAS token;
         * no simple key saves in flow after the explicit `?`), so the
         * r291 still-open-plain premise is false and the glued colon +
         * flow indicator derives as the VALUE boundary on both
         * engines. A plain follower keeps the decline (the libyaml
         * plain opens and the boundary shape is the documented
         * qflow/yd_r242 family, not the classifier's). */
        int rider = (nxt < n && p[nxt] == '*');
        if (!fc_plain_pieces(p, n, i, n, !y_cur_is_map(c), rider, &key, &stop, &sep)) {
            return y_fail(c);
        }
        if (key.colon) {
            if (!y_cur_is_map(c)) {
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
        if (y_cur_is_map(c)) {
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
    memset(&key, 0, sizeof(key)); /* every SPLIT slot initializes the key
                                   * node: the terminator-gate decline
                                   * below drops it (the r314 release
                                   * finding — the r301 alias-blank path
                                   * freed the uninitialized nest_buf
                                   * slot; the layout shift exposed it) */
    if (ks >= n || (fc_flow_ind(p[ks]) && p[ks] != '{' && p[ks] != '[')) {
        /* (`{` is carved out: it opens the nested-collection KEY slot
         * walk below -- the r310 widening; the r351 widening carves the
         * SEQ twin `[` the same way) */
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
             * both decline.
             *
             * the r326 widening (run 44624659366): the run may reach a
             * blank/break-preceded `#` instead — the engine's comment
             * condition fires, the run eats inertly to its break, and
             * the boundary derives after it (the artifact's
             * `?\t*%L #|...` site — libyaml never scans the alias, its
             * law-a KEY+comment-eat swallows the whole member tail
             * including the name). After the ride the separator/closer
             * derives as before, or a ':' + blank/break re-lexes as the
             * VALUE boundary (the r309 law's slot: the pair's empty
             * value opens, `=ALI *<name>` + the inserted empty). A ':'
             * across the PLAIN ride (no comment) stays declined; an eat
             * to EOF and any other byte decline. */
            size_t ve = fc_skip_blank_break(p, n, ae);
            int rode_comment = 0;
            if (ve < n && p[ve] == '#') {
                size_t e2 = ve + 1;
                while (e2 < n && fc_break_len(p, n, e2) == 0) {
                    e2++;
                }
                if (e2 >= n) {
                    return y_fail(c); /* the eat runs to EOF: never
                                       * terminates */
                }
                ve = fc_skip_blank_break(p, n, e2 + (size_t)fc_break_run_len(p, n, e2));
                rode_comment = 1;
            }
            if (ve >= n) {
                return y_fail(c); /* bytes after the alias name that are
                                   * not a blank/break-then-boundary ride:
                                   * outside the family */
            }
            if (fc_flow_ind(p[ve])) {
                stop = ve;
                sep = 1;
            } else if (rode_comment && p[ve] == ':' &&
                       (ve + 1 >= n || fc_blank(p[ve + 1]) || fc_break_len(p, n, ve + 1) > 0)) {
                /* the VALUE boundary across the comment ride (the r309
                 * slot; the common colon path below opens the empty
                 * value — the artifact renders `=ALI *%L` + the empty
                 * exactly as the separator form does) */
                key.colon = 1;
                key.colon_at = ve;
                stop = ve;
                sep = 0;
            } else if (p[ve] != ':' && p[ve] != '*' && p[ve] != '&' && p[ve] != '{') {
                /* the r342 widening (run 44891943987): the COLON-LESS
                 * explicit-key VALUE binding. When the name cut ends at
                 * a colon-riding break (the r342 charset widening above
                 * — the artifact's `*00:` + LF) and the blank/break run
                 * after the name reaches CONTENT (the floor's `m`), the
                 * engine's flow-map parser binds that node as the
                 * pair's VALUE with no VALUE token ever firing (probed
                 * `? *a<LF>  m,` -> pair(*a, m), `? *a:<LF>  m, n` ->
                 * pair(*a:, m) + the member n, `? *a:<LF>  ,` -> the
                 * empty value the separator form renders — y_value_node
                 * covers it). libyaml never sees the colon-ride: its
                 * scan_anchor cut is IS_ALPHA, so the glued ':' fires
                 * its flow VALUE token and the same post-run plain
                 * lands in its value slot — the dumps agree but for the
                 * NAME bytes (the r311/r318 charset divergence;
                 * libfyaml 04e0b58 corroborates the riding name event
                 * `=ALI *00:` and declines the binding itself — the
                 * documented third reading). Structural starts (an
                 * alias, an anchor, a nested map) and any ':' stay
                 * declined; quotes/tags/nested sequences ride
                 * y_value_node's own decls. */
                if (!y_value_node(c, ve, &stop, &val)) {
                    return y_fail(c);
                }
            } else {
                return y_fail(c);
            }
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
        /* the commas inside the run are COMMENT TEXT on both engines —
         * the r319 widening (run 44470194441): the run's separator
         * count is inert (the artifact's `?<CR>#, 12?1#|,?<CR>/...`
         * member carries two commas inside the eat and both engines
         * render the identical steal; the probes `[a,?\r#x,y,b\nc]` /
         * `[a,?\r#x,y,z,b\nc]` render the r245 `+MAP(c,)` byte-exact).
         * An eat running to EOF still declines, and the root matrix
         * keeps its shape: a comma-carrying run on the MAP root stays
         * untested-declined (the r245 steal stays SEQ-rooted).
         *
         * the r377 widening (the nightly's round 377, run
         * 45317828972, main 7171f226 — the 23-byte giant floor's
         * `?\r#\n5 ?#` member): the CLEAN eat (zero separators in the
         * run) on the SEQ ROOT joins — the parser inserts the empty
         * value at the closer and the stolen key rides the same plain
         * walk as the separator form (probed byte-equal `[1,?\r##\na,2]`
         * -> `+SEQ/1/+MAP/a//`, the floor's own member; the engines
         * AGREE here — the member is an agreeing composition riding
         * the dual byte-equality firewall, like the r351 nested-SEQ
         * slot). The MAP-rooted clean eat keeps the r277 law. */
        if (e >= n) {
            return y_fail(c);
        }
        if (seps >= 1 && y_cur_is_map(c)) {
            return y_fail(c); /* the r245 steal stays SEQ-rooted */
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
        if (p[ks] == ':' && ks + 1 < n &&
            (fc_blank(p[ks + 1]) || fc_break_len(p, n, ks + 1) > 0 || fc_flow_ind(p[ks + 1]))) {
            /* the r382 widening (run 45485140275's drill, issue #1642):
             * the comment-steal's COLON-LED VALUE boundary — the r286
             * law's slot mechanics one slot over. After the eat, the
             * stolen key region may open `:` + blank/break/FLOW
             * INDICATOR: the explicit key is EMPTY (the comment ate
             * every key byte), the colon re-lexes as the VALUE boundary
             * on BOTH engines (yeptris's 1.2 flow VALUE-boundary law,
             * libyaml's unconditional flow fetch_value), and the pair
             * rides the common colon path below (the value node or the
             * parser's inserted empty; the probes: `{?TAB#`\n:TAB),}` ->
             * (e,))`, `{?TAB#\n:,}` -> (e,e)`, `{?TAB#\n:TAB),}` ->
             * (e,))`, `{?TAB#\n:}` -> (e,e)` — all EQUAL, and the
             * artifact's own member `?TAB#` LF :TAB),` renders (e,))`
             * inside the DIFFER). The floor's `?TAB# LF :,` twin (the
             * r382 floor's member 2) rides the flow-indicator follower
             * arm of the same gate. The r277 glued colon-plain twin
             * (`:` + non-boundary content) keeps the plain walk below
             * (the r277 law's own derivation). A colon at the slot's
             * EOF stays declined. */
            memset(&key, 0, sizeof(key)); /* the empty key node */
            key.colon = 1;
            key.colon_at = ks;
            stop = ks + 1;
            sep = 1;
        } else if (p[ks] == ':' && ks + 1 >= n) {
            return y_fail(c); /* a colon at the stolen-key slot's EOF:
                               * untested */
        } else {
            /* the stolen key rides the SAME plain walk the split
             * plain-key slot takes — SEQ-rooted, so the r272
             * KEY-implicit colon cut applies (the r319 artifact's
             * stolen key `/<LF><CR><CR><PS> 51:<CR><CR>1.0e17.tag` cuts
             * at the colon+break boundary: key = `/\n\n\n51`, the
             * common colon path opens the value `1.0e17.tag` — the
             * engine renders `+MAP(/\\n\\n\\n51,1.0e17.tag)` byte-
             * exact). The MAP-rooted clean eat keeps the plain law (the
             * r277 law's colon-inside key). Flat stolen keys never see
             * the difference (no colon, no cut — every r245 probe shape
             * unchanged). */
            if (!fc_plain_pieces(p, n, ks, n, !y_cur_is_map(c), 0, &key, &stop, &sep)) {
                return y_fail(c);
            }
        }
    } else if (p[ks] == '{') {
        /* the nested flow map at the split-explicit-key member's KEY
         * slot (the r310 widening, run 44191067135): the r281 law's
         * mechanics one slot up -- the artifact's root member
         * `?\t{\n...\n}` hangs the nested collection off the split
         * explicit key ITSELF (the `?` drops, the `{` opens the key
         * node, and the blank/break run after the close rides to the
         * `:` VALUE boundary the same slot walk takes). The same member
         * laws derive the collection recursively (maps only, maps
         * decline past the depth budget); a glued colon (not a VALUE
         * boundary) and any other non-boundary byte decline. */
        if (getenv("YD_FC_TRACE") != NULL) {
            fprintf(stderr, "split-key nested map at %zu\n", ks);
        }
        if (c->depth >= (int)FC_MAX_NEST) {
            return y_fail(c); /* past the nesting budget */
        }
        fout nf = {NULL, 0, 0, 0};
        fo_str(&nf, "+MAP\n");
        fout outer = c->out;
        c->out = nf;
        c->depth++;
        c->seq_at[c->depth] = 0; /* the level is a MAP (fresh stamp) */
        size_t after = 0;
        int ok = y_members(c, '}', ks + 1, &after);
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
        memset(&key, 0, sizeof(key));
        key.nest_buf = nf.buf;
        key.nest_len = nf.len;
        stop = fc_skip_blank_break(p, n, after);
        if (stop < n && p[stop] == ':' &&
            (stop + 1 >= n || fc_blank(p[stop + 1]) || fc_break_len(p, n, stop + 1) > 0 ||
             fc_flow_ind(p[stop + 1]))) {
            key.colon = 1; /* the VALUE boundary after the collection
                            * key (the r286 shape); the common colon
                            * path below opens the empty value */
            key.colon_at = stop;
        } else if (stop >= n || !fc_flow_ind(p[stop])) {
            fc_val_drop(&key);
            return y_fail(c);
        }
    } else if (p[ks] == '[') {
        /* the nested flow SEQUENCE at the split-explicit-key member's
         * KEY slot (the r351 widening, run 44956787130): the r310 law's
         * SEQ twin one slot up -- the artifact's root member
         * `?\t[]: #|#|+` ` hangs the EMPTY nested collection off the
         * split explicit key ITSELF (the `?` drops, the `[` opens the
         * key node, and the GLUED ':' after the close is the VALUE
         * boundary the same slot walk takes; the comment ride in the
         * value slot is the r309 law's own). The same member laws
         * derive the collection recursively (the r314 mechanics, maps
         * and seqs alike); a post-close run to a non-boundary byte and
         * any other composition declines. Both engines render the
         * collection as the KEY with the parser's inserted empty value
         * — the member is an AGREEING composition; its bytes ride the
         * dual byte-equality firewall. */
        if (c->depth >= (int)FC_MAX_NEST) {
            return y_fail(c); /* past the nesting budget */
        }
        if (getenv("YD_FC_TRACE") != NULL) {
            fprintf(stderr, "split-key nested SEQ at %zu\n", ks);
        }
        fout nf = {NULL, 0, 0, 0};
        fo_str(&nf, "+SEQ\n");
        fout outer = c->out;
        c->out = nf;
        c->depth++;
        c->seq_at[c->depth] = 1; /* the level is a SEQUENCE (the r314
                                  * stamp; the member walks below ride
                                  * y_cur_is_map) */
        size_t after = 0;
        int ok = y_members(c, ']', ks + 1, &after);
        c->depth--;
        nf = c->out;
        c->out = outer;
        if (!ok || nf.bad) {
            free(nf.buf);
            return 0;
        }
        fo_str(&nf, "-SEQ\n");
        if (nf.bad) {
            free(nf.buf);
            return y_fail(c);
        }
        memset(&key, 0, sizeof(key));
        key.nest_buf = nf.buf;
        key.nest_len = nf.len;
        stop = fc_skip_blank_break(p, n, after);
        if (stop < n && p[stop] == ':' &&
            (stop + 1 >= n || fc_blank(p[stop + 1]) || fc_break_len(p, n, stop + 1) > 0 ||
             fc_flow_ind(p[stop + 1]))) {
            key.colon = 1; /* the VALUE boundary after the collection
                            * key (the r286 shape; the common colon
                            * path below opens the empty value) */
            key.colon_at = stop;
        } else if (stop >= n || !fc_flow_ind(p[stop])) {
            fc_val_drop(&key);
            return y_fail(c);
        }
    } else if (p[ks] == '"' || p[ks] == '\'' || p[ks] == '&' || p[ks] == '!') {
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
            if (!fc_plain_pieces(p, n, ks, n, 0, 0, &key, &stop, &sep)) {
                return y_fail(c);
            }
        }
    }
    if (key.colon) {
        if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
            fc_val_drop(&key); /* the key-slot nested stash (the r310
                                * widening) dies with the failed value
                                * walk */
            return 0;
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        fc_val_drop(&val);
        fc_val_drop(&key);
        return y_fail(c);
    }
    *term_at = stop;
    if (y_cur_is_map(c)) {
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
        if (key_is_alias) {
            /* the r342 widening: the wrapper renders the alias key
             * event exactly as the MAP branch does (the artifact's
             * SEQ-rooted pair renders `=ALI *0:` byte-exact) */
            fo_str(&c->out, "=ALI *");
            fo_bytes(&c->out, p + alias_at, alias_len);
            fo_str(&c->out, "\n");
        } else {
            fc_render_val(&c->out, p, &key);
        }
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
        /* the member IS the quoted span. MAP root (the r314 widening,
         * run 44354924371's tail member `"empky":\n}`): the quoted KEY
         * member — the DQ fold rides fc_quoted_value (the engines AGREE
         * byte-exact on the probes `{"a"}` / `{"a":}` / `{"a":x}`; the
         * walk claims the shape to reach the later members). The
         * boundary after the span: a `:` splits UNCONDITIONALLY after a
         * quoted scalar on both engines (no plain context — the value
         * slot rides y_value_node), a flow indicator leaves the value
         * empty; anything else declines. SEQ root keeps the keyless
         * member arm below. */
        fout tmp = {NULL, 0, 0, 0};
        size_t close = 0;
        if (!fc_quoted_value(p, n, i + 1, n, &tmp, &close)) {
            free(tmp.buf);
            return y_fail(c);
        }
        size_t j = fc_skip_blank_break(p, n, close + 1);
        fcnode val;
        memset(&val, 0, sizeof(val));
        size_t qstop;
        if (y_cur_is_map(c) && j < n && p[j] == ':') {
            /* the VALUE boundary after the quoted key (unconditional
             * split on both engines) */
            if (!y_value_node(c, j + 1, &qstop, &val)) {
                free(tmp.buf);
                return 0;
            }
            if (qstop >= n || !fc_flow_ind(p[qstop])) {
                fc_val_drop(&val);
                free(tmp.buf);
                return y_fail(c);
            }
        } else if (j >= n || !fc_flow_ind(p[j])) {
            free(tmp.buf);
            return y_fail(c); /* a non-boundary byte after the span */
        } else {
            qstop = j; /* the empty value (the parser's insertion) */
        }
        *term_at = qstop;
        fo_str(&c->out, "=VAL : ");
        fo_bytes(&c->out, tmp.buf, tmp.len);
        fo_str(&c->out, "\n");
        free(tmp.buf);
        if (y_cur_is_map(c)) {
            fc_render_val(&c->out, p, &val);
        }
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
        if (y_cur_is_map(c) && p[ae] == ':' &&
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
        if (y_cur_is_map(c) && ae < n && p[ae] == ':' && ae + 1 < n && fc_flow_ind(p[ae + 1])) {
            /* the r357 widening (the nightly's run 45033480083, main
             * 1e53f6b7): the GLUED-':' VALUE boundary at the flow MAP
             * key slot — the artifact `{*L:,&-:}`'s first member. The
             * r342 cut law is the face's own law: a flow indicator
             * closes the engine's alias-name walk BEFORE the colon
             * (probed `*00:,` cut before the colon), and the artifact's
             * own y-dump (`=ALI *L` + `=VAL : `) pins the same cut at
             * the key slot. The glued ':' fires the VALUE token on BOTH
             * engines — libyaml's flow fetch_value on the alias's
             * saved simple key (probed `{*L:,}` EQUAL pre-patch) — and
             * the member derives as the alias key + the parser's
             * inserted empty at the terminator. Gates: the follower is
             * a flow indicator EXACTLY (a content follower `{*L:x}` is
             * EQUAL — never a DIFFER, no claim exists; a break follower
             * rides the r342 break law INTO the name and stays the
             * split-key family's territory), and the MAP level (the
             * SEQ slot `[*L:,]` is EQUAL, never a DIFFER). Comparator
             * carry only, no engine change: the divergence this walk
             * unlocks lives in the NEXT member's anchor-name charset
             * (the r318 law — libfyaml 04e0b58 reads the artifact's
             * key node anchor byte-exact yeptris's way, `<-:` glued,
             * BOTH --yaml-1.2 and --yaml-1.1 passes; the alias member
             * itself is libfyaml's documented third reading), the
             * charset suite-pinned (2SXE/W5VH; libyaml behind the
             * suite, the r311 law). */
            *term_at = ae + 1;
            fo_str(&c->out, "=ALI *");
            fo_bytes(&c->out, p + as, al);
            fo_str(&c->out, "\n");
            fo_str(&c->out, "=VAL : \n"); /* the parser's inserted empty */
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
        if (y_cur_is_map(c)) {
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
        /* the engine's yep_scan_prop_end cut (scan.c): pure prop
         * charset — not blank, not break, not flow indicator, ',' or
         * '#', with the multibyte breaks ending the name. NO colon
         * follower exception (that is the alias walk's Y2GN law; the
         * anchor scan is pure charset — `&m:(}` names `m:(`). */
        size_t apl = 0;
        while (as + apl < n && !fc_blank(p[as + apl]) && !fc_flow_ind(p[as + apl]) &&
               p[as + apl] != '#' && fc_break_len(p, n, as + apl) == 0) {
            apl++;
        }
        size_t al = 0;
        while (as + al < n && fc_alpha(p[as + al])) {
            al++;
        }
        size_t ae = as + al;
        if (al == 0) {
            return y_fail(c);
        }
        if (y_cur_is_map(c)) {
            /* the r318 widening (run 44458062455): the MAP key slot's
             * charset-divergent anchor member. When the two cuts
             * disagree and libyaml's cut lands on a GLUED ':' (the
             * artifact's `&m:(}` — the engine's prop charset keeps
             * `:(` inside the name per the suite (2SXE/W5VH; libyaml
             * is behind the suite, the r311 law), libyaml's scan_anchor
             * cuts at `m` and re-lexes the tail: the 1.1 flow VALUE
             * token fires on the glued ':' (key = anchored-empty `&m`,
             * value = the plain `(`), while yeptris keeps the whole
             * name so NO VALUE token fires and the parser inserts the
             * empty value at the closer. The member derives as the
             * anchored-empty key (`=VAL : &m:( `) + the inserted empty
             * (`=VAL : `); the libyaml walk derives the cut tail
             * independently and the entry's dual byte-equality is the
             * firewall. Gates: the ':' glued AT the libyaml cut (a
             * blank/break between cut and ':' re-opens the value-slot
             * compositions the probes never cover), the prop name
             * terminated by a flow indicator, and the divergence real
             * (apl > al — an agreeing name is the r76-arm territory,
             * declined). A first non-alpha cut byte that is not ':'
             * re-lexes as plain content with no VALUE token — the
             * libyaml parser errors there (not a DIFFER shape). */
            if (apl <= al || ae >= n || p[ae] != ':') {
                return y_fail(c);
            }
            size_t te = as + apl;
            if (te < n && fc_flow_ind(p[te])) {
                *term_at = te;
                fo_str(&c->out, "=VAL : &");
                fo_bytes(&c->out, p + as, apl);
                fo_str(&c->out, " \n");
                fo_str(&c->out, "=VAL : \n"); /* the parser's inserted empty */
                return 1;
            }
            /* the r355 widening (the nightly's run 45016455586, main
             * b3b8b07f): the engine cut ends at a BLANK/BREAK RUN and an
             * ALIAS fires on the anchored node — the artifact's `&m:=ALI
             * *(}` member. The engine's prop charset keeps the glued ':'
             * and the tail run up to the blank (`m:=ALI` — the r318
             * suite-pinned cut, 2SXE/W5VH); the blank closes it, and the
             * next token decides the node: a `*` fires the engine's
             * alias walk, whose name cut rides to the member terminator
             * (`(` rides the alias charset — the r342/r353 paren law's
             * charset neighbor; probed `{*(: 1}` -> `=ALI *(`). The
             * anchor prop does NOT render on the alias event: the
             * member derives as the bare `=ALI *<name>` plus the
             * parser's inserted empty (`=VAL : `) at the closer.
             * libyaml's walk derives its own reading independently (the
             * IS_ALPHA cut at `m`, the glued ':' re-lexed as the VALUE
             * token, the tail as one flat plain — mid-plain `*(` is
             * content), and the entry's dual byte-equality is the
             * firewall. Probes (byte-exact both sides): `{&m:=ALI *(}`,
             * `{&m:I *(}`, `{&m:ALI *(}` — all DIFFER with this shape
             * (the artifact's own floor `{?*L,&m:=ALI *(}` and role
             * floor `{?*L,&m:I *(}` ride it after the glued-? rider
             * member); the ALIAS-START twin `{&m: *x}` DIFFERs with the
             * same y-side (the libyaml value is an ALIAS event there,
             * its valid IS_ALPHA name — the l-walk derives it and the
             * firewall keeps the claim honest). The `(`-name
             * star-first twin `{&m: *(}` is libyaml's scan_anchor ERROR
             * (a split, the ledger's territory — no claim here). Gates:
             * the run is blank/break only (a `#` inside it is the
             * engine's comment condition — untested, declined), the
             * node is the alias marker EXACTLY (a plain-leading tail
             * keeps the alias INSIDE the plain — probed `{a:I *(}` /
             * `{a:*(}` EQUAL, and the anchored-plain ride
             * `{&m:  I *(}` / `{&m:\nI *(}` classifies through the
             * plain arms), the name cut is non-zero and ends AT a flow
             * indicator (the member terminator), and the divergence is
             * real (apl > al). Comparator carry only, no engine change:
             * libfyaml 04e0b58 errors the floor and the artifact
             * outright ("unexpected alias" at the star, both --yaml-1.2
             * and --yaml-1.1 passes) — no third vote, the comparators
             * split along the libyaml-compat-bar line exactly as the
             * r335/r317 precedent did, and the underlying charset laws
             * are suite-pinned (2SXE/W5VH; libyaml is behind the suite,
             * the r311 law). */
            if (te >= n || (!fc_blank(p[te]) && fc_break_len(p, n, te) == 0)) {
                return y_fail(c);
            }
            size_t av = fc_skip_blank_break(p, n, te);
            if (av >= n || p[av] != '*') {
                return y_fail(c);
            }
            size_t ans = av + 1;
            size_t anl = fc_alias_name(p, n, ans, n);
            if (anl == 0) {
                return y_fail(c);
            }
            size_t ate = ans + anl;
            if (ate >= n || !fc_flow_ind(p[ate])) {
                return y_fail(c);
            }
            *term_at = ate;
            fo_str(&c->out, "=ALI *");
            fo_bytes(&c->out, p + ans, anl);
            fo_str(&c->out, "\n");
            fo_str(&c->out, "=VAL : \n"); /* the parser's inserted empty */
            return 1;
        }
        /* the r315 widening (run 44411162279): a blank/break run between
         * the anchor name and the member terminator rides like the alias
         * slots take it (the r301/r302 law — the same prop walk, the
         * engine skips the run before the flow indicator and nothing
         * folds into the empty node: the artifact's `&i\r,` /
         * `&binary\r,` sites render `=VAL : &<name> ` byte-exact on both
         * engines). A `#` inside the run stops the walk (the
         * flow-indicator gate declines it — the engine's comment
         * condition fires there); a `?`/`:` after the run is no flow
         * indicator and declines the same way (the KEY/VALUE boundary
         * compositions the probes never cover). */
        size_t ve = fc_skip_blank_break(p, n, ae);
        if (ve >= n || !fc_flow_ind(p[ve])) {
            return y_fail(c);
        }
        *term_at = ve;
        fo_str(&c->out, "=VAL : &");
        fo_bytes(&c->out, p + as, al);
        fo_str(&c->out, " \n");
        return 1;
    }
    if (ch == '!') {
        /* the r335 non-specific-tag member (the nightly's run
         * 44808502141, main af1bacea): a flow-SEQ member that IS the
         * bare `!` tag (the artifact's `!,` slot — the tag-glued
         * comment-piece family's `!` riding before a glued-`?#`
         * member). The engine's yep_scan_tag resolves the bare `!` to
         * the non-specific tag and the member's flow-indicator
         * terminator leaves the tagged node EMPTY — the dump drops the
         * tag (the non-specific `!` resolves by style, the r335
         * artifact's actual `=VAL : ` line byte-exact on BOTH engines:
         * libyaml's parser renders the same tagged empty, and the
         * classifier's l-walk already models the bare-`!` drop in
         * l_render_node — the y-side arm is the last model gap). The
         * arm claims the member so the walk reaches the later members
         * (`?#`, the quoted-CR span, `?g?`, `""`). Gates: the bare `!`
         * EXACTLY (a `!name`/`!!` handle or a `!<verbatim>` form
         * carries dump props and a scalar payload — untested, the
         * single-byte adjacency keeps them out), the flow indicator
         * GLUED after the tag (a blank/break run opens the tagged
         * scalar-value slot the probes never cover — the r315
         * anchor-arm's run shape, declined on the tag slot), and the
         * SEQ slot (a MAP key slot renders the tagged-empty KEY plus
         * the parser's inserted empty — untested; the artifact's slot
         * is SEQ). libfyaml 04e0b58 errors the artifact outright
         * ("invalid tag terminator", both --yaml-1.1 and --yaml-1.2
         * passes) — no third vote; the comparators split along the
         * libyaml-is-the-compat-bar line exactly as the r317 precedent
         * did — comparator carry only, no engine change. */
        if (y_cur_is_map(c) || i + 1 >= n || !fc_flow_ind(p[i + 1])) {
            return y_fail(c);
        }
        *term_at = i + 1;
        fo_str(&c->out, "=VAL : \n");
        return 1;
    }
    if (ch == '\'' || ch == '!' || ch == '#' || ch == '{') {
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
    if (ch == '[') {
        /* the r314 law (run 44354924371): the nested flow SEQUENCE at a
         * member start — the r281 nested-flow-map law's SEQ twin (the
         * r310 widening's mechanics at the plain member slot). The
         * member IS the nested collection: the same member laws derive
         * its entries recursively (+SEQ/-SEQ around them), stashed on
         * the key node like the value-slot `{` stash. yeptris keeps the
         * collection as the bare member (the map's value slot empty) or
         * as the key before a VALUE boundary; the libyaml side rides
         * the l-side nested-seq law (the tokenizer level slots + the
         * recursive seq body). The floor `{[?&m]} ` fires exactly this:
         * the glued-`?` plain `?&m` inside the nested seq (the HM87/652Z
         * plain-? law, d) against libyaml's KEY-token wrapper (the 1.1
         * flow-`?` law — `&` is an indicator follower, outside the r183
         * arm's plain-safe gate). A quoted / `?`-slot `[` keeps its
         * decline (untested shapes stay out); the value-slot `[` took
         * the r371 widening (the nested-seq VALUE arm above). */
        if (c->depth >= (int)FC_MAX_NEST) {
            return y_fail(c); /* past the nesting budget */
        }
        fout nf = {NULL, 0, 0, 0};
        fo_str(&nf, "+SEQ\n");
        fout outer = c->out;
        c->out = nf;
        c->depth++;
        c->seq_at[c->depth] = 1; /* the level is a SEQUENCE (the r314
                                  * stamp; the member walks below ride
                                  * y_cur_is_map) */
        size_t after = 0;
        int ok = y_members(c, ']', i + 1, &after);
        c->depth--;
        nf = c->out;
        c->out = outer;
        if (!ok || nf.bad) {
            free(nf.buf);
            return 0;
        }
        fo_str(&nf, "-SEQ\n");
        if (nf.bad) {
            free(nf.buf);
            return y_fail(c);
        }
        memset(&key, 0, sizeof(key));
        key.nest_buf = nf.buf;
        key.nest_len = nf.len;
        /* the r351 widening (run 44956787130): a BLANK-preceded comment
         * ride sits between the closer and the member terminator (the
         * artifact's `[] #\n,` site — the comment law is the y_members
         * loop-top's own: a blank/break precedes the `#`, both engines
         * eat to the break, and the walk re-enters at the separator;
         * the value slot stays EMPTY — the parser's inserted scalar).
         * A post-comment ':' is the composition the artifact never
         * covers (untested shapes stay out), and a GLUED `]#` stays out
         * with it (the documented yeptris-rejects/libyaml-accepts
         * comment split, never a comment). */
        int rode_comment = 0;
        stop = fc_skip_blank_break(p, n, after);
        while (stop < n && p[stop] == '#' && stop > after) {
            rode_comment = 1;
            while (stop < n && fc_break_len(p, n, stop) == 0) {
                stop++;
            }
            stop = fc_skip_blank_break(p, n, stop);
        }
        if (stop < n && p[stop] == ':' && !rode_comment) {
            key.colon = 1; /* the VALUE boundary after the collection
                            * key: a `:` splits UNCONDITIONALLY after a
                            * closer on both engines (no plain context
                            * survives `]` — the r314 probe `{[?&m]:x}`
                            * pairs the value byte-exact); the common
                            * colon path below opens the value slot */
            key.colon_at = stop;
        } else if (stop >= n || !fc_flow_ind(p[stop])) {
            fc_val_drop(&key);
            return y_fail(c);
        }
        if (key.colon) {
            /* the value node opens in the branch (the shared colon path
             * below sits inside the plain-led arm) */
            if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
                return 0;
            }
        }
    } else if (ch == ':' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
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
        if (!fc_plain_pieces(p, n, i, n, 0, 0, &key, &stop, &sep)) {
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
    if (y_cur_is_map(c)) {
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
        /* the r315 widening (run 44411162279): the run between the name
         * and the terminator rides blank/break (the bare-anchor member's
         * law — the artifact's `,\r&binary\r,` fold-in member renders
         * `=VAL : &<name> \n=VAL : \n` byte-exact on both engines) */
        size_t ve = fc_skip_blank_break(p, n, ae);
        if (al == 0 || ve >= n || !fc_flow_ind(p[ve])) {
            return y_fail(c);
        }
        *term_at = ve;
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
    if (ch == '?' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
        return y_fail(c); /* the SPLIT explicit-key member under the open
                           * pair: unprobed, stays out. The GLUED spell
                           * falls through to the plain walk (P7's `?a`
                           * law — the question rides as content, the
                           * r315 artifact's ` ?#ԃ` fold-in member takes
                           * the key slot with the parser's empty value,
                           * byte-exact on both engines). */
    }
    if (ch == '\'' || ch == '#' || ch == '[' || ch == '{') {
        return y_fail(c); /* the rest of the non-flat spell: unprobed
                           * under the open pair */
    }
    if (ch == '-' && (i + 1 >= n || fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0 ||
                      fc_flow_ind(p[i + 1]))) {
        return y_fail(c);
    }
    /* the plain member: glued colons and questions ride as content (P5's
     * `a:b`, P7's `?a`). The r321 widening (the scheduled nightly's
     * round 321, run 44511876660, main 40e5dac9 — the 18-byte floor
     * `[1,?#,:<CR>1,?/<PS>:<CR>]`): a `:`-VALUE boundary after the
     * piece's break run now rides the common value slot — the
     * engine re-lexes the post-break `:` as the VALUE token (the
     * boundary arms of fc_piece/fc_plain_pieces already detect it:
     * `:` + flow indicator and `:` + break both terminate the plain),
     * and the value node rides y_value_node exactly like the
     * MAP-root/plain member path above (the probes `[1,:<CR>m,b<PS>:<CR>]`
     * -> key `b` value empty, `...b<PS>:<CR>9` -> value `9`,
     * `...b<PS>:,` -> the `:`+flow-ind form, all byte-exact on the
     * engine — every isolated instance is YEPTRIS-ONLY (libyaml's
     * raw-spell parser error), so the classifier is consulted only on
     * the composed DIFFER shapes the other members' laws reach). Only
     * the plain/empty value form claims (an alias, an anchor, a
     * nested map, or a second boundary in the value: unprobed under
     * the open pair, declines). */
    fcnode key;
    memset(&key, 0, sizeof(key));
    fcnode val;
    memset(&val, 0, sizeof(val));
    size_t stop;
    int sep;
    if (!fc_plain_pieces(p, n, i, n, 0, 0, &key, &stop, &sep)) {
        return y_fail(c);
    }
    if (key.colon) {
        if (!y_value_node(c, key.colon_at + 1, &stop, &val)) {
            fc_val_drop(&val);
            return 0;
        }
        if (val.colon || val.alias_len > 0 || val.anchor_len > 0 || val.nest_buf != NULL) {
            fc_val_drop(&val);
            return y_fail(c); /* a non-plain value under the open pair: unprobed */
        }
    }
    if (stop >= n || !fc_flow_ind(p[stop])) {
        fc_val_drop(&val);
        return y_fail(c);
    }
    *term_at = stop;
    fc_render_val(&c->out, p, &key);
    fc_render_val(&c->out, p, &val);
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
        if (ch == ',' || ch == (y_cur_is_map(c) ? ']' : '}')) {
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
    /* the lead: the r349 doc-marker run (its walk consumed every hop;
     * the entry gate verified the bracket AT c->lead_end), else the
     * blank/break run before the root bracket (the r296 artifact's
     * `\n [` head) — both engines scan-to-token before the root, so
     * the members start past it */
    size_t lead = fc_skip_blank_break(p, n, c->lead_end);
    fo_str(&c->out, "+STR\n");
    /* the r349 law: each doc marker but the last opens an EMPTY first
     * document (`---\n---\n[...]` renders one `=VAL : ` on both
     * engines) */
    for (int k = 1; k < c->lead_docs; k++) {
        fo_str(&c->out, "=VAL : \n");
    }
    /* the r375 tag-property lead: the tag rides the ROOT header (the
     * bare `!` resolves to the non-specific tag the renderer keeps on
     * a collection — only the scalar's empty render drops it, the
     * r335 law — byte-equal `<!>` on both engines across the probe
     * matrix) */
    if (c->lead_tag) {
        fo_str(&c->out, c->is_map ? "+MAP <!>\n" : "+SEQ <!>\n");
    } else {
        fo_str(&c->out, c->is_map ? "+MAP\n" : "+SEQ\n");
    }
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

static int y_derive(const uint8_t* data, size_t size, int is_map, size_t lead_end, int lead_docs,
                    int lead_tag, fout* out) {
    yctx c;
    memset(&c, 0, sizeof(c));
    c.p = data;
    c.n = size;
    c.is_map = is_map;
    c.lead_end = lead_end; /* the r349 doc-marker lead (0 = no lead) */
    c.lead_docs = lead_docs;
    c.lead_tag = lead_tag; /* the r375 tag-property lead (the header `<!>`) */
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
    size_t lead_end; /* the r349 doc-marker lead's end (the root bracket;
                      * the tokenizer starts there, 0 = no lead) */
    int lead_docs;   /* the lead's marker count (the render emits one
                      * empty first document per marker but the last) */
    int lead_tag;    /* the r375 tag-property lead: the tokenizer starts
                      * at offset 0 so the TAG token precedes the root
                      * (l_render_stream renders the header `<!>`) */
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
        if (cc == '\\' && i + 1 < n && p[i + 1] == 'u') {
            /* the r374 widening (run 45259544033, main 84150a76): the
             * `\uXXXX` escape decodes on the libyaml side too — the
             * matrix measured libyaml's scan_flow_scalar decode
             * IDENTICAL to yeptris's (mid-content, chained, at the
             * lead, at the close edge, across the fold compositions;
             * surrogates and the malformed spellings are both-errors,
             * never a consult). The decoded bytes ride the dump escape;
             * the pending fold run / blank run flushes exactly as a
             * content byte does (libyaml's leading_blanks semantics,
             * fc_quoted_value's twin arm). A surrogate codepoint
             * declines (a decline is cheap; it can only be a
             * both-error, never this family's DIFFER). */
            if (run_n > 0) {
                fc_brk_join(o, kinds, run_n); /* the pending fold joins
                                               * (the escape is content:
                                               * libyaml's leading_blanks
                                               * flush, the r287 twin) */
                run_n = 0;
                blank_n = 0;
            } else if (blank_n > 0) {
                if (first && (run_n != 1 || kinds[0].raw)) {
                    return l_fail(c); /* the blank-led span gate, the
                                       * content path's twin at the
                                       * escape byte */
                }
                fo_escape(o, p + blank_at, blank_n);
                blank_n = 0;
            }
            first = 0;
            if (i + 6 > n) {
                return l_fail(c); /* the hex digits must be inside the
                                   * input (a truncated escape is a
                                   * both-error, never this family) */
            }
            unsigned v = 0;
            for (int k = 2; k <= 5; k++) {
                if (!fc_hex(p[i + (size_t)k])) {
                    return l_fail(c); /* a bad hex digit: both-error */
                }
                v = (v << 4) + fc_hexval(p[i + (size_t)k]);
            }
            if (v >= 0xD800 && v <= 0xDFFF) {
                return l_fail(c); /* a surrogate half: both reject */
            }
            uint8_t utf[4];
            size_t un;
            if (v < 0x80) {
                utf[0] = (uint8_t)v;
                un = 1;
            } else if (v < 0x800) {
                utf[0] = (uint8_t)(0xC0 | (v >> 6));
                utf[1] = (uint8_t)(0x80 | (v & 0x3F));
                un = 2;
            } else {
                utf[0] = (uint8_t)(0xE0 | (v >> 12));
                utf[1] = (uint8_t)(0x80 | ((v >> 6) & 0x3F));
                utf[2] = (uint8_t)(0x80 | (v & 0x3F));
                un = 3;
            }
            fo_escape(o, utf, un);
            i += 6;
            continue;
        }
        if (cc == '\\') {
            return l_fail(c); /* every other escape follower: outside */
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
            if (c->depth >= (int)FC_MAX_NEST) {
                return l_fail(c); /* past the nesting budget */
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
            /* the document indicators at a line start — libyaml's fetch
             * law (scanner.c's fetch_next_token) fires the indicator
             * only when the byte AFTER the triple is IS_BLANKZ (blank,
             * break, or EOF); `...L` (the r321 artifact's tail member,
             * run 44511876660) rides as PLAIN CONTENT — the model's
             * check was stricter than the engine and declined the
             * whole soup on it. */
            if (col0 && n - c->pos >= 3 &&
                (memcmp(p + c->pos, "---", 3) == 0 || memcmp(p + c->pos, "...", 3) == 0) &&
                (n - c->pos == 3 || fc_blank(p[c->pos + 3]) ||
                 fc_break_len(p, n, c->pos + 3) > 0)) {
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
static int l_render_seq(lctx* c, fout* o, size_t* ip);
static int l_render_map_rt(lctx* c, fout* o, size_t* ip, int root_tag);

/* the nested arm: the root header rides bare (root_tag 0) */
static int l_render_map(lctx* c, fout* o, size_t* ip) {
    return l_render_map_rt(c, o, ip, 0);
}

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
            t2->kind == LT_FLMAP || t2->kind == LT_FLSEQ) {
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
    if (t->kind == LT_FLSEQ) {
        if (anchor != NULL) {
            return l_fail(c); /* an anchored nested seq: outside */
        }
        return l_render_seq(c, o, ip);
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
static int l_render_map_rt(lctx* c, fout* o, size_t* ip, int root_tag) {
    (*ip)++; /* past FLMAP-START */
    /* the r375 tag-property lead rides the ROOT map's header (the
     * nested maps always render bare — root_tag is 0 below the root) */
    fo_str(o, root_tag ? "+MAP <!>\n" : "+MAP\n");
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

/* The nested flow-sequence body (the r314 law's libyaml side): *ip sits
 * ON the FLSEQ-START token. The same walk the root seq render takes —
 * the KEY single-pair wrapper (+MAP/-MAP, the parser's
 * flow_sequence_entry_mapping_* states), ENTRY separators with the
 * trailing-comma re-peek, key-less members rendering the bare node —
 * ending at the level's FLSEQ-END (the root walk's EOS/-STR tail stays
 * the root's alone). The floor's `[?&m]` member renders
 * `+MAP/=VAL : &m / =VAL : / -MAP` — the anchor rides the wrapper's
 * EMPTY key (the node-less position law) while the parser's value slot
 * inserts the empty scalar. */
static int l_render_seq(lctx* c, fout* o, size_t* ip) {
    (*ip)++; /* past FLSEQ-START */
    fo_str(o, "+SEQ\n");
    int first = 1;
    for (;;) {
        if (*ip >= c->ntoks) {
            return l_fail(c);
        }
        ltok* e = &c->toks[*ip];
        if (e->kind == LT_EOS || e->kind == LT_ERROR) {
            return l_fail(c); /* the collection never closed */
        }
        if (e->kind == LT_FLSEQ_END) {
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
            if (e->kind == LT_FLSEQ_END) {
                (*ip)++;
                break;
            }
            if (e->kind == LT_EOS || e->kind == LT_ERROR) {
                return l_fail(c);
            }
        }
        first = 0;
        if (e->kind == LT_KEY) {
            /* the single-pair mapping wrapper */
            (*ip)++;
            fo_str(o, "+MAP\n");
            if (*ip >= c->ntoks) {
                return l_fail(c);
            }
            ltok* k = &c->toks[*ip];
            if (k->kind == LT_VALUE || k->kind == LT_ENTRY) {
                (*ip)++; /* the SEQ mapping-key state consumes this token */
                fo_str(o, "=VAL : \n");
            } else if (k->kind == LT_FLSEQ_END) {
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
                if (v->kind == LT_ENTRY || v->kind == LT_FLSEQ_END) {
                    fo_str(o, "=VAL : \n");
                } else {
                    if (!l_render_node(c, o, ip)) {
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
        if (!l_render_node(c, o, ip)) {
            return l_fail(c);
        }
    }
    fo_str(o, "-SEQ\n");
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
    /* the r349 law: each doc marker but the last opens an EMPTY first
     * document (byte-equal on both engines — see y_root's render) */
    for (int k = 1; k < c->lead_docs; k++) {
        fo_str(o, "=VAL : \n");
    }
    ltok* t = &c->toks[ip];
    /* the r375 tag-property lead (run 45281713143): the bare `!` tag
     * token precedes the root collection (parse_node's properties
     * walk) — the tag rides the ROOT header byte-equal on both engines
     * (`+SEQ <!>`/`+MAP <!>` across the r375 probe matrix; the
     * collection's tag never "resolves by style" — only the scalar's
     * empty render drops it, the r335 law). Gates: the bare `!` token
     * EXACTLY (a `!!`/`!h!` handle or a `!<verbatim>` form carries a
     * different resolved tag — unverified shapes stay out), and the
     * collection DIRECTLY after the lead run (fc_tag_lead already
     * declined every other shape upstream). */
    int root_tag = 0;
    if (t->kind == LT_TAG) {
        if (t->tlen != 1 || t->text == NULL || t->text[0] != '!') {
            return l_fail(c);
        }
        root_tag = 1;
        ip++;
        if (ip >= c->ntoks) {
            return l_fail(c);
        }
        t = &c->toks[ip];
    }
    int is_seq;
    if (t->kind == LT_FLSEQ) {
        is_seq = 1;
    } else if (t->kind == LT_FLMAP) {
        /* the flow mapping (recursive body — the r281 nested-map law);
         * the tag lead rides the root header (the r375 widening) */
        if (!l_render_map_rt(c, o, &ip, root_tag)) {
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
    if (root_tag) {
        fo_str(o, is_seq ? "+SEQ <!>\n" : "+MAP <!>\n");
    } else {
        fo_str(o, is_seq ? "+SEQ\n" : "+MAP\n");
    }
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

static int l_derive(const uint8_t* data, size_t size, size_t lead_end, int lead_docs, int lead_tag,
                    fout* out) {
    lctx c;
    memset(&c, 0, sizeof(c));
    c.p = data;
    c.n = size;
    c.pos = lead_tag ? 0 : lead_end; /* the r349 doc-marker lead: the
                                      * tokenizer starts past it (0 =
                                      * no lead, the old walk); the
                                      * r375 tag-property lead restarts
                                      * AT the `!` so the TAG token is
                                      * emitted ahead of the root */
    c.lead_end = lead_end;
    c.lead_docs = lead_docs;
    c.lead_tag = lead_tag;
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
    size_t lead_end = 0;
    int lead_docs = fc_doc_lead(data, size, &lead_end);
    int lead_tag = 0;
    if (lead_docs == 0 && data[0] == '!') {
        /* the r375 tag-property lead (the bare `!` + blank/break run
         * ahead of the root bracket): the walk admits exactly the
         * probe-verified shapes (fc_tag_lead) — every other lead byte
         * still declines */
        lead_tag = fc_tag_lead(data, size, &lead_end);
    }
    size_t root = (lead_docs > 0 || lead_tag) ? lead_end : fc_skip_blank_break(data, size, 0);
    if (root >= size || (data[root] != '[' && data[root] != '{')) {
        return 0; /* the family is flow-rooted (law j): a leading
                   * blank/break run is scan-to-token territory on both
                   * engines (the r296 artifact's `\n [` head), the
                   * r349 doc-marker LEAD walk admits exactly the
                   * verified marker-line shapes (fc_doc_lead, one empty
                   * first document per marker but the last), and the
                   * r375 tag-property lead admits the bare-`!` + run
                   * head (fc_tag_lead) — any other
                   * lead byte (a comment, a directive, a doc-end run the
                   * lead walk declined, an indented `---`) still
                   * declines */
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
    int ok = y_derive(data, size, is_map, lead_end, lead_docs, lead_tag, &yd);
    if (ok) {
        ok = l_derive(data, size, lead_end, lead_docs, lead_tag, &ld);
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

/* ---- the r386 glue-fold composition waiver (law w) -------------------
 *
 * The measured member grammar (the four red runs' artifacts + the
 * r361/r384 pins) and the byte-exact yeptris-side render. Everything
 * unmeasured declines; the waiver is a decline, never a claim.
 * The r388 extension (issue #1665, run 45610560196): the folded prop
 * tail's anchor NAME rides any IS_ALPHA spelling (the fresh
 * `?##\r\n&mq` member — the measured grammar already rendered the
 * folded plain; the content-set ADMISSION check gains the prop-render
 * twin its allowance rule (b) carried, so the rebind `&mq ` maps even
 * without a bare `&mq,` member elsewhere in the stream).
 */

/* the decline set: single quotes (the anchor-ride quoted-body class —
 * the r238 arm owns it), the reserved backtick (the r382 lenient
 * members ride their own arm), the escape byte (law k) */
static int fw_decline_byte(unsigned char c) {
    return c == '\'' || c == '`' || c == '\\';
}

/* a content byte inside a member (the measured alphabet: printable
 * ASCII minus the flow-structural bytes; every letter/digit rides;
 * the exotic bytes come only as the PS/LS break multibyte, which the
 * break walk consumes) */
static int fw_content_byte(unsigned char c) {
    if (c < 0x20 || c == 0x7f) {
        return 0;
    }
    if (fc_flow_ind(c)) {
        return 0; /* ,[]{} are structure — a `[`/`{` inside a member is
                   * nesting (unmeasured: declines); the matching
                   * closer terminates the member */
    }
    return 1;
}

/* The fold-law content render (the yeptris-side render, byte-exact):
 * content pieces with interior blanks verbatim, the blank edges of a
 * break run eaten (the trailing-piece trim and the post-fold blank
 * eat — the measured `/\n\r\r<PS> 51` -> `/\n\n\n51` and
 * `1.0e17.tag\r ` -> `1.0e17.tag`), the break-BYTE count law
 * (CRLF = one break; 1 -> space, n>1 -> n-1 `\n` escapes), bytes per
 * fo_escape. Stops at `,`, the closer, or end; nesting (`[`/`{`)
 * declines. Returns 0 on a `:` VALUE boundary it wants the caller to
 * see (via at_colon) or on any unmeasured shape. */
static int fw_render_content(const uint8_t* p, size_t n, size_t i, unsigned char closer, fout* o,
                             size_t* stop_at, int* at_colon) {
    size_t k = i;
    int started = 0;
    *at_colon = 0;
    for (;;) {
        /* the content run (interior blanks ride verbatim) */
        size_t cs = k;
        while (k < n && fw_content_byte(p[k]) && !fc_blank(p[k]) && fc_break_len(p, n, k) == 0 &&
               p[k] != ':' && p[k] != closer) {
            k++;
        }
        if (k > cs) {
            fo_escape(o, p + cs, k - cs);
            started = 1;
        }
        if (k >= n || p[k] == ',' || p[k] == closer) {
            break; /* the member terminator: trailing run unrendered */
        }
        if (p[k] == ':') {
            /* the boundary fires only when a blank/break (or the
             * member end) follows — a glued `:` + content is interior
             * content (the `1.0e57.tag:y|` law) */
            if (k + 1 < n && (fc_blank(p[k + 1]) || fc_break_len(p, n, k + 1) > 0)) {
                *at_colon = 1;
                *stop_at = k;
                return 1;
            }
            if (k + 1 >= n) {
                return 0; /* the member-end colon: unmeasured */
            }
            /* glued to content: ride it */
            fo_str(o, ":");
            k++;
            started = 1;
            continue;
        }
        if (fc_flow_ind(p[k])) {
            return 0; /* a non-matching flow indicator inside a member:
                       * nesting — unmeasured */
        }
        if (fw_decline_byte(p[k])) {
            return 0;
        }
        /* the blank/break run (blank edges of a break run eaten) */
        size_t bs = k;
        uint32_t brk = 0;
        while (k < n && (fc_blank(p[k]) || fc_break_len(p, n, k) > 0)) {
            int bl = fc_break_len(p, n, k);
            if (bl > 0) {
                if (bl == 1 && p[k] == '\r' && k + 1 < n && p[k + 1] == '\n') {
                    k += 2;
                } else {
                    k += (size_t)bl;
                }
                brk++;
                continue;
            }
            k++;
        }
        if (brk == 0) {
            /* a blank-only run: verbatim when content follows (the
             * `+\t \t tag:` interior), unrendered at the terminator
             * (the loop breaks there next round). The r389 extension
             * (the post-r388 nightly's run 45625878936): the run that
             * pads a `:` VALUE boundary is EATEN, never ridden — the
             * measured renders (`{=VAL : x,}` / `{=VAL  : x,}` /
             * `{x  : y,}` / `{x  y : z,}` all byte-equal: interior
             * blanks ride, the boundary padding vanishes) — the engine
             * ends the key at its last content byte; a NON-boundary
             * colon (glued content: `{x :2002:binary,}`) still rides
             * the run and the colon verbatim. The r386 grammar never
             * carried a blank-before-boundary member (its colon faces
             * are glued (`2002:binary`) or break-run-fed (`50:`-
             * class), both unchanged) — the hole surfaced only on the
             * CopyPart paste member `=VAL : ...` whose dump-vocabulary
             * spacing rode one byte into the derived key (`=VAL ` vs
             * the actual `=VAL`). */
            if (k < n && p[k] != ',' && p[k] != closer && started && fw_content_byte(p[k]) &&
                !(p[k] == ':' && k + 1 < n &&
                  (fc_blank(p[k + 1]) || fc_break_len(p, n, k + 1) > 0))) {
                fo_escape(o, p + bs, k - bs); /* the interior blank run
                                               * rides escaped (the
                                               * measured `+\t \t tag:`
                                               * render) */
            }
            continue;
        }
        /* a break run: the fold fires only when content follows (the
         * trailing run at the member end is the terminator's trim) */
        if (k >= n || p[k] == ',' || p[k] == closer || !fw_content_byte(p[k])) {
            continue;
        }
        if (p[k] == ':') {
            /* the fold run ends at a VALUE boundary — the key slot's
             * post-break colon (the PS-key members). The boundary is
             * the caller's (the value slot follows). */
            *at_colon = 1;
            *stop_at = k;
            return 1;
        }
        if (fw_decline_byte(p[k]) || fc_flow_ind(p[k])) {
            return 0;
        }
        fo_fold_run(o, brk);
        started = 1;
    }
    *stop_at = k;
    return 1;
}

/* The split-`?` key slot's render: content-led rides the fold render;
 * an `!`-led slot is the TAGGED-EMPTY render (the measured
 * `?\r!2002:binary\r ` -> `<!2002:binary> `: the tag rides, the value
 * empty). Returns 0 on any other shape. */
static int fw_render_key_slot(const uint8_t* p, size_t n, size_t ks, unsigned char closer, fout* o,
                              size_t* stop_at, int* at_colon, int is_value) {
    if (ks >= n) {
        return 1; /* the empty slot (the `{? \t ,` head) */
    }
    if (is_value && p[ks] == '&') {
        /* the ANCHORED-EMPTY value slot (the measured `<PS> 50:\r\r&m,`
         * members -> `=VAL : &m `: the prop render, the name cut at the
         * terminator) */
        size_t ae = ks + 1;
        while (ae < n && fc_alpha(p[ae])) {
            ae++;
        }
        if (ae == ks + 1 || ae >= n || (p[ae] != ',' && p[ae] != closer)) {
            return 0;
        }
        fo_str(o, "&");
        fo_bytes(o, p + ks + 1, ae - ks - 1);
        fo_str(o, " ");
        *stop_at = ae;
        *at_colon = 0;
        return 1;
    }
    if (p[ks] == '!') {
        /* the tag run: `!` + content up to the blank/break/terminator */
        size_t te = ks + 1;
        while (te < n && fw_content_byte(p[te]) && !fc_blank(p[te]) &&
               fc_break_len(p, n, te) == 0 && p[te] != ',' && p[te] != closer) {
            te++; /* interior ':' rides (the measured `!2002:binary`) */
        }
        if (te == ks + 1) {
            return 0; /* a bare `!`: unmeasured */
        }
        fo_str(o, "<");
        fo_escape(o, p + ks, te - ks);
        fo_str(o, "> ");
        /* the tag run ends at its trailing blank/break run — the run
         * rides to the terminator (the measured `?!`-member shape:
         * `!2002:binary\r ,`) */
        *stop_at = fc_skip_blank_break(p, n, te);
        *at_colon = 0;
        return 1;
    }
    if (p[ks] == ':') {
        /* the colon-led empty key (the slot's content was all eaten):
         * the boundary is the VALUE token — the caller opens the value
         * slot from here */
        if (ks + 1 < n && (fc_blank(p[ks + 1]) || fc_break_len(p, n, ks + 1) > 0)) {
            *stop_at = ks;
            *at_colon = 1;
            return 1;
        }
        return 0;
    }
    if (p[ks] == ',' || p[ks] == closer) {
        /* the EMPTY slot: the run ate every key byte (the measured
         * `{? \t ,` head member renders the empty key + empty value) */
        *stop_at = ks;
        *at_colon = 0;
        return 1;
    }
    if (fw_decline_byte(p[ks])) {
        return 0;
    }
    return fw_render_content(p, n, ks, closer, o, stop_at, at_colon);
}

/* One member: from i (at the first byte after the separator's blank
 * run), to the terminator. Emits the yeptris event lines for the
 * member. Returns 0 on any unmeasured shape. */
static int fw_member(const uint8_t* p, size_t n, size_t i, unsigned char closer, int is_map,
                     fout* o, size_t* term_at) {
    if (i >= n) {
        return 0;
    }
    unsigned char c = p[i];
    size_t stop = 0;
    int colon = 0;

    if (c == '?' && i + 1 < n && (fc_blank(p[i + 1]) || fc_break_len(p, n, i + 1) > 0)) {
        /* the SPLIT-`?` member: the key slot after the run */
        size_t ks = fc_skip_blank_break(p, n, i + 1);
        fout kout = {NULL, 0, 0, 0};
        if (!fw_render_key_slot(p, n, ks, closer, &kout, &stop, &colon, 0)) {
            free(kout.buf);
            return 0;
        }
        size_t after = stop;
        fout vout = {NULL, 0, 0, 0};
        if (colon) {
            /* the VALUE boundary: the slot after the colon's own run */
            size_t vs = fc_skip_blank_break(p, n, after + 1);
            int vcolon = 0;
            if (!fw_render_key_slot(p, n, vs, closer, &vout, &stop, &vcolon, 1) || vcolon) {
                free(kout.buf);
                free(vout.buf);
                return 0; /* a nested boundary in the value slot:
                           * unmeasured */
            }
            after = stop;
        }
        if (after >= n || (p[after] != ',' && p[after] != closer)) {
            free(kout.buf);
            free(vout.buf);
            return 0; /* the terminator gate */
        }
        *term_at = after;
        if (!is_map) {
            fo_str(o, "+MAP\n");
            fo_str(o, "=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n=VAL : ");
            fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
            fo_str(o, "\n-MAP\n");
        } else {
            fo_str(o, "=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n=VAL : ");
            fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
            fo_str(o, "\n");
        }
        free(kout.buf);
        free(vout.buf);
        return 1;
    }
    if (c == '?') {
        /* the glued-`?` member (head/plain/indicator): the `?` rides as
         * the member's first content byte; the indicator form (`?` then
         * `,`/closer) renders the empty pair */
        if (i + 1 >= n || p[i + 1] == ',' || p[i + 1] == closer) {
            *term_at = i + 1;
            if (!is_map) {
                fo_str(o, "+MAP\n=VAL : \n=VAL : \n-MAP\n");
            } else {
                fo_str(o, "=VAL : \n=VAL : \n");
            }
            return 1;
        }
        fout kout = {NULL, 0, 0, 0};
        if (!fw_render_content(p, n, i, closer, &kout, &stop, &colon)) {
            free(kout.buf);
            return 0;
        }
        fout vout = {NULL, 0, 0, 0};
        if (colon) {
            size_t vs = fc_skip_blank_break(p, n, stop + 1);
            int vcolon = 0;
            if (!fw_render_key_slot(p, n, vs, closer, &vout, &stop, &vcolon, 1) || vcolon) {
                free(kout.buf);
                free(vout.buf);
                return 0;
            }
        }
        if (stop >= n || (p[stop] != ',' && p[stop] != closer)) {
            free(kout.buf);
            free(vout.buf);
            return 0;
        }
        *term_at = stop;
        if (colon && !is_map) {
            /* the SEQ single-pair wrapper (the r272 floor's law) */
            fo_str(o, "+MAP\n=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n=VAL : ");
            fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
            fo_str(o, "\n-MAP\n");
        } else if (colon && is_map) {
            /* the r390 MEASURED MAP-root render (the artifact's `?*2:`
             * member, probed `{?*2:\n,s}` — the flat pair INSIDE the
             * root map, no wrapper: the root map itself is the pair;
             * the wrapper fires only on the SEQ root, where yeptris
             * wraps the implicit single pair — probed `[?*2:\n,s]`
             * renders `+MAP/=VAL : ?*2/=VAL : /-MAP`) */
            fo_str(o, "=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n=VAL : ");
            fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
            fo_str(o, "\n");
        } else if (is_map) {
            fo_str(o, "=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n=VAL : \n");
        } else {
            fo_str(o, "=VAL : ");
            fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
            fo_str(o, "\n");
        }
        free(kout.buf);
        free(vout.buf);
        return 1;
    }
    if (c == '&') {
        /* the bare ANCHORED-EMPTY member (the measured `&m,` ->
         * `=VAL : &m `: the prop render, the name cut at the
         * terminator). The r390 EXTENSION (the post-r389 nightly's run
         * 45643869326, issue #1671 — the family's 7th re-derivation and
         * the THIRD post-waiver one; the r389 paste-member extension
         * HELD its contract, the fresh member correctly declined): the
         * name rides the GLUED-CHARSET cut — the engine's prop walk
         * (yep_scan_prop_char) keeps `:` `~` `(` inside ONE name (the
         * r318 suite law, 2SXE/W5VH; libyaml is behind the suite, the
         * r311 law), probed this round at the bare-anchor MEMBER slot
         * (`&m:~y,` / `&m:(,` / `&m:x,` / `&a:b1,` all render
         * `=VAL : &<name> ` + the parser's inserted empty on yeptris
         * while libyaml splits at the first colon into the
         * anchored-empty key `&<n> ` + the plain value). Measured
         * faces, never guessed: the admitted name alphabet is exactly
         * the probed charset (fc_alpha ∪ {`:`, `~`, `(`}); the name
         * must NOT end at a riding `:` (the engine's un-consume law —
         * a name-final colon's render depends on the FOLLOWER
         * (collection/block-scalar/props run un-consume it, the
         * engine.c follower scan) — a different composition, stays
         * out; probed `{&m:\r,s}` rides `m:` — its own face, declined
         * here); after the name either the terminator sits directly
         * (the r318 `{&m:(}` face) or a blank/break run rides to it
         * (the r315 ride, re-measured this round on the glued-charset
         * name: `{&m:~y\r,}` and `{&m:~y\r  ,}` render identically) —
         * any other byte (`#`, quotes, a props-led second anchor —
         * probed `{&m:~y\r\r&s:~t,}` YEPTRIS-ONLY-ERRORS on the
         * duplicate-anchor law) declines. The render: `=VAL : &<name> `
         * + the inserted empty (the MAP root's pair) or the single
         * line (the SEQ root — probed `[&m:~y\r,s]` renders
         * `=VAL : &m:~y ` unwrapped). */
        size_t ae = i + 1;
        while (ae < n && (fc_alpha(p[ae]) || p[ae] == ':' || p[ae] == '~' || p[ae] == '(')) {
            ae++;
        }
        if (ae == i + 1 || p[ae - 1] == ':') {
            return 0; /* an empty name, or the colon-end face: the
                       * un-consume law's follower-dependent territory */
        }
        size_t te = ae;
        if (te >= n || (p[te] != ',' && p[te] != closer)) {
            te = fc_skip_blank_break(p, n, ae);
            if (te >= n || (p[te] != ',' && p[te] != closer)) {
                return 0;
            }
        }
        *term_at = te;
        if (is_map) {
            fo_str(o, "=VAL : &");
            fo_bytes(o, p + i + 1, ae - i - 1);
            fo_str(o, " \n=VAL : \n");
        } else {
            fo_str(o, "=VAL : &");
            fo_bytes(o, p + i + 1, ae - i - 1);
            fo_str(o, " \n");
        }
        return 1;
    }
    /* the plain/tag-soup member (the `11`, `2002:binary`,
     * `1.0e57.tag:y|` class); a member-start `!`/`*`/`:`/`"` is
     * unmeasured (the artifacts carry them only inside the fold-joined
     * plains or behind the split-`?` slots) */
    if (c == '!' || c == '*' || c == ':' || c == '"' || fw_decline_byte(c)) {
        return 0;
    }
    fout kout = {NULL, 0, 0, 0};
    if (!fw_render_content(p, n, i, closer, &kout, &stop, &colon)) {
        free(kout.buf);
        return 0;
    }
    fout vout = {NULL, 0, 0, 0};
    if (colon) {
        size_t vs = fc_skip_blank_break(p, n, stop + 1);
        int vcolon = 0;
        if (!fw_render_key_slot(p, n, vs, closer, &vout, &stop, &vcolon, 1) || vcolon) {
            free(kout.buf);
            free(vout.buf);
            return 0;
        }
    }
    if (stop >= n || (p[stop] != ',' && p[stop] != closer)) {
        free(kout.buf);
        free(vout.buf);
        return 0;
    }
    *term_at = stop;
    if (colon && !is_map) {
        /* the SEQ single-pair wrapper (the plain-led key:value member:
         * the measured `2002:binaryg:\r\r1.0e17.tag` pair) */
        fo_str(o, "+MAP\n=VAL : ");
        fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
        fo_str(o, "\n=VAL : ");
        fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
        fo_str(o, "\n-MAP\n");
    } else if (!is_map) {
        fo_str(o, "=VAL : ");
        fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
        fo_str(o, "\n");
    } else if (colon) {
        fo_str(o, "=VAL : ");
        fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
        fo_str(o, "\n=VAL : ");
        fo_bytes(o, vout.buf ? vout.buf : "", vout.len);
        fo_str(o, "\n");
    } else {
        /* the MAP root's plain member: the key + the parser's empty
         * value (the measured `?ml?\t ` -> `?ml?` + empty) */
        fo_str(o, "=VAL : ");
        fo_bytes(o, kout.buf ? kout.buf : "", kout.len);
        fo_str(o, "\n=VAL : \n");
    }
    free(kout.buf);
    free(vout.buf);
    return 1;
}

/* The dump shape walk (both sides): only the family's line vocabulary,
 * the root wrapper (open once, close once at depth 0, then -STR ends),
 * nested +MAP/-MAP single-pair wrappers balanced; nested sequences and
 * every other form decline. */
static int fw_shape_ok(const char* dump, size_t len, int root_map) {
    size_t i = 0;
    int depth = 0;
    if (len < 15 || memcmp(dump, "+STR\n", 5) != 0) {
        return 0;
    }
    i = 5;
    const char* root_open = root_map ? "+MAP\n" : "+SEQ\n";
    const char* root_close = root_map ? "-MAP\n" : "-SEQ\n";
    size_t rl = strlen(root_open);
    if (i + rl > len || memcmp(dump + i, root_open, rl) != 0) {
        return 0;
    }
    i += rl;
    while (i < len) {
        const char* nl = memchr(dump + i, '\n', len - i);
        if (nl == NULL) {
            return 0; /* every line ends with \n (-STR\n last) */
        }
        size_t ll = (size_t)(nl - (dump + i)) + 1;
        const char* ln = dump + i;
        if (ll == rl && memcmp(ln, root_close, rl) == 0 && depth == 0) {
            /* the root close: only -STR may follow */
            i += ll;
            if (i + 5 == len && memcmp(dump + i, "-STR\n", 5) == 0) {
                return 1;
            }
            return 0;
        }
        if (ll == 5 && memcmp(ln, "+MAP\n", 5) == 0) {
            depth++;
        } else if (ll == 5 && memcmp(ln, "-MAP\n", 5) == 0) {
            if (depth == 0) {
                return 0;
            }
            depth--;
        } else if (ll == 5 && (memcmp(ln, "+SEQ\n", 5) == 0 || memcmp(ln, "-SEQ\n", 5) == 0)) {
            return 0; /* a nested sequence: unmeasured */
        } else if (ll >= 7 && memcmp(ln, "=VAL : ", 7) == 0) {
            /* the value/key line (content rides) */
        } else if (ll >= 6 && memcmp(ln, "=ALI *", 6) == 0) {
            /* the alias render */
        } else {
            return 0; /* an unknown line form */
        }
        i += ll;
    }
    return 0;
}

/* The head-split fingerprint: yeptris holds flat `?`-LED content value
 * lines the libyaml stream lacks entirely (measured 558/685 vs 0 on the
 * r386 artifacts). The r390 widening: from the `?#` hash-run heads to
 * ANY `?`-led line — the flow-`?` KEY-token law consumes/splits the `?`
 * at EVERY member-start spelling (the law-a comment-eat, the law-d
 * split, the r339 alias ride — the r390 artifact's `?*2` member renders
 * `=ALI *2` on libyaml), so libyaml NEVER keeps a flat `?`-led VAL line
 * on a family-shaped input. A quoted `"?x"` span renders `?x` on BOTH
 * engines and still fails the gate (lq > 0 declines — conservative). */
static int fw_head_fingerprint(const char* ydump, const char* ldump) {
    size_t yq = 0;
    size_t lq = 0;
    for (const char* s = ydump; (s = strstr(s, "=VAL : ?")) != NULL; s += 1) {
        yq++;
    }
    for (const char* s = ldump; (s = strstr(s, "=VAL : ?")) != NULL; s += 1) {
        lq++;
    }
    return yq > 0 && lq == 0;
}

/* The fold-face canonical form: every fold escape (`\n`) and every raw
 * exotic break byte (NEL/LS/PS) collapses to ONE token, runs of tokens
 * collapse to a single mark — so yeptris's `\n\n\n50` (the 1.2 fold
 * law) and libyaml's `\n\n<PS>50` (the raw-PS law, its fold having
 * eaten the blank) canonicalize to the same string. Everything else
 * rides verbatim. Returns 0 when the content carries no fold face
 * (nothing to canonicalize — the line must match verbatim). */
static int fw_canon_line(const char* s, size_t n, char* out, size_t out_cap) {
    size_t o = 0;
    int folds = 0;
    int any = 0;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n && s[i + 1] == 'n') {
            any = 1;
            folds++;
            i++;
            continue;
        }
        if ((unsigned char)s[i] == 0xE2 && i + 2 < n && (unsigned char)s[i + 1] == 0x80 &&
            ((unsigned char)s[i + 2] == 0xA8 || (unsigned char)s[i + 2] == 0xA9)) {
            any = 1;
            folds++;
            i += 2;
            continue;
        }
        if ((unsigned char)s[i] == 0xC2 && i + 1 < n && (unsigned char)s[i + 1] == 0x85) {
            any = 1;
            folds++;
            i++;
            continue;
        }
        if (folds > 0) {
            if (o + 2 >= out_cap) {
                return 0;
            }
            out[o++] = '@';
            folds = 0;
        }
        if (o + 1 >= out_cap) {
            return 0;
        }
        out[o++] = s[i];
    }
    if (folds > 0) {
        if (o + 2 >= out_cap) {
            return 0;
        }
        out[o++] = '@';
    }
    out[o] = '\0';
    return any;
}

/* The r390 GLUED-CHARSET SPLIT products (the r318 law riding the
 * waiver's accounting): a yeptris-side prop render `&<n>:<s> ` (ONE
 * name, the colon glued to content, never at the name's end) maps INTO
 * the libyaml pair `&<n> ` (the anchored-empty key render, its
 * trailing space) + `<s>` (the plain value) — libyaml's IS_ALPHA
 * anchor cut lands on the FIRST glued colon (probed this round at the
 * bare-anchor member slot: `&m:~y,` / `&m:(,` / `&m:x,` / `&a:b1,`).
 * pfx/sfx receive the two NUL-terminated products; returns 0 when the
 * form is not a split source (no colon, a colon-final name — the
 * un-consume law's follower-dependent territory, declined at the
 * member walk too — or a non-prop render). */
static int fw_split_products(const char* yf, char* pfx, size_t cap1, char* sfx, size_t cap2) {
    size_t yl = strlen(yf);
    if (yl < 5 || yf[0] != '&' || yf[yl - 1] != ' ') {
        return 0;
    }
    size_t colon = 0;
    int found = 0;
    for (size_t k = 1; k + 1 < yl; k++) {
        if (yf[k] == ':') {
            colon = k;
            found = 1;
            break;
        }
    }
    if (!found || colon + 1 > yl - 2) {
        return 0; /* no colon, or the colon at the name's end */
    }
    if (colon + 2 > cap1 || yl - 2 - colon + 1 > cap2) {
        return 0;
    }
    memcpy(pfx, yf, colon); /* '&' + the name prefix */
    pfx[colon] = ' ';
    pfx[colon + 1] = '\0';
    memcpy(sfx, yf + colon + 1, yl - 2 - colon); /* the value suffix */
    sfx[yl - 2 - colon] = '\0';
    return 1;
}

/* The r390 ALIAS-CUT probe source: a libyaml-side `=ALI *<name>` line
 * maps INTO the yeptris flat plain `?*<name>` (the flow-`?` KEY token
 * fires, libyaml's IS_ALPHA alias scan then reads the glued `*` — the
 * law-d/r199/r339 face; the r390 artifact's `?*2` member renders
 * `=ALI *2`). The name rides only when fully fc_alpha — a non-alpha
 * byte would clip libyaml's cut mid-name (unmeasured: no source). */
static int fw_alias_source(const char* yf, const char* probe, size_t pl) {
    size_t yl = strlen(yf);
    if (yl != pl + 2 || yf[0] != '?' || yf[1] != '*') {
        return 0;
    }
    if (memcmp(yf + 2, probe, pl) != 0) {
        return 0;
    }
    for (size_t k = 2; k < yl; k++) {
        if (!fc_alpha((unsigned char)yf[k])) {
            return 0;
        }
    }
    return 1;
}

/* The libyaml-side content gate (multiset-exact): the documented split
 * moves fold faces (the canon unifies), drops the `?` lead, eats the
 * `?#`-led heads to the line break — and NOTHING else. So the libyaml
 * side's canonical forms must map into the yeptris side's WITHOUT ever
 * exceeding the yeptris-side multiplicity, and every yeptris-side form
 * the libyaml side lacks must be a law-a head (the eaten member). A
 * single-byte tamper of either dump breaks the mapping. */
static int fw_content_set(const char* ydump, const char* ldump) {
    /* All scratch is heap and freed before return (the purity law).
     *
     * The multiset-exact accounting: the documented split maps the
     * yeptris-side value forms onto the libyaml side through exactly
     * three transforms — the verbatim ride, the `?`-lead drop (the
     * law-a KEY-token eat, `?ml?` -> `ml?`), and the line-break eat of
     * a `?#`-led head (`?## &m` -> `&m`, `?##|#|+@yaml.org` ->
     * `yaml.org`). Each libyaml probe form may therefore draw at most
     * ycount(direct) + ycount('?' + probe) + sum(ycount(head sources))
     * lines; anything more (or a probe with no source at all) is an
     * unexplained libyaml-side line — a single-byte tamper of the
     * libyaml dump cannot survive it. */
    const size_t max_forms = 24576;
    const size_t store_cap = 2u * 1024u * 1024u;
    char** yforms = malloc(max_forms * sizeof(char*));
    unsigned* ycnt = malloc(max_forms * sizeof(unsigned));
    char** praw = malloc(max_forms * sizeof(char*)); /* the probe forms (raw) */
    unsigned* pcount = malloc(max_forms * sizeof(unsigned));
    unsigned* pallowed = malloc(max_forms * sizeof(unsigned));
    char* canon = malloc(8192);
    char* withq = malloc(8194);
    char* store = malloc(store_cap);  /* the y raw contents */
    char* fstore = malloc(store_cap); /* the y canonical forms */
    char* pstore = malloc(store_cap); /* the probe forms */
    char* pfxbuf = malloc(store_cap); /* the split-product scratch */
    char* sfxbuf = malloc(store_cap);
    int result = 0;
    if (yforms == NULL || ycnt == NULL || praw == NULL || pcount == NULL || pallowed == NULL ||
        canon == NULL || withq == NULL || store == NULL || fstore == NULL || pstore == NULL ||
        pfxbuf == NULL || sfxbuf == NULL) {
        goto done;
    }
    size_t store_at = 0;
    size_t fstore_at = 0;
    size_t pstore_at = 0;
    size_t yn = 0;
    size_t pn = 0;
    const char* p = ydump;
    while (p != NULL && *p != '\0') {
        const char* nl = strchr(p, '\n');
        if (nl == NULL) {
            break;
        }
        size_t ll = (size_t)(nl - p);
        if (ll >= 7 && memcmp(p, "=VAL : ", 7) == 0) {
            const char* content = p + 7;
            size_t cl = ll - 7;
            const char* form;
            size_t fl;
            if (fw_canon_line(content, cl, canon, 8192)) {
                form = canon;
                fl = strlen(canon);
            } else {
                form = content;
                fl = cl;
            }
            size_t k;
            for (k = 0; k < yn; k++) {
                if (strlen(yforms[k]) == fl && memcmp(yforms[k], form, fl) == 0) {
                    ycnt[k]++;
                    break;
                }
            }
            if (k == yn && yn < max_forms && store_at + cl + 1 <= store_cap &&
                fstore_at + fl + 1 <= store_cap) {
                memcpy(store + store_at, content, cl);
                store[store_at + cl] = '\0';
                memcpy(fstore + fstore_at, form, fl);
                fstore[fstore_at + fl] = '\0';
                yforms[yn] = fstore + fstore_at;
                ycnt[yn] = 1;
                store_at += cl + 1;
                fstore_at += fl + 1;
                yn++;
            }
        }
        p = nl + 1;
    }
    /* the allowance buckets: direct + the `?`-lead-drop sources + the
     * `?#`-head line-break-eat sources */
    for (size_t k = 0; k < yn; k++) {
        pallowed[k] = ycnt[k]; /* the direct bucket */
    }
    for (size_t k = 0; k < yn; k++) {
        const char* yf = yforms[k];
        size_t yl = strlen(yf);
        if (yf[0] == '?' && yl > 1 && yf[1] != '#') {
            /* the lead-drop product: yf minus its leading `?` */
            const char* prod = yf + 1;
            size_t pl = yl - 1;
            for (size_t j = 0; j < yn; j++) {
                if (j != k && strlen(yforms[j]) == pl && memcmp(yforms[j], prod, pl) == 0) {
                    pallowed[j] += ycnt[k];
                    break;
                }
            }
        }
        if (yf[0] == '>') {
            /* the `>`-lead drop (the r258 law: libyaml's flow dispatch
             * eats the reserved lead; libfyaml errors the member) */
            const char* prod = yf + 1;
            if (yl > 1 && prod[0] == ' ') {
                prod++;
            }
            size_t pl = strlen(prod);
            for (size_t j = 0; j < yn; j++) {
                if (j != k && strlen(yforms[j]) == pl && memcmp(yforms[j], prod, pl) == 0) {
                    pallowed[j] += ycnt[k];
                    break;
                }
            }
        }
        if (yf[0] == '?' && yf[1] == '#') {
            const char* fold = strchr(yf, '@');
            const char* sp = strchr(yf, ' ');
            if (sp != NULL && (fold == NULL || sp < fold)) {
                fold = sp;
            }
            if (fold != NULL) {
                const char* prod = fold + 1;
                size_t pl = strlen(prod);
                for (size_t j = 0; j < yn; j++) {
                    if (strlen(yforms[j]) == pl && memcmp(yforms[j], prod, pl) == 0) {
                        pallowed[j] += ycnt[k];
                        break;
                    }
                    /* the prop-render twin: the post-eat anchor rides
                     * libyaml's prop render with its trailing space
                     * (`?## &m` -> `&m `) */
                    if (prod[0] == '&' || prod[0] == '*') {
                        char withs[8192];
                        if (pl + 1 < sizeof(withs)) {
                            memcpy(withs, prod, pl);
                            withs[pl] = ' ';
                            withs[pl + 1] = '\0';
                            if (strlen(yforms[j]) == pl + 1 &&
                                memcmp(yforms[j], withs, pl + 1) == 0) {
                                pallowed[j] += ycnt[k];
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    /* count the libyaml probe forms against the buckets */
    const char* q = ldump;
    while (q != NULL && *q != '\0') {
        const char* nl = strchr(q, '\n');
        if (nl == NULL) {
            break;
        }
        size_t ll = (size_t)(nl - q);
        if (ll >= 7 && memcmp(q, "=VAL : ", 7) == 0) {
            const char* content = q + 7;
            size_t cl = ll - 7;
            const char* probe;
            size_t probe_len;
            if (fw_canon_line(content, cl, canon, 8192)) {
                probe = canon;
                probe_len = strlen(canon);
            } else {
                probe = content;
                probe_len = cl;
            }
            /* the probe's bucket: the DIRECT form match only — a probe
             * with no direct bucket has no source (the transforms map
             * INTO existing yeptris forms, never create new buckets) */
            size_t k;
            for (k = 0; k < pn; k++) {
                if (strlen(praw[k]) == probe_len && memcmp(praw[k], probe, probe_len) == 0) {
                    pcount[k]++;
                    break;
                }
            }
            if (k == pn) {
                int has_direct = 0;
                for (size_t j = 0; j < yn; j++) {
                    if (strlen(yforms[j]) == probe_len &&
                        memcmp(yforms[j], probe, probe_len) == 0) {
                        has_direct = 1;
                        break;
                    }
                }
                if (!has_direct) {
                    /* the transform sources: `?`+probe or a `?#` head's
                     * after-fold suffix — the probe rides an EXISTING
                     * form's allowance only when such a source exists */
                    int has_source = 0;
                    if (probe_len + 1 < 8194) {
                        withq[0] = '?';
                        memcpy(withq + 1, probe, probe_len + 1);
                        for (size_t j = 0; j < yn; j++) {
                            if (strlen(yforms[j]) == probe_len + 1 &&
                                memcmp(yforms[j], withq, probe_len + 1) == 0) {
                                has_source = 1;
                                break;
                            }
                        }
                    }
                    /* the r390 GLUED-CHARSET SPLIT source: the probe is
                     * the `&<n> ` key render or the `<s>` value plain of
                     * a yeptris prop render `&<n>:<s> ` (the r318 law's
                     * split at the IS_ALPHA anchor cut). VAL probes
                     * only — the alias-cut face is its own line kind
                     * (`=ALI *`), bucketed separately below. */
                    if (!has_source) {
                        for (size_t j = 0; j < yn; j++) {
                            if (fw_split_products(yforms[j], pfxbuf, store_cap, sfxbuf,
                                                  store_cap)) {
                                if ((strlen(pfxbuf) == probe_len &&
                                     memcmp(pfxbuf, probe, probe_len) == 0) ||
                                    (strlen(sfxbuf) == probe_len &&
                                     memcmp(sfxbuf, probe, probe_len) == 0)) {
                                    has_source = 1;
                                    break;
                                }
                            }
                        }
                    }
                    /* the r389 prop-render twin at the GLUED-`?` slot
                     * (measured `{?&m,}`: yeptris rides the flat plain
                     * `?&m`, libyaml's flow-`?` KEY split renders the
                     * anchor prop `&m ` with its trailing space) — the
                     * space-stripped probe prefixed `?` maps INTO an
                     * existing yeptris form. Anchors only: the alias
                     * twin renders `=ALI *<name>` (a non-VAL line the
                     * content-set never counts — nothing to map). */
                    if (!has_source && probe_len >= 2 && probe[0] == '&' &&
                        probe[probe_len - 1] == ' ' && probe_len + 1 < 8194) {
                        withq[0] = '?';
                        memcpy(withq + 1, probe, probe_len - 1);
                        withq[probe_len] = '\0';
                        for (size_t j = 0; j < yn; j++) {
                            if (strlen(yforms[j]) == probe_len &&
                                memcmp(yforms[j], withq, probe_len) == 0) {
                                has_source = 1;
                                break;
                            }
                        }
                    }
                    if (!has_source) {
                        for (size_t j = 0; j < yn && !has_source; j++) {
                            const char* yf = yforms[j];
                            size_t yl = strlen(yf);
                            if (yl < 2 || yf[0] != '?' || yf[1] != '#') {
                                continue;
                            }
                            const char* fold = strchr(yf, '@');
                            const char* sp = strchr(yf, ' ');
                            if (sp != NULL && (fold == NULL || sp < fold)) {
                                fold = sp;
                            }
                            if (fold == NULL) {
                                continue;
                            }
                            size_t rest = yl - (size_t)(fold - yf) - 1;
                            if (rest == probe_len && memcmp(fold + 1, probe, probe_len) == 0) {
                                has_source = 1;
                            }
                            /* the prop-render twin at ADMISSION (the r388
                             * extension): the eaten head's post-break
                             * anchor rides libyaml's prop render with its
                             * trailing space (`?## &mq` -> `&mq ` — the
                             * r388 artifact's fresh anchor-name spelling,
                             * run 45610560196). The allowance loop (b)
                             * already carried this transform; the
                             * admission check did not — a head whose
                             * rebind name appeared NOWHERE else in the
                             * yeptris stream (no bare `&mq,` member) had
                             * no direct bucket and declined. The
                             * firewall is unchanged: the twin still maps
                             * INTO an existing yeptris-side form (the
                             * byte-exact derivation gates the input
                             * first), and the allowance counting below
                             * is untouched. */
                            if (!has_source && probe_len > 0 && rest + 1 == probe_len &&
                                (probe[0] == '&' || probe[0] == '*') && probe[rest] == ' ' &&
                                memcmp(fold + 1, probe, rest) == 0) {
                                has_source = 1;
                            }
                        }
                    }
                    if (!has_source) {
                        if (getenv("YD_FW_TRACE") != NULL) {
                            fprintf(stderr, "content-set: l-line not in y-set: %.*s\n",
                                    (int)(probe_len > 90 ? 90 : probe_len), probe);
                        }
                        goto done;
                    }
                }
                if (pn < max_forms && pstore_at + probe_len + 1 <= store_cap) {
                    memcpy(pstore + pstore_at, probe, probe_len);
                    pstore[pstore_at + probe_len] = '\0';
                    praw[pn] = pstore + pstore_at;
                    pstore_at += probe_len + 1;
                    pcount[pn] = 1;
                    pn++;
                }
            }
        } else if (ll >= 6 && memcmp(q, "=ALI *", 6) == 0) {
            /* the r390 ALIAS-CUT probe: the libyaml `=ALI *<name>` line
             * (the flow-`?` KEY token fired, libyaml's IS_ALPHA alias
             * scan read the glued `*` — the law-d/r199/r339 face, the
             * r390 artifact's `?*2` member). Bucketed under the \x01
             * prefix — a byte that never appears in admitted dump
             * vocabulary — so an alias bucket can never collide with a
             * `=VAL : *...` plain-content probe (the event kinds
             * differ; a VAL probe must not ride the alias source). The
             * ONLY source: the flat yeptris plain `?*<name>` (fully
             * fc_alpha — a non-alpha byte clips libyaml's cut,
             * unmeasured: no source). */
            const char* content = q + 6;
            size_t cl = ll - 6;
            if (cl == 0) {
                goto done; /* a bare `=ALI *` line: no measured source */
            }
            size_t k;
            for (k = 0; k < pn; k++) {
                if (praw[k][0] == '\x01' && strlen(praw[k]) == cl + 1 &&
                    memcmp(praw[k] + 1, content, cl) == 0) {
                    pcount[k]++;
                    break;
                }
            }
            if (k == pn) {
                int has_source = 0;
                for (size_t j = 0; j < yn; j++) {
                    if (fw_alias_source(yforms[j], content, cl)) {
                        has_source = 1;
                        break;
                    }
                }
                if (!has_source) {
                    if (getenv("YD_FW_TRACE") != NULL) {
                        fprintf(stderr, "content-set: alias line not in y-set: %.*s\n",
                                (int)(cl > 90 ? 90 : cl), content);
                    }
                    goto done;
                }
                if (pn < max_forms && pstore_at + cl + 2 <= store_cap) {
                    pstore[pstore_at] = '\x01';
                    memcpy(pstore + pstore_at + 1, content, cl);
                    pstore[pstore_at + cl + 1] = '\0';
                    praw[pn] = pstore + pstore_at;
                    pstore_at += cl + 2;
                    pcount[pn] = 1;
                    pn++;
                }
            }
        }
        q = nl + 1;
    }
    /* the allowances: a probe's cap = ycount(direct) + the transform
     * sources' counts (recomputed per probe bucket) */
    for (size_t k = 0; k < pn; k++) {
        const char* pf = praw[k];
        size_t pl = strlen(pf);
        unsigned allowed = 0;
        if (pf[0] == '\x01') {
            /* the r390 ALIAS-CUT bucket: the cap draws ONLY the flat
             * `?*<name>` sources' counts (multiset-exact: a tampered
             * alias name loses its source and declines) */
            for (size_t j = 0; j < yn; j++) {
                if (fw_alias_source(yforms[j], pf + 1, pl - 1)) {
                    allowed += ycnt[j];
                }
            }
        } else {
            for (size_t j = 0; j < yn; j++) {
                if (strlen(yforms[j]) == pl && memcmp(yforms[j], pf, pl) == 0) {
                    allowed += ycnt[j];
                }
            }
            if (pl + 1 < 8194) {
                /* (a) the `?`-lead drop (the law-a KEY-token eat) */
                withq[0] = '?';
                memcpy(withq + 1, pf, pl + 1);
                for (size_t j = 0; j < yn; j++) {
                    if (strlen(yforms[j]) == pl + 1 && memcmp(yforms[j], withq, pl + 1) == 0) {
                        allowed += ycnt[j];
                    }
                }
            }
            if (pl >= 2 && pf[0] == '&' && pf[pl - 1] == ' ' && pl + 1 < 8194) {
                /* (a2) the r389 twin's ALLOWANCE: the glued-`?` slot's
                 * anchor-prop render (`?&m` -> `&m `) draws its source's
                 * count — the cap stays multiset-exact (a tampered extra
                 * `&m ` line still exceeds direct + twin) */
                withq[0] = '?';
                memcpy(withq + 1, pf, pl - 1);
                withq[pl] = '\0';
                for (size_t j = 0; j < yn; j++) {
                    if (strlen(yforms[j]) == pl && memcmp(yforms[j], withq, pl) == 0) {
                        allowed += ycnt[j];
                    }
                }
            }
            if (pl + 1 < 8194) {
                /* (c) the prop-render twin: the eaten head's post-break
                 * anchor rides libyaml's prop render with its trailing
                 * space (`?## &m` -> `&m `) */
                withq[0] = ' ';
                memcpy(withq + 1, pf, pl + 1);
                for (size_t j = 0; j < yn; j++) {
                    if (strlen(yforms[j]) == pl + 1 && memcmp(yforms[j], withq, pl + 1) == 0 &&
                        (yforms[j][0] == '&' || yforms[j][0] == '*')) {
                        allowed += ycnt[j];
                    }
                }
            }
            /* (e) the r390 GLUED-CHARSET SPLIT's allowance: the `&<n> `
             * key render and the `<s>` value plain each draw the source
             * prop render's count (multiset-exact: a tampered name,
             * suffix, or colon position loses the source and declines) */
            for (size_t j = 0; j < yn; j++) {
                if (fw_split_products(yforms[j], pfxbuf, store_cap, sfxbuf, store_cap)) {
                    size_t p1l = strlen(pfxbuf);
                    size_t p2l = strlen(sfxbuf);
                    if ((p1l == pl && memcmp(pfxbuf, pf, pl) == 0) ||
                        (p2l == pl && memcmp(sfxbuf, pf, pl) == 0)) {
                        allowed += ycnt[j];
                    }
                }
            }
        }
        for (size_t j = 0; j < yn; j++) {
            const char* yf = yforms[j];
            size_t yl = strlen(yf);
            /* (b) the line-break eat of a `?#`-led head */
            if (yl >= 2 && yf[0] == '?' && yf[1] == '#') {
                const char* fold = strchr(yf, '@');
                const char* sp = strchr(yf, ' ');
                if (sp != NULL && (fold == NULL || sp < fold)) {
                    fold = sp;
                }
                if (fold != NULL) {
                    size_t rest = yl - (size_t)(fold - yf) - 1;
                    if ((rest == pl && memcmp(fold + 1, pf, pl) == 0) ||
                        (pf[0] == '&' && rest + 1 == pl && pf[rest] == ' ' &&
                         memcmp(fold + 1, pf, rest) == 0)) {
                        allowed += ycnt[j];
                    }
                }
            }
            /* (d) the `>`-lead drop (the r258 law: libyaml's flow
             * dispatch eats the reserved lead) */
            if (yl >= 2 && yf[0] == '>') {
                if (yf[1] == ' ' && yl - 2 == pl && memcmp(yf + 2, pf, pl) == 0) {
                    allowed += ycnt[j];
                } else if (yl - 1 == pl && memcmp(yf + 1, pf, pl) == 0) {
                    allowed += ycnt[j];
                }
            }
        }
        if (pl > 0 && pcount[k] > allowed) {
            /* the EMPTY form is exempt: the split inserts the parser's
             * empty values asymmetrically (a content tamper still
             * breaks its own line's membership above) */
            if (getenv("YD_FW_TRACE") != NULL) {
                fprintf(stderr, "content-set: probe exceeds allowance: %.*s (%u > %u)\n",
                        (int)(pl > 70 ? 70 : pl), pf, pcount[k], allowed);
            }
            goto done;
        }
    }
    result = 1;
done:
    free(yforms);
    free(ycnt);
    free(praw);
    free(pcount);
    free(pallowed);
    free(canon);
    free(withq);
    free(store);
    free(fstore);
    free(pstore);
    free(pfxbuf);
    free(sfxbuf);
    return result;
}

int yd_family_waiver(const uint8_t* data, size_t size, const char* ydump, const char* ldump) {
    if (data == NULL || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (size < 4 || size > FC_MAX_INPUT) {
        return 0;
    }
    /* the byte gates: no NUL, no line-start BOM (the classifier's own
     * entry law) */
    int line_start = 1;
    for (size_t i = 0; i < size;) {
        unsigned char ch = data[i];
        if (ch == 0x00) {
            return 0;
        }
        if (ch == 0xEF && i + 2 < size && data[i + 1] == 0xBB && data[i + 2] == 0xBF) {
            if (line_start) {
                return 0;
            }
            i += 3;
            continue;
        }
        size_t bl = fc_break_len(data, size, i);
        if (bl > 0) {
            i += (size_t)bl;
            line_start = 1;
            continue;
        }
        line_start = 0;
        i++;
    }
    /* the bare flow root (no doc-marker/tag leads: their classes ride
     * the l2/l3 laws and the anchor-ride lead stays on the r238 arm) */
    size_t root = fc_skip_blank_break(data, size, 0);
    if (root >= size || (data[root] != '[' && data[root] != '{')) {
        return 0;
    }
    unsigned char closer = (data[root] == '[') ? ']' : '}';
    int is_map = data[root] == '{';

    /* the yeptris-side derivation */
    fout yd = {NULL, 0, 0, 0};
    fo_str(&yd, "+STR\n");
    fo_str(&yd, is_map ? "+MAP\n" : "+SEQ\n");
    size_t i = root + 1;
    unsigned members = 0;
    int ok = 1;
    for (;;) {
        i = fc_skip_blank_break(data, size, i);
        if (i >= size) {
            ok = 0; /* the closer never came */
            break;
        }
        if (data[i] == closer) {
            i++;
            break;
        }
        if (data[i] == ',') {
            i++;
            continue;
        }
        if (members >= FC_MAX_MEMBERS) {
            ok = 0;
            break;
        }
        members++;
        size_t term = 0;
        if (!fw_member(data, size, i, closer, is_map, &yd, &term)) {
            ok = 0;
            break;
        }
        if (yd.bad || yd.len > FC_MAX_OUT) {
            ok = 0;
            break;
        }
        i = term;
    }
    if (ok) {
        fo_str(&yd, is_map ? "-MAP\n" : "-SEQ\n");
        fo_str(&yd, "-STR\n");
    }
    int ymatch = ok && !yd.bad && yd.buf != NULL && strcmp(yd.buf, ydump) == 0;
    if (!ymatch && getenv("YD_FW_TRACE") != NULL) {
        fprintf(stderr, "waiver: ymatch=%d ok=%d\n--- derived:\n%s\n--- actual:\n%s\n", ymatch, ok,
                ok && yd.buf ? yd.buf : "(declined)", ydump);
    }
    free(yd.buf);
    if (!ymatch) {
        return 0; /* the yeptris side must derive byte-exact: any engine
                   * drift on a class-shaped input stays red */
    }
    /* the libyaml side: shape-valid + the head-split fingerprint */
    if (!fw_shape_ok(ldump, strlen(ldump), is_map)) {
        if (getenv("YD_FW_TRACE") != NULL) {
            fprintf(stderr, "waiver: l-shape declined\n");
        }
        return 0;
    }
    if (!fw_shape_ok(ydump, strlen(ydump), is_map)) {
        if (getenv("YD_FW_TRACE") != NULL) {
            fprintf(stderr, "waiver: y-shape declined\n");
        }
        return 0;
    }
    if (!fw_head_fingerprint(ydump, ldump)) {
        if (getenv("YD_FW_TRACE") != NULL) {
            fprintf(stderr, "waiver: fingerprint declined\n");
        }
        return 0; /* a fold-only (or other) divergence: not the
                   * documented head split */
    }
    if (!fw_content_set(ydump, ldump)) {
        return 0; /* a libyaml-side line the split cannot explain */
    }
    return 1;
}

/* The tamper battery's invariance domains (see family_classify.h). The
 * r389 drill: the floor's sampled dels hit the empty-bucket tolerance
 * (two single-byte contents) and the artifact's sampled flip hit the
 * PS<->LS canon face — both domains predate r389 (the r386/r388 seeds
 * missed them by sampling luck), and both are computed here
 * independently of the waiver's own decision. */
int yd_family_tamper_in_domain(const char* ydump, const char* ldump, size_t pos, int op) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    size_t len = strlen(ldump);
    if (pos >= len) {
        return 0;
    }
    if (op == 0) {
        /* (i) the flip: only the raw LS/PS third byte inside the E2 80
         * multibyte — the swap keeps the canon-fold face (the law-b
         * equating). Every other byte class flips OUT of its face (the
         * escape bytes, the NEL pair, plain content) and must decline. */
        unsigned char b = (unsigned char)ldump[pos];
        return (b == 0xA8 || b == 0xA9) && pos >= 2 && (unsigned char)ldump[pos - 2] == 0xE2 &&
               (unsigned char)ldump[pos - 1] == 0x80;
    }
    if (op != 1) {
        return 0; /* the cut (truncation) always breaks the shape */
    }
    /* (ii) the del: the line containing pos */
    size_t ls = 0;
    for (size_t i = pos; i > 0; i--) {
        if (ldump[i - 1] == '\n') {
            ls = i;
            break;
        }
    }
    if (len - ls < 7 || memcmp(ldump + ls, "=VAL : ", 7) != 0) {
        return 0; /* not a VAL line: a structural tamper must decline */
    }
    const char* nl = strchr(ldump + ls, '\n');
    if (nl == NULL) {
        return 0;
    }
    size_t content_len = (size_t)(nl - (ldump + ls)) - 7;
    /* the deleted byte must be the line's ONLY content byte — a del on
     * the prefix, the colon, the boundary space, or the line break
     * itself changes the line's shape/membership and must decline */
    if (content_len == 1 && pos == (size_t)(nl - ldump) - 1 && strstr(ydump, "=VAL : \n") != NULL) {
        return 1; /* the emptied form rides the split's asymmetric empty
                   * bucket — the r386 accounting's tolerance domain */
    }
    return 0;
}
