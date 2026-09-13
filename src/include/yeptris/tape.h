/* tape.h — the compact JSON tape (issue #238's consumer shape,
 * TODO.restructure/85: the simdjson-class materialization).
 *
 * One fused strict walk appends ONE 16-byte record per token — no
 * DOM nodes, no links, no views. Numbers convert inline (the number
 * kernel); strings stay zero-copy spans into the caller's buffer;
 * containers carry their matching record index for O(1) skipping in
 * either direction. Parallel columns carve one block (the
 * drain-columns precedent).
 *
 * The tape is a document-level result (never per value across FFI);
 * the buffer the spans borrow must outlive it.
 */
#ifndef YEPTRIS_TAPE_H
#define YEPTRIS_TAPE_H

#include <stddef.h>
#include <stdint.h>

#include <yeptris/api.h>
#include <yeptris/error.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Record kinds (the kinds column). */
enum {
    YEP_T_DOC = 0, /* document boundary (records start after it) */
    YEP_T_NULL,
    YEP_T_BOOL,     /* val: 0/1 */
    YEP_T_INT,      /* val: int64 bits */
    YEP_T_FLOAT,    /* val: double bits */
    YEP_T_STR,      /* off/len: raw span (escapes untouched; the
                       qbc kernel found them) */
    YEP_T_SEQ_OPEN, /* val: index of the matching CLOSE */
    YEP_T_MAP_OPEN, /* val: index of the matching CLOSE; keys are
                       the STR records at even positions inside */
    YEP_T_CLOSE,    /* val: index of the matching OPEN */
};

typedef struct yeptris_json_tape {
    size_t count;
    uint8_t* kinds;  /* yeptris tape kind per record */
    uint32_t* offs;  /* span starts (STR) */
    uint32_t* lens;  /* span lengths (STR) */
    uint64_t* vals;  /* payload by kind (see the enum) */
    int64_t int_min; /* any INT beyond int64? (val holds the double
                        approximation; exact hosts rebuild from the
                        span — the number-kernel contract) */
    void* _block;    /* the carved allocation base */
} yeptris_json_tape;

/* Parses strict JSON into the tape. Accepts scalar roots (a single
 * record). Returns YEPTRIS_OK, YEPTRIS_ERROR_PARSE (details via
 * yeptris_last_error), YEPTRIS_ERROR_MEMORY, YEPTRIS_ERROR_ARG. */
YEPTRIS_API YeptrisStatus yeptris_parse_json_tape(const char* source, size_t len,
                                                  yeptris_json_tape* tape);

YEPTRIS_API void yeptris_tape_free(yeptris_json_tape* tape);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_TAPE_H */
