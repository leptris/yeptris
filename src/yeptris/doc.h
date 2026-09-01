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

#endif /* YEP_DOC_H */
