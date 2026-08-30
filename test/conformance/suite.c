/* suite.c — frontmatter loader: reads src/<id>.yaml fields. */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "suite.h"

static char* read_file(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = malloc((size_t)n + 1);
    if (buf == NULL) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    if (len != NULL) {
        *len = got;
    }
    return buf;
}

/* Extracts one block field. The src shape is:
 *   "- key: |" or "  key: |" then content lines indented deeper.
 * Returns a malloc'd string with the dedented content (single trailing
 * newline, per the suite's `|` clipping), or NULL when absent. */
char* extract_field(const char* text, const char* key) {
    size_t klen = strlen(key);
    const char* p = text;
    while (*p != '\0') {
        const char* eol = strchr(p, '\n');
        if (eol == NULL) {
            eol = p + strlen(p);
        }
        /* "- key:" or "  key:" at low indent */
        const char* line = p;
        size_t ll = (size_t)(eol - p);
        size_t ki = 0;
        if (ll > 2 && (line[0] == '-' || line[0] == ' ') && line[1] == ' ') {
            ki = 2;
        } else if (ll > 0 && line[0] == '-') {
            ki = 1;
        } else {
            ki = 0;
        }
        if (ll > ki + klen + 1 && strncmp(line + ki, key, klen) == 0 && line[ki + klen] == ':') {
            const char* rest = line + ki + klen + 1;
            /* inline scalar value? */
            while (rest < eol && (*rest == ' ' || *rest == '\t')) {
                rest++;
            }
            if (rest < eol && *rest != '|') {
                size_t n = (size_t)(eol - rest);
                char* v = malloc(n + 2);
                memcpy(v, rest, n);
                v[n] = '\n';
                v[n + 1] = '\0';
                return v;
            }
            /* block scalar: consume following deeper-indented lines */
            size_t content_indent = 0;
            const char* q = (eol < p + strlen(p)) ? eol + 1 : eol;
            /* detect indent from first following line */
            const char* r = q;
            while (*r == ' ') {
                r++;
            }
            content_indent = (size_t)(r - q);
            if (content_indent <= 2) {
                return NULL; /* empty block */
            }
            size_t cap = 256, n = 0;
            char* out = malloc(cap);
            while (*q != '\0') {
                const char* ne = strchr(q, '\n');
                size_t nl = ne ? (size_t)(ne - q) : strlen(q);
                /* stop at a line that is blank or indented less */
                int blank = 1;
                for (size_t i = 0; i < nl; i++) {
                    if (q[i] != ' ') {
                        blank = 0;
                        break;
                    }
                }
                size_t ind = 0;
                while (ind < nl && q[ind] == ' ') {
                    ind++;
                }
                if (!blank && ind < content_indent) {
                    break;
                }
                if (ne == NULL) {
                    break;
                }
                if (n + nl + 2 > cap) {
                    while (n + nl + 2 > cap) {
                        cap *= 2;
                    }
                    out = realloc(out, cap);
                }
                size_t take = nl > content_indent ? nl - content_indent : 0;
                const char* srcp = q + (nl > content_indent ? content_indent : nl);
                memcpy(out + n, srcp, take);
                n += take;
                out[n++] = '\n';
                q = ne + 1;
            }
            out[n] = '\0';
            return out;
        }
        p = (*eol == '\0') ? eol : eol + 1;
    }
    return NULL;
}

static int case_cmp(const void* a, const void* b) {
    return strcmp(((const yts_case*)a)->id, ((const yts_case*)b)->id);
}

long yts_load(const char* src_dir, yts_case** out) {
    DIR* d = opendir(src_dir);
    if (d == NULL) {
        return -1;
    }
    long cap = 512, n = 0;
    yts_case* cases = malloc((size_t)cap * sizeof(yts_case));
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl != 9 || strcmp(ent->d_name + 4, ".yaml") != 0) {
            continue;
        }
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", src_dir, ent->d_name);
        char* text = read_file(path, NULL);
        if (text == NULL) {
            continue;
        }
        if (n >= cap) {
            cap *= 2;
            cases = realloc(cases, (size_t)cap * sizeof(yts_case));
        }
        yts_case* c = &cases[n];
        memset(c, 0, sizeof(*c));
        memcpy(c->id, ent->d_name, 4);
        c->id[4] = '\0';
        c->yaml = extract_field(text, "yaml");
        c->tree = extract_field(text, "tree");
        c->fail = extract_field(text, "fail");
        c->error = extract_field(text, "error");
        free(text);
        if (c->yaml != NULL && (c->tree != NULL || c->error != NULL || c->fail != NULL)) {
            n++;
        } else {
            free(c->yaml);
            free(c->tree);
            free(c->fail);
            free(c->error);
        }
    }
    closedir(d);
    qsort(cases, (size_t)n, sizeof(yts_case), case_cmp);
    *out = cases;
    return n;
}

void yts_free(yts_case* cases, long n) {
    for (long i = 0; i < n; i++) {
        free(cases[i].yaml);
        free(cases[i].tree);
        free(cases[i].fail);
        free(cases[i].error);
    }
    free(cases);
}
