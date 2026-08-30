/* error.h — public status codes.
 *
 * Public entry points write only these codes (the internal error channel —
 * thread-local + per-document messages with line/column — lands with
 * TODO.impl/02). Enum values are ABI: bindings hard-code them and the ABI
 * pinning test (test/unit/test_abi.cpp) fails if one shifts without a major
 * version bump.
 */
#ifndef YEPTRIS_ERROR_H
#define YEPTRIS_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YEPTRIS_OK = 0,                /* success */
    YEPTRIS_ERROR_PARSE = 1,       /* malformed YAML (details + line/col via the error channel) */
    YEPTRIS_ERROR_MEMORY = 2,      /* allocation failure; document remains freeable */
    YEPTRIS_ERROR_DEPTH = 3,       /* nesting exceeds max_depth (default 1000) */
    YEPTRIS_ERROR_ENCODING = 4,    /* invalid input encoding / ill-formed UTF-8 */
    YEPTRIS_ERROR_IO = 5,          /* caller-supplied I/O failure */
    YEPTRIS_ERROR_ARG = 6,         /* invalid argument (NULL handle, bad option) */
    YEPTRIS_ERROR_UNSUPPORTED = 7, /* requested feature not built in */
    YEPTRIS_ERROR_INTERNAL = 8,    /* invariant violation — a bug, please report */
} YeptrisStatus;

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_ERROR_H */
