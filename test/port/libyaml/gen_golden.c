/* gen_golden.c — one-time golden generator (TODO.impl/17).
 *
 * Links libyaml and renders its event stream through event_dump.h,
 * writing <name>.in (exact parsed bytes) and <name>.tree (the dump)
 * per input. The snapshots are committed; CI compares the yeptris
 * runner against them without libyaml present.
 *
 * Usage:
 *   gen_golden <out-dir> --suite <yaml-test-suite-src>
 *   gen_golden <out-dir> --file <name> <path>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <yaml.h>

#include "../../conformance/suite.h"
#include "event_dump.h"

static void write_out(const char* dir, const char* name, const char* ext, const void* data,
                      size_t len) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s%s", dir, name, ext);
    FILE* f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr, "gen_golden: cannot write %s\n", path);
        exit(2);
    }
    fwrite(data, 1, len, f);
    fclose(f);
}

/* Runs libyaml over the input, appending the dump to out. */
static int libyaml_dump(const char* input, size_t len, FILE* out) {
    yaml_parser_t p;
    yaml_event_t ev;
    if (!yaml_parser_initialize(&p)) {
        return -1;
    }
    yaml_parser_set_input_string(&p, (const unsigned char*)input, len);
    int rc = 0;
    for (;;) {
        if (!yaml_parser_parse(&p, &ev)) {
            rc = -1;
            break;
        }
        yd_event y;
        memset(&y, 0, sizeof(y));
        int done = 0;
        switch (ev.type) {
        case YAML_STREAM_START_EVENT:
            y.type = YD_STREAM_START;
            break;
        case YAML_STREAM_END_EVENT:
            y.type = YD_STREAM_END;
            done = 1;
            break;
        case YAML_DOCUMENT_START_EVENT:
            y.type = YD_DOC_START;
            y.explicit_doc = !ev.data.document_start.implicit;
            break;
        case YAML_DOCUMENT_END_EVENT:
            y.type = YD_DOC_END;
            y.explicit_doc = !ev.data.document_end.implicit;
            break;
        case YAML_MAPPING_START_EVENT:
            y.type = YD_MAP_START;
            y.flow = (ev.data.mapping_start.style == YAML_FLOW_MAPPING_STYLE);
            y.anchor = (const char*)ev.data.mapping_start.anchor;
            y.tag = (const char*)ev.data.mapping_start.tag;
            break;
        case YAML_MAPPING_END_EVENT:
            y.type = YD_MAP_END;
            break;
        case YAML_SEQUENCE_START_EVENT:
            y.type = YD_SEQ_START;
            y.flow = (ev.data.sequence_start.style == YAML_FLOW_SEQUENCE_STYLE);
            y.anchor = (const char*)ev.data.sequence_start.anchor;
            y.tag = (const char*)ev.data.sequence_start.tag;
            break;
        case YAML_SEQUENCE_END_EVENT:
            y.type = YD_SEQ_END;
            break;
        case YAML_SCALAR_EVENT:
            y.type = YD_SCALAR;
            y.anchor = (const char*)ev.data.scalar.anchor;
            y.tag = (const char*)ev.data.scalar.tag;
            y.value = (const char*)ev.data.scalar.value;
            y.value_len = ev.data.scalar.length;
            switch (ev.data.scalar.style) {
            case YAML_SINGLE_QUOTED_SCALAR_STYLE:
                y.style = YD_SINGLE;
                break;
            case YAML_DOUBLE_QUOTED_SCALAR_STYLE:
                y.style = YD_DOUBLE;
                break;
            case YAML_LITERAL_SCALAR_STYLE:
                y.style = YD_LITERAL;
                break;
            case YAML_FOLDED_SCALAR_STYLE:
                y.style = YD_FOLDED;
                break;
            default:
                y.style = YD_PLAIN;
                break;
            }
            break;
        case YAML_ALIAS_EVENT:
            y.type = YD_ALIAS;
            y.anchor = (const char*)ev.data.alias.anchor;
            break;
        default:
            break;
        }
        char line[4096];
        size_t n = yd_line(&y, line, sizeof(line));
        fwrite(line, 1, n, out);
        yaml_event_delete(&ev);
        if (done) {
            break;
        }
    }
    if (rc != 0) {
        fputs("!ERROR\n", out);
    }
    yaml_parser_delete(&p);
    return rc;
}

static int g_suite_error; /* set per case: the suite rejects this input */

static void run_one(const char* dir, const char* name, const char* input, size_t len) {
    write_out(dir, name, ".in", input, len);
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.tree", dir, name);
    FILE* out = fopen(path, "wb");
    if (out == NULL) {
        fprintf(stderr, "gen_golden: cannot write %s\n", path);
        exit(2);
    }
    fputs(g_suite_error ? "# suite-error\n" : "# suite-ok\n", out);
    libyaml_dump(input, len, out);
    fclose(out);
}

static void read_whole(const char* path, char** buf, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr, "gen_golden: cannot read %s\n", path);
        exit(2);
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* b = malloc((size_t)n + 1);
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    *buf = b;
    *len = got;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: gen_golden <out-dir> --suite <src> | --file <name> <path>\n");
        return 1;
    }
    const char* dir = argv[1];
    mkdir(dir, 0777);
    if (strcmp(argv[2], "--file") == 0 && argc == 5) {
        char* buf;
        size_t len;
        read_whole(argv[4], &buf, &len);
        g_suite_error = 0;
        run_one(dir, argv[3], buf, len);
        free(buf);
        return 0;
    }
    if (strcmp(argv[2], "--suite") == 0 && argc == 4) {
        yts_case* cases = NULL;
        long n = yts_load(argv[3], &cases);
        if (n <= 0) {
            fprintf(stderr, "gen_golden: no cases at %s\n", argv[3]);
            return 2;
        }
        for (long i = 0; i < n; i++) {
            char* input = yts_case_input(&cases[i]);
            g_suite_error = (cases[i].fail != NULL || cases[i].error != NULL);
            run_one(dir, cases[i].id, input, strlen(input));
            free(input);
        }
        yts_free(cases, n);
        return 0;
    }
    fprintf(stderr, "usage: gen_golden <out-dir> --suite <src> | --file <name> <path>\n");
    return 1;
}
