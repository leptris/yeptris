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
    /* JSON Schema trial suppression (anyOf/oneOf/not probes) */
    int suppress;
    int trial_hit;
    int ref_depth; /* consecutive $ref expansions */
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

/* The bounded ECMA-regex subset behind `pattern` (jsonschema.c):
 * literals, '.', classes, groups, alternation, the greedy/lazy
 * quantifiers, anchors, \d\w\s\D\W\S\b\B and \xHH. Byte-oriented
 * (ASCII classes; UTF-8 literals match byte-wise). A Thompson NFA —
 * O(len * m), no backtracking blowup; patterns over the program
 * budget are invalid. 1 match, 0 no match, -1 invalid pattern. */
int yep_re_search(const char* pat, uint32_t pat_len, const char* s, uint32_t s_len);

#ifdef __cplusplus
}
#endif

#endif /* YEP_VALIDATE_INTERNAL_H */
