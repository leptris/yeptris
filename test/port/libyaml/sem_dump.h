/* sem_dump.h — the SEMANTIC event dump (TODO.impl/17B).
 *
 * SSOT for semantic equality between two serializations: values,
 * structure, anchors, and tags — never layout (styles, flow-ness,
 * document markers). Shared by the roundtrip gate and the emitter
 * differential so "semantically equal" means the same thing in both.
 */
#ifndef YD_SEM_DUMP_H
#define YD_SEM_DUMP_H

#include <stdlib.h>
#include <string.h>

#include "parse/engine.h"

#include "event_dump.h"

/* Appends the semantic event line to a malloc'd buffer in ctx. */
static int yd_sem_event(void* ctx, const yep_event* ev) {
    char** out = (char**)ctx;
    yd_event y;
    memset(&y, 0, sizeof(y));
    char abuf[256], tbuf[512];
    const char* a = NULL;
    const char* t = NULL;
    if (ev->anchor.len > 0 && ev->anchor.len < sizeof(abuf)) {
        memcpy(abuf, ev->anchor.p, ev->anchor.len);
        abuf[ev->anchor.len] = '\0';
        a = abuf;
    }
    if (ev->tag.len > 0 && ev->tag.len < sizeof(tbuf)) {
        memcpy(tbuf, ev->tag.p, ev->tag.len);
        tbuf[ev->tag.len] = '\0';
        t = tbuf;
    }
    switch (ev->type) {
    case YEP_EV_STREAM_START:
        y.type = YD_STREAM_START;
        break;
    case YEP_EV_STREAM_END:
        y.type = YD_STREAM_END;
        break;
    case YEP_EV_DOCUMENT_START:
    case YEP_EV_DOCUMENT_END:
        return 0; /* markers are layout, not semantics */
    case YEP_EV_MAP_START:
        y.type = YD_MAP_START; /* flow-ness is layout */
        break;
    case YEP_EV_MAP_END:
        y.type = YD_MAP_END;
        break;
    case YEP_EV_SEQ_START:
        y.type = YD_SEQ_START;
        break;
    case YEP_EV_SEQ_END:
        y.type = YD_SEQ_END;
        break;
    case YEP_EV_SCALAR:
        y.type = YD_SCALAR;
        y.value = ev->value.len > 0 ? (const char*)ev->value.p : "";
        y.value_len = ev->value.len;
        y.style = YD_PLAIN; /* style upgrades are roundtrip-legal */
        /* the non-specific tag "!" resolves by style — a quoted empty
         * with "!" and a quoted empty without are the same !!str */
        if (t != NULL && strcmp(t, "!") == 0) {
            t = NULL;
        }
        break;
    case YEP_EV_ALIAS:
        y.type = YD_ALIAS;
        if (ev->value.len > 0 && ev->value.len < sizeof(abuf)) {
            memcpy(abuf, ev->value.p, ev->value.len);
            abuf[ev->value.len] = '\0';
            a = abuf;
        }
        break;
    default:
        return 0;
    }
    y.anchor = a;
    y.tag = t;
    char line[4096];
    size_t n = yd_line(&y, line, sizeof(line));
    size_t ol = *out ? strlen(*out) : 0;
    char* nb = realloc(*out, ol + n + 1);
    if (nb == NULL) {
        return 1;
    }
    memcpy(nb + ol, line, n);
    nb[ol + n] = '\0';
    *out = nb;
    return 0;
}

/* Semantic dump of one parse (malloc'd; NULL on a parse error with
 * *ok = 0). The empty stream dumps "". */
static char* yd_sem_dump(const char* in, size_t len, int* ok) {
    char* out = NULL;
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    yep_sink sink = {yd_sem_event, &out};
    int rc = yep_engine_run(eng, in, len, &sink);
    yep_engine_destroy(eng);
    *ok = (rc == 0);
    if (!*ok) {
        free(out);
        return NULL;
    }
    return out ? out : strdup("");
}

#endif /* YD_SEM_DUMP_H */
