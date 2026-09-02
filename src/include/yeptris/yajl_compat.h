/* yajl_compat.h — the yajl generator migration layer (TODO.impl/21).
 *
 * REAL yajl_gen symbol names over the yeptris core: users link
 * -lyeptris-yajl instead of -lyajl and change nothing else. The
 * generator validates call order (yajl's own state machine) and
 * builds a Yeptris document; get_buf serializes JSON (beautify
 * selects the pretty writer, default compact).
 *
 * v1 scope: the generator API (yajl_gen_*). The incremental SAX
 * parser side (yajl_parse/yajl_handle) is served by 12's push API —
 * its yajl-shaped wrapper rides a later pass.
 */
#ifndef YEPTRIS_YAJL_COMPAT_H
#define YEPTRIS_YAJL_COMPAT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    yajl_gen_status_ok = 0,
    yajl_gen_keys_must_be_strings, /* 1 */
    yajl_max_depth_exceeded,       /* unused: depth-guarded builder */
    yajl_gen_in_error_state = 3,
    yajl_gen_generation_complete, /* 4: gen after get_buf without clear */
    yajl_gen_invalid_string = 5   /* NULL bytes with len 0 handled; reserved */
} yajl_gen_status;

typedef enum {
    yajl_gen_beautify = 1,      /* int: pretty output */
    yajl_gen_indent_string = 2, /* const char*: reserved (fixed 2-space) */
    yajl_gen_validate_utf8 = 3  /* int: always on (the front-end validates) */
} yajl_gen_option;

typedef struct yajl_gen_t* yajl_gen;
typedef struct yajl_alloc_funcs_t yajl_alloc_funcs;

/* NULL alloc (the yajl default) uses the system allocator. */
yajl_gen yajl_gen_alloc(const yajl_alloc_funcs* afs);
void yajl_gen_free(yajl_gen g);
void yajl_gen_reset(yajl_gen g, const char* sep_ignored);

/* var-args: (yajl_gen_beautify, int). Returns 1 on success. */
int yajl_gen_config(yajl_gen g, yajl_gen_option opt, ...);

yajl_gen_status yajl_gen_map_open(yajl_gen g);
yajl_gen_status yajl_gen_map_close(yajl_gen g);
yajl_gen_status yajl_gen_array_open(yajl_gen g);
yajl_gen_status yajl_gen_array_close(yajl_gen g);

/* In a map, the first call per entry is the KEY: it must be a
 * string (yajl accepts raw bytes; keys serialize JSON-quoted). */
yajl_gen_status yajl_gen_string(yajl_gen g, const unsigned char* str, size_t len);
yajl_gen_status yajl_gen_integer(yajl_gen g, long long n);
yajl_gen_status yajl_gen_double(yajl_gen g, double d);
yajl_gen_status yajl_gen_number(yajl_gen g, const char* s, size_t len);
yajl_gen_status yajl_gen_null(yajl_gen g);
yajl_gen_status yajl_gen_bool(yajl_gen g, int b);

/* The serialized document (owned by the generator; valid until the
 * next generating call or reset). Empty until the ROOT closes. */
const unsigned char* yajl_gen_get_buf(yajl_gen g, size_t* len);
void yajl_gen_clear(yajl_gen g);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_YAJL_COMPAT_H */
