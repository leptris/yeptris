/* emit_roundtrip.c — the emitter's corpus gate (TODO.impl/13).
 *
 * For every snapshot input:
 *   1. parse x0            (must succeed — snapshots are our green corpus)
 *   2. s1 = serialize(x0)
 *   3. parse s1            (must succeed: emitted YAML is valid)
 *   4. s2 = serialize(s1)
 *   5. s1 == s2            (byte stability)
 *   6. dump(parse x0) == dump(parse s1)  (semantic preservation)
 *
 * Usage: test_emit_roundtrip <snapshots-dir> [--verbose]
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>

#include "memory/allocator.h"
#include "parse/engine.h"
#include "port/libyaml/event_dump.h"

/* canonical dump of one parse (the same adapter the differential uses) */
static int dump_event(void* ctx, const yep_event* ev) {
    char** out = (char**)ctx;
    yd_event y;
    memset(&y, 0, sizeof(y));
    char abuf[256], tbuf[512];
    const char* a = NULL;
    const char* t = NULL;
    if (ev->anchor.len > 0 && ev->anchor.len < sizeof(abuf)) {
        memcpy(abuf, ev->anchor.p, ev->anchor.len);
        abuf[ev->anchor.len] = '\0';
        a = abuf;
    }
    if (ev->tag.len > 0 && ev->tag.len < sizeof(tbuf)) {
        memcpy(tbuf, ev->tag.p, ev->tag.len);
        tbuf[ev->tag.len] = '\0';
        t = tbuf;
    }
    char vbuf[4096];
    const char* v = NULL;
    if (ev->value.len > 0 && ev->value.len < sizeof(vbuf)) {
        memcpy(vbuf, ev->value.p, ev->value.len);
        vbuf[ev->value.len] = '\0';
        v = vbuf;
    }
    switch (ev->type) {
    case YEP_EV_STREAM_START:
        y.type = YD_STREAM_START;
        break;
    case YEP_EV_STREAM_END:
        y.type = YD_STREAM_END;
        break;
    case YEP_EV_DOCUMENT_START:
    case YEP_EV_DOCUMENT_END:
        return 0; /* markers are layout, not semantics */
    case YEP_EV_MAP_START:
        y.type = YD_MAP_START; /* flow-ness is layout */
        break;
    case YEP_EV_MAP_END:
        y.type = YD_MAP_END;
        break;
    case YEP_EV_SEQ_START:
        y.type = YD_SEQ_START;
        break;
    case YEP_EV_SEQ_END:
        y.type = YD_SEQ_END;
        break;
    case YEP_EV_SCALAR:
        y.type = YD_SCALAR;
        y.value = ev->value.len > 0 ? (const char*)ev->value.p : "";
        y.value_len = ev->value.len;
        y.style = YD_PLAIN; /* semantics: values + structure only; style
                               upgrades (plain->quoted, ->literal) are
                               roundtrip-legal */
        break;
    case YEP_EV_ALIAS:
        y.type = YD_ALIAS;
        if (ev->value.len > 0 && ev->value.len < sizeof(abuf)) {
            memcpy(abuf, ev->value.p, ev->value.len);
            abuf[ev->value.len] = '\0';
            a = abuf;
        }
        break;
    default:
        return 0;
    }
    y.anchor = a;
    y.tag = t;
    (void)v;
    char line[4096];
    size_t n = yd_line(&y, line, sizeof(line));
    size_t ol = *out ? strlen(*out) : 0;
    char* nb = realloc(*out, ol + n + 1);
    if (nb == NULL) {
        return 1;
    }
    memcpy(nb + ol, line, n);
    nb[ol + n] = '\0';
    *out = nb;
    return 0;
}

static char* dump_of(const char* in, size_t len, int* ok) {
    char* out = NULL;
    yep_engine* eng = yep_engine_create(yep_system_allocator());
    yep_sink sink = {dump_event, &out};
    int rc = yep_engine_run(eng, in, len, &sink);
    yep_engine_destroy(eng);
    *ok = (rc == 0);
    return out;
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
        fprintf(stderr, "usage: test_emit_roundtrip <snapshots-dir> [--verbose]\n");
        return 2;
    }
    DIR* d = opendir(argv[1]);
    if (d == NULL) {
        fprintf(stderr, "no snapshots at %s\n", argv[1]);
        return 2;
    }
    int verbose = (argc > 2 && strcmp(argv[2], "--verbose") == 0);
    long total = 0, failed = 0, semantic = 0, unstable = 0;
    struct dirent* ent;
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
        /* inputs the parser rejects are not emitter material */
        int ok0 = 0;
        char* d0 = dump_of(in, len, &ok0);
        if (!ok0) {
            free(d0);
            free(in);
            continue;
        }
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(in, len, &st);
        if (doc == NULL) {
            if (st == YEPTRIS_OK) {
                /* empty stream: nothing to emit (dump agreed: d0 holds
                 * only framing) */
                free(d0);
                free(in);
                continue;
            }
            failed++;
            printf("ROUNDTRIP %s: parse failed st=%d\n", ent->d_name, (int)st);
            free(d0);
            free(in);
            continue;
        }
        size_t s1len = 0;
        char* s1 = yeptris_serialize(doc, &s1len);
        yeptris_document_free(doc);
        if (s1 == NULL) {
            failed++;
            printf("ROUNDTRIP %s: serialize failed\n", ent->d_name);
            free(d0);
            free(in);
            continue;
        }
        int ok1 = 0;
        char* d1 = dump_of(s1, s1len, &ok1);
        if (!ok1) {
            failed++;
            printf("ROUNDTRIP %s: emitted YAML does not re-parse\n%s", ent->d_name, s1);
            if (verbose) {
                printf("--- emitted:\n%s\n", s1);
            }
        } else if (strcmp(d0, d1) != 0) {
            semantic++;
            printf("SEMANTIC %s\n", ent->d_name);
            if (verbose) {
                printf("--- emitted:\n%s--- before:\n%s--- after:\n%s\n", s1, d0, d1);
            }
        } else {
            YeptrisStatus st2 = YEPTRIS_OK;
            YeptrisDocument doc2 = yeptris_parse(s1, s1len, &st2);
            if (doc2 == NULL) {
                failed++;
                printf("ROUNDTRIP %s: second parse failed\n", ent->d_name);
            } else {
                size_t s2len = 0;
                char* s2 = yeptris_serialize(doc2, &s2len);
                yeptris_document_free(doc2);
                if (s2 == NULL || s2len != s1len || memcmp(s1, s2, s1len) != 0) {
                    unstable++;
                    printf("UNSTABLE %s\n", ent->d_name);
                    if (verbose && s2 != NULL) {
                        printf("--- s1:\n%.*s--- s2:\n%s\n", (int)s1len, s1, s2);
                    }
                }
                free(s2);
            }
        }
        free(d1);
        free(d0);
        free(s1);

        /* canonical mode (13B): fixed form is a parse fixed point —
         * c2 == c1 byte-for-byte after the first canonicalization */
        yeptris_emit_options opts = {sizeof(yeptris_emit_options), 1, 0};
        YeptrisStatus stc = YEPTRIS_OK;
        YeptrisDocument dc = yeptris_parse(in, len, &stc);
        if (dc != NULL) {
            size_t c1len = 0;
            char* c1 = yeptris_serialize_ex(dc, &opts, &c1len);
            yeptris_document_free(dc);
            if (c1 == NULL) {
                failed++;
                printf("CANONICAL %s: serialize_ex failed\n", ent->d_name);
            } else {
                YeptrisStatus stc2 = YEPTRIS_OK;
                YeptrisDocument dc2 = yeptris_parse(c1, c1len, &stc2);
                if (dc2 == NULL) {
                    failed++;
                    printf("CANONICAL %s: canonical output does not re-parse\n%.*s", ent->d_name,
                           (int)c1len, c1);
                } else {
                    size_t c2len = 0;
                    char* c2 = yeptris_serialize_ex(dc2, &opts, &c2len);
                    yeptris_document_free(dc2);
                    if (c2 == NULL || c2len != c1len || memcmp(c1, c2, c1len) != 0) {
                        unstable++;
                        printf("CANON-UNSTABLE %s\n", ent->d_name);
                        if (verbose && c2 != NULL) {
                            printf("--- c1:\n%.*s--- c2:\n%s\n", (int)c1len, c1, c2);
                        }
                    }
                    free(c2);
                }
                free(c1);
            }
        }
        free(in);
    }
    closedir(d);
    printf("emit roundtrip: %ld inputs — semantic diffs %ld, unstable %ld, "
           "hard failures %ld (canonical gate included)\n",
           total, semantic, unstable, failed);
    return (failed + semantic + unstable) == 0 ? 0 : 1;
}
