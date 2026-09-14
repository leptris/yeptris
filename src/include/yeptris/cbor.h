/* cbor.h — the CBOR (RFC 8949) public surface (TODO.cbor).
 *
 * CBOR is the JSON data model in binary: decode builds the same DOM
 * yeptris_parse_json builds, so every query, emit, and binding API
 * applies unchanged. The surface compiles only when the library was
 * built with YEPTRIS_WITH_CBOR (the default). */
#ifndef YEPTRIS_CBOR_H
#define YEPTRIS_CBOR_H

#include <stddef.h>

#include <yeptris/api.h>
#include <yeptris/error.h>
#include <yeptris/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Decode flags. */
#define YEPTRIS_CBOR_STRICT 0x1u /* reject non-minimal length arguments and
                                     non-text-string map keys (the deterministic
                                     profile's input side) */

/* Decodes ONE CBOR data item (RFC 8949). buf must remain valid for the
 * lifetime of the returned document (string values are borrowed
 * zero-copy views; indefinite-length chunks concatenate in the
 * document's arena). Trailing bytes after the item are an error —
 * CBOR Sequences (RFC 8742) get their own entry point.
 *
 * Representation notes (full ledger in src/yeptris/cbor/decode.c):
 * integers beyond int64 materialize as shortest-double text; bignums
 * (tags 2/3) stay tag+bytes; byte and text strings are tag-str
 * scalars; undefined and unassigned simple values are diagnostic text
 * ("undefined", "simple(N)"); tag chains ride the node's tag field as
 * a space-separated decimal chain, outermost first.
 *
 * On failure returns NULL; *status (may be NULL) receives the code
 * (YEPTRIS_ERROR_PARSE well-formedness, YEPTRIS_ERROR_ENCODING UTF-8,
 * YEPTRIS_ERROR_DEPTH nesting, YEPTRIS_ERROR_MEMORY) and
 * yeptris_last_error() carries the message with the byte offset.
 *
 * Memory: the caller owns the document; yeptris_document_free releases
 * everything in one call. */
YEPTRIS_API YeptrisDocument yeptris_cbor_decode(const void* buf, size_t len, uint32_t opts,
                                                YeptrisStatus* status);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_CBOR_H */
