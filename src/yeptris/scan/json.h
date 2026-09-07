/* json.h — the strict-JSON grammar interface (TODO.impl/08C/21).
 *
 * Kernels are exported (YEPTRIS_API) so host materializers (the Ruby
 * native extension's fused JSON→VALUE path) can call them without
 * pulling the whole static archive. */
#ifndef YEP_JSON_H
#define YEP_JSON_H

#include <stddef.h>
#include <yeptris/api.h>

#ifdef __cplusplus
extern "C" {
#endif

YEPTRIS_API int yep_json_ws(const char* p, size_t len, size_t* i, int* saw_tab);
YEPTRIS_API int yep_json_number(const char* p, size_t len, size_t* i);
YEPTRIS_API int yep_json_literal(const char* p, size_t len, size_t* i, const char* word);
YEPTRIS_API int yep_json_string(const char* p, size_t len, size_t* i, size_t* close_out, int* has_esc);
YEPTRIS_API int yep_json_document(const char* p, size_t len, size_t* err);

#ifdef __cplusplus
}
#endif

#endif /* YEP_JSON_H */
