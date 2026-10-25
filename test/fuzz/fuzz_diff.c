/* fuzz_diff.c — differential conformance fuzzer (TODO.impl/19).
 *
 * Property: for ANY input, yeptris and libyaml agree — same event
 * stream when both parse (EQUAL), or both reject (BOTH-ERROR).
 * DIFFER or either-only-accept is a conformance divergence and
 * aborts (with the dumps printed for minimization via yepdiff).
 * The classifier is diff_core.c — the same one yepdiff uses, so
 * the fuzzer and the human tool can never disagree about the rule.
 */

/* strdup rides sem_dump.h; strict -std=c11 (the LF harness) hides it
 * without the feature macro (the nightly's round-11 build break) */
#define _POSIX_C_SOURCE 200809L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff_core.h"
#include "sem_dump.h" /* the charset carry re-dumps yeptris on the split input */

/* The corpus basename this input came from (set by the walker), or
 * NULL under libFuzzer — splits must then be ledgered by content. */
static const char* probe_id = NULL;

/* True when the two dumps differ ONLY by a leading content-U+FEFF:
 * libyaml's UTF-16 reader drops a U+FEFF that follows the encoding
 * mark, while its own UTF-8 path KEEPS it (catch 121 matched that
 * twin) and yeptris is consistent across encodings -- one mark
 * stripped, content BOMs ride (the catch-121 law). The suite is
 * silent on the shape. The transform strips one leading EF BB BF
 * from yeptris's first scalar and compares. */
/* True when the difference is a UTF-8 BOM at a LINE START (the
 * nightly's round 60, crash-0f393f90: "\r!= \n<BOM>\ra\r\nt\r%\r\r!"):
 * libyaml strips a BOM that opens any line (its encoding-mark law is
 * not stream-position-bound), yeptris keeps it as CONTENT — and the
 * plain's fold then differs too (yeptris's BOM becomes first-class
 * content with a fold space after it; libyaml's leading break folds
 * away entirely), so the dump-level strip of yd_bom_content_divergence
 * cannot reach it. The probe removes every line-start BOM from the
 * input: the removal is pure to libyaml (it already stripped them)
 * and yeptris reads the BOM-less input exactly as libyaml's original —
 * both engines' dumps of the removal must equal libyaml's original. */
static int yd_bom_line_start_divergence(const uint8_t* data, size_t size, const char* ydump,
                                        const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 6 || size > 8192) {
        return 0;
    }
    int feature = 0;
    unsigned char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    int line_start = 1;
    for (size_t i = 0; i < size;) {
        if (line_start && i + 2 < size && data[i] == 0xEF && data[i + 1] == 0xBB &&
            data[i + 2] == 0xBF) {
            i += 3;
            feature = 1;
            continue;
        }
        sub[o++] = data[i];
        if (data[i] == '\n') {
            line_start = 1;
        } else if (data[i] == '\r') {
            if (i + 1 < size && data[i + 1] == '\n') {
                sub[o++] = data[++i];
            }
            line_start = 1;
        } else {
            line_start = 0;
        }
        i++;
    }
    if (!feature) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, o, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, o, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    free(sub);
    if (known) {
        return 1;
    }
    /* the INDENT twin (the nightly's round 63, crash-c0850abf's
     * sibling: "a:\na:\n<BOM> a:\na:\n<BOM>.b:\n\n" — BOMs as MAP
     * KEYS): libyaml strips a line-start BOM but still counts it for
     * indentation, so "<BOM>.b:" lands INDENTED (a nested map) while
     * yeptris's BOM-as-content keeps the key at column 0. The removal
     * probe above loses the nesting; the replacement probe rides it —
     * each line-start BOM becomes ONE space, and both engines' dumps
     * of that must equal libyaml's original. */
    unsigned char* sub2 = malloc(size + 1);
    if (sub2 == NULL) {
        return 0;
    }
    o = 0;
    line_start = 1;
    for (size_t i = 0; i < size;) {
        if (line_start && i + 2 < size && data[i] == 0xEF && data[i + 1] == 0xBB &&
            data[i + 2] == 0xBF) {
            i += 3;
            sub2[o++] = ' ';
            continue;
        }
        sub2[o++] = data[i];
        if (data[i] == '\n') {
            line_start = 1;
        } else if (data[i] == '\r') {
            if (i + 1 < size && data[i + 1] == '\n') {
                sub2[o++] = data[++i];
            }
            line_start = 1;
        } else {
            line_start = 0;
        }
        i++;
    }
    yok2 = 0;
    ydump2 = yd_sem_dump((const char*)sub2, o, &yok2);
    lok2 = 0;
    ldump2 = yd_libyaml_dump((const char*)sub2, o, &lok2);
    known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
             strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    free(sub2);
    return known;
}

static int yd_bom_content_divergence(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    const char* val = strstr(ydump, "=VAL : \xEF\xBB\xBF");
    if (val == NULL) {
        return 0; /* yeptris's first scalar must open with a content BOM */
    }
    char stripped[8192];
    size_t head = (size_t)(val - ydump) + 7; /* keep "=VAL : " */
    size_t left = strlen(ydump) - head;
    if (left >= sizeof(stripped)) {
        return 0;
    }
    memcpy(stripped, ydump, head); /* up to and incl. "=VAL : " */
    size_t o = head;
    const char* rest = ydump + head;
    if (rest[0] == (char)0xEF && rest[1] == (char)0xBB && rest[2] == (char)0xBF) {
        rest += 3; /* strip ONE content BOM */
    } else {
        return 0;
    }
    memcpy(stripped + o, rest, strlen(rest));
    o += strlen(rest);
    stripped[o] = '\0';
    if (strcmp(stripped, ldump) == 0) {
        return 1;
    }
    /* the value-vanishes twin (the nightly's crash-dbbc083c, two
     * content BOMs): libyaml strips BOTH BOM bytes and emits an EMPTY
     * stream, so the scalar line is GONE from its dump -- drop
     * yeptris's whole scalar line too and compare. */
    const char* line = stripped + (size_t)(val - ydump);
    while (line > stripped && line[-1] != '\n') {
        line--; /* back to the scalar line's start */
    }
    const char* lend = strchr(line, '\n');
    if (lend == NULL) {
        lend = line + strlen(line);
    } else {
        lend++;
    }
    char dropped[8192];
    size_t head2 = (size_t)(line - stripped);
    size_t tail = strlen(lend);
    if (head2 + tail < sizeof(dropped)) {
        memcpy(dropped, stripped, head2);
        memcpy(dropped + head2, lend, tail + 1);
        return strcmp(dropped, ldump) == 0;
    }
    return 0;
}

/* True when the two dumps differ ONLY by the flow-'?' family: libyaml
 * (pinned 90a56d4, 2011) makes a KEY token for a flow '?' with any
 * follower ("[?p]" -> {p: null}), while yaml-test-suite pins the
 * plain reading -- HM87 "[?x]" -> "=VAL :?x", 652Z "{ ?foo: bar }" ->
 * "=VAL :?foo" -- and yeptris follows the suite (the conformance
 * bar). The transform rewrites each libyaml single-pair map block
 * "+MAP / =VAL :X / =VAL : / -MAP" into "=VAL :?X" and compares. */
static int yd_qflow_divergence(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    /* the line arrays are HEAP-SIZED: fixed 512-line caps silently
     * truncated the walk mid-stream once a dump grew past them (the
     * nightly's round 43 artifact walks 644/557 lines -- every member
     * was covered, but the truncated walk hit the cap's misaligned
     * tail and the carry declined its own family) */
    size_t n_cap = 1;
    for (const char* q = ldump; *q != '\0'; q++) {
        if (*q == '\n') {
            n_cap++;
        }
    }
    size_t yn_cap = 1;
    for (const char* q = ydump; *q != '\0'; q++) {
        if (*q == '\n') {
            yn_cap++;
        }
    }
    const char** lines = malloc(n_cap * sizeof(*lines));
    size_t* lens = malloc(n_cap * sizeof(*lens));
    const char** ylines = malloc(yn_cap * sizeof(*ylines));
    size_t* ylens = malloc(yn_cap * sizeof(*ylens));
    if (lines == NULL || lens == NULL || ylines == NULL || ylens == NULL) {
        free(lines);
        free(lens);
        free(ylines);
        free(ylens);
        return 0;
    }
    size_t o = 0;
    int n = 0;
    const char* p = ldump;
    while (*p != '\0') {
        lines[n] = p;
        const char* nl = strchr(p, '\n');
        lens[n] = nl ? (size_t)(nl - p) : strlen(p);
        n++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    /* the yeptris dump, split into lines for the parallel walk */
    int yn = 0;
    p = ydump;
    while (*p != '\0') {
        ylines[yn] = p;
        const char* nl = strchr(p, '\n');
        ylens[yn] = nl ? (size_t)(nl - p) : strlen(p);
        yn++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    /* The counts legitimately differ: libyaml carries the +MAP/-MAP
     * wrappers (and sometimes the empty value line) that yeptris
     * flattens away; the walk below reconciles them. Parallel walk:
     * the dumps must be line-identical EXCEPT that yeptris spells a
     * flow-'?' key with a '?' prefix where libyaml consumed it, and
     * libyaml wraps the entry in +MAP/-MAP that yeptris flattens.
     * Both pair shapes ride: the single-pair
     * "+MAP / =VAL :X / =VAL : / -MAP" block (the classic [?x]) and
     * the multi-pair map where only the KEY lines carry the '?'
     * ("{\n?(e etry,\n?\n}", the nightly's crash-36f7f5f9). Every
     * branch advances j itself; the loop header never does. */
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (j >= yn) {
            free(lines);
            free(lens);
            free(ylines);
            free(ylens);
            return 0;
        }
        /* the single-pair wrapper block */
        if (lens[i] == 4 && strncmp(lines[i], "+MAP", 4) == 0 && i + 3 < n && lens[i + 1] >= 8 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && lens[i + 2] == 7 &&
            strncmp(lines[i + 2], "=VAL : ", 7) == 0 && lens[i + 3] == 4 &&
            strncmp(lines[i + 3], "-MAP", 4) == 0 && ylens[j] >= 8 &&
            strncmp(ylines[j], "=VAL : ?", 8) == 0 && ylens[j] == 8 + (lens[i + 1] - 7) &&
            memcmp(ylines[j] + 8, lines[i + 1] + 7, lens[i + 1] - 7) == 0) {
            i += 3; /* +MAP, the key, the empty value; -MAP rides the ++ */
            j++;    /* yeptris's flat key line */
            if (j < yn && ylens[j] == 7 && strncmp(ylines[j], "=VAL : ", 7) == 0) {
                j++; /* the empty value line, when yeptris kept it */
            }
            continue;
        }
        /* the key-line twin inside a bigger map: yeptris's value
         * carries the leading '?' libyaml consumed — including the
         * EMPTY key ("{?:\n}" -> libyaml key "", yeptris key "?";
         * the nightly's run 39534504716) */
        if (lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 && ylens[j] == lens[i] + 1 &&
            strncmp(ylines[j], "=VAL : ?", 8) == 0 &&
            memcmp(ylines[j] + 8, lines[i] + 7, lens[i] - 7) == 0) {
            j++;
            continue;
        }
        /* the VALUE-position colon rider ("[?[] :b]", the nightly's
         * round 46): after an explicit key's value, libyaml consumes
         * the leading ':' of a plain-first ":b" as the pair separator
         * while the suite pins the WHOLE ':b' as the plain (HM87
         * class) -- libyaml's (value b) folds onto yeptris's
         * (value :b). */
        if (lens[i] >= 8 && strncmp(lines[i], "=VAL : ", 7) == 0 && ylens[j] == lens[i] + 1 &&
            strncmp(ylines[j], "=VAL : :", 8) == 0 &&
            memcmp(ylines[j] + 8, lines[i] + 7, lens[i] - 7) == 0) {
            j++;
            continue;
        }
        if (lens[i] == 4 && strncmp(lines[i], "-MAP", 4) == 0 && j + 1 < yn && ylens[j] == 7 &&
            strncmp(ylines[j], "=VAL : ", 7) == 0) {
            /* the in-map null-pair spill ("[?[] :b,]", the nightly's
             * round 46; the round-48 soup composes SEVERAL): the ','
             * spills implicit null members inside the SAME map while
             * libyaml ends it. Fold every trailing run of exact
             * null:null pairs, then a lone null key when the closer
             * arrived before any value; the closer aligns below. */
            while (j + 1 < yn && ylens[j] == 7 && strncmp(ylines[j], "=VAL : ", 7) == 0 &&
                   ylens[j + 1] == 7 && strncmp(ylines[j + 1], "=VAL : ", 7) == 0) {
                j += 2;
            }
            if (j < yn && ylens[j] == 7 && strncmp(ylines[j], "=VAL : ", 7) == 0 && j + 1 < yn &&
                ylens[j + 1] == 4 && strncmp(ylines[j + 1], "-MAP", 4) == 0) {
                j += 1; /* the lone key */
            }
            if (!(j < yn && ylens[j] == 4 && strncmp(ylines[j], "-MAP", 4) == 0)) {
                free(lines);
                free(lens);
                free(ylines);
                free(ylens);
                return 0;
            }
        }
        /* a fold leaves the closer itself aligned: run the equality */
        if (lens[i] != ylens[j] || memcmp(lines[i], ylines[j], lens[i]) != 0) {
            free(lines);
            free(lens);
            free(ylines);
            free(ylens);
            return 0;
        }
        j++;
    }
    int known = j == yn; /* trailing yeptris lines: not the ?-family shape */
    free(lines);
    free(lens);
    free(ylines);
    free(ylens);
    return known;
}

/* True when the dumps differ ONLY by a flow explicit key that STARTS
 * with ':' adjacent to a non-space ("{?\n:cic l}", the nightly's run
 * 39469989528): libyaml's scanner makes a VALUE token of that leading
 * ':' (an EMPTY key plus the value "cic l"), while YAML 1.2
 * ns-plain-first admits ":cic l" as a plain scalar — the suite pins
 * that reading (HM87 "[:x]" -> "=VAL ::x"; 58MP "{x: :x}" ->
 * "=VAL ::x"). yeptris follows the suite.
 * The transform folds libyaml's empty-key line + value line into
 * yeptris's single ":X" key line plus its empty value line. */
static int yd_qcolon_walk(const char* ldump, const char* ydump2) {
    if (ldump == NULL || ydump2 == NULL) {
        return 0;
    }
    const char* lines[512];
    size_t lens[512];
    int n = 0;
    const char* p = ldump;
    while (*p != '\0' && n < 512) {
        lines[n] = p;
        const char* nl = strchr(p, '\n');
        lens[n] = nl ? (size_t)(nl - p) : strlen(p);
        n++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    const char* ylines[512];
    size_t ylens[512];
    int yn = 0;
    p = ydump2;
    while (*p != '\0' && yn < 512) {
        ylines[yn] = p;
        const char* nl = strchr(p, '\n');
        ylens[yn] = nl ? (size_t)(nl - p) : strlen(p);
        yn++;
        p = nl ? nl + 1 : p + strlen(p);
    }
    int j = 0;
    for (int i = 0; i < n; i++) {
        if (j >= yn) {
            return 0;
        }
        /* libyaml: empty key line (with optional node props — the
         * anchor-charset composition's "&a"), then the value;
         * yeptris: one ":X" key line carrying the same props, then
         * its empty value line */
        if (lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 &&
            (lens[i] == 7 || lines[i][lens[i] - 1] == ' ') && i + 1 < n && lens[i + 1] >= 8 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0) {
            size_t keylen = lens[i] + 1 + (lens[i + 1] - 7); /* props + ':' + X */
            if (ylens[j] == keylen && memcmp(ylines[j], lines[i], lens[i]) == 0 &&
                ylines[j][lens[i]] == ':' &&
                memcmp(ylines[j] + lens[i] + 1, lines[i + 1] + 7, lens[i + 1] - 7) == 0 &&
                j + 1 < yn && ylens[j + 1] == 7 && strncmp(ylines[j + 1], "=VAL : ", 7) == 0) {
                i++;
                j += 2;
                continue;
            }
        }
        /* the ?-COLON-VALUE twin (the nightly's round 38,
         * "{\n?:2,\n  foo :2,\n ---barM,\n}"): libyaml makes the
         * flow '?' an explicit-key indicator with ANY follower, so its
         * EMPTY key takes the ':2' as the VALUE ({empty => 2}), while
         * the suite's plain-first law keeps '?:2' ONE plain KEY with
         * an empty value (HM87/58MP): the ':' rides yeptris's plain,
         * so libyaml's (empty key, V) folds onto yeptris's ('?:V'
         * plain key, empty value). */
        if (i + 1 < n && lens[i] == 7 && strncmp(lines[i], "=VAL : ", 7) == 0 && lens[i + 1] > 7 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && j + 1 < yn &&
            ylens[j] == 9 + (lens[i + 1] - 7) && strncmp(ylines[j], "=VAL : ?:", 9) == 0 &&
            memcmp(ylines[j] + 9, lines[i + 1] + 7, lens[i + 1] - 7) == 0 && ylens[j + 1] == 7 &&
            strncmp(ylines[j + 1], "=VAL : ", 7) == 0) {
            i++;    /* the loop's own ++ completes the pair's two lines */
            j += 2; /* yeptris's '?:V' key and its empty value */
            continue;
        }
        /* the ?-single-pair twin (yd_qflow_divergence's block shape):
         * one artifact can COMPOSE both family members -- the '? ,'
         * comma spill pairs AND a trailing '?9' plain-vs-pair block
         * (yeptris#713, "[10 a , ? ,:50~ , a , ?9]") -- so this walk
         * must explain the ?-blocks as well; yd_qflow_divergence alone
         * stops at the spill, this one alone stopped at the block */
        if (lens[i] == 4 && strncmp(lines[i], "+MAP", 4) == 0 && i + 3 < n && lens[i + 1] >= 8 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && lens[i + 2] == 7 &&
            strncmp(lines[i + 2], "=VAL : ", 7) == 0 && lens[i + 3] == 4 &&
            strncmp(lines[i + 3], "-MAP", 4) == 0 && ylens[j] >= 8 &&
            strncmp(ylines[j], "=VAL : ?", 8) == 0 && ylens[j] == 8 + (lens[i + 1] - 7) &&
            memcmp(ylines[j] + 8, lines[i + 1] + 7, lens[i + 1] - 7) == 0) {
            i += 3; /* +MAP, the key, the empty value; -MAP rides the ++ */
            j++;    /* yeptris's flat '?X' key line */
            if (j < yn && ylens[j] == 7 && strncmp(ylines[j], "=VAL : ", 7) == 0) {
                j++; /* the empty value line, when yeptris kept it */
            }
            continue;
        }
        /* the BREAK-led twin (crash-2bbb3db7, "[920, a, ? ,:\n 5l 8]"):
         * the ':' after the '? ,' comma rides a line break, so
         * yeptris's spill is a SECOND implicit pair (+MAP, empty key,
         * the value, -MAP) instead of the same-line ':V' plain -- a
         * plain cannot start there (the ':' is followed by a break).
         * libyaml's one pair folds onto yeptris's closed pair plus
         * the spilled pair. */
        if (i + 3 < n && lens[i] == 4 && strncmp(lines[i], "+MAP", 4) == 0 && lens[i + 1] >= 7 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 &&
            (lens[i + 1] == 7 || lines[i + 1][lens[i + 1] - 1] == ' ') && lens[i + 2] > 7 &&
            strncmp(lines[i + 2], "=VAL : ", 7) == 0 && lens[i + 3] == 4 &&
            strncmp(lines[i + 3], "-MAP", 4) == 0 && j + 7 < yn && ylens[j] == 4 &&
            strncmp(ylines[j], "+MAP", 4) == 0 && ylens[j + 1] == lens[i + 1] &&
            memcmp(ylines[j + 1], lines[i + 1], lens[i + 1]) == 0 && ylens[j + 2] == 7 &&
            strncmp(ylines[j + 2], "=VAL : ", 7) == 0 && ylens[j + 3] == 4 &&
            strncmp(ylines[j + 3], "-MAP", 4) == 0 && ylens[j + 4] == 4 &&
            strncmp(ylines[j + 4], "+MAP", 4) == 0 && ylens[j + 5] == 7 &&
            strncmp(ylines[j + 5], "=VAL : ", 7) == 0 && ylens[j + 6] == lens[i + 2] &&
            memcmp(ylines[j + 6], lines[i + 2], lens[i + 2]) == 0 && ylens[j + 7] == 4 &&
            strncmp(ylines[j + 7], "-MAP", 4) == 0) {
            i += 3; /* the loop's own ++ completes the pair's four lines */
            j += 8; /* the closed pair (4) plus the spilled pair (4) */
            continue;
        }
        /* the VALUE-side twin (the nightly's round 26, crash-e43f206c:
         * "[100, 120, 12.00, 120, a\n \n, ? ,:50 ,1,3- 9]"): after a
         * '?'-opened pair terminated by ',', libyaml reads the
         * colon-led next entry as the pair's VALUE token (empty key ->
         * "50"), yeptris closes the pair empty and keeps ":50" plain
         * (the same ns-plain-first law, HM87/58MP). libyaml's pair
         * (key, V) folds onto yeptris's (key, empty, ":V"). */
        if (i + 2 < n && lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 &&
            (lens[i] == 7 || lines[i][lens[i] - 1] == ' ') && lens[i + 1] > 7 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && j + 3 < yn && ylens[j] == lens[i] &&
            memcmp(ylines[j], lines[i], lens[i]) == 0 && ylens[j + 1] == 7 &&
            strncmp(ylines[j + 1], "=VAL : ", 7) == 0 && ylens[j + 2] == lens[i + 2] &&
            memcmp(ylines[j + 2], lines[i + 2], lens[i + 2]) == 0 &&
            ylens[j + 3] == 8 + (lens[i + 1] - 7) && strncmp(ylines[j + 3], "=VAL : :", 8) == 0 &&
            memcmp(ylines[j + 3] + 8, lines[i + 1] + 7, lens[i + 1] - 7) == 0) {
            i += 2; /* + the loop's own i++ = the pair's three lines */
            j += 4;
            continue;
        }
        if (lens[i] != ylens[j] || memcmp(lines[i], ylines[j], lens[i]) != 0) {
            return 0;
        }
        j++;
    }
    return j == yn;
}

/* True when the difference is the flow-'?' + direct-colon family
 * (the nightly's round 58, crash-40e24e7f: "{\r?:\":b\"}" — the CR is
 * not load-bearing, the one-line {?:"b"} fires identically):
 * libyaml's scanner makes '?' a KEY token even glued to its follower,
 * and the ':' immediately after then reads as the VALUE indicator of
 * an implicit-empty key, so {?:"b"} parses as {~: b}; yeptris's pinned
 * plain reading (HM87/652Z) keeps '?' as plain content, so the whole
 * token is the KEY and the value is empty. The probe rewrites "?:"
 * into the quoted-empty-key form both engines read identically
 * (libyaml REJECTS the bare "{ :X}" empty implicit key; yeptris reads
 * the ':' as plain content there, so ':' alone cannot carry the
 * rewrite) and requires both engines' dumps of it to equal libyaml's
 * original. */
/* True when the difference is an ANCHOR whose follower glues to its
 * name (the nightly's round 62, crash-45b843db: "- &-:x+"): libyaml's
 * anchor charset is [0-9A-Za-z_-] and it ACCEPTS a ':' (or any
 * non-blank) right after the name, so &- anchors '-' and the plain
 * ":x+" follows; yeptris's props scan requires a separator after the
 * name and its plain-first guard bails the WHOLE token to plain
 * ("&-:x+"). The probe spaces the follower out ("&- :x+"): both
 * engines then read anchor '-' + plain ":x+", so both engines' dumps
 * of the spacing must equal libyaml's original. Block context only —
 * the flow-'&'-colon composite rides yd_flow_anchor_colon_divergence. */
static int yd_anchor_glued_follower_divergence(const uint8_t* data, size_t size, const char* ydump,
                                               const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 4 || size > 8192) {
        return 0;
    }
    size_t amp = (size_t)-1;
    size_t name_end = 0;
    size_t split_at = 0;
    int depth = 0;
    int quote = 0;
    int node_pos = 1; /* line start / after a blank or '- ' */
    int line_only_dash = 1;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\') {
                i++;
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            node_pos = 0;
            continue;
        }
        if ((c == '\'' || c == '"') && !(i > 0 && ((data[i - 1] >= 'a' && data[i - 1] <= 'z') ||
                                                   (data[i - 1] >= 'A' && data[i - 1] <= 'Z') ||
                                                   (data[i - 1] >= '0' && data[i - 1] <= '9') ||
                                                   data[i - 1] == '_' || data[i - 1] == '-'))) {
            /* quotes open only at token starts: a mid-word quote is
             * plain CONTENT (the catch-85 word-start law) — "k-':"
             * kept the tracker inside a phantom quote and the '&' on
             * the next line never gated */
            quote = (c == '\'') ? '\'' : '"';
            node_pos = 0;
            continue;
        }
        if (c == '#' && !node_pos) {
            /* a comment swallows the rest of the line */
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                i++;
            }
            if (i < size) {
                i--;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                i++;
            }
            node_pos = 1;
            line_only_dash = 1;
            depth = 0;
            continue;
        }
        if (c == ' ' || c == '\t') {
            continue;
        }
        if (c == '-' && node_pos) {
            line_only_dash = 0;
            node_pos = 1; /* '- ' keeps node position */
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
            node_pos = 1;
            line_only_dash = 0;
            continue;
        }
        if (c == ']' || c == '}') {
            depth--;
            node_pos = 0;
            continue;
        }
        if (c == '&' && depth == 0 && node_pos) {
            size_t j = i + 1;
            while (j < size &&
                   ((data[j] >= 'a' && data[j] <= 'z') || (data[j] >= 'A' && data[j] <= 'Z') ||
                    (data[j] >= '0' && data[j] <= '9') || data[j] == '_' || data[j] == '-')) {
                j++;
            }
            if (j > i + 1 && j < size && data[j] != ' ' && data[j] != '\t' && data[j] != '\n' &&
                data[j] != '\r' && data[j] != ',' && data[j] != '[' && data[j] != ']' &&
                data[j] != '{' && data[j] != '}' && data[j] != '#') {
                amp = i;
                name_end = j;
                split_at = j;
                break;
            }
        }
        node_pos = 0;
        line_only_dash = 0;
    }
    if (amp == (size_t)-1) {
        return 0;
    }
    (void)line_only_dash;
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    memcpy(sub, data, split_at);
    sub[split_at] = ' ';
    memcpy(sub + split_at + 1, data + split_at, size - split_at);
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, size + 1, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, size + 1, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    free(sub);
    return known;
}

static int yd_qflow_question_colon_divergence(const uint8_t* data, size_t size, const char* ydump,
                                              const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    size_t qc = (size_t)-1;
    int depth = 0;
    int quote = 0;
    for (size_t i = 0; i + 2 < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\') {
                i++;
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            continue;
        }
        if (c == '\'') {
            quote = '\'';
            continue;
        }
        if (c == '"') {
            quote = '"';
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
        } else if (c == ']' || c == '}') {
            depth--;
        } else if (c == '?' && depth > 0 && data[i + 1] == ':' &&
                   (data[i + 2] == '"' || data[i + 2] == '\'')) {
            qc = i;
            break;
        }
    }
    if (qc == (size_t)-1) {
        return 0;
    }
    char* sub = malloc(size + 2);
    if (sub == NULL) {
        return 0;
    }
    memcpy(sub, data, qc);
    sub[qc] = '"';
    sub[qc + 1] = '"';
    sub[qc + 2] = data[qc + 1]; /* the ':' */
    sub[qc + 3] = ' ';
    memcpy(sub + qc + 4, data + qc + 2, size - qc - 2);
    size_t subn = size + 2;
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    free(sub);
    return known;
}

static int yd_qflow_colon_divergence(const uint8_t* data, size_t size, const char* ydump,
                                     const char* ldump) {
    if (ydump == NULL || ldump == NULL || size == 0) {
        return 0;
    }
    int has_q = 0;
    for (size_t q = 0; q < size; q++) {
        if (data[q] == '?') {
            has_q = 1;
            break;
        }
    }
    if (!has_q) {
        return 0; /* the family is the explicit-key context only */
    }
    return yd_qcolon_walk(ldump, ydump);
}

/* True when the difference is exactly the documented LS/PS break-
 * normalization family. yeptris normalizes LS/PS (U+2028/9) to '\n'
 * BEFORE the parse; libyaml treats them as breaks in block context
 * but keeps them as RAW CONTENT inside quoted scalars (Psych:
 * k: "a\xe2\x80\xa8b" -> bytes 97 226 128 168 98; NEL folds inside
 * quotes in both engines, so only LS/PS diverge). The transform:
 * substitute LS/PS -> '\n' in a copy of the input; when libyaml's
 * dump of THAT equals yeptris's dump of the original, the divergence
 * is exactly this family. The full quote-aware normalization slice
 * (the front end cannot see quote state) is a filed follow-up. */
/* True when the difference is the raw-NEL line-break law (the
 * nightly's round 47, crash-48e46b87: CR+NEL inside a double-quoted
 * scalar): YAML 1.1 (libyaml) counts NEL (U+0085) as a line break --
 * CR+NEL normalizes to one '\n' -- while YAML 1.2 drops NEL from the
 * break set, so yeptris folds the CR to the flow-fold space. The two
 * readings are the schema difference: equal-length dumps whose every
 * differing byte is exactly ' ' vs '\n'. */
static int yd_nel_break_divergence(const uint8_t* data, size_t size, const char* ydump,
                                   const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int has_nel = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_nel = 1;
            break;
        }
    }
    if (!has_nel) {
        return 0;
    }
    /* the dumps ESCAPE: a real newline renders as the two chars "\n"
     * while yeptris's fold space renders as one ' ' -- align with
     * lookahead on the escaped form */
    int diffs = 0;
    for (const char* a = ydump; *a;) {
        if (*a == *ldump) {
            a++;
            ldump++;
            continue;
        }
        if (a[0] == ' ' && ldump[0] == '\\' && ldump[1] == 'n') {
            a += 1;
            ldump += 2;
            diffs++;
            continue;
        }
        if (a[0] == '\\' && a[1] == 'n' && ldump[0] == ' ') {
            a += 2;
            ldump += 1;
            diffs++;
            continue;
        }
        return 0;
    }
    return diffs > 0;
}

static int yd_breaknorm_divergence(const uint8_t* data, size_t size, const char* ydump) {
    if (ydump == NULL) {
        return 0;
    }
    /* The UTF-16 twin (the nightly's run 39258457271, crash-bad91ebf:
     * "fe ff 2d 3a 20 28 62 2d 2d 2b" — UTF-16BE whose U+2028 is the
     * 20 28 UNIT): the raw bytes never contain the UTF-8 LS/PS
     * triple, so the scan below misses the family through the
     * transcode front-end. Decode the BOM'd UTF-16 to UTF-8 with
     * LS/PS substituted by '\n' and apply the same transform law. */
    if (size >= 4 && size % 2 == 0) {
        int be = -1;
        if (data[0] == 0xFE && data[1] == 0xFF) {
            be = 1;
        } else if (data[0] == 0xFF && data[1] == 0xFE) {
            be = 0;
        }
        if (be >= 0) {
            char* sub = malloc(size * 2);
            if (sub != NULL) {
                size_t o = 0;
                int decodable = 1;
                for (size_t i = 2; i + 1 < size && decodable;) {
                    uint32_t u = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                                    : (uint32_t)((data[i + 1] << 8) | data[i]);
                    i += 2;
                    if (u >= 0xD800 && u <= 0xDBFF && i + 1 < size) {
                        uint32_t lo = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                                         : (uint32_t)((data[i + 1] << 8) | data[i]);
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                            i += 2;
                        } else {
                            decodable = 0; /* lone high surrogate */
                            break;
                        }
                    } else if (u >= 0xD800 && u <= 0xDFFF) {
                        decodable = 0;
                        break;
                    }
                    if (u == 0x2028 || u == 0x2029) {
                        sub[o++] = '\n';
                    } else if (u < 0x80) {
                        sub[o++] = (char)u;
                    } else if (u < 0x800) {
                        sub[o++] = (char)(0xC0 | (u >> 6));
                        sub[o++] = (char)(0x80 | (u & 0x3F));
                    } else if (u < 0x10000) {
                        sub[o++] = (char)(0xE0 | (u >> 12));
                        sub[o++] = (char)(0x80 | ((u >> 6) & 0x3F));
                        sub[o++] = (char)(0x80 | (u & 0x3F));
                    } else {
                        sub[o++] = (char)(0xF0 | (u >> 18));
                        sub[o++] = (char)(0x80 | ((u >> 12) & 0x3F));
                        sub[o++] = (char)(0x80 | ((u >> 6) & 0x3F));
                        sub[o++] = (char)(0x80 | (u & 0x3F));
                    }
                }
                int known = 0;
                if (decodable) {
                    int ok2 = 0;
                    char* ldump2 = yd_libyaml_dump(sub, o, &ok2);
                    known = (ldump2 != NULL && ok2 && strcmp(ydump, ldump2) == 0);
                    free(ldump2);
                }
                free(sub);
                if (known) {
                    return 1;
                }
                /* the plain-FOLD reading (the nightly's round 21,
                 * crash-1ae93822: U+2028/9 units inside a plain
                 * scalar): the spec's b-char law folds breaks — one
                 * break to a space, a run of n to n-1 newlines — and
                 * yeptris follows it, while this libyaml keeps LS/PS
                 * raw mid-plain. The decode with each LS/PS replaced
                 * by a literal newline re-enters libyaml's OWN fold
                 * law (1 break folds to space, n to n-1), so its dump
                 * of the substitution must equal yeptris's original
                 * for every plain-fold shape (single PS round 21;
                 * PS PS double-break round 21's composite tail). */
                if (decodable) {
                    char* sub2 = malloc(size * 2);
                    if (sub2 != NULL) {
                        size_t o2 = 0;
                        int dec2 = 1;
                        /* decode from byte 0: the leading U+FEFF unit
                         * rides as content, so libyaml's own UTF-8-BOM
                         * strip reproduces yeptris's single encoding-BOM
                         * strip on both sides of the comparison. */
                        for (size_t i = 0; i + 1 < size && dec2;) {
                            uint32_t u = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                                            : (uint32_t)((data[i + 1] << 8) | data[i]);
                            i += 2;
                            if (u >= 0xD800 && u <= 0xDBFF && i + 1 < size) {
                                uint32_t lo = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                                                 : (uint32_t)((data[i + 1] << 8) | data[i]);
                                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                    u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                                    i += 2;
                                } else {
                                    dec2 = 0;
                                    break;
                                }
                            } else if (u >= 0xD800 && u <= 0xDFFF) {
                                dec2 = 0;
                                break;
                            }
                            if (u == 0x2028 || u == 0x2029) {
                                sub2[o2++] = '\n';
                            } else if (u < 0x80) {
                                sub2[o2++] = (char)u;
                            } else if (u < 0x800) {
                                sub2[o2++] = (char)(0xC0 | (u >> 6));
                                sub2[o2++] = (char)(0x80 | (u & 0x3F));
                            } else if (u < 0x10000) {
                                sub2[o2++] = (char)(0xE0 | (u >> 12));
                                sub2[o2++] = (char)(0x80 | ((u >> 6) & 0x3F));
                                sub2[o2++] = (char)(0x80 | (u & 0x3F));
                            } else {
                                sub2[o2++] = (char)(0xF0 | (u >> 18));
                                sub2[o2++] = (char)(0x80 | ((u >> 12) & 0x3F));
                                sub2[o2++] = (char)(0x80 | ((u >> 6) & 0x3F));
                                sub2[o2++] = (char)(0x80 | (u & 0x3F));
                            }
                        }
                        int known2 = 0;
                        if (dec2) {
                            int ok3 = 0;
                            char* ldump3 = yd_libyaml_dump(sub2, o2, &ok3);
                            /* direct agreement, or the BOM-content twin:
                             * libyaml additionally strips the content BOM
                             * that opens sub2's first scalar (its stream +
                             * document-start BOM law), so the composed
                             * BOM carry must also hold. */
                            known2 = (ldump3 != NULL && ok3 &&
                                      (strcmp(ydump, ldump3) == 0 ||
                                       yd_bom_content_divergence(ydump, ldump3)));
                            free(ldump3);
                        }
                        free(sub2);
                        if (known2) {
                            return 1;
                        }
                    }
                }
            }
        }
    }

    if (ydump == NULL) {
        return 0;
    }
    int has = 0;
    for (size_t i = 0; i + 2 < size; i++) {
        if ((unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            has = 1;
            break;
        }
    }
    if (!has) {
        return 0; /* no LS/PS: some other divergence */
    }
    char* sub = malloc(size);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < size;) {
        if (i + 2 < size && (unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            sub[o++] = '\n';
            i += 3;
        } else {
            sub[o++] = (char)data[i++];
        }
    }
    int ok2 = 0;
    char* ldump2 = yd_libyaml_dump(sub, o, &ok2);
    free(sub);
    if (ldump2 == NULL || !ok2) {
        free(ldump2);
        return 0;
    }
    int known = strcmp(ydump, ldump2) == 0;
    free(ldump2);
    return known;
}

/* True when the divergence is the COMPOSITION of the two documented
 * families above: a content U+FEFF riding the encoding mark and/or
 * yeptris's LS/PS -> '\n' normalization. The transforms cannot be
 * applied to the dumps independently because the BOM changes whether
 * the breaks after it are leading -- libyaml drops a BOM at a line
 * start, while yeptris keeps it as content, so its following break
 * folds to a space (crash-feee50a2). The decisive test: rebuild the
 * input -- UTF-16 decoded to UTF-8, the leading content BOM dropped,
 * LS/PS substituted by '\n' -- and require BOTH engines to agree on
 * it (the nightly's crash-52fe8bd6 and crash-feee50a2). */
static int yd_composed_break_divergence(const uint8_t* data, size_t size) {
    int be = -1;
    if (size >= 4 && size % 2 == 0) {
        if (data[0] == 0xFE && data[1] == 0xFF) {
            be = 1;
        } else if (data[0] == 0xFF && data[1] == 0xFE) {
            be = 0;
        }
    }
    unsigned char* out = NULL;
    size_t o = 0;
    int feature = 0;
    if (be >= 0) {
        out = malloc(size * 2 + 1);
        if (out == NULL) {
            return 0;
        }
        size_t i = 2;
        if (i + 1 < size) {
            uint32_t u0 = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                             : (uint32_t)((data[i + 1] << 8) | data[i]);
            if (u0 == 0xFEFF) {
                feature = 1;
                i += 2; /* libyaml drops it at the line start */
            }
        }
        while (i + 1 < size) {
            uint32_t u = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                            : (uint32_t)((data[i + 1] << 8) | data[i]);
            i += 2;
            if (u >= 0xD800 && u <= 0xDBFF) {
                if (i + 1 >= size) {
                    free(out);
                    return 0;
                }
                uint32_t lo = be ? (uint32_t)((data[i] << 8) | data[i + 1])
                                 : (uint32_t)((data[i + 1] << 8) | data[i]);
                if (lo < 0xDC00 || lo > 0xDFFF) {
                    free(out);
                    return 0;
                }
                u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00);
                i += 2;
            } else if (u >= 0xDC00 && u <= 0xDFFF) {
                free(out);
                return 0;
            }
            if (u == 0x2028 || u == 0x2029) {
                u = '\n';
                feature = 1;
            }
            if (u < 0x80) {
                out[o++] = (unsigned char)u;
            } else if (u < 0x800) {
                out[o++] = (unsigned char)(0xC0 | (u >> 6));
                out[o++] = (unsigned char)(0x80 | (u & 0x3F));
            } else if (u < 0x10000) {
                out[o++] = (unsigned char)(0xE0 | (u >> 12));
                out[o++] = (unsigned char)(0x80 | ((u >> 6) & 0x3F));
                out[o++] = (unsigned char)(0x80 | (u & 0x3F));
            } else {
                out[o++] = (unsigned char)(0xF0 | (u >> 18));
                out[o++] = (unsigned char)(0x80 | ((u >> 12) & 0x3F));
                out[o++] = (unsigned char)(0x80 | ((u >> 6) & 0x3F));
                out[o++] = (unsigned char)(0x80 | (u & 0x3F));
            }
        }
    } else {
        size_t start = 0;
        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
            start = 3; /* the head mark: both engines drop it */
            if (size >= 6 && data[3] == 0xEF && data[4] == 0xBB && data[5] == 0xBF) {
                start = 6; /* the column-0 content BOM: libyaml drops it too */
                feature = 1;
            }
        }
        out = malloc(size + 1);
        if (out == NULL) {
            return 0;
        }
        for (size_t i = start; i < size;) {
            if (i + 2 < size && data[i] == 0xE2 && data[i + 1] == 0x80 &&
                (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
                out[o++] = '\n';
                i += 3;
                feature = 1;
            } else {
                out[o++] = data[i++];
            }
        }
    }
    if (!feature) {
        free(out);
        return 0;
    }
    char* tdump = NULL;
    char* tldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)out, o, &tdump, &tldump);
    free(tdump);
    free(tldump);
    free(out);
    return v == YD_EQUAL;
}

/* True when the difference is the libyaml root-block-scalar quirk the
 * suite overrules: content at column 0 under a column-0 block scalar
 * header IS content (W4TN, M7A3: "%!PS-Adobe-2.0"; T26H/DWX9; FP8R/
 * DK3J: "--- >" + "line1"), but libyaml ends the scalar at the first
 * unindented line and reports an empty value (crash-5caf49d3:
 * "|-" + "##"). Indent every column-0 content line by one space --
 * which changes no content byte -- and require libyaml's dump of THAT
 * to equal yeptris's dump of the original. */
static int yd_clip_breakless_divergence(const uint8_t* data, size_t size, const char* ydump);
static int yd_ps_space_divergence(const uint8_t* data, size_t size, const char* ydump,
                                  const char* ldump);

static int yd_root_block_content_divergence(const uint8_t* data, size_t size) {
    if (size == 0) {
        return 0;
    }
    char* sub = malloc(size * 2 + 2);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    size_t i = 0;
    int in_block = 0;
    int changed = 0;
    while (i < size) {
        size_t ls = i;
        /* a lone \r (or the \r of a CRLF pair) ends the line too — the
         * header law must see the '|' clean of the terminator (the
         * nightly's round 19, catch 165: "|\r\n##" read the header as
         * "|\r" and never entered block mode) */
        while (i < size && data[i] != '\n' && data[i] != '\r') {
            i++;
        }
        size_t le = i;
        int has_nl = i < size;
        size_t br = 0;
        if (has_nl) {
            br = (data[i] == '\r' && i + 1 < size && data[i + 1] == '\n') ? 2 : 1;
        }
        int root_marker = 0;
        if (le - ls >= 3 && data[ls] == '-' && data[ls + 1] == '-' && data[ls + 2] == '-') {
            size_t k = ls + 3;
            while (k < le && (data[k] == ' ' || data[k] == '\t')) {
                k++;
            }
            root_marker = (k == le);
        }
        if (le - ls >= 3 && data[ls] == '.' && data[ls + 1] == '.' && data[ls + 2] == '.') {
            size_t k = ls + 3;
            while (k < le && (data[k] == ' ' || data[k] == '\t')) {
                k++;
            }
            root_marker = (k == le);
        }
        if (in_block && root_marker) {
            in_block = 0;
        }
        int header = 0;
        if (!in_block) {
            size_t k = ls;
            if (le - ls >= 4 && data[ls] == '-' && data[ls + 1] == '-' && data[ls + 2] == '-' &&
                (data[ls + 3] == ' ' || data[ls + 3] == '\t')) {
                k = ls + 3;
                while (k < le && (data[k] == ' ' || data[k] == '\t')) {
                    k++;
                }
            }
            if (k < le && (data[k] == '|' || data[k] == '>')) {
                size_t m = k + 1;
                while (m < le &&
                       ((data[m] >= '0' && data[m] <= '9') || data[m] == '+' || data[m] == '-')) {
                    m++;
                }
                while (m < le && (data[m] == ' ' || data[m] == '\t')) {
                    m++;
                }
                /* a trailing COMMENT rides the header (glued or after
                 * blanks -- libyaml's header law, the #650/#662
                 * families); without this the '| #\n#' soup never
                 * entered block mode and the col-0 content line lost
                 * its indent fix (the nightly's round 55,
                 * crash-5b569810, min "| #\n#") */
                header = (m == le || data[m] == '#');
            }
        }
        int indent_line =
            in_block && !root_marker && le > ls && data[ls] != ' ' && data[ls] != '\t';
        if (indent_line) {
            sub[o++] = ' ';
            changed = 1;
        }
        memcpy(sub + o, data + ls, le - ls);
        o += le - ls;
        if (has_nl) {
            memcpy(sub + o, data + i, br); /* the terminator verbatim */
            o += br;
        }
        if (header) {
            in_block = 1;
        }
        if (has_nl) {
            i += br;
        }
    }
    if (!changed) {
        free(sub);
        return 0;
    }
    char* tdump = NULL;
    char* tldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)sub, o, &tdump, &tldump);
    int ok = v == YD_EQUAL ||
             /* the indented copy can still hit the breakless-EOF clip
              * family on its final line ("|\r\n##" indents to
              * "|\r\n ##": yeptris "##\n" vs libyaml "##") — that
              * residual is the documented clip carry, not this family
              * failing its own transform */
             (v == YD_DIFFER && yd_clip_breakless_divergence(sub, o, tdump));
    free(tdump);
    free(tldump);
    free(sub);
    return ok;
}

/* True when the difference is exactly the breakless-EOF clip family:
 * the suite pins clip chomping's trailing break even when the input's
 * last line carries none (L24T#2 "Trailing line of spaces", yaml
 * ending \u2229, expects "x\n \n") and yeptris follows the suite;
 * libyaml drops it ("x\n "). The transform: append '\n' to a copy of
 * the input, re-dump libyaml on THAT, and require exact equality with
 * yeptris's dump of the original -- the divergence is this family only
 * when the appended break is the entire difference. */
static int yd_clip_breakless_divergence(const uint8_t* data, size_t size, const char* ydump) {
    if (ydump == NULL || size == 0 || data[size - 1] == '\n' || data[size - 1] == '\r') {
        return 0; /* only a breakless final line can diverge this way */
    }
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    memcpy(sub, data, size);
    sub[size] = '\n';
    int ok2 = 0;
    char* ldump2 = yd_libyaml_dump(sub, size + 1, &ok2);
    free(sub);
    if (ldump2 == NULL || !ok2) {
        free(ldump2);
        return 0;
    }
    int known = strcmp(ydump, ldump2) == 0;
    free(ldump2);
    return known;
}

/* True when the difference is the LS/PS post-break whitespace
 * family: libyaml's reader treats a raw LS/PS inside a quoted scalar
 * as a break for indentation purposes and strips the space run right
 * after it, while yeptris keeps the bytes (YAML 1.2: LS/PS are not
 * breaks — the same suite-over-libyaml adjudication as the LS/PS
 * carries). Transform: drop the spaces directly after every raw
 * LS/PS; when libyaml's dump of that equals yeptris's original dump,
 * the divergence is this family (the nightly's round 28,
 * crash-2fe6db32: "k: \"a<LS> '<PS>d'b\"" lost the space after the
 * LS). */
static int yd_ps_space_divergence(const uint8_t* data, size_t size, const char* ydump,
                                  const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5) {
        return 0;
    }
    int need = 0;
    for (size_t i = 0; i + 3 < size; i++) {
        if ((unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9) &&
            data[i + 3] == ' ') {
            need = 1;
            break;
        }
    }
    if (!need) {
        return 0;
    }
    /* the relation is exact and dump-shaped: libyaml's dump equals
     * yeptris's with the space runs after every raw LS/PS removed */
    size_t y = 0;
    size_t l = 0;
    while (ydump[y] != '\0') {
        if ((unsigned char)ydump[y] == 0xE2 && (unsigned char)ydump[y + 1] == 0x80 &&
            ((unsigned char)ydump[y + 2] == 0xA8 || (unsigned char)ydump[y + 2] == 0xA9)) {
            if (ldump[l] != ydump[y] || ldump[l + 1] != ydump[y + 1] ||
                ldump[l + 2] != ydump[y + 2]) {
                return 0;
            }
            y += 3;
            l += 3;
            while (ydump[y] == ' ') {
                y++;
            }
            continue;
        }
        if (ldump[l] != ydump[y]) {
            return 0;
        }
        y++;
        l++;
    }
    return ldump[l] == '\0';
}

/* True when the difference is the plain-LS/PS fold law (the nightly's
 * round 56, crash-ecd39c56: "k: Y#<LS> 'c<PT>d'b\"\nj: '<PS>d'"). In
 * PLAIN scalars the two engines break differently: yeptris (YAML 1.2,
 * b-as-line-feed) folds an LS/PS break exactly like a newline — one
 * break to a space, following blanks folded away — while libyaml's
 * READ_LINE joins an LS/PS break RAW into the content (its fold law
 * only folds leading breaks that start with '\n'), so the blanks that
 * followed it are gone. The quoted sibling (both keep the raw code
 * point, yeptris keeps the blanks) is yd_ps_space_divergence; this is
 * the plain sibling, and its relation is exact and dump-shaped:
 * libyaml's dump equals yeptris's with every fold-space that faces a
 * raw LS/PS byte consumed as that triple. */
static int yd_plain_lsps_fold_divergence(const uint8_t* data, size_t size, const char* ydump,
                                         const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int has = 0;
    for (size_t i = 0; i + 2 < size; i++) {
        if ((unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            has = 1;
            break;
        }
    }
    if (!has) {
        return 0;
    }
    int diffs = 0;
    size_t y = 0;
    size_t l = 0;
    while (ydump[y] != '\0') {
        if (ldump[l] == ydump[y]) {
            y++;
            l++;
            continue;
        }
        if (ydump[y] == ' ' && (unsigned char)ldump[l] == 0xE2 &&
            (unsigned char)ldump[l + 1] == 0x80 &&
            ((unsigned char)ldump[l + 2] == 0xA8 || (unsigned char)ldump[l + 2] == 0xA9)) {
            y += 1;
            l += 3;
            diffs++;
            continue;
        }
        return 0;
    }
    return diffs > 0 && ldump[l] == '\0';
}

/* True when the difference is the flow-anchor-colon family: inside a
 * flow collection libyaml reads "&name:" as an anchored empty key
 * ("[&a: x]" -> {a-anchored empty => x}) while yeptris's designed
 * lenience keeps "&" as plain content in flow (anchors ride block and
 * refuse flow; the suite has ZERO flow-&-colon cases, so neither side
 * is suite-backed). The gate is a mini-scan: the "&name:" shape with
 * a terminator, found at bracket depth > 0 outside quotes, and no
 * alias anywhere (aliases would tangle the semantics). */
static int yd_flow_anchor_colon_divergence(const uint8_t* data, size_t size, const char* ydump,
                                           const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int depth = 0;
    int quote = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\') {
                i++; /* the escape rides */
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '#' && depth == 0) {
            break; /* a comment at block level ends the scan */
        }
        if (c == '[' || c == '{') {
            depth++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            continue;
        }
        if (c == '*' || c == '@' || c == '`') {
            return 0; /* aliases/exotics: out of family */
        }
        if (c == '&' && depth > 0) {
            size_t j = i + 1;
            if (j < size && (data[j] == '<' || data[j] == '!' || data[j] == '&')) {
                return 0; /* verbatim/shorthand/props chains: out of family */
            }
            while (j < size && data[j] != ':' && data[j] != ' ' && data[j] != '\t' &&
                   data[j] != ',' && data[j] != ']' && data[j] != '}') {
                j++;
            }
            if (j < size && data[j] == ':' && j > i + 1) {
                /* "&name:" at flow depth: the family. The follower is
                 * irrelevant -- after an anchor libyaml takes ':' as
                 * the key separator even glued ("&tag:examle.com",
                 * the nightly's crash-3483b4cd). */
                return 1;
            }
        }
    }
    return 0;
}

/* libyaml's anchor/alias name charset is [0-9A-Za-z_-] (IS_ALPHA,
 * scanner.c's scan_anchor); the suite keeps every ns-anchor-char
 * INSIDE the name (W5VH "&:@*!$\"<foo>:", 2SXE "&a: key", the
 * nightly's round-10 artifact "&----?x-"), and yeptris follows. When
 * a name leaves libyaml's set, it truncates there and re-lexes the
 * remainder as content ("k: &ab?cd v" anchors 'ab' and reads "?cd v"
 * as the value) — libyaml is behind the suite. Carry: truncate every
 * quote-free '&' name run at its first out-of-set byte and require
 * libyaml's dump of the truncation to equal yeptris's dump of the
 * original — the divergence is then exactly the charset law. */
static int yd_anchor_byte_in_libyaml_set(unsigned char n) {
    return (n >= '0' && n <= '9') || (n >= 'A' && n <= 'Z') || (n >= 'a' && n <= 'z') || n == '_' ||
           n == '-';
}

static int yd_anchor_name_charset_divergence(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    /* gate: some quote-free '&' name run leaves libyaml's set */
    int gate = 0;
    size_t amps = 0; /* each '&' run's split adds one byte to the copy */
    int quote = 0;
    char prev = '\n'; /* '#' is a comment only after a blank */
    /* a '&' at NODE position (line start, or after : [ { , with only
     * blanks between) can be props; elsewhere it is mid-plain CONTENT
     * and the split would corrupt the scalar's bytes (the nightly's
     * round 31, crash-f1215da4: "-- !!map---! &----?x- -" — the '&'
     * follows a blank inside the plain scalar, the blind split turned
     * it into "&---- ?x-" and the carry declined its own family) */
    int node_pos = 1;
    int only_dash = 1; /* the line so far is blanks and '-' (seq entry) */
    /* quotes open only at token starts (the catch-85 word-start law):
     * a mid-word quote is plain CONTENT ("v'"), and the naive tracker
     * swallowed the rest of the input — the '&' after it never gated
     * (the nightly's round 23, crash-c1dc27cf). A quote glued to a
     * props run ("&a\"x") still opens: props end at indicators. */
    int word_led = 1;  /* the next quote-ish byte starts a token */
    int prop_word = 0; /* inside/at the end of a & or ! run */
    int flowd = 0;     /* flow depth: block-plain content absorbs the
                        * flow indicators ("/]" is ONE plain -- the
                        * eager arming opened a phantom quote that
                        * swallowed the rest of the input,
                        * crash-283aaadd) */
    int saw_prop = 0;  /* a & or ! run: blanks after it keep the NEXT
                        * prop at node position ("!tag &anch" -- the
                        * tag-led props run left node_pos 0 and the
                        * gate skipped the anchor entirely, the
                        * nightly's round 40, crash-fe3b5522) */
    /* the block plain-continuation law (issue #718): once a plain
     * opens, continuation lines MORE indented than the plain's line
     * are CONTENT in both engines ("&o:q r\n&g&" -- the '&'-led
     * second line rides the plain; the blind line-start reset split
     * it and the carry declined its own family). A line at/below the
     * plain's column ends it (a root-level plain, column 0, swallows
     * every line -- "a\n- b" is ONE scalar, libyaml's indent -1). */
    int plain_open = 0;
    int coll_col = -1; /* the enclosing construct's column (the last
                        * seq dash / explicit-key '?' / mapping key at
                        * its line start; -1 = document root where
                        * libyaml's indent is -1 and every line rides) */
    size_t col = 0;
    size_t line_first = (size_t)-1;
    int line_pending = 0;
    int in_props_name = 0;
    int bs_open = 0; /* a |/> block-scalar body: every byte is text */
    size_t bs_col = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        /* column bookkeeping (approximate: 1 byte = 1 column) */
        if (c == '\n' || c == '\r') {
            col = 0;
            line_first = (size_t)-1;
            line_pending = (plain_open || bs_open) ? 1 : 0;
        } else {
            if (line_first == (size_t)-1 && c != ' ' && c != '\t') {
                line_first = col;
                if (line_pending) {
                    line_pending = 0;
                    if ((int)col > coll_col) {
                        node_pos = 0; /* the plain rides on */
                    } else {
                        plain_open = 0; /* the plain's line ended */
                        node_pos = 1;
                    }
                }
                if (bs_open) {
                    if (col <= bs_col) {
                        bs_open = 0;
                        node_pos = 1;
                    } else {
                        node_pos = 0;
                    }
                }
            }
            col++;
        }
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\') {
                i++;
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            word_led = 0;
            prop_word = 0;
            prev = (char)c;
            continue;
        }
        if ((c == '\'' || c == '"') && (word_led || prop_word)) {
            quote = c;
            word_led = 0;
            prop_word = 0;
            plain_open = 0;
            prev = (char)c;
            continue;
        }
        if ((c == '&' || c == '!' || c == '*') && !plain_open) {
            /* '*' is a props run like '&': the round-54 alias-name
             * composition splits '*a:' at the ':' exactly like the
             * anchor twin; leaving it to the word arm opened a phantom
             * plain and the split never fired */
            prop_word = 1;
            word_led = 0;
            saw_prop = 1;
            in_props_name = 1;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-' || c == '?') {
            /* blanks keep saw_prop: the props run continues */
            /* ':' does NOT arm a word start: a quote GLUED to the
             * separator ("- :\"" -> the plain ':\"') is plain-first
             * CONTENT per the suite (HM87), and the eager open
             * swallowed the rest of the input so the '&g:' anchor on a
             * later line never gated (crash-7fe697be). A quote after a
             * SEPARATED colon re-arms through the blank. */
            word_led = 1;
            prop_word = 0;
            if ((c == '-' || c == '?') && i + 1 < size && data[i + 1] != ' ' &&
                data[i + 1] != '\t' && data[i + 1] != '\n' && data[i + 1] != '\r' && !plain_open &&
                (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
                /* "-x"/"?x" at a node start: the indicator is PLAIN
                 * CONTENT (only "- "/"? " are constructs) -- the line's
                 * plain opens HERE, so a later blank-followed '&' on
                 * the line stays content (crash-3f2e419b: the
                 * "-- !!map---!, &----?x- -" key line). node_pos drops
                 * with it: the law left it at the line-start 1 and a
                 * later '[' re-armed flow mid-plain -- the '&'-after-
                 * comma then split a PLAIN's bytes and the dumps
                 * diverged by one space (the nightly's round 57,
                 * crash-bc242984, "-- [!!, &----?x- -") */
                plain_open = 1;
                node_pos = 0;
            }
        } else if (c == '[' || c == '{') {
            /* a flow opener arms only at NODE position; mid-plain it
             * is plain content and the '[/{' lands in the scalar */
            if (node_pos) {
                flowd++;
                word_led = 1;
                plain_open = 0;
            } else {
                word_led = 0;
            }
            prop_word = 0;
        } else if (c == ']' || c == '}') {
            /* the closer arms only inside OPEN flow; in block it is
             * mid-plain content (crash-283aaadd: "/]\"") */
            if (flowd > 0) {
                flowd--;
                word_led = 1;
            } else {
                word_led = 0;
            }
            prop_word = 0;
        } else if (c == ',') {
            word_led = (flowd > 0);
            prop_word = 0;
        } else {
            word_led = 0;
            prop_word = 0;
            saw_prop = 0;
            if (flowd == 0 && !bs_open) {
                if ((c == '|' || c == '>') && node_pos) {
                    bs_open = 1;
                    bs_col = line_first;
                    plain_open = 0;
                } else if (!plain_open && !in_props_name &&
                           (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
                    plain_open = 1;
                }
            }
            in_props_name = 0;
        }
        if (c == '#' && (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
            /* the comment runs to the LINE BREAK: a '&' inside it is
             * prose, but an anchor on a later line still gates (the
             * nightly's round 17, crash-39eb5a17: "&4:" after a
             * '# comment' line was never scanned and the family's
             * carry declined the shape) */
            while (i + 1 < size && data[i + 1] != '\n' && data[i + 1] != '\r') {
                i++;
            }
            prev = '\n';
            plain_open = 0;
            continue;
        }
        if ((c == '&' || c == '*') && !plain_open && (node_pos || saw_prop)) {
            /* '*' joins the family (the nightly's round 54,
             * crash-17654f5b: the ALIAS name rides libyaml's charset
             * law exactly like an anchor's -- "*a:" names "a:" in
             * yeptris (2SXE) while libyaml truncates at ':' and reads
             * the key separator; the split reconciles both) */
            amps++;
        }
        if ((c == '&' || c == '*') && !gate && !plain_open && (node_pos || saw_prop)) {
            size_t j = i + 1;
            while (j < size && data[j] != ' ' && data[j] != '\t' && data[j] != '\n' &&
                   data[j] != '\r') {
                if (!yd_anchor_byte_in_libyaml_set(data[j])) {
                    gate = 1;
                    break;
                }
                j++;
            }
        }
        if (c == '\n' || c == '\r') {
            node_pos = (plain_open || bs_open) ? 0 : 1;
            only_dash = 1;
        } else if (c != ' ' && c != '\t') {
            /* ':' = the pair separator; the flow indicators are NOT
             * node positions in block context (they are plain content
             * there — the nightly's round 31, crash-3f2e419b:
             * "-- !!map---!, &----?x- -" — the ',' marked a node
             * position that is mid-plain, the split corrupted the
             * scalar and the carry declined its own family) */
            /* the plain law wins: a '-'/'?'-led CONTENT line (the -x
             * law just opened the plain) is not node position -- the
             * dash clause re-raised node_pos AFTER the -x arm cleared
             * it and a mid-plain '[' re-armed flow (the nightly's
             * round 57, crash-bc242984) */
            node_pos = !plain_open && (c == ':' || c == '?' || (c == '-' && only_dash));
            if (c == '-' && only_dash && i + 2 < size && data[i + 1] == '-' && data[i + 2] == '-' &&
                (i + 3 >= size || data[i + 3] == ' ' || data[i + 3] == '\t' ||
                 data[i + 3] == '\n' || data[i + 3] == '\r')) {
                coll_col = -1; /* the document marker: root context */
                plain_open = 0;
            } else if (c == '-' && only_dash &&
                       (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                        data[i + 1] == '\n' || data[i + 1] == '\r')) {
                coll_col = (int)line_first;
            } else if (c == '?' && col == line_first &&
                       (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                        data[i + 1] == '\n' || data[i + 1] == '\r')) {
                coll_col = (int)line_first;
            }
            if (c == ':' && (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                             data[i + 1] == '\n' || data[i + 1] == '\r')) {
                plain_open = 0;             /* a key colon ends the riding plain */
                coll_col = (int)line_first; /* the mapping's indent is the key's column */
            } else if (c == ':' && flowd == 0 && !bs_open && i + 1 < size && data[i + 1] != ' ' &&
                       data[i + 1] != '\t' && data[i + 1] != '\n' && data[i + 1] != '\r') {
                /* a colon-led plain opens here (the HM87 riding colon:
                 * "&k :`c ..." -- libyaml truncates the anchor at ':'
                 * and the residual ':`c {...' IS the plain). Without
                 * this the walk kept plain_open closed and a later
                 * mid-plain '&' rode as props -- the split corrupted
                 * the scalar's bytes and the carry declined its own
                 * family (the nightly's round 51, crash-098e7549). */
                plain_open = 1;
            }
            if (c != '-') {
                only_dash = 0;
            }
        }
        prev = (char)c;
    }
    if (!gate) {
        return 0;
    }
    /* transform: split every quote-free '&' name run at its first
     * out-of-set byte ("&ab?cd" -> "&ab ?cd") — the reading yeptris
     * then produces IS libyaml's reading of the original; each run's
     * split adds one byte (amps counted in the gate walk) */
    char* sub = malloc(size + amps + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    quote = 0;
    prev = '\n';
    /* the gate's word-start law again: a mid-word quote is plain
     * content, or the copy rides "inside quotes" past the '&' run */
    int word_led2 = 1;
    int prop_word2 = 0;
    int node_pos2 = 1;
    int only_dash2 = 1;
    int flowd2 = 0;    /* the gate's flow-depth law again */
    int saw_prop2 = 0; /* the gate's props-continuation law again */
    /* the gate's plain-continuation law again (issue #718) */
    int plain_open2 = 0;
    int coll_col2 = -1; /* the gate's enclosing-construct column again */
    size_t col2 = 0;
    size_t line_first2 = (size_t)-1;
    int line_pending2 = 0;
    int in_props_name2 = 0;
    int bs_open2 = 0;
    size_t bs_col2 = 0;
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        if (c == '\n' || c == '\r') {
            col2 = 0;
            line_first2 = (size_t)-1;
            line_pending2 = (plain_open2 || bs_open2) ? 1 : 0;
        } else {
            if (line_first2 == (size_t)-1 && c != ' ' && c != '\t') {
                line_first2 = col2;
                if (line_pending2) {
                    line_pending2 = 0;
                    if ((int)col2 > coll_col2) {
                        node_pos2 = 0;
                    } else {
                        plain_open2 = 0;
                        node_pos2 = 1;
                    }
                }
                if (bs_open2) {
                    if (col2 <= bs_col2) {
                        bs_open2 = 0;
                        node_pos2 = 1;
                    } else {
                        node_pos2 = 0;
                    }
                }
            }
            col2++;
        }
        if (quote != 0) {
            sub[o++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[o++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            word_led2 = 0;
            prop_word2 = 0;
            i++;
            continue;
        }
        if ((c == '\'' || c == '"') && (word_led2 || prop_word2)) {
            quote = c;
            sub[o++] = (char)c;
            prev = (char)c;
            word_led2 = 0;
            prop_word2 = 0;
            plain_open2 = 0;
            i++;
            continue;
        }
        if ((c == '&' || c == '!' || c == '*') && !plain_open2) {
            prop_word2 = 1;
            word_led2 = 0;
            saw_prop2 = 1;
            in_props_name2 = 1; /* the gate's '*' props-run law again */
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-' || c == '?') {
            /* the gate's ':' law again (crash-7fe697be) */
            word_led2 = 1;
            prop_word2 = 0;
            if ((c == '-' || c == '?') && i + 1 < size && data[i + 1] != ' ' &&
                data[i + 1] != '\t' && data[i + 1] != '\n' && data[i + 1] != '\r' && !plain_open2 &&
                (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
                /* the gate's "-x"/"?x" content law again (crash-3f2e419b;
                 * node_pos drops with it -- the round-57 finding) */
                plain_open2 = 1;
                node_pos2 = 0;
            }
        } else if (c == '[' || c == '{') {
            if (node_pos2) {
                flowd2++;
                word_led2 = 1;
                plain_open2 = 0;
            } else {
                word_led2 = 0;
            }
            prop_word2 = 0;
        } else if (c == ']' || c == '}') {
            if (flowd2 > 0) {
                flowd2--;
                word_led2 = 1;
            } else {
                word_led2 = 0;
            }
            prop_word2 = 0;
        } else if (c == ',') {
            word_led2 = (flowd2 > 0);
            prop_word2 = 0;
        } else {
            word_led2 = 0;
            prop_word2 = 0;
            saw_prop2 = 0;
            if (flowd2 == 0 && !bs_open2) {
                if ((c == '|' || c == '>') && node_pos2) {
                    bs_open2 = 1;
                    bs_col2 = line_first2;
                    plain_open2 = 0;
                } else if (!plain_open2 && !in_props_name2 &&
                           (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
                    plain_open2 = 1;
                }
            }
            in_props_name2 = 0;
        }
        if (c == '#' && (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
            /* verbatim through the comment's line break, then keep
             * scanning: anchors on later lines still split (the gate
             * walks the same set) */
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[o++] = (char)data[i++];
            }
            prev = '\n';
            plain_open2 = 0;
            continue;
        }
        if ((c == '&' || c == '*') && !plain_open2 && (node_pos2 || saw_prop2)) {
            /* the '*' twin of the '&' split (the round-54 alias-name
             * composition -- see the gate walk) */
            sub[o++] = (char)c;
            i++;
            size_t name_o = o;
            while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                   data[i] != '\r' && yd_anchor_byte_in_libyaml_set(data[i])) {
                sub[o++] = (char)data[i++];
            }
            /* split ONLY a real anchor (at least one in-set name byte
             * before the out-of-set one): an '&' with an EMPTY name is
             * mid-plain CONTENT here ("--&\t0\- -:" -- both engines
             * read the line as one plain key), and the blind split
             * inserted a space that corrupted the scalar
             * (crash-e1d13ebe, the corpus sweep after #713's arms) */
            if (o > name_o && i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                data[i] != '\r') {
                sub[o++] = ' '; /* libyaml's truncation point becomes a token break */
                /* the residual token is copied blind, so the walk must
                 * take its first byte's own law here: a colon-riding
                 * plain ("&k :`c ...") OPENS the plain -- leaving it
                 * closed made the NEXT mid-plain '&' ride as props,
                 * the split corrupted the scalar's bytes and the carry
                 * declined its own family (the nightly's round 51,
                 * crash-098e7549). A ':' with a blank follower is the
                 * key separator ("&kk: |"), not a plain. */
                unsigned char f = data[i];
                int plain_residual =
                    flowd2 == 0 && !bs_open2 &&
                    !(f == ':' && (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                                   data[i + 1] == '\n' || data[i + 1] == '\r'));
                while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                       data[i] != '\r') {
                    sub[o++] = (char)data[i++];
                }
                if (plain_residual) {
                    plain_open2 = 1;
                }
            }
            node_pos2 = 0;
            prev = ' ';
            in_props_name2 = 0; /* the props run ended at the split */
            continue;
        }
        if (c == '\n' || c == '\r') {
            node_pos2 = (plain_open2 || bs_open2) ? 0 : 1;
            only_dash2 = 1;
        } else if (c != ' ' && c != '\t') {
            node_pos2 = !plain_open2 && (c == ':' || c == '?' || (c == '-' && only_dash2));
            if (c == '-' && only_dash2 && i + 2 < size && data[i + 1] == '-' &&
                data[i + 2] == '-' &&
                (i + 3 >= size || data[i + 3] == ' ' || data[i + 3] == '\t' ||
                 data[i + 3] == '\n' || data[i + 3] == '\r')) {
                coll_col2 = -1;
                plain_open2 = 0;
            } else if (c == '-' && only_dash2 &&
                       (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                        data[i + 1] == '\n' || data[i + 1] == '\r')) {
                coll_col2 = (int)line_first2;
            } else if (c == '?' && col2 == line_first2 &&
                       (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                        data[i + 1] == '\n' || data[i + 1] == '\r')) {
                coll_col2 = (int)line_first2;
            }
            if (c == ':' && (i + 1 >= size || data[i + 1] == ' ' || data[i + 1] == '\t' ||
                             data[i + 1] == '\n' || data[i + 1] == '\r')) {
                plain_open2 = 0;
                coll_col2 = (int)line_first2;
            } else if (c == ':' && flowd2 == 0 && !bs_open2 && i + 1 < size && data[i + 1] != ' ' &&
                       data[i + 1] != '\t' && data[i + 1] != '\n' && data[i + 1] != '\r') {
                plain_open2 = 1; /* the gate's colon-led plain law again */
            }
            if (c != '-') {
                only_dash2 = 0;
            }
        }
        sub[o++] = (char)c;
        prev = (char)c;
        i++;
    }
    int ok2 = 0;
    /* the split can compose with the breakless-EOF clip (the nightly's
     * round 20, catch 166: "&kk: |" + content at EOF — yeptris keeps
     * BOTH the suite's ':' anchor name and the clip break, so each
     * single carry's transform leaves the other difference on the
     * table): a second copy with a break appended must parse with NO
     * divergence at all */
    char* sub2 = NULL;
    size_t o2 = o;
    if (o > 0 && sub[o - 1] != '\n' && sub[o - 1] != '\r') {
        sub2 = malloc(o + 1);
        if (sub2 != NULL) {
            memcpy(sub2, sub, o);
            sub2[o] = '\n';
            o2 = o + 1;
        }
    }
    char* ydump2 = yd_sem_dump(sub, o, &ok2);
    if (ydump2 != NULL && ok2) {
        /* the PROPS-LED composite (the nightly's round 40,
         * "!str-f &U:ba:\n- ..."): yeptris's original reading puts
         * the whole riding name on the FOLLOWING collection (no pair
         * at all), while libyaml truncates the name at the ':' and
         * builds the pair -- so neither "libyaml(split) ==
         * yeptris(original)" nor the qcolon walk can reconcile. But
         * both engines agree on the SPLIT form, and libyaml is
         * INVARIANT under it (its own reading truncates at the ':'
         * anyway) -- exactly the charset-tokenization signature. */
        int ok3 = 0;
        char* ldump2 = yd_libyaml_dump(sub, o, &ok3);
        if (ldump2 != NULL && ok3 && strcmp(ldump, ldump2) == 0 && strcmp(ydump2, ldump2) == 0) {
            free(ldump2);
            free(ydump2);
            free(sub);
            free(sub2);
            return 1;
        }
        free(ldump2);
    }
    free(sub);
    if (ydump2 == NULL || !ok2) {
        free(ydump2);
        free(sub2);
        return 0;
    }
    /* the split can also expose the qflow-colon reading ("? &a:b&"
     * splits to "? &a :b&"; catch 160's shape) -- the key-side twin
     * below reconciles it */
    int known = strcmp(ldump, ydump2) == 0 || yd_qcolon_walk(ldump, ydump2);
    free(ydump2);
    if (!known && sub2 != NULL) {
        char* td = NULL;
        char* tl = NULL;
        known = yd_diff_classify((const char*)sub2, o2, &td, &tl) == YD_EQUAL;
        free(td);
        free(tl);
    }
    free(sub2);
    return known;
}

/* True when the difference is the glued-indicator-comment family:
 * a line starting with a block-scalar indicator IMMEDIATELY followed
 * by '#' (">#' > ':\tg" — the nightly's run 39258457271,
 * crash-d4bb362b), including chomping/indent indicators before the
 * '#' ("|+#~ c: d" — run 39478141022, crash-89bff209). libyaml reads
 * header + comment right after the indicator (the spec's
 * s-separate-in-line may be empty) and yields an empty scalar;
 * yeptris's comment law requires a preceding blank (e_comment_at), so
 * the line is plain CONTENT — a documented lenient divergence (the
 * suite has no glued-indicator test). The gate: the pattern exists,
 * and spacing it out ("> #", "|+ #") leaves libyaml's dump
 * byte-identical (its reading was never sensitive to the space) while
 * yeptris's dump differs — the divergence is then exactly the
 * indicator-comment law. */
/* The position of a '#' glued to a block-scalar header indicator,
 * possibly after chomping/indent indicators ("|#", "|+#~"); 0 = none.
 * The comment law's emptiness is what differs between the engines. */
static size_t yd_header_glue(const uint8_t* d, size_t t, size_t end) {
    if (t >= end || (d[t] != '>' && d[t] != '|')) {
        return 0;
    }
    size_t k = t + 1;
    while (k < end && ((d[k] >= '0' && d[k] <= '9') || d[k] == '+' || d[k] == '-')) {
        k++;
    }
    return (k < end && d[k] == '#') ? k : 0;
}

static int yd_header_glued_comment_divergence(const uint8_t* data, size_t size, const char* ydump,
                                              const char* ldump) {
    if (ydump == NULL || ldump == NULL || strcmp(ydump, ldump) == 0) {
        return 0;
    }
    size_t at = 0;
    int found = 0;
    while (at < size) {
        size_t line_end = at;
        while (line_end < size && data[line_end] != '\n' && data[line_end] != '\r') {
            line_end++;
        }
        size_t t = at;
        while (t < line_end && (data[t] == ' ' || data[t] == '\t')) {
            t++;
        }
        if (yd_header_glue(data, t, line_end) != 0) {
            found = 1;
            break;
        }
        at = line_end + 1;
    }
    if (!found) {
        return 0;
    }
    /* one extra byte per spaced line, up to one per input line */
    char* sub = malloc(size * 2 + 1);
    if (sub == NULL) {
        return 0;
    }
    /* space the glued comment out on EVERY matching line */
    size_t o = 0;
    at = 0;
    while (at < size) {
        size_t line_end = at;
        while (line_end < size && data[line_end] != '\n' && data[line_end] != '\r') {
            line_end++;
        }
        size_t t = at;
        while (t < line_end && (data[t] == ' ' || data[t] == '\t')) {
            t++;
        }
        size_t glue = yd_header_glue(data, t, line_end);
        for (size_t i = at; i < line_end; i++) {
            if (glue != 0 && i == glue) {
                sub[o++] = ' ';
            }
            sub[o++] = (char)data[i];
        }
        if (line_end < size) {
            sub[o++] = (char)data[line_end];
        }
        at = line_end + 1;
    }
    int ok2 = 0;
    char* ldump2 = yd_libyaml_dump(sub, o, &ok2);
    free(sub);
    int known = (ldump2 != NULL && ok2 && strcmp(ldump, ldump2) == 0);
    free(ldump2);
    return known;
}

/* The flow-'?' adjacency family with a QUOTED follower: libyaml's
 * scanner makes a KEY token for '?' in flow even without a following
 * space ("{?\"a\":x}" -> key a), while the 1.2 suite pins the plain
 * reading (HM87/652Z cover the plain followers; the same
 * ns-plain-first rule admits a quoted follower, since '"' is
 * ns-plain-safe in flow). The plain-follower shape rides
 * yd_qflow_divergence's line rewrite; this composition changes the
 * dump shape too much for a line rewrite, so the family is proven at
 * the INPUT level: replace each '?' directly before a quote with a
 * space and require libyaml's dump to be UNCHANGED. That establishes
 * the '?' was merely libyaml's key indicator; yeptris's different
 * dump is the suite's plain reading. (The substitution preserves byte
 * count, so any position fields stay aligned.) */
static int yd_qflow_adjacent_divergence(const uint8_t* data, size_t size, const char* ydump,
                                        const char* ldump) {
    if (ldump == NULL || ydump == NULL || size == 0 || size > 8192) {
        return 0;
    }
    /* each hit emits '?'+' ' while consuming two input bytes, so the
     * copy can reach size + size/2 ("|\r\r#?????'#??'#" two hits
     * overflowed the old size+1 buffer — ASan write, local corpus) */
    char* sub = malloc(size * 2 + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    int found = 0;
    /* only a '?' at a flow-NODE position is the explicit-key
     * indicator: a '?"' pair mid-token is the VALUE's plain content
     * (the nightly's round 64, crash-45b843db's sibling:
     * "{?\"a\":/*com?\"a\":/*comment*!!\"}" — spacing the value's
     * '?"' changed libyaml's bytes and the insensitivity check
     * declined its own family) */
    int depth = 0;
    int node_pos = 1;
    for (size_t i = 0; i < size; i++) {
        if (node_pos && data[i] == '?' && i + 1 < size &&
            (data[i + 1] == '"' || data[i + 1] == '\'')) {
            /* KEEP the '?' and space it out ('{?"a":x}' -> '{? "a":x}'):
             * libyaml reads the flow '?' as the explicit-key indicator
             * either way, so its dump is unchanged -- the old DROP form
             * stayed neutral only while the pair's value was non-empty;
             * with an EMPTY value the drop turned the explicit key into
             * a bare member and libyaml errored ("did not find expected
             * node content") -- the nightly's round 53, crash-52c8d1ef,
             * '{?"\n":}'. */
            sub[o++] = '?';
            sub[o++] = ' ';
            found = 1;
            node_pos = 0;
        } else {
            sub[o++] = (char)data[i];
            if (data[i] == '{' || data[i] == '[') {
                depth++;
                node_pos = 1;
            } else if (data[i] == '}' || data[i] == ']') {
                depth--;
                node_pos = 0;
            } else if (data[i] == ',') {
                node_pos = depth > 0;
            } else if (data[i] != ' ' && data[i] != '\t') {
                node_pos = 0;
            }
        }
    }
    int known = 0;
    if (found) {
        int ok2 = 0;
        char* ldump2 = yd_libyaml_dump(sub, o, &ok2);
        known = (ldump2 != NULL && ok2 && strcmp(ldump, ldump2) == 0);
        free(ldump2);
    }
    free(sub);
    if (known) {
        return 1;
    }
    /* the SEQ-entry twin (crash-ce698341, "[?\"\\l\"]"): spacing the '?'
     * BREAKS libyaml in a flow sequence (its pair collapses to a bare
     * scalar), so the insensitivity check above correctly declines.
     * Wrap the whole plain span in SINGLE quotes instead: single-
     * quoted escaping keeps backslashes raw -- exactly yeptris's plain
     * reading -- and both engines agree on a quoted token. Verify
     * libyaml(wrap) == yeptris(original). */
    char* wrap = malloc(size * 2 + 4);
    if (wrap == NULL) {
        return 0;
    }
    size_t wo = 0;
    int wrapped = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '?' && i + 1 < size && data[i + 1] == '"') {
            /* find the closing unescaped quote */
            size_t j = i + 2;
            int esc = 0;
            int quote_in_span = 0;
            while (j < size && (esc || data[j] != '"')) {
                if ((char)data[j] == '\'') {
                    quote_in_span = 1;
                }
                esc = (!esc && data[j] == '\\');
                j++;
            }
            if (j < size && !quote_in_span) {
                wrap[wo++] = '\'';
                for (size_t k = i; k <= j; k++) {
                    wrap[wo++] = (char)data[k];
                }
                wrap[wo++] = '\'';
                i = j;
                wrapped = 1;
                continue;
            }
        }
        wrap[wo++] = (char)data[i];
    }
    if (wrapped) {
        int ok3 = 0;
        char* ldump3 = yd_libyaml_dump(wrap, wo, &ok3);
        known = (ldump3 != NULL && ok3 && strcmp(ydump, ldump3) == 0);
        free(ldump3);
    }
    free(wrap);
    return known;
}

/* True when the difference is exactly the bare non-specific tag: a
 * standalone "!" token ("! a", "- ! tag:x:" — the nightly's round 22,
 * crash-d6be3347). The suite keeps the tag on the event as "<!>"
 * (8MK2/52DL/S4JQ pin "=VAL <!> :a"; the tag is exempt from %TAG per
 * 7.3.3, which the resolve exemption now implements) while libyaml
 * DROPS it on plain scalars. The transform removes the "!" token from
 * the input and requires libyaml's dump of that to equal yeptris's
 * dump with the "<!> " markers stripped. */
static int yd_nonspecific_tag_divergence(const uint8_t* data, size_t size, const char* ydump) {
    if (ydump == NULL || size == 0) {
        return 0;
    }
    /* gate: a quote-free standalone "!" token (space/tab-delimited) */
    int gate = 0;
    int quote = 0;
    char prev = '\n';
    size_t bang = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\') {
                i++;
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            prev = (char)c;
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            prev = (char)c;
            continue;
        }
        if (c == '!' && (prev == '\n' || prev == '\r' || prev == ' ' || prev == '\t') &&
            i + 1 < size &&
            (data[i + 1] == ' ' || data[i + 1] == '\t' || data[i + 1] == '\n' ||
             data[i + 1] == '\r')) {
            gate = 1;
            bang = i;
            break;
        }
        prev = (char)c;
    }
    if (!gate) {
        return 0;
    }
    /* transform: drop the "!" and one following separator */
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < size;) {
        if (i == bang) {
            i++; /* the '!' */
            if (i < size && (data[i] == ' ' || data[i] == '\t')) {
                i++; /* one separator */
            }
            continue;
        }
        sub[o++] = (char)data[i++];
    }
    int ok2 = 0;
    char* ldump2 = yd_libyaml_dump(sub, o, &ok2);
    free(sub);
    if (ldump2 == NULL || !ok2) {
        free(ldump2);
        return 0;
    }
    /* yeptris's dump with the non-specific "<!> " markers stripped */
    const char* mark = "<!> ";
    size_t mlen = 4;
    char* strip = malloc(strlen(ydump) + 1);
    if (strip == NULL) {
        free(ldump2);
        return 0;
    }
    size_t so = 0;
    for (const char* q = ydump; *q;) {
        if (strncmp(q, mark, mlen) == 0) {
            q += mlen;
            continue;
        }
        strip[so++] = *q++;
    }
    strip[so] = '\0';
    int known = strcmp(strip, ldump2) == 0;
    free(strip);
    free(ldump2);
    return known;
}

static void probe_one(const uint8_t* data, size_t size) {
    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)data, size, &ydump, &ldump);
    int split = v == YD_YEPTRIS_ONLY || v == YD_LIBYAML_ONLY;
    /* a documented divergence, not a bug: the suite sides with
     * yeptris (see yd_qflow_divergence) */
    int known = v == YD_DIFFER && (yd_qflow_divergence(ydump, ldump) ||
                                   yd_qflow_adjacent_divergence(data, size, ydump, ldump) ||
                                   yd_qflow_colon_divergence(data, size, ydump, ldump) ||
                                   yd_qflow_question_colon_divergence(data, size, ydump, ldump) ||
                                   yd_bom_content_divergence(ydump, ldump) ||
                                   yd_bom_line_start_divergence(data, size, ydump, ldump) ||
                                   yd_breaknorm_divergence(data, size, ydump) ||
                                   yd_nel_break_divergence(data, size, ydump, ldump) ||
                                   yd_composed_break_divergence(data, size) ||
                                   yd_root_block_content_divergence(data, size) ||
                                   yd_clip_breakless_divergence(data, size, ydump) ||
                                   yd_ps_space_divergence(data, size, ydump, ldump) ||
                                   yd_plain_lsps_fold_divergence(data, size, ydump, ldump) ||
                                   yd_flow_anchor_colon_divergence(data, size, ydump, ldump) ||
                                   yd_anchor_name_charset_divergence(data, size, ydump, ldump) ||
                                   yd_anchor_glued_follower_divergence(data, size, ydump, ldump) ||
                                   yd_header_glued_comment_divergence(data, size, ydump, ldump) ||
                                   yd_nonspecific_tag_divergence(data, size, ydump));
#ifdef YEP_FUZZ_STANDALONE
    /* corpus walker: splits are enforced against the ledger (an
     * unledgered split is a conformance regression) */
    if ((v == YD_DIFFER || split) && !known && !(probe_id != NULL && yd_ledger_has(probe_id))) {
        fprintf(stderr, "fuzz_diff: %s%s%s on input:\n--- yeptris:\n%s\n--- libyaml:\n%s\n",
                yd_verdict_name(v), probe_id ? " (" : "", probe_id ? probe_id : "",
                ydump ? ydump : "(error)", ldump ? ldump : "(error)");
        abort();
    }
#else
    /* libFuzzer entry (mutated bytes, no corpus id): only a true
     * semantic divergence aborts — BOTH accepted but the event
     * streams differ. Random-byte splits live in the suite-gray
     * zone the ledger documents by input, not by mutation. */
    (void)split;
    (void)probe_id;
    if (v == YD_DIFFER && !known) {
        fprintf(stderr, "fuzz_diff: %s on mutated input:\n--- yeptris:\n%s\n--- libyaml:\n%s\n",
                yd_verdict_name(v), ydump ? ydump : "(error)", ldump ? ldump : "(error)");
        abort();
    }
#endif
    free(ydump);
    free(ldump);
}

#ifdef YEP_FUZZ_STANDALONE

#define _POSIX_C_SOURCE 200809L
#include <dirent.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        char buf[1 << 16];
        size_t n = fread(buf, 1, sizeof(buf), stdin);
        probe_one((const uint8_t*)buf, n);
        return 0;
    }
    /* argv[1] may be the ledger, then corpus dirs/files */
    int first = 1;
    if (strncmp(argv[1], "--ledger=", 9) == 0) {
        if (yd_ledger_load(argv[1] + 9) < 0) {
            fprintf(stderr, "fuzz_diff: no ledger %s\n", argv[1] + 9);
            return 2;
        }
        first = 2;
        if (argc < 3) {
            return 0;
        }
    }
    static char id_buf[256];
    long files = 0;
    for (int a = first; a < argc; a++) {
        DIR* d = opendir(argv[a]);
        if (d != NULL) {
            struct dirent* ent;
            while ((ent = readdir(d)) != NULL) {
                size_t nl = strlen(ent->d_name);
                if (nl < 4 || strcmp(ent->d_name + nl - 3, ".in") != 0) {
                    continue; /* suite inputs only (.ly goldens aren't cases) */
                }
                char path[2048];
                snprintf(path, sizeof(path), "%s/%s", argv[a], ent->d_name);
                FILE* f = fopen(path, "rb");
                if (f == NULL) {
                    continue;
                }
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                snprintf(id_buf, sizeof(id_buf), "%.*s", (int)(nl - 3), ent->d_name);
                probe_id = id_buf;
                probe_one(buf, n);
                probe_id = NULL;
                files++;
            }
            closedir(d);
        } else {
            FILE* f = fopen(argv[a], "rb");
            if (f != NULL) {
                static uint8_t buf[1 << 22];
                size_t n = fread(buf, 1, sizeof(buf), f);
                fclose(f);
                const char* base = strrchr(argv[a], '/');
                base = base ? base + 1 : argv[a];
                size_t bl = strlen(base);
                snprintf(id_buf, sizeof(id_buf), "%.*s",
                         (int)(bl > 3 && strcmp(base + bl - 3, ".in") == 0 ? bl - 3 : bl), base);
                probe_id = id_buf;
                probe_one(buf, n);
                probe_id = NULL;
                files++;
            }
        }
    }
    printf("fuzz_diff: %ld inputs, no divergences\n", files);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    probe_one(data, size);
    return 0;
}

#endif
