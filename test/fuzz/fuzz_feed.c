/* fuzz_feed.c — chunked-feed invariant fuzzer (TODO.impl/19).
 *
 * Property: feeding the recorder the SAME bytes in any chunk split
 * (chunk boundaries derived deterministically from the input) yields
 * the SAME event stream as one whole-buffer feed — every record,
 * byte for byte: kind, style, flags, implicit tag, line, column,
 * value/anchor/tag content. Documents close at --- boundaries as
 * they arrive; chunking is transport, never semantics. Feeding after
 * final, and a NULL chunk with nonzero length, are rejected with
 * ERROR_ARG and never crash.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris/events.h>

/* A drained stream: records with their strings copied out of the
 * per-feed arena (it resets at every feed). */
typedef struct {
    uint8_t type, style, flags, tag_id;
    uint32_t line, col;
    size_t val_off, vlen; /* offsets into the stream's blob (it can
                           * realloc as later feeds drain — never
                           * store raw pointers) */
    size_t anchor_off, alen;
    size_t tag_off, tlen;
} ev_snap;

typedef struct {
    ev_snap* recs;
    size_t n, cap;
    char* blob;
    size_t blob_len, blob_cap;
    YeptrisStatus status;
} ev_stream;

/* Copies the bytes and returns their offset in the blob. */
static size_t blob_put(ev_stream* s, const char* p, size_t len, int* stored) {
    if (len == 0 || p == NULL) {
        *stored = 0;
        return 0;
    }
    if (s->blob_len + len > s->blob_cap) {
        size_t cap = s->blob_cap ? s->blob_cap : 256;
        while (s->blob_len + len > cap) {
            cap *= 2;
        }
        char* nb = realloc(s->blob, cap);
        if (nb == NULL) {
            abort();
        }
        s->blob = nb;
        s->blob_cap = cap;
    }
    memcpy(s->blob + s->blob_len, p, len);
    size_t at = s->blob_len;
    s->blob_len += len;
    *stored = 1;
    return at;
}

/* Snapshots whatever the recorder currently holds (drain per feed). */
static void stream_drain(ev_stream* s, YeptrisRecorder rec) {
    size_t n = 0;
    size_t arena_len = 0;
    const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &n);
    const char* arena = yeptris_recorder_arena(rec, &arena_len);
    for (size_t i = 0; i < n; i++) {
        if (s->n == s->cap) {
            size_t cap = s->cap ? s->cap * 2 : 64;
            ev_snap* nr = realloc(s->recs, cap * sizeof(*nr));
            if (nr == NULL) {
                abort();
            }
            s->recs = nr;
            s->cap = cap;
        }
        ev_snap* out = &s->recs[s->n++];
        out->type = rs[i].type;
        out->style = rs[i].style;
        out->flags = rs[i].flags;
        out->tag_id = rs[i].tag_id;
        out->line = rs[i].line;
        out->col = rs[i].col;
        /* NULL + 0 is UB even when the length is zero */
        const char* v = arena != NULL ? arena + rs[i].value_off : NULL;
        const char* a = arena != NULL ? arena + rs[i].anchor_off : NULL;
        const char* t = arena != NULL ? arena + rs[i].tag_off : NULL;
        int sv = 0, sa = 0, st = 0;
        out->val_off = blob_put(s, v, rs[i].value_len, &sv);
        out->vlen = sv ? rs[i].value_len : 0;
        out->anchor_off = blob_put(s, a, rs[i].anchor_len, &sa);
        out->alen = sa ? rs[i].anchor_len : 0;
        out->tag_off = blob_put(s, t, rs[i].tag_len, &st);
        out->tlen = st ? rs[i].tag_len : 0;
    }
}

static void stream_free(ev_stream* s) {
    free(s->recs);
    free(s->blob);
    memset(s, 0, sizeof(*s));
}

static void whole_run(const uint8_t* data, size_t size, ev_stream* out) {
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        out->status = YEPTRIS_ERROR_MEMORY;
        return;
    }
    out->status = yeptris_recorder_feed(rec, (const char*)data, size, 1);
    stream_drain(out, rec);
    yeptris_recorder_free(rec);
}

static void chunked_run(const uint8_t* data, size_t size, ev_stream* out) {
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        out->status = YEPTRIS_ERROR_MEMORY;
        return;
    }
    /* deterministic chunk sizes from the bytes themselves: a small
     * xorshift over each 8-byte block — the same input always splits
     * the same way, different inputs split differently */
    size_t i = 0;
    while (i < size) {
        uint32_t x = 0x9E3779B9u;
        for (size_t j = 0; j < 8 && i + j < size; j++) {
            x ^= data[i + j] << (j * 3);
        }
        x ^= x >> 15;
        x *= 0x2545F491u;
        size_t take = 1 + (x % 97);
        if (take > size - i) {
            take = size - i;
        }
        YeptrisStatus st = yeptris_recorder_feed(rec, (const char*)data + i, take, 0);
        if (st != YEPTRIS_OK) {
            /* a pre-final rejection must match the whole-buffer verdict:
             * an error inside a complete document is an error, period */
            out->status = st;
            stream_drain(out, rec);
            yeptris_recorder_free(rec);
            return;
        }
        stream_drain(out, rec);
        i += take;
    }
    out->status = yeptris_recorder_feed(rec, "", 0, 1);
    stream_drain(out, rec);
    yeptris_recorder_free(rec);
}

static const char* blob_at(const ev_stream* s, size_t off, size_t len) {
    return (len == 0) ? "" : s->blob + off;
}

static int snap_eq(const ev_stream* wa, const ev_stream* wb, const ev_snap* a, const ev_snap* b) {
    return a->type == b->type && a->style == b->style && a->flags == b->flags &&
           a->tag_id == b->tag_id && a->line == b->line && a->col == b->col && a->vlen == b->vlen &&
           (a->vlen == 0 || memcmp(blob_at(wa, a->val_off, a->vlen),
                                   blob_at(wb, b->val_off, b->vlen), a->vlen) == 0) &&
           a->alen == b->alen &&
           (a->alen == 0 || memcmp(blob_at(wa, a->anchor_off, a->alen),
                                   blob_at(wb, b->anchor_off, b->alen), a->alen) == 0) &&
           a->tlen == b->tlen &&
           (a->tlen == 0 || memcmp(blob_at(wa, a->tag_off, a->tlen),
                                   blob_at(wb, b->tag_off, b->tlen), a->tlen) == 0);
}

static void probe_one(const uint8_t* data, size_t size) {
    ev_stream whole = {0}, chunked = {0};
    whole_run(data, size, &whole);
    chunked_run(data, size, &chunked);
    int bad = whole.status != chunked.status;
    if (!bad && whole.n != chunked.n) {
        bad = 1;
    }
    size_t first_bad = whole.n;
    if (!bad) {
        for (size_t i = 0; i < whole.n; i++) {
            if (!snap_eq(&whole, &chunked, &whole.recs[i], &chunked.recs[i])) {
                bad = 1;
                first_bad = i;
                break;
            }
        }
    }
    if (bad) {
        /* transport leaked into semantics — print the first differing
         * record so the input can be minimized */
        fprintf(stderr, "fuzz_feed: divergence (whole st=%d n=%zu / chunked st=%d n=%zu)\n",
                whole.status, whole.n, chunked.status, chunked.n);
        if (first_bad < whole.n && first_bad < chunked.n) {
            const ev_snap* w = &whole.recs[first_bad];
            const ev_snap* c = &chunked.recs[first_bad];
            fprintf(stderr, "  [%zu] whole: t=%u s=%u f=%u tag=%u %u:%u v=%.*s\n", first_bad,
                    w->type, w->style, w->flags, w->tag_id, w->line, w->col, (int)w->vlen,
                    blob_at(&whole, w->val_off, w->vlen));
            fprintf(stderr, "  [%zu] chunk: t=%u s=%u f=%u tag=%u %u:%u v=%.*s\n", first_bad,
                    c->type, c->style, c->flags, c->tag_id, c->line, c->col, (int)c->vlen,
                    blob_at(&chunked, c->val_off, c->vlen));
        }
        abort();
    }

    /* one recorder takes exactly one final chunk */
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        return;
    }
    (void)yeptris_recorder_feed(rec, (const char*)data, size > 4 ? 4 : size, 0);
    (void)yeptris_recorder_feed(rec, "", 0, 1);
    if (yeptris_recorder_feed(rec, "x", 1, 1) != YEPTRIS_ERROR_ARG) {
        abort();
    }
    if (yeptris_recorder_feed(rec, NULL, 4, 0) != YEPTRIS_ERROR_ARG) {
        abort();
    }
    yeptris_recorder_free(rec);
    stream_free(&whole);
    stream_free(&chunked);
}

#ifdef YEP_FUZZ_STANDALONE

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        char buf[1 << 16];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        probe_one((const uint8_t*)buf, n);
        return 0;
    }
    long files = 0;
    for (int a = 1; a < argc; a++) {
        DIR* d = opendir(argv[a]);
        if (d != NULL) {
            struct dirent* ent;
            while ((ent = readdir(d)) != NULL) {
                char path[2048];
                snprintf(path, sizeof(path), "%s/%s", argv[a], ent->d_name);
                FILE* f = fopen(path, "rb");
                if (f == NULL) {
                    continue;
                }
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
        }
    }
    printf("fuzz_feed: %ld inputs, streams identical across chunkings\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size);
    return 0;
}

#endif
