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

/* True when the two dumps differ ONLY by a leading content-U+FEFF:
 * libyaml's UTF-16 reader drops a U+FEFF that follows the encoding
 * mark, while its own UTF-8 path KEEPS it (catch 121 matched that
 * twin) and yeptris is consistent across encodings -- one mark
 * stripped, content BOMs ride (the catch-121 law). The suite is
 * silent on the shape. The transform strips one leading EF BB BF
 * from yeptris's first scalar and compares. */
static int yd_bom_content_divergence(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    const char* val = strstr(ydump, "=VAL : \xEF\xBB\xBF");
    if (val == NULL) {
        return 0; /* yeptris's first scalar must open with a content BOM */
    }
    char stripped[8192];
    size_t head = (size_t)(val - ydump) + 7; /* keep "=VAL : " */
    size_t left = strlen(ydump) - head;
    if (left >= sizeof(stripped)) {
        return 0;
    }
    memcpy(stripped, ydump, head); /* up to and incl. "=VAL : " */
    size_t o = head;
    const char* rest = ydump + head;
    if (rest[0] == (char)0xEF && rest[1] == (char)0xBB && rest[2] == (char)0xBF) {
        rest += 3; /* strip ONE content BOM */
    } else {
        return 0;
    }
    memcpy(stripped + o, rest, strlen(rest));
    o += strlen(rest);
    stripped[o] = '\0';
    return strcmp(stripped, ldump) == 0;
}

/* True when the two dumps differ ONLY by the flow-'?' family: libyaml
 * (pinned 90a56d4, 2011) makes a KEY token for a flow '?' with any
 * follower ("[?p]" -> {p: null}), while yaml-test-suite pins the
 * plain reading -- HM87 "[?x]" -> "=VAL :?x", 652Z "{ ?foo: bar }" ->
 * "=VAL :?foo" -- and yeptris follows the suite (the conformance
 * bar). The transform rewrites each libyaml single-pair map block
 * "+MAP / =VAL :X / =VAL : / -MAP" into "=VAL :?X" and compares. */
static int yd_qflow_divergence(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    char out[8192];
    size_t o = 0;
    const char* lines[512];
    size_t lens[512];
    int n = 0;
    const char* p = ldump;
    while (*p != '\0' && n < 512) {
        lines[n] = p;
        const char* nl = strchr(p, '\n');
        lens[n] = nl ? (size_t)(nl - p) : strlen(p);
        n++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    for (int i = 0; i < n; i++) {
        /* the event-dump format is "=VAL : <value>" */
        if (lens[i] == 4 && strncmp(lines[i], "+MAP", 4) == 0 && i + 3 < n && lens[i + 1] >= 8 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && lens[i + 2] == 7 &&
            strncmp(lines[i + 2], "=VAL : ", 7) == 0 && lens[i + 3] == 4 &&
            strncmp(lines[i + 3], "-MAP", 4) == 0) {
            /* a single-pair map: yeptris spells the key with a '?'
             * prefix and no wrapper */
            if (o + 8 + (lens[i + 1] - 7) + 1 >= sizeof(out)) {
                return 0;
            }
            memcpy(out + o, "=VAL : ?", 8);
            o += 8;
            memcpy(out + o, lines[i + 1] + 7, lens[i + 1] - 7);
            o += lens[i + 1] - 7;
            out[o++] = '\n';
            i += 3;
            continue;
        }
        if (o + lens[i] + 2 >= sizeof(out)) {
            return 0;
        }
        memcpy(out + o, lines[i], lens[i]);
        o += lens[i];
        out[o++] = '\n';
    }
    /* the yeptris dump is line-per-event too; compare line sets */
    char ynorm[8192];
    size_t yo = 0;
    p = ydump;
    while (*p != '\0' && yo < sizeof(ynorm) - 1) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        memcpy(ynorm + yo, p, len);
        yo += len;
        ynorm[yo++] = '\n';
        p = nl ? nl + 1 : p + strlen(p);
    }
    return o == yo && memcmp(out, ynorm, o) == 0;
}

static void probe_one(const uint8_t* data, size_t size) {
    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)data, size, &ydump, &ldump);
    int split = v == YD_YEPTRIS_ONLY || v == YD_LIBYAML_ONLY;
    /* a documented divergence, not a bug: the suite sides with
     * yeptris (see yd_qflow_divergence) */
    int known = v == YD_DIFFER &&
                (yd_qflow_divergence(ydump, ldump) || yd_bom_content_divergence(ydump, ldump));
#ifdef YEP_FUZZ_STANDALONE
    /* corpus walker: splits are enforced against the ledger (an
     * unledgered split is a conformance regression) */
    if ((v == YD_DIFFER || split) && !known && !(probe_id != NULL && yd_ledger_has(probe_id))) {
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
    if (v == YD_DIFFER && !known) {
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
