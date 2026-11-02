/* fuzz_marshal.c — the Ruby Marshal wire readers' invariants (the
 * TODO.impl/19 discipline; the 2026-10 audit found every reader of the
 * format un-fuzzed).
 *
 * Two readers of the Marshal 4.8 wire format exist: the dump face's
 * yeptris_json_from_marshal (emit/writer.c, yeptris-ruby#322) and the
 * CBOR twin yeptris_cbor_from_marshal (cbor/from_marshal.c). Property:
 * arbitrary bytes only ever produce OK (bytes out, freeable),
 * UNSUPPORTED (the binding's fallback signal), MEMORY or ARG — never
 * another status, never a crash, never a read out of bounds, and no
 * allocation on failure. Both compactions/canonicalizations are
 * probed, raw bytes AND the 04 08 header prefix (the one gate both
 * readers share; prefixing reaches the deep walk the raw tail cannot).
 * Inputs over 1 MiB are skipped: the readers are O(n) and the cap
 * bounds the corpus walker. Memory safety rides the nightly's
 * ASAN/UBSan build.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#define __builtin_trap() __debugbreak()
#endif

#include <yeptris/cbor.h>
#include <yeptris/marshal.h>

#define MARSHAL_MAX (1024u * 1024u)

static void probe_json(const char* m, size_t n, int compact) {
    char* out = NULL;
    size_t olen = 0;
    YeptrisStatus st = yeptris_json_from_marshal(m, n, compact, &out, &olen);
    if (st == YEPTRIS_OK) {
        if (out == NULL) {
            __builtin_trap(); /* OK with no bytes */
        }
        yeptris_marshal_free(out);
    } else if (st == YEPTRIS_ERROR_UNSUPPORTED || st == YEPTRIS_ERROR_MEMORY) {
        if (out != NULL) {
            __builtin_trap(); /* failure allocated */
        }
    } else {
        __builtin_trap(); /* a status the reader does not own */
    }
}

static void probe_cbor(const char* m, size_t n, uint32_t opts) {
    char* out = NULL;
    size_t olen = 0;
    YeptrisStatus st = yeptris_cbor_from_marshal(m, n, opts, &out, &olen);
    if (st == YEPTRIS_OK) {
        if (out == NULL) {
            __builtin_trap();
        }
        yeptris_marshal_free(out);
    } else if (st == YEPTRIS_ERROR_UNSUPPORTED || st == YEPTRIS_ERROR_MEMORY) {
        if (out != NULL) {
            __builtin_trap();
        }
    } else {
        __builtin_trap();
    }
}

static int probe_one(const uint8_t* data, size_t size) {
    if (size > MARSHAL_MAX) {
        return 0;
    }
    const char* m = (const char*)data;
    probe_json(m, size, 0);
    probe_json(m, size, 1);
    probe_cbor(m, size, 0);
    probe_cbor(m, size, YEPTRIS_CBOR_CANONICAL);

    /* the 04 08 header is the readers' only shared gate: prefixed
     * mutated bytes reach the deep walk (symbols, links, ivars) */
    if (size > 0 && size <= MARSHAL_MAX - 2) {
        char* buf = (char*)malloc(size + 2);
        if (buf != NULL) {
            buf[0] = 0x04;
            buf[1] = 0x08;
            memcpy(buf + 2, data, size);
            probe_json(buf, size + 2, 0);
            probe_json(buf, size + 2, 1);
            probe_cbor(buf, size + 2, 0);
            probe_cbor(buf, size + 2, YEPTRIS_CBOR_CANONICAL);
            free(buf);
        }
    }
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
                static uint8_t buf[MARSHAL_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[MARSHAL_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
        }
    }
    printf("fuzz_marshal: %ld inputs, no invariant violations\n", files);
    return 0;
}

#else /* libFuzzer */

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return probe_one(data, size);
}

#endif
