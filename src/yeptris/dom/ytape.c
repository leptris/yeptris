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

#include <stdio.h>
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
    t->max_depth = max_depth; /* replay re-walks at the engine's limit */
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

/* ---- the fused block runner (TODO.restructure/79, session 3) -------
 *
 * ONE SWAR sweep per line + inline classification + an indent stack,
 * emitting the same record stream the engine's sink would — without
 * the engine. Byte-exact against the engine over the block-family
 * corpora (wide 600k / block 1.0M / deep 17k words, differential
 * pinned by the FusedTape test). Any line the loop does not own bails
 * to 1 and the caller re-runs the engine route (restart fallback).
 *
 * Bails (the engine route owns them): quoted/tagged/flow/folded/block
 * scalar values, compact map items, anchored CONTAINER keys (the
 * first pair inside one rides a two-SCALAR form with content-typed
 * tag ids — capture before owning), doc markers/directives, empty
 * "key:" at EOF or mid-doc, undefined aliases, root sequences. */

#define YT_FUSED_MAX_FRAMES 256
#define YT_FUSED_ANCHORS 4096

typedef struct {
    uint32_t indent;      /* COLUMN */
    uint8_t kind;         /* 0 map, 1 seq */
    uint8_t pending;      /* a "key:" awaiting its child */
    uint8_t started;      /* the start event went out */
    uint8_t emitted;      /* a pair/item/child landed here */
    uint32_t content_col; /* the column this frame's pairs live at */
    uint32_t key_off, key_len;
    uint32_t aid; /* anchor ordinal on the pending container (bails) */
} yt_frame;

typedef struct {
    yep_ytape* t;
    const char* p;
    size_t len;
    uint32_t anchor_seq;
    int parent_emitted;
    /* anchor name -> ordinal (the alias records carry the target's
     * id; a miss bails: the engine owns undefined aliases). Fixed
     * size (MSVC has no flexible array members); the struct is
     * heap-allocated per run. */
    struct {
        uint32_t off, len, aid;
    } anchors[YT_FUSED_ANCHORS];
} yt_fused;

/* a fixed 4096-slot linear-probe table: the corpus bound is one entry


static uint32_t yt_anchor_hash(const char* p, uint32_t off, uint32_t len) {
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < len; i++) {
        h = (h ^ (unsigned char)p[off + i]) * 16777619u;
    }
    return h;
}

static void yt_anchor_put(yt_fused* F, uint32_t off, uint32_t len, uint32_t aid) {
    uint32_t m = YT_FUSED_ANCHORS - 1;
    uint32_t i = yt_anchor_hash(F->p, off, len) & m;
    for (uint32_t n = 0; n < 64; n++) {
        if (F->anchors[i].len == 0) {
            F->anchors[i].off = off;
            F->anchors[i].len = len;
            F->anchors[i].aid = aid;
            return;
        }
        if (F->anchors[i].len == len && memcmp(F->p + F->anchors[i].off, F->p + off, len) == 0) {
            F->anchors[i].aid = aid; /* redefinition: latest wins */
return;
}
i = (i + 1) & m;
}
/* chain too long: leave unfindable — aliases to it will bail */
}

static int yt_anchor_get(const yt_fused* F, uint32_t off, uint32_t len, uint32_t* aid) {
    uint32_t m = YT_FUSED_ANCHORS - 1;
    uint32_t i = yt_anchor_hash(F->p, off, len) & m;
    for (uint32_t n = 0; n < 64; n++) {
        if (F->anchors[i].len == 0) {
            return 0;
        }
        if (F->anchors[i].len == len && memcmp(F->p + F->anchors[i].off, F->p + off, len) == 0) {
            *aid = F->anchors[i].aid;
            return 1;
        }
        i = (i + 1) & m;
    }
    return 0;
}

static void yt_f_empty_span(yt_fused* F) {
    /* the dead value-span pair the container events always carry */
    (void)yt_put(F->t, ((uint64_t)YTP_SPAN_POOL << 56) | 0u);
    (void)yt_put(F->t, 0);
}

static void yt_f_start(yt_fused* F, yt_frame* fr) {
    if (fr->started) {
        return;
    }
    fr->started = 1;
    (void)yt_put(F->t, yt_props(YTP_EVENT, 0, fr->kind == 1 ? 5u : 7u));
    yt_f_empty_span(F);
}

static void yt_f_close(yt_fused* F, uint8_t kind) {
    (void)yt_put(F->t, yt_props(YTP_EVENT, 0, kind == 1 ? 6u : 8u));
    yt_f_empty_span(F);
    F->t->depth--;
}

/* resolve a pending "key:": OPEN for a fresh parent, else the
 * key-SCALAR form; the start defers to the first content inside */
static void yt_f_open(yt_fused* F, yt_frame* fr, uint8_t seq) {
    if (!seq && fr->aid == 0 && fr->key_len < 0x1000000u && fr->key_off <= F->t->input_len &&
        !F->parent_emitted) {
        (void)yt_put(F->t, yt_span_in(fr->key_off, fr->key_len, YTP_OPEN));
        fr->started = 0;
    } else {
        (void)yt_put(F->t, yt_props(YTP_SCALAR, 0, 0x2900u));
        (void)yt_put(F->t, yt_span_in(fr->key_off, fr->key_len, YTP_SPAN_IN));
        fr->started = 0;
    }
    F->t->depth++;
}

static int yt_f_run(yt_fused* F) {
    const char* p = F->p;
    size_t len = F->len;
    yep_ytape* t = F->t;
    yt_frame st[YT_FUSED_MAX_FRAMES];
    int depth = 0;
    memset(&st[0], 0, sizeof(st[0]));
    int root_open = 0;
    (void)yt_put(t, ((uint64_t)YTP_EV_COMPACT << 56) | 1u);
    (void)yt_put(t, ((uint64_t)YTP_EV_COMPACT << 56) | 3u);
    size_t pos = 0;
    while (pos < len) {
        /* the sweep: one NUL-padded SWAR walk per line */
        const uint64_t k_nl = 0x0A0A0A0A0A0A0A0Aull, k_cr = 0x0D0D0D0D0D0D0D0Dull,
                       k_sp = 0x2020202020202020ull, k_co = 0x3A3A3A3A3A3A3A3Aull,
                       k_ha = 0x2323232323232323ull;
        size_t whole = len - pos;
        size_t end = whole, indent = whole, stop = whole;
        int have_indent = 0, stop_set = 0;
        size_t i = 0;
        while (i < whole) {
            size_t avail = whole - i < 8 ? whole - i : 8;
            uint64_t x;
            if (avail == 8) {
                memcpy(&x, p + pos + i, 8);
            } else {
                char buf[8] = {0, 0, 0, 0, 0, 0, 0, 0};
                memcpy(buf, p + pos + i, avail);
                memcpy(&x, buf, 8);
            }
            uint64_t valid = avail == 8 ? ~(uint64_t)0 : (((uint64_t)1 << (avail * 8)) - 1);
            uint64_t br = (yep_swar_eq8(x, k_nl) | yep_swar_eq8(x, k_cr)) & valid;
            uint64_t room = (br ? ((br & (~br + 1)) - 1) : valid) & YEP_SWAR_FLAGS & valid;
            if (!have_indent) {
                uint64_t nons = ~(yep_swar_eq8(x, k_sp)) & room;
                if (nons) {
                    indent = i + (size_t)(yep_ctz64(nons) >> 3);
                    have_indent = 1;
                }
            }
            if (have_indent && !stop_set) {
                uint64_t stm = (yep_swar_eq8(x, k_co) | yep_swar_eq8(x, k_ha)) & room;
                if (stm) {
                    size_t cand = i + (size_t)(yep_ctz64(stm) >> 3);
                    if (cand >= indent) {
                        stop = cand;
                        stop_set = 1;
                    }
                }
            }
            if (br) {
                end = i + (size_t)(yep_ctz64(br) >> 3);
                break;
            }
            i += 8;
        }
        if (!have_indent) {
            indent = end;
        }
        if (!stop_set) {
            stop = end;
        }
        uint32_t line_end = (uint32_t)(pos + end);
        uint32_t line_indent = (uint32_t)(pos + indent);
        uint32_t line_stop = (uint32_t)(pos + stop);
        uint8_t blank = (uint8_t)(indent >= end);
        unsigned char c = blank ? 0 : (unsigned char)p[line_indent];
        uint8_t comment =
            (uint8_t)(c == '#' && (line_indent == 0 || p[line_indent - 1] == ' ' ||
                                   p[line_indent - 1] == '\t' || p[line_indent - 1] == '\n'));
        uint8_t dash = 0;
        if (c == '-' && line_indent + 1 < line_end) {
            unsigned char n1 = (unsigned char)p[line_indent + 1];
            if (n1 == ' ' || n1 == '\t') {
                dash = 1;
            }
        }
        size_t next = line_end < len ? (size_t)line_end + 1 : len;
        if (!blank && !comment) {
            size_t t0 = line_indent;
            uint32_t col = (uint32_t)(line_indent - pos);
            if (c == '\t') {
                return 1; /* tab-led content: the engine's error */
            }
            if (dash && depth > 0 && st[depth].pending && col >= st[depth].indent) {
                st[depth].kind = 1;
                st[depth].indent = col;
                F->parent_emitted = depth >= 1 ? st[depth - 1].emitted : root_open;
                yt_f_open(F, &st[depth], 1);
                yt_f_start(F, &st[depth]);
                root_open = 1;
                st[depth].pending = 0;
                if (depth >= 1) {
                    st[depth - 1].emitted = 1; /* NOT content_col: item
                                                * columns are not the parent's pair column */
                }
            } else {
                while (depth > 0 && (st[depth].kind == 1 ? (col < st[depth].indent ||
                                                            (col <= st[depth].indent && !dash))
                                                         : col <= st[depth].indent)) {
                    if (st[depth].pending) {
                        return 1; /* empty "key:" mid-doc: the engine owns it */
                    }
                    yt_f_close(F, st[depth].kind);
                    depth--;
                }
            }
            if (dash) {
                if (st[depth].kind == 1 && col >= st[depth].indent) {
                    yt_f_start(F, &st[depth]);
                    size_t vt = t0 + 1;
                    while (vt < line_end && (p[vt] == ' ' || p[vt] == '\t')) {
                        vt++;
                    }
                    uint32_t vend = line_end;
                    while (vend > vt && (p[vend - 1] == ' ' || p[vend - 1] == '\t')) {
                        vend--;
                    }
                    if (vend == vt) {
                        return 1; /* empty item */
                    }
                    unsigned char vc = (unsigned char)p[vt];
                    if (vc == '"' || vc == '\'' || vc == '*' || vc == '&' || vc == '[' ||
                        vc == '{' || vc == '!' || vc == '|' || vc == '>' || vc == '%' ||
                        vc == '?') {
                        return 1;
                    }
                    if (vc == '-' && vt + 1 < line_end && (p[vt + 1] == ' ' || p[vt + 1] == '\t')) {
                        return 1; /* nested dash: the engine's nested seq */
                    }
                    /* compact map item "- k: v"? */
                    for (size_t ic = vt; ic < vend; ic++) {
                        if (p[ic] == ':') {
                            size_t nn = ic + 1;
                            if (nn >= vend || p[nn] == ' ' || p[nn] == '\t') {
                                return 1; /* compact items: the engine */
                            }
                        }
                    }
                    (void)yt_put(t, yt_span_in((uint32_t)vt, vend - (uint32_t)vt, YTP_ITEM));
                    st[depth].emitted = 1;
                    if (!st[depth].content_col) {
                        st[depth].content_col = col + 1;
                    }
                } else {
                    return 1;
                }
            } else if (stop_set && line_stop + 1 < line_end && p[line_stop] == ':' &&
                       (p[line_stop + 1] == ' ' || p[line_stop + 1] == '\t') &&
                       !(c == '&' || c == '!' || c == '*' || c == '\'' || c == '"' || c == '[' ||
                         c == '{' || c == '|' || c == '>' || c == '%' || c == '?' || c == '-')) {
                uint32_t kend = line_stop;
                while (kend > t0 && (p[kend - 1] == ' ' || p[kend - 1] == '\t')) {
                    kend--;
                }
                if ((uint32_t)(kend - t0) > 1024u) {
                    return 1; /* the simple-key length law: the engine errors */
                }
                size_t vt = line_stop + 1;
                while (vt < line_end && (p[vt] == ' ' || p[vt] == '\t')) {
                    vt++;
                }
                uint32_t aoff = 0, alen = 0, aid = 0;
                if (vt < line_end && p[vt] == '&') {
                    size_t ne = vt + 1;
                    while (ne < line_end && ne < len) {
                        unsigned char pc = (unsigned char)p[ne];
                        if (pc <= ' ' || pc == ',' || pc == '[' || pc == ']' || pc == '{' ||
                            pc == '}' || pc == '#') {
                            break;
                        }
                        ne++;
                    }
                    aoff = (uint32_t)(vt + 1);
                    alen = (uint32_t)(ne - vt - 1);
                    F->anchor_seq++;
                    aid = F->anchor_seq;
                    yt_anchor_put(F, aoff, alen, aid);
                    vt = ne;
                    while (vt < line_end && (p[vt] == ' ' || p[vt] == '\t')) {
                        vt++;
                    }
                }
                if (vt >= line_end || p[vt] == '#') {
                    /* "key:" with a following-lines container: push pending */
                    if (aid != 0) {
                        return 1; /* anchored container: the engine route */
                    }
                    if (st[depth].kind == 0 && st[depth].emitted &&
                        col + 1 != st[depth].content_col) {
                        return 1; /* continuation: the multiline arm */
                    }
                    if (depth >= YT_FUSED_MAX_FRAMES - 1) {
                        return 1;
                    }
                    if (depth > 0 && st[depth].pending) {
                        F->parent_emitted = st[depth - 1].emitted;
                        yt_f_open(F, &st[depth], 0);
                        root_open = 1;
                        st[depth].pending = 0;
                        st[depth - 1].emitted = 1;
                        if (!st[depth - 1].content_col) {
                            st[depth - 1].content_col = col + 1;
                        }
                    }
                    depth++;
                    memset(&st[depth], 0, sizeof(st[depth]));
                    st[depth].indent = col;
                    st[depth].pending = 1;
                    st[depth].key_off = (uint32_t)t0;
                    st[depth].key_len = kend - (uint32_t)t0;
                } else {
                    if (st[depth].kind == 0 && st[depth].emitted &&
                        col + 1 != st[depth].content_col) {
                        return 1; /* off-column: a continuation line — the
                                   * multiline arm owns those */
                    }
                    if (depth > 0 && st[depth].pending) {
                        F->parent_emitted = st[depth - 1].emitted;
                        yt_f_open(F, &st[depth], 0);
                        root_open = 1;
                        st[depth].pending = 0;
                        st[depth - 1].emitted = 1;
                        if (!st[depth - 1].content_col) {
                            st[depth - 1].content_col = col + 1;
                        }
                    } else if (!root_open && depth == 0) {
                        (void)yt_put(t, yt_props(YTP_EVENT, 0, 7u));
                        yt_f_empty_span(F);
                        root_open = 1;
                        st[0].started = 1;
                    }
                    yt_f_start(F, &st[depth]);
                    if (st[depth].kind != 0) {
                        return 1;
                    }
                    unsigned char vc = (unsigned char)p[vt];
                    uint32_t flags;
                    if (vc == '*' && aid != 0) {
                        return 1; /* an anchored alias: the engine owns it */
                    }
                    if (vc == '*') {
                        size_t ne = vt + 1;
                        while (ne < line_end) {
                            unsigned char pc = (unsigned char)p[ne];
                            if (pc <= ' ' || pc == ',' || pc == '[' || pc == ']' || pc == '{' ||
                                pc == '}' || pc == '#') {
                                break;
                            }
                            ne++;
                        }
                        uint32_t taid = 0;
                        if (!yt_anchor_get(F, (uint32_t)(vt + 1), (uint32_t)(ne - vt - 1), &taid)) {
                            return 1; /* undefined alias: the engine errors */
                        }
                        (void)yt_put(t, yt_props(YTP_PAIR, taid, 3u /*ALIAS*/));
                        (void)yt_put(t, yt_span_in((uint32_t)t0, kend - (uint32_t)t0, YTP_SPAN_IN));
                        (void)yt_put(t, yt_span_in((uint32_t)(vt + 1), (uint32_t)(ne - vt - 1),
                                                   YTP_SPAN_IN));
                        st[depth].emitted = 1;
                        goto line_done;
                    }
                    if (vc == '"' || vc == '\'' || vc == '|' || vc == '>' || vc == '[' ||
                        vc == '{' || vc == '!' || vc == '%') {
                        return 1; /* quoted/block/flow/tag values: the engine */
                    }
                    if ((vc == '-' || vc == '?') && (vt + 1 >= line_end || p[vt + 1] == ' ' ||
                                                     p[vt + 1] == '\t' || p[vt + 1] == '#')) {
                        return 1; /* an indicator-as-value: the engine errors */
                    }
                    /* a terminating ':' inside the value is the
                     * mapping-values error — the engine owns it */
                    for (const char* h = (const char*)memchr(p + vt, ':', line_end - vt);
                         h != NULL && h < p + line_end;
                         h = (const char*)memchr(h + 1, ':', line_end - (size_t)(h + 1 - p))) {
                        if (h + 1 >= p + line_end || h[1] == ' ' || h[1] == '\t') {
                            return 1;
                        }
                    }
                    /* trailing comment bounds the plain value */
                    uint32_t vend = line_end;
                    {
                        const char* h = (const char*)memchr(p + vt, '#', line_end - vt);
                        while (h != NULL) {
                            if (h > p + vt && (h[-1] == ' ' || h[-1] == '\t')) {
                                vend = (uint32_t)(h - p);
                                break;
                            }
                            h = (const char*)memchr(h + 1, '#', line_end - (size_t)(h + 1 - p));
                        }
                    }
                    while (vend > vt && (p[vend - 1] == ' ' || p[vend - 1] == '\t')) {
                        vend--;
                    }
                    if (aid != 0) {
                        flags = 4u | (1u << 3) | (1u << 4);
                    } else {
                        flags = 2u | (1u << 3);
                    }
                    (void)yt_put(t, yt_props(YTP_PAIR, aid, flags));
                    (void)yt_put(t, yt_span_in((uint32_t)t0, kend - (uint32_t)t0, YTP_SPAN_IN));
                    (void)yt_put(t, yt_span_in((uint32_t)vt, vend - (uint32_t)vt, YTP_SPAN_IN));
                    if (aid != 0) {
                        (void)yt_put(t, yt_span_in(aoff, alen, YTP_SPAN_IN));
                    }
                    st[depth].emitted = 1;
                    if (!st[depth].content_col) {
                        st[depth].content_col = col + 1;
                    }
                }
            } else {
                return 1; /* unclassified line: the engine route */
            }
        }
    line_done:
        pos = next;
    }
    if (!root_open) {
        return 1; /* empty document: the engine owns it */
    }
    while (depth > 0) {
        if (st[depth].pending) {
            return 1; /* empty "key:" at EOF: the engine */
        }
        yt_f_close(F, st[depth].kind);
        depth--;
    }
    yt_f_close(F, 0); /* the root always closes, started or not */
    (void)yt_put(t, ((uint64_t)YTP_EV_COMPACT << 56) | 4u);
    (void)yt_put(t, ((uint64_t)YTP_EV_COMPACT << 56) | 2u);
    t->docs = 1;
    return 0;
}

int ytap_fused_run(yep_ytape* t) {
    yt_fused* F = (yt_fused*)yep_alloc(yep_system_allocator(), sizeof(*F));
    if (F == NULL) {
        return 1; /* allocation refused: the engine route */
    }
    memset(F, 0, sizeof(*F));
    F->t = t;
    F->p = t->input;
    F->len = t->input_len;
    size_t mark = t->count;
    int rc = yt_f_run(F);
    yep_free(yep_system_allocator(), F);
    if (rc != 0) {
        t->count = (uint32_t)mark; /* unwind: the caller resets and re-runs */
        t->depth = 0;
        return 1;
    }
    return 0;
}

void ytap_reset(yep_ytape* t) {
    t->count = 0;
    t->depth = 0;
    t->docs = 0;
    t->oom = 0;
    t->flow_mark = 0;
}
