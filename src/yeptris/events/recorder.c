/* recorder.c — chunked event recorder (TODO.impl/12).
 *
 * Feed chunks, drain bulk: fixed-size records plus one string arena,
 * reset at every feed entry. v1 buffers input until the final chunk
 * (the engine is whole-buffer); the streaming feed machinery keeps
 * this exact wire shape when the engine gains resumable stepping. */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/events.h"
#include "../parse/engine.h"
#include "../resolve/resolver.h"
#include "capture.h"

struct yeptris_recorder {
    yep_rec_store store;
    char* input;
    size_t input_len, input_cap;
    int ran;
    int final_fed;
    int compat11;
    YeptrisStatus status;
    uint32_t err_line;
    uint32_t err_col;
};

YEPTRIS_API YeptrisRecorder yeptris_recorder_new(void) {
    return yeptris_recorder_new_ex(YEPTRIS_SCHEMA_12_CORE);
}

YEPTRIS_API YeptrisRecorder yeptris_recorder_new_ex(YeptrisSchema schema) {
    struct yeptris_recorder* r = calloc(1, sizeof(*r));
    if (r != NULL) {
        yep_rec_init(&r->store);
        r->compat11 = schema == YEPTRIS_SCHEMA_11_COMPAT;
    }
    return r;
}

YEPTRIS_API YeptrisStatus yeptris_recorder_feed(YeptrisRecorder rec, const char* chunk, size_t len,
                                                int final) {
    if (rec == NULL || (chunk == NULL && len != 0)) {
        return YEPTRIS_ERROR_ARG;
    }
    if (rec->final_fed) {
        return YEPTRIS_ERROR_ARG; /* one document per recorder (v1) */
    }
    if (len > 0) {
        if (rec->input_len + len > rec->input_cap) {
            size_t cap = rec->input_cap ? rec->input_cap : 4096;
            while (rec->input_len + len > cap) {
                cap *= 2;
            }
            char* nb = realloc(rec->input, cap);
            if (nb == NULL) {
                return YEPTRIS_ERROR_MEMORY;
            }
            rec->input = nb;
            rec->input_cap = cap;
        }
        memcpy(rec->input + rec->input_len, chunk, len);
        rec->input_len += len;
    }
    if (!final) {
        return YEPTRIS_OK;
    }
    rec->final_fed = 1;
    yep_rec_reset(&rec->store);
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    if (eng == NULL) {
        return YEPTRIS_ERROR_MEMORY;
    }
    yep_engine_set_resolver(eng, rec->compat11 ? yep_resolver_compat11() : yep_resolver_core12());
    yep_sink sink = {yep_rec_on_event, &rec->store};
    int rc = yep_engine_run(eng, rec->input, rec->input_len, &sink);
    const yep_error* err = yep_engine_error(eng);
    if (err != NULL && rc != 0) {
        rec->err_line = err->line;
        rec->err_col = err->col;
    }
    yep_engine_destroy(eng);
    rec->status = rc == 0 ? YEPTRIS_OK : YEPTRIS_ERROR_PARSE;
    return rec->status;
}

YEPTRIS_API const YeptrisEventRecord* yeptris_recorder_records(YeptrisRecorder rec, size_t* count) {
    if (rec == NULL) {
        return NULL;
    }
    if (count != NULL) {
        *count = rec->store.n;
    }
    return rec->store.recs;
}

YEPTRIS_API const char* yeptris_recorder_arena(YeptrisRecorder rec, size_t* len) {
    if (rec == NULL) {
        return NULL;
    }
    if (len != NULL) {
        *len = rec->store.arena_len;
    }
    return rec->store.arena;
}

YEPTRIS_API void yeptris_recorder_free(YeptrisRecorder rec) {
    if (rec == NULL) {
        return;
    }
    yep_rec_free(&rec->store);
    free(rec->input);
    free(rec);
}
