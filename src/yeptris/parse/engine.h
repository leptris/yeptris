/* engine.h — the one parser engine (TODO.impl/07).
 *
 * Non-recursive block machine + iterative flow kernel, producing events
 * into any sink (DOM builder today; pull/recorder in 12). v1 runs the
 * whole buffer in one call; resumable stepping lands with 12's streaming
 * (recorded as the item's open phase). Depth guard rejects pathological
 * nesting with an error, never a crash.
 */
#ifndef YEP_ENGINE_H
#define YEP_ENGINE_H

#include "common/error.h"
#include "memory/allocator.h"
#include "parse/events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct yep_engine yep_engine;

yep_engine* yep_engine_create(const yep_allocator* sys);
void yep_engine_destroy(yep_engine* e);

/* Parses [buf, buf+len) — must be validated UTF-8 (05). Returns 0 on
 * success, -1 on parse error (see yep_engine_error), -2 sink abort. */
int yep_engine_run(yep_engine* e, const char* buf, size_t len, const yep_sink* sink);

const yep_error* yep_engine_error(const yep_engine* e);

#ifdef __cplusplus
}
#endif

#endif /* YEP_ENGINE_H */
