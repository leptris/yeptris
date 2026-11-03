/* fuzz_validate.c — the validation face's invariants (#882; the
 * TODO.impl/19 discipline). The walks over UNTRUSTED bytes:
 *
 *  - the family walk (12_CORE / 12_JSON): parse+validate fused, the
 *    DOM route;
 *  - the event route (#902): the same families through the streaming
 *    twin (yeptris_validate_stream) — no DOM, the same contract;
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1 /* sigaction + dladdr under strict c11 */
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The trap-site helper prints on EVERY compiler (the bare-__builtin_trap
 * ud2 cost a CI dig: a trap fired silently on gcc because the naming
 * shim was MSVC-only, and the ud2 read as a compiler artifact). */
static const char* trap_site_;
static int last_status_;
static size_t last_count_;

static void trap_named(const char* site) {
    trap_site_ = site;
    fprintf(stderr, "fuzz_validate: TRAP %s (status=%d n=%zu)\n", site, last_status_, last_count_);
    fflush(stderr);
#if defined(_MSC_VER)
    __debugbreak();
#else
    __builtin_trap();
#endif
}

#if defined(_MSC_VER)
#include <intrin.h>
#endif

#include <yeptris/validate.h>

#define VALIDATE_MAX (1024u * 1024u)

static int status_known(YeptrisStatus st) {
    return st == YEPTRIS_OK || st == YEPTRIS_ERROR_SCHEMA || st == YEPTRIS_ERROR_PARSE ||
           st == YEPTRIS_ERROR_DEPTH || st == YEPTRIS_ERROR_ENCODING ||
           st == YEPTRIS_ERROR_MEMORY || st == YEPTRIS_ERROR_ARG;
}

static void drain(YeptrisStatus st, yeptris_validate_report* r) {
    last_status_ = (int)st;
    if (!status_known(st)) {
        trap_named("unknown-status"); /* a status the validator does not own */
        return;
    }
    if (st == YEPTRIS_OK || st == YEPTRIS_ERROR_SCHEMA) {
        if (r == NULL) {
            trap_named("no-report"); /* a completed validation with no report */
            return;
        }
        size_t n = yeptris_validate_report_count(r);
        last_count_ = n;
        for (size_t i = 0; i < n; i++) {
            const yeptris_diagnostic* d = yeptris_validate_report_at(r, i);
            if (d == NULL || d->code == 0u /* YEPTRIS_DIAG_NONE is never emitted */) {
                trap_named("code-none");
                return;
            }
            if (memchr(d->path, '\0', YEPTRIS_DIAG_PATH_MAX) == NULL) {
                trap_named("path-unterminated"); /* paths are always NUL-terminated */
                return;
            }
        }
        if (n > 256u /* YEP_VALIDATE_MAX_DIAGS, pinned by test_abi */) {
            trap_named("cap-exceeded"); /* the cap is a hard ceiling */
            return;
        }
    } else if (r != NULL) {
        trap_named("failed-with-report"); /* a failed validation allocated a report */
    }
    yeptris_validate_report_free(r);
}

static int probe_one(const uint8_t* data, size_t size) {
    if (size > VALIDATE_MAX) {
        return 0;
    }
    const char* buf = (const char*)data;
    yeptris_validate_report* r = NULL;
    YeptrisStatus st;

    /* sequenced: the report pointer must be read AFTER the call fills
     * it — `drain(f(&r), r)` reads argument r in unspecified order and
     * gcc-x86_64's right-to-left choice handed drain NULL (the silent
     * ud2 the amd64 CI dig chased) */
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_validate: leg core\n");
    st = yeptris_validate(buf, size, YEPTRIS_SCHEMA_12_CORE, &r);
    drain(st, r);
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_validate: leg json\n");
    st = yeptris_validate(buf, size, YEPTRIS_SCHEMA_12_JSON, &r);
    drain(st, r);
    /* the event route (#902): the same bytes through the streaming
     * twin — the same status contract, the same report shape */
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_validate: leg core-stream\n");
    st = yeptris_validate_stream(buf, size, YEPTRIS_SCHEMA_12_CORE, &r);
    drain(st, r);
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_validate: leg json-stream\n");
    st = yeptris_validate_stream(buf, size, YEPTRIS_SCHEMA_12_JSON, &r);
    drain(st, r);
    /* the schema side rides the same bytes: hostile schemas reach the
     * pointer resolver, the keyword readers and the regex kernel */
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_validate: leg jschema\n");
    st = yeptris_json_schema_validate(buf, size, buf, size, &r);
    drain(st, r);
    return 0;
}

#ifdef YEP_FUZZ_STANDALONE

#include <dirent.h>
#include <stdio.h>

#if !defined(_MSC_VER)
/* the x86_64-gcc ud2 dig: name the faulting PC (offset-from-base is
 * link-time under PIE, so addr2line maps it directly) */
#include <dlfcn.h>
#include <signal.h>
#include <unistd.h>
static void ill_handler(int sig, siginfo_t* si, void* uc) {
    (void)uc;
    Dl_info info;
    fprintf(stderr, "fuzz_validate: SIGNAL %d si_addr=%p", sig, si->si_addr);
    if (dladdr(si->si_addr, &info) != 0 && info.dli_fbase != NULL) {
        fprintf(stderr, " base=%p offset=0x%lx file=%s", info.dli_fbase,
                (unsigned long)((const char*)si->si_addr - (const char*)info.dli_fbase),
                info.dli_fname);
    }
    fprintf(stderr, "\n");
    _exit(134);
}
static void ill_install(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = ill_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
}
#else
static void ill_install(void) {}
#endif

int main(int argc, char** argv) {
    ill_install();
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
                fprintf(stderr, "fuzz_validate: seed %s (%zu bytes)\n", ent->d_name, n);
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
                fprintf(stderr, "fuzz_validate: file %s (%zu bytes)\n", argv[a], n);
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
