/* parse.h — the parse entry points (TODO.impl/07/11 public surface). */

#ifndef YEPTRIS_PARSE_H
#define YEPTRIS_PARSE_H

#include <stddef.h>

#include <yeptris/api.h>
#include <yeptris/error.h>
#include <yeptris/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parses a YAML document set. buf must remain valid for the lifetime of
 * the returned document (scalar values are borrowed zero-copy views).
 * On failure returns NULL; *status (may be NULL) receives the code and
 * yeptris_last_error() carries the message with line/column.
 *
 * Memory: the caller owns the document; free it with
 * yeptris_document_free — one call releases everything. */
YEPTRIS_API YeptrisDocument yeptris_parse(const char* buf, size_t len, YeptrisStatus* status);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_PARSE_H */
