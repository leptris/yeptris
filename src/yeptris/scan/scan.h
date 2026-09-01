/* scan.h — structure facts from bytes (TODO.impl/06).
 *
 * MECE law: scan answers "where does this line's content start and where
 * do spans end" — never grammar decisions, never allocation. The engine
 * (07) owns grammar. Line facts are produced lazily, one line at a time;
 * no whole-document table (streaming-friendly).
 */
#ifndef YEP_SCAN_H
#define YEP_SCAN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yep_line_info {
    uint32_t offset; /* byte offset of the line start */
    uint32_t end;    /* byte offset of the line break (exclusive content end) */
    uint16_t indent; /* column of the first non-space (== end - offset when blank) */
    uint16_t flags;
    unsigned char first; /* first content byte ('\0' for blank lines) */
} yep_line_info;

enum {
    YEP_LF_BLANK = 1u << 0,     /* only spaces */
    YEP_LF_COMMENT = 1u << 1,   /* first content char is '#' */
    YEP_LF_DOC_START = 1u << 2, /* "---" (then EOL / space / comment) */
    YEP_LF_DOC_END = 1u << 3,   /* "..." (then EOL / space / comment) */
    YEP_LF_DIRECTIVE = 1u << 4, /* '%' at column 0 */
    YEP_LF_TAB = 1u << 5,       /* tab within the indentation (error) */
};

/* Facts for the line starting at pos. Handles \n, \r\n, lone \r breaks. */
yep_line_info yep_scan_line(const char* p, size_t len, size_t pos);

/* What terminated a plain-scalar span. */
typedef enum {
    YEP_TERM_EOF = 0,
    YEP_TERM_EOL,
    YEP_TERM_COMMENT,  /* " #" — span excludes the preceding space */
    YEP_TERM_COLON,    /* ':' followed by blank/EOL — span ends before ':' */
    YEP_TERM_FLOW,     /* flow indicator (flow context only) */
    YEP_TERM_BAD_COLON /* ':' in a position YAML forbids (error context) */
} yep_span_term;

typedef struct yep_span {
    uint32_t start;
    uint32_t end; /* trimmed span end (exclusive) */
    yep_span_term term;
} yep_span;

/* Scans a plain scalar starting at pos (must be content, not a comment).
 * flow != 0 adds flow stop characters; in flow, ':' terminates when
 * followed by blank/EOL/flow indicator. Leading whitespace is NOT
 * consumed. Trailing spaces are trimmed from the span. */
yep_span yep_scan_plain(const char* p, size_t len, size_t pos, int flow);

/* ns-plain-first truth: whether c may START a plain scalar. ','
 * ']' '}' are c-flow-indicators, excluded in every context ('[' '{'
 * never reach a plain start: the engine dispatches flow first). */
int yep_plain_first_ok(unsigned char c);

/* Scans a quoted scalar whose opening quote is at pos. Returns span of
 * the CONTENT (between quotes) and sets *term (EOL on unterminated → the
 * caller errors). q is '\'' or '"'. */
yep_span yep_scan_quoted(const char* p, size_t len, size_t pos, int* has_escape);

/* True if a simple key may start at p[pos] (ns-plain-first approximation
 * plus quote/flow openers). Exposed for the engine's dispatch. */
int yep_scan_is_key_start(unsigned char c);

/* Consumed length of the line break at pos (0 if none): \n -> 1,
 * \r\n -> 2, lone \r -> 1. */
size_t yep_scan_break_len(const char* p, size_t len, size_t pos);

#ifdef __cplusplus
}
#endif

#endif /* YEP_SCAN_H */
