/* pull.c — StAX-style cursor (TODO.impl/12).
 *
 * Zero C->host dispatch: the run completes into the shared capture,
 * then events are handed out one (or a batch) at a time. Event strings
 * reference the puller's arena and are valid until the next call. */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/events.h"
#include "../parse/engine.h"
#include "capture.h"

struct yeptris_pull {
    yep_rec_store store;
    size_t next;
    YeptrisStatus status;
    uint32_t err_line;
    uint32_t err_col;
    YeptrisEvent current;
};

YEPTRIS_API YeptrisPullParser yeptris_pull_new(const char* buf, size_t len) {
    if (buf == NULL && len != 0) {
        return NULL;
    }
    struct yeptris_pull* p = calloc(1, sizeof(*p));
    if (p == NULL) {
        return NULL;
    }
    yep_rec_init(&p->store);
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    if (eng == NULL) {
        yeptris_pull_free(p);
        return NULL;
    }
    yep_sink sink = {yep_rec_on_event, &p->store};
    yep_engine_prepare(eng, buf, len);
    int rc = yep_engine_run(eng, buf, len, &sink);
    const yep_error* err = yep_engine_error(eng);
    if (err != NULL && rc != 0) {
        p->err_line = err->line;
        p->err_col = err->col;
    }
    yep_engine_destroy(eng);
    p->status = rc == 0 ? YEPTRIS_OK : YEPTRIS_ERROR_PARSE;
    return p;
}

YEPTRIS_API const YeptrisEvent* yeptris_pull_next(YeptrisPullParser pull) {
    if (pull == NULL || pull->next >= pull->store.n) {
        return NULL;
    }
    yep_rec_materialize(&pull->store, pull->next++, &pull->current);
    return &pull->current;
}

YEPTRIS_API size_t yeptris_pull_next_batch(YeptrisPullParser pull, YeptrisEvent* out, size_t max) {
    if (pull == NULL || out == NULL) {
        return 0;
    }
    size_t n = 0;
    while (n < max && pull->next < pull->store.n) {
        yep_rec_materialize(&pull->store, pull->next++, &out[n]);
        n++;
    }
    return n;
}

YEPTRIS_API YeptrisStatus yeptris_pull_status(const YeptrisPullParser pull, uint32_t* line,
                                              uint32_t* col) {
    if (pull == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    if (line != NULL) {
        *line = pull->err_line;
    }
    if (col != NULL) {
        *col = pull->err_col;
    }
    return pull->status;
}

YEPTRIS_API void yeptris_pull_free(YeptrisPullParser pull) {
    if (pull == NULL) {
        return;
    }
    yep_rec_free(&pull->store);
    free(pull);
}
