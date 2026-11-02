/* js_suite.c — the official JSON-Schema-Test-Suite driver (#885 item 2).
 *
 * Walks the vendored draft2020-12 fixtures under
 * test/conformance/jsonschema/ (tests/ holds the main files, optional/
 * the divergence section): a file is a JSON array of groups
 * {description, schema, tests:[{description, data, valid}]}.
 * The driver extracts the RAW
 * BYTES of every schema/data value with a bounded JSON span scanner —
 * no re-serialization, so the suite's number shapes (1.0, 1e308) and
 * escapes reach the validator unchanged — and feeds
 * yeptris_json_schema_validate. YEPTRIS_OK where the suite says
 * valid, YEPTRIS_ERROR_SCHEMA where it says invalid; every other
 * status (PARSE/MEMORY/ARG/...) is a failure to investigate.
 *
 * Documented carries ride carries.txt: "file.json" carries a whole
 * file, "file.json :: <description substring>" one case (the
 * substring matches the raw group or test description). A carried
 * failure is expected (remote refs, $id/$anchor machinery,
 * unimplemented keywords — see MATRIX.md); a carried case that now
 * PASSES prints UPASS (a stale carry) without failing the run. Any
 * uncarried failure fails the run.
 *
 * Usage: js_suite CARRIES_FILE DIR [DIR...]
 * Exit: 0 clean (only carried failures), 1 otherwise.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>

#include <yeptris/validate.h>

#define J_MAX_DEPTH 512 /* the fixture scanner's nesting bound */
#define J_MAX_SPAN (64u * 1024u * 1024u)

static long total_pass;
static long total_fail;
static long total_carried;
static long total_upass;
static long total_status;

/* ---- bounded JSON span scanning ------------------------------------------ */

static const char* jws(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
        p++;
    }
    return p;
}

/* The byte length of the JSON value starting at p (p is at a value
 * boundary); 0 malformed/empty. */
static size_t jspan(const char* p, const char* end, int depth) {
    if (depth > J_MAX_DEPTH) {
        return 0;
    }
    p = jws(p, end);
    if (p >= end) {
        return 0;
    }
    const char* start = p;
    if (*p == '"') {
        p++;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p += 2;
            } else {
                p++;
            }
        }
        if (p >= end) {
            return 0;
        }
        return (size_t)(p - start) + 1;
    }
    if (*p == '[' || *p == '{') {
        int lvl = 0;
        while (p < end) {
            char b = *p;
            if (b == '"') { /* strings: brackets inside do not nest */
                p++;
                while (p < end && *p != '"') {
                    if (*p == '\\' && p + 1 < end) {
                        p += 2;
                    } else {
                        p++;
                    }
                }
                if (p >= end) {
                    return 0;
                }
            } else if (b == '[' || b == '{') {
                if (++lvl > J_MAX_DEPTH) {
                    return 0;
                }
            } else if (b == ']' || b == '}') {
                /* any closer pops (the fixtures are well-formed JSON:
                 * the bracket kind matches the innermost open) */
                lvl--;
                if (lvl == 0) {
                    return (size_t)(p - start) + 1;
                }
                if (lvl < 0) {
                    return 0;
                }
            }
            p++;
        }
        return 0;
    }
    /* true / false / null */
    static const char* lits[] = {"true", "false", "null"};
    for (int i = 0; i < 3; i++) {
        size_t ll = strlen(lits[i]);
        if ((size_t)(end - p) >= ll && memcmp(p, lits[i], ll) == 0) {
            return ll;
        }
    }
    /* number */
    if (*p == '-' || *p == '+' || (*p >= '0' && *p <= '9')) {
        p++;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' || *p == 'e' || *p == 'E' ||
                           *p == '+' || *p == '-')) {
            p++;
        }
        return (size_t)(p - start);
    }
    return 0;
}

/* Finds `key` among the top-level members of the object spanning
 * [p,end); vp/ve receive the value span. 0 found, -1 not. */
static int j_member(const char* p, const char* end, const char* key, const char** vp,
                    const char** ve) {
    p = jws(p, end);
    if (p >= end || *p != '{') {
        return -1;
    }
    p++;
    for (;;) {
        p = jws(p, end);
        if (p >= end) {
            return -1;
        }
        if (*p == '}') {
            return -1;
        }
        if (*p != '"') {
            return -1;
        }
        const char* ks = p;
        p++;
        while (p < end && *p != '"') {
            if (*p == '\\' && p + 1 < end) {
                p += 2;
            } else {
                p++;
            }
        }
        if (p >= end) {
            return -1;
        }
        size_t kl = (size_t)(p - ks) - 1;
        p++;
        p = jws(p, end);
        if (p >= end || *p != ':') {
            return -1;
        }
        p++;
        const char* vs = jws(p, end);
        size_t vl = jspan(vs, end, 0);
        if (vl == 0) {
            return -1;
        }
        if (kl == strlen(key) && memcmp(ks + 1, key, kl) == 0) {
            *vp = vs;
            *ve = vs + vl;
            return 0;
        }
        p = vs + vl;
        p = jws(p, end);
        if (p < end && *p == ',') {
            p++;
            continue;
        }
        return -1;
    }
}

/* An array iterator: begin, then next() until it returns 0 (end) or
 * -1 (malformed). */
typedef struct {
    const char* p;
    const char* end;
    int started;
} jarr;

static int jarr_begin(jarr* it, const char* p, const char* end) {
    p = jws(p, end);
    if (p >= end || *p != '[') {
        return -1;
    }
    it->p = p + 1;
    it->end = end;
    it->started = 0;
    return 0;
}

static long jarr_next(jarr* it, const char** elp, const char** ele) {
    const char* p = jws(it->p, it->end);
    if (p >= it->end) {
        return -1;
    }
    if (*p == ']') {
        return 0;
    }
    if (it->started) {
        if (*p != ',') {
            return -1;
        }
        p = jws(p + 1, it->end);
    }
    size_t vl = jspan(p, it->end, 0);
    if (vl == 0) {
        return -1;
    }
    it->started = 1;
    it->p = p + vl;
    *elp = p;
    *ele = p + vl;
    return (long)vl;
}

/* ---- carries -------------------------------------------------------------- */

typedef struct {
    char file[128];
    char needle[192];  /* generic: matches a group OR test description */
    char gneedle[192]; /* two-level form: group description substring */
    char tneedle[192]; /* ... and test description substring */
} carry;

static carry carries[512];
static int carry_cnt;

static void carry_load(const char* path) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        return;
    }
    char line[512];
    while (fgets(line, sizeof(line), f) != NULL &&
           carry_cnt < (int)(sizeof(carries) / sizeof(carries[0]))) {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        carry c;
        memset(&c, 0, sizeof(c));
        char* sep1 = strstr(line, " :: ");
        if (sep1 == NULL) {
            size_t fl = len < sizeof(c.file) - 1 ? len : sizeof(c.file) - 1;
            memcpy(c.file, line, fl);
            c.file[fl] = '\0';
        } else {
            size_t fl = (size_t)(sep1 - line);
            if (fl >= sizeof(c.file)) {
                fl = sizeof(c.file) - 1;
            }
            memcpy(c.file, line, fl);
            c.file[fl] = '\0';
            char* rest = sep1 + 4;
            char* sep2 = strstr(rest, " :: ");
            if (sep2 == NULL) {
                size_t nl = strlen(rest);
                if (nl >= sizeof(c.needle)) {
                    nl = sizeof(c.needle) - 1;
                }
                memcpy(c.needle, rest, nl);
                c.needle[nl] = '\0';
            } else {
                size_t gl = (size_t)(sep2 - rest);
                if (gl >= sizeof(c.gneedle)) {
                    gl = sizeof(c.gneedle) - 1;
                }
                memcpy(c.gneedle, rest, gl);
                c.gneedle[gl] = '\0';
                const char* td = sep2 + 4;
                size_t tl = strlen(td);
                if (tl >= sizeof(c.tneedle)) {
                    tl = sizeof(c.tneedle) - 1;
                }
                memcpy(c.tneedle, td, tl);
                c.tneedle[tl] = '\0';
            }
        }
        carries[carry_cnt++] = c;
    }
    fclose(f);
}

/* memcmp-scan (memmem is not portable — MSVC lacks it). The match
 * must start on a word boundary: "valid tree" must not light up
 * inside "invalid tree". */
static int contains(const char* hay, size_t hay_len, const char* nd, size_t nd_len) {
    if (nd_len == 0) {
        return 1;
    }
    if (nd_len > hay_len) {
        return 0;
    }
    for (size_t i = 0; i + nd_len <= hay_len; i++) {
        if (hay[i] == nd[0] && memcmp(hay + i, nd, nd_len) == 0) {
            if (i > 0) {
                unsigned char prev = (unsigned char)hay[i - 1];
                if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z') ||
                    (prev >= '0' && prev <= '9')) {
                    continue;
                }
            }
            return 1;
        }
    }
    return 0;
}

/* 0 not carried; 1 whole-file carry; 2 per-case carry. "file" alone
 * matches everything in the file, "file :: N" matches the generic
 * needle against a group or test description, "file :: G :: T"
 * requires the group AND test substrings. */
static int carried_kind(const char* fname, const char* gdesc, size_t gdl, const char* tdesc,
                        size_t tdl) {
    for (int i = 0; i < carry_cnt; i++) {
        if (strcmp(carries[i].file, fname) != 0) {
            continue;
        }
        if (carries[i].needle[0] != '\0') {
            size_t nl = strlen(carries[i].needle);
            if ((gdesc != NULL && contains(gdesc, gdl, carries[i].needle, nl)) ||
                (tdesc != NULL && contains(tdesc, tdl, carries[i].needle, nl))) {
                return 2;
            }
            continue;
        }
        if (carries[i].gneedle[0] != '\0' || carries[i].tneedle[0] != '\0') {
            int gok = carries[i].gneedle[0] == '\0' ||
                      (gdesc != NULL &&
                       contains(gdesc, gdl, carries[i].gneedle, strlen(carries[i].gneedle)));
            int tok = carries[i].tneedle[0] == '\0' ||
                      (tdesc != NULL &&
                       contains(tdesc, tdl, carries[i].tneedle, strlen(carries[i].tneedle)));
            if (gok && tok) {
                return 2;
            }
            continue;
        }
        return 1; /* bare file: the whole file is carried */
    }
    return 0;
}

/* ---- the per-file walk ------------------------------------------------------ */

typedef struct {
    int pass;
    int fail;
    int carried;
    int upass;
    int status_fail;
} fstats;

static void status_name(YeptrisStatus st) {
    switch (st) {
    case YEPTRIS_OK:
        printf("OK");
        break;
    case YEPTRIS_ERROR_SCHEMA:
        printf("ERROR_SCHEMA");
        break;
    case YEPTRIS_ERROR_PARSE:
        printf("ERROR_PARSE");
        break;
    case YEPTRIS_ERROR_DEPTH:
        printf("ERROR_DEPTH");
        break;
    case YEPTRIS_ERROR_ENCODING:
        printf("ERROR_ENCODING");
        break;
    case YEPTRIS_ERROR_MEMORY:
        printf("ERROR_MEMORY");
        break;
    case YEPTRIS_ERROR_ARG:
        printf("ERROR_ARG");
        break;
    default:
        printf("status %d", (int)st);
        break;
    }
}

static int process_file(const char* path, const char* fname, fstats* st) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        printf("js_suite: cannot open %s\n", path);
        return -1;
    }
    static char buf[16u * 1024u * 1024u];
    size_t len = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    const char* end = buf + len;

    jarr groups;
    if (jarr_begin(&groups, buf, end) != 0) {
        printf("js_suite: %s is not a JSON array\n", fname);
        return -1;
    }
    const char* gp;
    const char* ge;
    while ((gp = ge = NULL, jarr_next(&groups, &gp, &ge)) > 0) {
        const char* sdp = NULL;
        const char* sde = NULL;
        const char* tsp = NULL;
        const char* tse = NULL;
        const char* gdp = NULL;
        const char* gde = NULL;
        if (j_member(gp, ge, "schema", &sdp, &sde) != 0 ||
            j_member(gp, ge, "tests", &tsp, &tse) != 0) {
            printf("js_suite: %s has a group without schema/tests\n", fname);
            st->fail++;
            continue;
        }
        size_t gsl = (size_t)(sde - sdp);
        if (gsl > J_MAX_SPAN) {
            printf("js_suite: %s schema span too large\n", fname);
            st->fail++;
            continue;
        }
        j_member(gp, ge, "description", &gdp, &gde);
        if (gdp != NULL) {
            gdp++; /* the opening quote */
            gde--; /* the closing quote */
        }
        jarr tests;
        if (jarr_begin(&tests, tsp, tse) != 0) {
            printf("js_suite: %s has a malformed tests array\n", fname);
            st->fail++;
            continue;
        }
        const char* tp;
        const char* te;
        while ((tp = te = NULL, jarr_next(&tests, &tp, &te)) > 0) {
            const char* tdp = NULL;
            const char* tde = NULL;
            const char* dp = NULL;
            const char* de = NULL;
            const char* vp = NULL;
            const char* ve = NULL;
            if (j_member(tp, te, "data", &dp, &de) != 0 ||
                j_member(tp, te, "valid", &vp, &ve) != 0) {
                printf("js_suite: %s has a test without data/valid\n", fname);
                st->fail++;
                continue;
            }
            j_member(tp, te, "description", &tdp, &tde);
            if (tdp != NULL) {
                tdp++;
                tde--;
            }
            size_t dsl = (size_t)(de - dp);
            if (dsl > J_MAX_SPAN) {
                printf("js_suite: %s data span too large\n", fname);
                st->fail++;
                continue;
            }
            int expected = (size_t)(ve - vp) == 4 && memcmp(vp, "true", 4) == 0;
            int kind = carried_kind(fname, gdp, gdp ? (size_t)(gde - gdp) : 0, tdp,
                                    tdp ? (size_t)(tde - tdp) : 0);

            yeptris_validate_report* r = NULL;
            YeptrisStatus got = yeptris_json_schema_validate(sdp, gsl, dp, dsl, &r);
            int ok;
            if (got == YEPTRIS_OK || got == YEPTRIS_ERROR_SCHEMA) {
                ok = (got == YEPTRIS_OK) == expected;
            } else {
                ok = 0; /* a parse/memory/arg status: investigate */
                st->status_fail++;
            }
            if (ok && kind == 2) {
                /* a per-case carry that unexpectedly passes: the carry
                 * is stale — loud, but not a run failure */
                st->upass++;
                printf("UPASS %s :: %.*s :: %.*s passes now — drop the carry\n", fname,
                       gdp ? (int)(gde - gdp) : 4, gdp ? gdp : "????", tdp ? (int)(tde - tdp) : 4,
                       tdp ? tdp : "????");
            } else if (ok) {
                /* whole-file carried files: an agreeing verdict is
                 * coincidental (the machinery is missing) — counted as
                 * pass, explained by the whole-file carry */
                st->pass++;
            } else if (kind != 0) {
                st->carried++;
                printf("carry %s :: %.*s :: %.*s (expected %s)\n", fname,
                       gdp ? (int)(gde - gdp) : 4, gdp ? gdp : "????", tdp ? (int)(tde - tdp) : 4,
                       tdp ? tdp : "????", expected ? "valid" : "invalid");
            } else {
                st->fail++;
                printf("FAIL %s :: %.*s :: %.*s : expected %s, got ", fname,
                       gdp ? (int)(gde - gdp) : 4, gdp ? gdp : "????", tdp ? (int)(tde - tdp) : 4,
                       tdp ? tdp : "????", expected ? "valid" : "invalid");
                status_name(got);
                printf("\n");
                if (r != NULL) {
                    size_t n = yeptris_validate_report_count(r);
                    for (size_t i = 0; i < n && i < 4; i++) {
                        const yeptris_diagnostic* d = yeptris_validate_report_at(r, i);
                        printf("    diag[%zu] code=%u path=%s detail=%s\n", i, d->code, d->path,
                               d->detail);
                    }
                }
            }
            if (r != NULL) {
                yeptris_validate_report_free(r);
            }
        }
    }
    return 0;
}

/* 1 when the file itself is carried bare (a whole-file carry). */
static int file_is_carried(const char* fname) {
    for (int i = 0; i < carry_cnt; i++) {
        if (strcmp(carries[i].file, fname) == 0 && carries[i].needle[0] == '\0' &&
            carries[i].gneedle[0] == '\0' && carries[i].tneedle[0] == '\0') {
            return 1;
        }
    }
    return 0;
}

static int process_dir(const char* dir, int* files) {
    DIR* d = opendir(dir);
    if (d == NULL) {
        printf("js_suite: cannot open dir %s\n", dir);
        return -1;
    }
    struct dirent* ent;
    int rc = 0;
    while ((ent = readdir(d)) != NULL) {
        size_t nl = strlen(ent->d_name);
        if (nl < 6 || strcmp(ent->d_name + nl - 5, ".json") != 0) {
            continue;
        }
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        fstats st;
        memset(&st, 0, sizeof(st));
        if (process_file(path, ent->d_name, &st) != 0) {
            rc = -1;
        }
        if (file_is_carried(ent->d_name)) {
            printf("== %s: %d pass (coincidental agreements included), %d carried, %d FAILED, "
                   "%d bad-status — WHOLE FILE CARRIED\n",
                   ent->d_name, st.pass, st.carried, st.fail, st.status_fail);
        } else {
            printf("== %s: %d pass, %d carried, %d FAILED, %d bad-status, %d upass\n", ent->d_name,
                   st.pass, st.carried, st.fail, st.status_fail, st.upass);
        }
        (*files)++;
        total_pass += st.pass;
        total_fail += st.fail;
        total_carried += st.carried;
        total_upass += st.upass;
        total_status += st.status_fail;
        if (st.fail > 0 || st.status_fail > 0) {
            rc = -1;
        }
    }
    closedir(d);
    return rc;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("usage: js_suite CARRIES_FILE DIR [DIR...]\n");
        return 1;
    }
    carry_load(argv[1]);
    printf("js_suite: %d carries loaded\n", carry_cnt);
    int files = 0;
    int rc = 0;
    for (int a = 2; a < argc; a++) {
        if (process_dir(argv[a], &files) != 0) {
            rc = 1;
        }
    }
    printf("== TOTAL: %d files, %ld pass, %ld carried, %ld FAILED, %ld bad-status, %ld upass\n",
           files, total_pass, total_carried, total_fail, total_status, total_upass);
    if (rc != 0 || total_fail > 0 || total_status > 0) {
        printf("js_suite: FAILURES PRESENT\n");
        return 1;
    }
    printf("js_suite: clean (all failures carried)\n");
    return 0;
}
