/* tape.h — the compact JSON tape (issue #238's consumer shape,
 * TODO.restructure/85: the simdjson-class materialization).
 *
 * One fused strict walk appends ONE record per token — no DOM nodes,
 * no links, no views. Records are THREE parallel columns (kind byte,
 * off, len) carved from one block: bools encode in the kind, container
 * links ride the off column, and NUMBERS RECORD SPANS ONLY — parse
 * validates the grammar (the lean shape scan) and materialize converts
 * (yeptris_tape_convert; the C-API materializer converts inline).
 * Strings stay zero-copy spans into the caller's buffer.
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
    YEP_T_TRUE,     /* the kind IS the value */
    YEP_T_FALSE,    /* the kind IS the value */
    YEP_T_INT,      /* off/len: signed digit span; convert at materialize */
    YEP_T_FLOAT,    /* off/len: float text span; convert at materialize */
    YEP_T_STR,      /* off/len: raw span (escapes untouched; the
                        qbc kernel found them) */
    YEP_T_SEQ_OPEN, /* off: index of the matching CLOSE */
    YEP_T_MAP_OPEN, /* off: index of the matching CLOSE; keys are
                        the STR records at even positions inside */
    YEP_T_CLOSE,    /* off: index of the matching OPEN */
};

typedef struct yeptris_json_tape {
    size_t count;
    uint8_t* kinds;   /* yeptris tape kind per record */
    uint32_t* offs;   /* span starts (STR/INT/FLOAT); container links */
    uint32_t* lens;   /* span lengths (STR/INT/FLOAT) */
    int64_t int_min;  /* set by yeptris_tape_convert when an INT span
                         exceeds int64 (materialize-time discovery) */
    const void* _src; /* the parsed buffer (spans borrow it; it must
                         outlive the tape — same contract as the spans) */
    size_t _srclen;
    void* _block; /* the carved allocation base */
} yeptris_json_tape;

/* Parses strict JSON into the tape. Accepts scalar roots (a single
 * record). Returns YEPTRIS_OK, YEPTRIS_ERROR_PARSE (details via
 * yeptris_last_error), YEPTRIS_ERROR_MEMORY, YEPTRIS_ERROR_ARG. */
YEPTRIS_API YeptrisStatus yeptris_parse_json_tape(const char* source, size_t len,
                                                  yeptris_json_tape* tape);

YEPTRIS_API void yeptris_tape_free(yeptris_json_tape* tape);

/* Converts the INT/FLOAT records in [from, to): ivals/dvals are
 * parallel to the RECORD indices (slots for other kinds are left
 * untouched; either pointer may be NULL). Returns the number of
 * number records converted, or SIZE_MAX if a span does not re-scan
 * as a number (impossible for a tape this library produced).
 * Sets t->int_min when an INT span exceeds int64. */
YEPTRIS_API size_t yeptris_tape_convert(yeptris_json_tape* t, size_t from, size_t to,
                                        int64_t* ivals, double* dvals);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_TAPE_H */
