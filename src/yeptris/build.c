/* build.c — the public from-scratch construction API (TODO.impl/11
 * phase 3). Thin: argument checking + handle wrapping over the dom
 * mutation primitives. The emitter and every query API work on
 * synthesized documents unchanged — same nodes, same invariants. */

#include <string.h>

#include <yeptris.h>

#include "doc.h"
#include "dom/dom.h"
#include "memory/allocator.h"

static yeptris_document* doc_of(YeptrisDocument handle) {
    return (yeptris_document*)handle;
}

static yeptris_node* node_of(YeptrisNode handle) {
    return (yeptris_node*)handle;
}

YEPTRIS_API YeptrisDocument yeptris_document_new(void) {
    const yep_allocator* sys = yep_system_allocator();
    yep_dom* dom = yep_dom_create(sys);
    if (dom == NULL) {
        return NULL;
    }
    yeptris_document* doc = yep_alloc(sys, sizeof(yeptris_document));
    if (doc == NULL) {
        yep_dom_destroy(dom);
        return NULL;
    }
    memset(doc, 0, sizeof(*doc));
    doc->dom = dom;
    doc->sys = sys;
    return (YeptrisDocument)doc;
}

YEPTRIS_API int yeptris_document_set_root(YeptrisDocument handle, YeptrisNode root) {
    yeptris_document* doc = doc_of(handle);
    yeptris_node* n = node_of(root);
    if (doc == NULL || n == NULL || n->doc != doc) {
        return YEPTRIS_ERROR_PARSE;
    }
    int rc = yep_mut_add_root(doc->dom, n->id);
    return rc == 0 ? YEPTRIS_OK
                   : (rc == -1 ? YEPTRIS_ERROR_MEMORY
                               : (rc == -3 ? YEPTRIS_ERROR_DEPTH : YEPTRIS_ERROR_ARG));
}

YEPTRIS_API YeptrisNode yeptris_node_new_mapping(YeptrisDocument handle) {
    yeptris_document* doc = doc_of(handle);
    if (doc == NULL) {
        return NULL;
    }
    uint32_t id = yep_mut_map(doc->dom, 0);
    return id == UINT32_MAX ? NULL : (YeptrisNode)yep_handle_new(doc, id);
}

YEPTRIS_API YeptrisNode yeptris_node_new_sequence(YeptrisDocument handle) {
    yeptris_document* doc = doc_of(handle);
    if (doc == NULL) {
        return NULL;
    }
    uint32_t id = yep_mut_seq(doc->dom, 0);
    return id == UINT32_MAX ? NULL : (YeptrisNode)yep_handle_new(doc, id);
}

/* Memory: the value is copied into the document (nothing is borrowed
 * from the caller). Plain style types the copy through the resolver,
 * quoted styles force string — identical to parse-time typing. */
YEPTRIS_API YeptrisNode yeptris_node_new_scalar(YeptrisDocument handle, const char* value,
                                                size_t len, YeptrisScalarStyle style) {
    yeptris_document* doc = doc_of(handle);
    if (doc == NULL || (value == NULL && len > 0) || style < YEPTRIS_STYLE_PLAIN ||
        style > YEPTRIS_STYLE_FOLDED) {
        return NULL;
    }
    uint32_t id = yep_mut_scalar(doc->dom, value, len, (uint8_t)style);
    return id == UINT32_MAX ? NULL : (YeptrisNode)yep_handle_new(doc, id);
}

static int mut_status(int rc) {
    switch (rc) {
    case 0:
    case 1: /* map_set replaced — success for the caller */
        return YEPTRIS_OK;
    case -1:
        return YEPTRIS_ERROR_MEMORY;
    case -3:
        return YEPTRIS_ERROR_DEPTH;
    default: /* -2 duplicate key, -4 double-attach */
        return YEPTRIS_ERROR_PARSE;
    }
}

static int same_doc(YeptrisNode parent, YeptrisNode child) {
    yeptris_node* p = node_of(parent);
    yeptris_node* c = node_of(child);
    return p != NULL && c != NULL && p->doc == c->doc;
}

YEPTRIS_API int yeptris_node_map_add(YeptrisNode handle, const char* key, size_t key_len,
                                     YeptrisNode value) {
    yeptris_node* m = node_of(handle);
    if (m == NULL || key == NULL || !same_doc(handle, value)) {
        return YEPTRIS_ERROR_ARG;
    }
    return mut_status(yep_mut_map_add(m->doc->dom, m->id, key, key_len, node_of(value)->id));
}

/* Replace-or-add (json-c semantics): an existing key keeps its
 * position and swaps the value; a new key appends. */
YEPTRIS_API int yeptris_node_map_set(YeptrisNode handle, const char* key, size_t key_len,
                                     YeptrisNode value) {
    yeptris_node* m = node_of(handle);
    if (m == NULL || key == NULL || !same_doc(handle, value)) {
        return YEPTRIS_ERROR_ARG;
    }
    return mut_status(yep_mut_map_set(m->doc->dom, m->id, key, key_len, node_of(value)->id));
}

YEPTRIS_API int yeptris_node_seq_add(YeptrisNode handle, YeptrisNode value) {
    yeptris_node* s = node_of(handle);
    if (s == NULL || !same_doc(handle, value)) {
        return YEPTRIS_ERROR_ARG;
    }
    return mut_status(yep_mut_seq_add(s->doc->dom, s->id, node_of(value)->id));
}

YEPTRIS_API int yeptris_node_seq_set(YeptrisNode handle, size_t index, YeptrisNode value) {
    yeptris_node* s = node_of(handle);
    if (s == NULL || !same_doc(handle, value)) {
        return YEPTRIS_ERROR_ARG;
    }
    return mut_status(yep_mut_seq_set(s->doc->dom, s->id, (uint32_t)index, node_of(value)->id));
}

YEPTRIS_API int yeptris_node_seq_del(YeptrisNode handle, size_t index) {
    yeptris_node* s = node_of(handle);
    if (s == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    return yep_mut_seq_del(s->doc->dom, s->id, (uint32_t)index) == 0 ? YEPTRIS_OK
                                                                     : YEPTRIS_ERROR_PARSE;
}

/* The removed subtree stays document-owned (arena lifetime); its
 * handle remains valid but unreachable from the root. */
YEPTRIS_API int yeptris_node_set_anchor(YeptrisNode handle, const char* name, size_t len) {
    yeptris_node* n = node_of(handle);
    if (n == NULL || name == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    return yep_mut_set_anchor(n->doc->dom, n->id, name, len) == 0 ? YEPTRIS_OK : YEPTRIS_ERROR_ARG;
}

YEPTRIS_API int yeptris_node_set_tag(YeptrisNode handle, const char* tag, size_t len) {
    yeptris_node* n = node_of(handle);
    if (n == NULL || tag == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    return yep_mut_set_tag(n->doc->dom, n->id, tag, len) == 0 ? YEPTRIS_OK : YEPTRIS_ERROR_ARG;
}

YEPTRIS_API YeptrisNode yeptris_node_new_alias(YeptrisDocument handle, YeptrisNode target,
                                               const char* name, size_t len) {
    yeptris_document* doc = doc_of(handle);
    yeptris_node* t = node_of(target);
    if (doc == NULL || t == NULL || t->doc != doc || name == NULL) {
        return NULL;
    }
    uint32_t id = yep_mut_new_alias(doc->dom, t->id, name, len);
    return id == UINT32_MAX ? NULL : (YeptrisNode)yep_handle_new(doc, id);
}

YEPTRIS_API int yeptris_node_map_del(YeptrisNode handle, const char* key, size_t key_len) {
    yeptris_node* m = node_of(handle);
    if (m == NULL || key == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    return yep_mut_map_del(m->doc->dom, m->id, key, key_len) == 0 ? YEPTRIS_OK
                                                                  : YEPTRIS_ERROR_PARSE;
}
