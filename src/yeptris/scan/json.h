/* json.h — the strict-JSON grammar interface (TODO.impl/08C/21). */

#ifndef YEP_JSON_H
#define YEP_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whitespace/token skip: *i advances past JSON ws; 1 = a token byte
 * waits, 0 = EOF or a tab (the caller decides fallback semantics). */
int yep_json_ws(const char* p, size_t len, size_t* i, int* saw_tab);

/* Strict RFC 8259 number: -?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][-+]?[0-9]+)?
 * followed by a delimiter. *i lands past the last digit. */
int yep_json_number(const char* p, size_t len, size_t* i);

/* Exact literal ("true"/"false"/"null") followed by a delimiter. */
int yep_json_literal(const char* p, size_t len, size_t* i, const char* word);

/* Strict string: no raw breaks, JSON escapes only. One stopset walk
 * decides close/break/escape; *close_out is the closing quote, and
 * *has_esc reports backslashes. */
int yep_json_string(const char* p, size_t len, size_t* i, size_t* close_out, int* has_esc);

/* Whole-document strict validation: optional ws, one value, optional
 * ws, EOF. 1 = strict JSON, else 0 with *err at the violation. */
int yep_json_document(const char* p, size_t len, size_t* err);

#ifdef __cplusplus
}
#endif

#endif /* YEP_JSON_H */
