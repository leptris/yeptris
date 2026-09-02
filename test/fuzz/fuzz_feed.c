/* fuzz_feed.c — chunked-feed invariant fuzzer (TODO.impl/19).
 *
 * Property: feeding the recorder the SAME bytes in any chunk split
 * (chunk boundaries derived deterministically from the input) lands
 * in the same state as one whole-buffer feed — same status, same
 * event count. Chunking is transport, never semantics. Feeding after
 * final, and a NULL chunk with nonzero length, are rejected with
 * ERROR_ARG and never crash.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris/events.h>

static size_t whole_run(const uint8_t* data, size_t size, YeptrisStatus* status) {
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        *status = YEPTRIS_ERROR_MEMORY;
        return 0;
    }
    *status = yeptris_recorder_feed(rec, (const char*)data, size, 1);
    size_t count = 0;
    if (*status == YEPTRIS_OK) {
        (void)yeptris_recorder_records(rec, &count);
    }
    yeptris_recorder_free(rec);
    return count;
}

static size_t chunked_run(const uint8_t* data, size_t size, YeptrisStatus* status) {
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        *status = YEPTRIS_ERROR_MEMORY;
        return 0;
    }
    /* deterministic chunk sizes from the bytes themselves: a small
     * xorshift over each 8-byte block — the same input always splits
     * the same way, different inputs split differently */
    size_t i = 0;
    while (i < size) {
        uint32_t x = 0x9E3779B9u;
        for (size_t j = 0; j < 8 && i + j < size; j++) {
            x ^= data[i + j] << (j * 3);
        }
        x ^= x >> 15;
        x *= 0x2545F491u;
        size_t take = 1 + (x % 97);
        if (take > size - i) {
            take = size - i;
        }
        YeptrisStatus st = yeptris_recorder_feed(rec, (const char*)data + i, take, 0);
        if (st != YEPTRIS_OK) {
            /* pre-final feeds only fail on OOM/ARG — a transport-free
             * rejection must match the whole-buffer verdict */
            *status = st;
            yeptris_recorder_free(rec);
            return 0;
        }
        i += take;
    }
    *status = yeptris_recorder_feed(rec, "", 0, 1);
    size_t count = 0;
    if (*status == YEPTRIS_OK) {
        (void)yeptris_recorder_records(rec, &count);
    }
    yeptris_recorder_free(rec);
    return count;
}

static void probe_one(const uint8_t* data, size_t size) {
    YeptrisStatus st_whole = YEPTRIS_OK;
    YeptrisStatus st_chunked = YEPTRIS_OK;
    size_t n_whole = whole_run(data, size, &st_whole);
    size_t n_chunked = chunked_run(data, size, &st_chunked);
    if (st_whole != st_chunked || n_whole != n_chunked) {
        abort(); /* chunking changed the outcome: transport leaked into semantics */
    }

    /* one recorder takes exactly one final chunk */
    YeptrisRecorder rec = yeptris_recorder_new();
    if (rec == NULL) {
        return;
    }
    (void)yeptris_recorder_feed(rec, (const char*)data, size > 4 ? 4 : size, 0);
    (void)yeptris_recorder_feed(rec, "", 0, 1);
    if (yeptris_recorder_feed(rec, "x", 1, 1) != YEPTRIS_ERROR_ARG) {
        abort();
    }
    if (yeptris_recorder_feed(rec, NULL, 4, 0) != YEPTRIS_ERROR_ARG) {
        abort();
    }
    yeptris_recorder_free(rec);
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
    printf("fuzz_feed: %ld inputs, no crashes, chunking is transport\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size);
    return 0;
}

#endif
