/* fuzz_schema_compile.c — the COMPILE face's invariants (#885 item 3;
 * the TODO.validate item-7 audit). The compile path takes UNTRUSTED
 * SCHEMA bytes — a face the validate walker (fuzz_validate.c) never
 * reaches: its schema leg rides the one-shot, where the schema bytes
 * die with the call. Here they are RETAINED: the compile arena, the
 * eager pattern-program builder (validate/regexp.c's NFA compiler),
 * the keyword table and the $ref machinery all live past the call and
 * must free cleanly, every iteration.
 *
 * Per input, both schema kinds where applicable: the compile face is
 * the JSON Schema kind only (the family walk's 12_CORE/12_JSON kinds
 * have no compiled form — they ride fuzz_validate.c). The legs:
 *
 *  - compile: yeptris_schema_compile of the bytes; NULL (any failure)
 *    is always legal — the face's only observable is the handle;
 *  - self: the compiled schema validates ITS OWN bytes (the one-shot
 *    harness's cross product, through the retained handle);
 *  - twice: the same instance again — the handle is immutable after
 *    compile (the pure-reader law), so status and report count must
 *    repeat exactly;
 *  - halves: the second mutated region of the same input (both halves
 *    of the bytes) as the instance — many-instances-per-schema, the
 *    economy's whole point;
 *  - the (NULL, 0) empty-instance and the NULL-report ARG edges.
 *
 * Property: arbitrary bytes only ever return OK / SCHEMA / PARSE /
 * DEPTH / ENCODING / MEMORY / ARG from yeptris_schema_validate — never
 * another status, never a crash, never a read out of bounds; a report
 * comes back exactly when the status says one exists; and the handle
 * frees in ONE call, leak-free (the nightly's ASan build is the
 * referee). The depth guards bound every recursion: the DOM's
 * 1000-level parse cap (deep schema bytes fail the compile, never the
 * stack), the 64-deep $ref chain cap and the 256-diagnostic report cap
 * ride the same walk the one-shot face uses. Inputs over 1 MiB are
 * skipped: the walkers are O(n) and the cap bounds the corpus walker.
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
    fprintf(stderr, "fuzz_schema_compile: TRAP %s (status=%d n=%zu)\n", site, last_status_,
            last_count_);
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

#define SCHEMA_MAX (1024u * 1024u)

static int status_known(YeptrisStatus st) {
    return st == YEPTRIS_OK || st == YEPTRIS_ERROR_SCHEMA || st == YEPTRIS_ERROR_PARSE ||
           st == YEPTRIS_ERROR_DEPTH || st == YEPTRIS_ERROR_ENCODING ||
           st == YEPTRIS_ERROR_MEMORY || st == YEPTRIS_ERROR_ARG;
}

/* The report contract the compiled face shares with the one-shot:
 * OK/SCHEMA carry a drained report (every diagnostic coded, its path
 * NUL-terminated, the 256 cap a hard ceiling); every other status
 * leaves *out NULL. Frees the report either way. */
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

/* Runs the instance legs against one live handle. The handle is freed
 * HERE — every iteration releases its arena, DOM and pattern programs
 * (the ASan nightly is the leak referee). */
static void probe_handle(yeptris_schema* s, const char* buf, size_t size) {
    yeptris_validate_report* r = NULL;
    YeptrisStatus st;

    /* self: the schema validates its own bytes (the one-shot's cross
     * product, through the retained handle) */
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_schema_compile: leg self\n");
    st = yeptris_schema_validate(s, buf, size, &r);
    drain(st, r);

    /* twice: the same instance must repeat exactly — the handle is
     * immutable after compile (the pure-reader law) */
    fprintf(stderr, "fuzz_schema_compile: leg twice\n");
    r = NULL;
    {
        YeptrisStatus st2 = yeptris_schema_validate(s, buf, size, &r);
        if (st2 != st || (r != NULL && yeptris_validate_report_count(r) != last_count_)) {
            trap_named("reader-impure"); /* the compiled walk is a pure reader */
        }
        drain(st2, r);
    }

    /* halves: the second mutated region of the same input — the
     * many-instances shape (each half as the instance) */
    size_t half = size / 2;
    fprintf(stderr, "fuzz_schema_compile: leg half-lo\n");
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    st = yeptris_schema_validate(s, buf, half, &r);
    drain(st, r);
    fprintf(stderr, "fuzz_schema_compile: leg half-hi\n");
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    st = yeptris_schema_validate(s, buf + (size - half), half, &r);
    drain(st, r);

    /* the empty-instance edge: OK with an empty report, or the report
     * allocation failed — nothing else */
    st = YEPTRIS_ERROR_ARG;
    r = NULL;
    fprintf(stderr, "fuzz_schema_compile: leg empty-instance\n");
    st = yeptris_schema_validate(s, NULL, 0, &r);
    last_status_ = (int)st;
    if (!status_known(st)) {
        trap_named("unknown-status");
    } else if (st == YEPTRIS_OK) {
        if (r == NULL) {
            trap_named("no-report");
        } else if (yeptris_validate_report_count(r) != 0) {
            trap_named("empty-with-diags"); /* nothing to invalidate in an empty stream */
        }
    } else if (st != YEPTRIS_ERROR_MEMORY || r != NULL) {
        trap_named("empty-instance-status");
    }
    yeptris_validate_report_free(r);

    /* the ARG edge: a NULL out pointer is the caller's error */
    fprintf(stderr, "fuzz_schema_compile: leg null-report\n");
    st = yeptris_schema_validate(s, buf, size, NULL);
    if (st != YEPTRIS_ERROR_ARG) {
        last_status_ = (int)st;
        trap_named("null-report-not-arg");
    }

    fprintf(stderr, "fuzz_schema_compile: leg free\n");
    yeptris_schema_free(s);
}

static int probe_one(const uint8_t* data, size_t size) {
    if (size > SCHEMA_MAX) {
        return 0;
    }
    const char* buf = (const char*)data;

    /* the compile leg: NULL (any failure) is always legal — bad
     * arguments, bytes that hold no document, a parse that fails
     * (DEPTH/ENCODING/... through the engine's own status), an
     * allocation that fails. Every surviving handle is drained by
     * probe_handle and freed there. */
    fprintf(stderr, "fuzz_schema_compile: leg compile\n");
    yeptris_schema* s = yeptris_schema_compile(buf, size);
    if (s != NULL) {
        probe_handle(s, buf, size);
    }

    /* the fixed edges (input-independent, cheap): NULL bytes with
     * length is ARG; NULL with 0 and empty bytes hold no document */
    if (yeptris_schema_compile(NULL, 4) != NULL) {
        trap_named("null-bytes-compiled");
    }
    s = yeptris_schema_compile(NULL, 0);
    if (s != NULL) {
        yeptris_schema_free(s); /* legal to survive; the contract prefers NULL */
    }
    /* freeing NULL is a no-op (the header's law) */
    yeptris_schema_free(NULL);
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
    fprintf(stderr, "fuzz_schema_compile: SIGNAL %d si_addr=%p", sig, si->si_addr);
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
                static uint8_t buf[SCHEMA_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                fprintf(stderr, "fuzz_schema_compile: seed %s (%zu bytes)\n", ent->d_name, n);
                probe_one(buf, n);
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[SCHEMA_MAX];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                fprintf(stderr, "fuzz_schema_compile: file %s (%zu bytes)\n", argv[a], n);
                probe_one(buf, n);
                files++;
            }
        }
    }
    printf("fuzz_schema_compile: %ld inputs, no invariant violations\n", files);
    return 0;
}

#else /* libFuzzer */

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    return probe_one(data, size);
}

#endif
