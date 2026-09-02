/* yajl_compat.c — the yajl generator over the yeptris core (TODO.impl/21).
 *
 * A state machine over the DOM builder: yajl's call-order rules
 * (maps alternate string-key/value; a closed root accepts no more)
 * enforced here, values build a document, get_buf serializes JSON.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/dom.h"
#include "../../include/yeptris/error.h"
#include "../../include/yeptris/yajl_compat.h"

#include "../doc.h"
#include "../emit/writer.h"

#define YAJL_MAX_STACK 512

typedef struct yajl_gen_t {
    yeptris_document* doc;
    YeptrisNode stack[YAJL_MAX_STACK];
    int depth;
    int key_pending[YAJL_MAX_STACK]; /* per open map: string key next */
    int root_closed;
    YeptrisNode pending_key; /* map: key placed, awaiting its value */
    unsigned char* buf;      /* get_buf cache: freed on next gen call */
    size_t buf_len;
    int beautify;
    char numbuf[48];
} yajl_gen_t;

yajl_gen yajl_gen_alloc(const yajl_alloc_funcs* afs) {
    (void)afs; /* the system allocator (yajl's NULL default) */
    yajl_gen g = calloc(1, sizeof(*g));
    if (g == NULL) {
        return NULL;
    }
    g->doc = yeptris_document_new();
    if (g->doc == NULL) {
        free(g);
        return NULL;
    }
    return g;
}

void yajl_gen_free(yajl_gen g) {
    if (g == NULL) {
        return;
    }
    free(g->buf);
    yeptris_document_free(g->doc);
    free(g);
}

void yajl_gen_reset(yajl_gen g, const char* sep) {
    if (g == NULL) {
        return;
    }
    (void)sep; /* yajl emits the separator between streams; the buf
                * clears on the next generation anyway */
    free(g->buf);
    g->buf = NULL;
    g->buf_len = 0;
    yeptris_document_free(g->doc);
    g->doc = yeptris_document_new();
    g->depth = 0;
    g->root_closed = 0;
    g->pending_key = NULL;
    memset(g->key_pending, 0, sizeof(g->key_pending));
}

int yajl_gen_config(yajl_gen g, yajl_gen_option opt, ...) {
    if (g == NULL) {
        return 0;
    }
    va_list ap;
    va_start(ap, opt);
    int rc = 0;
    switch (opt) {
    case yajl_gen_beautify:
        g->beautify = va_arg(ap, int) != 0;
        rc = 1;
        break;
    case yajl_gen_indent_string:
    case yajl_gen_validate_utf8:
        rc = 1; /* accepted: fixed 2-space indent; UTF-8 always on */
        break;
    }
    va_end(ap);
    return rc;
}

/* Places a completed value under the innermost open container (or
 * closes the root). yajl rule: inside a map, the value slot must be
 * preceded by its string key (key_pending). */
static yajl_gen_status place(yajl_gen g, YeptrisNode value) {
    if (g->root_closed) {
        return yajl_gen_generation_complete;
    }
    if (g->depth > 0 && yeptris_node_kind(g->stack[g->depth - 1]) == YEPTRIS_NODE_MAPPING &&
        g->key_pending[g->depth - 1] && yeptris_node_style(value) != YEPTRIS_STYLE_DOUBLE_QUOTED) {
        return yajl_gen_keys_must_be_strings; /* yajl's map-key rule */
    }
    if (g->depth == 0) {
        if (g->root_closed) {
            return yajl_gen_generation_complete;
        }
        if (yeptris_document_set_root(g->doc, value) != YEPTRIS_OK) {
            return yajl_gen_in_error_state;
        }
        /* a root SCALAR completes generation; an open container
         * continues (it closes later at depth 0) */
        g->root_closed = 1;
        return yajl_gen_status_ok;
    }
    YeptrisNode top = g->stack[g->depth - 1];
    if (yeptris_node_kind(top) == YEPTRIS_NODE_MAPPING) {
        if (g->key_pending[g->depth - 1]) {
            /* this placement IS the key: remember it for the value
             * call (keys are strings per yajl's rule) */
            g->pending_key = value;
            g->key_pending[g->depth - 1] = 0;
        } else {
            if (g->pending_key == NULL) {
                return yajl_gen_in_error_state;
            }
            if (yeptris_node_map_add_node(top, g->pending_key, value) != YEPTRIS_OK) {
                return yajl_gen_in_error_state;
            }
            g->pending_key = NULL;
            g->key_pending[g->depth - 1] = 1;
        }
        return yajl_gen_status_ok;
    }
    if (yeptris_node_seq_add(top, value) != YEPTRIS_OK) {
        return yajl_gen_in_error_state;
    }
    return yajl_gen_status_ok;
}

yajl_gen_status yajl_gen_map_open(yajl_gen g) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    if (g->root_closed) {
        return yajl_gen_generation_complete;
    }
    if (g->depth >= YAJL_MAX_STACK) {
        return yajl_max_depth_exceeded;
    }
    YeptrisNode m = yeptris_node_new_mapping(g->doc);
    if (m == NULL) {
        return yajl_gen_in_error_state;
    }
    yajl_gen_status st = place(g, m);
    if (st != yajl_gen_status_ok) {
        return st;
    }
    g->root_closed = 0; /* an open root container keeps generating */
    g->stack[g->depth] = m;
    g->key_pending[g->depth] = 1;
    g->depth++;
    return yajl_gen_status_ok;
}

yajl_gen_status yajl_gen_map_close(yajl_gen g) {
    if (g == NULL || g->depth == 0 ||
        yeptris_node_kind(g->stack[g->depth - 1]) != YEPTRIS_NODE_MAPPING) {
        return yajl_gen_in_error_state;
    }
    if (!g->key_pending[g->depth - 1] && g->pending_key != NULL) {
        return yajl_gen_keys_must_be_strings; /* an incomplete pair */
    }
    g->depth--;
    if (g->depth == 0) {
        g->root_closed = 1; /* the root container finished */
    }
    return yajl_gen_status_ok;
}

yajl_gen_status yajl_gen_array_open(yajl_gen g) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    if (g->root_closed) {
        return yajl_gen_generation_complete;
    }
    if (g->depth >= YAJL_MAX_STACK) {
        return yajl_max_depth_exceeded;
    }
    YeptrisNode sq = yeptris_node_new_sequence(g->doc);
    if (sq == NULL) {
        return yajl_gen_in_error_state;
    }
    yajl_gen_status st = place(g, sq);
    if (st != yajl_gen_status_ok) {
        return st;
    }
    g->root_closed = 0;
    g->stack[g->depth] = sq;
    g->key_pending[g->depth] = 0;
    g->depth++;
    return yajl_gen_status_ok;
}

yajl_gen_status yajl_gen_array_close(yajl_gen g) {
    if (g == NULL || g->depth == 0 ||
        yeptris_node_kind(g->stack[g->depth - 1]) != YEPTRIS_NODE_SEQUENCE) {
        return yajl_gen_in_error_state;
    }
    g->depth--;
    if (g->depth == 0) {
        g->root_closed = 1;
    }
    return yajl_gen_status_ok;
}

yajl_gen_status yajl_gen_string(yajl_gen g, const unsigned char* str, size_t len) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    if (g->root_closed) {
        return yajl_gen_generation_complete;
    }
    if (g->depth > 0 && yeptris_node_kind(g->stack[g->depth - 1]) == YEPTRIS_NODE_MAPPING &&
        !g->key_pending[g->depth - 1]) {
        /* value slot: a JSON string */
        YeptrisNode n =
            yeptris_node_new_scalar(g->doc, (const char*)str, len, YEPTRIS_STYLE_DOUBLE_QUOTED);
        if (n == NULL) {
            return yajl_gen_in_error_state;
        }
        return place(g, n);
    }
    /* key slot (or root/seq): keys build as plain scalar nodes whose
     * JSON serialization quotes them; yajl requires map keys to be
     * strings, and gen_string is the only legal key call */
    YeptrisNode n =
        yeptris_node_new_scalar(g->doc, (const char*)str, len, YEPTRIS_STYLE_DOUBLE_QUOTED);
    if (n == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, n);
}

yajl_gen_status yajl_gen_integer(yajl_gen g, long long n) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    snprintf(g->numbuf, sizeof(g->numbuf), "%lld", n);
    YeptrisNode v =
        yeptris_node_new_scalar(g->doc, g->numbuf, strlen(g->numbuf), YEPTRIS_STYLE_PLAIN);
    if (v == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, v);
}

yajl_gen_status yajl_gen_double(yajl_gen g, double d) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    snprintf(g->numbuf, sizeof(g->numbuf), "%.17g", d);
    YeptrisNode v =
        yeptris_node_new_scalar(g->doc, g->numbuf, strlen(g->numbuf), YEPTRIS_STYLE_PLAIN);
    if (v == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, v);
}

yajl_gen_status yajl_gen_number(yajl_gen g, const char* s, size_t len) {
    if (g == NULL || s == NULL) {
        return yajl_gen_in_error_state;
    }
    YeptrisNode v = yeptris_node_new_scalar(g->doc, s, len, YEPTRIS_STYLE_PLAIN);
    if (v == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, v);
}

yajl_gen_status yajl_gen_null(yajl_gen g) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    YeptrisNode v = yeptris_node_new_scalar(g->doc, "null", 4, YEPTRIS_STYLE_PLAIN);
    if (v == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, v);
}

yajl_gen_status yajl_gen_bool(yajl_gen g, int b) {
    if (g == NULL) {
        return yajl_gen_in_error_state;
    }
    YeptrisNode v =
        yeptris_node_new_scalar(g->doc, b ? "true" : "false", b ? 4 : 5, YEPTRIS_STYLE_PLAIN);
    if (v == NULL) {
        return yajl_gen_in_error_state;
    }
    return place(g, v);
}

const unsigned char* yajl_gen_get_buf(yajl_gen g, size_t* len) {
    if (g == NULL || !g->root_closed) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    if (g->buf == NULL) {
        size_t n = 0;
        char* out = g->beautify ? yep_serialize_json_pretty((const yeptris_document*)g->doc, &n)
                                : yep_serialize_json_compact((const yeptris_document*)g->doc, &n);
        if (out == NULL) {
            if (len != NULL) {
                *len = 0;
            }
            return NULL;
        }
        if (n > 0 && out[n - 1] == '\n') {
            out[n - 1] = '\0'; /* yajl output carries no newline */
            n--;
        }
        g->buf = (unsigned char*)out;
        g->buf_len = n;
    }
    if (len != NULL) {
        *len = g->buf_len;
    }
    return g->buf;
}

void yajl_gen_clear(yajl_gen g) {
    if (g == NULL) {
        return;
    }
    yajl_gen_reset(g, NULL);
}
