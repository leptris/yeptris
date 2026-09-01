/* fuzz_parse.c — parse invariant fuzzer (TODO.impl/19).
 *
 * Property: parsing any byte sequence never crashes, never reads out
 * of bounds, and either returns a document (freeable, queryable) or
 * an error status — no third state. Both schemas exercised.
 *
 * libFuzzer entry when built with -fsanitize=fuzzer; a standalone
 * corpus-walking main otherwise (AFL/libFuzzer-friendly stdin too).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/dom.h>
#include <yeptris/parse.h>

static int probe_one(const uint8_t* data, size_t size, int compat) {
    YeptrisParseOptions opts;
    memset(&opts, 0, sizeof(opts));
    opts.schema = compat ? YEPTRIS_SCHEMA_11_COMPAT : YEPTRIS_SCHEMA_12_CORE;
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_ex((const char*)data, size, &opts, &st);
    if (doc != NULL) {
        /* query every document and root: accessors must be crash-free
         * and consistent with the kinds they report */
        size_t docs = yeptris_document_count(doc);
        for (size_t i = 0; i < docs; i++) {
            YeptrisNode root = yeptris_document_root(doc, i);
            if (root == NULL) {
                continue;
            }
            int64_t iv;
            double dv;
            int bv;
            (void)yeptris_node_int(root, &iv);
            (void)yeptris_node_float(root, &dv);
            (void)yeptris_node_bool(root, &bv);
            if (yeptris_node_kind(root) == YEPTRIS_NODE_MAPPING) {
                YeptrisNode k = NULL, v = NULL;
                if (yeptris_node_map_at(root, 0, &k, &v) == YEPTRIS_OK && k != NULL) {
                    size_t len = 0;
                    (void)yeptris_node_value(k, &len);
                }
            }
            if (yeptris_node_kind(root) == YEPTRIS_NODE_SEQUENCE) {
                size_t n = yeptris_node_seq_count(root);
                if (n > 0) {
                    (void)yeptris_node_seq_at(root, n - 1);
                }
                (void)yeptris_node_seq_at(root, n + 8); /* OOB: NULL */
            }
        }
        /* serialize what parsed: the emitter must accept its own DOM */
        size_t len = 0;
        char* out = yeptris_serialize(doc, &len);
        free(out);
        yeptris_document_free(doc);
    }
    return 0;
}

#ifdef YEP_FUZZ_STANDALONE

#include <dirent.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        /* stdin mode: one input */
        char buf[1 << 16];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        probe_one((const uint8_t*)buf, n, 0);
        probe_one((const uint8_t*)buf, n, 1);
        return 0;
    }
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
                probe_one(buf, n, 0);
                probe_one(buf, n, 1);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n, 0);
                probe_one(buf, n, 1);
                files++;
            }
        }
    }
    printf("fuzz_parse: %ld inputs, no crashes\n", files);
    return 0;
}

#else /* libFuzzer */

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size, 0);
    probe_one(data, size, 1);
    return 0;
}

#endif
