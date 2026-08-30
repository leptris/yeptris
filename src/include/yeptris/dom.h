/* document.h / node.h surface — DOM accessors (TODO.impl/11).
 * Pinned enums: bindings hard-code these values (test_abi). */

#ifndef YEPTRIS_DOM_PUBLIC_H
#define YEPTRIS_DOM_PUBLIC_H

#include <stddef.h>
#include <stdint.h>

#include <yeptris/api.h>
#include <yeptris/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    YEPTRIS_NODE_SCALAR = 0,
    YEPTRIS_NODE_SEQUENCE = 1,
    YEPTRIS_NODE_MAPPING = 2,
    YEPTRIS_NODE_ALIAS = 3,
} YeptrisNodeKind;

typedef enum {
    YEPTRIS_STYLE_ANY = 0,
    YEPTRIS_STYLE_PLAIN = 1,
    YEPTRIS_STYLE_SINGLE_QUOTED = 2,
    YEPTRIS_STYLE_DOUBLE_QUOTED = 3,
    YEPTRIS_STYLE_LITERAL = 4,
    YEPTRIS_STYLE_FOLDED = 5,
} YeptrisScalarStyle;

/* Releases the document and everything reachable from it (one call).
 * Handles are invalid afterwards. NULL is accepted. */
YEPTRIS_API void yeptris_document_free(YeptrisDocument doc);

/* Number of documents in the parsed stream. */
YEPTRIS_API size_t yeptris_document_count(YeptrisDocument doc);

/* Root node of document i (0-based); NULL when out of range. */
YEPTRIS_API YeptrisNode yeptris_document_root(YeptrisDocument doc, size_t index);

YEPTRIS_API YeptrisNodeKind yeptris_node_kind(YeptrisNode node);

/* Scalar content / alias name. Returns NULL for collections.
 * Memory: borrowed from the input (or document-owned when the value was
 * folded/escaped) — valid until yeptris_document_free. */
YEPTRIS_API const char* yeptris_node_value(YeptrisNode node, size_t* len);

YEPTRIS_API YeptrisScalarStyle yeptris_node_style(YeptrisNode node);

/* Node properties; empty (NULL, 0) when absent. */
YEPTRIS_API const char* yeptris_node_tag(YeptrisNode node, size_t* len);
YEPTRIS_API const char* yeptris_node_anchor(YeptrisNode node, size_t* len);

/* Alias target node; NULL for non-aliases. */
YEPTRIS_API YeptrisNode yeptris_node_alias_target(YeptrisNode node);

/* Sequence entry count / entry i. */
YEPTRIS_API size_t yeptris_node_seq_count(YeptrisNode node);
YEPTRIS_API YeptrisNode yeptris_node_seq_at(YeptrisNode node, size_t index);

/* Mapping pair count / value for a key (linear; interning lands with the
 * full DOM item). The first matching key wins. */
YEPTRIS_API size_t yeptris_node_map_count(YeptrisNode node);
YEPTRIS_API YeptrisNode yeptris_node_map_get(YeptrisNode node, const char* key, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* YEPTRIS_DOM_PUBLIC_H */
