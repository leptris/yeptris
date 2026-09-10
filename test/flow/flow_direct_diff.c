/* flow_direct_diff.c — the flow fast-path differential (TODO.restructure/50).
 *
 * PERMANENT gate: the same input runs through the engine twice — once
 * into the DOM sink WITHOUT on_flow_json (event-built), once WITH it
 * (direct-built) — and the trees must agree on every node field,
 * link, and alias target. Inputs that error must error on both paths
 * alike. Walks every corpus file given on the command line plus the
 * built-in edge cases.
 */

#include "tree_diff.h"

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

static int g_fail = 0;
static int g_cases = 0;
static int g_flow_cases = 0;

static void diff_one_impl(const char* name, const char* buf, size_t len, int want_flow) {
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
    d1->input_len = len;
    d2->input_base = buf;
    d2->input_len = len;

    yep_sink ev_only = {yep_dom_on_event, d1, NULL, NULL};
    yep_sink direct = {yep_dom_on_event, d2, counting_on_flow_json, NULL};
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
static int g_want_flow_flag = 0;
static void diff_one(const char* name, const char* buf, size_t len) {
    diff_one_impl(name, buf, len, g_want_flow_flag);
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

int main(int argc, char** argv) {
    for (int i = 0; k_must_fire[i] != NULL; i++) {
        g_want_flow_flag = 1;
        diff_one(k_must_fire[i], k_must_fire[i], strlen(k_must_fire[i]));
    }
    for (int i = 0; k_fallback[i] != NULL; i++) {
        g_want_flow_flag = 0;
        diff_one(k_fallback[i], k_fallback[i], strlen(k_fallback[i]));
    }
    for (int i = 1; i < argc; i++) {
        tree_diff_walk(argv[i], diff_one);
    }
    printf("flow-direct-diff: %d cases, %d fast-path subtrees, %d failures\n", g_cases,
           g_flow_cases, g_fail);
    return g_fail != 0;
}
