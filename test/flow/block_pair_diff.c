/* block_pair_diff.c — the block pair fast-path differential
 * (TODO.restructure/54). PERMANENT gate: the same input runs through
 * the engine twice — sink WITHOUT on_block_pair (event-built) and WITH
 * it (pair-built), both carrying the flow fast path — trees must agree
 * on every node field, link, and alias target. */

#include "tree_diff.h"

static int g_fail = 0;
static int g_cases = 0;
static int g_pair_cases = 0;
static int g_pair_hits = 0;

static int counting_pair(void* ctx, const yep_view* key, const yep_block_value* v, uint32_t line,
                         uint16_t key_col, uint16_t val_col) {
    g_pair_hits++;
    return dom_on_block_pair(ctx, key, v, line, key_col, val_col);
}

static void diff_one_impl(const char* name, const char* buf, size_t len, int want_pair) {
    g_cases++;
    const yep_allocator* sys = yep_system_allocator();
    yep_engine* e1 = yep_engine_create(sys);
    yep_engine* e2 = yep_engine_create(sys);
    yep_dom* d1 = yep_dom_create(sys);
    yep_dom* d2 = yep_dom_create(sys);
    g_pair_hits = 0;
    if (e1 == NULL || e2 == NULL || d1 == NULL || d2 == NULL) {
        fprintf(stderr, "DIFF %s: setup OOM\n", name);
        g_fail++;
        goto out;
    }
    d1->input_base = buf;
    d1->input_len = len;
    d2->input_base = buf;
    d2->input_len = len;

    yep_sink ev_only = {yep_dom_on_event, d1, dom_on_flow_json, NULL};
    yep_sink paired = {yep_dom_on_event, d2, dom_on_flow_json, counting_pair};
    int rc1 = yep_engine_run(e1, buf, len, &ev_only);
    int rc2 = yep_engine_run(e2, buf, len, &paired);

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
    if (want_pair && g_pair_hits == 0) {
        fprintf(stderr, "DIFF %s: expected the pair path to fire\n", name);
        g_fail++;
    }
    g_pair_cases += g_pair_hits;
out:
    yep_dom_destroy(d1);
    yep_dom_destroy(d2);
    yep_engine_destroy(e1);
    yep_engine_destroy(e2);
}
static int g_want_pair_flag = 0;
static void diff_one(const char* name, const char* buf, size_t len) {
    diff_one_impl(name, buf, len, g_want_pair_flag);
}

static const char* k_must_fire[] = {
    "k: word\n",
    "k: a b c d e f g\n",
    "k: word # comment tail\n",
    "k: word\n  folded continuation\nz: 1\n",
    "defaults: &d0 a b\nuse: *d0\n",
    "x: &x0 word\ny: *x0\n",
    "a: &a1 one word\nb: *a1\nc: plain\n",
    "k: 12\nj: -2.5e3\nb: true\nn: null\n",
    "k: :colon-led\n",
    "outer:\n  k: word\n  x: &x1 tail\n  y: *x1\n",
    "url: http://x.y/z:80 path\n",
    NULL,
};

static const char* k_no_fire[] = {
    "k:\n",                      /* EMPTY: events stay */
    "k: {\"a\": 1, \"b\": 2}\n", /* FLOW: the flow fast path owns it */
    "k: \"quoted\"\n",           /* quoted: general path */
    "k: |\n  block\n",           /* block scalar */
    "k: !tag word\n",            /* tagged value */
    "k: &a\n  later\n",          /* anchor, value on following lines */
    "k: - x\n",                  /* error shape */
    "k: v: w\n",                 /* error shape */
    "- plain entry\n",           /* dash arm: not a KEY pair */
    "\"k\": word\n",             /* quoted key: general path */
    "&a k: word\n",              /* anchored key: general path */
    NULL,
};

int main(int argc, char** argv) {
    for (int i = 0; k_must_fire[i] != NULL; i++) {
        g_want_pair_flag = 1;
        diff_one(k_must_fire[i], k_must_fire[i], strlen(k_must_fire[i]));
    }
    for (int i = 0; k_no_fire[i] != NULL; i++) {
        g_want_pair_flag = 0;
        diff_one(k_no_fire[i], k_no_fire[i], strlen(k_no_fire[i]));
    }
    for (int i = 1; i < argc; i++) {
        tree_diff_walk(argv[i], diff_one);
    }
    printf("block-pair-diff: %d cases, %d pair lines, %d failures\n", g_cases, g_pair_cases,
           g_fail);
    return g_fail != 0;
}
