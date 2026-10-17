/* push.c — callback push over the engine sink (TODO.impl/12).
 *
 * The C-user convenience shape: one call per event, strings valid only
 * during the callback. Bindings prefer the recorder's bulk drain. */

#include <string.h>

#include "../../include/yeptris/events.h"
#include "../common/simd_text.h"
#include "../encoding/encoding.h"
#include "../memory/allocator.h"
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
    /* the encoding front-end every byte-taking entry owns (parse.c,
     * the value drains): BOM sniff + UTF-16/32 transcode -- the push
     * route fed raw bytes to the engine, and a lone UTF-16LE BOM
     * parsed as a garbage scalar where libyaml (and yeptris_parse)
     * yield an empty stream (the nightly's round 115) */
    yep_encoding enc = YEP_ENC_UNKNOWN;
    size_t bom = yep_bom_sniff((const unsigned char*)buf, len, &enc);
    const char* data = buf + bom;
    size_t data_len = len - bom;
    unsigned char* transcoded = NULL;
    if (enc == YEP_ENC_UTF16LE || enc == YEP_ENC_UTF16BE || enc == YEP_ENC_UTF32LE ||
        enc == YEP_ENC_UTF32BE) {
        const yep_allocator* sys = yep_system_allocator();
        size_t tlen = 0;
        size_t terr = 0;
        int rc = yep_transcode_to_utf8(sys, enc, (const unsigned char*)data, data_len, &transcoded,
                                       &tlen, &terr);
        if (rc != 0) {
            return YEPTRIS_ERROR_PARSE;
        }
        size_t pverr = 0;
        if (!yep_printable_validate(transcoded, tlen, &pverr)) {
            yep_free(sys, transcoded);
            return YEPTRIS_ERROR_PARSE;
        }
        data = (const char*)transcoded;
        data_len = tlen;
    }

    yep_engine* eng = yep_engine_create(yep_system_allocator());
    if (eng == NULL) {
        yep_free(yep_system_allocator(), transcoded);
        return YEPTRIS_ERROR_MEMORY;
    }
    yep_push_ctx p = {on_event, ctx};
    yep_sink sink = {.on_event = push_on_event,
                     .ctx = &p,
                     .on_flow_build = NULL,
                     .on_flow_commit = NULL,
                     .on_flow_rollback = NULL,
                     .on_block_pair = NULL,
                     .on_block_open = NULL,
                     .on_block_item = NULL};
    yep_text_stats pst;
    yep_text_active()->scan_stats(data, data_len, &pst);
    yep_engine_prepare(eng, &pst);
    int rc = yep_engine_run(eng, data, data_len, &sink);
    yep_engine_destroy(eng);
    yep_free(yep_system_allocator(), transcoded);
    if (rc == 0) {
        return YEPTRIS_OK;
    }
    /* -2: the callback aborted — reported as a parse stop, not a bug */
    return YEPTRIS_ERROR_PARSE;
}
