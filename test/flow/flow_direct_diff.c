/* flow_direct_diff.c — the flow fast-path differential (TODO.restructure/50).
 *
 * PERMANENT gate: the same input runs through the engine twice — once
 * into the DOM sink WITHOUT on_flow_json (event-built), once WITH it
 * (direct-built) — and the trees must agree on every node field,
 * link, and alias target. Inputs that error must error on both paths
 * alike. Walks every corpus file given on the command line plus the
 * built-in edge cases.
 */

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dom/dom.h"
#include "parse/engine.h"

static int g_fail = 0;
static int g_cases = 0;
static int g_flow_cases = 0;

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

/* counts fast-path uses so the differential proves the arm is live.
 * The sink ctx is the DOM itself (the event callback shares it), so
 * the counter lives at file scope — single-threaded harness. */
static int g_flow_hits_run = 0;
static int counting_on_flow_json(void* ctx, const char* p, size_t open, size_t close, uint32_t line,
                                 size_t line_start, yep_view anchor, yep_view tag,
                                 uint32_t anchor_id) {
    g_flow_hits_run++;
    return dom_on_flow_json(ctx, p, open, close, line, line_start, anchor, tag, anchor_id);
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

static int node_eq(const yep_dom* a, const yep_dom* b, uint32_t ia, uint32_t ib, int depth) {
    if (depth > YEP_DOM_MAX_DEPTH) {
        return 0;
    }
    const yep_dnode* x = &a->nodes[ia];
    const yep_dnode* y = &b->nodes[ib];
    if (x->kind != y->kind || x->style != y->style || x->implicit != y->implicit ||
        x->tag_id != y->tag_id || x->flow != y->flow || x->line != y->line || x->col != y->col ||
        x->count != y->count) {
        return 0;
    }
    if (!str_eq(a, x->value, y->value) || !str_eq(a, x->tag, y->tag) ||
        !str_eq(a, x->anchor, y->anchor)) {
        return 0;
    }
    if (x->kind == YEP_DOM_ALIAS) {
        const yep_dnode* xt = &a->nodes[x->target];
        const yep_dnode* yt = &b->nodes[y->target];
        if (xt->kind != yt->kind || xt->line != yt->line || xt->col != yt->col ||
            xt->value.len != yt->value.len) {
            return 0;
        }
    }
    uint32_t ca = x->first_child;
    uint32_t cb = y->first_child;
    while (ca != UINT32_MAX || cb != UINT32_MAX) {
        if (ca == UINT32_MAX || cb == UINT32_MAX) {
            return 0;
        }
        if (!node_eq(a, b, ca, cb, depth + 1)) {
            return 0;
        }
        ca = a->nodes[ca].next_sibling;
        cb = b->nodes[cb].next_sibling;
    }
    return 1;
}

static int dom_eq(const yep_dom* a, const yep_dom* b) {
    if (a->dcount != b->dcount) {
        return 0;
    }
    for (uint32_t i = 0; i < a->dcount; i++) {
        if (a->docs[i] == UINT32_MAX || b->docs[i] == UINT32_MAX ||
            !node_eq(a, b, a->docs[i], b->docs[i], 0)) {
            return 0;
        }
    }
    return 1;
}

static void diff_one(const char* name, const char* buf, size_t len, int want_flow) {
    g_cases++;
    const yep_allocator* sys = yep_system_allocator();

    yep_engine* e1 = yep_engine_create(sys);
    yep_engine* e2 = yep_engine_create(sys);
    yep_dom* d1 = yep_dom_create(sys);
    yep_dom* d2 = yep_dom_create(sys);
    g_flow_hits_run = 0;
    if (e1 == NULL || e2 == NULL || d1 == NULL || d2 == NULL) {
        fprintf(stderr, "DIFF %s: setup OOM\n", name);
        g_fail++;
        goto out;
    }
    d1->input_base = buf;
    d2->input_base = buf;

    yep_sink ev_only = {yep_dom_on_event, d1, NULL};
    yep_sink direct = {yep_dom_on_event, d2, counting_on_flow_json};
    int rc1 = yep_engine_run(e1, buf, len, &ev_only);
    int rc2 = yep_engine_run(e2, buf, len, &direct);

    if ((rc1 == 0) != (rc2 == 0)) {
        fprintf(stderr, "DIFF %s: rc %d vs %d\n", name, rc1, rc2);
        g_fail++;
        goto out;
    }
    if (rc1 != 0) {
        goto out; /* both errored: identical */
    }
    if (!dom_eq(d1, d2)) {
        fprintf(stderr, "DIFF %s: trees diverge (ncount %u vs %u)\n", name, d1->ncount, d2->ncount);
        g_fail++;
        goto out;
    }
    if (want_flow && g_flow_hits_run == 0) {
        fprintf(stderr, "DIFF %s: expected the fast path to fire\n", name);
        g_fail++;
    }
    g_flow_cases += g_flow_hits_run;
out:
    yep_dom_destroy(d1);
    yep_dom_destroy(d2);
    yep_engine_destroy(e1);
    yep_engine_destroy(e2);
}

static const char* k_must_fire[] = {
    /* spans past the 24-byte fast minimum; the fast path must fire
     * and the tree must match the event path exactly */
    "- {\"id\": 7, \"name\": \"alpha\", \"vals\": [1, 2, 3], \"ok\": true}\n",
    "k: {\"a\": 1, \"b\": [true, false, null]}\n",
    "k: [1, -2.5e10, 0.5, -0, 1E+2, 3.14159]\n",
    "a: &x0 {\"n\": 1, \"deep\": [1, 2]}\nb: *x0\n",
    "k: [[[[1, 2]]], [[3, 4]], [[5, 6]]]\n",
    "k: {\"n\": {\"e\": {\"s\": [{\"d\": [0, 1]}]}}}\n",
    "k:  {  \"sp\" :  1,  \"t\": 2  }  \n",
    "k: {\"esc\": \"a\\\"b\\\\c\\n\\t\\u0041\", \"z\": \"tail\"}\n",
    "k: {\"u\": \"\\u00e9\\u65e5\\u65e5\\u672c\", \"v\": 1}\n",
    "k:\n  - {\"m\": 1, \"x\": [1]}\n  - {\"m\": 2, \"x\": [2]}\n",
    "top:\n  k: {\"deep\": {\"x\": 1}, \"y\": [1]}\n",
    "k: [1, 2, 3, 4, 5] # trailing comment\n",
    "k: {\n  \"a\": 1,\n  \"b\": 2\n}\n",
    "k: [\n 1,\n 2,\n 3\n]\n",
    "[{\"a\":[1,{\"b\":2}],\"c\":\"d\"}]\n",
    "k: [ true, false, null, 12, 3.5, \"s\" ]\n",
    NULL,
};

static const char* k_fallback[] = {
    /* no fire (too small, or not JSON-class); trees still identical */
    "k: []\nk2: {}\n",
    "k: [[[[1]]]]\n",
    "k: 5",
    "k: [1]\n",
    "k: [1, 2,]\n",       /* trailing comma: general kernel */
    "k: {a: 1}\n",        /* YAML-style: general kernel */
    "k: [&a 1]\n",        /* anchor inside: general kernel */
    "k: [*a]\na: &a 1\n", /* alias inside: general kernel */
    "k: {\"a\": 1}: v\n", /* flow key: pre-scan path */
    NULL,
};

static void walk_dir(const char* dir) {
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
            diff_one(path, buf, len, 0);
            free(buf);
        }
    }
    closedir(d);
}

int main(int argc, char** argv) {
    for (int i = 0; k_must_fire[i] != NULL; i++) {
        diff_one(k_must_fire[i], k_must_fire[i], strlen(k_must_fire[i]), 1);
    }
    for (int i = 0; k_fallback[i] != NULL; i++) {
        diff_one(k_fallback[i], k_fallback[i], strlen(k_fallback[i]), 0);
    }
    for (int i = 1; i < argc; i++) {
        walk_dir(argv[i]);
    }
    printf("flow-direct-diff: %d cases, %d fast-path subtrees, %d failures\n", g_cases,
           g_flow_cases, g_fail);
    return g_fail != 0;
}
