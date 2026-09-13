/* tape.c — the compact JSON tape (TODO.restructure/85).
 *
 * The strict fused walk drives: one record per token, numbers
 * converted inline by the number kernel, strings as zero-copy spans,
 * containers linked to their matching record for O(1) skipping. The
 * record budget derives from the input length — every token consumes
 * at least one input byte — so len+2 slots suffice with no pre-pass.
 *
 * Route shape mirrors yeptris_parse_json: a gate-clean container root
 * fuses validation into the walk; everything else validates through
 * yep_json_document first (the error precedence json-suite-strict
 * pins), then builds. The walk's DONE delivers the root close as a
 * token; the root OPEN is synthesized here (walk_init consumes the
 * opener without emitting it). */

#include <string.h>

#include <yeptris/tape.h>

#include "common/error.h"
#include "common/simd_text.h"
#include "encoding/encoding.h"
#include "memory/allocator.h"
#include "scan/json.h"

typedef struct {
    yeptris_json_tape* t;
    size_t cap;
    uint32_t open[YEP_JSON_WALK_DEPTH]; /* record indices */
    int depth;
    int oom;
} tape_ctx;

static int rec_put(tape_ctx* c, uint8_t kind, uint32_t off, uint32_t len, uint64_t val) {
    if (c->t->count >= c->cap) {
        c->oom = 1;
        return 0;
    }
    size_t i = c->t->count++;
    c->t->kinds[i] = kind;
    c->t->offs[i] = off;
    c->t->lens[i] = len;
    c->t->vals[i] = val;
    return 1;
}

static YeptrisStatus tape_carve(yeptris_json_tape* t, size_t len) {
    size_t cap = len + 2;
    size_t off_o = (cap + 15) & ~(size_t)15;
    size_t val_o = (off_o + 2 * cap * sizeof(uint32_t) + 15) & ~(size_t)15;
    char* block = yep_alloc(yep_system_allocator(), val_o + cap * sizeof(uint64_t));
    if (block == NULL) {
        return YEPTRIS_ERROR_MEMORY;
    }
    t->_block = block;
    t->kinds = (uint8_t*)block;
    t->offs = (uint32_t*)(void*)(block + off_o);
    t->lens = t->offs + cap;
    t->vals = (uint64_t*)(void*)(block + val_o);
    t->count = 0;
    t->int_min = 0;
    return YEPTRIS_OK;
}

/* Numbers convert through the number kernel (schema.c's call shape:
 * the span is exactly the token, the cursor must land on its end). */
static int tape_put_number(tape_ctx* c, const char* p, size_t at, size_t end) {
    size_t i = 0;
    int64_t iv = 0;
    double dv = 0;
    int shape = 0;
    size_t span = end - at;
    if (yep_json_number_scan(p + at, span, &i, &shape, &iv, &dv) == 0 || i != span) {
        return 0;
    }
    uint8_t kind = shape == 1 ? YEP_T_FLOAT : YEP_T_INT;
    uint64_t val;
    if (shape == 0) {
        val = (uint64_t)iv;
    } else {
        if (shape == 2) {
            c->t->int_min = 1; /* integer text beyond int64: val is the
                                  double approximation; exact hosts
                                  rebuild from the span */
        }
        memcpy(&val, &dv, sizeof(val));
    }
    return rec_put(c, kind, (uint32_t)at, (uint32_t)span, val);
}

/* Scalar roots: the walk needs an opener, so a bare root value
 * converts through the same kernels. */
static int tape_put_root_scalar(tape_ctx* c, const char* p, size_t len, size_t at) {
    char ch = p[at];
    if (ch == '"') {
        size_t i = at;
        size_t close = 0;
        int esc = 0;
        if (!yep_json_string(p, len, &i, &close, &esc)) {
            return 0;
        }
        return rec_put(c, YEP_T_STR, (uint32_t)(at + 1), (uint32_t)(close - at - 1), 0);
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
        const char* word = ch == 't' ? "true" : (ch == 'f' ? "false" : "null");
        size_t i = at;
        if (!yep_json_literal(p, len, &i, word)) {
            return 0;
        }
        uint8_t kind = ch == 'n' ? YEP_T_NULL : YEP_T_BOOL;
        uint64_t val = kind == YEP_T_BOOL ? (uint64_t)(ch == 't') : 0;
        return rec_put(c, kind, (uint32_t)at, (uint32_t)(i - at), val);
    }
    size_t i = 0;
    int64_t iv = 0;
    double dv = 0;
    int shape = 0;
    if (yep_json_number_scan(p + at, len - at, &i, &shape, &iv, &dv) == 0) {
        return 0;
    }
    return tape_put_number(c, p, at, at + i);
}

/* One strict walk over p[open] → records. Returns OK, MEMORY (tape
 * zeroed), or PARSE (walk rejected — the caller routes to the
 * validating fallback; tape zeroed). check_tail: the fast route has
 * no document validation, so it rejects trailing garbage here. */
static YeptrisStatus tape_walk(const char* p, size_t len, size_t open, yeptris_json_tape* t,
                               int check_tail) {
    if (tape_carve(t, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    tape_ctx c = {t, len + 2, {0}, 0, 0};
    if (!rec_put(&c, YEP_T_DOC, 0, 0, 0)) {
        goto mem;
    }
    uint8_t root = p[open] == '[' ? YEP_T_SEQ_OPEN : YEP_T_MAP_OPEN;
    if (!rec_put(&c, root, (uint32_t)open, 0, 0)) {
        goto mem;
    }
    c.open[c.depth++] = (uint32_t)(c.t->count - 1);

    yep_json_walk w;
    yep_json_walk_init(&w, p, len, open, YEP_JSON_WALK_DEPTH);
    w.strict = 1;
    yep_json_tok tok;
    for (;;) {
        yep_jw_status st = yep_json_walk_next(&w, &tok);
        if (st == YEP_JW_REJECT) {
            goto reject;
        }
        switch (tok.cls) {
        case '[':
        case '{': {
            uint8_t kind = tok.cls == '[' ? YEP_T_SEQ_OPEN : YEP_T_MAP_OPEN;
            if (!rec_put(&c, kind, (uint32_t)tok.at, 0, 0)) {
                goto mem;
            }
            c.open[c.depth++] = (uint32_t)(c.t->count - 1);
            break;
        }
        case ']':
        case '}': {
            if (!rec_put(&c, YEP_T_CLOSE, (uint32_t)tok.at, 0, 0)) {
                goto mem;
            }
            uint32_t close_idx = (uint32_t)(c.t->count - 1);
            uint32_t open_idx = c.open[--c.depth];
            c.t->vals[open_idx] = close_idx;
            c.t->vals[close_idx] = open_idx;
            break;
        }
        case '"':
            if (!rec_put(&c, YEP_T_STR, (uint32_t)(tok.at + 1), (uint32_t)(tok.end - tok.at - 2),
                         0)) {
                goto mem;
            }
            break;
        case 'a': {
            char lead = p[tok.at];
            uint8_t kind = lead == 'n' ? YEP_T_NULL : YEP_T_BOOL;
            uint64_t val = kind == YEP_T_BOOL ? (uint64_t)(lead == 't') : 0;
            if (!rec_put(&c, kind, (uint32_t)tok.at, (uint32_t)(tok.end - tok.at), val)) {
                goto mem;
            }
            break;
        }
        default: /* '#' */
            if (!tape_put_number(&c, p, tok.at, tok.end)) {
                goto reject;
            }
            break;
        }
        if (st == YEP_JW_DONE) {
            break; /* the DONE token was the root close, handled above */
        }
    }
    if (check_tail) {
        size_t tail = w.i;
        while (tail < len &&
               (p[tail] == ' ' || p[tail] == '\t' || p[tail] == '\n' || p[tail] == '\r')) {
            tail++;
        }
        if (tail != len) {
            goto reject;
        }
    }
    if (c.oom) {
        goto mem;
    }
    return YEPTRIS_OK;

mem:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_MEMORY;
reject:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_PARSE;
}

YEPTRIS_API YeptrisStatus yeptris_parse_json_tape(const char* source, size_t len,
                                                  yeptris_json_tape* tape) {
    if ((source == NULL && len != 0) || tape == NULL) {
        return YEPTRIS_ERROR_ARG;
    }
    memset(tape, 0, sizeof(*tape));
    if (len > 0xFFFFFFF0u) {
        yep_error_set(yep_error_tls(), YEP_ERR_UNEXPECTED, 0, 0, 0,
                      "tape spans are uint32; document exceeds 4 GiB");
        return YEPTRIS_ERROR_ARG;
    }

    int gated = len > 0 && yep_text_active()->gate_scan(source, len);
    if (!gated) {
        size_t off = 0;
        while (off < len && (source[off] == ' ' || source[off] == '\t' || source[off] == '\n' ||
                             source[off] == '\r')) {
            off++;
        }
        if (off < len && (source[off] == '[' || source[off] == '{')) {
            YeptrisStatus st = tape_walk(source, len, off, tape, 1);
            if (st != YEPTRIS_ERROR_PARSE) {
                return st; /* OK or MEMORY; a reject falls through */
            }
        }
    }

    /* The validating sequence — parse_json's fallback, byte for byte
     * in precedence: grammar, then encoding when the gate tripped. */
    size_t verr = 0;
    if (!yep_json_document(source, len, &verr)) {
        yep_error_set(yep_error_tls(), YEP_ERR_UNEXPECTED, 0, 0, verr,
                      "not strict JSON at byte %zu", verr);
        return YEPTRIS_ERROR_PARSE;
    }
    if (gated) {
        size_t uerr = 0;
        if (!yep_utf8_validate((const unsigned char*)source, len, &uerr)) {
            yep_error_set(yep_error_tls(), YEP_ERR_ENCODING, 0, 0, uerr,
                          "ill-formed UTF-8 at byte %zu", uerr);
            return YEPTRIS_ERROR_ENCODING;
        }
    }

    size_t at = 0;
    while (at < len &&
           (source[at] == ' ' || source[at] == '\t' || source[at] == '\n' || source[at] == '\r')) {
        at++;
    }
    if (at < len && (source[at] == '[' || source[at] == '{')) {
        YeptrisStatus st = tape_walk(source, len, at, tape, 0);
        if (st == YEPTRIS_OK || st == YEPTRIS_ERROR_MEMORY) {
            return st;
        }
        yep_error_set(yep_error_tls(), YEP_ERR_INTERNAL, 0, 0, at,
                      "tape walk rejected a validated document at byte %zu", at);
        return YEPTRIS_ERROR_INTERNAL;
    }
    if (tape_carve(tape, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    tape_ctx c = {tape, len + 2, {0}, 0, 0};
    if (!rec_put(&c, YEP_T_DOC, 0, 0, 0) || !tape_put_root_scalar(&c, source, len, at)) {
        yep_error_set(yep_error_tls(), YEP_ERR_INTERNAL, 0, 0, at,
                      "tape scalar conversion rejected a validated root at byte %zu", at);
        yeptris_tape_free(tape);
        return YEPTRIS_ERROR_INTERNAL;
    }
    return YEPTRIS_OK;
}

YEPTRIS_API void yeptris_tape_free(yeptris_json_tape* tape) {
    if (tape == NULL) {
        return;
    }
    yep_free(yep_system_allocator(), tape->_block);
    memset(tape, 0, sizeof(*tape));
}
