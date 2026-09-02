/* dom.c — event-built DOM (builder sink + storage). */

#include <string.h>

#include "dom.h"

static int dom_grow_nodes(yep_dom* d, uint32_t need) {
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

static int dom_grow_docs(yep_dom* d, uint32_t need) {
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

/* Copies a non-borrowed value into the DOM pool (borrowed stays a view). */
static yep_view dom_value(yep_dom* d, const yep_event* ev) {
    if (ev->borrowed || ev->value.len == 0) {
        return ev->value;
    }
    char* cp = yep_pool_alloc(d->pool, ev->value.len, 16);
    if (cp == NULL) {
        return ev->value;
    }
    memcpy(cp, ev->value.p, ev->value.len);
    yep_view v = {cp, ev->value.len};
    return v;
}

static uint32_t dom_new_node(yep_dom* d, const yep_event* ev, uint8_t kind) {
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
        n->tag = ev->tag;
        n->tag_id = ev->tag_id;
        n->anchor = ev->anchor;
        n->style = ev->style;
        n->implicit = ev->implicit;
        n->flow = ev->flow;
        n->line = ev->line;
        n->col = ev->col;
    }
    return d->ncount++;
}

static int dom_anchor_set(yep_dom* d, yep_view name, uint32_t node) {
    return yep_nametab_set(&d->anchors, name, node);
}

static uint32_t dom_anchor_get(const yep_dom* d, yep_view name) {
    return yep_nametab_get(&d->anchors, name);
}

static void dom_link(yep_dom* d, uint32_t parent, uint32_t child) {
    yep_dnode* p = &d->nodes[parent];
    if (p->count == 0) {
        p->first_child = child;
    } else {
        d->nodes[p->last_child].next_sibling = child;
    }
    p->last_child = child;
    p->count++;
}

/* Places a completed node: value for a pending key, child of the top
 * collection, or document root. */
static int dom_place(yep_dom* d, uint32_t id) {
    if (d->depth == 0) {
        if (!dom_grow_docs(d, 1)) {
            return -1;
        }
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
    case YEP_EV_STREAM_END:
    case YEP_EV_DOCUMENT_START:
    case YEP_EV_DOCUMENT_END:
        /* bindings are document-scoped, like the engine's names */
        yep_nametab_clear(&d->anchors);
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
        if (!yep_view_is_empty(ev->anchor) && !dom_anchor_set(d, ev->anchor, id)) {
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
        d->nodes[id].value = dom_value(d, ev);
        if (!yep_view_is_empty(ev->anchor) && !dom_anchor_set(d, ev->anchor, id)) {
            return -1;
        }
        return dom_place(d, id);
    }

    case YEP_EV_ALIAS: {
        uint32_t target = dom_anchor_get(d, ev->value);
        if (target == UINT32_MAX) {
            return -1;
        }
        uint32_t id = dom_new_node(d, ev, YEP_DOM_ALIAS);
        if (id == UINT32_MAX) {
            return -1;
        }
        d->nodes[id].value = ev->value;
        d->nodes[id].target = target;
        return dom_place(d, id);
    }

    default:
        return -1;
    }
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
    if (!yep_nametab_init(&d->anchors, sys)) {
        yep_pool_destroy(pool);
        yep_free(sys, d);
        return NULL;
    }
    return d;
}

void yep_dom_destroy(yep_dom* d) {
    if (d == NULL) {
        return;
    }
    yep_nametab_free(&d->anchors);
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
        char* out = yep_finish_double(b->p, (uint32_t)(start + 1), (uint32_t)close, 0, b->d->pool,
                                      &n->value.len);
        if (out == NULL) {
            return -1;
        }
        n->value.p = out;
    } else {
        n->value.p = b->p + start + 1;
        n->value.len = (uint32_t)(close - start - 1);
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
    n->value.p = b->p + start;
    n->value.len = (uint32_t)(i - start);
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
