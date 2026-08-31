/* iterparse.c — document-at-a-time iteration (TODO.impl/12).
 *
 * Multi-document streams: each next() runs the engine from the current
 * cursor and captures ONE document's events (records and arena reset
 * per document — memory bounded by the largest document, not the
 * stream). The sink aborts at DOCUMENT_END; the engine's position
 * accessor gives the resume offset. */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/events.h"
#include "../parse/engine.h"
#include "capture.h"

typedef struct {
    yep_rec_store* store;
    int doc_ends; /* DOCUMENT_END events seen this run */
} yep_iter_sink;

static int iter_on_event(void* ctx, const yep_event* ev) {
    yep_iter_sink* s = (yep_iter_sink*)ctx;
    /* stream framing is synthesized by the iterator (the aborted run
     * never reaches STREAM_END); documents are the slice unit */
    if (ev->type == YEP_EV_STREAM_START || ev->type == YEP_EV_STREAM_END) {
        return 0;
    }
    if (ev->type == YEP_EV_DOCUMENT_END) {
        s->doc_ends++;
    }
    int rc = yep_rec_on_event(s->store, ev);
    if (rc != 0) {
        return rc;
    }
    if (s->doc_ends >= 1) {
        return 1; /* abort: the document is complete */
    }
    return 0;
}

struct yeptris_iterparse {
    const char* buf;
    size_t len;
    size_t cursor;
    int done;
    int emitted_stream_start;
    int pending_stream_end; /* deliver a final lone STREAM_END slice */
    YeptrisStatus status;
    uint32_t err_line;
    uint32_t err_col;
    yep_rec_store store;
    size_t next;
    YeptrisEvent* mat;
    size_t mat_cap;
    YeptrisEvent final_end;
    YeptrisEvent final_pair[2];
};

YEPTRIS_API YeptrisIterparse yeptris_iterparse_new(const char* buf, size_t len) {
    if (buf == NULL && len != 0) {
        return NULL;
    }
    struct yeptris_iterparse* it = calloc(1, sizeof(*it));
    if (it == NULL) {
        return NULL;
    }
    it->buf = buf;
    it->len = len;
    yep_rec_init(&it->store);
    return it;
}

YEPTRIS_API const YeptrisEvent* yeptris_iterparse_next(YeptrisIterparse it, size_t* count) {
    if (it == NULL) {
        return NULL;
    }
    for (;;) {
        if (it->done && it->pending_stream_end && it->next >= it->store.n) {
            it->pending_stream_end = 0;
            if (!it->emitted_stream_start) {
                /* a stream with zero documents still frames +STR */
                it->emitted_stream_start = 1;
                it->final_pair[0].type = YEPTRIS_EV_STREAM_START;
                it->final_pair[1].type = YEPTRIS_EV_STREAM_END;
                if (count != NULL) {
                    *count = 2;
                }
                return it->final_pair;
            }
            it->final_end.type = YEPTRIS_EV_STREAM_END;
            if (count != NULL) {
                *count = 1;
            }
            return &it->final_end;
        }
        if (it->next < it->store.n) {
            /* materialize the whole captured slice (one document) */
            size_t n = it->store.n;
            if (n + 1 > it->mat_cap) {
                YeptrisEvent* nm = realloc(it->mat, (n + 1) * sizeof(*nm));
                if (nm == NULL) {
                    it->done = 1;
                    it->status = YEPTRIS_ERROR_MEMORY;
                    return NULL;
                }
                it->mat = nm;
                it->mat_cap = n + 1;
            }
            for (size_t i = 0; i < n; i++) {
                yep_rec_materialize(&it->store, i, &it->mat[i]);
            }
            size_t base = 0;
            if (!it->emitted_stream_start) {
                it->emitted_stream_start = 1;
                /* prepend the synthesized STREAM_START */
                memmove(&it->mat[1], it->mat, n * sizeof(*it->mat));
                memset(&it->mat[0], 0, sizeof(it->mat[0]));
                it->mat[0].type = YEPTRIS_EV_STREAM_START;
                base = 1;
            }
            it->next = it->store.n; /* consumed */
            if (count != NULL) {
                *count = n + base;
            }
            return it->mat;
        }
        if (it->done) {
            return NULL;
        }
        /* capture the next document: abort the run at its DOCUMENT_END;
         * the marker line stays unconsumed and doubles as the next
         * document's start on the resumed run */
        yep_rec_reset(&it->store);
        it->next = 0;
        yep_iter_sink s = {&it->store, 0};
        yep_engine* eng = yep_engine_create(yep_system_allocator());
        if (eng == NULL) {
            it->done = 1;
            it->status = YEPTRIS_ERROR_MEMORY;
            return NULL;
        }
        yep_sink sink = {iter_on_event, &s};
        int rc = yep_engine_run(eng, it->buf + it->cursor, it->len - it->cursor, &sink);
        size_t pos = yep_engine_pos(eng);
        const yep_error* err = yep_engine_error(eng);
        if (rc == -1 && err != NULL) {
            it->err_line = err->line;
            it->err_col = err->col;
        }
        yep_engine_destroy(eng);
        if (rc == -1) {
            it->done = 1;
            it->status = YEPTRIS_ERROR_PARSE;
            if (it->store.n == 0) {
                if (!it->emitted_stream_start) {
                    /* the engine emitted +STR before failing: match it */
                    it->emitted_stream_start = 1;
                    it->final_end.type = YEPTRIS_EV_STREAM_START;
                    if (count != NULL) {
                        *count = 1;
                    }
                    return &it->final_end;
                }
                return NULL;
            }
            continue; /* surface the partial slice, then stop */
        }
        it->cursor += pos;
        if (rc == 0 || s.doc_ends == 0) {
            it->done = 1; /* natural end or blank tail: stream is over */
            it->pending_stream_end = 1;
        }
    }
}

YEPTRIS_API YeptrisStatus yeptris_iterparse_status(const YeptrisIterparse it, uint32_t* line,
                                                   uint32_t* col) {
    if (it == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    if (line != NULL) {
        *line = it->err_line;
    }
    if (col != NULL) {
        *col = it->err_col;
    }
    return it->status;
}

YEPTRIS_API void yeptris_iterparse_free(YeptrisIterparse it) {
    if (it == NULL) {
        return;
    }
    yep_rec_free(&it->store);
    free(it->mat);
    free(it);
}
