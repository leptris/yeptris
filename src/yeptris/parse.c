/* parse.c — public API glue: encoding front-end → engine → DOM. */

#include <string.h>

#include "doc.h"
#include "dom/dom.h"
#include "encoding/encoding.h"
#include "memory/allocator.h"
#include "memory/pool.h"
#include "parse/engine.h"
#include "resolve/resolver.h"

#include "parse/events.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#include <yeptris.h>

/* Node handle: a (document, node-id) pair so nodes stay usable even if
 * the node pool grows (ids are stable; pointers are not). */
typedef struct yeptris_node {
    yeptris_document* doc;
    uint32_t id;
} yeptris_node;

YEPTRIS_API const char* yeptris_last_error(uint32_t* line, uint32_t* col) {
    const yep_error* tls = yep_error_tls();
    if (line != NULL) {
        *line = tls->line;
    }
    if (col != NULL) {
        *col = tls->col;
    }
    return tls->msg;
}

YEPTRIS_API YeptrisDocument yeptris_parse(const char* buf, size_t len, YeptrisStatus* status) {
    return yeptris_parse_ex(buf, len, NULL, status);
}

YEPTRIS_API YeptrisDocument yeptris_parse_ex(const char* buf, size_t len,
                                             const YeptrisParseOptions* opts,
                                             YeptrisStatus* status) {
    YeptrisStatus st = YEPTRIS_OK;
    if (buf == NULL && len != 0) {
        st = YEPTRIS_ERROR_ARG;
        goto fail;
    }

    const yep_allocator* sys = yep_system_allocator();

    /* Encoding front-end: BOM sniff; borrow UTF-8, transcode the rest. */
    yep_encoding enc = YEP_ENC_UNKNOWN;
    size_t bom = yep_bom_sniff((const unsigned char*)buf, len, &enc);
    const char* data = buf + bom;
    size_t data_len = len - bom;

    unsigned char* transcoded = NULL;
    size_t transcoded_len = 0;
    if (enc == YEP_ENC_UTF16LE || enc == YEP_ENC_UTF16BE || enc == YEP_ENC_UTF32LE ||
        enc == YEP_ENC_UTF32BE) {
        size_t terr = 0;
        int rc = yep_transcode_to_utf8(sys, enc, (const unsigned char*)data, data_len, &transcoded,
                                       &transcoded_len, &terr);
        if (rc != 0) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, terr,
                          "ill-formed input encoding");
            yep_free(sys, transcoded);
            st = YEPTRIS_ERROR_ENCODING;
            goto fail;
        }
        data = (const char*)transcoded;
        data_len = transcoded_len;
        size_t pverr = 0;
        if (!yep_printable_validate((const unsigned char*)data, data_len, &pverr)) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, pverr,
                          "non-printable character at byte %zu", pverr);
            yep_free(sys, transcoded);
            st = YEPTRIS_ERROR_ENCODING;
            goto fail;
        }
    } else {
        size_t verr = 0;
        if (!yep_printable_validate((const unsigned char*)data, data_len, &verr)) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, verr,
                          "ill-formed or non-printable UTF-8 at byte %zu", verr);
            st = YEPTRIS_ERROR_ENCODING;
            goto fail;
        }
    }

    /* Engine → DOM. */
    yep_engine* eng = yep_engine_create(sys);
    if (opts != NULL) {
        yep_engine_set_resolver(eng, opts->schema == YEPTRIS_SCHEMA_11_COMPAT
                                         ? yep_resolver_compat11()
                                         : yep_resolver_core12());
        if (opts->max_depth > 0) {
            yep_engine_set_max_depth(eng, opts->max_depth);
        }
    }
    if (eng == NULL) {
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }
    yep_dom* dom = yep_dom_create(sys);
    if (dom == NULL) {
        yep_engine_destroy(eng);
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }

    yep_sink sink = {yep_dom_on_event, dom};
    int rc = yep_engine_run(eng, data, data_len, &sink);
    if (rc != 0) {
        const yep_error* ee = yep_engine_error(eng);
        yep_err_code code = ee ? ee->code : YEP_ERR_UNEXPECTED;
        if (ee) {
            yep_error* tls = yep_error_tls();
            *tls = *ee;
        }
        yep_dom_destroy(dom);
        yep_engine_destroy(eng);
        yep_free(sys, transcoded);
        st = (code == YEP_ERR_MEMORY)  ? YEPTRIS_ERROR_MEMORY
             : (code == YEP_ERR_DEPTH) ? YEPTRIS_ERROR_DEPTH
                                       : YEPTRIS_ERROR_PARSE;
        goto fail;
    }
    /* Resolved tags, folded and escaped scalars live in the engine's
     * finish pool; the document must own it or every such string would
     * dangle at engine teardown (found by ASAN). */
    yep_pool* finish = yep_engine_detach_pool(eng);
    yep_engine_destroy(eng);

    /* Empty stream (no documents): NULL document with YEPTRIS_OK. */
    if (dom->dcount == 0) {
        yep_dom_destroy(dom);
        yep_pool_destroy(finish);
        yep_free(sys, transcoded);
        if (status != NULL) {
            *status = YEPTRIS_OK;
        }
        return NULL;
    }

    yeptris_document* doc = yep_alloc(sys, sizeof(yeptris_document));
    if (doc == NULL) {
        yep_dom_destroy(dom);
        yep_pool_destroy(finish);
        yep_free(sys, transcoded);
        st = YEPTRIS_ERROR_MEMORY;
        goto fail;
    }
    doc->dom = dom;
    doc->sys = sys;
    doc->transcoded = transcoded;
    doc->transcoded_len = transcoded_len;
    doc->input = buf;
    doc->finish_pool = finish;
    return (YeptrisDocument)doc;

fail:
    if (status != NULL) {
        *status = st;
    }
    return NULL;
}

YEPTRIS_API void yeptris_document_free(YeptrisDocument handle) {
    yeptris_document* doc = (yeptris_document*)handle;
    if (doc == NULL) {
        return;
    }
    yep_dom_destroy(doc->dom);
    yep_pool_destroy((yep_pool*)doc->finish_pool);
    yep_free(doc->sys, doc->transcoded);
    yep_free(doc->sys, doc);
}

YEPTRIS_API size_t yeptris_document_count(YeptrisDocument handle) {
    yeptris_document* doc = (yeptris_document*)handle;
    return doc ? doc->dom->dcount : 0;
}

static YeptrisNode node_new_handle(yeptris_document* doc, uint32_t id) {
    yeptris_node* n = yep_pool_alloc(doc->dom->pool, sizeof(yeptris_node), 16);
    if (n == NULL) {
        return NULL;
    }
    n->doc = doc;
    n->id = id;
    return (YeptrisNode)n;
}

YEPTRIS_API YeptrisNode yeptris_document_root(YeptrisDocument handle, size_t index) {
    yeptris_document* doc = (yeptris_document*)handle;
    if (doc == NULL || index >= doc->dom->dcount) {
        return NULL;
    }
    return node_new_handle(doc, doc->dom->docs[index]);
}

static const yep_dnode* node_of(YeptrisNode handle) {
    yeptris_node* n = (yeptris_node*)handle;
    if (n == NULL || n->doc == NULL) {
        return NULL;
    }
    return yep_dom_node(n->doc->dom, n->id);
}

YEPTRIS_API YeptrisNodeKind yeptris_node_kind(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL) {
        return YEPTRIS_NODE_SCALAR;
    }
    switch (n->kind) {
    case YEP_DOM_SEQUENCE:
        return YEPTRIS_NODE_SEQUENCE;
    case YEP_DOM_MAPPING:
        return YEPTRIS_NODE_MAPPING;
    case YEP_DOM_ALIAS:
        return YEPTRIS_NODE_ALIAS;
    default:
        return YEPTRIS_NODE_SCALAR;
    }
}

static const char* view_out(yep_view v, size_t* len) {
    if (len != NULL) {
        *len = v.len;
    }
    return v.len ? v.p : (v.p ? v.p : "");
}

YEPTRIS_API const char* yeptris_node_value(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || (n->kind != YEP_DOM_SCALAR && n->kind != YEP_DOM_ALIAS)) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    return view_out(n->value, len);
}

YEPTRIS_API YeptrisScalarStyle yeptris_node_style(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return n ? (YeptrisScalarStyle)n->style : YEPTRIS_STYLE_ANY;
}

YEPTRIS_API const char* yeptris_tag_uri(YeptrisTagId id) {
    return yep_tag_uri((yep_tag_id)id);
}

YEPTRIS_API YeptrisTagId yeptris_node_tag_id(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL) {
        return YEPTRIS_TAG_CUSTOM;
    }
    if (n->kind == YEPTRIS_NODE_SEQUENCE) {
        return YEPTRIS_TAG_SEQ;
    }
    if (n->kind == YEPTRIS_NODE_MAPPING) {
        return YEPTRIS_TAG_MAP;
    }
    return (YeptrisTagId)n->tag_id;
}

/* strips '_' and ',' into dst (nul-terminated); returns length */
static size_t clean_num(const yep_dnode* n, char* dst, size_t cap) {
    size_t o = 0;
    for (uint32_t i = 0; i < n->value.len && o + 1 < cap; i++) {
        char c = ((const char*)n->value.p)[i];
        if (c == '_' || c == ',') {
            continue;
        }
        dst[o++] = c;
    }
    dst[o] = '\0';
    return o;
}

YEPTRIS_API YeptrisStatus yeptris_node_int(YeptrisNode handle, int64_t* out) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || out == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    if (n->tag_id != YEPTRIS_TAG_INT) {
        return YEPTRIS_ERROR_PARSE;
    }
    char buf[80];
    size_t len = clean_num(n, buf, sizeof(buf));
    if (len == 0) {
        return YEPTRIS_ERROR_PARSE;
    }
    int base = 10;
    const char* s = buf;
    if (buf[0] == '-' || buf[0] == '+') {
        s++;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
    } else if (s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        base = 2;
    } else if (s[0] == '0' && s[1] == 'o') {
        memmove(buf + (size_t)(s - buf) + 1, s + 2, len - (size_t)(s - buf) - 1);
        len -= 1;
        buf[len] = '\0';
        base = 8; /* 0o17 -> 017 octal */
    } else if (s[0] == '0' && s[1] != '\0' && s[1] != '.') {
        base = 8; /* compat leading-0 octal */
    }
    if (memchr(buf, ':', len) != NULL) {
        /* compat sexagesimal: [-+]?d+(:dd){1,2} */
        long long sign = 1;
        const char* p = buf;
        if (p[0] == '-') {
            sign = -1;
            p++;
        } else if (p[0] == '+') {
            p++;
        }
        long long v = 0;
        int groups = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p - '0');
            p++;
        }
        while (*p == ':' && groups < 2) {
            p++;
            long long g = 0;
            int d = 0;
            while (*p >= '0' && *p <= '9') {
                g = g * 10 + (*p - '0');
                p++;
                d++;
            }
            if (d == 0) {
                return YEPTRIS_ERROR_PARSE;
            }
            v = v * 60 + g;
            groups++;
        }
        if (groups == 0 || *p != '\0') {
            return YEPTRIS_ERROR_PARSE;
        }
        *out = sign * v;
        return YEPTRIS_OK;
    }
    char* end = NULL;
    errno = 0;
    long long v = strtoll(buf, &end, base);
    if (end == buf || *end != '\0' || errno == ERANGE) {
        return YEPTRIS_ERROR_PARSE;
    }
    *out = (int64_t)v;
    return YEPTRIS_OK;
}

YEPTRIS_API YeptrisStatus yeptris_node_float(YeptrisNode handle, double* out) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || out == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    if (n->tag_id != YEPTRIS_TAG_FLOAT) {
        return YEPTRIS_ERROR_PARSE;
    }
    char buf[80];
    size_t len = clean_num(n, buf, sizeof(buf));
    if (len == 0) {
        return YEPTRIS_ERROR_PARSE;
    }
    /* .inf / .nan family (sign allowed on inf) */
    {
        const char* s = buf;
        size_t sl = len;
        if (s[0] == '-' || s[0] == '+') {
            s++;
            sl--;
        }
        if (sl == 4 && s[0] == '.') {
            if ((s[1] == 'i' || s[1] == 'I') && (s[2] == 'n' || s[2] == 'N') &&
                (s[3] == 'f' || s[3] == 'F')) {
                *out = (buf[0] == '-') ? -INFINITY : INFINITY;
                return YEPTRIS_OK;
            }
            if ((s[1] == 'n' || s[1] == 'N') && (s[2] == 'a' || s[2] == 'A') &&
                (s[3] == 'n' || s[3] == 'N')) {
                *out = NAN;
                return YEPTRIS_OK;
            }
        }
    }
    /* sexagesimal: [-+]?d+(:dd){1,2}(.d*)? */
    if (memchr(buf, ':', len) != NULL) {
        double sign = 1.0;
        const char* s = buf;
        if (s[0] == '-') {
            sign = -1.0;
            s++;
        } else if (s[0] == '+') {
            s++;
        }
        double v = 0.0;
        const char* p = s;
        int groups = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10.0 + (*p - '0');
            p++;
        }
        while (*p == ':' && groups < 2) {
            p++;
            double g = 0.0;
            int d = 0;
            while (*p >= '0' && *p <= '9') {
                g = g * 10.0 + (*p - '0');
                p++;
                d++;
            }
            if (d == 0) {
                return YEPTRIS_ERROR_PARSE;
            }
            v = v * 60.0 + g;
            groups++;
        }
        if (groups == 0 || (*p != '\0' && *p != '.')) {
            return YEPTRIS_ERROR_PARSE;
        }
        if (*p == '.') {
            double frac = 0.0, scale = 0.1;
            p++;
            while (*p >= '0' && *p <= '9') {
                frac += (*p - '0') * scale;
                scale /= 10.0;
                p++;
            }
            v += frac;
        }
        if (*p != '\0') {
            return YEPTRIS_ERROR_PARSE;
        }
        *out = sign * v;
        return YEPTRIS_OK;
    }
    char* end = NULL;
    errno = 0;
    double v = strtod(buf, &end);
    if (end == buf || *end != '\0') {
        return YEPTRIS_ERROR_PARSE;
    }
    (void)errno;
    *out = v;
    return YEPTRIS_OK;
}

YEPTRIS_API YeptrisStatus yeptris_node_bool(YeptrisNode handle, int* out) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || out == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    if (n->tag_id != YEPTRIS_TAG_BOOL) {
        return YEPTRIS_ERROR_PARSE;
    }
    /* the resolver accepted it; true-set membership decides the value */
    static const char* k_true[] = {"true", "True", "TRUE", "y",  "Y", "yes",
                                   "Yes",  "YES",  "on",   "On", "ON"};
    for (size_t i = 0; i < sizeof(k_true) / sizeof(k_true[0]); i++) {
        size_t m = strlen(k_true[i]);
        if (n->value.len == m && memcmp(n->value.p, k_true[i], m) == 0) {
            *out = 1;
            return YEPTRIS_OK;
        }
    }
    *out = 0;
    return YEPTRIS_OK;
}

YEPTRIS_API const char* yeptris_node_tag(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->tag.len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return n ? NULL : NULL;
    }
    return view_out(n->tag, len);
}

YEPTRIS_API const char* yeptris_node_anchor(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->anchor.len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    return view_out(n->anchor, len);
}

static YeptrisNode wrap(yeptris_node* base, uint32_t id) {
    if (base == NULL || base->doc == NULL) {
        return NULL;
    }
    /* Handles live in the document's pool: one document_free reclaims
     * every handle ever handed out (zero-leak contract). */
    yeptris_node* n = yep_pool_alloc(base->doc->dom->pool, sizeof(yeptris_node), 16);
    if (n == NULL) {
        return NULL;
    }
    n->doc = base->doc;
    n->id = id;
    return (YeptrisNode)n;
}

YEPTRIS_API YeptrisNode yeptris_node_alias_target(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_ALIAS || n->target == UINT32_MAX) {
        return NULL;
    }
    return wrap((yeptris_node*)handle, n->target);
}

YEPTRIS_API size_t yeptris_node_seq_count(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return (n && n->kind == YEP_DOM_SEQUENCE) ? n->count : 0;
}

YEPTRIS_API YeptrisNode yeptris_node_seq_at(YeptrisNode handle, size_t index) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_SEQUENCE || index >= n->count) {
        return NULL;
    }
    const yep_dom* dom = ((yeptris_node*)handle)->doc->dom;
    uint32_t id = n->first_child;
    for (size_t i = 0; i < index && id != UINT32_MAX; i++) {
        const yep_dnode* cur = yep_dom_node(dom, id);
        id = cur ? cur->next_sibling : UINT32_MAX;
    }
    return wrap((yeptris_node*)handle, id);
}

YEPTRIS_API size_t yeptris_node_map_count(YeptrisNode handle) {
    const yep_dnode* n = node_of(handle);
    return (n && n->kind == YEP_DOM_MAPPING) ? n->count / 2 : 0;
}

YEPTRIS_API int yeptris_node_map_at(YeptrisNode handle, size_t index, YeptrisNode* key,
                                    YeptrisNode* value) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEPTRIS_NODE_MAPPING || index >= n->count / 2) {
        return -1;
    }
    yeptris_node* h = (yeptris_node*)handle;
    const yep_dom* d = h->doc->dom;
    uint32_t child = n->first_child;
    for (size_t i = 0; i < index * 2; i++) {
        child = d->nodes[child].next_sibling; /* pairs are key,value,… */
    }
    if (key != NULL) {
        *key = wrap(h, child);
    }
    if (value != NULL) {
        *value = wrap(h, d->nodes[child].next_sibling);
    }
    return 0;
}

YEPTRIS_API YeptrisNode yeptris_node_map_get(YeptrisNode handle, const char* key, size_t key_len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->kind != YEP_DOM_MAPPING || key == NULL) {
        return NULL;
    }
    const yep_dom* dom = ((yeptris_node*)handle)->doc->dom;
    uint32_t id = n->first_child;
    int want_key = 1;
    while (id != UINT32_MAX) {
        const yep_dnode* cur = yep_dom_node(dom, id);
        if (cur == NULL) {
            break;
        }
        if (want_key && cur->kind == YEP_DOM_SCALAR) {
            yep_view kv = {key, (uint32_t)key_len};
            if (yep_view_eq(cur->value, kv)) {
                return wrap((yeptris_node*)handle, cur->next_sibling);
            }
        }
        want_key = !want_key;
        id = cur->next_sibling;
    }
    return NULL;
}
