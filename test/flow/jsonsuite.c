#define _POSIX_C_SOURCE 200809L /* strdup: glibc hides it under -std=c11 */
/* jsonsuite.c — JSONTestSuite under YAML semantics (TODO.impl/08C).
 *
 * JSONTestSuite classifies by RFC 8259; yeptris is a YAML 1.2 parser,
 * so the pinned verdict file (jsonsuite-expected.txt) records OUR
 * classification: JSON-invalid-but-YAML-valid inputs accept; the three
 * known spec splits (DEL, U+FFFF: printable in RFC 8259, not in YAML
 * 1.2 c-printable) reject. The gate is behavioral: no drift from the
 * pinned verdicts, either direction.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/json.h>

typedef struct {
    char* name;
    int accept;
} verdict;

int main(int argc, char** argv) {
    int strict = (argc > 1 && strcmp(argv[1], "--strict") == 0);
    if (strict) {
        argv++;
        argc--;
    }
    if (argc < 3) {
        fprintf(stderr, "usage: jsonsuite [--strict] <test_parsing-dir> <expected-file>\n");
        return 2;
    }
    /* strict mode ignores the verdict file: RFC 8259 rules apply
     * directly — y_ must accept, n_ must reject, i_ recorded only */
    FILE* ef = fopen(argv[2], "r");
    if (ef == NULL) {
        fprintf(stderr, "jsonsuite: no expected file at %s\n", argv[2]);
        return 2;
    }
    verdict v[512];
    int nv = 0;
    char line[256];
    while (fgets(line, sizeof(line), ef) != NULL && nv < 512) {
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        }
        char* sp = strchr(line, ' ');
        if (sp == NULL) {
            continue;
        }
        *sp = '\0';
        v[nv].name = strdup(line);
        v[nv].accept = (sp[1] == 'a');
        nv++;
    }
    fclose(ef);

    DIR* d = opendir(argv[1]);
    if (d == NULL) {
        fprintf(stderr, "jsonsuite: no cases at %s (run scripts/fetch-corpora.sh)\n", argv[1]);
        return 2;
    }
    int checked = 0, bad = 0;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n < 6 || strcmp(ent->d_name + n - 5, ".json") != 0) {
            continue;
        }
        verdict* mine = NULL;
        if (!strict) {
            for (int i = 0; i < nv; i++) {
                if (strcmp(v[i].name, ent->d_name) == 0) {
                    mine = &v[i];
                    break;
                }
            }
            if (mine == NULL) {
                printf("UNLISTED %s (suite drifted from the pin?)\n", ent->d_name);
                bad++;
                continue;
            }
        } else {
            static verdict sv;
            sv.name = ent->d_name;
            switch (ent->d_name[0]) {
            case 'y':
                sv.accept = 1;
                break;
            case 'n':
                sv.accept = 0;
                break;
            default: /* i_: implementation-defined, non-gating */
                sv.accept = -1;
                break;
            }
            mine = &sv;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);
        FILE* f = fopen(path, "rb");
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc(len > 0 ? (size_t)len : 1);
        if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
            fclose(f);
            free(buf);
            printf("READFAIL %s\n", ent->d_name);
            bad++;
            continue;
        }
        fclose(f);
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = strict ? yeptris_parse_json(buf, (size_t)len, &st)
                                     : yeptris_parse(buf, (size_t)len, &st);
        int accept = (doc != NULL);
        yeptris_document_free(doc);
        free(buf);
        if (mine->accept < 0) {
            continue; /* i_: informational only */
        }
        checked++;
        if (accept != mine->accept) {
            printf("DIVERGE %s: pinned %s, now %s\n", ent->d_name,
                   mine->accept ? "accept" : "reject", accept ? "accept" : "reject");
            bad++;
        }
    }
    closedir(d);
    for (int i = 0; i < nv; i++) {
        free(v[i].name);
    }
    printf("json suite: %d/%d match pinned verdicts, %d divergences\n", checked - bad, checked,
           bad);
    return bad == 0 ? 0 : 1;
}
