/* emit.c — public serialization API (TODO.impl/13).
 *
 * Two passes of the ONE writer: dry (exact size), wet (linear writes
 * into the caller's buffer — zero reallocations by construction). */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/emit.h"
#include "../doc.h"
#include "writer.h"

static int opts_canonical(const yeptris_emit_options* opts) {
    return opts != NULL && opts->size >= sizeof(*opts) && opts->canonical;
}

YEPTRIS_API size_t yeptris_serialize_into_ex(YeptrisDocument handle,
                                             const yeptris_emit_options* opts, char* buf,
                                             size_t cap) {
    if (handle == NULL) {
        return 0;
    }
    yep_emitter em;
    em.doc = (const yeptris_document*)handle;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = opts_canonical(opts);
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return 0;
    }
    size_t need = yep_emit_run(&em, 1);
    if (buf == NULL || cap < need + 1) {
        yep_nametab_free(&em.canon_names);
        return need;
    }
    em.w.p = buf;
    size_t wrote = yep_emit_run(&em, 0);
    yep_nametab_free(&em.canon_names);
    buf[wrote] = '\0';
    return wrote;
}

YEPTRIS_API char* yeptris_serialize_ex(YeptrisDocument handle, const yeptris_emit_options* opts,
                                       size_t* len) {
    if (handle == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.doc = (const yeptris_document*)handle;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = opts_canonical(opts);
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        return NULL;
    }
    em.w.p = out;
    size_t wrote = yep_emit_run(&em, 0);
    yep_nametab_free(&em.canon_names);
    out[wrote] = '\0';
    if (len != NULL) {
        *len = wrote;
    }
    return out;
}

YEPTRIS_API size_t yeptris_serialize_into(YeptrisDocument handle, char* buf, size_t cap) {
    return yeptris_serialize_into_ex(handle, NULL, buf, cap);
}

YEPTRIS_API char* yeptris_serialize(YeptrisDocument handle, size_t* len) {
    return yeptris_serialize_ex(handle, NULL, len);
}
