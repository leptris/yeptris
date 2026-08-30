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
        n->anchor = ev->anchor;
        n->style = ev->style;
        n->implicit = ev->implicit;
        n->line = ev->line;
        n->col = ev->col;
    }
    return d->ncount++;
}

static int dom_anchor_set(yep_dom* d, yep_view name, uint32_t node) {
    for (uint32_t i = 0; i < d->anchor_count; i++) {
        if (yep_view_eq(d->anchors[i].name, name)) {
            d->anchors[i].node = node;
            return 1;
        }
    }
    if (d->anchor_count >= YEP_DOM_MAX_ANCHORS) {
        return 0;
    }
    d->anchors[d->anchor_count].name = name;
    d->anchors[d->anchor_count].node = node;
    d->anchor_count++;
    return 1;
}

static uint32_t dom_anchor_get(const yep_dom* d, yep_view name) {
    for (uint32_t i = 0; i < d->anchor_count; i++) {
        if (yep_view_eq(d->anchors[i].name, name)) {
            return d->anchors[i].node;
        }
    }
    return UINT32_MAX;
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
        if (!d->map_pending_key) {
            d->pending_key = id;
            d->map_pending_key = 1;
            return 0;
        }
        d->map_pending_key = 0;
        dom_link(d, top, d->pending_key); /* key */
        dom_link(d, top, id);             /* value */
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
    return d;
}

void yep_dom_destroy(yep_dom* d) {
    if (d == NULL) {
        return;
    }
    yep_pool_destroy(d->pool);
    yep_free(d->sys, d);
}

const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id) {
    if (d == NULL || id >= d->ncount) {
        return NULL;
    }
    return &d->nodes[id];
}
