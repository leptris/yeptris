/* suite.c — frontmatter loader: reads src/<id>.yaml fields. */

#define _POSIX_C_SOURCE 200809L /* strdup+dirent: glibc hides them under -std=c11 */

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
            /* block scalar: consume following deeper-indented lines. An
             * explicit indicator "|2" pins the indent (key indent 2 + N);
             * otherwise it is detected from the first content line. */
            size_t content_indent = 0;
            int explicit_indent = 0;
            {
                const char* ind = rest;
                while (ind < eol && *ind == ' ') {
                    ind++;
                }
                if (ind < eol && (*ind == '|' || *ind == '>')) {
                    ind++;
                    if (ind < eol && *ind >= '1' && *ind <= '9') {
                        explicit_indent = 2 + (*ind - '0');
                    }
                }
            }
            const char* q = (eol < p + strlen(p)) ? eol + 1 : eol;
            /* detect indent from first following line */
            const char* r = q;
            while (*r == ' ') {
                r++;
            }
            content_indent = (size_t)(r - q);
            if (explicit_indent > 0) {
                content_indent = (size_t)explicit_indent;
            } else if (content_indent <= 2) {
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
        /* One file may hold several test entries ("- name:" / "- fail:");
         * each becomes its own case; later ids gain a #N suffix. */
        long entry = 0;
        const char* p = text;
        while (p != NULL && *p != '\0') {
            const char* eol = strchr(p, '\n');
            if (eol == NULL) {
                eol = p + strlen(p);
            }
            size_t ll = (size_t)(eol - p);
            int entry_key =
                (ll >= 7 && (memcmp(p, "- name:", 7) == 0 || memcmp(p, "- fail:", 7) == 0 ||
                             memcmp(p, "- yaml:", 7) == 0)) ||
                (ll >= 8 && memcmp(p, "- error:", 8) == 0) ||
                (ll >= 9 && memcmp(p, "- brief:", 9) == 0);
            if (entry_key) {
                /* entry starts here; find its end (next entry or EOF) */
                const char* q = eol;
                const char* end = text + strlen(text);
                for (;;) {
                    const char* ne = strchr(q + 1, '\n');
                    if (ne == NULL) {
                        break;
                    }
                    const char* ln = ne + 1;
                    size_t l2 = strcspn(ln, "\n");
                    int next_key = (l2 >= 7 && (memcmp(ln, "- name:", 7) == 0 ||
                                                memcmp(ln, "- fail:", 7) == 0 ||
                                                memcmp(ln, "- yaml:", 7) == 0)) ||
                                   (l2 >= 8 && memcmp(ln, "- error:", 8) == 0) ||
                                   (l2 >= 9 && memcmp(ln, "- brief:", 9) == 0);
                    if (next_key) {
                        end = ln;
                        break;
                    }
                    q = ne;
                }
                size_t len = (size_t)(end - p);
                char* slice = malloc(len + 1);
                memcpy(slice, p, len);
                slice[len] = '\0';
                entry++;
                if (n >= cap) {
                    cap *= 2;
                    cases = realloc(cases, (size_t)cap * sizeof(yts_case));
                }
                yts_case* c = &cases[n];
                memset(c, 0, sizeof(*c));
                memcpy(c->id, ent->d_name, 4);
                if (entry == 1) {
                    c->id[4] = '\0';
                } else {
                    snprintf(c->id + 4, sizeof(c->id) - 4, "#%ld", entry);
                }
                c->yaml = extract_field(slice, "yaml");
                c->tree = extract_field(slice, "tree");
                c->fail = extract_field(slice, "fail");
                c->error = extract_field(slice, "error");
                free(slice);
                if (c->yaml != NULL && (c->tree != NULL || c->error != NULL || c->fail != NULL)) {
                    n++;
                } else {
                    free(c->yaml);
                    free(c->tree);
                    free(c->fail);
                    free(c->error);
                }
                p = (end < text + strlen(text)) ? end : NULL;
            } else {
                p = (*eol == '\0') ? NULL : eol + 1;
            }
        }
        free(text);
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

/* --- input preparation (shared with the libyaml differential, 17) --- */

/* The suite renders significant bytes with glyphs; de-visualize:
 * U+2423 (\xe2\x90\xa3) -> space, U+21B5 (\xe2\x86\xb5) -> NEL break
 * (a NEL-only line is ONE break), an em-dash run + U+00BB -> tab, a
 * lone U+00BB -> tab, and U+220E (\xe2\x88\x8e) terminates input with
 * no trailing newline. */
static char* yts_devisualize(const char* in) {
    char* out = strdup(in);
    for (char* p = out; *p != '\0'; p++) {
        if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x90 &&
            (unsigned char)p[2] == 0xA3) {
            p[0] = ' ';
            memmove(p + 1, p + 3, strlen(p + 3) + 1);
        } else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x86 &&
                   (unsigned char)p[2] == 0xB5) {
            if (p[3] == '\n') {
                p[0] = '\n'; /* a NEL-only line is one break */
                memmove(p + 1, p + 4, strlen(p + 4) + 1);
            } else {
                p[0] = '\n';
                memmove(p + 1, p + 3, strlen(p + 3) + 1);
            }
        } else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x80 &&
                   (unsigned char)p[2] == 0x94) {
            char* q = p + 3;
            while ((unsigned char)q[0] == 0xE2 && (unsigned char)q[1] == 0x80 &&
                   (unsigned char)q[2] == 0x94) {
                q += 3;
            }
            if ((unsigned char)q[0] == 0xC2 && (unsigned char)q[1] == 0xBB) {
                p[0] = '\t';
                memmove(p + 1, q + 2, strlen(q + 2) + 1);
            }
        } else if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0xBB) {
            p[0] = '\t'; /* standalone: a tab at an exact 4-stop column */
            memmove(p + 1, p + 2, strlen(p + 2) + 1);
        } else if ((unsigned char)p[0] == 0xE2 && (unsigned char)p[1] == 0x88 &&
                   (unsigned char)p[2] == 0x8E) {
            p[0] = '\0'; /* EOF marker: nothing follows */
            break;
        }
    }
    return out;
}

char* yts_case_input(const yts_case* c) {
    return yts_devisualize(c->yaml);
}
