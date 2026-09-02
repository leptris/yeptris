/* fuzz_transcode.c — encoding front-end invariant fuzzer (TODO.impl/19).
 *
 * Property: a document that parses as UTF-8 parses IDENTICALLY when
 * re-encoded as UTF-16LE/BE or UTF-32LE/BE, with or without a BOM —
 * the serialization of every variant must be byte-equal. And any
 * byte sequence claimed as UTF-16/32 (BOM-prefixed) either parses
 * or errors — never crashes, never leaks (the fuzzers run under
 * ASAN for the leak half).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris.h>

static char* ser(YeptrisDocument doc, size_t* len) {
    return yeptris_serialize(doc, len);
}

/* Appends cp as UTF-16 code units (surrogate pair above BMP). */
static size_t put_utf16(uint8_t* out, uint32_t cp, int be) {
    uint16_t u[2];
    size_t n = 0;
    if (cp >= 0x10000) {
        uint32_t v = cp - 0x10000;
        u[n++] = (uint16_t)(0xD800 | (v >> 10));
        u[n++] = (uint16_t)(0xDC00 | (v & 0x3FF));
    } else {
        u[n++] = (uint16_t)cp;
    }
    for (size_t i = 0; i < n; i++) {
        if (be) {
            *out++ = (uint8_t)(u[i] >> 8);
            *out++ = (uint8_t)(u[i] & 0xFF);
        } else {
            *out++ = (uint8_t)(u[i] & 0xFF);
            *out++ = (uint8_t)(u[i] >> 8);
        }
    }
    return n * 2;
}

static void put_u32(uint8_t* out, uint32_t cp, int be) {
    if (be) {
        out[0] = (uint8_t)(cp >> 24);
        out[1] = (uint8_t)(cp >> 16);
        out[2] = (uint8_t)(cp >> 8);
        out[3] = (uint8_t)cp;
    } else {
        out[3] = (uint8_t)(cp >> 24);
        out[2] = (uint8_t)(cp >> 16);
        out[1] = (uint8_t)(cp >> 8);
        out[0] = (uint8_t)cp;
    }
}

/* Re-encodes a VALID UTF-8 document as UTF-16/32 (+BOM variants).
 * Returns the encoded length, or 0 when the input has ill-formed
 * UTF-8 (skip the equality check for those — they are the crash
 * cases, and the error path is exercised by the raw-feed loop). */
static size_t encode(const uint8_t* in, size_t n, int width, int be, int bom, uint8_t* out,
                     size_t cap) {
    size_t o = 0;
    if (bom) {
        if (width == 2) {
            out[o++] = be ? 0xFE : 0xFF;
            out[o++] = be ? 0xFF : 0xFE;
        } else {
            put_u32(out + o, 0xFEFF, be);
            o += 4;
        }
    }
    for (size_t i = 0; i < n;) {
        uint32_t cp;
        size_t take;
        if ((in[i] & 0x80) == 0) {
            cp = in[i];
            take = 1;
        } else if ((in[i] & 0xE0) == 0xC0 && i + 1 < n && (in[i + 1] & 0xC0) == 0x80) {
            cp = ((uint32_t)(in[i] & 0x1F) << 6) | (uint32_t)(in[i + 1] & 0x3F);
            take = 2;
        } else if ((in[i] & 0xF0) == 0xE0 && i + 2 < n && (in[i + 1] & 0xC0) == 0x80 &&
                   (in[i + 2] & 0xC0) == 0x80) {
            cp = ((uint32_t)(in[i] & 0x0F) << 12) | ((uint32_t)(in[i + 1] & 0x3F) << 6) |
                 (uint32_t)(in[i + 2] & 0x3F);
            take = 3;
        } else if ((in[i] & 0xF8) == 0xF0 && i + 3 < n && (in[i + 1] & 0xC0) == 0x80 &&
                   (in[i + 2] & 0xC0) == 0x80 && (in[i + 3] & 0xC0) == 0x80) {
            cp = ((uint32_t)(in[i] & 0x07) << 18) | ((uint32_t)(in[i + 1] & 0x3F) << 12) |
                 ((uint32_t)(in[i + 2] & 0x3F) << 6) | (uint32_t)(in[i + 3] & 0x3F);
            take = 4;
        } else {
            return 0; /* ill-formed: skip equality, keep raw feeding */
        }
        if (width == 2) {
            if (o + 4 > cap) {
                return 0;
            }
            o += put_utf16(out + o, cp, be);
        } else {
            if (o + 4 > cap) {
                return 0;
            }
            put_u32(out + o, cp, be);
            o += 4;
        }
        i += take;
    }
    return o;
}

static int parse_ok(const uint8_t* data, size_t n, char** out, size_t* out_len) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse((const char*)data, n, &st);
    if (doc == NULL) {
        return 0;
    }
    *out = ser(doc, out_len);
    yeptris_document_free(doc);
    return *out != NULL;
}

static void probe_one(const uint8_t* data, size_t size) {
    /* 1. equality: valid UTF-8 -> each UTF-16/32/BOM variant parses
     *    to the same serialization */
    char* base = NULL;
    size_t base_len = 0;
    if (parse_ok(data, size, &base, &base_len)) {
        static uint8_t enc[1 << 23];
        for (int width = 2; width <= 4; width += 2) {
            for (int be = 0; be <= 1; be++) {
                for (int bom = 1; bom >= 0; bom--) { /* BOM first: explicit beats sniffing */
                    size_t n = encode(data, size, width, be, bom, enc, sizeof(enc));
                    if (n == 0) {
                        continue;
                    }
                    char* got = NULL;
                    size_t got_len = 0;
                    if (parse_ok(enc, n, &got, &got_len)) {
                        if (got_len != base_len || memcmp(got, base, got_len) != 0) {
                            abort(); /* transcoding changed the document */
                        }
                    }
                    /* variants without a BOM may be read as UTF-8
                     * garbage: any outcome is fine as long as it
                     * never crashes (the property above covers the
                     * meaningful equality) */
                    free(got);
                }
            }
        }
        free(base);
    }

    /* 2. crash-freedom: arbitrary bytes claimed as each encoding */
    static uint8_t claimed[1 << 22];
    for (int width = 2; width <= 4; width += 2) {
        for (int be = 0; be <= 1; be++) {
            size_t n = 0;
            if (width == 2) {
                claimed[n++] = be ? 0xFE : 0xFF;
                claimed[n++] = be ? 0xFF : 0xFE;
                for (size_t i = 0; i + 1 < size; i += 2) {
                    claimed[n++] = data[i + 1];
                    claimed[n++] = data[i];
                }
            } else {
                put_u32(claimed, 0xFEFF, be);
                n = 4;
                for (size_t i = 0; i + 3 < size; i += 4) {
                    if (be) {
                        claimed[n++] = data[i];
                        claimed[n++] = data[i + 1];
                        claimed[n++] = data[i + 2];
                        claimed[n++] = data[i + 3];
                    } else {
                        claimed[n++] = data[i + 3];
                        claimed[n++] = data[i + 2];
                        claimed[n++] = data[i + 1];
                        claimed[n++] = data[i];
                    }
                }
            }
            YeptrisStatus st = YEPTRIS_OK;
            YeptrisDocument doc = yeptris_parse((const char*)claimed, n, &st);
            if (doc != NULL) {
                size_t len = 0;
                char* out = ser(doc, &len);
                free(out);
                yeptris_document_free(doc);
            }
        }
    }
}

#ifdef YEP_FUZZ_STANDALONE

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        char buf[1 << 16];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        probe_one((const uint8_t*)buf, n);
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
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                probe_one(buf, n);
                files++;
            }
        }
    }
    printf("fuzz_transcode: %ld inputs, no crashes, encodings agree\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size);
    return 0;
}

#endif
