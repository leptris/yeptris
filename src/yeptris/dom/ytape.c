/* ytape.c — the packed YAML record tape (#378, TODO.max-perf/12).
 *
 * Recorder: a yep_sink over the SEVEN committed-op callbacks plus the
 * general event path. The engine stays in its fused arms (no event
 * construction on classified shapes — the event-recorder prototype's
 * measured floor); the recorder's writes are 8-byte words instead of
 * the eager builders' 48-byte node inits, arena copies, resolver calls
 * and placement.
 *
 * Replay: decodes words into the SAME builder functions
 * (dom_on_block_pair/open/item, dom_on_scalar, dom_on_flow_build,
 * yep_dom_on_event), so typing, anchoring and placement semantics are
 * the eager path's by construction. The differential gate
 * (test/unit/test_ytape.cpp) pins tree identity across the corpora.
 */

#include <string.h>

#include "dom/dom.h"
#include "memory/allocator.h"
#include "scan/json.h"
#include "scan/scan.h"
#include "ytape.h"

/* ---- word packing ---- */

#define YTP_KIND_OF(w) ((uint8_t)((w) >> 56))

static int yt_put(yep_ytape* t, uint64_t w);

static uint64_t yt_span_in(uint32_t off, uint32_t len, uint8_t kind) {
    return ((uint64_t)kind << 56) | ((uint64_t)(len & 0xFFFFFFu) << 32) | (uint64_t)off;
}

static uint64_t yt_props(uint8_t kind, uint32_t anchor_id, uint32_t flags) {
    return ((uint64_t)kind << 56) | ((uint64_t)(anchor_id & 0xFFFFFFu) << 32) |
           (uint64_t)(flags & 0xFFFFFFFFu);
}

/* ---- the recorder ---- */

int ytap_init(yep_ytape* t, const char* input, size_t len, int max_depth) {
    memset(t, 0, sizeof(*t));
    t->input = input;
    t->input_len = len;
    t->max_depth = max_depth;
    size_t cap = len / 2 + 64;
    if (cap > 0xFFFFFFFFu) {
        cap = 0xFFFFFFFFu;
    }
    t->w = (uint64_t*)yep_alloc(yep_system_allocator(), cap * sizeof(uint64_t));
    if (t->w == NULL) {
        return -1;
    }
    t->cap = (uint32_t)cap;
    return 0;
}

void ytap_free(yep_ytape* t) {
    if (t->w != NULL) {
        yep_free(yep_system_allocator(), t->w);
        t->w = NULL;
    }
}

/* Growth bounded by the input (the bounded-parse law): a 4N-word
 * ceiling, never a runaway realloc chain. The allocator has no
 * realloc — one copy per growth, at most a few per document. */
static int yt_put(yep_ytape* t, uint64_t w) {
    if (t->count >= t->cap) {
        if (t->oom) {
            return -1;
        }
        size_t ncap = (size_t)t->cap * 2;
        size_t ceiling = t->input_len * 4 + 256;
        if (ncap > ceiling) {
            ncap = ceiling;
        }
        if (ncap <= (size_t)t->cap) {
            t->oom = 1;
            return -1;
        }
        uint64_t* nw = (uint64_t*)yep_alloc(yep_system_allocator(), ncap * sizeof(uint64_t));
        if (nw == NULL) {
            t->oom = 1;
            return -1;
        }
        memcpy(nw, t->w, (size_t)t->count * sizeof(uint64_t));
        yep_free(yep_system_allocator(), t->w);
        t->w = nw;
        t->cap = (uint32_t)ncap;
    }
    t->w[t->count++] = w;
    return 0;
}

/* The pool-form span: two words, 32-bit length, raw pointer (stable:
 * pool blocks never move until the document frees them). */
static int yt_span_pool(yep_ytape* t, const void* p, size_t len) {
    if (yt_put(t, ((uint64_t)YTP_SPAN_POOL << 56) | (uint64_t)(uint32_t)len) != 0) {
        return -1;
    }
    return yt_put(t, (uint64_t)(uintptr_t)p);
}

/* One span word when the view is input-relative and fits 24 bits; the
 * generic pair otherwise. */
static int yt_span(yep_ytape* t, const yep_view* v) {
    if (v->p >= t->input && v->p <= t->input + t->input_len && v->len < 0x1000000u) {
        return yt_put(t, yt_span_in((uint32_t)(v->p - t->input), (uint32_t)v->len, YTP_SPAN_IN));
    }
    return yt_span_pool(t, v->p, v->len);
}

/* Span-mode bits shared by the props words: 0 absent, 1 input, 2 pool
 * (mode is re-derived by pointer range — the recorder never trusts
 * borrowedness for tag/anchor views). */
#define YTM_ABSENT 0
#define YTM_IN 1
#define YTM_POOL 2

static int yt_mode(const yep_ytape* t, const yep_view* v) {
    if (v->p == NULL || v->len == 0) {
        return YTM_ABSENT;
    }
    if (v->p >= t->input && v->p <= t->input + t->input_len) {
        return YTM_IN;
    }
    return YTM_POOL;
}

static int yt_span_by_mode(yep_ytape* t, const yep_view* v) {
    if (v->p == NULL || v->len == 0) {
        return 0;
    }
    return yt_span(t, v);
}

/* The general event path. STREAM/DOCUMENT frames compact to one word
 * (the DOM reads nothing else off them); SEQ/MAP/SCALAR/ALIAS pack
 * props + value + optional tag/anchor spans. */
static int yt_on_event(void* ctx, const yep_event* ev) {
    yep_ytape* t = (yep_ytape*)ctx;
    switch (ev->type) {
    case YEP_EV_STREAM_START:
    case YEP_EV_STREAM_END:
    case YEP_EV_DOCUMENT_START:
    case YEP_EV_DOCUMENT_END:
        if (ev->type == YEP_EV_DOCUMENT_START) {
            t->docs++;
        }
        return yt_put(t, ((uint64_t)YTP_EV_COMPACT << 56) | (uint64_t)ev->type);
    case YEP_EV_SEQ_START:
    case YEP_EV_MAP_START:
        if (t->depth >= YEP_DOM_MAX_DEPTH) {
            return -1; /* the DOM answers -1 here; the engine's error
                        * path stays identical */
        }
        t->depth++;
        break;
    case YEP_EV_SEQ_END:
    case YEP_EV_MAP_END:
        t->depth--;
        break;
    default:
        break;
    }
    uint32_t anchor_hi = 0;
    uint32_t aid = ev->anchor_id;
    if (aid > 0xFFFFFFu) {
        anchor_hi = 1;
        aid = 0xFFFFFFu;
    }
    uint32_t tag_mode = (uint32_t)yt_mode(t, &ev->tag);
    uint32_t anchor_mode = (uint32_t)yt_mode(t, &ev->anchor);
    uint32_t flags = (uint32_t)ev->type | ((uint32_t)ev->style << 4) |
                     ((uint32_t)ev->implicit << 7) | ((uint32_t)ev->flow << 8) |
                     ((uint32_t)ev->multiline << 9) | ((uint32_t)(ev->borrowed != 0) << 10) |
                     (tag_mode << 11) | (anchor_mode << 13) | (anchor_hi << 15) |
                     ((uint32_t)ev->tag_id << 16);
    if (yt_put(t, yt_props(YTP_EVENT, aid, flags)) != 0) {
        return -1;
    }
    if (yt_span(t, &ev->value) != 0) {
        return -1;
    }
    if (tag_mode != YTM_ABSENT && yt_span_by_mode(t, &ev->tag) != 0) {
        return -1;
    }
    if (anchor_mode != YTM_ABSENT && yt_span_by_mode(t, &ev->anchor) != 0) {
        return -1;
    }
    if (anchor_hi && yt_put(t, (uint64_t)ev->anchor_id) != 0) {
        return -1;
    }
    return 0;
}

static int yt_on_scalar(void* ctx, const yep_view* value, const yep_view* tag,
                        const yep_view* anchor, uint32_t anchor_id, uint8_t tag_id, uint8_t style,
                        uint8_t implicit, uint8_t flow, int borrowed) {
    yep_ytape* t = (yep_ytape*)ctx;
    uint32_t anchor_hi = 0;
    uint32_t aid = anchor_id;
    if (aid > 0xFFFFFFu) {
        anchor_hi = 1;
        aid = 0xFFFFFFu;
    }
    uint32_t tag_mode = (uint32_t)yt_mode(t, tag);
    uint32_t anchor_mode = (uint32_t)yt_mode(t, anchor);
    uint32_t flags = (uint32_t)tag_id | ((uint32_t)style << 8) | ((uint32_t)implicit << 11) |
                     ((uint32_t)flow << 12) | ((uint32_t)(borrowed != 0) << 13) | (tag_mode << 14) |
                     (anchor_mode << 16) | (anchor_hi << 18);
    if (yt_put(t, yt_props(YTP_SCALAR, aid, flags)) != 0) {
        return -1;
    }
    if (yt_span(t, value) != 0) {
        return -1;
    }
    if (tag_mode != YTM_ABSENT && yt_span_by_mode(t, tag) != 0) {
        return -1;
    }
    if (anchor_mode != YTM_ABSENT && yt_span_by_mode(t, anchor) != 0) {
        return -1;
    }
    if (anchor_hi && yt_put(t, (uint64_t)anchor_id) != 0) {
        return -1;
    }
    return 0;
}

static int yt_on_block_pair(void* ctx, const yep_view* key, const yep_block_value* v, uint32_t line,
                            uint16_t key_col, uint16_t val_col) {
    (void)line;
    (void)key_col;
    (void)val_col; /* yep_dnode carries no positions (64-2c) */
    yep_ytape* t = (yep_ytape*)ctx;
    uint32_t anchor_hi = 0;
    uint32_t aid = v->anchor_id;
    if (aid > 0xFFFFFFu) {
        anchor_hi = 1;
        aid = 0xFFFFFFu;
    }
    uint32_t flags = ((uint32_t)v->cls & 7u) | ((uint32_t)(v->borrowed != 0) << 3) |
                     ((uint32_t)yt_mode(t, &v->anchor) << 4) | (anchor_hi << 6);
    if (yt_put(t, yt_props(YTP_PAIR, aid, flags)) != 0) {
        return -1;
    }
    if (yt_span(t, key) != 0) {
        return -1;
    }
    /* ALIAS names record as spans either way (the eager builder
     * stamps borrowed=1 for them); plain values borrow by the flag. */
    if (v->borrowed || v->cls == YEP_LVAL_ALIAS) {
        if (yt_span(t, &v->value) != 0) {
            return -1;
        }
    } else if (yt_span_pool(t, v->value.p, v->value.len) != 0) {
        return -1;
    }
    if (((flags >> 4) & 3u) != YTM_ABSENT && yt_span_by_mode(t, &v->anchor) != 0) {
        return -1;
    }
    if (anchor_hi && yt_put(t, (uint64_t)v->anchor_id) != 0) {
        return -1;
    }
    return 1; /* handled: the engine skips the two events */
}

static int yt_on_block_open(void* ctx, const yep_view* key, uint32_t line, uint16_t key_col) {
    (void)line;
    (void)key_col;
    yep_ytape* t = (yep_ytape*)ctx;
    if (t->depth >= YEP_DOM_MAX_DEPTH) {
        return 0; /* the engine's event path owns the depth error */
    }
    if (key->len < 0x1000000u && key->p >= t->input && key->p <= t->input + t->input_len) {
        if (yt_put(t, yt_span_in((uint32_t)(key->p - t->input), (uint32_t)key->len, YTP_OPEN)) !=
            0) {
            return -1;
        }
    } else {
        /* a pool or oversized block key: the scalar form's open-key
         * bit carries it (defensive — the engine classifies keys from
         * the line; replay routes back to on_block_open) */
        if (yt_put(t, yt_props(YTP_SCALAR, 0, 1u << 21)) != 0) {
            return -1;
        }
        if (yt_span(t, key) != 0) {
            return -1;
        }
    }
    t->depth++;
    return 1;
}

static int yt_on_block_item(void* ctx, const yep_block_value* v) {
    yep_ytape* t = (yep_ytape*)ctx;
    if (v->borrowed && v->value.len < 0x1000000u && v->value.p >= t->input &&
        v->value.p <= t->input + t->input_len) {
        if (yt_put(t, yt_span_in((uint32_t)(v->value.p - t->input), (uint32_t)v->value.len,
                                 YTP_ITEM)) != 0) {
            return -1;
        }
        return 1;
    }
    /* pool content or oversized: the scalar form with the item bit */
    if (yt_put(t, yt_props(YTP_SCALAR, 0, 1u << 20)) != 0) {
        return -1;
    }
    if (yt_span_pool(t, v->value.p, v->value.len) != 0) {
        return -1;
    }
    return 1;
}

static int yt_on_flow_build(void* ctx, const char* p, size_t open, size_t len, uint32_t line,
                            size_t line_start, yep_view anchor, yep_view tag, uint32_t anchor_id,
                            int max_depth, size_t* close) {
    (void)line;
    (void)line_start; /* the flow builder reads neither */
    yep_ytape* t = (yep_ytape*)ctx;
    if (t->depth >= YEP_DOM_MAX_DEPTH) {
        return 0; /* the event path owns the error shape */
    }
    /* classification = the DOM's own walk with the builds stripped:
     * the same reject/long-key verdicts, then ONE record for the
     * whole staged span (replay runs the full build through
     * dom_on_flow_build). */
    yep_json_walk w;
    yep_json_tok tok;
    yep_json_walk_init(&w, p, len, open, max_depth);
    w.strict = t->flow_strict;
    for (;;) {
        yep_jw_status st = yep_json_walk_next(&w, &tok);
        if (st == YEP_JW_REJECT) {
            return 0; /* not JSON-class: the general kernel */
        }
        if (st == YEP_JW_DONE) {
            break;
        }
    }
    if (w.long_key) {
        return 2; /* pass 2 owns the simple-key error, as the DOM's */
    }
    t->flow_mark = t->count;
    uint32_t anchor_hi = 0;
    uint32_t aid = anchor_id;
    if (aid > 0xFFFFFFu) {
        anchor_hi = 1;
        aid = 0xFFFFFFu;
    }
    uint32_t tag_mode = (uint32_t)yt_mode(t, &tag);
    uint32_t anchor_mode = (uint32_t)yt_mode(t, &anchor);
    uint32_t flags = tag_mode | (anchor_mode << 2) | (anchor_hi << 4);
    if (yt_put(t, yt_props(YTP_FLOW, aid, flags)) != 0) {
        return -1;
    }
    if (yt_put(t, (uint64_t)open | ((uint64_t)len << 32)) != 0) {
        return -1;
    }
    if (tag_mode != YTM_ABSENT && yt_span_by_mode(t, &tag) != 0) {
        return -1;
    }
    if (anchor_mode != YTM_ABSENT && yt_span_by_mode(t, &anchor) != 0) {
        return -1;
    }
    if (anchor_hi && yt_put(t, (uint64_t)anchor_id) != 0) {
        return -1;
    }
    *close = w.close;
    return 1;
}

static int yt_on_flow_commit(void* ctx) {
    (void)ctx; /* the records are already written */
    return 1;
}

static void yt_on_flow_rollback(void* ctx) {
    yep_ytape* t = (yep_ytape*)ctx;
    t->count = t->flow_mark; /* the DOM's stage reset, in words */
}

void ytap_sink(yep_ytape* t, yep_sink* sink) {
    memset(sink, 0, sizeof(*sink));
    sink->on_event = yt_on_event;
    sink->ctx = t;
    sink->on_flow_build = yt_on_flow_build;
    sink->on_flow_commit = yt_on_flow_commit;
    sink->on_flow_rollback = yt_on_flow_rollback;
    sink->on_block_pair = yt_on_block_pair;
    sink->on_scalar = yt_on_scalar;
    sink->on_block_open = yt_on_block_open;
    sink->on_block_item = yt_on_block_item;
}

/* ---- replay ---- */

static int yt_read_span(const yep_ytape* t, const uint64_t* w, uint32_t* i, uint32_t count,
                        yep_view* v) {
    if (*i >= count) {
        return -1;
    }
    uint64_t word = w[(*i)++];
    uint8_t k = YTP_KIND_OF(word);
    if (k == YTP_SPAN_IN) {
        v->p = t->input + (uint32_t)word;
        v->len = (uint32_t)(word >> 32) & 0xFFFFFFu;
        return 0;
    }
    if (k == YTP_SPAN_POOL) {
        if (*i >= count) {
            return -1;
        }
        v->p = (const char*)(uintptr_t)w[(*i)++];
        v->len = (uint32_t)word;
        return 0;
    }
    return -1; /* a props word in a span slot: corruption */
}

int dom_from_ytape(yep_dom* d, const yep_ytape* t) {
    if (d == NULL || t == NULL || t->w == NULL) {
        return -1;
    }
    d->input_base = t->input;
    d->input_len = t->input_len;
    yep_dom_prepare_len(d, t->input_len);
    const uint64_t* w = t->w;
    uint32_t count = t->count;
    uint32_t i = 0;
    while (i < count) {
        uint8_t kind = YTP_KIND_OF(w[i]);
        switch (kind) {
        case YTP_EV_COMPACT: {
            yep_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = (yep_event_type)(uint8_t)w[i];
            i++;
            if (yep_dom_on_event(d, &ev) != 0) {
                return -1;
            }
            break;
        }
        case YTP_EVENT: {
            uint32_t flags = (uint32_t)(w[i] & 0xFFFFFFFFu);
            uint32_t aid = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            yep_event ev;
            memset(&ev, 0, sizeof(ev));
            ev.type = (yep_event_type)(flags & 0xFu);
            ev.style = (uint8_t)((flags >> 4) & 7u);
            ev.implicit = (uint8_t)((flags >> 7) & 1u);
            ev.flow = (uint8_t)((flags >> 8) & 1u);
            ev.multiline = (uint8_t)((flags >> 9) & 1u);
            ev.borrowed = (int)((flags >> 10) & 1u);
            uint32_t tag_mode = (flags >> 11) & 3u;
            uint32_t anchor_mode = (flags >> 13) & 3u;
            uint32_t anchor_hi = (flags >> 15) & 1u;
            ev.tag_id = (uint8_t)((flags >> 16) & 0xFFu);
            if (yt_read_span(t, w, &i, count, &ev.value) != 0) {
                return -1;
            }
            if (tag_mode != YTM_ABSENT) {
                if (yt_read_span(t, w, &i, count, &ev.tag) != 0) {
                    return -1;
                }
            }
            if (anchor_mode != YTM_ABSENT) {
                if (yt_read_span(t, w, &i, count, &ev.anchor) != 0) {
                    return -1;
                }
            }
            if (anchor_hi) {
                if (i >= count) {
                    return -1;
                }
                aid = (uint32_t)w[i++];
            }
            ev.anchor_id = aid;
            if (yep_dom_on_event(d, &ev) != 0) {
                return -1;
            }
            break;
        }
        case YTP_SCALAR: {
            uint32_t flags = (uint32_t)(w[i] & 0xFFFFFFFFu);
            uint32_t aid = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            int is_item = (int)((flags >> 20) & 1u);
            int is_open_key = (int)((flags >> 21) & 1u);
            if (is_item || is_open_key) {
                yep_view v;
                if (yt_read_span(t, w, &i, count, &v) != 0) {
                    return -1;
                }
                yep_block_value bv;
                memset(&bv, 0, sizeof(bv));
                bv.cls = YEP_LVAL_PLAIN;
                bv.borrowed = (v.p >= t->input && v.p <= t->input + t->input_len) ? 1 : 0;
                bv.value = v;
                int rc = is_item ? dom_on_block_item(d, &bv) : dom_on_block_open(d, &v, 0, 0);
                if (rc < 0) {
                    return -1;
                }
                break;
            }
            uint8_t tag_id = (uint8_t)(flags & 0xFFu);
            uint8_t style = (uint8_t)((flags >> 8) & 7u);
            uint8_t implicit = (uint8_t)((flags >> 11) & 1u);
            uint8_t flow = (uint8_t)((flags >> 12) & 1u);
            int borrowed = (int)((flags >> 13) & 1u);
            uint32_t tag_mode = (flags >> 14) & 3u;
            uint32_t anchor_mode = (flags >> 16) & 3u;
            uint32_t anchor_hi = (flags >> 18) & 1u;
            yep_view value, tag = {0}, anchor = {0};
            if (yt_read_span(t, w, &i, count, &value) != 0) {
                return -1;
            }
            if (tag_mode != YTM_ABSENT && yt_read_span(t, w, &i, count, &tag) != 0) {
                return -1;
            }
            if (anchor_mode != YTM_ABSENT && yt_read_span(t, w, &i, count, &anchor) != 0) {
                return -1;
            }
            if (anchor_hi) {
                if (i >= count) {
                    return -1;
                }
                aid = (uint32_t)w[i++];
            }
            if (dom_on_scalar(d, &value, tag_mode ? &tag : NULL, anchor_mode ? &anchor : NULL, aid,
                              tag_id, style, implicit, flow, borrowed) < 0) {
                return -1;
            }
            break;
        }
        case YTP_ITEM: {
            yep_view v;
            v.p = t->input + (uint32_t)w[i];
            v.len = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            yep_block_value bv;
            memset(&bv, 0, sizeof(bv));
            bv.cls = YEP_LVAL_PLAIN;
            bv.borrowed = 1;
            bv.value = v;
            if (dom_on_block_item(d, &bv) < 0) {
                return -1;
            }
            break;
        }
        case YTP_PAIR: {
            uint32_t flags = (uint32_t)(w[i] & 0xFFFFFFFFu);
            uint32_t aid = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            yep_block_value bv;
            memset(&bv, 0, sizeof(bv));
            bv.cls = (uint8_t)(flags & 7u);
            bv.borrowed = (int)((flags >> 3) & 1u);
            uint32_t anchor_mode = (flags >> 4) & 3u;
            uint32_t anchor_hi = (flags >> 6) & 1u;
            yep_view key;
            if (yt_read_span(t, w, &i, count, &key) != 0) {
                return -1;
            }
            if (yt_read_span(t, w, &i, count, &bv.value) != 0) {
                return -1;
            }
            if (bv.cls == YEP_LVAL_ALIAS) {
                bv.borrowed = 1; /* the eager builder stamps alias names */
            }
            if (anchor_mode != YTM_ABSENT && yt_read_span(t, w, &i, count, &bv.anchor) != 0) {
                return -1;
            }
            if (anchor_hi) {
                if (i >= count) {
                    return -1;
                }
                aid = (uint32_t)w[i++];
            }
            bv.anchor_id = aid;
            if (dom_on_block_pair(d, &key, &bv, 0, 0, 0) < 0) {
                return -1;
            }
            break;
        }
        case YTP_OPEN: {
            yep_view key;
            key.p = t->input + (uint32_t)w[i];
            key.len = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            if (dom_on_block_open(d, &key, 0, 0) < 0) {
                return -1;
            }
            break;
        }
        case YTP_FLOW: {
            uint32_t flags = (uint32_t)(w[i] & 0xFFFFFFFFu);
            uint32_t aid = (uint32_t)(w[i] >> 32) & 0xFFFFFFu;
            i++;
            uint32_t tag_mode = flags & 3u;
            uint32_t anchor_mode = (flags >> 2) & 3u;
            uint32_t anchor_hi = (flags >> 4) & 1u;
            if (i >= count) {
                return -1;
            }
            size_t open = (size_t)(uint32_t)w[i];
            size_t len = (size_t)(uint32_t)(w[i] >> 32);
            i++;
            yep_view tag = {0}, anchor = {0};
            if (tag_mode != YTM_ABSENT && yt_read_span(t, w, &i, count, &tag) != 0) {
                return -1;
            }
            if (anchor_mode != YTM_ABSENT && yt_read_span(t, w, &i, count, &anchor) != 0) {
                return -1;
            }
            if (anchor_hi) {
                if (i >= count) {
                    return -1;
                }
                aid = (uint32_t)w[i++];
            }
            size_t close = 0;
            if (dom_on_flow_build(d, t->input, open, len, 0, 0, anchor, tag, aid, t->max_depth,
                                  &close) != 1) {
                return -1;
            }
            if (dom_on_flow_commit(d) < 0) {
                return -1;
            }
            break;
        }
        default:
            return -1; /* a span word where an op belongs: corruption */
        }
    }
    return 0;
}
