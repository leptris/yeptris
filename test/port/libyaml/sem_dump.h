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

/* The append state: length and capacity are tracked, never re-derived.
 * The nightly's run 42517419521 (timeout-a9991d98, an 82KB flow-'?'
 * soup driving 13401 events) rode strlen+realloc PER EVENT here —
 * O(events^2) byte scans and copies that crossed the CI harness's
 * 5s per-unit alarm once the unit rode ~24 full-input dumps through
 * the arm chain. Tracking the length turns each append into one
 * memcpy; the capacity grows geometrically, so the whole dump is
 * O(events + output). The emitted bytes are identical. */
typedef struct {
    char* buf;
    size_t len;
    size_t cap;
} yd_sem_out;

/* Appends the semantic event line to a malloc'd buffer in ctx. */
static int yd_sem_event(void* ctx, const yep_event* ev) {
    yd_sem_out* out = (yd_sem_out*)ctx;
    yd_event y;
    memset(&y, 0, sizeof(y));
    /* the harness input cap is 8192, so a props buffer sized to it
     * never truncates: a tag past the old 512 was DROPPED here while
     * the libyaml side truncated its copy — a manufactured divergence
     * (the nightly's round 59, crash-dc4d99f4) */
    char abuf[2048], tbuf[8192];
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
    size_t need = out->len + n + 1;
    if (need > out->cap) {
        size_t nc = out->cap ? out->cap : 256;
        while (nc < need) {
            nc *= 2;
        }
        char* nb = realloc(out->buf, nc);
        if (nb == NULL) {
            return 1;
        }
        out->buf = nb;
        out->cap = nc;
    }
    memcpy(out->buf + out->len, line, n);
    out->len += n;
    out->buf[out->len] = '\0';
    return 0;
}

/* Semantic dump of one parse (malloc'd; NULL on a parse error with
 * *ok = 0). The empty stream dumps "". */
static char* yd_sem_dump(const char* in, size_t len, int* ok) {
    yd_sem_out out = {NULL, 0, 0};
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    yep_sink sink = {.on_event = yd_sem_event,
                     .ctx = &out,
                     .on_flow_build = NULL,
                     .on_flow_commit = NULL,
                     .on_flow_rollback = NULL,
                     .on_block_pair = NULL};
    int rc = yep_engine_run(eng, in, len, &sink);
    yep_engine_destroy(eng);
    *ok = (rc == 0);
    if (!*ok) {
        free(out.buf);
        return NULL;
    }
    return out.buf ? out.buf : strdup("");
}

#endif /* YD_SEM_DUMP_H */
