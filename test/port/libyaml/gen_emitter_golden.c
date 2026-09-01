/* gen_emitter_golden.c — one-time EMITTER goldens (TODO.impl/17B).
 *
 * libyaml parses each input and re-emits it through its own emitter;
 * the bytes are committed as <name>.ly. The runner then proves
 * yeptris's serialization is semantically equal to libyaml's
 * emission (both re-parsed and event-dumped) — emitter parity
 * against the reference, not against ourselves.
 *
 * Usage: gen_emitter_golden <snapshots-dir>
 */
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yaml.h>

static int emit_with_libyaml(const char* in, size_t len, char** out, size_t* out_len) {
    yaml_parser_t p;
    yaml_emitter_t e;
    if (!yaml_parser_initialize(&p) || !yaml_emitter_initialize(&e)) {
        yaml_parser_delete(&p);
        return 0;
    }
    yaml_parser_set_input_string(&p, (const unsigned char*)in, len);
    *out = malloc(len * 4 + 4096);
    yaml_emitter_set_output_string(&e, (unsigned char*)*out, len * 4 + 4096, out_len);
    int rc = 1;
    for (;;) {
        yaml_event_t ev;
        if (!yaml_parser_parse(&p, &ev)) {
            fprintf(stderr, "parse fail: %s\n", p.problem ? p.problem : "?");
            rc = 0;
            break;
        }
        int last = (ev.type == YAML_STREAM_END_EVENT);
        if (!yaml_emitter_emit(&e, &ev)) {
            fprintf(stderr, "emit fail: %s\n", e.problem ? e.problem : "?");
            rc = 0;
            break;
        }
        if (last) {
            break;
        }
    }
    yaml_emitter_flush(&e);
    yaml_emitter_delete(&e);
    yaml_parser_delete(&p);
    return rc;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: gen_emitter_golden <snapshots-dir>\n");
        return 2;
    }
    DIR* d = opendir(argv[1]);
    if (d == NULL) {
        fprintf(stderr, "no dir %s\n", argv[1]);
        return 2;
    }
    struct dirent* ent;
    long total = 0, emitted = 0, skipped = 0;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 3 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[1], ent->d_name);
        FILE* f = fopen(path, "rb");
        if (f == NULL) {
            continue;
        }
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* in = malloc((size_t)n + 1);
        size_t got = fread(in, 1, (size_t)n, f);
        fclose(f);
        in[got] = '\0';
        total++;
        char* out = NULL;
        size_t out_len = 0;
        if (!emit_with_libyaml(in, got, &out, &out_len)) {
            skipped++; /* libyaml rejects or emitter-fails: not golden material */
            free(out);
            free(in);
            continue;
        }
        char opath[1024];
        snprintf(opath, sizeof(opath), "%s/%.*s.ly", argv[1], (int)(nl - 3), ent->d_name);
        FILE* o = fopen(opath, "wb");
        fwrite(out, 1, out_len, o);
        fclose(o);
        emitted++;
        free(out);
        free(in);
    }
    closedir(d);
    printf("emitter goldens: %ld/%ld emitted, %ld skipped\n", emitted, total, skipped);
    return 0;
}
