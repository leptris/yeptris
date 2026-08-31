/* parse.c — public API glue: encoding front-end → engine → DOM. */

#include <string.h>

#include "dom/dom.h"
#include "encoding/encoding.h"
#include "memory/allocator.h"
#include "memory/pool.h"
#include "parse/engine.h"
#include "parse/events.h"

#include <yeptris.h>

/* Document handle: the DOM plus the owning allocator and transcoded
 * buffer (when the input was not UTF-8). */
typedef struct yeptris_document {
    yep_dom* dom;
    const yep_allocator* sys;
    unsigned char* transcoded; /* owned when non-NULL */
    size_t transcoded_len;
    const char* input; /* borrowed input (for lifetime documentation) */
    void* finish_pool; /* engine finish pool: resolved tags, folded and
                          escaped scalars outlive the engine through the
                          document (ASAN: heap-use-after-free otherwise) */
} yeptris_document;

/* Node handle: a (document, node-id) pair so nodes stay usable even if
 * the node pool grows (ids are stable; pointers are not). */
typedef struct yeptris_node {
    yeptris_document* doc;
    uint32_t id;
} yeptris_node;

YEPTRIS_API const char* yeptris_last_error(uint32_t* line, uint32_t* col) {
    const yep_error* tls = yep_error_tls();
    if (line != NULL) {
        *line = tls->line;
    }
    if (col != NULL) {
        *col = tls->col;
    }
    return tls->msg;
}

YEPTRIS_API YeptrisDocument yeptris_parse(const char* buf, size_t len, YeptrisStatus* status) {
    YeptrisStatus st = YEPTRIS_OK;
    if (buf == NULL && len != 0) {
        st = YEPTRIS_ERROR_ARG;
        goto fail;
    }

    const yep_allocator* sys = yep_system_allocator();

    /* Encoding front-end: BOM sniff; borrow UTF-8, transcode the rest. */
    yep_encoding enc = YEP_ENC_UNKNOWN;
    size_t bom = yep_bom_sniff((const unsigned char*)buf, len, &enc);
    const char* data = buf + bom;
    size_t data_len = len - bom;

    unsigned char* transcoded = NULL;
    size_t transcoded_len = 0;
    if (enc == YEP_ENC_UTF16LE || enc == YEP_ENC_UTF16BE || enc == YEP_ENC_UTF32LE ||
        enc == YEP_ENC_UTF32BE) {
        size_t terr = 0;
        int rc = yep_transcode_to_utf8(sys, enc, (const unsigned char*)data, data_len, &transcoded,
                                       &transcoded_len, &terr);
        if (rc != 0) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, terr,
                          "ill-formed input encoding");
            yep_free(sys, transcoded);
            st = YEPTRIS_ERROR_ENCODING;
            goto fail;
        }
        data = (const char*)transcoded;
        data_len = transcoded_len;
    } else {
        size_t verr = 0;
        if (!yep_utf8_validate((const unsigned char*)data, data_len, &verr)) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, verr,
                          "ill-formed UTF-8 at byte %zu", verr);
            st = YEPTRIS_ERROR_ENCODING;
            goto fail;
        }
    }

    /* Engine → DOM. */
    yep_engine* eng = yep_engine_create(sys);
    if (eng == NULL) {
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }
    yep_dom* dom = yep_dom_create(sys);
    if (dom == NULL) {
        yep_engine_destroy(eng);
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }

    yep_sink sink = {yep_dom_on_event, dom};
    int rc = yep_engine_run(eng, data, data_len, &sink);
    if (rc != 0) {
        const yep_error* ee = yep_engine_error(eng);
        yep_err_code code = ee ? ee->code : YEP_ERR_UNEXPECTED;
        if (ee) {
            yep_error* tls = yep_error_tls();
            *tls = *ee;
        }
        yep_dom_destroy(dom);
        yep_engine_destroy(eng);
        yep_free(sys, transcoded);
        st = (code == YEP_ERR_MEMORY)  ? YEPTRIS_ERROR_MEMORY
             : (code == YEP_ERR_DEPTH) ? YEPTRIS_ERROR_DEPTH
                                       : YEPTRIS_ERROR_PARSE;
        goto fail;
    }
    /* Resolved tags, folded and escaped scalars live in the engine's
     * finish pool; the document must own it or every such string would
     * dangle at engine teardown (found by ASAN). */
    yep_pool* finish = yep_engine_detach_pool(eng);
    yep_engine_destroy(eng);

    /* Empty stream (no documents): NULL document with YEPTRIS_OK. */
    if (dom->dcount == 0) {
        yep_dom_destroy(dom);
        yep_pool_destroy(finish);
        yep_free(sys, transcoded);
        if (status != NULL) {
            *status = YEPTRIS_OK;
        }
        return NULL;
    }

    yeptris_document* doc = yep_alloc(sys, sizeof(yeptris_document));
    if (doc == NULL) {
        yep_dom_destroy(dom);
        yep_pool_destroy(finish);
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }
    doc->dom = dom;
    doc->sys = sys;
    doc->transcoded = transcoded;
    doc->transcoded_len = transcoded_len;
    doc->input = buf;
    doc->finish_pool = finish;
    return (YeptrisDocument)doc;

fail:
    if (status != NULL) {
        *status = st;
    }
    return NULL;
}

YEPTRIS_API void yeptris_document_free(YeptrisDocument handle) {
    yeptris_document* doc = (yeptris_document*)handle;
    if (doc == NULL) {
        return;
    }
    yep_dom_destroy(doc->dom);
    yep_pool_destroy((yep_pool*)doc->finish_pool);
    yep_free(doc->sys, doc->transcoded);
    yep_free(doc->sys, doc);
}

YEPTRIS_API size_t yeptris_document_count(YeptrisDocument handle) {
    yeptris_document* doc = (yeptris_document*)handle;
    return doc ? doc->dom->dcount : 0;
}

static YeptrisNode node_new_handle(yeptris_document* doc, uint32_t id) {
    yeptris_node* n = yep_pool_alloc(doc->dom->pool, sizeof(yeptris_node), 16);
    if (n == NULL) {
        return NULL;
    }
    n->doc = doc;
    n->id = id;
    return (YeptrisNode)n;
}

YEPTRIS_API YeptrisNode yeptris_document_root(YeptrisDocument handle, size_t index) {
    yeptris_document* doc = (yeptris_document*)handle;
    if (doc == NULL || index >= doc->dom->dcount) {
        return NULL;
    }
    return node_new_handle(doc, doc->dom->docs[index]);
}

static const yep_dnode* node_of(YeptrisNode handle) {
    yeptris_node* n = (yeptris_node*)handle;
    if (n == NULL || n->doc == NULL) {
        return NULL;
    }
    return yep_dom_node(n->doc->dom, n->id);
}

YEPTRIS_API YeptrisNodeKind yeptris_node_kind(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL) {
        return YEPTRIS_NODE_SCALAR;
    }
    switch (n->kind) {
    case YEP_DOM_SEQUENCE:
        return YEPTRIS_NODE_SEQUENCE;
    case YEP_DOM_MAPPING:
        return YEPTRIS_NODE_MAPPING;
    case YEP_DOM_ALIAS:
        return YEPTRIS_NODE_ALIAS;
    default:
        return YEPTRIS_NODE_SCALAR;
    }
}

static const char* view_out(yep_view v, size_t* len) {
    if (len != NULL) {
        *len = v.len;
    }
    return v.len ? v.p : (v.p ? v.p : "");
}

YEPTRIS_API const char* yeptris_node_value(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || (n->kind != YEP_DOM_SCALAR && n->kind != YEP_DOM_ALIAS)) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    return view_out(n->value, len);
}

YEPTRIS_API YeptrisScalarStyle yeptris_node_style(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return n ? (YeptrisScalarStyle)n->style : YEPTRIS_STYLE_ANY;
}

YEPTRIS_API const char* yeptris_node_tag(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->tag.len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return n ? NULL : NULL;
    }
    return view_out(n->tag, len);
}

YEPTRIS_API const char* yeptris_node_anchor(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->anchor.len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    return view_out(n->anchor, len);
}

static YeptrisNode wrap(yeptris_node* base, uint32_t id) {
    if (base == NULL || base->doc == NULL) {
        return NULL;
    }
    /* Handles live in the document's pool: one document_free reclaims
     * every handle ever handed out (zero-leak contract). */
    yeptris_node* n = yep_pool_alloc(base->doc->dom->pool, sizeof(yeptris_node), 16);
    if (n == NULL) {
        return NULL;
    }
    n->doc = base->doc;
    n->id = id;
    return (YeptrisNode)n;
}

YEPTRIS_API YeptrisNode yeptris_node_alias_target(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_ALIAS || n->target == UINT32_MAX) {
        return NULL;
    }
    return wrap((yeptris_node*)handle, n->target);
}

YEPTRIS_API size_t yeptris_node_seq_count(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return (n && n->kind == YEP_DOM_SEQUENCE) ? n->count : 0;
}

YEPTRIS_API YeptrisNode yeptris_node_seq_at(YeptrisNode handle, size_t index) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_SEQUENCE || index >= n->count) {
        return NULL;
    }
    const yep_dom* dom = ((yeptris_node*)handle)->doc->dom;
    uint32_t id = n->first_child;
    for (size_t i = 0; i < index && id != UINT32_MAX; i++) {
        const yep_dnode* cur = yep_dom_node(dom, id);
        id = cur ? cur->next_sibling : UINT32_MAX;
    }
    return wrap((yeptris_node*)handle, id);
}

YEPTRIS_API size_t yeptris_node_map_count(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return (n && n->kind == YEP_DOM_MAPPING) ? n->count / 2 : 0;
}

YEPTRIS_API YeptrisNode yeptris_node_map_get(YeptrisNode handle, const char* key, size_t key_len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_MAPPING || key == NULL) {
        return NULL;
    }
    const yep_dom* dom = ((yeptris_node*)handle)->doc->dom;
    uint32_t id = n->first_child;
    int want_key = 1;
    while (id != UINT32_MAX) {
        const yep_dnode* cur = yep_dom_node(dom, id);
        if (cur == NULL) {
            break;
        }
        if (want_key && cur->kind == YEP_DOM_SCALAR) {
            yep_view kv = {key, (uint32_t)key_len};
            if (yep_view_eq(cur->value, kv)) {
                return wrap((yeptris_node*)handle, cur->next_sibling);
            }
        }
        want_key = !want_key;
        id = cur->next_sibling;
    }
    return NULL;
}
