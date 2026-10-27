/* fuzz_cbor.c — CBOR codec invariants fuzzer (TODO.cbor/06, the
 * item-19 discipline).
 *
 * Properties (trap = invariant violation):
 * - decode (lenient AND strict) only ever returns the status classes
 *   the decoder owns; anything else is a bug
 * - any input the lenient decode ACCEPTS must canonical-encode
 *   (CBOR-decoded docs never carry aliases or non-decimal tags)
 * - decode -> canonical encode -> decode -> canonical encode is
 *   byte-stable: the second encode equals the first (tree equality
 *   through canonical bytes — the 02 stability contract)
 * - the sequence walker over the same buffer never crashes
 * Memory safety lives under ASAN/valgrind nightlies (leaks here).
 */

#include <stdint.h>
#include <stdlib.h>

#if defined(_MSC_VER)
#define __builtin_trap() __debugbreak()
#endif
#include <string.h>

#include <yeptris.h>
#include <yeptris/cbor.h>

static int seq_count_cb(void* ctx, YeptrisDocument item, size_t index) {
    (void)index;
    (*(size_t*)ctx)++;
    yeptris_document_free(item);
    return 0;
}

static int probe_one(const uint8_t* data, size_t size) {
    for (int mode = 0; mode < 2; mode++) {
        uint32_t opts = mode ? YEPTRIS_CBOR_STRICT : 0;
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument doc = yeptris_cbor_decode(data, size, opts, &st);
        if (doc == NULL) {
            if (st != YEPTRIS_ERROR_PARSE && st != YEPTRIS_ERROR_DEPTH &&
                st != YEPTRIS_ERROR_ENCODING && st != YEPTRIS_ERROR_MEMORY &&
                st != YEPTRIS_ERROR_ARG) {
                __builtin_trap(); /* a status the decoder does not own */
            }
            continue;
        }
        if (st != YEPTRIS_OK) {
            __builtin_trap(); /* document + non-OK status */
        }
        size_t n1 = 0;
        uint8_t* enc = yeptris_cbor_encode(doc, YEPTRIS_CBOR_CANONICAL, &n1);
        yeptris_document_free(doc);
        if (enc == NULL) {
            __builtin_trap(); /* accepted CBOR must canonical-encode */
        }
        YeptrisStatus st2 = YEPTRIS_OK;
        YeptrisDocument doc2 = yeptris_cbor_decode(enc, n1, 0, &st2);
        if (doc2 == NULL) {
            free(enc);
            __builtin_trap(); /* encode(decode(x)) must re-decode */
        }
        size_t n2 = 0;
        uint8_t* enc2 = yeptris_cbor_encode(doc2, YEPTRIS_CBOR_CANONICAL, &n2);
        yeptris_document_free(doc2);
        if (enc2 == NULL || n1 != n2 || memcmp(enc, enc2, n1) != 0) {
            free(enc);
            free(enc2);
            __builtin_trap(); /* canonical byte instability */
        }
        free(enc);
        free(enc2);
    }
    size_t items = 0;
    YeptrisStatus sst = YEPTRIS_OK;
    (void)yeptris_cbor_decode_sequence(data, size, 0, seq_count_cb, &items, &sst);
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
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        }
    }
    printf("fuzz_cbor: %ld inputs, no invariant violations\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return probe_one(data, size);
}

#endif
