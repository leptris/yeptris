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
    int schema;                /* the parse's schema (YEPTRIS_SCHEMA_*): document
                                * property — host policies (Psych float quirks) are
                                * conditioned on it (TODO.restructure/32) */
    unsigned char* transcoded; /* owned when non-NULL */
    size_t transcoded_len;
    const char* input; /* borrowed input (lifetime documentation) */
    void* finish_pool; /* engine finish pool: resolved tags, folded and
                          escaped scalars outlive the engine through the
                          document */
    void* lazy_tape;   /* #342 slice 2: the parsed tape when the tree is
                        * deferred (gate-clean strict JSON). dom==NULL until
                        * the first tree access materializes via dom_from_tape;
                        * freed with the document. void* to avoid a header
                        * cycle — parse.c casts to yeptris_json_tape* */
} yeptris_document;

/* Node handle: a (document, node-id) pair so nodes stay usable even if
 * the node pool grows (ids are stable; pointers are not). Defined here
 * (not parse.c) since the query layer and the builder share it. */
/* #342 slice 2: the lazy-tree choke point. Returns the document's dom,
 * materializing it from lazy_tape on first access (NULL when materialization
 * fails — callers treat it as an empty/unusable tree). Every consumer that
 * reads ->dom on a parse-produced document routes through here; the builder
 * and CBOR decode paths set dom eagerly and are unaffected. parse.c owns it. */
typedef struct yeptris_node {
    yeptris_document* doc;
    uint32_t id;
} yeptris_node;

#ifdef __cplusplus
extern "C" {
#endif

yep_dom* yep_doc_dom(yeptris_document* doc);
yeptris_node* yep_handle_new(yeptris_document* doc, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* YEP_DOC_H */
