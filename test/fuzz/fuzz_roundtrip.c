/* fuzz_roundtrip.c — serialize/parse symmetry fuzzer (TODO.impl/19).
 *
 * Property: for any input that parses, serialize(doc) must re-parse,
 * and the re-serialization must be byte-stable — emitter/parser
 * asymmetries surface here. TODO.restructure/21: parseable inputs
 * must also marshal without error (OK or UNSUPPORTED, never a crash
 * or an unexpected status) — format-level properties are pinned by
 * the Ruby binding's differential corpus; memory safety lives here.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/marshal.h>
#include <yeptris/visit.h>

static int probe_one(const uint8_t* data, size_t size) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse((const char*)data, size, &st);
    if (doc == NULL) {
        return 0;
    }
    {
        char* out = NULL;
        size_t olen = 0;
        YeptrisStatus ms = yeptris_marshal((const char*)data, size, YEPTRIS_SCHEMA_11_COMPAT,
                                           YEPTRIS_MARSHAL_ALL_DOCS, &out, &olen);
        if (ms != YEPTRIS_OK && ms != YEPTRIS_ERROR_UNSUPPORTED) {
            __builtin_trap(); /* parseable input must be marshalable */
        }
        if (ms == YEPTRIS_OK && (olen < 2 || (uint8_t)out[0] != 0x04 || (uint8_t)out[1] != 0x08)) {
            __builtin_trap(); /* Marshal 4.8 header */
        }
        yeptris_marshal_free(out);
    }
    /* TODO.restructure/24: the visit API must accept anything the
     * engine parsed (OK or the same failure class — never a crash);
     * strict-JSON inputs additionally take the fused scan. */
    {
        static const YeptrisVisitVTable none = {0};
        (void)yeptris_visit((const char*)data, size, YEPTRIS_SCHEMA_11_COMPAT, &none, NULL);
        (void)yeptris_visit_json((const char*)data, size, &none, NULL);
    }
    size_t l1 = 0;
    char* s1 = yeptris_serialize(doc, &l1);
    yeptris_document_free(doc);
    if (s1 == NULL) {
        return 0;
    }
    YeptrisStatus st2 = YEPTRIS_OK;
    YeptrisDocument doc2 = yeptris_parse(s1, l1, &st2);
    if (doc2 == NULL) {
        /* emitted YAML must re-parse: an invariant, not a fuzz choice */
        __builtin_trap();
    }
    size_t l2 = 0;
    char* s2 = yeptris_serialize(doc2, &l2);
    yeptris_document_free(doc2);
    if (s2 != NULL && (l1 != l2 || memcmp(s1, s2, l1) != 0)) {
        __builtin_trap(); /* byte instability */
    }
    free(s1);
    free(s2);
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
    printf("fuzz_roundtrip: %ld inputs, no crashes\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return probe_one(data, size);
}

#endif
