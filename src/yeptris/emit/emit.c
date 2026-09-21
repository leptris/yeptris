/* emit.c — public serialization API (TODO.impl/13).
 *
 * Two passes of the ONE writer: dry (exact size), wet (linear writes
 * into the caller's buffer — zero reallocations by construction). */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/emit.h"
#include "../doc.h"
#include "writer.h"

static int opts_canonical(const yeptris_emit_options* opts) {
    return opts != NULL && opts->size >= sizeof(*opts) && opts->canonical;
}

static int opts_width(const yeptris_emit_options* opts) {
    if (opts == NULL || opts->size < sizeof(*opts) || opts->best_width <= 0) {
        return 80; /* libyaml best_width parity */
    }
    return opts->best_width;
}

static int opts_doc_marker(const yeptris_emit_options* opts) {
    /* size-versioned: callers compiled against the older struct keep
     * the implicit single-document behavior */
    if (opts == NULL ||
        opts->size < (uint32_t)(offsetof(yeptris_emit_options, explicit_doc_start) + sizeof(int))) {
        return 0;
    }
    return opts->explicit_doc_start != 0;
}

YEPTRIS_API size_t yeptris_serialize_into_ex(YeptrisDocument handle,
                                             const yeptris_emit_options* opts, char* buf,
                                             size_t cap) {
    if (handle == NULL) {
        return 0;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = (const yeptris_document*)handle;
    if (yep_doc_dom((yeptris_document*)handle) == NULL) { /* #342 lazy */
        return 0;
    }
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = opts_canonical(opts);
    em.w.explicit_doc_start = opts_doc_marker(opts);
    em.w.json = 0;
    em.w.json_compact = 0;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = opts_width(opts);
    em.w.col = 0;
    em.w.sv_input = handle ? ((const yeptris_document*)handle)->dom->input_base : NULL;
    em.w.sv_arena = handle ? ((const yeptris_document*)handle)->dom->str : NULL;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return 0;
    }
    em.w.sc_dec = (uint8_t*)malloc(em.doc->dom->ncount > 0 ? em.doc->dom->ncount : 1);
    if (em.w.sc_dec == NULL) {
        yep_nametab_free(&em.canon_names);
        return 0;
    }
    size_t need = yep_emit_run(&em, 1);
    if (buf == NULL || cap < need + 1) {
        free(em.w.sc_dec);
        yep_nametab_free(&em.canon_names);
        return need;
    }
    em.w.p = buf;
    size_t wrote = yep_emit_run(&em, 0);
    free(em.w.sc_dec);
    yep_nametab_free(&em.canon_names);
    buf[wrote] = '\0';
    return wrote;
}

YEPTRIS_API void yeptris_free(void* p) {
    free(p);
}

YEPTRIS_API char* yeptris_serialize_ex(YeptrisDocument handle, const yeptris_emit_options* opts,
                                       size_t* len) {
    if (handle == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = (const yeptris_document*)handle;
    if (yep_doc_dom((yeptris_document*)handle) == NULL) { /* #342 lazy */
        return NULL;
    }
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = opts_canonical(opts);
    em.w.explicit_doc_start = opts_doc_marker(opts);
    em.w.json = 0;
    em.w.json_compact = 0;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = opts_width(opts);
    em.w.col = 0;
    em.w.sv_input = handle ? ((const yeptris_document*)handle)->dom->input_base : NULL;
    em.w.sv_arena = handle ? ((const yeptris_document*)handle)->dom->str : NULL;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    /* #352 slice 2: ONE wet pass into a growable buffer — the dry
     * exact-sizing pass measured 0.54-0.73x of this route on CI-fresh
     * runners (the emit-split table's 2p/1p column). The estimate
     * starts at the input size (+64) and doubles on demand; a final
     * shrink-to-fit keeps the returned allocation tight. No dry pass,
     * so no decisions table. */
    size_t est = em.doc->dom->input_len + 64;
    if (est < 256) {
        est = 256;
    }
    char* out = (char*)malloc(est);
    if (out == NULL) {
        yep_nametab_free(&em.canon_names);
        return NULL;
    }
    em.w.p = out;
    em.w.cap = est;
    em.w.grow = 1;
    size_t wrote = yep_emit_run(&em, 0);
    em.w.grow = 0;
    /* wr_grow reallocs through w->p — the local `out` is stale after
     * any growth; every post-run touch goes through the writer's
     * pointer (the first round's abort was exactly this aliasing) */
    out = em.w.p;
    if (em.w.oom) {
        free(out);
        yep_nametab_free(&em.canon_names);
        return NULL;
    }
    if (wrote + 1 < em.w.cap) {
        char* tight = (char*)realloc(out, wrote + 1);
        if (tight != NULL) {
            out = tight;
        }
    }
    yep_nametab_free(&em.canon_names);
    out[wrote] = '\0';
    if (len != NULL) {
        *len = wrote;
    }
    return out;
}

YEPTRIS_API char* yeptris_serialize_json(YeptrisDocument handle, size_t* len) {
    if (handle == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = (const yeptris_document*)handle;
    if (yep_doc_dom((yeptris_document*)handle) == NULL) { /* #342 lazy */
        return NULL;
    }
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = 0;
    em.w.explicit_doc_start = 0; /* JSON output carries no --- */
    em.w.json = 1;
    em.w.json_compact = 0;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = 0; /* JSON: no folding (single-line output) */
    em.w.col = 0;
    em.w.sv_input = handle ? ((const yeptris_document*)handle)->dom->input_base : NULL;
    em.w.sv_arena = handle ? ((const yeptris_document*)handle)->dom->str : NULL;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        yep_nametab_free(&em.canon_names);
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

YEPTRIS_API char* yeptris_serialize_json_ex(YeptrisDocument handle, size_t* len, int compact) {
    if (handle == NULL) {
        return NULL;
    }
    if (handle == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = (const yeptris_document*)handle;
    if (yep_doc_dom((yeptris_document*)handle) == NULL) { /* #342 lazy */
        return NULL;
    }
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = 0;
    em.w.explicit_doc_start = 0; /* JSON output carries no --- */
    em.w.json = 1;
    em.w.json_compact = compact ? 1 : 0;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = 0; /* JSON: no folding (single-line output) */
    em.w.col = 0;
    em.w.sv_input = handle ? ((const yeptris_document*)handle)->dom->input_base : NULL;
    em.w.sv_arena = handle ? ((const yeptris_document*)handle)->dom->str : NULL;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        yep_nametab_free(&em.canon_names);
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

char* yep_serialize_json_compact(const yeptris_document* doc, size_t* len) {
    if (doc == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = doc;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = 0;
    em.w.explicit_doc_start = 0; /* JSON output carries no --- */
    em.w.json = 1;
    em.w.json_compact = 1;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = 0;
    em.w.col = 0;
    em.w.sv_input = doc->dom->input_base;
    em.w.sv_arena = doc->dom->str;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        yep_nametab_free(&em.canon_names);
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

char* yep_serialize_json_pretty(const yeptris_document* doc, size_t* len) {
    if (doc == NULL) {
        return NULL;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = doc;
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = 0;
    em.w.explicit_doc_start = 0; /* JSON output carries no --- */
    em.w.json = 1;
    em.w.json_compact = 0;
    em.w.json_pretty = 1;
    em.w.pretty_depth = 0;
    em.w.best_width = 0;
    em.w.col = 0;
    em.w.sv_input = doc->dom->input_base;
    em.w.sv_arena = doc->dom->str;
    em.w.sink = NULL;
    em.w.watermark = 0;
    em.w.sink_aborted = 0;
    em.w.flushed = 0;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return NULL;
    }
    size_t need = yep_emit_run(&em, 1);
    char* out = malloc(need + 1);
    if (out == NULL) {
        yep_nametab_free(&em.canon_names);
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

/* Streaming writer (13C): the writer appends linearly (nothing is
 * back-patched), so flushing at a high-water mark is always safe —
 * memory stays bounded by the mark, never the document. */
YEPTRIS_API size_t yeptris_serialize_stream(YeptrisDocument handle,
                                            const yeptris_emit_options* opts,
                                            yeptris_emit_sink sink, void* ctx) {
    if (handle == NULL || sink == NULL) {
        return 0;
    }
    yep_emitter em;
    em.w.sc_dec = NULL;
    em.w.sc_i = 0;
    em.w.cap = 0;
    em.w.grow = 0;
    em.w.oom = 0;
    em.doc = (const yeptris_document*)handle;
    if (yep_doc_dom((yeptris_document*)handle) == NULL) { /* #342 lazy */
        return 0;
    }
    em.w.p = NULL;
    em.w.last = 0;
    em.w.force_flow = 0;
    em.w.canonical = opts_canonical(opts);
    em.w.explicit_doc_start = opts_doc_marker(opts);
    em.w.json = 0;
    em.w.json_compact = 0;
    em.w.json_pretty = 0;
    em.w.pretty_depth = 0;
    em.w.best_width = opts_width(opts);
    em.w.col = 0;
    em.w.sv_input = handle ? ((const yeptris_document*)handle)->dom->input_base : NULL;
    em.w.sv_arena = handle ? ((const yeptris_document*)handle)->dom->str : NULL;
    em.w.sink = sink;
    em.w.sink_ctx = ctx;
    em.w.watermark = YEP_EMIT_WATERMARK;
    if (!yep_nametab_init(&em.canon_names, yep_system_allocator())) {
        return 0;
    }
    /* one scratch window; the writer flushes it when it fills */
    char* window = malloc(YEP_EMIT_WATERMARK * 2);
    if (window == NULL) {
        yep_nametab_free(&em.canon_names);
        return 0;
    }
    em.w.p = window;
    em.w.sink_aborted = 0;
    size_t total = yep_emit_run(&em, 0);
    if (em.w.len > 0 && !em.w.sink_aborted) {
        if (sink(ctx, window, em.w.len) != 0) {
            em.w.sink_aborted = 1;
        } else {
            /* total already counted by the flush hooks */
        }
    }
    free(window);
    yep_nametab_free(&em.canon_names);
    return em.w.sink_aborted ? 0 : total;
}
