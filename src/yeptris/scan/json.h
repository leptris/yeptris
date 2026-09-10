/* json.h — the strict-JSON grammar interface (TODO.impl/08C/21).
 *
 * Kernels are exported (YEPTRIS_API) so host materializers (the Ruby
 * native extension's fused JSON→VALUE path) can call them without
 * pulling the whole static archive. */
#ifndef YEP_JSON_H
#define YEP_JSON_H

#include <stddef.h>
#include <yeptris/api.h>

#ifdef __cplusplus
extern "C" {
#endif

YEPTRIS_API int yep_json_ws(const char* p, size_t len, size_t* i, int* saw_tab);

/* Token-stream walker over a JSON-class flow span (TODO.restructure/53):
 * the ONE grammar walk, shared by the engine's validator and the DOM's
 * fused builder — validate and build ride the same state machine. */
#define YEP_JSON_WALK_DEPTH 256

typedef enum {
    YEP_JW_OK = 0,     /* token delivered in *t */
    YEP_JW_DONE = 1,   /* the span's matching close was consumed */
    YEP_JW_REJECT = 2, /* not JSON-class: the general kernel judges */
} yep_jw_status;

typedef struct yep_json_tok {
    size_t at;      /* token start (the quote/opener/digit) */
    size_t end;     /* token end (past the closing quote for strings) */
    char cls;       /* '"' string, '#' number, 'a' literal, '[', '{', ']', '}' */
    int has_escape; /* strings: a backslash escape is present */
    int is_float;   /* numbers (cls '#'): the text has '.' or an exponent */
} yep_json_tok;

typedef struct yep_json_walk {
    const char* p;
    size_t len;
    int max_depth; /* the engine's runtime nesting limit */
    size_t i;      /* cursor: one past the opener */
    size_t close;  /* valid once DONE */
    int long_key;  /* a map key over the 1024-char simple-key limit */
    int saw_tab;
    uint8_t kind[YEP_JSON_WALK_DEPTH];   /* 0 seq, 1 map */
    uint8_t expect[YEP_JSON_WALK_DEPTH]; /* walker states */
    int depth;
} yep_json_walk;

/* Opens over p[open] ('[' or '{'); call _next until DONE/REJECT. */
void yep_json_walk_init(yep_json_walk* w, const char* p, size_t len, size_t open, int max_depth);
yep_jw_status yep_json_walk_next(yep_json_walk* w, yep_json_tok* t);
YEPTRIS_API int yep_json_number(const char* p, size_t len, size_t* i);
/* The grammar walk fused with conversion (TODO.restructure/26): one
 * scan validates and converts. *is_float reports the TEXT shape:
 * 0 = integer (*iv exact, INT64_MIN included), 1 = float text
 * (*dv converted via the number-kernel SSOT), 2 = integer text
 * beyond int64 (*dv approximate; exact hosts rebuild from the span
 * start..*i). Out params may be NULL. */
YEPTRIS_API int yep_json_number_scan(const char* p, size_t len, size_t* i, int* is_float,
                                     int64_t* iv, double* dv);
YEPTRIS_API int yep_json_literal(const char* p, size_t len, size_t* i, const char* word);
YEPTRIS_API int yep_json_string(const char* p, size_t len, size_t* i, size_t* close_out,
                                int* has_esc);
YEPTRIS_API int yep_json_document(const char* p, size_t len, size_t* err);

#ifdef __cplusplus
}
#endif

#endif /* YEP_JSON_H */
