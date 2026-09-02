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

/* Compact node strings: (region-tagged offset, length) instead of
 * pointers — the node drops to <= 64 B (TODO.impl/11's gate). Two
 * regions: the borrowed parse input (offset from its base) and the
 * DOM's own contiguous string arena (offset from its start; the
 * arena grows by realloc, which MOVES bytes but never invalidates
 * OFFSETS — the leptris arena discipline). Absence = len 0. */
#define YEP_SV_INPUT 0x80000000u /* region bit: 1 = arena, 0 = input */
#define YEP_SV_OFF 0x7FFFFFFFu   /* 31-bit offsets (2 GiB ceiling) */

typedef struct yep_sview {
    uint32_t off; /* YEP_SV_INPUT | byte offset into the region */
    uint32_t len;
} yep_sview;

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
    yep_sview value; /* scalar content / alias name */
    yep_sview tag;
    yep_sview anchor;
    uint32_t line;
    uint32_t col;
} yep_dnode;

/* 11's node-size gate: compact views keep the dense record <= 64 B.
 * (_Static_assert is C; this header reaches C++ test TUs.) */
#if defined(__cplusplus)
static_assert(sizeof(yep_dnode) <= 64, "yep_dnode exceeds the 64 B gate");
#else
_Static_assert(sizeof(yep_dnode) <= 64, "yep_dnode exceeds the 64 B gate");
#endif

typedef struct yep_dom {
    const yep_allocator* sys;
    yep_pool* pool; /* node/docs arrays (strings live in str) */
    /* string arena: contiguous, realloc-grown; node strings are
     * OFFSETS into it, so growth never dangles (dom_str_put) */
    char* str;
    uint32_t str_len, str_cap;
    const char* input_base; /* borrowed parse input (offset base) */
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

/* Decodes to the pointer view callers use (defined after yep_dom).
 * Absent views (len 0) decode to a valid base pointer with length
 * 0 — callers test len. */
static inline yep_view yep_dom_view(const yep_dom* d, yep_sview sv) {
    yep_view v = {NULL, 0};
    if (sv.len == 0) {
        return v; /* NULL+0 would be UB on a regionless document */
    }
    v.len = sv.len;
    v.p = (sv.off & YEP_SV_INPUT ? d->str : d->input_base) + (sv.off & YEP_SV_OFF);
    return v;
}

/* Input-region encoding; arena copies go through dom_str_put. */
static inline yep_sview yep_sv_input(const yep_dom* d, yep_view v) {
    yep_sview sv = {(uint32_t)((const char*)v.p - d->input_base), v.len};
    return sv;
}

yep_dom* yep_dom_create(const yep_allocator* sys);

/* Pre-sizes the node array and string arena from content-derived
 * hints (TODO.impl/06: one SIMD count pass -> reserves -> at most
 * logarithmic growth remains instead of doubling from empty).
 * Hints are advisory: overshoot wastes memory, undershoot falls
 * back to normal growth. 0 hint skips that reserve. */
void yep_dom_reserve(yep_dom* d, uint32_t node_hint, uint32_t str_hint);

/* One-shot sizing from the input (count passes + reserve above) —
 * the single home of the heuristic; parse_impl and the memory bench
 * both call it so the measure and the product can't drift. */
void yep_dom_prepare(yep_dom* d, const char* buf, size_t len);

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

/* Appends len bytes to the string arena; returns the ARENA-offset
 * encoding (input_base-relative encodings use yep_sv_input). */
yep_sview yep_dom_str_put(yep_dom* d, const char* p, uint32_t len);

/* Reserve/commit for in-place decoding (the JSON builder's escape
 * path): tail ensures capacity and returns the write position at
 * str_len; commit advances by the written length and returns the
 * arena-encoded view. */
char* yep_dom_str_tail(yep_dom* d, uint32_t cap);
yep_sview yep_dom_str_commit(yep_dom* d, uint32_t written);
void yep_dom_destroy(yep_dom* d);
int yep_dom_on_event(void* ctx, const yep_event* ev);

/* Node id accessor for the public API layer. */
const yep_dnode* yep_dom_node(const yep_dom* d, uint32_t id);

#ifdef __cplusplus
}
#endif

#endif /* YEP_DOM_H */
