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

#include <stdlib.h>
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

/* Append one converted number record (the walker/root scan did the
 * ONE pass; the shape contract is the number kernel's). */
static int tape_put_converted(tape_ctx* c, size_t at, size_t span, int shape, int64_t iv,
                              double dv) {
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
    return tape_put_converted(c, at, i, shape, iv, dv);
}

/* One strict walk over p[open] -> records. Returns OK, MEMORY
 * (tape zeroed), or PARSE (walk rejected; tape zeroed).
 * check_tail: the fast route has no document validation, so it
 * rejects trailing garbage here.
 *
 * FUSED specialization of yep_json_walk_next: same states, same
 * rejects, byte for byte — but the token never crosses a call
 * boundary or a tok struct. Cursor, record count, and the
 * top-of-stack (kind/expect/open_at) ride in locals; the depth
 * arrays are only touched on push/pop. Columns write directly
 * (capacity is structurally sound: every record consumes at least
 * one input byte, and the carve holds len+2). yep_json_walk stays
 * the reference machine for DOM/push/pull; tape-diff (corpus + the
 * 2M fuzz parity) pins the equivalence. */
static YeptrisStatus tape_walk(const char* p, size_t len, size_t open, yeptris_json_tape* t,
                               int check_tail) {
    if (tape_carve(t, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    uint8_t* kinds = t->kinds;
    uint32_t* offs = t->offs;
    uint32_t* lens = t->lens;
    uint64_t* vals = t->vals;
    size_t cap = len + 2;
    uint32_t open_at[YEP_JSON_WALK_DEPTH];
    uint8_t kind[YEP_JSON_WALK_DEPTH];

    size_t count = 0;
    kinds[0] = YEP_T_DOC;
    offs[0] = 0;
    lens[0] = 0;
    vals[0] = 0;
    count = 1;

    uint8_t top_kind = p[open] == '[' ? 0 : 1;
    kinds[1] = top_kind ? YEP_T_MAP_OPEN : YEP_T_SEQ_OPEN;
    offs[1] = (uint32_t)open;
    lens[1] = 0;
    uint32_t top_open = 1;
    count = 2;
    uint8_t top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
    int depth = 1;
    open_at[0] = top_open;
    kind[0] = top_kind;

    size_t i = open + 1;
    for (;;) {
        while (i < len && (p[i] == ' ' || p[i] == '\n' || p[i] == '\r')) {
            i++;
        }
        if (i >= len || p[i] == '\t') {
            goto reject;
        }
        size_t at = i;
        char c = p[i];

        /* walker order: punctuation states first so a value arm cannot
         * fire in JW_COMMA_OR_CLOSE / JW_COLON ("[1 2]" is a reject).
         * Close is legal in COMMA_OR_CLOSE, so it falls through. */
        if (top_expect == JW_COLON) {
            if (c != ':') {
                goto reject;
            }
            top_expect = JW_VALUE;
            i = at + 1;
            continue;
        }
        if (top_expect == JW_COMMA_OR_CLOSE) {
            if (c == ',') {
                top_expect = top_kind ? JW_KEY : JW_VALUE;
                i = at + 1;
                continue;
            }
            if (c != ']' && c != '}') {
                goto reject;
            }
            /* close falls through */
        }

        int key_slot = top_kind == 1 && (top_expect == JW_KEY_OR_CLOSE || top_expect == JW_KEY);

        if ((unsigned)(c - '0') <= 9u || c == '-') {
            if (key_slot) {
                goto reject;
            }
            int shape = 0;
            int64_t iv = 0;
            double dv = 0;
            if (!yep_json_number_scan(p, len, &i, &shape, &iv, &dv)) {
                goto reject;
            }
            uint64_t v;
            if (shape == 0) {
                v = (uint64_t)iv;
            } else {
                if (shape == 2) {
                    t->int_min = 1;
                }
                memcpy(&v, &dv, sizeof(v));
            }
            kinds[count] = shape == 1 ? YEP_T_FLOAT : YEP_T_INT;
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)(i - at);
            vals[count] = v;
            count++;
            top_expect = JW_COMMA_OR_CLOSE;
            continue;
        }
        if (c == '"') {
            size_t close = 0;
            int esc = 0;
            if (!yep_json_string(p, len, &i, &close, &esc)) {
                goto reject;
            }
            kinds[count] = YEP_T_STR;
            offs[count] = (uint32_t)(at + 1);
            lens[count] = (uint32_t)(close - at - 1);
            /* no val store: undefined for STR by contract (the FFI
             * readers never touch it — one 8B store saved per string) */
            count++;
            top_expect = key_slot ? JW_COLON : JW_COMMA_OR_CLOSE;
            continue;
        }
        if (c == ']' || c == '}') {
            int want = c == ']' ? 0 : 1;
            if (top_kind != want ||
                (top_expect != JW_VALUE_OR_CLOSE && top_expect != JW_KEY_OR_CLOSE &&
                 top_expect != JW_COMMA_OR_CLOSE)) {
                goto reject;
            }
            kinds[count] = YEP_T_CLOSE;
            offs[count] = (uint32_t)at;
            lens[count] = 0;
            vals[count] = top_open;
            count++;
            vals[top_open] = (uint32_t)(count - 1);
            depth--;
            i = at + 1;
            if (depth == 0) {
                break;
            }
            top_kind = kind[depth - 1];
            top_expect = JW_COMMA_OR_CLOSE;
            top_open = open_at[depth - 1];
            continue;
        }
        if (c == '{' || c == '[') {
            if (depth >= YEP_JSON_WALK_DEPTH) {
                goto reject;
            }
            if (key_slot) {
                goto reject;
            }
            /* spill current top, then push */
            kind[depth - 1] = top_kind;
            open_at[depth - 1] = top_open;
            top_kind = c == '[' ? 0 : 1;
            kinds[count] = c == '[' ? YEP_T_SEQ_OPEN : YEP_T_MAP_OPEN;
            offs[count] = (uint32_t)at;
            lens[count] = 0;
            /* no val store: the CLOSE arm writes the link; an unclosed
             * OPEN's val is never read (errors free the tape) */
            top_open = (uint32_t)count;
            count++;
            top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
            kind[depth] = top_kind;
            open_at[depth] = top_open;
            depth++;
            i = at + 1;
            continue;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            const char* w = c == 't' ? "true" : (c == 'f' ? "false" : "null");
            size_t wl = c == 't' ? 4 : (c == 'f' ? 5 : 4);
            for (size_t k = 0; k < wl; k++) {
                if (at + k >= len || p[at + k] != w[k]) {
                    goto reject;
                }
            }
            if (at + wl < len) {
                char z = p[at + wl];
                if (z != ' ' && z != '\n' && z != '\r' && z != ',' && z != ']' && z != '}' &&
                    z != ':') {
                    goto reject;
                }
            }
            if (key_slot) {
                goto reject;
            }
            uint8_t lkind = c == 'n' ? YEP_T_NULL : YEP_T_BOOL;
            kinds[count] = lkind;
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)wl;
            vals[count] = lkind == YEP_T_BOOL ? (uint64_t)(c == 't') : 0;
            count++;
            i = at + wl;
            top_expect = JW_COMMA_OR_CLOSE;
            continue;
        }
        goto reject;
    }
    if (check_tail) {
        size_t tail = i;
        while (tail < len &&
               (p[tail] == ' ' || p[tail] == '\t' || p[tail] == '\n' || p[tail] == '\r')) {
            tail++;
        }
        if (tail != len) {
            goto reject;
        }
    }
    if (count > cap) {
        goto mem;
    }
    t->count = count;
    return YEPTRIS_OK;

mem:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_MEMORY;
reject:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_PARSE;
}

/* ---- stage 2: the indexed walk (lean) ------------------------------
 * Consumes stage 1's positions: ws is never scanned (the positions
 * hop it), string interiors never touched. Loop law: the scalar
 * token in (tok_end, structurals[si]) is parsed BEFORE the structural
 * dispatches (a close must not accept while an unparsed token
 * precedes it). Bounds: every record needs one input byte and the
 * record budget is len+2 — checked ONCE at the end, not per record.
 * Same records and rejects as the fused walk (tape-diff pins it). */

static YeptrisStatus tape_walk_indexed(const char* p, size_t len, size_t open_pos,
                                       const uint32_t* structurals, size_t ns,
                                       const uint32_t* str_close, size_t nstr,
                                       yeptris_json_tape* t, int check_tail) {
    if (tape_carve(t, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    const size_t cap = len + 2;
    uint8_t* kinds = t->kinds;
    uint32_t* offs = t->offs;
    uint32_t* lens = t->lens;
    uint64_t* vals = t->vals;
    size_t count = 0;

    uint32_t stack_open[YEP_JSON_WALK_DEPTH];
    uint8_t stack_kind[YEP_JSON_WALK_DEPTH];

    kinds[count] = YEP_T_DOC; /* slot 0 */
    offs[count] = 0;
    lens[count] = 0;
    vals[count] = 0;
    count++;

    uint8_t top_kind = p[open_pos] == '[' ? 0 : 1;
    uint32_t top_open = (uint32_t)count;
    uint8_t top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
    int depth = 1;
    stack_open[0] = top_open;
    stack_kind[0] = top_kind;
    kinds[count] = top_kind ? YEP_T_MAP_OPEN : YEP_T_SEQ_OPEN; /* slot 1 */
    offs[count] = 0;
    lens[count] = 0;
    vals[count] = 0;
    count++;

    size_t stri = 0;
    size_t tok_end = open_pos + 1;
    int done = 0;

    for (size_t si = 1; si < ns && !done; si++) {
        uint32_t at = structurals[si];
        char ch = p[at];

        /* the scalar token in (tok_end, at), if any — BEFORE dispatch */
        if (tok_end < at) {
            size_t j = tok_end;
            char w0 = p[j];
            if (w0 == ' ' || w0 == '\n' || w0 == '\r') {
                do {
                    j++;
                } while (j < at && (p[j] == ' ' || p[j] == '\n' || p[j] == '\r'));
            }
            if (j < at) {
                char sc0 = p[j];
                if (sc0 == '\t') {
                    goto reject; /* tabs: the fallback route owns them */
                }
                if (top_expect == JW_COLON || top_expect == JW_COMMA_OR_CLOSE) {
                    goto reject; /* a token where punctuation was required */
                }
                int kslot =
                    top_kind == 1 && (top_expect == JW_KEY_OR_CLOSE || top_expect == JW_KEY);
                if (sc0 == 't' || sc0 == 'f' || sc0 == 'n') {
                    const char* w = sc0 == 't' ? "true" : (sc0 == 'f' ? "false" : "null");
                    size_t wl = sc0 == 't' ? 4 : (sc0 == 'f' ? 5 : 4);
                    if ((size_t)(at - j) < wl) {
                        goto reject;
                    }
                    for (size_t k = 0; k < wl; k++) {
                        if (p[j + k] != w[k]) {
                            goto reject;
                        }
                    }
                    for (size_t k = j + wl; k < at; k++) {
                        if (p[k] != ' ' && p[k] != '\n' && p[k] != '\r' && p[k] != '\t') {
                            goto reject; /* one token per span */
                        }
                    }
                    if (kslot) {
                        goto reject;
                    }
                    kinds[count] = sc0 == 'n' ? YEP_T_NULL : YEP_T_BOOL;
                    offs[count] = (uint32_t)j;
                    lens[count] = (uint32_t)wl;
                    vals[count] = (uint64_t)(sc0 == 't');
                    count++;
                    top_expect = JW_COMMA_OR_CLOSE;
                } else if ((sc0 >= '0' && sc0 <= '9') || sc0 == '-') {
                    size_t i2 = j;
                    int shape = 0;
                    int64_t iv = 0;
                    double dv = 0;
                    if (!yep_json_number_scan(p, at, &i2, &shape, &iv, &dv) || i2 > at) {
                        goto reject;
                    }
                    for (size_t k = i2; k < at; k++) {
                        if (p[k] != ' ' && p[k] != '\n' && p[k] != '\r' && p[k] != '\t') {
                            goto reject; /* exactly one number per span */
                        }
                    }
                    if (kslot) {
                        goto reject;
                    }
                    if (shape == 2) {
                        t->int_min = 1;
                    }
                    kinds[count] = shape == 1 ? YEP_T_FLOAT : YEP_T_INT;
                    offs[count] = (uint32_t)j;
                    lens[count] = (uint32_t)(i2 - j);
                    if (shape == 0) {
                        vals[count] = (uint64_t)iv;
                    } else {
                        memcpy(&vals[count], &dv, sizeof(dv));
                    }
                    count++;
                    top_expect = JW_COMMA_OR_CLOSE;
                } else {
                    goto reject; /* YAML plain scalar / comment / indicator */
                }
            }
        }

        /* dispatch the structural */
        int key_slot = top_kind == 1 && (top_expect == JW_KEY_OR_CLOSE || top_expect == JW_KEY);
        switch (ch) {
        case '"': {
            if (top_expect == JW_COLON || top_expect == JW_COMMA_OR_CLOSE || stri >= nstr) {
                goto reject;
            }
            uint32_t close = str_close[stri++];
            if (close <= at) {
                goto reject;
            }
            kinds[count] = YEP_T_STR;
            offs[count] = at + 1;
            lens[count] = close - at - 1;
            vals[count] = 0;
            count++;
            tok_end = (size_t)close + 1;
            top_expect = key_slot ? JW_COLON : JW_COMMA_OR_CLOSE;
            break;
        }
        case '{':
        case '[': {
            if (key_slot || top_expect == JW_COLON || top_expect == JW_COMMA_OR_CLOSE ||
                depth >= YEP_JSON_WALK_DEPTH) {
                goto reject;
            }
            stack_kind[depth - 1] = top_kind;
            top_kind = ch == '[' ? 0 : 1;
            top_open = (uint32_t)count;
            stack_open[depth] = top_open;
            kinds[count] = ch == '[' ? YEP_T_SEQ_OPEN : YEP_T_MAP_OPEN;
            offs[count] = 0;
            lens[count] = 0;
            vals[count] = 0;
            count++;
            top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
            depth++;
            tok_end = (size_t)at + 1;
            break;
        }
        case ']':
        case '}': {
            int want = ch == ']' ? 0 : 1;
            if (top_kind != want ||
                (top_expect != JW_VALUE_OR_CLOSE && top_expect != JW_KEY_OR_CLOSE &&
                 top_expect != JW_COMMA_OR_CLOSE)) {
                goto reject;
            }
            kinds[count] = YEP_T_CLOSE;
            offs[count] = 0;
            lens[count] = 0;
            vals[count] = top_open;
            count++;
            vals[top_open] = count - 1;
            depth--;
            tok_end = (size_t)at + 1;
            if (depth == 0) {
                done = 1;
            } else {
                top_kind = stack_kind[depth - 1];
                top_expect = JW_COMMA_OR_CLOSE;
                top_open = stack_open[depth - 1];
            }
            break;
        }
        case ':': {
            if (top_expect != JW_COLON) {
                goto reject;
            }
            top_expect = JW_VALUE;
            tok_end = (size_t)at + 1;
            break;
        }
        case ',': {
            if (top_expect != JW_COMMA_OR_CLOSE) {
                goto reject;
            }
            top_expect = top_kind ? JW_KEY : JW_VALUE;
            tok_end = (size_t)at + 1;
            break;
        }
        default:
            goto reject;
        }
    }
    if (!done || count > cap) {
        goto reject; /* structure ran out, or the impossible cap tripped */
    }
    if (check_tail) {
        size_t tail = tok_end;
        while (tail < len &&
               (p[tail] == ' ' || p[tail] == '\t' || p[tail] == '\n' || p[tail] == '\r')) {
            tail++;
        }
        if (tail != len) {
            goto reject;
        }
    }
    t->count = count;
    return YEPTRIS_OK;

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
            /* the two-stage route: stage 1 locates structure (scalar
             * classifier first; the SIMD classifiers stack on), stage
             * 2 walks the precomputed positions lean */
            uint32_t* structurals = (uint32_t*)malloc((len + 2) * sizeof(uint32_t));
            uint32_t* str_close = (uint32_t*)malloc((len + 2) * sizeof(uint32_t));
            if (structurals == NULL || str_close == NULL) {
                free(structurals);
                free(str_close);
                return YEPTRIS_ERROR_MEMORY;
            }
            size_t ns = 0;
            size_t nstr = 0;
            YeptrisStatus st;
            if (yep_json_index_build(source, len, structurals, &ns, str_close, &nstr)) {
                st = tape_walk_indexed(source, len, off, structurals, ns, str_close, nstr, tape,
                                       1);
            } else {
                st = YEPTRIS_ERROR_PARSE;
            }
            free(structurals);
            free(str_close);
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
