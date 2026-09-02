/* fuzz_diff.c — differential conformance fuzzer (TODO.impl/19).
 *
 * Property: for ANY input, yeptris and libyaml agree — same event
 * stream when both parse (EQUAL), or both reject (BOTH-ERROR).
 * DIFFER or either-only-accept is a conformance divergence and
 * aborts (with the dumps printed for minimization via yepdiff).
 * The classifier is diff_core.c — the same one yepdiff uses, so
 * the fuzzer and the human tool can never disagree about the rule.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff_core.h"

/* The corpus basename this input came from (set by the walker), or
 * NULL under libFuzzer — splits must then be ledgered by content. */
static const char* probe_id = NULL;

static void probe_one(const uint8_t* data, size_t size) {
    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)data, size, &ydump, &ldump);
    int split = v == YD_YEPTRIS_ONLY || v == YD_LIBYAML_ONLY;
#ifdef YEP_FUZZ_STANDALONE
    /* corpus walker: splits are enforced against the ledger (an
     * unledgered split is a conformance regression) */
    if ((v == YD_DIFFER || split) && !(probe_id != NULL && yd_ledger_has(probe_id))) {
        fprintf(stderr, "fuzz_diff: %s%s%s on input:\n--- yeptris:\n%s\n--- libyaml:\n%s\n",
                yd_verdict_name(v), probe_id ? " (" : "", probe_id ? probe_id : "",
                ydump ? ydump : "(error)", ldump ? ldump : "(error)");
        abort();
    }
#else
    /* libFuzzer entry (mutated bytes, no corpus id): only a true
     * semantic divergence aborts — BOTH accepted but the event
     * streams differ. Random-byte splits live in the suite-gray
     * zone the ledger documents by input, not by mutation. */
    (void)split;
    (void)probe_id;
    if (v == YD_DIFFER) {
        fprintf(stderr, "fuzz_diff: %s on mutated input:\n--- yeptris:\n%s\n--- libyaml:\n%s\n",
                yd_verdict_name(v), ydump ? ydump : "(error)", ldump ? ldump : "(error)");
        abort();
    }
#endif
    free(ydump);
    free(ldump);
}

#ifdef YEP_FUZZ_STANDALONE

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        char buf[1 << 16];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        probe_one((const uint8_t*)buf, n);
        return 0;
    }
    /* argv[1] may be the ledger, then corpus dirs/files */
    int first = 1;
    if (strncmp(argv[1], "--ledger=", 9) == 0) {
        if (yd_ledger_load(argv[1] + 9) < 0) {
            fprintf(stderr, "fuzz_diff: no ledger %s\n", argv[1] + 9);
            return 2;
        }
        first = 2;
        if (argc < 3) {
            return 0;
        }
    }
    static char id_buf[256];
    long files = 0;
    for (int a = first; a < argc; a++) {
        DIR* d = opendir(argv[a]);
        if (d != NULL) {
            struct dirent* ent;
            while ((ent = readdir(d)) != NULL) {
                size_t nl = strlen(ent->d_name);
                if (nl < 4 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
                    continue; /* suite inputs only (.ly goldens aren't cases) */
                }
                char path[2048];
                snprintf(path, sizeof(path), "%s/%s", argv[a], ent->d_name);
                FILE* f = fopen(path, "rb");
                if (f == NULL) {
                    continue;
                }
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                snprintf(id_buf, sizeof(id_buf), "%.*s", (int)(nl - 3), ent->d_name);
                probe_id = id_buf;
                probe_one(buf, n);
                probe_id = NULL;
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                const char* base = strrchr(argv[a], '/');
                base = base ? base + 1 : argv[a];
                size_t bl = strlen(base);
                snprintf(id_buf, sizeof(id_buf), "%.*s",
                         (int)(bl > 3 && strcmp(base + bl - 3, ".in") == 0 ? bl - 3 : bl), base);
                probe_id = id_buf;
                probe_one(buf, n);
                probe_id = NULL;
                files++;
            }
        }
    }
    printf("fuzz_diff: %ld inputs, no divergences\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size);
    return 0;
}

#endif
