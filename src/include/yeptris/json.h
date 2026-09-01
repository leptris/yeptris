/* json.h — strict JSON mode (TODO.impl/08C/21).
 *
 * YAML 1.2 strictly contains JSON, but users of json-c-shaped APIs
 * want RFC 8259 semantics exactly: this entry validates the whole
 * input against the strict grammar (scan/json.c is the one truth)
 * before the engine runs, so an accepted input is guaranteed to be
 * pure JSON and every invalid one fails with the violation offset.
 * Charset note: JSON strings allow DEL and forbid raw C0 — the
 * opposite corner of YAML c-printable — so this path validates
 * UTF-8 well-formedness only, never the printable set.
 */
#ifndef YEPTRIS_JSON_H
#define YEPTRIS_JSON_H

#include <stddef.h>

#include <yeptris/api.h>
#include <yeptris/parse.h>
#include <yeptris/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Parses one strict-JSON document. Same contract as yeptris_parse:
 * a YeptrisDocument handle (queries via the DOM API) or NULL with
 * *st set; violations carry the byte offset via yeptris_last_error. */
YEPTRIS_API YeptrisDocument yeptris_parse_json(const char* data, size_t len, YeptrisStatus* st);

/* Serializes the document's FIRST document as strict JSON (flow
 * collections, JSON escapes, shortest floats, null/true/false words).
 * Returns a malloc'd NUL-terminated buffer (caller frees) with *len
 * (may be NULL) set, or NULL on a NULL document / allocation
 * failure / zero documents. */
YEPTRIS_API char* yeptris_serialize_json(YeptrisDocument doc, size_t* len);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_JSON_H */
