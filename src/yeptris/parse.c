/* parse.c — public API glue: encoding front-end → engine → DOM. */

#include <string.h>

#include "doc.h"
#include "dom/dom.h"
#include "encoding/encoding.h"
#include "memory/allocator.h"
#include "memory/pool.h"
#include "parse/engine.h"
#include "resolve/resolver.h"
#include "scan/json.h"

#include "parse/events.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>

#include <yeptris.h>

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

static YeptrisDocument parse_impl(const char* buf, size_t len, const YeptrisParseOptions* opts,
                                  int json_mode, YeptrisStatus* status);

YEPTRIS_API YeptrisDocument yeptris_parse(const char* buf, size_t len, YeptrisStatus* status) {
    return yeptris_parse_ex(buf, len, NULL, status);
}

YEPTRIS_API YeptrisDocument yeptris_parse_json(const char* buf, size_t len, YeptrisStatus* status) {
    YeptrisStatus st = YEPTRIS_OK;
    if (buf == NULL && len != 0) {
        st = YEPTRIS_ERROR_ARG;
        goto jfail;
    }
    size_t verr = 0;
    if (!yep_json_document(buf, len, &verr)) {
        yep_error_set(yep_error_tls(), YEP_ERR_UNEXPECTED, 0, 0, verr,
                      "not strict JSON at byte %zu", verr);
        st = YEPTRIS_ERROR_PARSE;
        goto jfail;
    }
    size_t uerr = 0;
    if (!yep_utf8_validate((const unsigned char*)buf, len, &uerr)) {
        yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, uerr, "ill-formed UTF-8 at byte %zu",
                      uerr);
        st = YEPTRIS_ERROR_ENCODING;
        goto jfail;
    }
    /* Direct DOM construction (TODO.impl/27): the validated buffer
     * needs no engine — one walk builds the tree, skipping the event
     * pipeline. A builder/validator disagreement defers to the engine
     * (never happens in-tree; belt and braces). */
    {
        const yep_allocator* sys = yep_system_allocator();
        yep_dom* dom = yep_dom_create(sys);
        if (dom == NULL) {
            st = YEPTRIS_ERROR_MEMORY;
            goto jfail;
        }
        dom->input_base = buf; /* strict JSON is UTF-8 by definition */
        yep_dom_prepare(dom, buf, len);
        int brc = yep_dom_build_json(dom, buf, len);
        if (brc == -1) {
            yep_dom_destroy(dom);
            st = YEPTRIS_ERROR_MEMORY;
            goto jfail;
        }
        if (brc == -2) {
            yep_dom_destroy(dom);
            return parse_impl(buf, len, NULL, 1, status);
        }
        yeptris_document* doc = yep_alloc(sys, sizeof(yeptris_document));
        if (doc == NULL) {
            yep_dom_destroy(dom);
            st = YEPTRIS_ERROR_MEMORY;
            goto jfail;
        }
        doc->dom = dom;
        doc->sys = sys;
        doc->transcoded = NULL;
        doc->transcoded_len = 0;
        doc->input = buf;
        doc->finish_pool = NULL;
        if (status != NULL) {
            *status = YEPTRIS_OK;
        }
        return (YeptrisDocument)doc;
    }
jfail:
    if (status != NULL) {
        *status = st;
    }
    return NULL;
}

static YeptrisDocument parse_impl(const char* buf, size_t len, const YeptrisParseOptions* opts,
                                  int json_mode, YeptrisStatus* status) {
    YeptrisStatus st = YEPTRIS_OK;
    if (buf == NULL && len != 0) {
        st = YEPTRIS_ERROR_ARG;
        goto fail;
    }

    const yep_allocator* sys = yep_system_allocator();

    /* Encoding front-end: BOM sniff; borrow UTF-8, transcode the rest. */
    const char* data = buf;
    size_t data_len = len;
    unsigned char* transcoded = NULL;
    size_t transcoded_len = 0;
    yep_encoding enc = YEP_ENC_UNKNOWN;

    if (json_mode) {
        /* JSON mode validated grammar and UTF-8 well-formedness at the
         * entry: no BOM, no transcoding, and never the printable set —
         * RFC 8259 allows DEL and noncharacters that YAML c-printable
         * excludes. The engine takes the bytes directly. */
        goto engine_enter;
    }

    size_t bom = yep_bom_sniff((const unsigned char*)buf, len, &enc);
    data = buf + bom;
    data_len = len - bom;

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
    yep_engine* eng = NULL;
engine_enter:
    eng = yep_engine_create(sys);
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
    yep_dom_prepare(dom, buf, len);
    yep_engine_prepare(eng, buf, len);

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
    /* DOM strings are input-offsets or arena copies; nothing in the
     * tree references the finish pool anymore — release it now */

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
    dom->input_base = transcoded ? (const char*)transcoded : buf;
    doc->dom = dom;
    doc->sys = sys;
    doc->transcoded = transcoded;
    doc->transcoded_len = transcoded_len;
    doc->input = buf;
    /* the finish pool dies here: every DOM string is an input offset
     * or an arena copy (dom_ev_str) — nothing references it */
    yep_pool_destroy(finish);
    doc->finish_pool = NULL;
    return (YeptrisDocument)doc;

fail:
    if (status != NULL) {
        *status = st;
    }
    return NULL;
}

YEPTRIS_API YeptrisDocument yeptris_parse_ex(const char* buf, size_t len,
                                             const YeptrisParseOptions* opts,
                                             YeptrisStatus* status) {
    return parse_impl(buf, len, opts, 0, status);
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

yeptris_node* yep_handle_new(yeptris_document* doc, uint32_t id) {
    yeptris_node* n = yep_hpool_alloc(doc->dom->handles, sizeof(yeptris_node), 16);
    if (n == NULL) {
        return NULL;
    }
    n->doc = doc;
    n->id = id;
    return n;
}

static YeptrisNode node_new_handle(yeptris_document* doc, uint32_t id) {
    return (YeptrisNode)yep_handle_new(doc, id);
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

YEPTRIS_API uint32_t yeptris_node_id(YeptrisNode handle) {
    yeptris_node* n = (yeptris_node*)handle;
    return n == NULL ? UINT32_MAX : n->id;
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

/* Exact decimal fast path (TODO.impl/08B): [-+]?digits[.digits]
 * [(eE)[-+]?digits] with <= 15 significant mantissa digits and an
 * adjusted exponent within +-22 converts with one multiplication by
 * a table power of ten — provably correctly rounded in that range
 * (Clinger); libc strtod stays the fallback outside it. */
static const double k_pow10[23] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,  1e10, 1e11,
    1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22,
};

static int fast_double(const char* s, size_t len, double* out) {
    size_t i = 0;
    int neg = 0;
    if (i < len && (s[i] == '-' || s[i] == '+')) {
        neg = s[i] == '-';
        i++;
    }
    uint64_t m = 0;
    int digits = 0;
    int dot = -1;
    for (; i < len; i++) {
        char c = s[i];
        if (c == '.') {
            if (dot >= 0) {
                return 0;
            }
            dot = (int)digits;
            continue;
        }
        if (c < '0' || c > '9') {
            break;
        }
        if (digits >= 15) {
            return 0; /* outside the exact range */
        }
        m = m * 10u + (uint64_t)(c - '0');
        digits++;
    }
    if (digits == 0 || (i < len && s[i] != 'e' && s[i] != 'E')) {
        return 0; /* empty mantissa or trailing junk */
    }
    int e10 = 0;
    if (i < len) {
        i++; /* e/E */
        int eneg = 0;
        if (i < len && (s[i] == '-' || s[i] == '+')) {
            eneg = s[i] == '-';
            i++;
        }
        if (i >= len) {
            return 0;
        }
        for (; i < len; i++) {
            if (s[i] < '0' || s[i] > '9') {
                return 0;
            }
            e10 = e10 * 10 + (s[i] - '0');
            if (e10 > 308) {
                return 0;
            }
        }
        if (eneg) {
            e10 = -e10;
        }
    }
    if (dot >= 0) {
        e10 -= digits - dot;
    }
    if (e10 > 22 || e10 < -22 || m >= (1ull << 53)) {
        return 0; /* outside Clinger's exact range */
    }
    if (e10 >= 0) {
        double v = (double)m * k_pow10[e10];
        *out = neg ? -v : v;
    } else {
        double v = (double)m / k_pow10[-e10];
        *out = neg ? -v : v;
    }
    return 1;
}

/* Decodes a node's compact string through its document's regions. */
static yep_view node_view(const yeptris_node* h, yep_sview sv) {
    return yep_dom_view(h->doc->dom, sv);
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
    return view_out(node_view((yeptris_node*)handle, n->value), len);
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

/* strips '_' and ',' into dst (nul-terminated); returns length.
 * The COMMON case (no separators) returns the borrowed view — no
 * copy, no nul-termination needed by the in-place parsers. */
static size_t clean_num(const yeptris_node* h, const yep_dnode* n, char* dst, size_t cap,
                        const char** text) {
    yep_view v = node_view(h, n->value);
    if (v.len == 0) {
        *text = dst;
        dst[0] = '\0';
        return 0;
    }
    if (memchr(v.p, '_', v.len) == NULL && memchr(v.p, ',', v.len) == NULL) {
        *text = (const char*)v.p; /* zero-copy fast path (08B) */
        return v.len;
    }
    size_t o = 0;
    for (uint32_t i = 0; i < v.len && o + 1 < cap; i++) {
        char c = ((const char*)v.p)[i];
        if (c == '_' || c == ',') {
            continue;
        }
        dst[o++] = c;
    }
    dst[o] = '\0';
    *text = dst;
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
    const char* num = NULL;
    size_t len = clean_num((yeptris_node*)handle, n, buf, sizeof(buf), &num);
    if (len == 0) {
        return YEPTRIS_ERROR_PARSE;
    }
    /* the 0o mutation path below rewrites the text; copy borrowed
     * views into buf first so the input is never touched */
    if (num != buf) {
        if (len >= sizeof(buf)) {
            return YEPTRIS_ERROR_PARSE;
        }
        memcpy(buf, num, len);
        buf[len] = '\0';
        num = buf;
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
    const char* num = NULL;
    size_t len = clean_num((yeptris_node*)handle, n, buf, sizeof(buf), &num);
    if (len == 0) {
        return YEPTRIS_ERROR_PARSE;
    }
    /* 08B fast path: the common decimal shape converts exactly with
     * integer arithmetic (Clinger bounds: mantissa < 2^53, adjusted
     * exponent within +-22); everything else falls to strtod */
    {
        double fast;
        if (fast_double(num, len, &fast)) {
            *out = fast;
            return YEPTRIS_OK;
        }
    }
    if (num != buf) {
        if (len >= sizeof(buf)) {
            return YEPTRIS_ERROR_PARSE;
        }
        memcpy(buf, num, len);
        buf[len] = '\0';
        num = buf;
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
        yep_view v = node_view((yeptris_node*)handle, n->value);
        if (v.len == m && memcmp(v.p, k_true[i], m) == 0) {
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
    return view_out(node_view((yeptris_node*)handle, n->tag), len);
}

YEPTRIS_API const char* yeptris_node_anchor(YeptrisNode handle, size_t* len) {
    const yep_dnode* n = node_of(handle);
    if (n == NULL || n->anchor.len == 0) {
        if (len != NULL) {
            *len = 0;
        }
        return NULL;
    }
    return view_out(node_view((yeptris_node*)handle, n->anchor), len);
}

static YeptrisNode wrap(yeptris_node* base, uint32_t id) {
    if (base == NULL || base->doc == NULL) {
        return NULL;
    }
    /* Handles live in the document's thread-safe arena: one
     * document_free reclaims every handle ever handed out (zero-leak
     * contract) while read-only sharing stays safe (TODO.impl/19) */
    yeptris_node* n = yep_hpool_alloc(base->doc->dom->handles, sizeof(yeptris_node), 16);
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
    yep_dom* dom = ((yeptris_node*)handle)->doc->dom;
    /* O(1) through the lazy index (first use builds it); the scan
     * below is the build-failure fallback */
    uint32_t hit =
        yep_midx_lookup(dom, ((yeptris_node*)handle)->id, (yep_view){key, (uint32_t)key_len});
    if (hit != UINT32_MAX) {
        const yep_dnode* kn = yep_dom_node(dom, hit);
        if (kn != NULL) {
            return wrap((yeptris_node*)handle, kn->next_sibling);
        }
    }
    uint32_t id = n->first_child;
    int want_key = 1;
    while (id != UINT32_MAX) {
        const yep_dnode* cur = yep_dom_node(dom, id);
        if (cur == NULL) {
            break;
        }
        if (want_key && cur->kind == YEP_DOM_SCALAR) {
            yep_view kv = {key, (uint32_t)key_len};
            if (yep_view_eq(yep_dom_view(dom, cur->value), kv)) {
                return wrap((yeptris_node*)handle, cur->next_sibling);
            }
        }
        want_key = !want_key;
        id = cur->next_sibling;
    }
    return NULL;
}
