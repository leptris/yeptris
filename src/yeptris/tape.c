/* tape.c — the compact JSON tape (TODO.restructure/85, v2 records).
 *
 * The strict fused walk drives: one THREE-column record per token
 * (kind byte, off, len) — bools encode in the kind, container links
 * ride the off column, numbers record spans only (the lean shape scan
 * validates; yeptris_tape_convert materializes). Strings stay
 * zero-copy spans. The record budget derives from the input length —
 * every token consumes at least one input byte — so len+2 slots
 * suffice with no pre-pass.
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

#if defined(__GNUC__)
#define YEP_UNUSED_FN __attribute__((unused))
#else
#define YEP_UNUSED_FN
#endif

#ifndef YEP_TOKEN_CONTRACT_ROUTE
#define YEP_TOKEN_CONTRACT_ROUTE 0
#endif

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

static int rec_put(tape_ctx* c, uint8_t kind, uint32_t off, uint32_t len) {
    if (c->t->count >= c->cap) {
        c->oom = 1;
        return 0;
    }
    size_t i = c->t->count++;
    c->t->kinds[i] = kind;
    c->t->offs[i] = off;
    c->t->lens[i] = len;
    return 1;
}

static YeptrisStatus tape_carve(yeptris_json_tape* t, size_t len) {
    size_t cap = len + 2;
    size_t off_o = (cap + 15) & ~(size_t)15;
    char* block = yep_alloc(yep_system_allocator(), off_o + 2 * cap * sizeof(uint32_t));
    if (block == NULL) {
        return YEPTRIS_ERROR_MEMORY;
    }
    t->_block = block;
    t->kinds = (uint8_t*)block;
    t->offs = (uint32_t*)(void*)(block + off_o);
    t->lens = t->offs + cap;
    t->count = 0;
    t->int_min = 0;
    return YEPTRIS_OK;
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
        return rec_put(c, YEP_T_STR, (uint32_t)(at + 1), (uint32_t)(close - at - 1));
    }
    if (ch == 't' || ch == 'f' || ch == 'n') {
        const char* word = ch == 't' ? "true" : (ch == 'f' ? "false" : "null");
        size_t i = at;
        if (!yep_json_literal(p, len, &i, word)) {
            return 0;
        }
        uint8_t kind = ch == 'n' ? YEP_T_NULL : (ch == 't' ? YEP_T_TRUE : YEP_T_FALSE);
        return rec_put(c, kind, (uint32_t)at, (uint32_t)(i - at));
    }
    size_t i = 0;
    int flt = 0;
    if (yep_json_number_shape(p + at, len - at, &i, &flt) == 0) {
        return 0;
    }
    return rec_put(c, flt ? YEP_T_FLOAT : YEP_T_INT, (uint32_t)at, (uint32_t)i);
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
/* The punctuation inline (the simdjson tape-builder lesson): after a
 * value or a key, the next token is a comma/colon or the close —
 * consume it inline instead of paying a full loop iteration for a
 * one-byte state flip. Anything else (close, garbage, a tab) falls
 * through with the cursor parked for the main loop to judge. */
static inline uint8_t tape_inline_punct(const char* p, size_t len, size_t* i, uint8_t want) {
    size_t j = *i;
    while (j < len && (p[j] == ' ' || p[j] == '\n' || p[j] == '\r')) {
        j++;
    }
    if (j < len && (uint8_t)p[j] == want) {
        *i = j + 1;
        return 1;
    }
    *i = j;
    return 0;
}

static YeptrisStatus tape_walk(const char* p, size_t len, size_t open, yeptris_json_tape* t,
                               int check_tail) {
    if (tape_carve(t, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    uint8_t* kinds = t->kinds;
    uint32_t* offs = t->offs;
    uint32_t* lens = t->lens;
    size_t cap = len + 2;
    uint32_t open_at[YEP_JSON_WALK_DEPTH];
    uint8_t kind[YEP_JSON_WALK_DEPTH];

    size_t count = 0;
    kinds[0] = YEP_T_DOC;
    offs[0] = 0;
    lens[0] = 0;
    count = 1;

    uint8_t top_kind = p[open] == '[' ? 0 : 1;
    kinds[1] = top_kind ? YEP_T_MAP_OPEN : YEP_T_SEQ_OPEN;
    offs[1] = 0; /* the CLOSE arm writes the link */
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
            int flt = 0;
            if (!yep_json_number_shape(p, len, &i, &flt)) {
                goto reject;
            }
            kinds[count] = flt ? YEP_T_FLOAT : YEP_T_INT;
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)(i - at);
            count++;
            if (tape_inline_punct(p, len, &i, ',')) {
                top_expect = top_kind ? JW_KEY : JW_VALUE;
                key_slot = top_kind ? 1 : 0;
            } else {
                top_expect = JW_COMMA_OR_CLOSE;
            }
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
            count++;
            if (key_slot) { /* the key's colon, inline */
                if (tape_inline_punct(p, len, &i, ':')) {
                    top_expect = JW_VALUE;
                    key_slot = 0;
                } else {
                    top_expect = JW_COLON;
                }
            } else if (tape_inline_punct(p, len, &i, ',')) {
                top_expect = top_kind ? JW_KEY : JW_VALUE;
                key_slot = top_kind ? 1 : 0;
            } else {
                top_expect = JW_COMMA_OR_CLOSE;
            }
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
            offs[count] = top_open; /* link to the OPEN */
            lens[count] = 0;
            offs[top_open] = (uint32_t)count; /* the OPEN's link back */
            count++;
            depth--;
            i = at + 1;
            if (depth == 0) {
                break;
            }
            top_kind = kind[depth - 1];
            top_expect = JW_COMMA_OR_CLOSE;
            top_open = open_at[depth - 1];
            key_slot = 0; /* never read before the comma resets it —
                             set anyway so the flag has one owner */
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
            offs[count] = 0; /* the matching CLOSE writes the link */
            lens[count] = 0;
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
            /* packed literal compare: bounds-checked span first, then
             * one u32 load (false adds its 5th byte) */
            size_t wl = c == 'f' ? 5 : 4;
            if (at + wl > len) {
                goto reject;
            }
            uint32_t got4;
            memcpy(&got4, p + at, 4);
            if (got4 != (c == 't'   ? 0x65757274u /* "true" */
                         : c == 'n' ? 0x6C6C756Eu /* "null" */
                                    : 0x736C6166u /* "fals" */) ||
                (c == 'f' && p[at + 4] != 'e')) {
                goto reject;
            }
            if (at + wl < len) {
                char z = p[at + wl];
                if (z != ' ' && z != '\n' && z != '\r' && z != ',' && z != ']' && z != '}' &&
                    z != ':') {
                    goto reject;
                }
            }
            kinds[count] = c == 'n' ? YEP_T_NULL : (c == 't' ? YEP_T_TRUE : YEP_T_FALSE);
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)wl;
            count++;
            i = at + wl;
            if (tape_inline_punct(p, len, &i, ',')) {
                top_expect = top_kind ? JW_KEY : JW_VALUE;
                key_slot = top_kind ? 1 : 0;
            } else {
                top_expect = JW_COMMA_OR_CLOSE;
            }
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
    t->_src = p;
    t->_srclen = len;
    return YEPTRIS_OK;

mem:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_MEMORY;
reject:
    yeptris_tape_free(t);
    return YEPTRIS_ERROR_PARSE;
}

/* The token-contract walk: consumes stage 1's positions — whitespace
 * is never scanned (a one-byte-typical gap check proves the residue
 * since the last token is whitespace), string interiors never appear
 * (yep_json_string stays the authority on closes, escapes, and raw
 * C0), numbers are span records via the shape scan. Same states and
 * rejects as the fused walk; tape-diff pins the equivalence. */
static YEP_UNUSED_FN YeptrisStatus tape_walk_idx(const char* p, size_t len, size_t open,
                                                 const uint32_t* idx, size_t nidx,
                                                 yeptris_json_tape* t) {
    if (tape_carve(t, len) != YEPTRIS_OK) {
        return YEPTRIS_ERROR_MEMORY;
    }
    uint8_t* kinds = t->kinds;
    uint32_t* offs = t->offs;
    uint32_t* lens = t->lens;
    uint32_t open_at[YEP_JSON_WALK_DEPTH];
    uint8_t kind[YEP_JSON_WALK_DEPTH];

    size_t count = 0;
    kinds[0] = YEP_T_DOC;
    offs[0] = 0;
    lens[0] = 0;
    count = 1;

    uint8_t top_kind = p[open] == '[' ? 0 : 1;
    kinds[1] = top_kind ? YEP_T_MAP_OPEN : YEP_T_SEQ_OPEN;
    offs[1] = 0;
    lens[1] = 0;
    uint32_t top_open = 1;
    count = 2;
    uint8_t top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
    /* key_slot as a maintained flag: transitions update it, the loop
     * never recomputes (two compares per token saved) */
    int key_slot = top_kind ? 1 : 0;
    int depth = 1;
    open_at[0] = top_open;
    kind[0] = top_kind;

    size_t si = 0;
    while (si < nidx && idx[si] <= open) {
        si++; /* the root opener (and anything before it) is consumed */
    }
    size_t prev_end = open + 1;
    for (;;) {
        if (si >= nidx) {
            goto reject; /* EOF mid-structure */
        }
        size_t at = idx[si++];
        if (at >= len) {
            goto reject;
        }
        char c = p[at];
        /* no gap check: every byte class that could hide between
         * tokens is itself a token (dropped scalars are scalar-run
         * starts, raw C0 is a scalar — the state machine rejects
         * them); legal ws is simply invisible. Tabs are ws-class and
         * fall to the document path, exactly like the fused walk. */
        prev_end = at + 1;

        if (top_expect == JW_COLON) {
            if (c != ':') {
                goto reject;
            }
            top_expect = JW_VALUE;
            key_slot = 0; /* a value follows the colon */
            continue;
        }
        if (top_expect == JW_COMMA_OR_CLOSE) {
            if (c == ',') {
                top_expect = top_kind ? JW_KEY : JW_VALUE;
                key_slot = top_kind ? 1 : 0;
                continue;
            }
            if (c != ']' && c != '}') {
                goto reject;
            }
        } else if (key_slot) {
            /* KEY position: a string or the container's close — the
             * value arms are unreachable, the chain short-circuits */
            if (c != '"' && c != ']' && c != '}') {
                goto reject;
            }
        }

        if ((unsigned)(c - '0') <= 9u || c == '-') {
            size_t i = at;
            int flt = 0;
            if (!yep_json_number_shape(p, len, &i, &flt)) {
                goto reject;
            }
            kinds[count] = flt ? YEP_T_FLOAT : YEP_T_INT;
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)(i - at);
            count++;
            prev_end = i;
            top_expect = JW_COMMA_OR_CLOSE;
            continue;
        }
        if (c == '"') {
            size_t i = at;
            size_t close = 0;
            int esc = 0;
            if (!yep_json_string(p, len, &i, &close, &esc)) {
                goto reject;
            }
            kinds[count] = YEP_T_STR;
            offs[count] = (uint32_t)(at + 1);
            lens[count] = (uint32_t)(close - at - 1);
            count++;
            prev_end = i;
            top_expect = key_slot ? JW_COLON : JW_COMMA_OR_CLOSE;
            /* key_slot stays 1 through COLON — the value arm clears it */
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
            offs[count] = top_open;
            lens[count] = 0;
            offs[top_open] = (uint32_t)count;
            count++;
            depth--;
            if (depth == 0) {
                break;
            }
            top_kind = kind[depth - 1];
            top_expect = JW_COMMA_OR_CLOSE;
            top_open = open_at[depth - 1];
            key_slot = 0; /* never read before the comma resets it —
                             set anyway so the flag has one owner */
            continue;
        }
        if (c == '{' || c == '[') {
            if (depth >= YEP_JSON_WALK_DEPTH) {
                goto reject;
            }
            kind[depth - 1] = top_kind;
            open_at[depth - 1] = top_open;
            top_kind = c == '[' ? 0 : 1;
            kinds[count] = c == '[' ? YEP_T_SEQ_OPEN : YEP_T_MAP_OPEN;
            offs[count] = 0;
            lens[count] = 0;
            top_open = (uint32_t)count;
            count++;
            top_expect = top_kind ? JW_KEY_OR_CLOSE : JW_VALUE_OR_CLOSE;
            key_slot = top_kind ? 1 : 0;
            kind[depth] = top_kind;
            open_at[depth] = top_open;
            depth++;
            continue;
        }
        if (c == 't' || c == 'f' || c == 'n') {
            size_t wl = c == 'f' ? 5 : 4;
            if (at + wl > len) {
                goto reject;
            }
            uint32_t got4;
            memcpy(&got4, p + at, 4);
            if (got4 != (c == 't'   ? 0x65757274u /* "true" */
                         : c == 'n' ? 0x6C6C756Eu /* "null" */
                                    : 0x736C6166u /* "fals" */) ||
                (c == 'f' && p[at + 4] != 'e')) {
                goto reject;
            }
            if (at + wl < len) {
                char z = p[at + wl];
                if (z != ' ' && z != '\n' && z != '\r' && z != ',' && z != ']' && z != '}' &&
                    z != ':') {
                    goto reject;
                }
            }
            kinds[count] = c == 'n' ? YEP_T_NULL : (c == 't' ? YEP_T_TRUE : YEP_T_FALSE);
            offs[count] = (uint32_t)at;
            lens[count] = (uint32_t)wl;
            count++;
            prev_end = at + wl;
            top_expect = JW_COMMA_OR_CLOSE;
            key_slot = 0;
            continue;
        }
        goto reject;
    }
    /* trailing whitespace only */
    while (prev_end < len && (p[prev_end] == ' ' || p[prev_end] == '\t' || p[prev_end] == '\n' ||
                              p[prev_end] == '\r')) {
        prev_end++;
    }
    if (prev_end != len) {
        goto reject;
    }
    if (count > len + 2) {
        goto mem;
    }
    t->count = count;
    t->_src = p;
    t->_srclen = len;
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
            /* the token-contract route: stage 1 (vector indexes) + the
             * lean walk. Measured BEHIND the fused walk on json-doc
             * (417-426 vs 526-533: stage 1 at 1408 MB/s + a walk whose
             * arms carry the same grammar cost) — gated OFF until the
             * sized cuts land (the op-table classifier trick, the
             * specialized map/seq loops, stage-1 string closes). The
             * fused walk is the shipped route; the law. */
#if YEP_TOKEN_CONTRACT_ROUTE
            uint32_t* idx = (uint32_t*)malloc((len + 2) * sizeof(uint32_t));
            if (idx == NULL) {
                return YEPTRIS_ERROR_MEMORY;
            }
            size_t nidx = 0;
            if (yep_text_active()->json_stage1(source, len, idx, &nidx)) {
                YeptrisStatus st = tape_walk_idx(source, len, off, idx, nidx, tape);
                free(idx);
                if (st != YEPTRIS_ERROR_PARSE) {
                    return st; /* OK or MEMORY; a reject falls through */
                }
            } else {
                free(idx);
            }
#endif
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
    tape->_src = source;
    tape->_srclen = len;
    tape_ctx c = {tape, len + 2, {0}, 0, 0};
    if (!rec_put(&c, YEP_T_DOC, 0, 0) || !tape_put_root_scalar(&c, source, len, at)) {
        yep_error_set(yep_error_tls(), YEP_ERR_INTERNAL, 0, 0, at,
                      "tape scalar conversion rejected a validated root at byte %zu", at);
        yeptris_tape_free(tape);
        return YEPTRIS_ERROR_INTERNAL;
    }
    return YEPTRIS_OK;
}

YEPTRIS_API size_t yeptris_tape_convert(yeptris_json_tape* t, size_t from, size_t to,
                                        int64_t* ivals, double* dvals) {
    if (t == NULL || t->_src == NULL || from > to || to > t->count) {
        return SIZE_MAX;
    }
    const char* p = (const char*)t->_src;
    size_t n = 0;
    for (size_t i = from; i < to; i++) {
        uint8_t k = t->kinds[i];
        if (k != YEP_T_INT && k != YEP_T_FLOAT) {
            continue;
        }
        size_t j = 0;
        int64_t iv = 0;
        double dv = 0;
        int shape = 0;
        if (!yep_json_number_scan(p + t->offs[i], t->lens[i], &j, &shape, &iv, &dv)) {
            return SIZE_MAX;
        }
        if (shape == 0) {
            if (ivals != NULL) {
                ivals[i] = iv;
            }
        } else {
            if (shape == 2) {
                t->int_min = 1;
            }
            if (dvals != NULL) {
                dvals[i] = dv;
            }
        }
        n++;
    }
    return n;
}

YEPTRIS_API void yeptris_tape_free(yeptris_json_tape* tape) {
    if (tape == NULL) {
        return;
    }
    yep_free(yep_system_allocator(), tape->_block);
    memset(tape, 0, sizeof(*tape));
}
