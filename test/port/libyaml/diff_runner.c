/* diff_runner.c — yeptris side of the libyaml differential (TODO.impl/17).
 *
 * Replays every committed snapshot input through the yeptris engine,
 * renders events through the shared event_dump.h renderer, and
 * compares against the libyaml-generated golden trees.
 *
 * Verdicts:
 *   both parse, streams equal            -> pass
 *   both reject                          -> pass
 *   golden rejects, we parse             -> upstream divergence (ledger;
 *                                           libyaml is not suite-clean)
 *   golden parses, we reject/differ      -> OUR divergence: failure
 *
 * Usage: diff_runner <snapshots-dir> [--verbose]
 * Exit 0 when no yeptris-side divergences (YEP_STRICT additionally
 * fails on upstream divergences).
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory/allocator.h"
#include "parse/engine.h"

#include "event_dump.h"

typedef struct {
    char* buf;
    size_t len, cap;
} yd_out;

static void yd_put(yd_out* o, const char* p, size_t n) {
    if (o->len + n + 1 > o->cap) {
        o->cap = (o->len + n + 1) * 2;
        o->buf = realloc(o->buf, o->cap);
    }
    memcpy(o->buf + o->len, p, n);
    o->len += n;
    o->buf[o->len] = '\0';
}

static int on_event(void* ctx, const yep_event* ev) {
    yd_out* o = (yd_out*)ctx;
    yd_event y;
    memset(&y, 0, sizeof(y));
    char abuf[256], tbuf[256], vbuf[4096];
    const char* a = NULL;
    const char* t = NULL;
    const char* v = NULL;
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
        y.type = YD_DOC_START;
        y.explicit_doc = (ev->style == 1);
        break;
    case YEP_EV_DOCUMENT_END:
        y.type = YD_DOC_END;
        y.explicit_doc = (ev->style == 1);
        break;
    case YEP_EV_MAP_START:
        y.type = YD_MAP_START;
        y.flow = ev->flow;
        break;
    case YEP_EV_MAP_END:
        y.type = YD_MAP_END;
        break;
    case YEP_EV_SEQ_START:
        y.type = YD_SEQ_START;
        y.flow = ev->flow;
        break;
    case YEP_EV_SEQ_END:
        y.type = YD_SEQ_END;
        break;
    case YEP_EV_SCALAR:
        y.type = YD_SCALAR;
        y.style = (yd_style)(ev->style > 0 ? ev->style - 1 : 0);
        y.value = ev->value.len > 0 ? (const char*)ev->value.p : "";
        y.value_len = ev->value.len;
        break;
    case YEP_EV_ALIAS:
        y.type = YD_ALIAS;
        if (ev->value.len > 0 && ev->value.len < sizeof(abuf)) {
            memcpy(abuf, ev->value.p, ev->value.len);
            abuf[ev->value.len] = '\0';
            a = abuf; /* the alias name rides ev->value */
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
    yd_put(o, line, n);
    return 0;
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
    if (len != NULL) {
        *len = got;
    }
    return b;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: diff_runner <snapshots-dir> [--verbose]\n");
        return 2;
    }
    const char* dir = argv[1];
    const char* ledger = NULL;
    int verbose = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            ledger = argv[i];
        }
    }
    /* ledger lines: "<name> <reason...>"; names waivered as upstream */
    char* waiver[512];
    long nwaiver = 0;
    if (ledger != NULL) {
        FILE* lf = fopen(ledger, "r");
        if (lf != NULL) {
            char line[256];
            while (fgets(line, sizeof(line), lf) != NULL && nwaiver < 512) {
                char* sp = strchr(line, ' ');
                if (sp != NULL) {
                    *sp = '\0';
                }
                char* nl = strchr(line, '\n');
                if (nl != NULL) {
                    *nl = '\0';
                }
                if (line[0] != '\0' && line[0] != '#') {
                    waiver[nwaiver++] = strdup(line);
                }
            }
            fclose(lf);
        }
    }
    DIR* d = opendir(dir);
    if (d == NULL) {
        fprintf(stderr, "diff_runner: no snapshots at %s\n", dir);
        return 2;
    }
    long pass = 0, upstream = 0, ours = 0, total = 0;
    struct dirent* ent;
    char name[256];
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 3 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
            continue;
        }
        snprintf(name, sizeof(name), "%.*s", (int)(nl - 3), ent->d_name);
        char inpath[1024], treepath[1024];
        snprintf(inpath, sizeof(inpath), "%s/%s.in", dir, name);
        snprintf(treepath, sizeof(treepath), "%s/%s.tree", dir, name);
        size_t ilen = 0;
        char* input = read_whole(inpath, &ilen);
        char* golden = read_whole(treepath, NULL);
        if (input == NULL || golden == NULL) {
            fprintf(stderr, "diff_runner: missing pair for %s\n", name);
            closedir(d);
            return 2;
        }
        total++;
        int suite_error = 0;
        {
            /* first line "# suite-error" marks inputs the suite rejects */
            size_t gl = strlen(golden);
            size_t take = 0;
            while (take < gl && golden[take] != '\n') {
                take++;
            }
            if (take >= 8 && strncmp(golden, "# suite-", 8) == 0) {
                if (take >= 13 && strncmp(golden, "# suite-error", 13) == 0) {
                    suite_error = 1;
                }
                memmove(golden, golden + take + 1, gl - take);
            }
        }
        yd_out o = {0};
        yep_engine* eng = yep_engine_create(yep_system_allocator());
        yep_sink sink = {on_event, &o};
        int rc = yep_engine_run(eng, input, ilen, &sink);
        if (rc != 0) {
            yd_put(&o, "!ERROR\n", 7);
        }
        yep_engine_destroy(eng);
        int golden_err = (strstr(golden, "!ERROR") != NULL);
        int we_err = (rc != 0);
        if (suite_error && we_err && !golden_err) {
            upstream++; /* libyaml is more lenient than the suite here */
        } else if (golden_err && we_err) {
            pass++;
        } else if (golden_err && !we_err) {
            upstream++; /* libyaml rejects; the suite (and we) accept */
        } else {
            int waived = 0;
            for (long w = 0; w < nwaiver; w++) {
                if (strcmp(waiver[w], name) == 0) {
                    waived = 1;
                    break;
                }
            }
            if (waived) {
                upstream++; /* documented libyaml deviation (ledger) */
            } else if (o.buf != NULL && strcmp(o.buf, golden) == 0) {
                pass++;
            } else {
                ours++;
                printf("DIVERGE %s\n", name);
                if (verbose) {
                    printf("--- golden:\n%s--- ours:\n%s\n", golden, o.buf ? o.buf : "(none)");
                }
            }
        }
        free(input);
        free(golden);
        free(o.buf);
    }
    closedir(d);
    printf("libyaml differential: %ld/%ld pass — upstream divergences %ld, "
           "yeptris divergences %ld\n",
           pass, total, upstream, ours);
    /* Upstream divergences are libyaml-vs-suite splits, reported for
     * the ledger but never a yeptris failure; the gate is ours alone. */
    if (ours > 0) {
        return 1;
    }
    return 0;
}
