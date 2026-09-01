/* writer.h — internal emitter engine (TODO.impl/13). */
#ifndef YEP_EMIT_WRITER_H
#define YEP_EMIT_WRITER_H

#include <stdint.h>

#include "../common/nametab.h"
#include "../doc.h"
#include "../dom/dom.h"

/* Streaming high-water mark: the writer flushes when len crosses it
 * (append-only output makes any point safe; 13C). */
#define YEP_EMIT_WATERMARK (1u << 16)

typedef int (*yep_emit_flush)(void* ctx, const char* bytes, size_t len);

typedef struct yep_writer {
    char* p;             /* output (wet) */
    size_t len;          /* bytes written (or counted, dry) */
    int dry;             /* 1: size only */
    char last;           /* last byte written ('\0' at start) — dry-safe */
    int force_flow;      /* complex-key emission: collections render flow */
    yep_emit_flush sink; /* 13C: streaming flush (NULL = buffered) */
    void* sink_ctx;
    size_t watermark;
    size_t flushed;   /* total bytes handed to the sink */
    int sink_aborted; /* a sink returned nonzero: stop, report 0 */
    int canonical;    /* 13B: fixed canonical form — flow collections,
                       * quoted strings, typed words, shortest floats */
    int json;         /* 21: JSON output — canonical minus YAML words:
                       * null (not ~), no document markers, JSON escapes */
} yep_writer;

typedef struct yep_emitter {
    const yeptris_document* doc;
    yep_writer w;
    yep_nametab canon_names; /* canonical anchor renaming: view -> index */
} yep_emitter;

/* One pass over the document set. dry=1 counts exactly. */
size_t yep_emit_run(yep_emitter* em, int dry);

#endif /* YEP_EMIT_WRITER_H */
