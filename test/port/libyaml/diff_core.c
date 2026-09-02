/* diff_core.c — the shared differential classifier. */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

#include "diff_core.h"
#include "sem_dump.h"

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

static char* ly_dump(const char* in, size_t n, int* ok) {
    yaml_parser_t p;
    *ok = 0;
    if (!yaml_parser_initialize(&p)) {
        return NULL;
    }
    yaml_parser_set_input_string(&p, (const unsigned char*)in, n);
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
    *ok = !bad;
    if (acc == NULL && !bad) {
        acc = strdup("");
    }
    return acc;
}

yd_verdict yd_diff_classify(const char* in, size_t n, char** yep_dump, char** ly_dump_out) {
    if (yep_dump != NULL) {
        *yep_dump = NULL;
    }
    if (ly_dump_out != NULL) {
        *ly_dump_out = NULL;
    }
    int yok = 0;
    char* ydump = yd_sem_dump(in, n, &yok);
    int lok = 0;
    char* ldump = ly_dump(in, n, &lok);

    yd_verdict v;
    if (yok && lok) {
        v = strcmp(ydump ? ydump : "", ldump ? ldump : "") == 0 ? YD_EQUAL : YD_DIFFER;
    } else if (!yok && !lok) {
        v = YD_BOTH_ERROR;
    } else {
        v = yok ? YD_YEPTRIS_ONLY : YD_LIBYAML_ONLY;
    }
    if (yep_dump != NULL) {
        *yep_dump = ydump;
    } else {
        free(ydump);
    }
    if (ly_dump_out != NULL) {
        *ly_dump_out = ldump;
    } else {
        free(ldump);
    }
    return v;
}

const char* yd_verdict_name(yd_verdict v) {
    switch (v) {
    case YD_EQUAL:
        return "EQUAL";
    case YD_DIFFER:
        return "DIFFER";
    case YD_BOTH_ERROR:
        return "BOTH-ERROR";
    case YD_YEPTRIS_ONLY:
        return "YEPTRIS-ONLY-ACCEPT";
    case YD_LIBYAML_ONLY:
        return "LIBYAML-ONLY-ACCEPT";
    }
    return "?";
}

/* ---- divergence ledger (the documented-devergence SSOT) -----------
 * Same shape diff_runner waives against; the loader lives here so the
 * dev tool and the fuzz gate can never disagree about the rule. */
#include <stdio.h>

static char ledger_ids[256][64];
static int ledger_n = 0;

int yd_ledger_load(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return -1;
    }
    char line[512];
    ledger_n = 0;
    while (fgets(line, sizeof(line), f) != NULL && ledger_n < 256) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        char* sp = strchr(line, ' ');
        size_t len = sp ? (size_t)(sp - line) : strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == ' ')) {
            len--;
        }
        if (len == 0 || len >= sizeof(ledger_ids[0])) {
            continue;
        }
        memcpy(ledger_ids[ledger_n], line, len);
        ledger_ids[ledger_n][len] = '\0';
        ledger_n++;
    }
    fclose(f);
    return ledger_n;
}

int yd_ledger_has(const char* corpus_id) {
    for (int i = 0; i < ledger_n; i++) {
        if (strcmp(ledger_ids[i], corpus_id) == 0) {
            return 1;
        }
    }
    return 0;
}
