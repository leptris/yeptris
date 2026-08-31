/* events_crosscheck.c — one engine, four models, provably identical
 * streams (TODO.impl/12 acceptance): every snapshot input runs through
 * push, pull (single + batch), recorder and iterparse; each model's
 * events render through the SAME canonical renderer (event_dump.h);
 * any difference fails.
 *
 * Usage: test_events_crosscheck <snapshots-dir> [--verbose]
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>

#include "port/libyaml/event_dump.h"

static void yd_from_public(yd_event* y, const YeptrisEvent* e) {
    memset(y, 0, sizeof(*y));
    switch (e->type) {
    case YEPTRIS_EV_STREAM_START:
        y->type = YD_STREAM_START;
        break;
    case YEPTRIS_EV_STREAM_END:
        y->type = YD_STREAM_END;
        break;
    case YEPTRIS_EV_DOCUMENT_START:
        y->type = YD_DOC_START;
        y->explicit_doc = e->explicit_marker;
        break;
    case YEPTRIS_EV_DOCUMENT_END:
        y->type = YD_DOC_END;
        y->explicit_doc = e->explicit_marker;
        break;
    case YEPTRIS_EV_MAPPING_START:
        y->type = YD_MAP_START;
        y->flow = e->flow;
        break;
    case YEPTRIS_EV_MAPPING_END:
        y->type = YD_MAP_END;
        break;
    case YEPTRIS_EV_SEQUENCE_START:
        y->type = YD_SEQ_START;
        y->flow = e->flow;
        break;
    case YEPTRIS_EV_SEQUENCE_END:
        y->type = YD_SEQ_END;
        break;
    case YEPTRIS_EV_SCALAR:
        y->type = YD_SCALAR;
        y->value = e->value ? e->value : "";
        y->value_len = e->value_len;
        switch (e->style) {
        case YEPTRIS_STYLE_SINGLE_QUOTED:
            y->style = YD_SINGLE;
            break;
        case YEPTRIS_STYLE_DOUBLE_QUOTED:
            y->style = YD_DOUBLE;
            break;
        case YEPTRIS_STYLE_LITERAL:
            y->style = YD_LITERAL;
            break;
        case YEPTRIS_STYLE_FOLDED:
            y->style = YD_FOLDED;
            break;
        default:
            y->style = YD_PLAIN;
            break;
        }
        break;
    case YEPTRIS_EV_ALIAS:
        y->type = YD_ALIAS;
        break;
    }
}

/* copies of string fields that yd_line borrows: anchor/tag are owned
 * by the model's storage during rendering — safe: we render inside
 * each model's validity window */
static void yd_render_public(char** out, size_t* len, size_t* cap, const YeptrisEvent* e) {
    yd_event y;
    yd_from_public(&y, e);
    /* alias/anchor/tag need NUL-terminated copies for the renderer */
    char abuf[256], tbuf[512];
    static const char* cached;
    (void)cached;
    if (e->anchor_len > 0 && e->anchor_len < sizeof(abuf)) {
        memcpy(abuf, e->anchor, e->anchor_len);
        abuf[e->anchor_len] = '\0';
        y.anchor = abuf;
    }
    if (e->tag_len > 0 && e->tag_len < sizeof(tbuf)) {
        memcpy(tbuf, e->tag, e->tag_len);
        tbuf[e->tag_len] = '\0';
        y.tag = tbuf;
    }
    if (e->type == YEPTRIS_EV_ALIAS) {
        if (e->value_len < sizeof(abuf)) {
            memcpy(abuf, e->value, e->value_len);
            abuf[e->value_len] = '\0';
            y.anchor = abuf;
        }
    }
    char line[4096];
    size_t n = yd_line(&y, line, sizeof(line));
    if (*len + n + 1 > *cap) {
        *cap = (*len + n + 1) * 2;
        *out = realloc(*out, *cap);
    }
    memcpy(*out + *len, line, n);
    *len += n;
    (*out)[*len] = '\0';
}

typedef struct {
    char* buf;
    size_t len, cap;
} sbuf;

static void sbuf_reset(sbuf* b) {
    b->len = 0;
    if (b->buf != NULL) {
        b->buf[0] = '\0';
    }
}

/* push model */
static int push_cb(void* ctx, const YeptrisEvent* e) {
    yd_render_public(&((sbuf*)ctx)->buf, &((sbuf*)ctx)->len, &((sbuf*)ctx)->cap, e);
    return 0;
}

static void run_push(const char* in, size_t len, sbuf* out) {
    sbuf_reset(out);
    yeptris_push_parse(in, len, push_cb, out);
}

static void run_pull(const char* in, size_t len, sbuf* out) {
    sbuf_reset(out);
    YeptrisPullParser p = yeptris_pull_new(in, len);
    const YeptrisEvent* e;
    while ((e = yeptris_pull_next(p)) != NULL) {
        yd_render_public(&out->buf, &out->len, &out->cap, e);
    }
    yeptris_pull_free(p);
}

static void run_pull_batch(const char* in, size_t len, sbuf* out) {
    sbuf_reset(out);
    YeptrisPullParser p = yeptris_pull_new(in, len);
    YeptrisEvent evs[64];
    size_t n;
    while ((n = yeptris_pull_next_batch(p, evs, 64)) > 0) {
        for (size_t i = 0; i < n; i++) {
            yd_render_public(&out->buf, &out->len, &out->cap, &evs[i]);
        }
    }
    yeptris_pull_free(p);
}

static void run_recorder(const char* in, size_t len, sbuf* out) {
    sbuf_reset(out);
    YeptrisRecorder r = yeptris_recorder_new();
    yeptris_recorder_feed(r, in, len, 1);
    size_t n = 0;
    const YeptrisEventRecord* recs = yeptris_recorder_records(r, &n);
    size_t alen = 0;
    const char* arena = yeptris_recorder_arena(r, &alen);
    for (size_t i = 0; i < n; i++) {
        YeptrisEvent e;
        memset(&e, 0, sizeof(e));
        e.type = (YeptrisEventType)recs[i].type;
        e.style = recs[i].style;
        e.flow = (recs[i].flags & YEPTRIS_EF_FLOW) != 0;
        e.explicit_marker = (recs[i].flags & YEPTRIS_EF_EXPLICIT) != 0;
        e.implicit = (recs[i].flags & YEPTRIS_EF_IMPLICIT) != 0;
        e.line = recs[i].line;
        e.col = recs[i].col;
        if (recs[i].value_len > 0) {
            e.value = arena + recs[i].value_off;
            e.value_len = recs[i].value_len;
        }
        if (recs[i].anchor_len > 0) {
            e.anchor = arena + recs[i].anchor_off;
            e.anchor_len = recs[i].anchor_len;
        }
        if (recs[i].tag_len > 0) {
            e.tag = arena + recs[i].tag_off;
            e.tag_len = recs[i].tag_len;
        }
        yd_render_public(&out->buf, &out->len, &out->cap, &e);
    }
    yeptris_recorder_free(r);
}

static void run_iterparse(const char* in, size_t len, sbuf* out) {
    sbuf_reset(out);
    YeptrisIterparse it = yeptris_iterparse_new(in, len);
    size_t n = 0;
    const YeptrisEvent* evs;
    while ((evs = yeptris_iterparse_next(it, &n)) != NULL) {
        for (size_t i = 0; i < n; i++) {
            yd_render_public(&out->buf, &out->len, &out->cap, &evs[i]);
        }
    }
    yeptris_iterparse_free(it);
}

static char* read_whole(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    *len = got;
    return b;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: test_events_crosscheck <snapshots-dir>\n");
        return 2;
    }
    DIR* d = opendir(argv[1]);
    if (d == NULL) {
        fprintf(stderr, "no snapshots at %s\n", argv[1]);
        return 2;
    }
    long total = 0, failed = 0;
    struct dirent* ent;
    sbuf a = {0}, b = {0}, c = {0}, e = {0};
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 3 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);
        size_t len = 0;
        char* in = read_whole(path, &len);
        if (in == NULL) {
            continue;
        }
        total++;
        run_push(in, len, &a);
        run_pull(in, len, &b);
        run_pull_batch(in, len, &c);
        run_recorder(in, len, &e);
        sbuf f = {0};
        run_iterparse(in, len, &f);
        const char* ref = a.buf ? a.buf : "";
        int bad = 0;
        if (strcmp(b.buf ? b.buf : "", ref) != 0) {
            bad = 1;
        }
        if (strcmp(c.buf ? c.buf : "", ref) != 0) {
            bad = 2;
        }
        if (strcmp(e.buf ? e.buf : "", ref) != 0) {
            bad = 3;
        }
        if (strcmp(f.buf ? f.buf : "", ref) != 0) {
            bad = 4;
        }
        if (bad != 0) {
            failed++;
            printf("CROSSCHECK-DIFF %s (model %d)\n", ent->d_name, bad);
            if (argc > 2 && strcmp(argv[2], "--verbose") == 0) {
                printf("--- push:\n%s--- other:\n%s\n", ref,
                       bad == 1   ? b.buf
                       : bad == 2 ? c.buf
                       : bad == 3 ? e.buf
                                  : f.buf);
            }
        }
        free(in);
        free(f.buf);
    }
    closedir(d);
    free(a.buf);
    free(b.buf);
    free(c.buf);
    free(e.buf);
    printf("events crosscheck: %ld/%ld identical across push, pull, pull-batch, "
           "recorder, iterparse\n",
           total - failed, total);
    return failed == 0 ? 0 : 1;
}
