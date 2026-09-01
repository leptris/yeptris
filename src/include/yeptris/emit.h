/* emit.h — serialization (TODO.impl/13).
 *
 * One writer engine serves DOM walks and (with 13C) event streams;
 * sizing runs the SAME engine dry, so the size query and the write
 * agree by construction — zero reallocations after the reserve.
 *
 * Roundtrip fidelity: re-serializing a parsed document re-emits its
 * recorded scalar styles (a plain stays plain where safe, quoted stays
 * quoted, block scalars stay block). Byte stability:
 * serialize(parse(serialize(x))) == serialize(x) — gated by the
 * corpus roundtrip test.
 */
#ifndef YEPTRIS_EMIT_H
#define YEPTRIS_EMIT_H

#include <stddef.h>
#include <stdint.h>

#include <yeptris/api.h>
#include <yeptris/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Emission options (13B). Versioned by size: initialize with
 * {sizeof(yeptris_emit_options)} so future fields stay optional. */
typedef struct yeptris_emit_options {
    uint32_t size; /* sizeof(yeptris_emit_options) */
    int canonical; /* fixed form: flow collections, quoted strings,
                    * typed words, shortest floats; a parse of the
                    * output re-serializes byte-identically */
    int reserved;  /* width/folding arrive here (13B continuation) */
} yeptris_emit_options;

/* Options-aware entries; opts == NULL selects the fidelity mode
 * (recorded styles re-emitted). Same contracts as below. */
YEPTRIS_API size_t yeptris_serialize_into_ex(YeptrisDocument doc, const yeptris_emit_options* opts,
                                             char* buf, size_t cap);
YEPTRIS_API char* yeptris_serialize_ex(YeptrisDocument doc, const yeptris_emit_options* opts,
                                       size_t* len);

/* Serializes the document set. buf == NULL: returns the exact byte
 * count needed and writes nothing. Otherwise writes at most cap bytes
 * (NUL-terminated when cap allows) and returns the count written;
 * when cap is too small nothing is written and the needed size is
 * returned. Returns 0 on a NULL document. */
YEPTRIS_API size_t yeptris_serialize_into(YeptrisDocument doc, char* buf, size_t cap);

/* Convenience: serializes into a malloc'd buffer (caller frees).
 * Returns NULL on allocation failure or a NULL document; *len (may be
 * NULL) receives the byte count. The buffer is NUL-terminated. */
YEPTRIS_API char* yeptris_serialize(YeptrisDocument doc, size_t* len);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_EMIT_H */
