/* yepdiff.c — one input, both libraries, classified (TODO.impl/17C).
 *
 * A dev tool (not shipped): parses the input with yeptris and with
 * libyaml, renders both event streams through the shared dump format,
 * and classifies: EQUAL / DIFFER / BOTH-ERROR / SPLIT (one accepts,
 * the other rejects). The fuzzer (19) drives it for differential
 * testing; humans use it to minimize reproducers.
 *
 * Usage: yepdiff <input-file>
 * Exit: 0 equal or both-error, 1 differ or split.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

#include "../test/port/libyaml/sem_dump.h"

/* libyaml side rendered into the same dump lines */
static int ly_line(const yaml_event_t* ev, char* out, size_t cap) {
    yd_event y;
    memset(&y, 0, sizeof(y));
    char abuf[256], tbuf[512], vbuf[4096];
    const char *a = NULL, *t = NULL;
    switch (ev->type) {
    case YAML_STREAM_START_EVENT:
        y.type = YD_STREAM_START;
        break;
    case YAML_STREAM_END_EVENT:
        y.type = YD_STREAM_END;
        break;
    case YAML_DOCUMENT_START_EVENT:
    case YAML_DOCUMENT_END_EVENT:
        return 0; /* layout */
    case YAML_MAPPING_START_EVENT:
        y.type = YD_MAP_START;
        goto props;
    case YAML_SEQUENCE_START_EVENT:
        y.type = YD_SEQ_START;
    props:
        if ((const char*)ev->data.sequence_start.anchor != NULL) {
            snprintf(abuf, sizeof(abuf), "%s", (const char*)ev->data.sequence_start.anchor);
            a = abuf;
        }
        if ((const char*)ev->data.sequence_start.tag != NULL) {
            snprintf(tbuf, sizeof(tbuf), "%s", (const char*)ev->data.sequence_start.tag);
            t = tbuf;
        }
        break;
    case YAML_MAPPING_END_EVENT:
        y.type = YD_MAP_END;
        break;
    case YAML_SEQUENCE_END_EVENT:
        y.type = YD_SEQ_END;
        break;
    case YAML_ALIAS_EVENT:
        y.type = YD_ALIAS;
        snprintf(abuf, sizeof(abuf), "%s",
                 (const char*)ev->data.alias.anchor ? (const char*)ev->data.alias.anchor : "");
        a = abuf;
        break;
    case YAML_SCALAR_EVENT: {
        y.type = YD_SCALAR;
        size_t vl = ev->data.scalar.length;
        if (vl >= sizeof(vbuf)) {
            vl = sizeof(vbuf) - 1;
        }
        memcpy(vbuf, (const char*)ev->data.scalar.value, vl);
        vbuf[vl] = '\0';
        y.value = vbuf;
        y.value_len = vl;
        if ((const char*)ev->data.scalar.anchor != NULL) {
            snprintf(abuf, sizeof(abuf), "%s", (const char*)ev->data.scalar.anchor);
            a = abuf;
        }
        if ((const char*)ev->data.scalar.tag != NULL) {
            snprintf(tbuf, sizeof(tbuf), "%s", (const char*)ev->data.scalar.tag);
            t = tbuf;
            if (strcmp(tbuf, "!") == 0) {
                t = NULL; /* non-specific: resolves by style */
            }
        }
        break;
    }
    default:
        return 0;
    }
    y.anchor = a;
    y.tag = t;
    return (int)yd_line(&y, out, cap);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: yepdiff <input-file>\n");
        return 2;
    }
    FILE* f = fopen(argv[1], "rb");
    if (f == NULL) {
        fprintf(stderr, "yepdiff: no file %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* in = malloc((size_t)n + 1);
    if (fread(in, 1, (size_t)n, f) != (size_t)n) {
        return 2;
    }
    fclose(f);

    int yok = 0;
    char* ydump = yd_sem_dump(in, (size_t)n, &yok);

    char* ldump = NULL;
    int lok = 0;
    {
        yaml_parser_t p;
        if (yaml_parser_initialize(&p)) {
            yaml_parser_set_input_string(&p, (const unsigned char*)in, (size_t)n);
            char* acc = NULL;
            size_t alen = 0;
            int bad = 0;
            for (;;) {
                yaml_event_t ev;
                if (!yaml_parser_parse(&p, &ev)) {
                    bad = 1;
                    break;
                }
                char line[4096];
                int ln = ly_line(&ev, line, sizeof(line));
                if (ln > 0) {
                    char* nb = realloc(acc, alen + (size_t)ln + 1);
                    if (nb == NULL) {
                        bad = 1;
                        free(acc);
                        acc = NULL;
                        break;
                    }
                    acc = nb;
                    memcpy(acc + alen, line, (size_t)ln);
                    alen += (size_t)ln;
                    acc[alen] = '\0';
                }
                int last = (ev.type == YAML_STREAM_END_EVENT);
                yaml_event_delete(&ev);
                if (last) {
                    break;
                }
            }
            yaml_parser_delete(&p);
            lok = !bad;
            ldump = acc ? acc : strdup("");
        }
    }

    const char* verdict;
    int rc;
    if (yok && lok) {
        if (strcmp(ydump ? ydump : "", ldump ? ldump : "") == 0) {
            verdict = "EQUAL";
            rc = 0;
        } else {
            verdict = "DIFFER";
            rc = 1;
        }
    } else if (!yok && !lok) {
        verdict = "BOTH-ERROR";
        rc = 0;
    } else {
        verdict = yok ? "YEPTRIS-ONLY-ACCEPT" : "LIBYAML-ONLY-ACCEPT";
        rc = 1;
    }
    printf("%s %s\n", verdict, argv[1]);
    if (rc == 1) {
        printf("--- yeptris (%s):\n%s\n--- libyaml (%s):\n%s\n", yok ? "ok" : "error",
               ydump ? ydump : "(error)", lok ? "ok" : "error", ldump ? ldump : "(error)");
    }
    free(ydump);
    free(ldump);
    free(in);
    return rc;
}
