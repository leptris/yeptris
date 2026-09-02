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
#include "dom/mapindex.h"
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
    uint8_t attached;     /* mutation: linked under a parent or a root */
    uint16_t depth;       /* mutation: root 0, child parent+1 (capped 1000) */
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
    /* handle arena: thread-safe (atomic bump) — query APIs allocate
     * YeptrisNode wrappers here, so read-only document sharing across
     * threads is safe; parse-path pools stay single-threaded */
    struct yep_hpool* handles;
    yep_dnode* nodes;
    uint32_t ncount, ncap;
    uint32_t* docs; /* document root node ids */
    uint32_t dcount, dcap;
    yep_nametab anchors;        /* anchor name -> binding node id */
    struct yep_midx_state midx; /* lazy per-map lookup index (mapindex.c) */
    /* builder state */
    uint32_t stack[YEP_DOM_MAX_DEPTH];
    int depth;
    int doc_open;
    int map_pending_key[YEP_DOM_MAX_DEPTH]; /* per-frame: key seen, value next */
    uint32_t pending_key_id[YEP_DOM_MAX_DEPTH];
    uint32_t pending_key;
} yep_dom;

yep_dom* yep_dom_create(const yep_allocator* sys);

/* builder helpers shared with mutate.c (dom-internal) */
int dom_grow_nodes(yep_dom* d, uint32_t need);
int dom_grow_docs(yep_dom* d, uint32_t need);
uint32_t dom_new_node(yep_dom* d, const yep_event* ev, uint8_t kind);
void dom_link(yep_dom* d, uint32_t parent, uint32_t child);

/* thread-safe handle arena (see hpool.c) */
struct yep_hpool* yep_hpool_create(const yep_allocator* sys);
void yep_hpool_destroy(struct yep_hpool* p);
void* yep_hpool_alloc(struct yep_hpool* p, size_t size, size_t align);

/* mutate.c — the from-scratch builder (TODO.impl/11 phase 3).
 *
 * Values passed at runtime have no input buffer to borrow from, so
 * they are copied into the DOM pool. Plain scalars are typed by the
 * core12 resolver (the typing SSOT — mutation never invents types);
 * quoted/block styles are strings. Synthesized documents resolve with
 * the default schema; a compat-mode constructor arrives with 15.
 *
 * Returns: node ids (UINT32_MAX = OOM); statuses 0 ok, -1 invalid
 * argument / OOM, -2 duplicate key (add), -3 would exceed
 * YEP_DOM_MAX_DEPTH, -4 node already attached (would alias a parent).
 */
uint32_t yep_mut_scalar(yep_dom* d, const char* value, size_t len, uint8_t style);
uint32_t yep_mut_seq(yep_dom* d, uint8_t flow);
uint32_t yep_mut_map(yep_dom* d, uint8_t flow);
int yep_mut_seq_add(yep_dom* d, uint32_t seq, uint32_t child);
int yep_mut_map_add(yep_dom* d, uint32_t map, const char* key, size_t klen, uint32_t value);
/* Replace-in-place (json-c semantics: position kept); 1 = replaced. */
int yep_mut_map_set(yep_dom* d, uint32_t map, const char* key, size_t klen, uint32_t value);
int yep_mut_seq_del(yep_dom* d, uint32_t seq, uint32_t index);
/* Replace the entry at index (json-c array_put_idx): unlinks the old
 * subtree and splices the new node in at the SAME position. */
int yep_mut_seq_set(yep_dom* d, uint32_t seq, uint32_t index, uint32_t value);
int yep_mut_map_del(yep_dom* d, uint32_t map, const char* key, size_t klen);
int yep_mut_add_root(yep_dom* d, uint32_t node);
void yep_mut_set_depths(yep_dom* d, uint32_t id, uint16_t depth);

/* Direct DOM construction from a strict-validated JSON buffer
 * (TODO.impl/27): no engine, no event pipeline. The caller validated
 * with yep_json_document; 0 on success, -1 OOM, -2 grammar surprise
 * (defensive). */
int yep_dom_build_json(yep_dom* d, const char* buf, size_t len);
void yep_dom_destroy(yep_dom* d);
int yep_dom_on_event(void* ctx, const yep_event* ev);

/* Node id accessor for the public API layer. */
const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* YEP_DOM_H */
