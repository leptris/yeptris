/* dom.c — event-built DOM (builder sink + storage). */

#include <string.h>

#include "common/simd_text.h"

#include <pthread.h>

#include "dom.h"

int dom_grow_nodes(yep_dom* d, uint32_t need) {
    if (d->ncount + need <= d->ncap) {
        return 1;
    }
    uint32_t ncap = d->ncap ? d->ncap * 2 : 64;
    while (ncap < d->ncount + need) {
        ncap *= 2;
    }
    yep_dnode* nd = yep_pool_alloc(d->pool, (size_t)ncap * sizeof(yep_dnode), 16);
    if (nd == NULL) {
        return 0;
    }
    if (d->ncount > 0) {
        memcpy(nd, d->nodes, (size_t)d->ncount * sizeof(yep_dnode));
    }
    d->nodes = nd;
    d->ncap = ncap;
    return 1;
}

int dom_grow_docs(yep_dom* d, uint32_t need) {
    if (d->dcount + need <= d->dcap) {
        return 1;
    }
    uint32_t ncap = d->dcap ? d->dcap * 2 : 16;
    while (ncap < d->dcount + need) {
        ncap *= 2;
    }
    uint32_t* nd = yep_pool_alloc(d->pool, (size_t)ncap * sizeof(uint32_t), 16);
    if (nd == NULL) {
        return 0;
    }
    if (d->dcount > 0) {
        memcpy(nd, d->docs, (size_t)d->dcount * sizeof(uint32_t));
    }
    d->docs = nd;
    d->dcap = ncap;
    return 1;
}

/* Doubles the arena until cap bytes fit; 0 on OOM. Offsets stored in
 * nodes stay valid across the move. */
static int str_grow(yep_dom* d, uint32_t need) {
    if (d->str_len + need <= d->str_cap) {
        return 1;
    }
    uint32_t cap = d->str_cap ? d->str_cap : 256;
    while (cap < d->str_len + need) {
        cap *= 2;
    }
    char* ns = yep_alloc(d->sys, cap);
    if (ns == NULL) {
        return 0;
    }
    if (d->str_len > 0) {
        memcpy(ns, d->str, d->str_len);
    }
    yep_free(d->sys, d->str);
    d->str = ns;
    d->str_cap = cap;
    return 1;
}

yep_sview yep_dom_str_put(yep_dom* d, const char* p, uint32_t len) {
    yep_sview sv = {YEP_SV_INPUT, 0}; /* empty: arena offset 0 */
    if (len == 0 || !str_grow(d, len)) {
        return sv; /* len 0 or OOM: empty view */
    }
    memcpy(d->str + d->str_len, p, len);
    sv.off = YEP_SV_INPUT | d->str_len;
    sv.len = len;
    d->str_len += len;
    return sv;
}

char* yep_dom_str_tail(yep_dom* d, uint32_t cap) {
    if (!str_grow(d, cap)) {
        return NULL;
    }
    return d->str + d->str_len;
}

yep_sview yep_dom_str_commit(yep_dom* d, uint32_t written) {
    yep_sview sv = {YEP_SV_INPUT | d->str_len, written};
    d->str_len += written;
    return sv;
}

/* Node string: borrowed views stay INPUT offsets (zero copy);
 * everything the engine produced (folded, escaped, resolved tags,
 * anchor names) is copied into the arena — the finish pool is then
 * free to die with the engine. */
static yep_sview dom_str_in(yep_dom* d, const yep_view* v, int borrowed) {
    if (v == NULL || v->len == 0) {
        yep_sview sv = {0, 0};
        return sv;
    }
    if (borrowed && v->p != NULL && d->input_base != NULL && (const char*)v->p >= d->input_base) {
        return yep_sv_input(d, *v);
    }
    return yep_dom_str_put(d, (const char*)v->p, v->len);
}

/* The node-init law (every builder's SSOT): creates one node with the
 * given metadata; tag/anchor views encode through dom_str_in (tags and
 * event-path anchors copy to the arena; input-backed views borrow).
 * tag_id stays 0 — the caller resolves (scalars) or leaves 0. */
uint32_t dom_open_node(yep_dom* d, uint8_t kind, const yep_view* tag, const yep_view* anchor,
                       int anchor_borrowed, uint8_t style, uint8_t implicit, uint8_t flow,
                       uint32_t line, uint32_t col) {
    if (!dom_grow_nodes(d, 1)) {
        return UINT32_MAX;
    }
    yep_dnode* n = &d->nodes[d->ncount];
    memset(n, 0, sizeof(*n));
    n->first_child = UINT32_MAX;
    n->last_child = UINT32_MAX;
    n->next_sibling = UINT32_MAX;
    n->target = UINT32_MAX;
    n->kind = kind;
    n->tag = dom_str_in(d, tag, 0);
    n->anchor = dom_str_in(d, anchor, anchor_borrowed);
    n->style = style;
    n->implicit = implicit;
    n->flow = flow;
    n->line = line;
    n->col = col;
    return d->ncount++;
}

uint32_t dom_new_node(yep_dom* d, const yep_event* ev, uint8_t kind) {
    if (ev == NULL) {
        return dom_open_node(d, kind, NULL, NULL, 0, 0, 0, 0, 0, 0);
    }
    /* event-path anchors ride the VALUE's borrowedness (historical
     * law, kept byte-identical) */
    uint32_t id = dom_open_node(d, kind, &ev->tag, &ev->anchor, ev->borrowed, ev->style,
                                ev->implicit, ev->flow, ev->line, ev->col);
    if (id == UINT32_MAX) {
        return UINT32_MAX;
    }
    d->nodes[id].tag_id = ev->tag_id;
    return id;
}

static int dom_anchor_set(yep_dom* d, uint32_t ordinal, uint32_t node) {
    if (ordinal == 0) {
        return -1;
    }
    if (ordinal > d->anchor_nodes_cap) {
        uint32_t cap = d->anchor_nodes_cap ? d->anchor_nodes_cap * 2 : 64;
        while (cap < ordinal) {
            cap *= 2;
        }
        uint32_t* na = yep_alloc(d->sys, cap * sizeof(*na));
        if (na == NULL) {
            return -1;
        }
        if (d->anchor_nodes != NULL) {
            memcpy(na, d->anchor_nodes, d->anchor_nodes_cap * sizeof(*na));
            yep_free(d->sys, d->anchor_nodes);
        }
        d->anchor_nodes = na;
        d->anchor_nodes_cap = cap;
    }
    d->anchor_nodes[ordinal - 1] = node;
    if (ordinal > d->anchor_max) {
        d->anchor_max = ordinal;
    }
    return 0;
}

static uint32_t dom_anchor_get(const yep_dom* d, uint32_t ordinal) {
    if (ordinal == 0 || ordinal > d->anchor_max) {
        return UINT32_MAX;
    }
    return d->anchor_nodes[ordinal - 1];
}

/* The one place links form (builder and mutation both): records
 * attachment and depth so mutation can reject double-parents and cap
 * nesting without parent pointers. */
void dom_link(yep_dom* d, uint32_t parent, uint32_t child) {
    yep_dnode* p = &d->nodes[parent];
    if (p->count == 0) {
        p->first_child = child;
    } else {
        d->nodes[p->last_child].next_sibling = child;
    }
    p->last_child = child;
    p->count++;
    d->nodes[child].attached = 1;
    d->nodes[child].depth = (uint16_t)(p->depth + 1);
}

/* Places a completed node: value for a pending key, child of the top
 * collection, or document root. */
static int dom_place(yep_dom* d, uint32_t id) {
    if (d->depth == 0) {
        if (!dom_grow_docs(d, 1)) {
            return -1;
        }
        d->nodes[id].attached = 1;
        d->nodes[id].depth = 0;
        d->docs[d->dcount++] = id;
        return 0;
    }
    uint32_t top = d->stack[d->depth - 1];
    yep_dnode* p = &d->nodes[top];
    if (p->kind == YEP_DOM_MAPPING) {
        /* per-frame pairing: a map nested as a KEY consumes this frame's
         * slot without disturbing the parent's (global state corrupted
         * complex keys) */
        if (!d->map_pending_key[d->depth - 1]) {
            d->pending_key_id[d->depth - 1] = id;
            d->map_pending_key[d->depth - 1] = 1;
            return 0;
        }
        d->map_pending_key[d->depth - 1] = 0;
        dom_link(d, top, d->pending_key_id[d->depth - 1]); /* key */
        dom_link(d, top, id);                              /* value */
        return 0;
    }
    dom_link(d, top, id);
    return 0;
}

int yep_dom_on_event(void* ctx, const yep_event* ev) {
    yep_dom* d = (yep_dom*)ctx;
    switch (ev->type) {
    case YEP_EV_STREAM_START:
    case YEP_EV_DOCUMENT_START:
    case YEP_EV_DOCUMENT_END:
        /* bindings are document-scoped, like the engine's names */
        if (d->anchor_max != 0) {
            memset(d->anchor_nodes, 0, d->anchor_max * sizeof(*d->anchor_nodes));
            d->anchor_max = 0;
        }
        return 0;
    case YEP_EV_STREAM_END:
        /* no depth walk: the parse links TOP-DOWN (every child is
         * linked the moment it starts, under a parent whose depth is
         * final), so dom_link's O(1) assignment already globalizes
         * them. The walk remains the MUTATION builder's job — it
         * attaches pre-built subtrees whose descendants keep local
         * depths (yep_mut_set_depths after attach). */
        return 0;

    case YEP_EV_SEQ_START:
    case YEP_EV_MAP_START: {
        if (d->depth >= YEP_DOM_MAX_DEPTH) {
            return -1;
        }
        uint8_t kind = (ev->type == YEP_EV_SEQ_START) ? YEP_DOM_SEQUENCE : YEP_DOM_MAPPING;
        uint32_t id = dom_new_node(d, ev, kind);
        if (id == UINT32_MAX) {
            return -1;
        }
        if (ev->anchor_id != 0 && dom_anchor_set(d, ev->anchor_id, id) != 0) {
            return -1;
        }
        if (dom_place(d, id) != 0) {
            return -1;
        }
        d->map_pending_key[d->depth] = 0;
        d->stack[d->depth++] = id;
        return 0;
    }

    case YEP_EV_SEQ_END:
    case YEP_EV_MAP_END:
        if (d->depth == 0) {
            return -1;
        }
        d->depth--;
        return 0;

    case YEP_EV_SCALAR: {
        uint32_t id = dom_new_node(d, ev, YEP_DOM_SCALAR);
        if (id == UINT32_MAX) {
            return -1;
        }
        d->nodes[id].value = dom_str_in(d, &ev->value, ev->borrowed);
        if (ev->anchor_id != 0 && dom_anchor_set(d, ev->anchor_id, id) != 0) {
            return -1;
        }
        return dom_place(d, id);
    }

    case YEP_EV_ALIAS: {
        uint32_t target = dom_anchor_get(d, ev->anchor_id);
        if (target == UINT32_MAX) {
            return -1;
        }
        uint32_t id = dom_new_node(d, ev, YEP_DOM_ALIAS);
        if (id == UINT32_MAX) {
            return -1;
        }
        d->nodes[id].value = dom_str_in(d, &ev->value, ev->borrowed);
        d->nodes[id].target = target;
        return dom_place(d, id);
    }

    default:
        return -1;
    }
}

void yep_dom_reserve(yep_dom* d, uint32_t node_hint, uint32_t str_hint) {
    if (d == NULL) {
        return;
    }
    if (node_hint > d->ncount) {
        /* capacity for node_hint TOTAL nodes; growth remains on OOM */
        (void)dom_grow_nodes(d, node_hint - d->ncount);
    }
    if (str_hint > d->str_cap) {
        (void)str_grow(d, str_hint);
    }
}

/* Content-derived sizing (TODO.impl/06): three SIMD count passes,
 * then reserves. Every structural byte bounds a node boundary; the
 * arena reserve only pays when the content copies (quoted scalars
 * escape, block scalars fold) — borrowed-only documents must not
 * allocate an arena at all. Hints are advisory. */
void yep_dom_prepare(yep_dom* d, const yep_text_stats* st) {
    if (d == NULL || st == NULL) {
        return;
    }
    /* a block line carries at least TWO nodes (a key and its value,
     * or a dash and its entry): nl-colons cancels to ~zero on plain
     * scalar maps, so the old hint undershot by 200k nodes on
     * anchor-heavy (the growth memmove profiled at 5% of parse) */
    size_t structural = st->comma + st->dash + st->bracket + st->brace + 8;
    size_t by_line = st->nl * 2 + 8;
    uint32_t node_hint = (uint32_t)(structural > by_line ? structural : by_line);
    uint32_t str_hint = (st->dq + st->sq + st->pipe) > 0
                            ? (uint32_t)(st->dq * 16 + st->sq * 8 + st->pipe * 128 + 64)
                            : 0;
    yep_dom_reserve(d, node_hint, str_hint);
}

yep_dom* yep_dom_create(const yep_allocator* sys) {
    if (sys == NULL) {
        return NULL;
    }
    yep_pool* pool = yep_pool_create(sys, 8192);
    if (pool == NULL) {
        return NULL;
    }
    yep_dom* d = yep_alloc(sys, sizeof(yep_dom));
    if (d == NULL) {
        yep_pool_destroy(pool);
        return NULL;
    }
    memset(d, 0, sizeof(*d));
    d->sys = sys;
    d->pool = pool;
    d->handles = yep_hpool_create(sys);
    if (d->handles == NULL) {
        yep_pool_destroy(pool);
        yep_free(sys, d);
        return NULL;
    }
    if (pthread_mutex_init(&d->midx.mu, NULL) != 0) {
        yep_free(d->sys, d->anchor_nodes);
        yep_hpool_destroy(d->handles);
        yep_pool_destroy(pool);
        yep_free(sys, d);
        return NULL;
    }
    d->midx.mu_ready = 1;
    return d;
}

void yep_dom_destroy(yep_dom* d) {
    if (d == NULL) {
        return;
    }
    yep_free(d->sys, d->str);
    yep_midx_destroy(d);
    yep_free(d->sys, d->anchor_nodes);
    yep_hpool_destroy(d->handles);
    yep_pool_destroy(d->pool);
    yep_free(d->sys, d);
}

/* ------------------------------------------------- direct builders */
/* Two direct builders, one set of laws: yep_dom_build_json walks a
 * whole strictly-validated JSON buffer (TODO.impl/27, the parse_json
 * seam); dom_on_flow_json walks an engine-validated span mid-document
 * (TODO.restructure/50, the sink fast path). Both create nodes through
 * dom_open_node and place them through dom_place on the DOM's own
 * stack — the event sink's pairing/link/anchor laws, shared, so the
 * trees cannot diverge (the flow-direct-diff ctest pins it). */

#include "parse/scalars.h"
#include "resolve/resolver.h"
#include "scan/json.h"
#include "scan/scan.h"

static const yep_resolver* dom_resolver(const yep_dom* d) {
    return d->resolver != NULL ? d->resolver : yep_resolver_core12();
}

typedef struct {
    yep_dom* d;
    const char* p;
    size_t len;
    size_t i;
} jbuilder;

/* Whole-buffer JSON: value-first recursion (the grammar is validated,
 * the walk only advances). */
static int jb_value(jbuilder* b);

static int jb_string_node(jbuilder* b) {
    uint32_t id = dom_open_node(b->d, YEP_DOM_SCALAR, NULL, NULL, 0, 0, 0, 0, 0, 0);
    if (id == UINT32_MAX) {
        return -1;
    }
    yep_dnode* n = &b->d->nodes[id];
    size_t close;
    int has_esc = 0;
    size_t start = b->i;
    if (!yep_json_string(b->p, b->len, &b->i, &close, &has_esc)) {
        return -2;
    }
    if (has_esc) {
        uint32_t span = (uint32_t)(close - start - 1);
        char* dst = yep_dom_str_tail(b->d, span);
        if (dst == NULL) {
            return -1;
        }
        n->value = yep_dom_str_commit(
            b->d, yep_finish_double_into(b->p, (uint32_t)(start + 1), (uint32_t)close, dst, span));
    } else {
        yep_view v = {b->p + start + 1, (uint32_t)(close - start - 1)};
        n->value = dom_str_in(b->d, &v, 1);
    }
    n->style = YEP_STYLE_DOUBLE_QUOTED;
    n->tag_id = 0; /* str */
    return dom_place(b->d, id) == 0 ? 0 : -1;
}

static int jb_scalar_node(jbuilder* b) {
    uint32_t id = dom_open_node(b->d, YEP_DOM_SCALAR, NULL, NULL, 0, 0, 0, 0, 0, 0);
    if (id == UINT32_MAX) {
        return -1;
    }
    yep_dnode* n = &b->d->nodes[id];
    size_t start = b->i;
    size_t i = b->i;
    const yep_resolver* r = dom_resolver(b->d);
    if (b->p[i] == 't') {
        if (!yep_json_literal(b->p, b->len, &i, "true")) {
            return -2;
        }
    } else if (b->p[i] == 'f') {
        if (!yep_json_literal(b->p, b->len, &i, "false")) {
            return -2;
        }
    } else if (b->p[i] == 'n') {
        if (!yep_json_literal(b->p, b->len, &i, "null")) {
            return -2;
        }
    } else {
        if (!yep_json_number(b->p, b->len, &i)) {
            return -2;
        }
    }
    b->i = i;
    yep_view v = {b->p + start, (uint32_t)(i - start)};
    n->value = dom_str_in(b->d, &v, 1);
    n->style = YEP_STYLE_PLAIN;
    n->implicit = 1;
    n->tag_id = r->resolve(NULL, v.p, v.len);
    return dom_place(b->d, id) == 0 ? 0 : -1;
}

static void jb_ws(jbuilder* b) {
    while (b->i < b->len) {
        char c = b->p[b->i];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            b->i++;
        } else {
            break;
        }
    }
}

static int jb_value(jbuilder* b) {
    yep_dom* d = b->d;
    jb_ws(b);
    if (b->i >= b->len) {
        return -2;
    }
    char c = b->p[b->i];
    if (c == '{' || c == '[') {
        if (d->depth >= YEP_DOM_MAX_DEPTH) {
            return -2;
        }
        int map = (c == '{');
        b->i++;
        uint32_t id = dom_open_node(d, map ? YEP_DOM_MAPPING : YEP_DOM_SEQUENCE, NULL, NULL, 0, 0,
                                    0, 1, 0, 0);
        if (id == UINT32_MAX) {
            return -1;
        }
        if (dom_place(d, id) != 0) {
            return -1;
        }
        d->map_pending_key[d->depth] = 0;
        d->stack[d->depth++] = id;
        jb_ws(b);
        if (b->i < b->len && b->p[b->i] == (map ? '}' : ']')) {
            b->i++;
            d->depth--;
            return 0;
        }
        for (;;) {
            int rc = jb_value(b);
            if (rc != 0) {
                return rc;
            }
            jb_ws(b);
            if (map) {
                /* a value must be followed by , or } — the map's key
                 * slot is pending until the ':' value completes */
                if (b->i < b->len && b->p[b->i] == ':' && d->map_pending_key[d->depth - 1] != 0) {
                    b->i++;
                    rc = jb_value(b);
                    if (rc != 0) {
                        return rc;
                    }
                    jb_ws(b);
                }
            }
            if (b->i >= b->len) {
                return -2;
            }
            if (b->p[b->i] == ',') {
                b->i++;
                continue;
            }
            if (b->p[b->i] == (map ? '}' : ']')) {
                b->i++;
                d->depth--;
                return 0;
            }
            return -2;
        }
    }
    if (c == '"') {
        return jb_string_node(b);
    }
    return jb_scalar_node(b);
}

int yep_dom_build_json(yep_dom* d, const char* buf, size_t len) {
    jbuilder b = {d, buf, len, 0};
    int rc = jb_value(&b);
    if (rc != 0) {
        return rc;
    }
    jb_ws(&b);
    if (b.i != len) {
        return -2;
    }
    return 0;
}

/* --- the flow fast path (TODO.restructure/50) --- */

/* One walk of an engine-validated JSON-class span, mirroring
 * e_flow_json's pass-2 emission order and line/col choreography
 * node-for-node: placement rides the DOM's live stack (the same
 * dom_place the event sink uses), scalars resolve through the
 * document's resolver (the typing SSOT), escaped strings unescape
 * straight into the arena (one copy where the event path pays two).
 * Returns 0 built, -1 abort (OOM / depth), -2 validator surprise. */
static int dom_flow_walk(yep_dom* d, const char* p, size_t open, size_t close, uint32_t line,
                         size_t line_start, const yep_view* anchor, const yep_view* tag,
                         uint32_t anchor_id) {
    if (d->depth >= YEP_DOM_MAX_DEPTH) {
        return -1; /* the event sink's per-START cap, checked at root */
    }
    uint32_t cur_line = line;
    size_t cur_ls = line_start;
    size_t cur_scan = open; /* unscanned region starts at the span */

    uint32_t id = dom_open_node(d, p[open] == '[' ? YEP_DOM_SEQUENCE : YEP_DOM_MAPPING, tag, anchor,
                                0, 0, 0, 1, cur_line, (uint32_t)(open + 1 - cur_ls) + 1);
    if (id == UINT32_MAX) {
        return -1;
    }
    if (anchor_id != 0 && dom_anchor_set(d, anchor_id, id) != 0) {
        return -1;
    }
    if (dom_place(d, id) != 0) {
        return -1;
    }
    d->map_pending_key[d->depth] = 0;
    d->stack[d->depth++] = id;

    const yep_resolver* r = dom_resolver(d);
    /* pass 2's single-line shape: no newline in the span, no per-token
     * line bookkeeping (mirrored exactly — the walk may not pay what
     * the event path skipped) */
    int single_line = (memchr(p + open, '\n', close - open) == NULL);
    int sd = 1; /* frames opened by THIS span (d->depth may be deeper) */
    size_t i = open + 1;
    for (;;) {
        while (i < close && (p[i] == ' ' || p[i] == '\n' || p[i] == '\r')) {
            i++;
        }
        char c = p[i];
        if (c == ']' || c == '}') {
            if (!single_line) {
                yep_scan_advance_line(p, &cur_scan, i, &cur_line, &cur_ls);
            }
            d->depth--;
            sd--;
            if (sd == 0) {
                return 0;
            }
            i++;
            continue;
        }
        if (c == '{' || c == '[') {
            if (d->depth >= YEP_DOM_MAX_DEPTH) {
                return -1;
            }
            yep_scan_advance_line(p, &cur_scan, i, &cur_line, &cur_ls);
            uint32_t cid =
                dom_open_node(d, c == '[' ? YEP_DOM_SEQUENCE : YEP_DOM_MAPPING, NULL, NULL, 0, 0, 0,
                              1, cur_line, (uint32_t)(i + 1 - cur_ls) + 1);
            if (cid == UINT32_MAX) {
                return -1;
            }
            if (dom_place(d, cid) != 0) {
                return -1;
            }
            d->map_pending_key[d->depth] = 0;
            d->stack[d->depth++] = cid;
            sd++;
            i++;
            continue;
        }
        if (c == ',' || c == ':') {
            i++;
            continue;
        }
        /* scalar: quoted, number, or literal — the validated walk */
        if (!single_line) {
            yep_scan_advance_line(p, &cur_scan, i, &cur_line, &cur_ls);
        }
        uint32_t col = (uint32_t)(i - cur_ls) + 1;
        size_t vstart = i;
        if (c == '"') {
            uint32_t sid = dom_open_node(d, YEP_DOM_SCALAR, NULL, NULL, 0, YEP_STYLE_DOUBLE_QUOTED,
                                         0, 0, cur_line, col);
            if (sid == UINT32_MAX) {
                return -1;
            }
            size_t vclose;
            int he = 0;
            if (!yep_json_string(p, close + 1, &i, &vclose, &he)) {
                return -2; /* validated: cannot happen */
            }
            yep_dnode* n = &d->nodes[sid];
            if (he) {
                uint32_t span = (uint32_t)(vclose - vstart - 1);
                char* dst = yep_dom_str_tail(d, span);
                if (dst == NULL) {
                    return -1;
                }
                n->value =
                    yep_dom_str_commit(d, yep_finish_double_into(p, (uint32_t)(vstart + 1),
                                                                 (uint32_t)vclose, dst, span));
            } else {
                yep_view v = {p + vstart + 1, (uint32_t)(vclose - vstart - 1)};
                n->value = dom_str_in(d, &v, 1);
            }
            if (dom_place(d, sid) != 0) {
                return -1;
            }
            continue;
        }
        /* number or literal: walk the exact token like pass 2 */
        uint32_t sid =
            dom_open_node(d, YEP_DOM_SCALAR, NULL, NULL, 0, YEP_STYLE_PLAIN, 1, 0, cur_line, col);
        if (sid == UINT32_MAX) {
            return -1;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            i += (c == 't') ? 4 : (c == 'f') ? 5 : 4;
        } else {
            i++;
            while (i < close && ((p[i] >= '0' && p[i] <= '9') || p[i] == '.' || p[i] == 'e' ||
                                 p[i] == 'E' || p[i] == '+' || p[i] == '-')) {
                i++;
            }
        }
        yep_view v = {p + vstart, (uint32_t)(i - vstart)};
        d->nodes[sid].value = dom_str_in(d, &v, 1);
        d->nodes[sid].tag_id = r->resolve(NULL, v.p, v.len);
        if (dom_place(d, sid) != 0) {
            return -1;
        }
    }
}

/* The sink's block fast path (TODO.restructure/54): one classified
 * `key: value` line becomes two nodes — the key scalar resolved like
 * every implicit key event, the value through its class — placed with
 * the shared pairing law. The engine offers the line BEFORE emitting;
 * returning 0 here (only for an unbound alias target, which cannot
 * happen when the engine resolved the name) falls back to the two
 * events with nothing built. */
int dom_on_block_pair(void* ctx, const yep_view* key, const yep_block_value* v, uint32_t line,
                      uint16_t key_col, uint16_t val_col) {
    yep_dom* d = (yep_dom*)ctx;
    uint32_t target = 0;
    if (v->cls == YEP_LVAL_ALIAS) {
        target = dom_anchor_get(d, v->anchor_id);
        if (target == UINT32_MAX) {
            return 0;
        }
    }
    const yep_resolver* r = dom_resolver(d);
    uint32_t kid = dom_open_node(d, YEP_DOM_SCALAR, NULL, NULL, 0, YEP_STYLE_PLAIN, 1, 0, line,
                                 (uint32_t)key_col + 1);
    if (kid == UINT32_MAX) {
        return -1;
    }
    d->nodes[kid].value = dom_str_in(d, key, 1);
    d->nodes[kid].tag_id = r->resolve(NULL, key->p, key->len);
    if (dom_place(d, kid) != 0) {
        return -1;
    }
    if (v->cls == YEP_LVAL_ALIAS) {
        uint32_t vid =
            dom_open_node(d, YEP_DOM_ALIAS, NULL, NULL, 0, 0, 0, 0, line, (uint32_t)val_col + 1);
        if (vid == UINT32_MAX) {
            return -1;
        }
        d->nodes[vid].value = dom_str_in(d, &v->value, 0);
        d->nodes[vid].target = target;
        return dom_place(d, vid) == 0 ? 1 : -1;
    }
    int anchored = (v->cls == YEP_LVAL_ANCHOR_PLAIN);
    uint32_t vid = dom_open_node(d, YEP_DOM_SCALAR, NULL, anchored ? &v->anchor : NULL, v->borrowed,
                                 YEP_STYLE_PLAIN, 1, 0, line, (uint32_t)val_col + 1);
    if (vid == UINT32_MAX) {
        return -1;
    }
    d->nodes[vid].value = dom_str_in(d, &v->value, v->borrowed);
    d->nodes[vid].tag_id = r->resolve(NULL, v->value.p, v->value.len);
    if (anchored && v->anchor_id != 0 && dom_anchor_set(d, v->anchor_id, vid) != 0) {
        return -1;
    }
    return dom_place(d, vid) == 0 ? 1 : -1;
}

int dom_on_flow_json(void* ctx, const char* p, size_t open, size_t close, uint32_t line,
                     size_t line_start, yep_view anchor, yep_view tag, uint32_t anchor_id) {
    yep_dom* d = (yep_dom*)ctx;
    int rc = dom_flow_walk(d, p, open, close, line, line_start, &anchor, &tag, anchor_id);
    return rc == 0 ? 1 : -1;
}

const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id) {
    if (d == NULL || id >= d->ncount) {
        return NULL;
    }
    return &d->nodes[id];
}
