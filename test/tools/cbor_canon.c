/* cbor_canon.c — the differential driver (TODO.cbor/06): reads CBOR
 * bytes from the file argument (or stdin), decodes, canonical-
 * encodes, prints "OK <hex>" — or the failure status. The conformance
 * scripts compare this line against the reference runtimes. */
#include <stdio.h>
#include <stdlib.h>

#include <yeptris.h>
#include <yeptris/cbor.h>

int main(int argc, char** argv) {
    static unsigned char buf[1 << 24];
    size_t n = 0;
    if (argc > 1) {
        FILE* f = fopen(argv[1], "rb");
        if (f == NULL) {
            puts("ERR open");
            return 2;
        }
        n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
    } else {
        n = fread(buf, 1, sizeof(buf), stdin);
    }
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_cbor_decode(buf, n, 0, &st);
    if (doc == NULL) {
        printf("REJECT %d\n", (int)st);
        return 1;
    }
    size_t len = 0;
    unsigned char* enc = yeptris_cbor_encode(doc, YEPTRIS_CBOR_CANONICAL, &len);
    yeptris_document_free(doc);
    if (enc == NULL) {
        puts("ENCFAIL");
        return 1;
    }
    printf("OK ");
    for (size_t i = 0; i < len; i++) {
        printf("%02x", enc[i]);
    }
    printf("\n");
    free(enc);
    return 0;
}
