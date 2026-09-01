/* emit.c — public serialization API (TODO.impl/13).
 *
 * Two passes of the ONE writer: dry (exact size), wet (linear writes
 * into the caller's buffer — zero reallocations by construction). */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/emit.h"
#include "../doc.h"
#include "writer.h"

YEPTRIS_API size_t yeptris_serialize_into(YeptrisDocument handle, char* buf, size_t cap) {
    if (handle == NULL) {
        return 0;
    }
    yep_emitter em;
    em.doc = (const yeptris_document*)handle;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    size_t need = yep_emit_run(&em, 1);
    if (buf == NULL || cap < need + 1) {
        return need;
    }
    em.w.p = buf;
    size_t wrote = yep_emit_run(&em, 0);
    buf[wrote] = '\0';
    return wrote;
}

YEPTRIS_API char* yeptris_serialize(YeptrisDocument handle, size_t* len) {
    if (handle == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.doc = (const yeptris_document*)handle;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        return NULL;
    }
    em.w.p = out;
    size_t wrote = yep_emit_run(&em, 0);
    out[wrote] = '\0';
    if (len != NULL) {
        *len = wrote;
    }
    return out;
}
