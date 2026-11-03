/* validate/internal.h — the validation face's shared internals (#882).
 *
 * Private to src/yeptris/validate/: the report shape, the diagnostic
 * emitter (path capture + line/col resolution), and the JSON-pointer
 * scratch path. The two walks (validate.c's schema family and
 * jsonschema.c's keyword subset) share only this — MECE keeps the
 * checks themselves in their own TUs.
 */
#ifndef YEP_VALIDATE_INTERNAL_H
#define YEP_VALIDATE_INTERNAL_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <yeptris/validate.h>

#include "../dom/dom.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The report cap: a hostile document can carry O(nodes) violations, so
 * the walk stops here and the LIMIT sentinel replaces the last slot. */
#define YEP_VALIDATE_MAX_DIAGS 256

struct yeptris_validate_report {
    yeptris_diagnostic* diags;
    uint32_t count;
    int truncated;
};

typedef struct yep_vctx {
    const yep_dom* dom;
    yeptris_validate_report* rep;
    /* line/col resolution over the node bytes (lazy: built on the
     * first positioned diagnostic; valid documents never pay) */
    const char* input;
    size_t input_len;
    uint32_t* line_off; /* byte offset of every line start */
    uint32_t line_cnt, line_cap;
    /* JSON-pointer path under construction (DFS scratch) */
    char* path;
    size_t path_len, path_cap;
    int depth; /* the walk's nesting guard (never exceeds the DOM's) */
    int oom;   /* an allocation failed: stop diagnosing */
    /* JSON Schema trial suppression (anyOf/oneOf/not probes): emits
     * inside a probe never reach the report and never touch
     * fail_mark — the probe's own validity is ITS trial_fail delta
     * (zero delta = held), the enclosing application's is its
     * fail_mark delta */
    int suppress;
    /* the walks' invalid-application counters: every emit increments
     * exactly one of them (suppressed -> trial_fail, otherwise
     * fail_mark), so an application's validity is one comparison (the
     * unevaluated* annotation model reads fail_mark) */
    int fail_mark;
    int trial_fail;
    int ref_depth; /* consecutive $ref/$dynamicRef expansions */
} yep_vctx;

/* Offset of a node's first positioned byte (the value span of the node
 * itself, or of its first scalar leaf for collections; UINT32_MAX when
 * everything reachable lives in the arena — line/col unknown). */
uint32_t yep_validate_node_off(const yep_dom* d, uint32_t id);

/* Emits one diagnostic at the current path. line/col resolve from
 * `off` through the lazy line index; 0/0 when unknown. Detail is
 * printf-composed into the bounded slot. Never fails the caller: the
 * report cap turns the last slot into the LIMIT sentinel. */
void yep_validate_emit(yep_vctx* c, int code, uint32_t off, const char* detail_fmt, ...);

/* JSON-pointer path scratch: push an escaped segment / a decimal
 * index, pop to a mark. 0 ok, -1 OOM (ctx.oom set; the walk unwinds). */
int yep_validate_path_push(yep_vctx* c, const char* seg, size_t seg_len);
int yep_validate_path_push_index(yep_vctx* c, uint32_t idx);
void yep_validate_path_pop(yep_vctx* c, size_t mark);

/* The stream face's document attribution (#885 item 4): a validated
 * stream is addressed as an array of documents — when it holds MORE
 * THAN ONE document, every diagnostic path is prefixed with the
 * document's index segment, so diagnostics name their document ("/1/k"
 * is key k of document 1; the line/col stay input-absolute). Streams
 * with a single document keep the bare instance pointers (the
 * JSON-Schema-Test-Suite's paths and every single-doc pin are
 * unchanged). Call before a document's walk and pop the returned mark
 * after it; OOM sets ctx.oom and the walk unwinds. */
size_t yep_validate_doc_prefix(yep_vctx* c, int qualified, uint32_t index);

/* The bounded ECMA-regex subset behind `pattern` (jsonschema.c):
 * literals, '.', classes, groups, alternation, the greedy/lazy
 * quantifiers, anchors, \d\w\s\D\W\S\b\B and \xHH. Byte-oriented
 * (ASCII classes; UTF-8 literals match byte-wise). A Thompson NFA —
 * O(len * m), no backtracking blowup, no recursion over the subject.
 * Patterns over the program budget are invalid. 1 match, 0 no match,
 * -1 invalid pattern. */
int yep_re_search(const char* pat, uint32_t pat_len, const char* s, uint32_t s_len);

/* The retained-program cache behind the compiled schema (#885 item 3):
 * pattern source -> compiled NFA, populated at compile time, immutable
 * afterwards (read-only sharing, the document law). A miss — a NULL
 * cache included — compiles fresh with yep_re_search's exact verdicts;
 * cache_add returns 1 stored, 0 not retained (invalid pattern, empty,
 * table full — misses recompile), -1 allocation failure (loud). */
typedef struct yep_re_cache yep_re_cache;
yep_re_cache* yep_re_cache_new(void);
void yep_re_cache_free(yep_re_cache* t);
int yep_re_cache_add(yep_re_cache* t, const char* pat, uint32_t plen);
int yep_re_cache_search(const yep_re_cache* t, const char* pat, uint32_t plen, const char* s,
                        uint32_t s_len);

/* Canonical scalar tag of a DOM node (explicit tag, then the parse
 * resolver's plain verdict, then str): the value model both walks
 * compare under. */
uint8_t yep_validate_scalar_tag(const yep_dnode* n);

/* Canonical-form hash and equality over DOM subtrees (spec 3.2.1.3:
 * same tag + canonical form; numbers numerically within a tag). The
 * equality may cross documents — the schema's enum/const live in a
 * different DOM than the instance. Depth-bounded. */
uint64_t yep_validate_node_hash(const yep_dom* d, uint32_t id, int depth);
int yep_validate_node_equal(const yep_dom* da, uint32_t a, const yep_dom* db, uint32_t b,
                            int depth);

/* ---- the schema identifier map (resources.c) ----------------------------- */

/* Bounded identifier tables: an identifier past a cap (or a URI over
 * the length cap) is not registered — the reference then fails to
 * resolve at validation (DIAG_REF), exactly like any other unknown
 * target. Nothing loops, nothing guesses. */
#define YEP_JS_MAX_RESOURCES 1024
#define YEP_JS_MAX_ANCHORS 1024
#define YEP_JS_MAX_REFSITES 8192
#define YEP_JS_URI_MAX 512
#define YEP_JS_MAX_SCOPE 256 /* the runtime dynamic-scope stack */

/* One schema RESOURCE: a canonical (fragment-free) URI naming the
 * schema object that established it with "$id" — pointer fragments
 * walk from that node. The document root is resource 0 (the empty
 * base "" when the schema has no "$id"). */
typedef struct {
    char* uri;
    uint32_t root;
} js_resource;

/* A plain-name fragment: canonical URI + '#' + name -> the node.
 * `dynamic` distinguishes "$dynamicAnchor" (the $dynamicRef
 * extension points) from "$anchor". `res` is the owning resource
 * index (anchors belong to the resource of their nearest "$id"). */
typedef struct {
    char* key;
    uint32_t node;
    uint32_t res;
    uint8_t dynamic;
} js_anchor;

/* A "$ref"/"$dynamicRef" site: the canonical base URI it resolves
 * against, and that base resource's root (where fragment-only
 * pointers start walking). */
typedef struct {
    uint32_t site;
    char* base;
    uint32_t base_root;
} js_refsite;

/* The compiled schema's identifier map (built once by the crawl at
 * compile time, read-only afterwards — the document law). Everything
 * it owns dies with yep_js_resources_free. */
typedef struct {
    js_resource* res;
    uint32_t res_n;
    js_anchor* anc;
    uint32_t anc_n;
    js_refsite* site;
    uint32_t site_n;
    uint32_t* rindex; /* node id -> owning resource index (UINT32_MAX unvisited) */
    uint32_t rindex_n;
} js_resources;

/* The constructor: one bounded DFS over the schema DOM collecting
 * every $id/$anchor/$dynamicAnchor and $ref/$dynamicRef site. NULL on
 * allocation failure. */
js_resources* yep_js_resources_crawl(const yep_dom* d, uint32_t root);
void yep_js_resources_free(js_resources* r);

/* Lookups (linear over the bounded tables): resource by canonical
 * URI; anchor by key (URI + '#' + name); a resource's DYNAMIC anchor
 * by plain name (the dynamic-scope search's per-resource probe); a
 * ref site's base. Each returns 1 found, 0 absent. */
int js_res_find(const js_resources* r, const char* uri, uint32_t* idx_out);
int js_anchor_find(const js_resources* r, const char* key, size_t key_len, uint32_t* node_out,
                   uint8_t* dyn_out);
int js_anchor_dynamic_in_res(const js_resources* r, uint32_t res_idx, const char* name,
                             size_t name_len, uint32_t* node_out);
int js_refsite_find(const js_resources* r, uint32_t site_node, const char** base,
                    uint32_t* base_root);

/* URI-reference resolution (RFC 3986 section 5.2, the exercised
 * subset: schemes, authorities, path merge with dot-segment removal,
 * query carry). Produces the canonical fragment-free form. 0 ok, -1
 * overlong (out holds ""). */
int js_uri_resolve(const char* base, const char* ref, size_t ref_len, char* out, size_t cap);

/* jsonschema.c's driver: applies the schema tree (its own DOM) to
 * every instance document root, emitting into `rep`. `cache` is the
 * compiled schema's retained pattern programs (NULL: none — the
 * one-shot face). `res` is the compiled schema's identifier map (the
 * resources/anchors/ref-site bases the reference machinery resolves
 * through; NULL: none — every reference beyond a bare "#/..." pointer
 * stays unresolvable). Returns 0, -1 on OOM. */
int yep_jsonschema_run(const yep_dom* schema_dom, uint32_t schema_root, const yep_dom* instance_dom,
                       const uint32_t* doc_roots, uint32_t doc_count, yeptris_validate_report* rep,
                       const yep_re_cache* cache, const js_resources* res);

/* The report constructor (validate.c owns the report machinery): a
 * fresh empty report, NULL on allocation failure. */
yeptris_validate_report* yep_validate_report_new(void);

/* compile.c's seam (#885 item 3): the compiled-schema constructor with
 * an explicit status — the one-shot face preserves its exact observable
 * statuses through it. NULL on failure with *st = YEPTRIS_ERROR_ARG
 * (NULL bytes with length, or bytes that hold no document; the thread
 * error channel carries the message), the failed parse's status, or
 * YEPTRIS_ERROR_MEMORY. */
struct yeptris_schema;
yeptris_schema* yep_schema_compile_st(const char* bytes, size_t n, YeptrisStatus* st);

#ifdef __cplusplus
}
#endif

#endif /* YEP_VALIDATE_INTERNAL_H */
