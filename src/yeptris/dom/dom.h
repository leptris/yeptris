/* dom.h — minimal event-built DOM (TODO.impl/11 v1).
 *
 * Nodes are dense records in a pool; children are sibling-linked
 * (first_child/last_child/next_sibling) — safe when parent and child
 * collections interleave. Anchors bind to node ids. The builder is a
 * sink over the engine (MECE: the DOM never parses). v1 deviation from
 * the board (recorded there): one dom.c with a kind enum — per-kind
 * vtables land with mutation + emitter integration, where polymorphic
 * behavior actually appears.
 *
 * Lifetime: borrowed event values (from the caller's input) stay views
 * into that input; engine-pool values (folded/escaped) are copied into
 * the DOM's pool. The input buffer must outlive the document.
 */
#ifndef YEP_DOM_H
#define YEP_DOM_H

#include <stdint.h>

#include "common/nametab.h"
#include "memory/allocator.h"
#include "memory/pool.h"
#include "parse/events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YEP_DOM_SCALAR = 0,
    YEP_DOM_SEQUENCE,
    YEP_DOM_MAPPING,
    YEP_DOM_ALIAS,
} yep_dom_kind;

#define YEP_DOM_MAX_DEPTH 1000

typedef struct yep_dnode {
    uint8_t kind;
    uint8_t style;
    uint8_t implicit;
    uint8_t tag_id;       /* resolved tag (resolve/resolver.h) */
    uint8_t flow;         /* collection opened in flow style */
    uint32_t first_child; /* child link (mappings: key,value,key,value…) */
    uint32_t last_child;
    uint32_t next_sibling;
    uint32_t count;  /* children (mappings: 2 × pairs) */
    uint32_t target; /* alias: target node id */
    yep_view value;  /* scalar content / alias name */
    yep_view tag;
    yep_view anchor;
    uint32_t line;
    uint32_t col;
} yep_dnode;

typedef struct yep_dom {
    const yep_allocator* sys;
    yep_pool* pool; /* owned copies of non-borrowed values */
    yep_dnode* nodes;
    uint32_t ncount, ncap;
    uint32_t* docs; /* document root node ids */
    uint32_t dcount, dcap;
    yep_nametab anchors; /* anchor name -> binding node id */
    /* builder state */
    uint32_t stack[YEP_DOM_MAX_DEPTH];
    int depth;
    int doc_open;
    int map_pending_key[YEP_DOM_MAX_DEPTH]; /* per-frame: key seen, value next */
    uint32_t pending_key_id[YEP_DOM_MAX_DEPTH];
    uint32_t pending_key;
} yep_dom;

yep_dom* yep_dom_create(const yep_allocator* sys);
void yep_dom_destroy(yep_dom* d);
int yep_dom_on_event(void* ctx, const yep_event* ev);

/* Node id accessor for the public API layer. */
const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* YEP_DOM_H */
