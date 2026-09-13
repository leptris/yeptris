/* test/compat/dirent.h — the POSIX dirent surface over FindFirstFile.
 *
 * This directory rides the test targets' include path ONLY under MSVC
 * (see test/CMakeLists.txt); everywhere else the real <dirent.h> is
 * found first, so the corpus walkers keep one source shape.
 *
 * readdir yields "." and ".." like POSIX (the file-extension filters
 * ignore them either way). */
#ifndef YEP_TEST_COMPAT_DIRENT_H
#define YEP_TEST_COMPAT_DIRENT_H

#include <string.h>

/* the standard pollution guards — test locals must not collide with
 * windows.h macros (an unguarded shim renamed a canonical-block local
 * out from under the compiler) */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

typedef struct DIR {
    HANDLE h; /* INVALID_HANDLE_VALUE until the first real read */
    int done;
    int dot_phase; /* 0 = yield ".", 1 = yield "..", 2 = real entries */
    char pat[MAX_PATH + 3];
    struct dirent {
        char d_name[MAX_PATH];
    } e;
} DIR;

static inline DIR* opendir(const char* name) {
    size_t n = strlen(name);
    if (n > MAX_PATH) {
        return NULL;
    }
    DIR* d = (DIR*)malloc(sizeof(DIR));
    if (d == NULL) {
        return NULL;
    }
    memcpy(d->pat, name, n);
    d->pat[n] = '\\';
    d->pat[n + 1] = '*';
    d->pat[n + 2] = '\0';
    d->h = INVALID_HANDLE_VALUE;
    d->done = 0;
    d->dot_phase = 0;
    return d;
}

static inline struct dirent* readdir(DIR* d) {
    WIN32_FIND_DATAA fd;
    if (d->done) {
        return NULL;
    }
    if (d->dot_phase == 0) {
        d->dot_phase = 1;
        strcpy_s(d->e.d_name, sizeof(d->e.d_name), ".");
        return &d->e;
    }
    if (d->dot_phase == 1) {
        d->dot_phase = 2;
        strcpy_s(d->e.d_name, sizeof(d->e.d_name), "..");
        return &d->e;
    }
    int ok;
    if (d->h == INVALID_HANDLE_VALUE) {
        d->h = FindFirstFileA(d->pat, &fd);
        ok = (d->h != INVALID_HANDLE_VALUE);
    } else {
        ok = FindNextFileA(d->h, &fd) != 0;
    }
    if (!ok) {
        d->done = 1;
        return NULL;
    }
    strcpy_s(d->e.d_name, sizeof(d->e.d_name), fd.cFileName);
    return &d->e;
}

static inline int closedir(DIR* d) {
    if (d == NULL) {
        return -1;
    }
    int ok = d->h != INVALID_HANDLE_VALUE ? (FindClose(d->h) ? 0 : -1) : 0;
    free(d);
    return ok;
}

#endif /* YEP_TEST_COMPAT_DIRENT_H */
