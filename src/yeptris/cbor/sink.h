/* sink.h — the CBOR decode seam (#157).
 *
 * The grammar loop (decode.c's cbor_item machinery) is the single
 * source of CBOR semantics; a sink implements the item constructors
 * on top of it. Two sinks exist: the DOM builder (the default, the
 * yeptris_cbor_decode public path) and the native ext's VALUE builder
 * (one pass, no intermediate). Sinks receive the resolver's tag_id
 * and the pending semantic-tag chain in its text form ("N N N",
 * outermost first — NULL when the item carries no chain).
 */
#ifndef YEP_CBOR_SINK_H
#define YEP_CBOR_SINK_H

#include <stddef.h>
#include <stdint.h>

#include <yeptris/api.h> /* YEPTRIS_API (the ext links these) */

typedef struct yep_cbor_sink {
    void* ctx;
    /* All callbacks return 0 on sink failure (the decoder reports
     * MEMORY). They may raise through host languages (longjmp): the
     * decoder owns no host-managed state across them except the
     * indefinite buffer, which its own exit paths free. */
    /* is_key: 1 = the item lands in the enclosing map's key slot
     * (sinks key-intern strings; the DOM ignores it). Rendered text:
     * numbers-as-text, false/true/null/undefined, simple(N) — the
     * exact bytes the DOM path stores. */
    int (*text)(void* ctx, const char* s, uint32_t n, uint8_t tag_id, int is_key, const char* tag,
                uint32_t tag_len);
    /* Known integers: value = negative ? -(mag) : mag, mag <= 2^63. */
    int (*int_val)(void* ctx, int negative, uint64_t mag, int is_key, const char* tag,
                   uint32_t tag_len);
    int (*float_val)(void* ctx, double dv, int is_key, const char* tag, uint32_t tag_len);
    /* Strings. borrowed: 1 = p is the input (a sink may record the
     * offset), 0 = the decoder's scratch buffer (copy or take it). */
    int (*str_val)(void* ctx, const unsigned char* p, uint32_t n, int borrowed, int is_key,
                   const char* tag, uint32_t tag_len);
    int (*bytes_val)(void* ctx, const unsigned char* p, uint32_t n, int borrowed, int is_key,
                     const char* tag, uint32_t tag_len);
    /* Containers. cap: expected item count (maps: pairs*2), UINT64_MAX
     * for indefinite. close fires for BOTH the definite-complete and
     * the break paths, after the last child. */
    int (*open)(void* ctx, int is_map, uint64_t cap, const char* tag, uint32_t tag_len);
    int (*close)(void* ctx, int is_map);
} yep_cbor_sink;

/* The generic engine: the grammar, driving `sink`. Same reject/
 * encoding/depth semantics as the DOM entry. */
YEPTRIS_API int yep_cbor_decode_gen(const unsigned char* p, size_t len, int strict,
                                    size_t* consumed, const yep_cbor_sink* sink);

/* The DOM sink: pairs yep_cbor_decode_gen with a yep_dom* (the
 * yep_cbor_decode_dom public path's driver; d is a yep_dom*). */
YEPTRIS_API int yep_cbor_decode_dom_sink(void* d, const unsigned char* p, size_t len, int strict,
                                         size_t* consumed);

#endif /* YEP_CBOR_SINK_H */
