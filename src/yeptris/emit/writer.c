/* writer.c — the single writer engine (TODO.impl/13).
 *
 * One recursive emitter drives both sizing (dry: counts only) and
 * output (wet): the size query and the bytes agree by construction.
 *
 * Layout contract (indent = the CONTENT column of the node):
 *   block map value:  "key: <inline>" or "key:\n<indent+2>…"
 *   block seq item:   "- <inline>" (compact: nested block collections
 *                     start on the dash line at indent+2)
 *   flow collections re-emit compact: {k: v, k2: v2} / [a, b]
 *   scalars re-emit their recorded style; plain values that are not
 *   plain-safe in position fall back to double quotes; any multiline
 *   scalar emits as a literal block (deterministic upgrade: parse of
 *   the upgrade records literal, so byte stability holds)
 *   anchors: "&name " before the node; aliases: "*name"
 *   multi-document streams mark every document with "---"
 */

#include "writer.h"

#include <string.h>

#include "style.h"

#include <stdio.h>
#include <stdlib.h>

#include "../../include/yeptris/resolve.h"
#include "float/api.h"
#include "resolve/resolver.h"

/* Streaming flush (13C): append-only output means any high-water
 * crossing is a safe cut point. */
static void wr_maybe_flush(yep_writer* w) {
    if (w->sink == NULL || w->dry || w->len < w->watermark) {
        return;
    }
    if (w->sink(w->sink_ctx, w->p, w->len) != 0) {
        w->sink_aborted = 1;
        return;
    }
    w->flushed += w->len;
    w->len = 0;
}

static void wr_col_update(yep_writer* w, const char* p, uint32_t n) {
    for (uint32_t i = n; i > 0; i--) { /* last newline wins */
        if (p[i - 1] == '\n') {
            w->col = (int)(n - i);
            return;
        }
    }
    w->col += (int)n;
}

static void wr_put(yep_writer* w, const char* p, uint32_t n) {
    if (n > 0) {
        w->last = p[n - 1];
    }
    wr_col_update(w, p, n);
    if (w->dry) {
        w->len += n;
        return;
    }
    memcpy(w->p + w->len, p, n);
    w->len += n;
    wr_maybe_flush(w);
}

static void wr_byte(yep_writer* w, char c) {
    w->last = c;
    if (c == '\n') {
        w->col = 0;
    } else {
        w->col++;
    }
    if (w->dry) {
        w->len++;
        return;
    }
    w->p[w->len] = c;
    w->len++;
    wr_maybe_flush(w);
}

static void wr_indent(yep_writer* w, int depth) {
    for (int i = 0; i < depth; i++) {
        wr_byte(w, ' ');
    }
}

static void emit_dq(yep_writer* w, const char* p, uint32_t n) {
    wr_byte(w, '"');
    for (uint32_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c == '"' || c == '\\') {
            wr_byte(w, '\\');
            wr_byte(w, (char)c);
        } else if (c == '\n') {
            wr_put(w, "\\n", 2);
        } else if (c == '\r') {
            wr_put(w, "\\r", 2);
        } else if (c == '\t') {
            wr_put(w, "\\t", 2);
        } else if (c < 0x20 || c == 0x7f) {
            static const char* named[6] = {"\\0", "\\a", "\\b", "\\v", "\\f", "\\e"};
            static const unsigned char code[6] = {0x00, 0x07, 0x08, 0x0b, 0x0c, 0x1b};
            int k;
            for (k = 0; k < 6; k++) {
                if (code[k] == c) {
                    break;
                }
            }
            if (k < 6) {
                wr_put(w, named[k], 2);
            } else {
                static const char hd[] = "0123456789abcdef";
                char hex[4] = {'\\', 'x', hd[(c >> 4) & 0xf], hd[c & 0xf]};
                wr_put(w, hex, 4);
            }
        } else {
            wr_byte(w, (char)c);
        }
    }
    wr_byte(w, '"');
}

static void emit_sq(yep_writer* w, const char* p, uint32_t n) {
    wr_byte(w, '\'');
    for (uint32_t i = 0; i < n; i++) {
        if (p[i] == '\'') {
            wr_put(w, "''", 2);
        } else {
            wr_byte(w, p[i]);
        }
    }
    wr_byte(w, '\'');
}

/* JSON string escapes: \" \\ \b \f \n \r \t and \uXXXX for other
 * controls; raw UTF-8 passes through (RFC 8259). */
static void emit_dq_json(yep_writer* w, const char* p, uint32_t n) {
    wr_byte(w, '"');
    static const char* named[32] = {
        NULL, NULL,  NULL,  NULL, NULL, NULL, NULL, "\\b", NULL, "\\t", "\\n",
        NULL, "\\f", "\\r", NULL, NULL, NULL, NULL, NULL,  NULL, NULL,  NULL,
        NULL, NULL,  NULL,  NULL, NULL, NULL, NULL, NULL,  NULL, NULL,
    };
    for (uint32_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c < 32 && named[c] != NULL) {
            wr_put(w, named[c], 2);
        } else if (c == '"' || c == '\\') {
            wr_byte(w, '\\');
            wr_byte(w, (char)c);
        } else if (c < 0x20) {
            static const char hd[] = "0123456789abcdef";
            char hex[6] = {'\\', 'u', '0', '0', hd[(c >> 4) & 0xf], hd[c & 0xf]};
            wr_put(w, hex, 6);
        } else {
            wr_byte(w, (char)c);
        }
    }
    wr_byte(w, '"');
}

/* Literal block: header at the cursor, body lines at parent_col+2.
 * The explicit indicator is always '2' because the pad is defined as
 * parent_col+2 everywhere (the contract that keeps it valid). */
static void emit_literal(yep_writer* w, const yep_dnode* n, int parent_col) {
    const char* p = (const char*)n->value.p;
    uint32_t len = n->value.len;
    uint32_t trail = 0;
    while (trail < len && p[len - 1 - trail] == '\n') {
        trail++;
    }
    uint32_t body = len - trail;
    wr_byte(w, '|');
    if (trail == 0) {
        wr_byte(w, '-');
    } else if (trail > 1) {
        wr_byte(w, '+');
    }
    /* Always emit the explicit indicator: the pad is parent_col+2 by
     * contract (keeps more-indented first lines parseable) */
    wr_byte(w, '2');
    wr_byte(w, '\n');
    int pad = parent_col + 2;
    wr_indent(w, pad);
    for (uint32_t i = 0; i < body; i++) {
        wr_byte(w, p[i]);
        if (p[i] == '\n' && i + 1 < body) {
            wr_indent(w, pad);
        }
    }
    for (uint32_t i = 0; i < trail; i++) {
        wr_byte(w, '\n');
    }
}

/* Canonical scalar (13B): typed words and numbers plain, everything
 * else double-quoted; floats re-print through the shortest printer so
 * canonical output is a fixed point (parse of canonical re-prints
 * identically — the byte-stability gate). */
static void emit_canonical_scalar(yep_writer* w, const yep_dnode* n) {
    const char* p = (const char*)n->value.p;
    uint32_t len = n->value.len;
    if (n->tag.len == 0) {
        switch (n->tag_id) {
        case YEPTRIS_TAG_NULL:
            if (w->json) {
                wr_put(w, "null", 4);
            } else {
                wr_put(w, "~", 1);
            }
            return;
        case YEPTRIS_TAG_BOOL:
            if (len == 4) { /* true/TRUE/True/null-ish variants */
                wr_put(w, (p[0] == 't' || p[0] == 'T') ? "true" : "null", 4);
            } else { /* false family (5 chars) */
                wr_put(w, "false", 5);
            }
            return;
        case YEPTRIS_TAG_FLOAT: {
            const char* word = yep_tag_float_word(p, len);
            if (word != NULL) {
                wr_put(w, word, (uint32_t)strlen(word));
                return;
            }
            char buf[40];
            char* end = NULL;
            char tmp[40];
            uint32_t tl = len < sizeof(tmp) - 1 ? len : (uint32_t)sizeof(tmp) - 1;
            memcpy(tmp, p, tl);
            tmp[tl] = '\0';
            double d = strtod(tmp, &end);
            if (end != NULL && *end == '\0' && end != tmp) {
                int wl = yep_d2s_shortest(d, buf);
                wr_put(w, buf, (uint32_t)wl);
                return;
            }
            break; /* unparseable view: fall through to quoting */
        }
        case YEPTRIS_TAG_INT:
            wr_put(w, p, len);
            return;
        default:
            break;
        }
    }
    if (w->json) {
        emit_dq_json(w, p, len);
    } else {
        emit_dq(w, p, len);
    }
}

static void emit_scalar(yep_writer* w, const yep_dnode* n, int parent_col, int as_key) {
    const char* p = (const char*)n->value.p;
    uint32_t len = n->value.len;
    if (w->canonical || w->json) {
        emit_canonical_scalar(w, n);
        return;
    }
    int multiline = (memchr(p, '\n', len) != NULL);
    int blockable = 0;
    if (multiline && !as_key) {
        /* A block whose body is all whitespace parses back empty, and a
         * raw CR re-parses as a break — both emit double-quoted. */
        int allws = 1;
        int has_cr = 0;
        for (uint32_t i = 0; i < len; i++) {
            if (p[i] != ' ' && p[i] != '\t' && p[i] != '\n' && p[i] != '\r') {
                allws = 0;
            }
            if (p[i] == '\r') {
                has_cr = 1;
            }
        }
        blockable = !allws && !has_cr;
    }
    int sty = n->style;
    if (sty == 4 || sty == 5) {
        if (multiline && as_key) {
            emit_dq(w, p, len); /* keys stay inline: no block scalars */
            return;
        }
        if (multiline && blockable) {
            emit_literal(w, n, parent_col);
            return;
        }
        if (multiline) {
            emit_dq(w, p, len);
            return;
        }
        sty = 1; /* a chomp-stripped block is plain bytes: re-emit plain */
    }
    switch (sty) {
    case 1: /* plain */
        if (len == 0) {
            if (as_key || !n->implicit) {
                emit_dq(w, p, len); /* empty keys must stay visible */
            }
            break; /* an implicit empty value emits as nothing */
        }
        if ((as_key ? yep_style_plain_key_safe(p, len) : yep_style_plain_safe(p, len))) {
            wr_put(w, p, len);
        } else {
            emit_dq(w, p, len);
        }
        break;
    case 2: /* single */
        if (!multiline) {
            emit_sq(w, p, len);
        } else {
            emit_dq(w, p, len);
        }
        break;
    default: /* double, literal-as-key, folded-as-key, any */
        emit_dq(w, p, len);
        break;
    }
}

/* Canonical mode renames anchors to generated names (a0, a1, ...):
 * exotic source names (flow indicators inside them) cannot survive
 * flow context raw, and renaming preserves the binding semantics.
 * Deterministic by definition order -> byte-stable across roundtrips.
 * The emitter's nametab lives in yep_emitter (owner: yep_emit_run). */
static void emit_anchor_ex(yep_emitter* em, const yep_dnode* n) {
    yep_writer* w = &em->w;
    if (n->anchor.len == 0) {
        return;
    }
    wr_byte(w, '&');
    if (w->canonical) {
        uint32_t idx = yep_nametab_get(&em->canon_names, n->anchor);
        if (idx == YEP_NAMETAB_NIL) {
            idx = em->canon_names.count;
            yep_nametab_set(&em->canon_names, n->anchor, idx);
        }
        char name[24];
        int nl = snprintf(name, sizeof(name), "a%u", idx);
        wr_put(w, name, (uint32_t)nl);
    } else {
        wr_put(w, (const char*)n->anchor.p, n->anchor.len);
    }
    wr_byte(w, ' ');
}

static void emit_alias_ex(yep_emitter* em, const yep_dnode* n) {
    yep_writer* w = &em->w;
    wr_byte(w, '*');
    if (w->canonical) {
        uint32_t idx = yep_nametab_get(&em->canon_names, n->value);
        char name[24];
        int nl;
        if (idx == YEP_NAMETAB_NIL) {
            nl = snprintf(name, sizeof(name), "%.*s", (int)n->value.len, (const char*)n->value.p);
        } else {
            nl = snprintf(name, sizeof(name), "a%u", idx);
        }
        wr_put(w, name, (uint32_t)nl);
    } else {
        wr_put(w, (const char*)n->value.p, n->value.len);
    }
}

/* Explicit tag, verbatim URI form ("!<tag:…> "). Never skipped: a
 * recorded tag always re-emits (implied-by-style skipping made the
 * roundtrip byte-unstable: plain+!!str and ""+!!str collided). */
static void emit_tag(yep_writer* w, const yep_dnode* n) {
    if (n->tag.len == 0) {
        return;
    }
    wr_byte(w, '!');
    wr_byte(w, '<');
    wr_put(w, (const char*)n->tag.p, n->tag.len);
    wr_byte(w, '>');
    wr_byte(w, ' ');
}

/* Line-tail properties for block collections: "!<t> &a" after
 * "key:"/"-" — properties may not precede a dash entry directly (the
 * suite's SY6V rule). */
static void emit_props_tail(yep_writer* w, const yep_dnode* n) {
    if (n->tag.len > 0) {
        wr_byte(w, ' ');
        wr_byte(w, '!');
        wr_byte(w, '<');
        wr_put(w, (const char*)n->tag.p, n->tag.len);
        wr_byte(w, '>');
    }
    if (n->anchor.len > 0) {
        wr_byte(w, ' ');
        wr_byte(w, '&');
        wr_put(w, (const char*)n->anchor.p, n->anchor.len);
    }
}

static void emit_node(yep_emitter* em, uint32_t id, int parent_col, int as_key);

static void emit_flow(yep_emitter* em, uint32_t id) {
    yep_writer* w = &em->w;
    const yep_dom* d = em->doc->dom;
    const yep_dnode* n = yep_dom_node(d, id);
    emit_tag(w, n);
    emit_anchor_ex(em, n);
    int map = (n->kind == 2);
    wr_byte(w, map ? '{' : '[');
    int flow_indent = w->col;
    uint32_t child = n->first_child;
    uint32_t idx = 0;
    while (child != UINT32_MAX) {
        const yep_dnode* cn = yep_dom_node(d, child);
        if (idx > 0) {
            /* 13B: past best_width, break AFTER the previous value
             * (value boundaries only, never inside a scalar) and
             * re-indent to the opening bracket's column */
            if (w->best_width > 0 && w->col > w->best_width) {
                wr_byte(w, ',');
                wr_byte(w, '\n');
                wr_indent(w, flow_indent);
            } else {
                wr_put(w, ", ", 2);
            }
        }
        if (map) {
            int explicit_key = cn->anchor.len > 0 || cn->tag.len > 0 || cn->kind != 0;
            if (explicit_key) {
                wr_put(w, "? ", 2);
                emit_node(em, child, 0, 1);
                wr_put(w, " : ", 3);
            } else {
                emit_node(em, child, 0, 1);
                wr_put(w, ": ", 2);
            }
            emit_node(em, cn->next_sibling, 0, 0);
            child = cn->next_sibling;
            cn = yep_dom_node(d, child);
        } else {
            emit_node(em, child, 0, 0);
        }
        idx++;
        child = cn->next_sibling;
    }
    wr_byte(w, map ? '}' : ']');
}

/* Block map: first key at the cursor (the caller placed it at
 * content_col = parent_col+2); following keys on new lines there. */
static void emit_block_map(yep_emitter* em, uint32_t id, int content_col) {
    yep_writer* w = &em->w;
    const yep_dom* d = em->doc->dom;
    const yep_dnode* n = yep_dom_node(d, id);
    uint32_t child = n->first_child;
    uint32_t pair = 0;
    while (child != UINT32_MAX) {
        const yep_dnode* kn = yep_dom_node(d, child);
        const yep_dnode* vn = yep_dom_node(d, kn->next_sibling);
        if (pair > 0) {
            if (w->last != '\n') {
                wr_byte(w, '\n'); /* keep-chomped bodies own their break */
            }
            wr_indent(w, content_col);
        }
        int complex_key = (kn->kind != 0);
        if (complex_key) {
            w->force_flow = 1; /* collections as flow keys: compact and
                                  free of the block '?' layout edges */
            wr_put(w, "? ", 2);
            emit_props_tail(w, kn); /* the key's props ride "? " */
            if (kn->anchor.len > 0 || kn->tag.len > 0) {
                wr_byte(w, '\n');
                wr_indent(w, content_col + 2);
            }
            emit_node(em, child, content_col, 1);
            wr_byte(w, '\n');
            wr_indent(w, content_col);
            wr_put(w, ": ", 2);
            w->force_flow = 0;
            emit_node(em, kn->next_sibling, content_col, 0);
        } else {
            emit_tag(w, kn);
            emit_anchor_ex(em, kn);
            emit_scalar(w, kn, content_col, 1);
            wr_byte(w, ':');
            if (vn->kind == 0 || vn->kind == 3 || vn->flow) {
                wr_byte(w, ' ');
                emit_node(em, kn->next_sibling, content_col, 0);
            } else {
                /* nested block collection: next line at content+2 */
                emit_props_tail(w, vn);
                wr_byte(w, '\n');
                wr_indent(w, content_col + 2);
                emit_node(em, kn->next_sibling, content_col, 0);
            }
        }
        pair++;
        child = vn->next_sibling;
    }
}

/* Block seq: "- item" lines at content_col = parent_col+2. */
static void emit_block_seq(yep_emitter* em, uint32_t id, int content_col) {
    yep_writer* w = &em->w;
    const yep_dom* d = em->doc->dom;
    const yep_dnode* n = yep_dom_node(d, id);
    uint32_t child = n->first_child;
    uint32_t idx = 0;
    while (child != UINT32_MAX) {
        const yep_dnode* cn = yep_dom_node(d, child);
        if (idx > 0) {
            if (w->last != '\n') {
                wr_byte(w, '\n'); /* keep-chomped bodies own their break */
            }
            wr_indent(w, content_col);
        }
        wr_put(w, "- ", 2);
        if (cn->kind != 0 && cn->kind != 3 && !cn->flow &&
            (cn->anchor.len > 0 || cn->tag.len > 0)) {
            /* props may not precede a dash: they ride the dash line and
             * the collection starts on the next line (SY6V) */
            emit_props_tail(w, cn);
            wr_byte(w, '\n');
            wr_indent(w, content_col + 2);
            emit_node(em, child, content_col, 0);
        } else {
            /* compact: a block collection starts right after "- " */
            emit_node(em, child, content_col, 0);
        }
        idx++;
        child = cn->next_sibling;
    }
}

static void emit_node(yep_emitter* em, uint32_t id, int parent_col, int as_key) {
    yep_writer* w = &em->w;
    const yep_dom* d = em->doc->dom;
    const yep_dnode* n = yep_dom_node(d, id);
    if (n == NULL) {
        return;
    }
    if (n->kind == 3) { /* alias */
        emit_alias_ex(em, n);
        return;
    }
    if ((n->kind == 1 || n->kind == 2) && (n->flow || w->canonical || w->json || w->force_flow)) {
        /* force_flow holds through the subtree: a complex key's nested
         * collections all render flow (scalars are never collections) */
        emit_flow(em, id);
        return;
    }
    if (n->kind == 2) { /* mapping */
        if (n->count == 0) {
            emit_tag(w, n);
            emit_anchor_ex(em, n);
            wr_put(w, "{}", 2);
        } else {
            emit_block_map(em, id, parent_col + 2);
        }
        return;
    }
    if (n->kind == 1) { /* sequence */
        if (n->count == 0) {
            emit_tag(w, n);
            emit_anchor_ex(em, n);
            wr_put(w, "[]", 2);
        } else {
            emit_block_seq(em, id, parent_col + 2);
        }
        return;
    }
    emit_tag(w, n);
    emit_anchor_ex(em, n);
    if (parent_col < 0) {
        parent_col = 0; /* root scalar: block body at column 2 */
    }
    emit_scalar(w, n, parent_col, as_key);
}

size_t yep_emit_run(yep_emitter* em, int dry) {
    yep_writer* w = &em->w;
    const yep_dom* d = em->doc->dom;
    w->dry = dry;
    w->len = 0;
    w->last = 0;
    w->force_flow = 0;
    w->flushed = 0;
    w->sink_aborted = 0;
    w->col = 0;
    yep_nametab_clear(&em->canon_names);
    if (w->json) {
        /* JSON has no multi-document streams: exactly one root */
        if (d->dcount == 1) {
            const yep_dnode* root = yep_dom_node(d, d->docs[0]);
            emit_node(em, d->docs[0], -2, 0);
            if (root != NULL && root->kind != 0 && w->last != '\n') {
                wr_byte(w, '\n');
            }
        }
        return w->flushed + w->len;
    }
    int multi = (d->dcount > 1) || w->canonical;
    for (uint32_t i = 0; i < d->dcount; i++) {
        if (multi) {
            wr_put(w, "---\n", 4);
        }
        const yep_dnode* root = yep_dom_node(d, d->docs[i]);
        if (root != NULL && (root->kind == 1 || root->kind == 2) && !root->flow && !w->canonical &&
            (root->tag.len > 0 || root->anchor.len > 0)) {
            /* root block-collection props ride their own leading line;
             * scalars and flow collections carry them inline */
            if (root->tag.len > 0) {
                wr_byte(w, '!');
                wr_byte(w, '<');
                wr_put(w, (const char*)root->tag.p, root->tag.len);
                wr_byte(w, '>');
            }
            if (root->anchor.len > 0) {
                wr_byte(w, '&');
                wr_put(w, (const char*)root->anchor.p, root->anchor.len);
            }
            wr_byte(w, '\n');
        }
        emit_node(em, d->docs[i], -2, 0); /* root content at column 0 */
        if (w->last != '\n') {
            wr_byte(w, '\n');
        }
    }
    return w->flushed + w->len;
}
