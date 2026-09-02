/* capture.c — the shared capture sink (TODO.impl/12). */

#include "capture.h"

#include <string.h>

#include "../../include/yeptris/events.h"

/* The record IS the public ABI shape (events.h pins it). */
_Static_assert(sizeof(YeptrisEventRecord) == 36, "record layout pinned");

void yep_rec_init(yep_rec_store* s) {
    s->recs = NULL;
    s->n = 0;
    s->cap = 0;
    s->arena = NULL;
    s->arena_len = 0;
    s->arena_cap = 0;
}

void yep_rec_reset(yep_rec_store* s) {
    s->n = 0;
    s->arena_len = 0;
}

void yep_rec_free(yep_rec_store* s) {
    free(s->recs);
    free(s->arena);
    yep_rec_init(s);
}

static uint32_t arena_put(yep_rec_store* s, const char* p, uint32_t len) {
    if (len == 0 || p == NULL) {
        return 0;
    }
    if (s->arena_len + len > s->arena_cap) {
        size_t cap = s->arena_cap ? s->arena_cap : 256;
        while (s->arena_len + len > cap) {
            cap *= 2;
        }
        char* na = realloc(s->arena, cap);
        if (na == NULL) {
            return 0; /* OOM: the string is dropped, not fatal */
        }
        s->arena = na;
        s->arena_cap = cap;
    }
    memcpy(s->arena + s->arena_len, p, len);
    uint32_t off = (uint32_t)s->arena_len;
    s->arena_len += len;
    return off;
}

int yep_rec_on_event(void* ctx, const yep_event* ev) {
    yep_rec_store* s = (yep_rec_store*)ctx;
    if (s->n == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 64;
        YeptrisEventRecord* nr = realloc(s->recs, cap * sizeof(*nr));
        if (nr == NULL) {
            return -1;
        }
        s->recs = nr;
        s->cap = cap;
    }
    YeptrisEventRecord* r = &s->recs[s->n];
    memset(r, 0, sizeof(*r));
    switch (ev->type) {
    case YEP_EV_STREAM_START:
        r->type = YEPTRIS_EV_STREAM_START;
        break;
    case YEP_EV_STREAM_END:
        r->type = YEPTRIS_EV_STREAM_END;
        break;
    case YEP_EV_DOCUMENT_START:
        r->type = YEPTRIS_EV_DOCUMENT_START;
        if (ev->style == 1) {
            r->flags |= YEPTRIS_EF_EXPLICIT;
        }
        break;
    case YEP_EV_DOCUMENT_END:
        r->type = YEPTRIS_EV_DOCUMENT_END;
        if (ev->style == 1) {
            r->flags |= YEPTRIS_EF_EXPLICIT;
        }
        break;
    case YEP_EV_SEQ_START:
        r->type = YEPTRIS_EV_SEQUENCE_START;
        break;
    case YEP_EV_SEQ_END:
        r->type = YEPTRIS_EV_SEQUENCE_END;
        break;
    case YEP_EV_MAP_START:
        r->type = YEPTRIS_EV_MAPPING_START;
        break;
    case YEP_EV_MAP_END:
        r->type = YEPTRIS_EV_MAPPING_END;
        break;
    case YEP_EV_SCALAR:
        r->type = YEPTRIS_EV_SCALAR;
        r->style = ev->style;
        r->tag_id = ev->tag_id; /* the resolver's verdict, once */
        if (ev->implicit) {
            r->flags |= YEPTRIS_EF_IMPLICIT;
        }
        break;
    case YEP_EV_ALIAS:
        r->type = YEPTRIS_EV_ALIAS;
        break;
    default:
        return 0;
    }
    if (ev->flow) {
        r->flags |= YEPTRIS_EF_FLOW;
    }
    r->line = ev->line;
    r->col = ev->col;
    r->value_off = arena_put(s, (const char*)ev->value.p, (uint32_t)ev->value.len);
    r->value_len = (uint32_t)ev->value.len;
    r->anchor_off = arena_put(s, (const char*)ev->anchor.p, (uint32_t)ev->anchor.len);
    r->anchor_len = (uint32_t)ev->anchor.len;
    r->tag_off = arena_put(s, (const char*)ev->tag.p, (uint32_t)ev->tag.len);
    r->tag_len = (uint32_t)ev->tag.len;
    s->n++;
    return 0;
}

void yep_rec_materialize(const yep_rec_store* s, size_t i, void* out) {
    const YeptrisEventRecord* r = &s->recs[i];
    YeptrisEvent* e = (YeptrisEvent*)out;
    memset(e, 0, sizeof(*e));
    e->type = (YeptrisEventType)r->type;
    e->style = r->style;
    e->flow = (r->flags & YEPTRIS_EF_FLOW) != 0;
    e->explicit_marker = (r->flags & YEPTRIS_EF_EXPLICIT) != 0;
    e->implicit = (r->flags & YEPTRIS_EF_IMPLICIT) != 0;
    e->line = r->line;
    e->col = r->col;
    if (r->value_len > 0) {
        e->value = s->arena + r->value_off;
        e->value_len = r->value_len;
    }
    if (r->anchor_len > 0) {
        e->anchor = s->arena + r->anchor_off;
        e->anchor_len = r->anchor_len;
    }
    if (r->tag_len > 0) {
        e->tag = s->arena + r->tag_off;
        e->tag_len = r->tag_len;
    }
}
