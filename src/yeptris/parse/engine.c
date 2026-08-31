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
    YEP_CTX_VALUE_LINE,  /* value of "key:" on a FOLLOWING line: indentless
                          * sequences allowed, same-line prohibitions lifted */
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

    int doc_content;     /* any node emitted for the current document */
    int q_key_pending;   /* an explicit '?' key awaits its ':' value line */
    int q_map_depth;     /* frame depth of the '?' mapping (flush guard) */
    int q_value_pending; /* an explicit key was emitted; its value is null
                            until a ':' line supplies one */
    /* %TAG handle map for the current document (engine = tag SSOT) */
    struct {
        yep_view handle, prefix;
    } tagmap[8];
    int tagmap_n;
    int saw_yaml; /* %YAML seen for the pending document */

    /* Flow single-pair deferral: a sequence entry's events are buffered
     * until we know whether ':' follows ("[a: b]" needs MAP_START before
     * the key's events, but ':' is only seen after them). */
    yep_event* ev_buf;
    uint32_t ev_buf_n, ev_buf_cap;
    uint32_t ev_scopes;  /* active entry-buffer scopes */
    char* norm_buf;      /* owned input copy with exotic breaks normalized */
    int last_root_flow;  /* a root-level flow node just completed */
    uint16_t flow_floor; /* parent column of the current block-level flow */
    int flow_enforce;    /* flow continuation lines must out-indent it */
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

static void e_event_init(yep_event* ev, yep_event_type t) {
    memset(ev, 0, sizeof(*ev));
    ev->type = t;
}

static int emit_now(yep_engine* e, const yep_event* ev) {
    if (e->ev_scopes > 0) {
        if (e->ev_buf_n == e->ev_buf_cap) {
            uint32_t cap = e->ev_buf_cap ? e->ev_buf_cap * 2 : 16;
            yep_event* nb = yep_pool_alloc(e->pool, (size_t)cap * sizeof(yep_event), 16);
            if (nb == NULL) {
                e_fail(e, YEP_ERR_MEMORY, e->pos);
                return -2;
            }
            if (e->ev_buf_n > 0) {
                memcpy(nb, e->ev_buf, (size_t)e->ev_buf_n * sizeof(yep_event));
            }
            e->ev_buf = nb;
            e->ev_buf_cap = cap;
        }
        e->ev_buf[e->ev_buf_n++] = *ev;
        return 0;
    }
    return e->sink ? e->sink->on_event(e->sink->ctx, ev) : 0;
}

/* Emits buffered events from `from` to the sink and truncates. */
static int e_buf_send(yep_engine* e, uint32_t from) {
    for (uint32_t i = from; i < e->ev_buf_n; i++) {
        if (e->sink && e->sink->on_event(e->sink->ctx, &e->ev_buf[i]) != 0) {
            return -2;
        }
    }
    e->ev_buf_n = from;
    return 0;
}

/* Inserts a flow MAP_START at `from`, wrapping the buffered entry. */
static int e_buf_wrap(yep_engine* e, uint32_t from) {
    if (e->ev_buf_n >= e->ev_buf_cap) {
        uint32_t cap = e->ev_buf_cap ? e->ev_buf_cap * 2 : 16;
        yep_event* nb = yep_pool_alloc(e->pool, (size_t)cap * sizeof(yep_event), 16);
        if (nb == NULL) {
            e_fail(e, YEP_ERR_MEMORY, e->pos);
            return -2;
        }
        if (e->ev_buf_n > 0) {
            memcpy(nb, e->ev_buf, (size_t)e->ev_buf_n * sizeof(yep_event));
        }
        e->ev_buf = nb;
        e->ev_buf_cap = cap;
    }
    memmove(&e->ev_buf[from + 1], &e->ev_buf[from],
            (size_t)(e->ev_buf_n - from) * sizeof(yep_event));
    e_event_init(&e->ev_buf[from], YEP_EV_MAP_START);
    e->ev_buf[from].flow = 1;
    e->ev_buf_n++;
    return 0;
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

/* '#' only opens a comment when a blank (or nothing) precedes it. */
static int e_comment_at(const yep_engine* e) {
    return e->p[e->pos] == '#' &&
           (e->pos == 0 || e->p[e->pos - 1] == ' ' || e->p[e->pos - 1] == '\t' ||
            e->p[e->pos - 1] == '\n' || e->p[e->pos - 1] == '\r');
}

static int e_at_eol(yep_engine* e) {
    return e->pos >= e->len || e->p[e->pos] == '\n' || e->p[e->pos] == '\r' || e_comment_at(e);
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

/* Emits the null value for an explicit key that never got its ':' line. */
static int e_flush_q_value(yep_engine* e) {
    if (!e->q_value_pending) {
        return 0;
    }
    if (e->depth > e->q_map_depth) {
        return 0; /* the key's own structure is still open */
    }
    e->q_value_pending = 0;
    yep_event ev;
    e_event_init(&ev, YEP_EV_SCALAR);
    ev.style = YEP_STYLE_PLAIN;
    ev.implicit = 1;
    return emit_now(e, &ev) == 0 ? 0 : -2;
}

static int e_close_to(yep_engine* e, int to_depth) {
    while (e->depth > to_depth) {
        /* the pending '?' value flushes once the key's structure closed */
        int rc = e_flush_q_value(e);
        if (rc != 0) {
            return rc;
        }
        if (e->q_value_pending && e->depth == e->q_map_depth) {
            continue; /* flushed the null; this frame closes next round */
        }
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
static int e_quoted_floor(yep_engine* e, yep_event* ev, uint16_t min_indent, int enforce) {
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
            /* each following line must be content, not a document marker,
             * and in block context must out-indent the parent (QB6E) */
            size_t br = yep_scan_break_len(e->p, e->len, i);
            yep_line_info ql = yep_scan_line(e->p, e->len, i + br);
            if (ql.flags & (YEP_LF_DOC_START | YEP_LF_DOC_END | YEP_LF_DIRECTIVE)) {
                return e_fail(e, YEP_ERR_UNTERMINATED_QUOTE, i + br);
            }
            if (enforce && !(ql.flags & YEP_LF_BLANK) && ql.indent <= min_indent) {
                return e_fail(e, YEP_ERR_BAD_INDENT, i + br + ql.indent);
            }
        }
    }
    /* escape pre-validation (double quotes only: ' has no escapes) */
    for (size_t i = (q == '"') ? start : end; i < end; i++) {
        if (e->p[i] != '\\' || i + 1 >= end) {
            continue;
        }
        unsigned char esc = (unsigned char)e->p[i + 1];
        if (esc == 'x' || esc == 'u' || esc == 'U' || esc == '\n' || esc == '\r') {
            continue; /* checked by finish_double */
        }
        switch (esc) {
        case '0':
        case 'a':
        case 'b':
        case 't':
        case '\t':
        case 'n':
        case 'v':
        case 'f':
        case 'r':
        case 'e':
        case ' ':
        case '"':
        case '/':
        case '\\':
        case 'N':
        case '_':
        case 'L':
        case 'P':
            break;
        default:
            return e_fail(e, YEP_ERR_INVALID_ESCAPE, i);
        }
        i++; /* skip the escaped byte */
    }

    ev->style = (q == '\'') ? YEP_STYLE_SINGLE_QUOTED : YEP_STYLE_DOUBLE_QUOTED;
    if (q == '\'') {
        ev->multiline = (uint8_t)multiline;
        char* out = yep_finish_single(e->p, start, end, multiline, e->pool, &ev->value.len);
        if (out == NULL) {
            ev->value.p = e->p + start;
            ev->value.len = end - start; /* finish_single returns before writing len */
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
        ev->multiline = (uint8_t)multiline;
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
static int e_block_scalar(yep_engine* e, yep_event* ev, int parent_col) {
    int folded = (e->p[e->pos] == '>');
    e->pos++;
    int chomp = 0;
    int explicit_indent = 0;
    while (e->pos < e->len && e->p[e->pos] != '\n' && e->p[e->pos] != '\r') {
        char c = e->p[e->pos];
        if (c == ' ' || c == '\t') {
            e->pos++;
            continue;
        }
        if (e_comment_at(e)) {
            e_skip_to_eol(e);
            break;
        }
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
    int blank_tab = 0;
    int max_blank_indent = -1;

    while (e->pos < e->len) {
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        int blank = (li.flags & YEP_LF_BLANK) != 0;
        if (!blank) {
            if (saw_content && parent_col < 0 &&
                (li.flags & (YEP_LF_DOC_START | YEP_LF_DOC_END | YEP_LF_DIRECTIVE))) {
                break; /* root block ends at ---/.../% even at the content column */
            }
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
            /* a blank line whose spaces extend past the content indent is
             * CONTENT (kept verbatim); otherwise it is a pure break */
            if (content_indent >= 0 && li.indent > content_indent) {
                if (e->fold_n >= YEP_MAX_FOLD_LINES) {
                    return e_fail(e, YEP_ERR_MEMORY, e->pos);
                }
                e->fold[e->fold_n].content.p = e->p + li.offset + (uint32_t)content_indent;
                e->fold[e->fold_n].content.len =
                    li.end > li.offset + content_indent ? li.end - li.offset - content_indent : 0;
                e->fold[e->fold_n].breaks_before =
                    saw_content ? pending_breaks + 1 : pending_breaks;
                e->fold[e->fold_n].more_indented = 1;
                e->fold_n++;
                saw_content = 1;
                pending_breaks = 0;
                trailing_breaks = 0;
                {
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
                continue;
            }
            if (!saw_content) {
                if (memchr(e->p + li.offset, '\t', li.end - li.offset) != NULL) {
                    blank_tab = 1;
                }
                if ((int)li.indent > max_blank_indent) {
                    max_blank_indent = (int)li.indent;
                }
            }
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
        e->fold[e->fold_n].more_indented =
            li.indent > content_indent ||
            (from < li.end && (e->p[from] == ' ' || e->p[from] == '\t'));
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
    if (saw_content ? (max_blank_indent > content_indent)
                    : (max_blank_indent > parent_col || blank_tab)) {
        /* a leading blank line out-indents the first content line (or,
         * with no content, the parent column) (5LLU/S98Z/W9L4/Y79Y) */
        return e_fail(e, YEP_ERR_BAD_INDENT, e->pos);
    }

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
        if (s0.term == YEP_TERM_COMMENT) {
            e->pos = s0.end;
            e_skip_to_eol(e);
            return 0; /* the comment ends the scalar here */
        }
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
        if (e_comment_at(e)) {
            e_skip_to_eol(e);
            break; /* a comment terminates the plain scalar */
        }
        if (e->pos >= e->len) {
            break;
        }
        breaks++;
        e_line_done(e, e->pos);
        yep_line_info li = yep_scan_line(e->p, e->len, e->pos);
        if (li.flags & YEP_LF_BLANK) {
            /* Park at the line end: the loop's next break consumption
             * counts this blank line's own break. */
            e->pos = li.end;
            continue;
        }
        /* At document root there is no parent block, so continuation
         * lines may sit at column 0. */
        int too_shallow = root_ctx ? (li.indent < 0) : (li.indent <= block_floor);
        if (too_shallow || (li.flags & (YEP_LF_DOC_START | YEP_LF_DOC_END))) {
            break; /* a directive-LIKE line is content (XLQ9) */
        }
        size_t cs = e->pos + li.indent;
        while (cs < e->len && (e->p[cs] == ' ' || e->p[cs] == '\t')) {
            cs++; /* continuation leading white space is stripped */
        }
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
            break; /* the comment ends the scalar; later lines are new */
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
static int e_hex_val(unsigned char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

/* Tags carry URI escapes (%XX) in every form (verbatim, !!, %TAG). */
static yep_view e_tag_uri_decode(yep_engine* e, yep_view v) {
    const char* p = v.p;
    for (uint32_t i = 0; i + 2 < v.len; i++) {
        if (p[i] == '%' && e_hex_val((unsigned char)p[i + 1]) >= 0 &&
            e_hex_val((unsigned char)p[i + 2]) >= 0) {
            char* out = yep_pool_alloc(e->pool, v.len, 16);
            if (out == NULL) {
                return v;
            }
            uint32_t o = 0;
            for (uint32_t j = 0; j < v.len;) {
                if (j + 2 < v.len && p[j] == '%' && e_hex_val((unsigned char)p[j + 1]) >= 0 &&
                    e_hex_val((unsigned char)p[j + 2]) >= 0) {
                    out[o++] = (char)(e_hex_val((unsigned char)p[j + 1]) * 16 +
                                      e_hex_val((unsigned char)p[j + 2]));
                    j += 3;
                } else {
                    out[o++] = p[j++];
                }
            }
            yep_view d = {out, o};
            return d;
        }
    }
    return v;
}

static yep_view e_resolve_tag_raw(yep_engine* e, yep_view tag) {
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
    if (tag.len >= 2 && tag.p[0] == '!' && tag.p[1] == '!' && best < 0) {
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

static yep_view e_resolve_tag(yep_engine* e, yep_view tag) {
    return e_tag_uri_decode(e, e_resolve_tag_raw(e, tag));
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
    /* note: document markers inside a flow collection are checked by the
     * main loop (they end the line scan); quoted spans reject them in
     * e_quoted_floor. */
    for (;;) {
        while (e->pos < e->len && (e->p[e->pos] == ' ' || e->p[e->pos] == '\t')) {
            e->pos++;
        }
        if (e->pos < e->len && e_comment_at(e)) {
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
        if (c == '#' && e_comment_at(e)) {
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
    (void)keyish;
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
    if (!yep_view_is_empty(anchor)) {
        anchor_define(e, anchor);
    }
    e_flow_ws(e);
    c = (unsigned char)e->p[e->pos];
    if ((c == ',' || c == ']' || c == '}' || c == ':') &&
        (!yep_view_is_empty(anchor) || !yep_view_is_empty(tag))) {
        ev->implicit = 1; /* properties with no node: a tagged null */
        return 1;
    }
    if (c == '"' || c == '\'') {
        ev->implicit = 0;
        return e_quoted_floor(e, ev, 0, 0) == 0 ? 1 : -1;
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
    int crossed_break = 0;
    for (;;) {
        yep_span s = yep_scan_plain(e->p, e->len, e->pos, 1);
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
                break; /* a comment ends the scalar: the next token is new */
            }
            e_line_done(e, e->pos);
            crossed_break = 1;
            {
                yep_line_info cl = yep_scan_line(e->p, e->len, e->pos);
                if (cl.flags & YEP_LF_COMMENT) {
                    e_line_done(e, cl.end);
                    break; /* a comment line ends the scalar too */
                }
            }
            e_flow_ws(e);
            if (e->pos < e->len && e->p[e->pos] != ']' && e->p[e->pos] != '}' &&
                e->p[e->pos] != ',' && !e_comment_at(e) && e->p[e->pos] != '\n' &&
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
    if (e->fold_n == 1 && e->fold[0].content.len == 1 && e->fold[0].content.p[0] == '-') {
        return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "[-, -]": bare dash entry */
    }
    ev->style = YEP_STYLE_PLAIN;
    ev->implicit = 1;
    ev->multiline = (e->fold_n > 1) || crossed_break;
    return 1;
}

/* Iterative flow kernel; emits events for the whole collection. */
static int e_flow(yep_engine* e, yep_view anchor, yep_view tag) {
    struct {
        uint8_t kind;        /* 0 seq, 1 map */
        uint8_t pending_key; /* map: 1 key seen awaiting ':' value; 2 key emitted w/o ':' */
        uint8_t sep;         /* a ',' was consumed and no entry followed yet */
        uint8_t just_closed; /* a nested collection closed here (key candidate) */
        uint8_t pair_open;   /* seq: a buffered single-pair awaits its MAP_END */
        uint8_t pair_value;  /* seq: the open pair already has its value */
        uint8_t pair_wrap;   /* map frame pushed as a seq single-pair */
        int32_t buf_from;    /* seq: entry events buffered from this index */
        uint32_t entry_line; /* seq: line where the buffered entry ended */
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
    st[n].just_closed = 0;
    st[n].pair_open = 0;
    st[n].buf_from = -1;
    st[n].entries = 0;
    n++;

    for (;;) {
        e_event_init(&ev, YEP_EV_NONE);
        if (st[n - 1].kind == 0 && st[n - 1].buf_from < 0 && st[n - 1].pair_open == 0 &&
            e->pos < e->len && e->p[e->pos] != ',' && e->p[e->pos] != ']' && e->p[e->pos] != '}' &&
            e->p[e->pos] != ':') {
            /* a sequence entry may become a single-pair key: defer it */
            st[n - 1].buf_from = (int32_t)e->ev_buf_n;
            st[n - 1].entry_line = e->line;
            e->ev_scopes++;
        }
        int keyish = (st[n - 1].kind == 1 && st[n - 1].pending_key == 0);
        int q_key = 0;
        int rc;
        e_flow_ws(e);
        if (e->pos < e->len && e->pos == e->line_start) {
            yep_line_info fl = yep_scan_line(e->p, e->len, e->line_start);
            if (fl.flags & (YEP_LF_DOC_START | YEP_LF_DOC_END | YEP_LF_DIRECTIVE)) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* marker inside flow */
            }
            if (e->flow_enforce && !(fl.flags & (YEP_LF_BLANK | YEP_LF_COMMENT)) &&
                fl.indent <= e->flow_floor) {
                /* a block-level flow's lines out-indent its parent (9C9N) */
                return e_fail(e, YEP_ERR_BAD_INDENT, e->pos + fl.indent);
            }
        }
        if (e->pos < e->len && e->p[e->pos] != ',' && e->p[e->pos] != ']' && e->p[e->pos] != '}' &&
            !(e->p[e->pos] == ':' &&
              ((st[n - 1].kind == 1 && st[n - 1].pending_key == 1) || st[n - 1].just_closed)) &&
            st[n - 1].entries > 0 && st[n - 1].sep == 0 && !st[n - 1].pair_open &&
            !(st[n - 1].kind == 1 && st[n - 1].pending_key == 1)) {
            if (getenv("YEP_TRACE"))
                fprintf(stderr, "[mc] pos=%c kind=%d ent=%u sep=%u pk=%u jc=%u\n", e->p[e->pos],
                        st[n - 1].kind, st[n - 1].entries, st[n - 1].sep, st[n - 1].pending_key,
                        st[n - 1].just_closed);
            /* a node without a preceding ',' or ':' (CML9 missing comma);
             * in a sequence a ':'-led token is a new plain, so it errors */
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
        }
        if (e->pos < e->len && e->p[e->pos] == '?' &&
            (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r' ||
             yep_ct_is((unsigned char)e->p[e->pos + 1], YEP_CT_FLOW_IND))) {
            e->pos++;
            e_flow_ws(e);
            if (e->pos >= e->len ||
                (e->p[e->pos] == ':' &&
                 (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
                  e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r' ||
                  yep_ct_is((unsigned char)e->p[e->pos + 1], YEP_CT_FLOW_IND) ||
                  e->p[e->pos + 1] == ',' || e->p[e->pos + 1] == ']' || e->p[e->pos + 1] == '}')) ||
                e->p[e->pos] == ',' || e->p[e->pos] == ']' || e->p[e->pos] == '}') {
                /* '? : v' / '?' at the boundary: an EMPTY explicit key */
                e_event_init(&ev, YEP_EV_SCALAR);
                ev.implicit = 1;
                ev.line = e->line;
                ev.col = e_col(e, e->pos) + 1;
                rc = 1;
                q_key = 1;
            } else {
                rc = e_flow_node(e, &ev, keyish);
                q_key = 1;
            }
        } else {
            rc = e_flow_node(e, &ev, keyish);
        }
        if (rc == -1) {
            return -1;
        }
        if (rc == 2) {
            if (n >= YEP_MAX_DEPTH) {
                return e_fail(e, YEP_ERR_DEPTH, e->pos);
            }
            st[n - 1].pair_value = 1;
            unsigned char oc = (unsigned char)e->p[e->pos];
            e->pos++;
            yep_view pa = ev.anchor, pt = ev.tag; /* props before the opener */
            e_event_init(&ev, oc == '[' ? YEP_EV_SEQ_START : YEP_EV_MAP_START);
            ev.flow = 1;
            ev.anchor = pa;
            ev.tag = pt;
            ev.line = e->line;
            ev.col = e_col(e, e->pos);
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            st[n - 1].just_closed = 0;
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
            st[n].pair_open = 0;
            st[n].pair_value = 0;
            st[n].pair_wrap = 0;
            st[n].buf_from = -1;
            st[n].entries = 0;
            n++;
            continue;
        }
        if (rc == 1) {
            st[n - 1].sep = 0;
            st[n - 1].just_closed = 0;
            st[n - 1].entries++;
            if (st[n - 1].buf_from >= 0) {
                st[n - 1].entry_line = e->line; /* node end (pre-colon ws) */
            }
            if (st[n - 1].pair_open) {
                st[n - 1].pair_value = 1; /* the pair's value arrived */
            }
            /* key detection: ':' after the node (spaces and line breaks
             * may precede it: "unquoted : value", "\"foo\"\n: bar").
             * After a QUOTED node the colon separates JSON-style. */
            size_t save_pos = e->pos;
            e_flow_ws(e); /* crosses newlines; restored below when no colon */
            int quoted = (ev.type == YEP_EV_SCALAR && (ev.style == YEP_STYLE_SINGLE_QUOTED ||
                                                       ev.style == YEP_STYLE_DOUBLE_QUOTED));
            int key_pos = (st[n - 1].kind == 1 && st[n - 1].pending_key == 0);
            int cross_line = 0;
            for (size_t k = save_pos; k < e->pos; k++) {
                if (e->p[k] == '\n' || e->p[k] == '\r') {
                    cross_line = 1;
                    break;
                }
            }
            int colon = (e->pos < e->len && e->p[e->pos] == ':' &&
                         ((quoted && !ev.multiline && (!cross_line || st[n - 1].kind == 1)) ||
                          (key_pos && !ev.multiline && !cross_line) || e->pos + 1 >= e->len ||
                          e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
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
                if (st[n - 1].buf_from >= 0 && !quoted && !q_key &&
                    (save_pos < e->line_start || ev.multiline)) {
                    /* "[a\n: b]": an IMPLICIT key cannot span lines ("? " may) */
                    return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
                }
                /* single-pair mapping inside a sequence: the entry's
                 * events are buffered; wrap them in MAP_START .. MAP_END */
                e->pos++;
                {
                    int32_t bf = st[n - 1].buf_from;
                    if (bf >= 0) {
                        st[n - 1].buf_from = -1;
                        e->ev_scopes--;
                        if (e_buf_wrap(e, (uint32_t)bf) != 0) {
                            return -2;
                        }
                        if (e->ev_scopes == 0 && e->ev_buf_n > 0) {
                            int brc = e_buf_send(e, 0);
                            if (brc != 0) {
                                return brc;
                            }
                        }
                        if (emit_now(e, &ev) != 0) {
                            return -2;
                        }
                        st[n - 1].pair_open = 1;
                        continue;
                    } else {
                        yep_event mk;
                        e_event_init(&mk, YEP_EV_MAP_START);
                        mk.flow = 1;
                        mk.line = ev.line;
                        mk.col = ev.col;
                        if (emit_now(e, &mk) != 0) {
                            return -2;
                        }
                        if (emit_now(e, &ev) != 0) {
                            return -2;
                        }
                    }
                    st[n - 1].pair_open = 1;
                }
                continue; /* the value parses as the next regular node */
            }
            if (q_key && st[n - 1].kind == 0) {
                /* "[? x]": an explicit pair with no ':' — null value */
                int32_t bf = st[n - 1].buf_from;
                if (bf >= 0) {
                    st[n - 1].buf_from = -1;
                    e->ev_scopes--;
                    if (e_buf_wrap(e, (uint32_t)bf) != 0) {
                        return -2;
                    }
                    st[n - 1].pair_open = 1;
                    if (e->ev_scopes == 0 && e->ev_buf_n > 0) {
                        int brc = e_buf_send(e, 0);
                        if (brc != 0) {
                            return brc;
                        }
                    }
                    yep_event nv;
                    e_event_init(&nv, YEP_EV_SCALAR);
                    nv.implicit = 1;
                    if (emit_now(e, &nv) != 0) {
                        return -2;
                    }
                    continue;
                }
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
            if (st[n - 1].pair_open) {
                if (!st[n - 1].pair_value) {
                    yep_event nv;
                    e_event_init(&nv, YEP_EV_SCALAR);
                    nv.implicit = 1;
                    if (emit_now(e, &nv) != 0) {
                        return -2;
                    }
                }
                yep_event mk;
                e_event_init(&mk, YEP_EV_MAP_END);
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                st[n - 1].pair_open = 0;
                st[n - 1].pair_value = 0;
            }
            if (st[n - 1].buf_from >= 0) {
                st[n - 1].buf_from = -1;
                e->ev_scopes--;
                if (e->ev_scopes == 0 && e->ev_buf_n > 0) {
                    int brc = e_buf_send(e, 0);
                    if (brc != 0) {
                        return brc;
                    }
                }
            }
            if (st[n - 1].sep) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "{,x}" / "[1,,2]" */
            }
            if (st[n - 1].entries == 0 && !st[n - 1].pair_open) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "[, a]" */
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
            st[n - 1].just_closed = 0;
            continue;
        }
        if (c == ']' || c == '}') {
            while (st[n - 1].pair_wrap) {
                /* a single-pair wrapper inside a sequence closes on any
                 * bracket ("[ : v ]", CFD4) */
                if (st[n - 1].pending_key != 0) {
                    yep_event nv;
                    e_event_init(&nv, YEP_EV_SCALAR);
                    nv.implicit = 1;
                    if (emit_now(e, &nv) != 0) {
                        return -2;
                    }
                }
                yep_event mk;
                e_event_init(&mk, YEP_EV_MAP_END);
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                st[n - 1].pair_wrap = 0;
                n--;
                st[n - 1].just_closed = 1;
                st[n - 1].entry_line = e->line;
            }
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
            if (st[n - 1].pair_open) {
                if (!st[n - 1].pair_value) {
                    yep_event nv;
                    e_event_init(&nv, YEP_EV_SCALAR);
                    nv.implicit = 1;
                    if (emit_now(e, &nv) != 0) {
                        return -2;
                    }
                }
                yep_event mk;
                e_event_init(&mk, YEP_EV_MAP_END);
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                st[n - 1].pair_open = 0;
                st[n - 1].pair_value = 0;
            }
            if (st[n - 1].buf_from >= 0) {
                st[n - 1].buf_from = -1;
                e->ev_scopes--;
                if (e->ev_scopes == 0 && e->ev_buf_n > 0) {
                    int brc = e_buf_send(e, 0);
                    if (brc != 0) {
                        return brc;
                    }
                }
            }
            e->pos++;
            e_event_init(&ev, st[n - 1].kind ? YEP_EV_MAP_END : YEP_EV_SEQ_END);
            if (emit_now(e, &ev) != 0) {
                return -2;
            }
            n--;
            if (n == 0) {
                if (e->ev_scopes != 0) {
                    return e_fail(e, YEP_ERR_INTERNAL, e->pos);
                }
                if (e->ev_buf_n > 0) {
                    int brc = e_buf_send(e, 0);
                    if (brc != 0) {
                        return brc;
                    }
                }
                e_skip_inline_space(e);
                return 0;
            }
            st[n - 1].just_closed = 1;
            if (st[n - 1].pair_open) {
                st[n - 1].pair_value = 1;
            }
            if (st[n - 1].buf_from >= 0) {
                st[n - 1].entry_line = e->line; /* the nested key ended here */
            }
            continue;
        }
        if (c == ':') {
            e->pos++;
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 2) {
                /* ':' after a bare key completes it (value next) */
                st[n - 1].pending_key = 1;
                continue;
            }
            if (st[n - 1].kind == 1 && st[n - 1].just_closed) {
                /* '[a, b]: v' inside a map — the closed collection is the key */
                st[n - 1].just_closed = 0;
                st[n - 1].pending_key = 1;
                continue;
            }
            if (st[n - 1].kind == 1 && st[n - 1].pending_key == 0 &&
                (st[n - 1].sep || st[n - 1].entries == 0)) {
                /* '{: x' or '{a, : x' — empty key at a fresh entry */
                yep_event kv;
                e_event_init(&kv, YEP_EV_SCALAR);
                kv.implicit = 1;
                if (emit_now(e, &kv) != 0) {
                    return -2;
                }
                st[n - 1].pending_key = 1;
                st[n - 1].entries = 1;
                continue;
            }
            if (st[n - 1].kind == 0 && st[n - 1].buf_from >= 0) {
                if (st[n - 1].entry_line != e->line) {
                    /* the key ended on an earlier line */
                    return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
                }
                /* '[ [a, b]: v' — the buffered collection is the pair key */
                int32_t bf0 = st[n - 1].buf_from;
                int was_empty = ((uint32_t)bf0 == e->ev_buf_n);
                st[n - 1].buf_from = -1;
                e->ev_scopes--;
                if (e_buf_wrap(e, (uint32_t)bf0) != 0) {
                    return -2;
                }
                if (e->ev_scopes == 0 && e->ev_buf_n > 0) {
                    int brc = e_buf_send(e, 0);
                    if (brc != 0) {
                        return brc;
                    }
                }
                if (was_empty) {
                    yep_event kv; /* "[ : v" — the empty key */
                    e_event_init(&kv, YEP_EV_SCALAR);
                    kv.implicit = 1;
                    if (emit_now(e, &kv) != 0) {
                        return -2;
                    }
                }
                st[n - 1].pair_open = 1;
                continue; /* value next */
            }
            if (st[n - 1].kind == 0 && n >= 1) {
                /* '[ : v' — single-pair mapping with an empty key */
                if (n >= YEP_MAX_DEPTH) {
                    return e_fail(e, YEP_ERR_DEPTH, e->pos);
                }
                yep_event mk;
                e_event_init(&mk, YEP_EV_MAP_START);
                mk.flow = 1;
                if (emit_now(e, &mk) != 0) {
                    return -2;
                }
                yep_event kv;
                e_event_init(&kv, YEP_EV_SCALAR);
                kv.implicit = 1;
                if (emit_now(e, &kv) != 0) {
                    return -2;
                }
                st[n - 1].sep = 0;
                st[n - 1].entries++;
                st[n].kind = 1;
                st[n].pending_key = 1;
                st[n].sep = 0;
                st[n].pair_open = 0;
                st[n].pair_value = 0;
                st[n].pair_wrap = 0;
                st[n].pair_wrap = 1;
                st[n].buf_from = -1;
                st[n].entries = 0;
                n++;
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
        /* (ctx stays: AFTER_COLON converts to VALUE_LINE at the break) */
    }

    unsigned char c = (unsigned char)e->p[e->pos];

    if (c == ',' && (!yep_view_is_empty(anchor) || !yep_view_is_empty(tag) ||
                     !yep_view_is_empty(pend_a) || !yep_view_is_empty(pend_t))) {
        return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "!!str, xxx" (block) */
    }
    if (c == '-' && (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
                     e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
        if (!yep_view_is_empty(anchor) || !yep_view_is_empty(tag)) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "&a - x" */
        }
        uint16_t col = e_col(e, e->pos);
        int rc = e_open_seq(e, col, e->line, col + 1, node_a, node_t);
        if (rc != 0) {
            return rc;
        }
        e->pos++; /* '-' */
        return e_parse_value(e, YEP_CTX_AFTER_DASH, col);
    }

    if (c == '?' && (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
                     e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
        if (ctx == YEP_CTX_AFTER_COLON) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "key: ? a" */
        }
        uint16_t col = e_col(e, e->pos);
        int rc = e_open_map(e, col, e->line, col + 1, pend_a, pend_t);
        if (rc != 0) {
            return rc;
        }
        e->q_map_depth = e->depth;
        e->pos++;
        e_skip_inline_space(e);
        if (e->pos < e->len && e->p[e->pos] == ':' &&
            (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
            size_t after = e->pos + 1;
            while (after < e->len && (e->p[after] == ' ' || e->p[after] == '\t')) {
                after++;
            }
            if (after >= e->len || e->p[after] == '\n' || e->p[after] == '\r' ||
                (e->p[after] == '#' && after > e->pos + 1)) {
                /* '? :' at end of line — empty explicit key */
                yep_event kv;
                e_event_init(&kv, YEP_EV_SCALAR);
                kv.style = YEP_STYLE_PLAIN;
                kv.implicit = 1;
                if (emit_now(e, &kv) != 0) {
                    return -2;
                }
                e->pos++;
                return e_parse_value(e, YEP_CTX_AFTER_COLON, col);
            }
            /* '? : x' — the ':' starts a nested pair: the KEY is that map */
        }
        /* the explicit key is parsed as a value (may be empty → null);
         * its content may ALIGN with the '?' column */
        e->q_key_pending = 1;
        {
            int rc = e_parse_value(e, YEP_CTX_AFTER_Q, col);
            if (rc != 0) {
                return rc;
            }
            e->q_value_pending = 1; /* until a ':' line supplies the value */
            return 0;
        }
    }

    if (c == '[' || c == '{') {
        /* pre-scan: is this flow collection a mapping KEY? (a key must
         * begin and end on one line — "[23\n]: 42" is not a key) */
        size_t save = e->pos;
        uint32_t line0 = e->line;
        if (e_skip_flow(e) != 0) {
            return -1;
        }
        e_skip_inline_space(e);
        int is_key = e_colon_at(e, e->pos) && e->line == line0;
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
        {
            e->flow_floor = floor_col;
            e->flow_enforce = (e->depth > 0);
            int rc = e_flow(e, node_a, node_t);
            e->flow_enforce = 0;
            if (rc != 0) {
                return rc;
            }
            if (!e_at_eol(e)) {
                return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "[ a ] ]" */
            }
            if (e->depth == 0) {
                e->last_root_flow = 1; /* "[23\n]: 42" is not a key */
            }
            return 0;
        }
    }

    if (c == '"' || c == '\'') {
        if (e_quoted_floor(e, &ev, floor_col, e->depth > 0) != 0) {
            return -1;
        }
        if (e_colon_at(e, e->pos)) {
            if (ctx == YEP_CTX_AFTER_COLON || ev.multiline) {
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
        if (!yep_view_is_empty(anchor) || !yep_view_is_empty(tag)) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "&a *b" */
        }
        if (e_alias(e, &ev) != 0) {
            return -1;
        }
        if (e_colon_at(e, e->pos)) {
            if (ctx == YEP_CTX_AFTER_COLON || ev.multiline) {
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
        int parent_col = (e->depth == 0) ? -1 : (int)floor_col;
        if (e_block_scalar(e, &ev, parent_col) != 0) {
            return -1;
        }
        return emit_now(e, &ev) == 0 ? 0 : -2;
    }

    /* plain scalar */
    size_t start = e->pos;
    yep_span s = yep_scan_plain(e->p, e->len, e->pos, 0);
    if (s.term != YEP_TERM_COLON && !yep_view_is_empty(pend_a) && !yep_view_is_empty(anchor)) {
        /* two anchors on ONE scalar ("&a
&b v"); a colon means the
         * inline anchor belongs to the key and the pend one elsewhere */
        return e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
    }
    if (s.term != YEP_TERM_COLON && ctx == YEP_CTX_FRESH && e->depth > 0 &&
        e_col(e, node_at) == e->frames[e->depth - 1].col) {
        /* a plain scalar at an open container's column with no ':' —
         * neither a mapping key nor a sequence entry */
        return e_fail(e, YEP_ERR_UNEXPECTED, s.end);
    }
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
        /* advance from the TRIMMED span end (not the stale pre-scan
         * position — the first ':' in the buffer may be INSIDE the key);
         * spaces may still precede the terminating colon */
        e->pos = s.end;
        while (e->pos < e->len && e->p[e->pos] != ':') {
            e->pos++;
        }
        e->pos++; /* past ':' */
        return e_parse_value(e, YEP_CTX_AFTER_COLON, key_col);
    }

    ev.value.p = e->p + s.start;
    ev.value.len = s.end - s.start;
    ev.borrowed = 1;
    ev.style = YEP_STYLE_PLAIN;
    ev.implicit = 1;
    e->pos = s.end;
    if (e_plain_multiline(e, start, floor_col, &ev, e->depth == 0) != 0) {
        return -1;
    }
    if (e->fold_n == 1 && s.term == YEP_TERM_COMMENT) {
        e_skip_to_eol(e); /* single line ending in a comment */
    }
    return emit_now(e, &ev) == 0 ? 0 : -2;
}

/* Value after "key:" / "-" / "?": on this line, or on following lines. */
static int e_parse_value(yep_engine* e, yep_ctx ctx, uint16_t floor_col) {
    e_skip_inline_space(e);
    if (!e_at_eol(e)) {
        if (ctx == YEP_CTX_AFTER_COLON && e->p[e->pos] == '-' &&
            (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos); /* "key: - x" */
        }
        if (ctx == YEP_CTX_AFTER_DASH && e->p[e->pos] == ':' &&
            (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
            /* "- :" — a compact pair with an empty key */
            uint16_t col = e_col(e, e->pos);
            int rc = e_open_map(e, col, e->line, col + 1, e->pend_anchor, e->pend_tag);
            if (rc != 0) {
                return rc;
            }
            e->pend_anchor.p = NULL;
            e->pend_anchor.len = 0;
            e->pend_tag.p = NULL;
            e->pend_tag.len = 0;
            yep_event kv;
            e_event_init(&kv, YEP_EV_SCALAR);
            kv.implicit = 1;
            if (emit_now(e, &kv) != 0) {
                return -2;
            }
            e->pos++;
            return e_parse_value(e, YEP_CTX_AFTER_COLON, col);
        }
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
        if (li.flags & YEP_LF_BLANK) {
            e_line_done(e, li.end);
            continue;
        }
        if (li.flags & YEP_LF_COMMENT) {
            e_line_done(e, li.end);
            continue;
        }
        if (li.flags & YEP_LF_TAB) {
            return e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
        }
        if (li.indent == floor_col && e->depth > 0 &&
            (ctx == YEP_CTX_AFTER_COLON || ctx == YEP_CTX_VALUE_LINE) &&
            (li.first == '&' || li.first == '!')) {
            /* properties at the parent column are a new node, not a value */
            return e_fail(e, YEP_ERR_UNEXPECTED, e->pos + li.indent);
        }
        yep_ctx vctx = (ctx == YEP_CTX_AFTER_COLON) ? YEP_CTX_VALUE_LINE : ctx;
        /* At document root there is no parent block: a following line
         * at any indent is the value ("&a\n- x"). Explicit-key content
         * may align with the '?' column. Flow nodes and property runs
         * continue the value even at the parent column ("k:\n!!seq\n[a]"). */
        if (li.indent > floor_col || e->depth == 0 ||
            (ctx == YEP_CTX_AFTER_Q && li.indent == floor_col && li.first != ':') ||
            ((li.first == '[' || li.first == '{') && li.indent == floor_col)) {
            e->pos += li.indent;
            return e_node(e, vctx, floor_col);
        }
        /* indentless sequence as a mapping value; under a sequence frame
         * a dash at the same column is a SIBLING, not a continuation */
        if (li.indent == floor_col && li.first == '-' &&
            ((ctx == YEP_CTX_AFTER_DASH && e->depth > 0 &&
              e->frames[e->depth - 1].kind == YEP_FRAME_MAP &&
              e->frames[e->depth - 1].col == floor_col) ||
             vctx == YEP_CTX_VALUE_LINE) &&
            (li.offset + li.indent + 1 >= e->len || e->p[li.offset + li.indent + 1] == ' ' ||
             e->p[li.offset + li.indent + 1] == '\t' || e->p[li.offset + li.indent + 1] == '\n' ||
             e->p[li.offset + li.indent + 1] == '\r')) {
            e->pos += li.indent;
            return e_node(e, ctx == YEP_CTX_AFTER_DASH ? ctx : vctx, floor_col);
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
    yep_free(e->sys, e->norm_buf);
    if (e == NULL) {
        return;
    }
    yep_pool_destroy(e->pool);
    yep_free(e->sys, e);
}

/* YAML 1.2 line breaks beyond \n/\r: NEL, LS, PS. When present, the
 * input is copied with each normalized to a single '\n' (libyaml reader
 * behavior); inputs without them keep the zero-copy path. */
static const char* e_normalize_breaks(yep_engine* e, const char* p, size_t len) {
    const unsigned char* q = (const unsigned char*)p;
    size_t i = 0;
    int found = 0;
    while (i < len) {
        if (q[i] == 0xC2 && i + 1 < len && q[i + 1] == 0x85) {
            found = 1;
            break;
        }
        if (q[i] == 0xE2 && i + 2 < len && q[i + 1] == 0x80 &&
            (q[i + 2] == 0xA8 || q[i + 2] == 0xA9)) {
            found = 1;
            break;
        }
        i++;
    }
    if (!found) {
        return p;
    }
    char* out = yep_alloc(e->sys, len + 1);
    if (out == NULL) {
        return p; /* fall back: parse as-is */
    }
    size_t o = 0;
    for (size_t j = 0; j < len;) {
        if (q[j] == 0xC2 && j + 1 < len && q[j + 1] == 0x85) {
            out[o++] = '\n';
            j += 2;
        } else if (q[j] == 0xE2 && j + 2 < len && q[j + 1] == 0x80 &&
                   (q[j + 2] == 0xA8 || q[j + 2] == 0xA9)) {
            out[o++] = '\n';
            j += 3;
        } else {
            out[o++] = p[j++];
        }
    }
    out[o] = '\0';
    e->norm_buf = out;
    return out;
}

int yep_engine_run(yep_engine* e, const char* buf, size_t len, const yep_sink* sink) {
    if (e == NULL || (buf == NULL && len != 0)) {
        return -1;
    }
    yep_free(e->sys, e->norm_buf);
    e->norm_buf = NULL;
    e->p = e_normalize_breaks(e, buf, len);
    e->len = (e->norm_buf != NULL) ? strlen(e->norm_buf) : len;
    e->pos = 0;
    e->line = 1;
    e->line_start = 0;
    e->depth = 0;
    e->anchor_count = 0;
    e->tagmap_n = 0;
    e->saw_yaml = 0;
    e->doc_content = 0;
    e->q_key_pending = 0;
    e->q_value_pending = 0;
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
            size_t t = li.offset + li.indent;
            while (t < li.end && (e->p[t] == ' ' || e->p[t] == '\t')) {
                t++;
            }
            if (t >= li.end || (e->p[t] != '[' && e->p[t] != '{')) {
                e_fail(e, YEP_ERR_TAB_IN_INDENT, e->pos + li.indent);
                goto fail;
            }
            li.indent = (uint16_t)(t - li.offset);
        }
        if (li.flags & YEP_LF_DIRECTIVE) {
            if (doc_open && e->doc_content) {
                e_fail(e, YEP_ERR_BAD_DIRECTIVE, e->pos);
                goto fail;
            }
            if (e->saw_yaml) {
                e_fail(e, YEP_ERR_BAD_DIRECTIVE, e->pos); /* repeated %YAML */
                goto fail;
            }
            {
                /* %YAML takes exactly one version argument */
                size_t chk = li.offset + li.indent + 1;
                if (li.end - chk >= 4 && memcmp(e->p + chk, "YAML", 4) == 0) {
                    e->saw_yaml = 1;
                    size_t a0 = chk + 4;
                    while (a0 < li.end && e->p[a0] == ' ') {
                        a0++;
                    }
                    size_t a1 = a0;
                    int dots = 0;
                    while (a1 < li.end &&
                           ((e->p[a1] >= '0' && e->p[a1] <= '9') || e->p[a1] == '.')) {
                        dots += (e->p[a1] == '.');
                        a1++;
                    }
                    size_t rest = a1;
                    while (rest < li.end && e->p[rest] == ' ') {
                        rest++;
                    }
                    if (rest < li.end && e->p[rest] == '#' && rest > a1) {
                        rest = li.end; /* trailing comment (blank before #) */
                    }
                    if (a1 == a0 || dots != 1 || rest < li.end || e->p[a0] == '.' ||
                        e->p[a1 - 1] == '.') {
                        e_fail(e, YEP_ERR_BAD_DIRECTIVE, chk);
                        goto fail;
                    }
                }
            }
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
                if (!e->doc_content) {
                    e_event_init(&ev, YEP_EV_SCALAR);
                    ev.implicit = 1;
                    if (emit_now(e, &ev) != 0) {
                        return -2;
                    }
                }
                e->doc_content = 0;
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
            e->saw_yaml = 0;
            e->last_root_flow = 0; /* a new document may carry its own directives */
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
            {
                size_t rest = li.offset + li.indent + 3;
                while (rest < e->len &&
                       (e->p[rest] == ' ' || e->p[rest] == '\t' || e->p[rest] == '\r')) {
                    rest++;
                }
                if (rest < e->len && e->p[rest] != '\n' && e->p[rest] != '#') {
                    e_fail(e, YEP_ERR_UNEXPECTED, rest);
                    goto fail;
                }
            }
            int rc = e_close_to(e, 0);
            if (rc != 0) {
                return rc;
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
                ev.style = 1;    /* explicit "..." marker */
                e->tagmap_n = 0; /* directives are per-document */
                if (emit_now(e, &ev) != 0) {
                    return -2;
                }
                doc_open = 0;
                e->doc_content = 0;
                e->saw_yaml = 0;
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
                                                     e->p[li.offset + li.indent + 1] == '\t' ||
                                                     e->p[li.offset + li.indent + 1] == '\n' ||
                                                     e->p[li.offset + li.indent + 1] == '\r'));
                } else {
                    continues = yep_scan_is_key_start(li.first) || li.first == ':' ||
                                li.first == '?' ||
                                (li.first == '-' && !(li.offset + li.indent + 1 >= e->len ||
                                                      e->p[li.offset + li.indent + 1] == ' ' ||
                                                      e->p[li.offset + li.indent + 1] == '\t' ||
                                                      e->p[li.offset + li.indent + 1] == '\n' ||
                                                      e->p[li.offset + li.indent + 1] == '\r'));
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

        if (e->depth == 0 && e->doc_content) {
            /* a completed root scalar followed by more content: a second
             * root node in the same document ("word1  # c\nword2") */
            e_fail(e, YEP_ERR_UNEXPECTED, e->pos + li.indent);
            goto fail;
        }
        if (e->depth > 0 && e->frames[e->depth - 1].col < c) {
            /* deeper than any open container at main dispatch: nested
             * values are consumed inside e_parse_value, so this line is
             * an orphan continuation ("key: word1\n# c\n  word2") */
            e_fail(e, YEP_ERR_BAD_INDENT, e->pos + li.indent);
            goto fail;
        }
        e->pos += li.indent;
        unsigned char first = (unsigned char)e->p[e->pos];
        if (first == ':' &&
            (e->pos + 1 >= e->len || e->p[e->pos + 1] == ' ' || e->p[e->pos + 1] == '\t' ||
             e->p[e->pos + 1] == '\n' || e->p[e->pos + 1] == '\r')) {
            /* explicit-key value line, or a bare ':' pair (empty key) */
            int rc;
            if (e->last_root_flow) {
                e_fail(e, YEP_ERR_UNEXPECTED, e->pos);
                goto fail; /* a root flow node cannot become a key */
            }
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
            e->q_value_pending = 0;
            e->pos++;
            rc = e_parse_value(e, YEP_CTX_AFTER_DASH, e->frames[e->depth - 1].col);
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
            e->last_root_flow = 0;
            int rc = e_flush_q_value(e); /* a key without its ':' line */
            if (rc != 0) {
                return -2;
            }
            rc = e_node(e, YEP_CTX_FRESH, e->depth > 0 ? e->frames[e->depth - 1].col : 0);
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

    if ((e->tagmap_n > 0 || e->saw_yaml) && !doc_open) {
        /* a directive was never followed by "---" */
        e_fail(e, YEP_ERR_BAD_DIRECTIVE, e->len);
        goto fail;
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
