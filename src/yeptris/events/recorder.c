/* recorder.c — chunked event recorder (TODO.impl/12).
 *
 * Feed chunks, drain bulk: fixed-size records plus one string arena,
 * reset at every feed entry. The engine's resumable stepping (07)
 * advances the parse as complete documents arrive — each feed's
 * records are the events for documents closed by that chunk, so a
 * stream watcher holds one document of memory, not the whole stream. */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/events.h"
#include "../parse/engine.h"
#include "../resolve/resolver.h"
#include "capture.h"

struct yeptris_recorder {
    yep_rec_store store;
    yep_engine* eng; /* created on first feed; steps per feed */
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
        return YEPTRIS_ERROR_ARG; /* one stream per recorder */
    }
    if (rec->eng == NULL) {
        rec->eng = yep_engine_create(yep_system_allocator());
        if (rec->eng == NULL) {
            return YEPTRIS_ERROR_MEMORY;
        }
        yep_engine_set_resolver(rec->eng,
                                rec->compat11 ? yep_resolver_compat11() : yep_resolver_core12());
    }
    if (final) {
        rec->final_fed = 1;
    }
    yep_rec_reset(&rec->store);
    yep_sink sink = {yep_rec_on_event, &rec->store};
    int rc = yep_engine_step(rec->eng, chunk, len, final, &sink);
    if (rc != 0) {
        const yep_error* err = yep_engine_error(rec->eng);
        if (err != NULL) {
            rec->err_line = err->line;
            rec->err_col = err->col;
        }
        rec->final_fed = 1; /* a parse error kills the stream */
        rec->status = YEPTRIS_ERROR_PARSE;
        return rec->status;
    }
    rec->status = YEPTRIS_OK;
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
    yep_engine_destroy(rec->eng);
    yep_rec_free(&rec->store);
    free(rec);
}
