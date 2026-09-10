/* tree_diff.h — the differential comparator shared by the fast-path
 * gates (flow-direct-diff, block-pair-diff): same input, two engine
 * runs, the DOM trees must agree on every node field, link, and
 * alias target; inputs that error must error alike. */

#ifndef YEP_TEST_TREE_DIFF_H
#define YEP_TEST_TREE_DIFF_H

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dom/dom.h"
#include "parse/engine.h"

static char* slurp(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    char* buf = (char*)malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static int str_eq(const yep_dom* d, yep_sview a, yep_sview b) {
    if (a.len != b.len) {
        return 0;
    }
    if (a.len == 0) {
        return 1;
    }
    yep_view va = yep_dom_view(d, a);
    yep_view vb = yep_dom_view(d, b);
    return va.p != NULL && vb.p != NULL && memcmp(va.p, vb.p, va.len) == 0;
}

/* Iterative compare (a permanent gate must not ride the C stack:
 * corpus inputs reach the depth cap). Pairs of node ids are compared
 * in tree order: every field, then children via the sibling links. */
typedef struct {
    uint32_t a, b;
} pair_t;

static int node_eq(const yep_dom* a, const yep_dom* b, uint32_t ra, uint32_t rb) {
    size_t cap = 64, n = 0;
    pair_t* st = (pair_t*)malloc(cap * sizeof(pair_t));
    if (st == NULL) {
        return 0;
    }
    st[n++] = (pair_t){ra, rb};
    int eq = 1;
    while (n > 0 && eq) {
        pair_t pr = st[--n];
        if (pr.a >= a->ncount || pr.b >= b->ncount) {
            fprintf(stderr, "DIFF oob-id a=%u/%u b=%u/%u\n", pr.a, a->ncount, pr.b, b->ncount);
            eq = 0;
            break;
        }
        const yep_dnode* x = &a->nodes[pr.a];
        const yep_dnode* y = &b->nodes[pr.b];
        if (x->kind != y->kind || x->style != y->style || x->implicit != y->implicit ||
            x->tag_id != y->tag_id || x->flow != y->flow || x->line != y->line ||
            x->col != y->col || x->count != y->count) {
            fprintf(stderr,
                    "DIFF field a=%u b=%u kind %u/%u style %u/%u impl %u/%u tag %u/%u flow "
                    "%u/%u line %u/%u col %u/%u cnt %u/%u\n",
                    pr.a, pr.b, x->kind, y->kind, x->style, y->style, x->implicit, y->implicit,
                    x->tag_id, y->tag_id, x->flow, y->flow, x->line, y->line, x->col, y->col,
                    x->count, y->count);
            eq = 0;
            break;
        }
        if (!str_eq(a, x->value, y->value) || !str_eq(a, x->tag, y->tag) ||
            !str_eq(a, x->anchor, y->anchor)) {
            yep_view xv = yep_dom_view(a, x->value);
            yep_view yv = yep_dom_view(a, y->value);
            fprintf(stderr, "DIFF str a=%u b=%u vlen %u/%u v=%.*s|%.*s anc %u/%u\n", pr.a, pr.b,
                    xv.len, yv.len, (int)xv.len, xv.p ? (const char*)xv.p : "", (int)yv.len,
                    yv.p ? (const char*)yv.p : "", x->anchor.len, y->anchor.len);
            eq = 0;
            break;
        }
        if (x->kind == YEP_DOM_ALIAS) {
            if (x->target >= a->ncount || y->target >= b->ncount) {
                fprintf(stderr, "DIFF oob-target a=%u t=%u/%u b=%u t=%u/%u\n", pr.a, x->target,
                        a->ncount, pr.b, y->target, b->ncount);
                eq = 0;
                break;
            }
            const yep_dnode* xt = &a->nodes[x->target];
            const yep_dnode* yt = &b->nodes[y->target];
            if (xt->kind != yt->kind || xt->line != yt->line || xt->col != yt->col ||
                xt->value.len != yt->value.len) {
                eq = 0;
                break;
            }
        }
        if (n + (size_t)x->count >= cap) {
            while (n + (size_t)x->count >= cap) {
                cap *= 2;
            }
            pair_t* ns = (pair_t*)realloc(st, cap * sizeof(pair_t));
            if (ns == NULL) {
                free(st);
                return 0;
            }
            st = ns;
        }
        uint32_t ca = x->first_child;
        uint32_t cb = y->first_child;
        while (ca != UINT32_MAX && cb != UINT32_MAX) {
            st[n++] = (pair_t){ca, cb};
            ca = a->nodes[ca].next_sibling;
            cb = b->nodes[cb].next_sibling;
        }
        if (ca != UINT32_MAX || cb != UINT32_MAX) {
            fprintf(stderr, "DIFF chain-tail a=%u ca=%u cb=%u\n", pr.a, ca, cb);
            eq = 0; /* child-count mismatch already caught by count, kept safe */
        }
    }
    free(st);
    return eq;
}

static int dom_eq(const yep_dom* a, const yep_dom* b) {
    if (a->dcount != b->dcount) {
        return 0;
    }
    for (uint32_t i = 0; i < a->dcount; i++) {
        if (a->docs[i] == UINT32_MAX || b->docs[i] == UINT32_MAX ||
            !node_eq(a, b, a->docs[i], b->docs[i])) {
            return 0;
        }
    }
    return 1;
}

typedef void (*tree_diff_fn)(const char* name, const char* buf, size_t len);

static void tree_diff_walk(const char* dir, tree_diff_fn fn) {
    DIR* d = opendir(dir);
    if (d == NULL) {
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        int is_in = n >= 3 && strcmp(ent->d_name + n - 3, ".in") == 0;
        int is_yaml = n >= 5 && strcmp(ent->d_name + n - 5, ".yaml") == 0;
        if (!is_in && !is_yaml) {
            continue;
        }
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        size_t len = 0;
        char* buf = slurp(path, &len);
        if (buf != NULL) {
            fn(path, buf, len);
            free(buf);
        }
    }
    closedir(d);
}

#endif /* YEP_TEST_TREE_DIFF_H */
