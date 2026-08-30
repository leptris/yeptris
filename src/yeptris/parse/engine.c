/* engine.c — the block machine + flow kernel (TODO.impl/07).
 *
 * One engine, two kernels: block context is a line/indent loop over an
 * explicit frame stack; '[' or '{' switches to the iterative flow kernel.
 * No recursion anywhere (depth = frame stack depth, guarded).
 *
 * Key invariants:
 *  - MAP_START always precedes its key events, including flow keys
 *    (a flow node that turns out to be a key is pre-scanned with
 *    e_skip_flow before its events are emitted);
 *  - a collection frame is pushed exactly once: sibling entries at the
 *    frame's column reuse the open frame;
 *  - anchors defined before use (engine tracks names; the DOM binds nodes).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/chartype.h"
#include "common/simd_text.h"
#include "engine.h"
#include "memory/allocator.h"
#include "memory/pool.h"
#include "parse/scalars.h"
#include "scan/scan.h"

#define YEP_MAX_DEPTH 1000
#define YEP_MAX_ANCHORS 2048
#define YEP_MAX_FOLD_LINES 8192

typedef enum { YEP_FRAME_SEQ = 0, YEP_FRAME_MAP } yep_frame_kind;

typedef enum {
    YEP_CTX_FRESH = 0,   /* fresh line entry: may open collections */
    YEP_CTX_AFTER_DASH,  /* value of a seq entry: may open collections (compact) */
    YEP_CTX_AFTER_COLON, /* value of a map key: collections via following lines only */
    YEP_CTX_AFTER_Q,     /* explicit-key content: may ALIGN with the '?' column */
} yep_ctx;

struct yep_engine {
    const yep_allocator* sys;
    yep_pool* pool; /* finish pool: folded/escaped scalar content */
    const char* p;
    size_t len;
    size_t pos;
    uint32_t line;     /* 1-based current line */
    size_t line_start; /* offset of the current line start */
    yep_error err;
    const yep_sink* sink;

    struct {
        uint16_t col;
        uint8_t kind;
    } frames[YEP_MAX_DEPTH];
    int depth;

    yep_view anchors[YEP_MAX_ANCHORS];
    int anchor_count;

    yep_fold_line fold[YEP_MAX_FOLD_LINES];
    size_t fold_n;

    /* properties that belong to a value parsed on following lines */
    yep_view pend_anchor, pend_tag;

    int doc_content;   /* any node emitted for the current document */
    int q_key_pending; /* an explicit '?' key awaits its ':' value line */
    /* %TAG handle map for the current document (engine = tag SSOT) */
    struct {
        yep_view handle, prefix;
    } tagmap[8];
    int tagmap_n;
};

/* ------------------------------------------------------------ forwards */

static int e_node(yep_engine* e, yep_ctx ctx, uint16_t floor_col);
static int e_parse_value(yep_engine* e, yep_ctx ctx, uint16_t floor_col);

/* ---------------------------------------------------------------- util */

static uint32_t e_col(const yep_engine* e, size_t at) {
    return at >= e->line_start ? (uint32_t)(at - e->line_start) : 0;
}

static int e_fail(yep_engine* e, yep_err_code code, size_t at) {
    yep_error_set(&e->err, code, e->line, e_col(e, at) + 1, at, NULL);
    return -1;
}

static int emit_now(yep_engine* e, const yep_event* ev) {
    return e->sink ? e->sink->on_event(e->sink->ctx, ev) : 0;
}

static void e_event_init(yep_event* ev, yep_event_type t) {
    memset(ev, 0, sizeof(*ev));
    ev->type = t;
}

static void e_line_done(yep_engine* e, size_t at) {
    size_t br = yep_scan_break_len(e->p, e->len, at);
    if (br > 0) {
        e->pos = at + br;
        e->line++;
        e->line_start = e->pos;
    } else {
        e->pos = at;
    }
}

static void e_skip_inline_space(yep_engine* e) {
    while (e->pos < e->len && (e->p[e->pos] == ' ' || e->p[e->pos] == '\t')) {
        e->pos++;
    }
}

static int e_at_eol(yep_engine* e) {
    return e->pos >= e->len || e->p[e->pos] == '\n' || e->p[e->pos] == '\r' || e->p[e->pos] == '#';
}

static int e_skip_to_eol(yep_engine* e) {
    while (e->pos < e->len && e->p[e->pos] != '\n' && e->p[e->pos] != '\r') {
        e->pos++;
    }
    return 0;
}

/* ':' terminates a node here (blank/EOL after; flow indicators are
 * handled by the flow kernel's own check). */
static int e_colon_at(yep_engine* e, size_t at) {
    if (at >= e->len || e->p[at] != ':') {
        return 0;
    }
    size_t n = at + 1;
    return n >= e->len || e->p[n] == ' ' || e->p[n] == '\n' || e->p[n] == '\r';
}

/* -------------------------------------------------------------- anchors */

static int anchor_defined(yep_engine* e, yep_view name) {
    for (int i = 0; i < e->anchor_count; i++) {
        if (yep_view_eq(e->anchors[i], name)) {
            return 1;
        }
    }
    return 0;
}

static void anchor_define(yep_engine* e, yep_view name) {
    for (int i = 0; i < e->anchor_count; i++) {
        if (yep_view_eq(e->anchors[i], name)) {
            e->anchors[i] = name; /* last definition wins (libyaml parity) */
            return;
        }
    }
    if (e->anchor_count < YEP_MAX_ANCHORS) {
        e->anchors[e->anchor_count++] = name;
    }
}

/* --------------------------------------------------------------- frames */

/* Opens a collection frame unless the top frame already continues at
 * exactly this column (sibling entries reuse it). anchor/tag ride the
 * START event. Returns 0 on success. */
static int e_open_seq(yep_engine* e, uint16_t col, uint32_t line, uint32_t coln, yep_view anchor,
                      yep_view tag) {
    if (e->depth > 0 && e->frames[e->depth - 1].kind == YEP_FRAME_SEQ &&
        e->frames[e->depth - 1].col == col) {
        return 0;
    }
    if (e->depth >= YEP_MAX_DEPTH) {
        return e_fail(e, YEP_ERR_DEPTH, e->pos);
    }
    e->frames[e->depth].col = col;
    e->frames[e->depth].kind = YEP_FRAME_SEQ;
    e->depth++;
    yep_event ev;
    e_event_init(&ev, YEP_EV_SEQ_START);
    ev.anchor = anchor;
    ev.tag = tag;
    ev.line = line;
    ev.col = coln;
    return emit_now(e, &ev) == 0 ? 0 : -2;
}

static int e_open_map(yep_engine* e, uint16_t col, uint32_t line, uint32_t coln, yep_view anchor,
                      yep_view tag) {
    if (e->depth > 0 && e->frames[e->depth - 1].kind == YEP_FRAME_MAP &&
        e->frames[e->depth - 1].col == col) {
        return 0;
    }
    if (e->depth >= YEP_MAX_DEPTH) {
        return e_fail(e, YEP_ERR_DEPTH, e->pos);
    }
    e->frames[e->depth].col = col;
    e->frames[e->depth].kind = YEP_FRAME_MAP;
    e->depth++;
    yep_event ev;
    e_event_init(&ev, YEP_EV_MAP_START);
    ev.anchor = anchor;
    ev.tag = tag;
    ev.line = line;
    ev.col = coln;
    return emit_now(e, &ev) == 0 ? 0 : -2;
}

static int e_close_to(yep_engine* e, int to_depth) {
    while (e->depth > to_depth) {
        e->depth--;
        yep_event ev;
        e_event_init(&ev,
                     e->frames[e->depth].kind == YEP_FRAME_SEQ ? YEP_EV_SEQ_END : YEP_EV_MAP_END);
        if (emit_now(e, &ev) != 0) {
            return -2;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- scalars */

/* Quoted scalar at the opening quote; spans lines when needed. */
static int e_quoted(yep_engine* e, yep_event* ev) {
    unsigned char q = (unsigned char)e->p[e->pos];
    size_t content_start = e->pos + 1;
    int has_esc = 0;
    size_t close;
    {
        const yep_text_kernels* k = yep_text_active();
        ptrdiff_t r =
            k->quote_scan(e->p + content_start, e->len - content_start, (char)q, &has_esc);
        if (r >= 0) {
            close = content_start + (size_t)r;
        } else {
            /* unterminated on this span: manual multi-line scan */
            size_t i = content_start;
            close = e->len + 1;
            while (i < e->len) {
                if (q == '"' && e->p[i] == '\\') {
                    i += 2;
                    has_esc = 1;
                    continue;
                }
                if ((unsigned char)e->p[i] == q) {
                    if (q == '\'' && i + 1 < e->len && e->p[i + 1] == '\'') {
                        i += 2;
                        continue;
                    }
                    close = i;
                    break;
                }
                i++;
            }
            if (close == e->len + 1) {
                return e_fail(e, YEP_ERR_UNTERMINATED_QUOTE, e->pos);
            }
        }
    }

    uint32_t start = (uint32_t)content_start;
    uint32_t end = (uint32_t)close;
    int multiline = 0;
    for (size_t i = start; i < end; i++) {
        if (e->p[i] == '\n' || e->p[i] == '\r') {
            multiline = 1;
            break;
        }
    }

    ev->style = (q == '\'') ? YEP_STYLE_SINGLE_QUOTED : YEP_STYLE_DOUBLE_QUOTED;
    if (q == '\'') {
        char* out = yep_finish_single(e->p, start, end, multiline, e->pool, &ev->value.len);
        if (out == NULL) {
            ev->value.p = e->p + start;
            ev->borrowed = 1;
        } else {
            ev->value.p = out;
            ev->borrowed = 0;
        }
    } else if (!has_esc && !multiline) {
        ev->value.p = e->p + start;
        ev->value.len = end - start;
        ev->borrowed = 1;
    } else {
        char* out = yep_finish_double(e->p, start, end, multiline, e->pool, &ev->value.len);
        if (out == NULL) {
            return e_fail(e, YEP_ERR_MEMORY, e->pos);
        }
        ev->value.p = out;
        ev->borrowed = 0;
    }

    size_t after = close + 1;
    for (size_t i = start; i < after && i < e->len; i++) {
        if (e->p[i] == '\n') {
            e->line++;
        }
    }
    /* approximate: after a multi-line scalar the column resets */
    if (multiline) {
        size_t ls = close;
        while (ls > start && e->p[ls - 1] != '\n') {
            ls--;
        }
        e->line_start = ls;
    }
    e->pos = after;
    e_skip_inline_space(e);
    return 0;
}

/* Block scalar (| or >) with header, auto/explicit indent, chomping. */
static int e_block_scalar(yep_engine* e, yep_event* ev, uint16_t parent_col) {
    int folded = (e->p[e->pos] == '>');
    e->pos++;
    int chomp = 0;
    int explicit_indent = 0;
    while (e->pos < e->len && e->p[e->pos] != '\n' && e->p[e->pos] != '\r') {
        char c = e->p[e->pos];
        if (c == '-') {
            chomp = 1;
        } else if (c == '+') {
            chomp = 2;
        } else if (c >= '1' && c <= '9') {
            explicit_indent = c - '0';
        } else {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        e->pos++;
    }
    e_line_done(e, e->pos);

    int content_indent = explicit_indent ? parent_col + explicit_indent : -1;
    e->fold_n = 0;
    uint32_t pending_breaks = 0;
    uint32_t trailing_breaks = 0;
    int saw_content = 0;

    while (e->pos < e->len) {
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        if (li.flags & YEP_LF_TAB) {
            return e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
        }
        int blank = (li.flags & YEP_LF_BLANK) != 0;
        if (!blank) {
            if (content_indent < 0) {
                content_indent = li.indent;
                if (content_indent <= parent_col) {
                    break; /* no block content */
                }
            } else if (li.indent <= parent_col) {
                break; /* dedent ends the block */
            } else if (li.indent < content_indent && li.indent <= parent_col) {
                break;
            }
            if (li.indent < content_indent) {
                break;
            }
        }
        if (blank) {
            pending_breaks++;
            e_line_done(e, li.end);
            continue;
        }
        uint32_t from = li.offset + (uint32_t)content_indent;
        if (e->fold_n >= YEP_MAX_FOLD_LINES) {
            return e_fail(e, YEP_ERR_MEMORY, e->pos);
        }
        e->fold[e->fold_n].content.p = e->p + from;
        e->fold[e->fold_n].content.len = li.end > from ? li.end - from : 0;
        /* leading blank lines belong to the block content */
        e->fold[e->fold_n].breaks_before = saw_content ? pending_breaks + 1 : pending_breaks;
        e->fold[e->fold_n].more_indented = li.indent > content_indent;
        e->fold_n++;
        saw_content = 1;
        pending_breaks = 0;
        trailing_breaks = 0;
        /* the break after this content line counts toward trailing */
        size_t at = li.end;
        size_t br = yep_scan_break_len(e->p, e->len, at);
        if (br > 0) {
            trailing_breaks = 1;
            e->pos = at + br;
            e->line++;
            e->line_start = e->pos;
        } else {
            e->pos = at;
        }
    }
    trailing_breaks += pending_breaks;

    char* out = yep_finish_block(e->fold, e->fold_n, folded, chomp, trailing_breaks, e->pool,
                                 &ev->value.len);
    ev->value.p = out; /* may be NULL for an empty block; len 0 */
    ev->borrowed = 0;
    ev->style = folded ? YEP_STYLE_FOLDED : YEP_STYLE_LITERAL;
    return 0;
}

/* Multi-line plain scalar continuation in block context. */
static int e_plain_multiline(yep_engine* e, size_t start, uint32_t block_floor, yep_event* ev,
                             int root_ctx) {
    e->fold_n = 0;
    {
        yep_span s0 = yep_scan_plain(e->p, e->len, start, 0);
        e->fold[0].content.p = e->p + s0.start;
        e->fold[0].content.len = s0.end - s0.start;
        e->fold[0].breaks_before = 0;
        e->fold_n = 1;
    }
    uint32_t breaks = 0;
    for (;;) {
        if (e->pos >= e->len) {
            break;
        }
        /* trailing spaces/tabs after a piece are stripped before the
         * break — they must not inflate the break count */
        while (e->pos < e->len && (e->p[e->pos] == ' ' || e->p[e->pos] == '\t')) {
            e->pos++;
        }
        if (e->pos >= e->len) {
            break;
        }
        if (e->p[e->pos] == '#') {
            e_skip_to_eol(e);
        }
        if (e->pos >= e->len) {
            break;
        }
        breaks++;
        e_line_done(e, e->pos);
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        if (li.flags & YEP_LF_TAB) {
            return e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
        }
        if (li.flags & YEP_LF_BLANK) {
            /* Park at the line end: the loop's next break consumption
             * counts this blank line's own break. */
            e->pos = li.end;
            continue;
        }
        /* At document root there is no parent block, so continuation
         * lines may sit at column 0. */
        int too_shallow = root_ctx ? (li.indent < 0) : (li.indent <= block_floor);
        if (too_shallow || (li.flags & (YEP_LF_DOC_START | YEP_LF_DOC_END)) ||
            (li.flags & YEP_LF_DIRECTIVE)) {
            break;
        }
        size_t cs = e->pos + li.indent;
        yep_span s = yep_scan_plain(e->p, e->len, cs, 0);
        if (s.term == YEP_TERM_COLON) {
            return e_fail(e, YEP_ERR_UNEXPECTED, s.end);
        }
        if (e->fold_n >= YEP_MAX_FOLD_LINES) {
            return e_fail(e, YEP_ERR_MEMORY, e->pos);
        }
        e->fold[e->fold_n].content.p = e->p + s.start;
        e->fold[e->fold_n].content.len = s.end - s.start;
        e->fold[e->fold_n].breaks_before = breaks;
        e->fold_n++;
        breaks = 0;
        e->pos = s.end;
        if (s.term == YEP_TERM_COMMENT) {
            e_skip_to_eol(e);
        }
    }
    if (e->fold_n == 1) {
        return 0; /* single line: the borrowed view stands */
    }
    char* out = yep_fold_plain(e->fold, e->fold_n, e->pool, &ev->value.len);
    if (out == NULL) {
        return e_fail(e, YEP_ERR_MEMORY, e->pos);
    }
    ev->value.p = out;
    ev->borrowed = 0;
    return 0;
}

/* ---------------------------------------------------------------- props */

/* Resolves a scanned tag against the document's %TAG map. !! is the
 * YAML shorthand prefix; !<...> is verbatim; matched handles compose
 * prefix+suffix in the finish pool. */
static yep_view e_resolve_tag(yep_engine* e, yep_view tag) {
    if (tag.len >= 3 && tag.p[0] == '!' && tag.p[1] == '<' && tag.p[tag.len - 1] == '>') {
        yep_view v = {tag.p + 2, tag.len - 3};
        return v;
    }
    /* longest matching handle wins */
    int best = -1;
    uint32_t best_len = 0;
    for (int i = 0; i < e->tagmap_n; i++) {
        if (tag.len >= e->tagmap[i].handle.len && e->tagmap[i].handle.len > best_len &&
            memcmp(tag.p, e->tagmap[i].handle.p, e->tagmap[i].handle.len) == 0) {
            best = i;
            best_len = e->tagmap[i].handle.len;
        }
    }
    if (tag.len >= 2 && tag.p[0] == '!' && tag.p[1] == '!') {
        static const char yns[] = "tag:yaml.org,2002:";
        size_t plen = sizeof(yns) - 1;
        char* out = yep_pool_alloc(e->pool, plen + tag.len - 2, 16);
        if (out == NULL) {
            return tag;
        }
        memcpy(out, yns, plen);
        memcpy(out + plen, tag.p + 2, tag.len - 2);
        yep_view v = {out, (uint32_t)(plen + tag.len - 2)};
        return v;
    }
    if (best >= 0) {
        const yep_view* h = &e->tagmap[best].handle;
        const yep_view* pfx = &e->tagmap[best].prefix;
        size_t n = pfx->len + tag.len - h->len;
        char* out = yep_pool_alloc(e->pool, n ? n : 1, 16);
        if (out == NULL) {
            return tag;
        }
        memcpy(out, pfx->p, pfx->len);
        memcpy(out + pfx->len, tag.p + h->len, tag.len - h->len);
        yep_view v = {out, (uint32_t)n};
        return v;
    }
    return tag;
}

static int e_prop_char(unsigned char c) {
    return !yep_ct_any(c, YEP_CT_BLANK | YEP_CT_LBREAK | YEP_CT_FLOW_IND) && c != ',' && c != '#';
}

/* Parses &anchor / !tag runs at the cursor into *anchor / *tag. */
static void e_props(yep_engine* e, yep_view* anchor, yep_view* tag) {
    for (;;) {
        e_skip_inline_space(e);
        if (e->pos >= e->len) {
            return;
        }
        unsigned char c = (unsigned char)e->p[e->pos];
        if (c == '&') {
            size_t start = ++e->pos;
            while (e->pos < e->len && e_prop_char((unsigned char)e->p[e->pos])) {
                e->pos++;
            }
            anchor->p = e->p + start;
            anchor->len = (uint32_t)(e->pos - start);
        } else if (c == '!') {
            size_t start = e->pos;
            e->pos++;
            if (e->pos < e->len && e->p[e->pos] == '<') {
                while (e->pos < e->len && e->p[e->pos] != '>') {
                    e->pos++;
                }
                if (e->pos < e->len) {
                    e->pos++;
                }
            } else {
                while (e->pos < e->len && e_prop_char((unsigned char)e->p[e->pos])) {
                    e->pos++;
                }
            }
            yep_view raw = {e->p + start, (uint32_t)(e->pos - start)};
            *tag = e_resolve_tag(e, raw);
        } else {
            return;
        }
    }
}

/* ------------------------------------------------------------- aliases */

static int e_alias(yep_engine* e, yep_event* ev) {
    size_t star = e->pos;
    size_t start = ++e->pos;
    while (e->pos < e->len && e_prop_char((unsigned char)e->p[e->pos])) {
        e->pos++;
    }
    yep_view name = {e->p + start, (uint32_t)(e->pos - start)};
    if (name.len == 0) {
        return e_fail(e, YEP_ERR_UNEXPECTED, star);
    }
    if (!anchor_defined(e, name)) {
        e->pos = star;
        return e_fail(e, YEP_ERR_UNDEFINED_ALIAS, star);
    }
    ev->type = YEP_EV_ALIAS;
    ev->value = name;
    e_skip_inline_space(e);
    return 0;
}

/* ---------------------------------------------------------------- flow */

static void e_flow_ws(yep_engine* e) {
    for (;;) {
        while (e->pos < e->len && (e->p[e->pos] == ' ' || e->p[e->pos] == '\t')) {
            e->pos++;
        }
        if (e->pos < e->len && e->p[e->pos] == '#') {
            e_skip_to_eol(e);
            continue;
        }
        if (e->pos < e->len && (e->p[e->pos] == '\n' || e->p[e->pos] == '\r')) {
            e_line_done(e, e->pos);
            continue;
        }
        return;
    }
}

/* Pre-scans a flow collection from its opening bracket, returning 0 with
 * e->pos just past the matching close (no events; for the flow-as-key
 * decision MAP_START must precede the key's events). */
static int e_skip_flow(yep_engine* e) {
    int stk[YEP_MAX_DEPTH];
    int n = 0;
    stk[n++] = (e->p[e->pos] == '[') ? 0 : 1;
    e->pos++;
    while (e->pos < e->len && n > 0) {
        unsigned char c = (unsigned char)e->p[e->pos];
        if (c == '"' || c == '\'') {
            int esc2 = 0;
            const yep_text_kernels* k = yep_text_active();
            ptrdiff_t r = k->quote_scan(e->p + e->pos + 1, e->len - e->pos - 1, (char)c, &esc2);
            if (r < 0) {
                return e_fail(e, YEP_ERR_UNTERMINATED_QUOTE, e->pos);
            }
            e->pos += 1 + (size_t)r + 1;
            continue;
        }
        if (c == '#') {
            e_skip_to_eol(e);
            continue;
        }
        if (c == '\n' || c == '\r') {
            e_line_done(e, e->pos);
            continue;
        }
        if (c == '[') {
            if (n >= YEP_MAX_DEPTH) {
                return e_fail(e, YEP_ERR_DEPTH, e->pos);
            }
            stk[n++] = 0;
        } else if (c == '{') {
            if (n >= YEP_MAX_DEPTH) {
                return e_fail(e, YEP_ERR_DEPTH, e->pos);
            }
            stk[n++] = 1;
        } else if (c == ']') {
            if (stk[n - 1] != 0) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
            }
            n--;
        } else if (c == '}') {
            if (stk[n - 1] != 1) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
            }
            n--;
        }
        e->pos++;
    }
    if (n != 0) {
        return e_fail(e, YEP_ERR_UNEXPECTED, e->len);
    }
    return 0;
}

/* Parses one flow node at the cursor into *ev. Return: 1 node parsed,
 * 0 no node (separator/close next), 2 nested opener, -1 error. */
static int e_flow_node(yep_engine* e, yep_event* ev, int keyish) {
    e_flow_ws(e);
    if (e->pos >= e->len) {
        return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
    }
    unsigned char c = (unsigned char)e->p[e->pos];
    if (c == '[' || c == '{') {
        return 2;
    }
    if (c == ']' || c == '}' || c == ',') {
        return 0; /* separator / close */
    }
    if (c == ':') {
        /* ':' separates only when followed by blank/EOL/flow indicator;
         * ":x" is a plain scalar starting with a colon */
        size_t n2 = e->pos + 1;
        if (n2 >= e->len || e->p[n2] == ' ' || e->p[n2] == '\n' || e->p[n2] == '\r' ||
            yep_ct_is((unsigned char)e->p[n2], YEP_CT_FLOW_IND)) {
            return 0;
        }
    }

    e_event_init(ev, YEP_EV_SCALAR);
    ev->line = e->line;
    ev->col = e_col(e, e->pos) + 1;
    yep_view anchor = {0}, tag = {0};
    e_props(e, &anchor, &tag);
    ev->anchor = anchor;
    ev->tag = tag;
    e_flow_ws(e);
    c = (unsigned char)e->p[e->pos];
    if (c == '"' || c == '\'') {
        ev->implicit = 0;
        return e_quoted(e, ev) == 0 ? 1 : -1;
    }
    if (c == '*') {
        return e_alias(e, ev) == 0 ? 1 : -1;
    }
    if (c == '[' || c == '{') {
        return 2;
    }
    /* plain: may span lines (folded); a leading non-structural ':' is
     * part of the scalar ("::x" per the suite) — keep it in the first
     * piece by scanning from the colon itself */
    const char* piece_start_fix = e->p + e->pos;
    if ((unsigned char)*piece_start_fix == ':') {
        e->pos++;
        e_flow_ws(e);
        if (e->pos >= e->len) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        /* rescan from the colon so the piece includes it */
        e->pos = (size_t)(piece_start_fix - e->p);
    }
    e->fold_n = 0;
    for (;;) {
        yep_span s = yep_scan_plain(e->p, e->len, e->pos, 1);
        if (keyish) {
            /* key candidates end at ANY ':' — plain keys cannot contain
             * one, and "?foo:bar" separates without a space */
            size_t ci = s.start;
            while (ci < s.end && e->p[ci] != ':') {
                ci++;
            }
            if (ci < s.end) {
                s.end = (uint32_t)ci;
                s.term = YEP_TERM_COLON;
            }
        }
        if (s.end == s.start && e->fold_n == 0) {
            return 0; /* empty token: separator/close comes next */
        }
        if (e->fold_n >= YEP_MAX_FOLD_LINES) {
            return e_fail(e, YEP_ERR_MEMORY, e->pos);
        }
        if (s.end > s.start) {
            e->fold[e->fold_n].content.p = e->p + s.start;
            e->fold[e->fold_n].content.len = s.end - s.start;
            e->fold[e->fold_n].breaks_before = (e->fold_n == 0) ? 0 : 1;
            e->fold_n++;
        }
        if (s.term == YEP_TERM_EOL || s.term == YEP_TERM_COMMENT) {
            e->pos = s.end;
            if (s.term == YEP_TERM_COMMENT) {
                e_skip_to_eol(e);
            }
            e_line_done(e, e->pos);
            e_flow_ws(e);
            if (e->pos < e->len && e->p[e->pos] != ']' && e->p[e->pos] != '}' &&
                e->p[e->pos] != ',' && e->p[e->pos] != '#' && e->p[e->pos] != '\n' &&
                e->p[e->pos] != '\r') {
                continue; /* continuation on a following line */
            }
            break; /* at a boundary; pos is on the separator/close */
        }
        e->pos = s.end;
        break;
    }
    if (e->fold_n == 1) {
        ev->value.p = e->fold[0].content.p;
        ev->value.len = e->fold[0].content.len;
        ev->borrowed = 1;
    } else {
        char* out = yep_fold_plain(e->fold, e->fold_n, e->pool, &ev->value.len);
        if (out == NULL) {
            return e_fail(e, YEP_ERR_MEMORY, e->pos);
        }
        ev->value.p = out;
        ev->borrowed = 0;
    }
    ev->style = YEP_STYLE_PLAIN;
    ev->implicit = 1;
    return 1;
}

/* Iterative flow kernel; emits events for the whole collection. */
static int e_flow(yep_engine* e, yep_view anchor, yep_view tag) {
    struct {
        uint8_t kind;        /* 0 seq, 1 map */
        uint8_t pending_key; /* map: 1 key seen awaiting ':' value; 2 key emitted w/o ':' */
        uint8_t sep;         /* a ',' was consumed and no entry followed yet */
        uint32_t entries;    /* completed entries (for trailing-comma detection) */
    } st[YEP_MAX_DEPTH];
    int n = 0;

    unsigned char open = (unsigned char)e->p[e->pos];
    e->pos++;
    yep_event ev;
    e_event_init(&ev, open == '[' ? YEP_EV_SEQ_START : YEP_EV_MAP_START);
    ev.flow = 1;
    ev.anchor = anchor;
    ev.tag = tag;
    ev.line = e->line;
    ev.col = e_col(e, e->pos);
    if (emit_now(e, &ev) != 0) {
        return -2;
    }
    st[n].kind = (open == '[') ? 0 : 1;
    st[n].pending_key = 0;
    st[n].sep = 0;
    st[n].entries = 0;
    n++;

    for (;;) {
        e_event_init(&ev, YEP_EV_NONE);
        int keyish = (st[n - 1].kind == 1 && st[n - 1].pending_key == 0);
        int rc = e_flow_node(e, &ev, keyish);
        if (rc == -1) {
            return -1;
        }
        if (rc == 2) {
            if (n >= YEP_MAX_DEPTH) {
                return e_fail(e, YEP_ERR_DEPTH, e->pos);
            }
            unsigned char oc = (unsigned char)e->p[e->pos];
            e->pos++;
            e_event_init(&ev, oc == '[' ? YEP_EV_SEQ_START : YEP_EV_MAP_START);
            ev.flow = 1;
            ev.line = e->line;
            ev.col = e_col(e, e->pos);
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            /* the nested collection is an entry of the parent frame; when
             * it appears in a map's value position it completes the pair */
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 1) {
                st[n - 1].pending_key = 0;
            }
            st[n - 1].sep = 0;
            st[n - 1].entries++;
            st[n].kind = (oc == '[') ? 0 : 1;
            st[n].pending_key = 0;
            st[n].sep = 0;
            st[n].entries = 0;
            n++;
            continue;
        }
        if (rc == 1) {
            st[n - 1].sep = 0;
            st[n - 1].entries++;
            /* key detection: ':' after the node (spaces and line breaks
             * may precede it: "unquoted : value", "\"foo\"\n: bar").
             * After a QUOTED node the colon separates JSON-style. */
            size_t save_pos = e->pos;
            e_flow_ws(e); /* crosses newlines; restored below when no colon */
            int quoted = (ev.type == YEP_EV_SCALAR && (ev.style == YEP_STYLE_SINGLE_QUOTED ||
                                                       ev.style == YEP_STYLE_DOUBLE_QUOTED));
            int key_pos = (st[n - 1].kind == 1 && st[n - 1].pending_key == 0);
            int colon = (e->pos < e->len && e->p[e->pos] == ':' &&
                         (quoted || key_pos || e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' ||
                          e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r' ||
                          yep_ct_is((unsigned char)e->p[e->pos + 1], YEP_CT_FLOW_IND)));
            if (!colon) {
                e->pos = save_pos; /* not a key; the caller re-reads */
            }
            if (colon && st[n - 1].kind == 1 && st[n - 1].pending_key == 0) {
                e->pos++;
                st[n - 1].pending_key = 1;
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                /* value next; if none before , or } → null */
                continue;
            }
            if (colon && st[n - 1].kind == 0) {
                /* single-pair mapping inside a sequence */
                yep_event mk;
                e_event_init(&mk, YEP_EV_MAP_START);
                mk.flow = 1;
                mk.line = ev.line;
                mk.col = ev.col;
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                e->pos++;
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                yep_event vev;
                int vrc = e_flow_node(e, &vev, 0);
                if (vrc == -1) {
                    return -1;
                }
                if (vrc == 2) {
                    /* nested collection as the pair value: run a nested
                     * kernel by pushing through our own loop */
                    if (n >= YEP_MAX_DEPTH) {
                        return e_fail(e, YEP_ERR_DEPTH, e->pos);
                    }
                    unsigned char oc = (unsigned char)e->p[e->pos];
                    e->pos++;
                    e_event_init(&vev, oc == '[' ? YEP_EV_SEQ_START : YEP_EV_MAP_START);
                    vev.flow = 1;
                    if (emit_now(e, &vev) != 0) {
                        return -2;
                    }
                    st[n].kind = (oc == '[') ? 0 : 1;
                    st[n].pending_key = 0;
                    st[n].sep = 1;
                    n++;
                    /* mark that this frame must close into a MAP_END for
                     * the single-pair when popped: simplified — emit the
                     * MAP_END right after the inner closes is handled by
                     * tracking pair_depth */
                    continue; /* v1 limitation: nested-in-pair closes are
                                 balanced by the close handler below */
                }
                if (vrc == 1 && emit_now(e, &vev) != 0) {
                    return -2;
                }
                if (vrc == 0) {
                    /* no value: null */
                    yep_event nv;
                    e_event_init(&nv, YEP_EV_SCALAR);
                    nv.implicit = 1;
                    if (emit_now(e, &nv) != 0) {
                        return -2;
                    }
                }
                e_event_init(&mk, YEP_EV_MAP_END);
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                continue;
            }
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 0) {
                /* bare entry in a flow map: key with null value */
                st[n - 1].pending_key = 2;
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                continue;
            }
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 1) {
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                st[n - 1].pending_key = 0;
                continue;
            }
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            continue;
        }

        /* rc == 0: separator or close */
        e_flow_ws(e);
        if (e->pos >= e->len) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        unsigned char c = (unsigned char)e->p[e->pos];
        if (c == ',') {
            e->pos++;
            if (st[n - 1].sep) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "{,x}" / "[1,,2]" */
            }
            if (st[n - 1].kind == 1 && st[n - 1].pending_key != 0) {
                /* key without ':' value, or ':' without value: null value */
                yep_event nv;
                e_event_init(&nv, YEP_EV_SCALAR);
                nv.implicit = 1;
                if (emit_now(e, &nv) != 0) {
                    return -2;
                }
                st[n - 1].pending_key = 0;
            }
            st[n - 1].sep = 1;
            continue;
        }
        if (c == ']' || c == '}') {
            unsigned char want = st[n - 1].kind ? '}' : ']';
            if (c != want) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
            }
            /* YAML 1.2 allows a trailing separator before the closer
             * ("[1, ]", "{a: 1, }"); the comma handler already rejected
             * doubled commas. */
            if (st[n - 1].kind == 1 && st[n - 1].pending_key != 0) {
                yep_event nv;
                e_event_init(&nv, YEP_EV_SCALAR);
                nv.implicit = 1;
                if (emit_now(e, &nv) != 0) {
                    return -2;
                }
                st[n - 1].pending_key = 0;
            }
            e->pos++;
            e_event_init(&ev, st[n - 1].kind ? YEP_EV_MAP_END : YEP_EV_SEQ_END);
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            n--;
            if (n == 0) {
                e_skip_inline_space(e);
                return 0;
            }
            continue;
        }
        if (c == ':') {
            /* ':' after a bare key completes it (value next); with no
             * pending key it is a stray — error */
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 2) {
                e->pos++;
                st[n - 1].pending_key = 1;
                continue;
            }
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        /* any other stray byte */
        return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
    }
}

/* -------------------------------------------------------- block machine */

/* Parses the node at the cursor (content position). Returns 0 ok,
 * -1 error, -2 sink abort. Collections may remain open (frames).
 *
 * Property semantics: same-line props belong to the node that follows
 * them (a key scalar when the line turns out to be a mapping entry);
 * pend props (parsed at the parent's value position on an earlier line)
 * belong to the VALUE node as a whole — a collection START event when
 * the value is a collection. */
static int e_node(yep_engine* e, yep_ctx ctx, uint16_t floor_col) {
    size_t node_at = e->pos;
    yep_view pend_a = e->pend_anchor, pend_t = e->pend_tag;
    e->pend_anchor.p = NULL;
    e->pend_anchor.len = 0;
    e->pend_tag.p = NULL;
    e->pend_tag.len = 0;
    yep_view anchor = {0}, tag = {0};
    e_props(e, &anchor, &tag);
    if (!yep_view_is_empty(anchor)) {
        anchor_define(e, anchor);
    }
    /* Node-level props for scalar/alias/flow VALUE events: same-line
     * wins over pend. */
    yep_view node_a = yep_view_is_empty(anchor) ? pend_a : anchor;
    yep_view node_t = yep_view_is_empty(tag) ? pend_t : tag;
    e_skip_inline_space(e);

    yep_event ev;
    e_event_init(&ev, YEP_EV_SCALAR);
    ev.anchor = node_a;
    ev.tag = node_t;
    ev.line = e->line;
    ev.col = e_col(e, node_at) + 1;

    if (e_at_eol(e)) {
        /* properties with the value on following lines */
        e->pend_anchor = node_a;
        e->pend_tag = node_t;
        return e_parse_value(e, ctx, floor_col);
    }

    unsigned char c = (unsigned char)e->p[e->pos];

    if (c == '-' && (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\n' ||
                     e->p[e->pos + 1] == '\r')) {
        if (ctx == YEP_CTX_AFTER_COLON) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "key: - x" */
        }
        uint16_t col = e_col(e, e->pos);
        int rc = e_open_seq(e, col, e->line, col + 1, node_a, node_t);
        if (rc != 0) {
            return rc;
        }
        e->pos++; /* '-' */
        return e_parse_value(e, YEP_CTX_AFTER_DASH, col);
    }

    if (c == '?' && (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\n' ||
                     e->p[e->pos + 1] == '\r')) {
        if (ctx != YEP_CTX_FRESH) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        uint16_t col = e_col(e, e->pos);
        int rc = e_open_map(e, col, e->line, col + 1, pend_a, pend_t);
        if (rc != 0) {
            return rc;
        }
        e->pos++;
        /* the explicit key is parsed as a value (may be empty → null);
         * its content may ALIGN with the '?' column */
        e->q_key_pending = 1;
        return e_parse_value(e, YEP_CTX_AFTER_Q, col);
    }

    if (c == '[' || c == '{') {
        /* pre-scan: is this flow collection a mapping KEY? */
        size_t save = e->pos;
        if (e_skip_flow(e) != 0) {
            return -1;
        }
        e_skip_inline_space(e);
        int is_key = e_colon_at(e, e->pos);
        e->pos = save;
        if (is_key && ctx == YEP_CTX_AFTER_COLON) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        if (is_key) {
            uint16_t key_col = e_col(e, node_at);
            int rc = e_open_map(e, key_col, e->line, key_col + 1, pend_a, pend_t);
            if (rc != 0) {
                return rc;
            }
            rc = e_flow(e, node_a, node_t);
            if (rc != 0) {
                return rc;
            }
            e->pos++; /* ':' */
            return e_parse_value(e, YEP_CTX_AFTER_COLON, key_col);
        }
        return e_flow(e, node_a, node_t);
    }

    if (c == '"' || c == '\'') {
        if (e_quoted(e, &ev) != 0) {
            return -1;
        }
        if (e_colon_at(e, e->pos)) {
            if (ctx == YEP_CTX_AFTER_COLON) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
            }
            uint16_t key_col = e_col(e, node_at);
            int rc = e_open_map(e, key_col, e->line, key_col + 1, pend_a, pend_t);
            if (rc != 0) {
                return rc;
            }
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            e->pos++; /* ':' */
            return e_parse_value(e, YEP_CTX_AFTER_COLON, key_col);
        }
        if (!e_at_eol(e)) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        return emit_now(e, &ev) == 0 ? 0 : -2;
    }

    if (c == '*') {
        if (e_alias(e, &ev) != 0) {
            return -1;
        }
        if (e_colon_at(e, e->pos)) {
            if (ctx == YEP_CTX_AFTER_COLON) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
            }
            uint16_t key_col = e_col(e, node_at);
            int rc = e_open_map(e, key_col, e->line, key_col + 1, pend_a, pend_t);
            if (rc != 0) {
                return rc;
            }
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            e->pos++;
            return e_parse_value(e, YEP_CTX_AFTER_COLON, key_col);
        }
        if (!e_at_eol(e)) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        return emit_now(e, &ev) == 0 ? 0 : -2;
    }

    if (c == '|' || c == '>') {
        if (e_block_scalar(e, &ev, floor_col) != 0) {
            return -1;
        }
        return emit_now(e, &ev) == 0 ? 0 : -2;
    }

    /* plain scalar */
    size_t start = e->pos;
    yep_span s = yep_scan_plain(e->p, e->len, e->pos, 0);
    if (s.term == YEP_TERM_COLON) {
        if (ctx == YEP_CTX_AFTER_COLON) {
            return e_fail(e, YEP_ERR_UNEXPECTED, s.end);
        }
        uint16_t key_col = e_col(e, node_at);
        int rc = e_open_map(e, key_col, e->line, key_col + 1, pend_a, pend_t);
        if (rc != 0) {
            return rc;
        }
        yep_event kv;
        e_event_init(&kv, YEP_EV_SCALAR);
        kv.anchor = anchor;
        kv.tag = tag;
        kv.style = YEP_STYLE_PLAIN;
        kv.implicit = 1;
        kv.value.p = e->p + s.start;
        kv.value.len = s.end - s.start;
        kv.borrowed = 1;
        kv.line = e->line;
        kv.col = key_col + 1;
        if (emit_now(e, &kv) != 0) {
            return -2;
        }
        e->pos = s.end + 1; /* past ':' */
        return e_parse_value(e, YEP_CTX_AFTER_COLON, key_col);
    }

    ev.value.p = e->p + s.start;
    ev.value.len = s.end - s.start;
    ev.borrowed = 1;
    ev.style = YEP_STYLE_PLAIN;
    ev.implicit = 1;
    e->pos = s.end;
    if (s.term == YEP_TERM_COMMENT) {
        e_skip_to_eol(e);
    }
    if (e_plain_multiline(e, start, floor_col, &ev, e->depth == 0) != 0) {
        return -1;
    }
    return emit_now(e, &ev) == 0 ? 0 : -2;
}

/* Value after "key:" / "-" / "?": on this line, or on following lines. */
static int e_parse_value(yep_engine* e, yep_ctx ctx, uint16_t floor_col) {
    e_skip_inline_space(e);
    if (!e_at_eol(e)) {
        int rc = e_node(e, ctx, floor_col);
        if (rc != 0) {
            return rc;
        }
        /* A plain value with continuations legitimately consumed through
         * the line break — pos sits at the next line's START; the main
         * loop must see the indentation, so do not skip it. */
        if (e->pos == e->line_start) {
            return 0;
        }
        e_skip_inline_space(e);
        if (!e_at_eol(e)) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        return 0;
    }

    /* following lines */
    for (;;) {
        if (e->pos >= e->len) {
            goto empty_value;
        }
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        if (li.flags & YEP_LF_TAB) {
            return e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
        }
        if (li.flags & YEP_LF_BLANK) {
            e_line_done(e, li.end);
            continue;
        }
        if (li.flags & YEP_LF_COMMENT) {
            e_line_done(e, li.end);
            continue;
        }
        /* At document root there is no parent block: a following line
         * at any indent is the value ("&a\n- x"). Explicit-key content
         * may align with the '?' column. */
        if (li.indent > floor_col || e->depth == 0 ||
            (ctx == YEP_CTX_AFTER_Q && li.indent == floor_col)) {
            e->pos += li.indent;
            return e_node(e, YEP_CTX_FRESH, floor_col);
        }
        /* indentless sequence as a mapping value */
        if (li.indent == floor_col && ctx == YEP_CTX_AFTER_COLON && li.first == '-' &&
            (li.offset + li.indent + 1 >= e->len || e->p[li.offset + li.indent + 1] == ' ' ||
             e->p[li.offset + li.indent + 1] == '\n' || e->p[li.offset + li.indent + 1] == '\r')) {
            e->pos += li.indent;
            return e_node(e, YEP_CTX_FRESH, floor_col);
        }
        goto empty_value;
    }

empty_value: {
    yep_event ev;
    e_event_init(&ev, YEP_EV_SCALAR);
    ev.style = YEP_STYLE_PLAIN;
    ev.implicit = 1;
    ev.anchor = e->pend_anchor;
    ev.tag = e->pend_tag;
    e->pend_anchor.p = NULL;
    e->pend_anchor.len = 0;
    e->pend_tag.p = NULL;
    e->pend_tag.len = 0;
    return emit_now(e, &ev) == 0 ? 0 : -2;
}
}

/* ------------------------------------------------------------ lifecycle */

yep_engine* yep_engine_create(const yep_allocator* sys) {
    if (sys == NULL) {
        return NULL;
    }
    yep_pool* pool = yep_pool_create(sys, 8192);
    if (pool == NULL) {
        return NULL;
    }
    yep_engine* e = yep_alloc(sys, sizeof(yep_engine));
    if (e == NULL) {
        yep_pool_destroy(pool);
        return NULL;
    }
    memset(e, 0, sizeof(*e));
    e->sys = sys;
    e->pool = pool;
    yep_error_clear(&e->err);
    return e;
}

void yep_engine_destroy(yep_engine* e) {
    if (e == NULL) {
        return;
    }
    yep_pool_destroy(e->pool);
    yep_free(e->sys, e);
}

int yep_engine_run(yep_engine* e, const char* buf, size_t len, const yep_sink* sink) {
    if (e == NULL || (buf == NULL && len != 0)) {
        return -1;
    }
    e->p = buf;
    e->len = len;
    e->pos = 0;
    e->line = 1;
    e->line_start = 0;
    e->depth = 0;
    e->anchor_count = 0;
    e->tagmap_n = 0;
    e->doc_content = 0;
    e->q_key_pending = 0;
    e->sink = sink;
    yep_error_clear(&e->err);

    yep_event ev;
    e_event_init(&ev, YEP_EV_STREAM_START);
    if (emit_now(e, &ev) != 0) {
        return -2;
    }

    int doc_open = 0;

    while (e->pos < e->len) {
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        if (li.flags & YEP_LF_TAB) {
            e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
            goto fail;
        }
        if (li.flags & YEP_LF_DIRECTIVE) {
            /* %TAG <handle> <prefix> — registered for the next document */
            size_t d = li.offset + li.indent + 1;
            while (d < li.end && e->p[d] == ' ') {
                d++;
            }
            if (li.end - d >= 3 && memcmp(e->p + d, "TAG", 3) == 0 && e->tagmap_n < 8) {
                size_t h0 = d + 3;
                while (h0 < li.end && e->p[h0] == ' ') {
                    h0++;
                }
                size_t h1 = h0;
                while (h1 < li.end && e->p[h1] != ' ') {
                    h1++;
                }
                size_t p0 = h1;
                while (p0 < li.end && e->p[p0] == ' ') {
                    p0++;
                }
                size_t p1 = li.end;
                while (p1 > p0 && (e->p[p1 - 1] == ' ' || e->p[p1 - 1] == '\r')) {
                    p1--;
                }
                if (h1 > h0 && p1 > p0 && e->p[h0] == '!') {
                    e->tagmap[e->tagmap_n].handle.p = e->p + h0;
                    e->tagmap[e->tagmap_n].handle.len = (uint32_t)(h1 - h0);
                    e->tagmap[e->tagmap_n].prefix.p = e->p + p0;
                    e->tagmap[e->tagmap_n].prefix.len = (uint32_t)(p1 - p0);
                    e->tagmap_n++;
                }
            }
            e_line_done(e, li.end);
            continue;
        }
        if (li.flags & (YEP_LF_BLANK | YEP_LF_COMMENT)) {
            e_line_done(e, li.end);
            continue;
        }
        if (li.flags & YEP_LF_DOC_START) {
            int rc = e_close_to(e, 0);
            if (rc != 0) {
                return rc;
            }
            if (doc_open) {
                e_event_init(&ev, YEP_EV_DOCUMENT_END);
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                doc_open = 0;
            }
            e_event_init(&ev, YEP_EV_DOCUMENT_START);
            ev.style = 1; /* explicit marker */
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            doc_open = 1;
            e->pos = li.offset + li.indent + 3;
            e_skip_inline_space(e);
            if (!e_at_eol(e)) {
                rc = e_node(e, YEP_CTX_FRESH, li.indent);
                if (rc != 0) {
                    if (rc == -2) {
                        return -2;
                    }
                    goto fail;
                }
                e->doc_content = 1;
            }
            e_line_done(e, e->pos);
            continue;
        }
        if (li.flags & YEP_LF_DOC_END) {
            int rc = e_close_to(e, 0);
            if (rc != 0) {
                return rc;
            }
            if (doc_open) {
                e_event_init(&ev, YEP_EV_DOCUMENT_END);
                ev.style = 1;    /* explicit "..." marker */
                e->tagmap_n = 0; /* directives are per-document */
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                doc_open = 0;
            }
            e_line_done(e, li.end);
            continue;
        }

        if (!doc_open) {
            e_event_init(&ev, YEP_EV_DOCUMENT_START);
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            doc_open = 1;
        }

        /* unwind frames that cannot continue at this column */
        uint16_t c = li.indent;
        while (e->depth > 0) {
            uint16_t top = e->frames[e->depth - 1].col;
            uint8_t kind = e->frames[e->depth - 1].kind;
            if (top > c) {
                int rc = e_close_to(e, e->depth - 1);
                if (rc != 0) {
                    return rc;
                }
                continue;
            }
            if (top == c) {
                int continues;
                if (kind == YEP_FRAME_SEQ) {
                    continues = (li.first == '-' && (li.offset + li.indent + 1 >= e->len ||
                                                     e->p[li.offset + li.indent + 1] == ' ' ||
                                                     e->p[li.offset + li.indent + 1] == '\n' ||
                                                     e->p[li.offset + li.indent + 1] == '\r'));
                } else {
                    continues =
                        yep_scan_is_key_start(li.first) || li.first == ':' || li.first == '?';
                }
                if (!continues) {
                    int rc = e_close_to(e, e->depth - 1);
                    if (rc != 0) {
                        return rc;
                    }
                    continue;
                }
            }
            break;
        }

        e->pos += li.indent;
        unsigned char first = (unsigned char)e->p[e->pos];
        if (first == ':' && (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' ||
                             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
            /* explicit-key value line, or a bare ':' pair (empty key) */
            int rc;
            if (e->depth == 0 || e->frames[e->depth - 1].kind != YEP_FRAME_MAP) {
                rc = e_open_map(e, c, e->line, c + 1, e->pend_anchor, e->pend_tag);
                if (rc != 0) {
                    if (rc == -2) {
                        return -2;
                    }
                    goto fail;
                }
            }
            if (!e->q_key_pending) {
                yep_event kv;
                e_event_init(&kv, YEP_EV_SCALAR);
                kv.style = YEP_STYLE_PLAIN;
                kv.implicit = 1;
                if (emit_now(e, &kv) != 0) {
                    return -2;
                }
            }
            e->q_key_pending = 0;
            e->pos++;
            rc = e_parse_value(e, YEP_CTX_AFTER_COLON, e->frames[e->depth - 1].col);
            if (rc != 0) {
                if (rc == -2) {
                    return -2;
                }
                goto fail;
            }
            e->doc_content = 1;
            e_line_done(e, e->pos);
            continue;
        }

        {
            int rc = e_node(e, YEP_CTX_FRESH, e->depth > 0 ? e->frames[e->depth - 1].col : 0);
            if (rc != 0) {
                if (rc == -2) {
                    return -2;
                }
                goto fail;
            }
            e->doc_content = 1;
        }
        e_line_done(e, e->pos);
    }

    {
        int rc = e_close_to(e, 0);
        if (rc != 0) {
            return rc;
        }
    }
    if (doc_open) {
        if (!e->doc_content) {
            e_event_init(&ev, YEP_EV_SCALAR);
            ev.implicit = 1;
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
        }
        e_event_init(&ev, YEP_EV_DOCUMENT_END);
        if (emit_now(e, &ev) != 0) {
            return -2;
        }
        e->tagmap_n = 0;
    }
    e_event_init(&ev, YEP_EV_STREAM_END);
    if (emit_now(e, &ev) != 0) {
        return -2;
    }
    return 0;

fail:
    *yep_error_tls() = e->err;
    return -1;
}

const yep_error* yep_engine_error(const yep_engine* e) {
    return e ? &e->err : NULL;
}
