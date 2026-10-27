/* emit_diff.c — the emitter differential runner (TODO.impl/17B).
 *
 * For every corpus input with a committed libyaml emission (.ly):
 * libyaml's bytes and yeptris's serialization of the same input must
 * be SEMANTICALLY equal — each is parsed by yeptris and rendered
 * through the shared event_dump; a diff is a divergence. Style and
 * layout may differ; the event stream may not.
 *
 * Usage: test_emit_diff <snapshots-dir> [--verbose]
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>

#include "parse/engine.h"

#include "sem_dump.h"

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
        fprintf(stderr, "usage: emit_diff <snapshots-dir> [--verbose]\n");
        return 2;
    }
    DIR* d = opendir(argv[1]);
    if (d == NULL) {
        fprintf(stderr, "no dir\n");
        return 2;
    }
    int verbose = 0;
    const char* ledger = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else {
            ledger = argv[i];
        }
    }
    char* waiver[512];
    int nwaiver = 0;
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
    long total = 0, diffs = 0, hard = 0, waived = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 6 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
            continue;
        }
        char lypath[1024], inpath[1024];
        snprintf(inpath, sizeof(inpath), "%s/%s", argv[1], ent->d_name);
        snprintf(lypath, sizeof(lypath), "%s/%.*s.ly", argv[1], (int)(nl - 3), ent->d_name);
        size_t ly_len = 0;
        char* ly = read_whole(lypath, &ly_len);
        if (ly == NULL) {
            continue; /* no libyaml golden for this input */
        }
        size_t in_len = 0;
        char* in = read_whole(inpath, &in_len);
        if (in == NULL) {
            free(ly);
            continue;
        }
        char base[256];
        snprintf(base, sizeof(base), "%.*s", (int)(nl - 3), ent->d_name);
        int skip = 0;
        for (int w = 0; w < nwaiver; w++) {
            if (strcmp(waiver[w], base) == 0) {
                skip = 1;
                break;
            }
        }
        if (skip) {
            waived++;
            free(ly);
            free(in);
            continue;
        }
        total++;
        /* yeptris serialization of the input */
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_parse(in, in_len, &st);
        if (doc == NULL) {
            free(ly);
            free(in);
            continue; /* parser-side differential owns parse failures */
        }
        size_t y_len = 0;
        char* y = yeptris_serialize(doc, &y_len);
        yeptris_document_free(doc);
        if (y == NULL) {
            hard++;
            printf("EMIT-FAIL %s\n", ent->d_name);
            free(ly);
            free(in);
            continue;
        }
        int ok1 = 0, ok2 = 0;
        char* dump_ly = yd_sem_dump(ly, ly_len, &ok1);
        char* dump_y = yd_sem_dump(y, y_len, &ok2);
        if (!ok1 || !ok2) {
            hard++;
            printf("REPARSE-FAIL %s (libyaml bytes %d, ours %d)\n", ent->d_name, ok1, ok2);
        } else if (strcmp(dump_ly ? dump_ly : "", dump_y ? dump_y : "") != 0) {
            diffs++;
            printf("DIVERGE %s\n", ent->d_name);
            if (verbose) {
                printf("--- libyaml emission:\n%s--- ours:\n%s--- diff of dumps:\n%s%s\n", ly, y,
                       dump_ly ? dump_ly : "", dump_y ? dump_y : "");
            }
        }
        free(dump_ly);
        free(dump_y);
        free(y);
        free(ly);
        free(in);
    }
    closedir(d);
    for (int w = 0; w < nwaiver; w++) {
        free(waiver[w]);
    }
    printf("emitter differential: %ld inputs — divergences %ld, hard failures %ld, waived %ld\n",
           total, diffs, hard, waived);
    return (diffs + hard) == 0 ? 0 : 1;
}
