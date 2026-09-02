/* yepdiff.c — one input, both libraries, classified (TODO.impl/17C).
 *
 * A dev tool (not shipped): the differential classifier in
 * diff_core.c does the work; this is the human-facing file front
 * end. The fuzzers (19) drive the same classifier for corpus gates.
 *
 * Usage: yepdiff <input-file>
 * Exit: 0 equal or both-error, 1 differ or split.
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

#include "../test/port/libyaml/diff_core.h"

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
    if (in == NULL || fread(in, 1, (size_t)n, f) != (size_t)n) {
        free(in);
        fclose(f);
        return 2;
    }
    fclose(f);

    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify(in, (size_t)n, &ydump, &ldump);
    int rc = v == YD_EQUAL || v == YD_BOTH_ERROR ? 0 : 1;
    printf("%s %s\n", yd_verdict_name(v), argv[1]);
    if (rc == 1) {
        printf("--- yeptris (%s):\n%s\n--- libyaml (%s):\n%s\n",
               v == YD_LIBYAML_ONLY ? "error" : "ok", ydump ? ydump : "(error)",
               v == YD_YEPTRIS_ONLY ? "error" : "ok", ldump ? ldump : "(error)");
    }
    free(ydump);
    free(ldump);
    free(in);
    return rc;
}
