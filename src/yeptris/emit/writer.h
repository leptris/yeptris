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
    int canonical;
    int explicit_doc_start;
    /* every document carries the --- marker */ /* 13B: fixed canonical form — flow collections,
                                                 * quoted strings, typed words, shortest floats
                                                 */
    int json;                                   /* 21: JSON output — canonical minus YAML words:
                                                 * null (not ~), no document markers, JSON escapes */
    int json_compact;                           /* json-c to_json_string: no padding spaces */
    int json_pretty;                            /* json-c PRETTY: 2-space indent, per-entry lines */
    int pretty_depth;                           /* json_pretty nesting level (indent = 2*depth) */
    int best_width;                             /* 13B: 0 = default 80; flow lines wrap after a
                                                 * value past this width */
    int col;                                    /* current output column (wr_* maintains) */
    const char* sv_input;                       /* compact-view decode bases (dom regions) */
    const char* sv_arena;
    /* #352: per-scalar route decisions, derived once in the dry
     * sizing pass and consumed by the wet write pass (one byte per
     * scalar, indexed by consumption order; NULL = derive per pass) */
    uint8_t* sc_dec;
    uint32_t sc_i;
    /* #352 slice 2: the growable single-pass route — the exact-sizing
     * dry pass measured 0.54-0.73x of the one-pass stream route on
     * CI-fresh runners, so serialize_ex writes wet into a buffer that
     * doubles on demand (cap/grow), like the streaming window. The
     * fixed-buffer routes keep grow=0 and today's trust-the-count
     * behavior. oom marks a failed growth: writes stop touching memory
     * but the byte count keeps running so callers can fail cleanly. */
    size_t cap;
    int grow;
    int oom;
} yep_writer;

typedef struct yep_emitter {
    const yeptris_document* doc;
    yep_writer w;
    yep_nametab canon_names; /* canonical anchor renaming: view -> index */
} yep_emitter;

/* One pass over the document set. dry=1 counts exactly. */
size_t yep_emit_run(yep_emitter* em, int dry);

/* Compact single-line JSON (internal entries for the json-c compat
 * layer's to_json_string forms). pretty selects json-c's
 * JSON_C_TO_STRING_PRETTY layout: newline + 2-space indent per
 * nesting level, one entry per line. */
char* yep_serialize_json_compact(const yeptris_document* doc, size_t* len);
char* yep_serialize_json_pretty(const yeptris_document* doc, size_t* len);

#endif /* YEP_EMIT_WRITER_H */
