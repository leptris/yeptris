/* fuzz_validate.c — the validation face's invariants (#882; the
 * TODO.impl/19 discipline). Both walks over UNTRUSTED bytes:
 *
 *  - the family walk (12_CORE / 12_JSON): parse+validate fused;
 *  - the JSON Schema walk: the same bytes as BOTH schema and instance
 *    (the cross product reaches the schema-side guards, the pointer
 *    resolver and the regex kernel with hostile patterns).
 *
 * Property: arbitrary bytes only ever return OK / SCHEMA / PARSE /
 * DEPTH / ENCODING / MEMORY / ARG — never another status, never a
 * crash, never a read out of bounds; a report comes back exactly when
 * the status says one exists, and it frees cleanly. Inputs over 1 MiB
 * are skipped: the walkers are O(n) and the cap bounds the corpus
 * walker. Memory safety rides the nightly's ASAN/UBSan build; the
 * depth guards (the DOM's parse cap, the 64-deep $ref chain cap, the
 * 256-diagnostic report cap) bound every recursion.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define __builtin_trap() __debugbreak()
#endif

#include <yeptris/validate.h>

#define VALIDATE_MAX (1024u * 1024u)

static int status_known(YeptrisStatus st) {
    return st == YEPTRIS_OK || st == YEPTRIS_ERROR_SCHEMA || st == YEPTRIS_ERROR_PARSE ||
           st == YEPTRIS_ERROR_DEPTH || st == YEPTRIS_ERROR_ENCODING ||
           st == YEPTRIS_ERROR_MEMORY || st == YEPTRIS_ERROR_ARG;
}

static void drain(YeptrisStatus st, yeptris_validate_report* r) {
    if (!status_known(st)) {
        __builtin_trap(); /* a status the validator does not own */
    }
    if (st == YEPTRIS_OK || st == YEPTRIS_ERROR_SCHEMA) {
        if (r == NULL) {
            __builtin_trap(); /* a completed validation with no report */
        }
        size_t n = yeptris_validate_report_count(r);
        for (size_t i = 0; i < n; i++) {
            const yeptris_diagnostic* d = yeptris_validate_report_at(r, i);
            if (d == NULL || d->code == 0u /* YEPTRIS_DIAG_NONE is never emitted */) {
                __builtin_trap();
            }
            if (memchr(d->path, '\0', YEPTRIS_DIAG_PATH_MAX) == NULL) {
                __builtin_trap(); /* paths are always NUL-terminated */
            }
        }
        if (n > 256u /* YEP_VALIDATE_MAX_DIAGS, pinned by test_abi */) {
            __builtin_trap(); /* the cap is a hard ceiling */
        }
    } else if (r != NULL) {
        __builtin_trap(); /* a failed validation allocated a report */
    }
    yeptris_validate_report_free(r);
}

static int probe_one(const uint8_t* data, size_t size) {
    if (size > VALIDATE_MAX) {
        return 0;
    }
    const char* buf = (const char*)data;
    yeptris_validate_report* r = NULL;

    r = NULL;
    drain(yeptris_validate(buf, size, YEPTRIS_SCHEMA_12_CORE, &r), r);
    r = NULL;
    drain(yeptris_validate(buf, size, YEPTRIS_SCHEMA_12_JSON, &r), r);
    /* the schema side rides the same bytes: hostile schemas reach the
     * pointer resolver, the keyword readers and the regex kernel */
    r = NULL;
    drain(yeptris_json_schema_validate(buf, size, buf, size, &r), r);
    return 0;
}

#ifdef YEP_FUZZ_STANDALONE

#include <dirent.h>
#include <stdio.h>

int main(int argc, char** argv) {
    long files = 0;
    for (int a = 1; a < argc; a++) {
        DIR* d = opendir(argv[a]);
        if (d != NULL) {
            struct dirent* ent;
            while ((ent = readdir(d)) != NULL) {
                char path[2048];
                snprintf(path, sizeof(path), "%s/%s", argv[a], ent->d_name);
                FILE* f = fopen(path, "rb");
                if (f == NULL) {
                    continue;
                }
                static uint8_t buf[VALIDATE_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[VALIDATE_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
        }
    }
    printf("fuzz_validate: %ld inputs, no invariant violations\n", files);
    return 0;
}

#else /* libFuzzer */

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return probe_one(data, size);
}

#endif
