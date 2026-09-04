/* push.c — callback push over the engine sink (TODO.impl/12).
 *
 * The C-user convenience shape: one call per event, strings valid only
 * during the callback. Bindings prefer the recorder's bulk drain. */

#include <string.h>

#include "../../include/yeptris/events.h"
#include "../common/simd_text.h"
#include "../parse/engine.h"
#include "yeptris/error.h"
#include "yeptris/parse.h"

typedef struct {
    yeptris_event_fn fn;
    void* ctx;
} yep_push_ctx;

static int push_on_event(void* ctx, const yep_event* ev) {
    yep_push_ctx* p = (yep_push_ctx*)ctx;
    YeptrisEvent e;
    memset(&e, 0, sizeof(e));
    switch (ev->type) {
    case YEP_EV_STREAM_START:
        e.type = YEPTRIS_EV_STREAM_START;
        break;
    case YEP_EV_STREAM_END:
        e.type = YEPTRIS_EV_STREAM_END;
        break;
    case YEP_EV_DOCUMENT_START:
        e.type = YEPTRIS_EV_DOCUMENT_START;
        e.explicit_marker = (ev->style == 1);
        break;
    case YEP_EV_DOCUMENT_END:
        e.type = YEPTRIS_EV_DOCUMENT_END;
        e.explicit_marker = (ev->style == 1);
        break;
    case YEP_EV_SEQ_START:
        e.type = YEPTRIS_EV_SEQUENCE_START;
        break;
    case YEP_EV_SEQ_END:
        e.type = YEPTRIS_EV_SEQUENCE_END;
        break;
    case YEP_EV_MAP_START:
        e.type = YEPTRIS_EV_MAPPING_START;
        break;
    case YEP_EV_MAP_END:
        e.type = YEPTRIS_EV_MAPPING_END;
        break;
    case YEP_EV_SCALAR:
        e.type = YEPTRIS_EV_SCALAR;
        e.style = ev->style;
        e.implicit = ev->implicit;
        break;
    case YEP_EV_ALIAS:
        e.type = YEPTRIS_EV_ALIAS;
        break;
    default:
        return 0;
    }
    e.flow = ev->flow;
    e.line = ev->line;
    e.col = ev->col;
    e.value = ev->value.len > 0 ? (const char*)ev->value.p : NULL;
    e.value_len = ev->value.len;
    e.anchor = ev->anchor.len > 0 ? (const char*)ev->anchor.p : NULL;
    e.anchor_len = ev->anchor.len;
    e.tag = ev->tag.len > 0 ? (const char*)ev->tag.p : NULL;
    e.tag_len = ev->tag.len;
    return p->fn(p->ctx, &e);
}

YEPTRIS_API YeptrisStatus yeptris_push_parse(const char* buf, size_t len, yeptris_event_fn on_event,
                                             void* ctx) {
    if (buf == NULL && len != 0) {
        return YEPTRIS_ERROR_ARG;
    }
    if (on_event == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    if (eng == NULL) {
        return YEPTRIS_ERROR_MEMORY;
    }
    yep_push_ctx p = {on_event, ctx};
    yep_sink sink = {push_on_event, &p};
    yep_text_stats pst;
    yep_text_active()->scan_stats(buf, len, &pst);
    yep_engine_prepare(eng, &pst);
    int rc = yep_engine_run(eng, buf, len, &sink);
    yep_engine_destroy(eng);
    if (rc == 0) {
        return YEPTRIS_OK;
    }
    /* -2: the callback aborted — reported as a parse stop, not a bug */
    return YEPTRIS_ERROR_PARSE;
}
