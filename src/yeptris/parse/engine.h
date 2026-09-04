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
#include "common/simd_text.h"
#include "memory/allocator.h"
#include "memory/pool.h"
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

/* Content-derived pre-sizing before run(): reserves the anchor interner
 * from the stream's '&' occurrences (every anchor definition carries
 * exactly one, so the count never undershoots). */
void yep_engine_prepare(yep_engine* e, const yep_text_stats* st);

/* Resumable stepping (TODO.impl/07): chunks accumulate; every feed
 * parses the complete DOCUMENTS received so far (cut at `---` / `...`
 * boundaries at column 0, outside quotes and flow) and keeps the
 * trailing partial document buffered. final != 0 flushes it and ends
 * the stream. STREAM markers bracket the whole stepped stream, not
 * each step; each document's events arrive exactly once, in order.
 * Returns 0, -1 parse error, -2 sink abort. */
int yep_engine_step(yep_engine* e, const char* chunk, size_t len, int final, const yep_sink* sink);

/* Detaches the finish pool (ownership moves to the caller; the engine
 * allocates a fresh one if reused). Returns NULL when absent. */
yep_pool* yep_engine_detach_pool(yep_engine* e);

/* Implicit-typing schema for the next run (NULL: 1.2 core), and the
 * runtime nesting limit. */
struct yep_resolver;
void yep_engine_set_resolver(yep_engine* e, const struct yep_resolver* r);
void yep_engine_set_max_depth(yep_engine* e, int depth);

/* Byte offset the last run reached (into that run's buffer); used by
 * consumers that stop the engine at a boundary and resume. */
size_t yep_engine_pos(const yep_engine* e);

const yep_error* yep_engine_error(const yep_engine* e);

#ifdef __cplusplus
}
#endif

#endif /* YEP_ENGINE_H */
