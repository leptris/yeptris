/* events.h — the one grammar vocabulary (TODO.impl/07).
 *
 * Every consumption model (DOM builder, pull, push, recorder — 11/12) is
 * a sink over these events; the engine knows none of them (OCP). Values
 * are borrowed either from the input buffer or from the engine's finish
 * pool (folded/escaped content) — `borrowed` says which, so the DOM
 * builder copies exactly what must outlive the engine.
 */
#ifndef YEP_EVENTS_H
#define YEP_EVENTS_H

#include "common/string_view.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YEP_EV_NONE = 0,
    YEP_EV_STREAM_START,
    YEP_EV_STREAM_END,
    YEP_EV_DOCUMENT_START,
    YEP_EV_DOCUMENT_END,
    YEP_EV_SEQ_START,
    YEP_EV_SEQ_END,
    YEP_EV_MAP_START,
    YEP_EV_MAP_END,
    YEP_EV_SCALAR,
    YEP_EV_ALIAS,
} yep_event_type;

/* Mirrors libyaml's style set — the wire vocabulary of 12's compat layer. */
typedef enum {
    YEP_STYLE_ANY = 0,
    YEP_STYLE_PLAIN,
    YEP_STYLE_SINGLE_QUOTED,
    YEP_STYLE_DOUBLE_QUOTED,
    YEP_STYLE_LITERAL,
    YEP_STYLE_FOLDED,
} yep_scalar_style;

typedef struct yep_event {
    yep_event_type type;
    yep_view value;  /* scalar content / alias name */
    yep_view anchor; /* node properties, empty when absent */
    yep_view tag;
    uint8_t style;      /* yep_scalar_style */
    uint8_t implicit;   /* scalar written without quotes/tag (plain implicit) */
    uint8_t flow;       /* collection opened in flow context */
    uint8_t multiline;  /* scalar spanned lines: may not be a simple key */
    uint8_t tag_id;     /* resolved implicit/explicit tag (resolve/resolver.h) */
    uint8_t borrowed;   /* 1: value borrows the INPUT; 0: engine finish pool */
    uint32_t anchor_id; /* 1-based anchor ordinal (the engine's serial);
                         * on ALIAS: the TARGET's ordinal. 0 = none. Sinks
                         * that key on it skip the name hash entirely. */
    uint32_t line;      /* 1-based position of the node start */
    uint32_t col;
} yep_event;

struct yep_block_value; /* value facts for on_block_pair (below) */

typedef struct yep_sink {
    /* Returns 0 to continue, nonzero to abort the parse (sink error). */
    int (*on_event)(void* ctx, const yep_event* ev);
    void* ctx;
    /* Optional flow fast path (TODO.restructure/50): called right
     * after the engine strictly validates a JSON-class flow span
     * [open, close] and rules out the key/fallback shapes. p is the
     * parse buffer; line/line_start are the engine's position facts at
     * `open` (for node line/col). Return 1 = subtree built (the
     * engine continues past the close), 0 = not handled (the engine
     * emits the span's events exactly as before), <0 = abort. Sinks
     * that consume events (pull/push/recorder) leave it NULL. */
    int (*on_flow_json)(void* ctx, const char* p, size_t open, size_t close, uint32_t line,
                        size_t line_start, yep_view anchor, yep_view tag, uint32_t anchor_id);
    /* Optional block fast path (TODO.restructure/54): the engine's
     * classified KEY arm offers the whole line pair — key scalar plus
     * a classified value — before emitting any event. line/cols are
     * the engine's position facts. Return 1 = both nodes built and
     * placed, 0 = not handled (the engine emits the two events
     * exactly as before), <0 = abort. */
    int (*on_block_pair)(void* ctx, const yep_view* key, const struct yep_block_value* v,
                         uint32_t line, uint16_t key_col, uint16_t val_col);
} yep_sink;

/* Value facts for on_block_pair (the classified value classes the
 * block arms own; spans are input-borrowed). */
typedef struct yep_block_value {
    uint8_t cls;        /* YEP_LVAL_PLAIN / _ALIAS / _ANCHOR_PLAIN */
    uint8_t borrowed;   /* 1: value borrows the input; 0: engine pool */
    yep_view value;     /* scalar content / alias name */
    yep_view anchor;    /* _ANCHOR_PLAIN: the &name span */
    uint32_t anchor_id; /* engine ordinal: the definition (_ANCHOR_)
                         * or the resolved TARGET (_ALIAS_) */
} yep_block_value;

#ifdef __cplusplus
}
#endif

#endif /* YEP_EVENTS_H */
