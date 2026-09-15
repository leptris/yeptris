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

/* Flags. */
#define YEPTRIS_CBOR_STRICT                                                                        \
    0x1u /* decode: reject non-minimal length                                                      \
           arguments and non-text-string map keys                                                  \
           (the deterministic profile's input side) */
#define YEPTRIS_CBOR_CANONICAL                                                                     \
    0x2u /* encode: RFC 8949 s4.2.1 core                                                           \
           deterministic profile — minimal lengths,                                              \
           definite lengths, preferred floats, map                                                 \
           keys sorted bytewise on their encoded                                                   \
           forms (the length-first variant of                                                      \
           s4.2.3 is NOT selected) */

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

/* CBOR Sequences (RFC 8742): top-level data items, concatenated —
 * no framing bytes. Iterates the buffer, one document per item; the
 * callback OWNS each item (free it with yeptris_document_free) and
 * its nonzero return aborts the iteration. Returns the item count
 * delivered so far. An empty sequence is valid (0 items). A truncated
 * or malformed trailing item fails YEPTRIS_ERROR_PARSE with the item
 * index and byte offset on the error channel. opts: decode flags. */
typedef int (*yeptris_cbor_item_cb)(void* ctx, YeptrisDocument item, size_t index);
YEPTRIS_API size_t yeptris_cbor_decode_sequence(const void* buf, size_t len, uint32_t opts,
                                                yeptris_cbor_item_cb cb, void* ctx,
                                                YeptrisStatus* status);

/* Encodes n documents as a CBOR Sequence: the concatenation of the
 * items (opts as yeptris_cbor_encode_into). buf == NULL: the exact
 * byte count. Otherwise writes at most cap and returns the count; a
 * too-small cap writes nothing and returns the need. Returns 0 on a
 * NULL items entry or an unencodable document. */
YEPTRIS_API size_t yeptris_cbor_encode_sequence_into(YeptrisDocument* items, size_t n,
                                                     uint32_t opts, void* buf, size_t cap);

/* Convenience: the sequence in a malloc'd buffer — caller frees. */
YEPTRIS_API void* yeptris_cbor_encode_sequence(YeptrisDocument* items, size_t n, uint32_t opts,
                                               size_t* len);

/* Encodes the document's first root as ONE data item (sequences are a
 * separate surface). buf == NULL: returns the exact byte count needed.
 * Otherwise writes at most cap bytes and returns the count written; a
 * too-small cap writes nothing and returns the needed size. Returns 0
 * on a NULL document or an unencodable one (aliases, non-decimal tag
 * text, non-numeric INT/FLOAT text) — yeptris_last_error() says why.
 *
 * Canonical opts sort map keys (s4.2.1); without it, insertion order.
 * encode(decode(x)) is byte-stable across repeated calls. */
YEPTRIS_API size_t yeptris_cbor_encode_into(YeptrisDocument doc, uint32_t opts, void* buf,
                                            size_t cap);

/* Convenience: encodes into a malloc'd buffer — caller frees with
 * free(). Returns NULL on failure; *len (may be NULL) receives the
 * byte count. */
YEPTRIS_API void* yeptris_cbor_encode(YeptrisDocument doc, uint32_t opts, size_t* len);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_CBOR_H */
