/* values.c — the typed value stream (TODO.impl/15 phase F).
 *
 * One engine run through the shared capture sink, then one C pass
 * converting every scalar by its resolver tag through the number
 * kernels (parse/numbers.c — the same converters the node typed
 * accessors ride). The host walks a flat typed array: no per-scalar
 * parsing, no pending-key bookkeeping (is_key), anchor names arrive
 * as uniform YEP_V_ANCHOR entries decorating the value they bind. */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/values.h"
#include "../parse/engine.h"
#include "../parse/numbers.h"
#include "../resolve/resolver.h"
#include "capture.h"

#define YEP_V_MAX_DEPTH 1024

typedef struct {
    yep_rec_store store;
    YeptrisValue* vals;
    size_t n, cap;
    char* arena;
    size_t arena_len, arena_cap;
    int oom;
    uint8_t key_pend[YEP_V_MAX_DEPTH]; /* per open map: key slot taken */
    int depth;
} yep_value_ctx;

static uint32_t arena_put(yep_value_ctx* c, const char* p, uint32_t len) {
    if (c->arena_len + len + 1 > c->arena_cap) {
        size_t cap = c->arena_cap ? c->arena_cap : 256;
        while (cap < c->arena_len + len + 1) {
            cap *= 2;
        }
        char* na = realloc(c->arena, cap);
        if (na == NULL) {
            c->oom = 1;
            return 0;
        }
        c->arena = na;
        c->arena_cap = cap;
    }
    if (len != 0) {
        memcpy(c->arena + c->arena_len, p, len);
    }
    uint32_t off = (uint32_t)c->arena_len;
    c->arena_len += len;
    c->arena[c->arena_len++] = '\0'; /* names read as C strings */
    return off;
}

static void put(yep_value_ctx* c, const YeptrisValue* v) {
    if (c->n == c->cap) {
        size_t cap = c->cap ? c->cap * 2 : 256;
        YeptrisValue* nv = realloc(c->vals, cap * sizeof(*nv));
        if (nv == NULL) {
            c->oom = 1;
            return;
        }
        c->vals = nv;
        c->cap = cap;
    }
    c->vals[c->n++] = *v;
}

/* The map slot rule: in a mapping, entries alternate key/value — the
 * entry landing in an empty key slot is the key (is_key set), the
 * next completes the pair. Applies to scalars, aliases, and
 * collection opens (complex keys) alike. */
static void slot(yep_value_ctx* c, YeptrisValue* v) {
    if (c->depth > 0 && c->depth <= YEP_V_MAX_DEPTH) {
        int di = c->depth - 1;
        if (c->key_pend[di] == 0) {
            v->is_key = 1;
            c->key_pend[di] = 1;
        } else {
            c->key_pend[di] = 0;
        }
    }
}

static void anchor_entry(yep_value_ctx* c, const char* name, uint32_t len) {
    YeptrisValue a = {YEP_V_ANCHOR, 0, 0, 0, 0, len, 0};
    a.off = arena_put(c, name, len);
    put(c, &a);
}

static int transform(yep_value_ctx* c) {
    const YeptrisEventRecord* rs = c->store.recs;
    const char* ra = c->store.arena ? c->store.arena : "";
    for (size_t i = 0; i < c->store.n; i++) {
        const YeptrisEventRecord* r = &rs[i];
        if (r->anchor_len != 0 && r->type != YEPTRIS_EV_ALIAS) {
            anchor_entry(c, ra + r->anchor_off, r->anchor_len);
        }
        YeptrisValue v = {0, 0, 0, 0, 0, 0, 0};
        switch (r->type) {
        case YEPTRIS_EV_STREAM_START:
        case YEPTRIS_EV_STREAM_END:
            continue;
        case YEPTRIS_EV_DOCUMENT_START:
            v.kind = YEP_V_DOC;
            put(c, &v);
            continue;
        case YEPTRIS_EV_SEQUENCE_START:
            v.kind = YEP_V_SEQ_OPEN;
            v.tag_id = r->tag_id;
            slot(c, &v);
            put(c, &v);
            if (c->depth < YEP_V_MAX_DEPTH) {
                c->key_pend[c->depth] = 0;
            }
            c->depth++;
            continue;
        case YEPTRIS_EV_MAPPING_START:
            v.kind = YEP_V_MAP_OPEN;
            v.tag_id = r->tag_id;
            slot(c, &v);
            put(c, &v);
            if (c->depth < YEP_V_MAX_DEPTH) {
                c->key_pend[c->depth] = 0;
            }
            c->depth++;
            continue;
        case YEPTRIS_EV_SEQUENCE_END:
        case YEPTRIS_EV_MAPPING_END:
            v.kind = YEP_V_CLOSE;
            put(c, &v);
            c->depth--;
            continue;
        case YEPTRIS_EV_ALIAS:
            v.kind = YEP_V_ALIAS;
            v.off = arena_put(c, ra + r->value_off, r->value_len);
            v.len = r->value_len;
            slot(c, &v);
            put(c, &v);
            continue;
        case YEPTRIS_EV_SCALAR: {
            const char* text = ra + r->value_off;
            uint32_t len = r->value_len;
            int64_t vi = 0;
            double vd = 0.0;
            v.tag_id = r->tag_id;
            /* EVERY scalar carries its raw bytes: hosts with schema
             * quirks (Psych's single-char y/n, PyYAML's dot-required
             * floats) re-decide from tag_id + text without re-running
             * a conversion grammar */
            v.off = arena_put(c, text, len);
            v.len = len;
            if (r->tag_id != YEPTRIS_TAG_BOOL) {
                /* b doubles as the implicit-plain flag for the other
                 * kinds (host symbol scans and friends key on it) */
                v.b = (r->flags & 4) ? 1 : 0;
            }
            switch (r->tag_id) {
            case YEPTRIS_TAG_NULL:
                v.kind = YEP_V_NULL;
                break;
            case YEPTRIS_TAG_BOOL:
                v.kind = YEP_V_BOOL;
                v.b = (uint8_t)yep_num_bool_ci(text, len);
                break;
            case YEPTRIS_TAG_INT:
                v.kind = YEP_V_INT;
                if (yep_num_i64(text, len, &vi) != 0) {
                    /* the resolver tagged it but the kernel rejects:
                     * degrade to STR — host policy decides */
                    v.kind = YEP_V_STR;
                    v.p = 0;
                } else {
                    v.p = (uint64_t)vi;
                }
                break;
            case YEPTRIS_TAG_FLOAT:
                v.kind = YEP_V_FLOAT;
                if (yep_num_f64(text, len, &vd) != 0) {
                    v.kind = YEP_V_STR;
                } else {
                    memcpy(&v.p, &vd, sizeof(v.p));
                }
                break;
            case YEPTRIS_TAG_TIMESTAMP:
                v.kind = YEP_V_TIMESTAMP;
                break;
            default:
                v.kind = YEP_V_STR;
                break;
            }
            slot(c, &v);
            put(c, &v);
            continue;
        }
        default:
            continue;
        }
    }
    return c->oom ? -1 : 0;
}

YEPTRIS_API YeptrisStatus yeptris_value_drain(const char* yaml, size_t len, YeptrisSchema schema,
                                              YeptrisValue** vals, size_t* count, char** arena,
                                              size_t* arena_len) {
    if (vals == NULL || count == NULL || arena == NULL || arena_len == NULL ||
        (yaml == NULL && len != 0)) {
        return YEPTRIS_ERROR_ARG;
    }
    *vals = NULL;
    *count = 0;
    *arena = NULL;
    *arena_len = 0;

    yep_engine* eng = yep_engine_create(yep_system_allocator());
    if (eng == NULL) {
        return YEPTRIS_ERROR_MEMORY;
    }
    yep_engine_set_resolver(eng, schema == YEPTRIS_SCHEMA_11_COMPAT ? yep_resolver_compat11()
                                                                    : yep_resolver_core12());

    yep_value_ctx* c = calloc(1, sizeof(*c));
    if (c == NULL) {
        yep_engine_destroy(eng);
        return YEPTRIS_ERROR_MEMORY;
    }
    yep_rec_init(&c->store);

    YeptrisStatus st = YEPTRIS_OK;
    yep_sink sink = {yep_rec_on_event, &c->store};
    if (yep_engine_run(eng, yaml, len, &sink) != 0) {
        st = YEPTRIS_ERROR_PARSE;
    } else if (transform(c) != 0) {
        st = YEPTRIS_ERROR_MEMORY;
    }
    yep_engine_destroy(eng);
    yep_rec_free(&c->store);
    if (st != YEPTRIS_OK) {
        free(c->vals);
        free(c->arena);
        free(c);
        return st;
    }
    if (c->arena == NULL) {
        c->arena = malloc(1); /* non-NULL so hosts can free blindly */
        if (c->arena == NULL) {
            free(c->vals);
            free(c);
            return YEPTRIS_ERROR_MEMORY;
        }
    }
    *vals = c->vals;
    *count = c->n;
    *arena = c->arena;
    *arena_len = c->arena_len;
    free(c);
    return YEPTRIS_OK;
}

YEPTRIS_API void yeptris_value_free(YeptrisValue* vals, char* arena) {
    free(vals);
    free(arena);
}
