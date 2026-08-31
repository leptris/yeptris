/* resolver.h — the internal resolver interface (TODO.impl/10).
 *
 * MECE: scan produces facts, the engine parses, the RESOLVER alone
 * decides implicit typing. OCP: a new schema is a new resolver
 * instance; no schema branches exist anywhere else.
 */
#ifndef YEP_RESOLVER_H
#define YEP_RESOLVER_H

#include <stdint.h>

#include "common/string_view.h"

/* yep_view: bytes + length, the one string representation (SSOT). */

/* The tag-id space is the public YeptrisTagId (resolve.h); internal
 * re-export for the table SSOT. */
typedef uint8_t yep_tag_id;

typedef struct yep_resolver {
    /* Returns the implicit tag for a PLAIN, untagged scalar. */
    yep_tag_id (*resolve)(void* ctx, const char* p, uint32_t len);
    void* ctx;
} yep_resolver;

/* The two built-in resolvers (no ctx). */
const yep_resolver* yep_resolver_core12(void);
const yep_resolver* yep_resolver_compat11(void);

/* core tag id <-> canonical URI (tags.c is the identity SSOT). */
const char* yep_tag_uri(yep_tag_id id);

/* Explicit-tag resolution: the tag URI a node carried maps to a core
 * id when it matches one, else YEPTRIS_TAG_CUSTOM. */
yep_tag_id yep_tag_from_uri(const char* p, uint32_t len);

#endif /* YEP_RESOLVER_H */
