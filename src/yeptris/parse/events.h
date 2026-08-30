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
    uint8_t style;    /* yep_scalar_style */
    uint8_t implicit; /* scalar written without quotes/tag (plain implicit) */
    uint8_t flow;     /* collection opened in flow context */
    uint8_t borrowed; /* 1: value borrows the INPUT; 0: engine finish pool */
    uint32_t line;    /* 1-based position of the node start */
    uint32_t col;
} yep_event;

typedef struct yep_sink {
    /* Returns 0 to continue, nonzero to abort the parse (sink error). */
    int (*on_event)(void* ctx, const yep_event* ev);
    void* ctx;
} yep_sink;

#ifdef __cplusplus
}
#endif

#endif /* YEP_EVENTS_H */
