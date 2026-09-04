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

/* Node string from an engine event: borrowed views stay INPUT
 * offsets (zero copy); everything the engine produced (folded,
 * escaped, resolved tags, anchor names) is copied into the arena —
 * the finish pool is then free to die with the engine. */
static yep_sview dom_ev_str(yep_dom* d, const yep_event* ev, const yep_view* v, int borrowed) {
    if (v->len == 0) {
        yep_sview sv = {0, 0};
        return sv;
    }
    if (borrowed && v->p != NULL && d->input_base != NULL && (const char*)v->p >= d->input_base) {
        return yep_sv_input(d, *v);
    }
    return yep_dom_str_put(d, (const char*)v->p, v->len);
}

uint32_t dom_new_node(yep_dom* d, const yep_event* ev, uint8_t kind) {
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
    if (ev) {
        n->tag = dom_ev_str(d, ev, &ev->tag, 0);
        n->tag_id = ev->tag_id;
        n->anchor = dom_ev_str(d, ev, &ev->anchor, ev->borrowed);
        n->style = ev->style;
        n->implicit = ev->implicit;
        n->flow = ev->flow;
        n->line = ev->line;
        n->col = ev->col;
    }
    return d->ncount++;
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
        d->nodes[id].value = dom_ev_str(d, ev, &ev->value, ev->borrowed);
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
        d->nodes[id].value = dom_ev_str(d, ev, &ev->value, ev->borrowed);
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
void yep_dom_prepare(yep_dom* d, const char* buf, size_t len) {
    if (d == NULL || buf == NULL) {
        return;
    }
    const yep_text_kernels* k = yep_text_active();
    size_t nl = 0, commas = 0, dashes = 0;
    size_t colons = 0, brackets = 0, braces = 0;
    size_t dq = 0, sq = 0, pipes = 0;
    k->count3(buf, len, '\n', ',', '-', &nl, &commas, &dashes);
    k->count3(buf, len, ':', '[', '{', &colons, &brackets, &braces);
    k->count3(buf, len, '"', '\'', '|', &dq, &sq, &pipes);
    /* a block line carries at least TWO nodes (a key and its value,
     * or a dash and its entry): nl-colons cancels to ~zero on plain
     * scalar maps, so the old hint undershot by 200k nodes on
     * anchor-heavy (the growth memmove profiled at 5% of parse) */
    size_t structural = commas + dashes + brackets + braces + 8;
    size_t by_line = nl * 2 + 8;
    uint32_t node_hint = (uint32_t)(structural > by_line ? structural : by_line);
    uint32_t str_hint = (dq + sq + pipes) > 0 ? (uint32_t)(dq * 16 + sq * 8 + pipes * 128 + 64) : 0;
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

/* ------------------------------------------------- direct JSON builder */

/* Builds the DOM straight from a strict-validated JSON buffer
 * (TODO.impl/27): scan/json.c validated the grammar; this walk creates
 * nodes and links them with no event pipeline — one node per value,
 * pre-sized from a counting pass. Strings unescape directly into the
 * DOM pool (one copy; the engine path pays two). Numbers resolve
 * through the core12 resolver (typing SSOT). Returns 0 on success, -1
 * on allocation failure, -2 if the buffer is not what the validator
 * promised (defensive: never happens in-tree). */

#include "parse/scalars.h"
#include "resolve/resolver.h"
#include "scan/json.h"

typedef struct {
    yep_dom* d;
    const char* p;
    size_t len;
    size_t i;
    uint32_t parent; /* stack-free: link as we descend via dom_link */
    uint32_t stack[YEP_DOM_MAX_DEPTH];
    uint32_t pend_key[YEP_DOM_MAX_DEPTH];
    int depth;
} jbuilder;

static uint32_t jb_node(jbuilder* b, uint8_t kind) {
    if (!dom_grow_nodes(b->d, 1)) {
        return UINT32_MAX;
    }
    yep_dnode* n = &b->d->nodes[b->d->ncount];
    memset(n, 0, sizeof(*n));
    n->first_child = UINT32_MAX;
    n->last_child = UINT32_MAX;
    n->next_sibling = UINT32_MAX;
    n->target = UINT32_MAX;
    n->kind = kind;
    return b->d->ncount++;
}

/* Places a completed node: map value consumes the pending key, else
 * child of the top collection or document root. */
static int jb_place(jbuilder* b, uint32_t id) {
    yep_dom* d = b->d;
    if (b->depth == 0) {
        if (!dom_grow_docs(d, 1)) {
            return -1;
        }
        d->nodes[id].attached = 1;
        d->nodes[id].depth = 0;
        d->docs[d->dcount++] = id;
        return 0;
    }
    uint32_t top = b->stack[b->depth - 1];
    yep_dnode* p = &d->nodes[top];
    if (p->kind == YEP_DOM_MAPPING) {
        if (b->pend_key[b->depth - 1] == UINT32_MAX) {
            b->pend_key[b->depth - 1] = id;
            return 0;
        }
        uint32_t key = b->pend_key[b->depth - 1];
        b->pend_key[b->depth - 1] = UINT32_MAX;
        dom_link(d, top, key);
        dom_link(d, top, id);
        return 0; /* count semantics match the sink: children, halved
                     by the public accessors */
    }
    dom_link(d, top, id);
    return 0;
}

static int jb_value(jbuilder* b);

static int jb_string_node(jbuilder* b) {
    uint32_t id = jb_node(b, YEP_DOM_SCALAR);
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
        n->value = yep_sv_input(b->d, v);
    }
    n->style = YEP_STYLE_DOUBLE_QUOTED;
    n->tag_id = 0; /* str */
    return jb_place(b, id) == 0 ? 0 : -1;
}

static int jb_scalar_node(jbuilder* b, uint32_t tag_id) {
    uint32_t id = jb_node(b, YEP_DOM_SCALAR);
    if (id == UINT32_MAX) {
        return -1;
    }
    yep_dnode* n = &b->d->nodes[id];
    size_t start = b->i;
    size_t i = b->i;
    /* advance past the literal/number: the shared primitives validate
     * and move the cursor */
    if (b->p[i] == 't') {
        if (!yep_json_literal(b->p, b->len, &i, "true")) {
            return -2;
        }
        tag_id = 3; /* bool */
    } else if (b->p[i] == 'f') {
        if (!yep_json_literal(b->p, b->len, &i, "false")) {
            return -2;
        }
        tag_id = 3;
    } else if (b->p[i] == 'n') {
        if (!yep_json_literal(b->p, b->len, &i, "null")) {
            return -2;
        }
        tag_id = 4; /* null */
    } else {
        if (!yep_json_number(b->p, b->len, &i)) {
            return -2;
        }
        tag_id = yep_resolver_core12()->resolve(NULL, b->p + start, (uint32_t)(i - start));
    }
    b->i = i;
    yep_view v = {b->p + start, (uint32_t)(i - start)};
    n->value = yep_sv_input(b->d, v);
    n->style = YEP_STYLE_PLAIN;
    n->implicit = 1;
    n->tag_id = (uint8_t)tag_id;
    return jb_place(b, id) == 0 ? 0 : -1;
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
    jb_ws(b);
    if (b->i >= b->len) {
        return -2;
    }
    char c = b->p[b->i];
    if (c == '{' || c == '[') {
        if (b->depth >= YEP_DOM_MAX_DEPTH) {
            return -2;
        }
        int map = (c == '{');
        b->i++;
        uint32_t id = jb_node(b, map ? YEP_DOM_MAPPING : YEP_DOM_SEQUENCE);
        if (id == UINT32_MAX) {
            return -1;
        }
        b->d->nodes[id].flow = 1;
        if (jb_place(b, id) != 0) {
            return -1;
        }
        b->stack[b->depth] = id;
        b->pend_key[b->depth] = UINT32_MAX;
        b->depth++;
        jb_ws(b);
        if (b->i < b->len && b->p[b->i] == (map ? '}' : ']')) {
            b->i++;
            b->depth--;
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
                if (b->i < b->len && b->p[b->i] == ':' && b->pend_key[b->depth - 1] != UINT32_MAX) {
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
                b->depth--;
                return 0;
            }
            return -2;
        }
    }
    if (c == '"') {
        return jb_string_node(b);
    }
    return jb_scalar_node(b, 0);
}

int yep_dom_build_json(yep_dom* d, const char* buf, size_t len) {
    jbuilder b = {d, buf, len, 0, 0, {0}, {0}, 0};
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

const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id) {
    if (d == NULL || id >= d->ncount) {
        return NULL;
    }
    return &d->nodes[id];
}
