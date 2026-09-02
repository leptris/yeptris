/* doc.h — the internal document shape (TODO.impl/11/13).
 *
 * Private: parse.c builds it, the emitter reads it. The public header
 * only ever sees the opaque handle. */
#ifndef YEP_DOC_H
#define YEP_DOC_H

#include <stddef.h>

#include "dom/dom.h"
#include "memory/allocator.h"

typedef struct yeptris_document {
    yep_dom* dom;
    const yep_allocator* sys;
    unsigned char* transcoded; /* owned when non-NULL */
    size_t transcoded_len;
    const char* input; /* borrowed input (lifetime documentation) */
    void* finish_pool; /* engine finish pool: resolved tags, folded and
                          escaped scalars outlive the engine through the
                          document */
} yeptris_document;

/* Node handle: a (document, node-id) pair so nodes stay usable even if
 * the node pool grows (ids are stable; pointers are not). Defined here
 * (not parse.c) since the query layer and the builder share it. */
typedef struct yeptris_node {
    yeptris_document* doc;
    uint32_t id;
} yeptris_node;

yeptris_node* yep_handle_new(yeptris_document* doc, uint32_t id);

#endif /* YEP_DOC_H */
