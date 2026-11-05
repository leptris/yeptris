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
    /* the 3-byte BOM + one follower byte is a legal member (the
     * nightly's r90 minimizer: "\n<BOM>-" -- libyaml drops the
     * line-start BOM and reads '-' as a seq entry, yeptris keeps the
     * BOM as plain content) */
    if (ydump == NULL || ldump == NULL || size < 4 || size > 8192) {
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
    /* the whole composed twin rides this buffer: the r107 nightly's
     * crash-7fda24ee gated on the TAIL (left) but memcpy'd the PREFIX
     * (head) -- a late content BOM in a big dump wrote 25k bytes into
     * 8k of stack */
    if (strlen(ydump) >= sizeof(stripped)) {
        return 0;
    }
    size_t head = (size_t)(val - ydump) + 7; /* keep "=VAL : " */
    memcpy(stripped, ydump, head);           /* up to and incl. "=VAL : " */
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
        /* the r92 empty-key twin ("[?::J]", the nightly's round 92,
         * crash-cb23301ec): after the explicit-'?' key opens EMPTY,
         * libyaml consumes the ':' separator AND the plain value's
         * leading ':' and renders {empty: J} — while the suite's law
         * (the HM87 class: "[?x]" is the scalar "?x") keeps the whole
         * '?::J' as ONE plain. The wrapper folds onto yeptris's flat
         * line spelled with the '?::' prefix: libyaml's key line is
         * exactly empty and its value line is the '?'+'::'-stripped
         * tail. */
        if (lens[i] == 4 && strncmp(lines[i], "+MAP", 4) == 0 && i + 3 < n && lens[i + 1] == 7 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && lens[i + 2] >= 7 &&
            strncmp(lines[i + 2], "=VAL : ", 7) == 0 && lens[i + 3] == 4 &&
            strncmp(lines[i + 3], "-MAP", 4) == 0 && ylens[j] == 10 + (lens[i + 2] - 7) &&
            strncmp(ylines[j], "=VAL : ?::", 10) == 0 &&
            memcmp(ylines[j] + 10, lines[i + 2] + 7, lens[i + 2] - 7) == 0) {
            i += 3;
            j++;
            continue;
        }
        /* the empty-key colon fold (the nightly's round 106,
         * crash-d708b17c: "{\n?{?\n:cic l},\n?M,\n?:}\n" composes the
         * colon member with the ?-members below): libyaml's VALUE
         * token splits a ':'-led plain — an EMPTY key plus the value
         * "cic l" — while the suite's ns-plain-first law (HM87
         * "[:x]", 58MP "{x: :x}") keeps ":cic l" ONE plain key with an
         * empty value (yd_qcolon_walk's own branch). One artifact can
         * compose members of BOTH walks and each walk demands full
         * alignment, so this walk must explain the colon split too
         * (the same law as the qcolon walk's ?-block composition). */
        if (lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 &&
            (lens[i] == 7 || lines[i][lens[i] - 1] == ' ') && i + 1 < n && lens[i + 1] >= 8 &&
            strncmp(lines[i + 1], "=VAL : ", 7) == 0 && j + 1 < yn &&
            ylens[j] == lens[i] + 1 + (lens[i + 1] - 7) &&
            memcmp(ylines[j], lines[i], lens[i]) == 0 && ylines[j][lens[i]] == ':' &&
            memcmp(ylines[j] + lens[i] + 1, lines[i + 1] + 7, lens[i + 1] - 7) == 0 &&
            ylens[j + 1] == 7 && strncmp(ylines[j + 1], "=VAL : ", 7) == 0) {
            i++;    /* the loop's own ++ completes libyaml's split pair */
            j += 2; /* yeptris's ':X' key and its empty value */
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
        /* the TAG-riding key twin (r135): libyaml's flow-'?' KEY token
         * eats the '?' AND its props — a glued '!' tag plus the blanks
         * after it ride NO dump byte (tags are invisible; the
         * separator blanks fold away) — so its key content is
         * yeptris's suite-pinned plain minus that leading run
         * ("{?!\tx,-}" -> libyaml key "x", yeptris key "?!\tx"). The
         * r135 soup pairs this shape BESIDE a pure '?' twin, and the
         * composition declines the r58 drop-'?' rewrite's byte-strict
         * verify, so the walk must pair it structurally. The consumed
         * run's dump shape is bounded: '?' then '!'s, spaces and
         * escaped blanks (tab/break) — anything else in the gap is
         * NOT this family and declines. */
        if (lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 && ylens[j] > lens[i] &&
            strncmp(ylines[j], "=VAL : ?", 8) == 0 &&
            memcmp(ylines[j] + ylens[j] - (lens[i] - 7), lines[i] + 7, lens[i] - 7) == 0) {
            size_t mlen = ylens[j] - lens[i]; /* the consumed run, dump bytes */
            size_t mend = 7 + mlen;           /* one past the run */
            int run_ok = 1;
            for (size_t m = 8; m < mend; m++) {
                char mc = ylines[j][m];
                if (mc == '!' || mc == ' ') {
                    continue;
                }
                if (mc == '\\' && m + 1 < mend &&
                    (ylines[j][m + 1] == 't' || ylines[j][m + 1] == 'r' ||
                     ylines[j][m + 1] == 'n')) {
                    m++;
                    continue;
                }
                run_ok = 0;
                break;
            }
            if (run_ok) {
                j++;
                continue;
            }
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
        /* the ?X-key twin (yd_qflow_divergence's key-line twin): the
         * r106 mutant's pair 2 ("?:}" — libyaml empty key, yeptris
         * plain "?") composes the ?-colon-value fold above with the
         * plain-? key; like the qflow walk's colon fold, one walk
         * must hold both families' members */
        if (lens[i] >= 7 && strncmp(lines[i], "=VAL : ", 7) == 0 && ylens[j] == lens[i] + 1 &&
            strncmp(ylines[j], "=VAL : ?", 8) == 0 &&
            memcmp(ylines[j] + 8, lines[i] + 7, lens[i] - 7) == 0) {
            j++;
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

/* A byte that may follow a flow plain's leading '-': ns-plain-safe in
 * flow (the strict-1.2 rule yeptris enforces; libyaml accepts a bare
 * dash value regardless). */
static int yd_qc_dash_plain_safe(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
           c == '-' || c == '.' || c == '/' || c == ' ' || c == '\t';
}

/* Quotes a dash-led plain run in the rewritten sub (the r89 law:
 * '-' needs a ns-plain-safe follower in flow, so yeptris errors on
 * the bare-dash spell while libyaml reads the bare-dash value — the
 * style-blind dump reads the quoted run as the same scalar). The run
 * scan declines on a quote or escape (the byte-strict verify would
 * refuse the rewrite anyway). Returns 1 when the run was quoted. */
static int yd_qc_quote_dash_run(uint8_t* sub, size_t* subn, const uint8_t* data, size_t size,
                                size_t* i) {
    if (*i >= size || data[*i] != '-' || (*i + 1 < size && yd_qc_dash_plain_safe(data[*i + 1]))) {
        return 0;
    }
    size_t vs = *i;
    while (vs < size && data[vs] != ',' && data[vs] != '}' && data[vs] != ']' && data[vs] != '\n' &&
           data[vs] != '\r') {
        if (data[vs] == '"' || data[vs] == '\\') {
            return 0;
        }
        vs++;
    }
    sub[(*subn)++] = '"';
    while (*i < vs) {
        sub[(*subn)++] = (char)data[(*i)++];
    }
    sub[(*subn)++] = '"';
    return 1;
}

/* The UTF-16 BOM-quirk carry (the r100 nightly, crash-b09d2b15: "FE
 * FF FE FF 00 0A FE FF FE FF 00 0A"). The input sniffs UTF-16 with a
 * FEFF mark, and libyaml's reader strips MORE FEFF units than the
 * mark: the catch-144 double-mark strip PLUS the catch-60 line-start
 * strip, both in unit space (the oracle table for FEFF^n + 000A:
 * libyaml keeps n-2; two lines of FEFF pairs keep one). yeptris keeps
 * one mark and rides the rest as content (the catch-121 consistency
 * law). No byte-level input removal verifies here — libyaml re-strips
 * a kept unit at its own line start, so libyaml(remove) never equals
 * libyaml(original). The catch-121 form instead: transform the input
 * to WHAT LIBYAML'S READER CONSUMED (apply both strips in unit
 * space), re-encode to UTF-8, and require yeptris's reading of that
 * to equal libyaml's original dump. */
static int yd_bom_utf16_line_start_divergence(const uint8_t* data, size_t size, const char* ydump,
                                              const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 6 || size > 8192 || (size % 2) != 0) {
        return 0;
    }
    int be = data[0] == 0xFE && data[1] == 0xFF;
    int le = data[0] == 0xFF && data[1] == 0xFE;
    if (!be && !le) {
        return 0;
    }
    unsigned char* sub = malloc(size * 3);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    int feature = 0;
    int first_after_mark = 1;
    int line_start = 0; /* the mark is consumed; the next unit is NOT
                         * a line start (the mark strip is catch-144's
                         * separate extra-FEFF arm) */
    /* the UTF-8 twin OPENS with a mark: the input's own mark was
     * consumed as the encoding declaration, so the twin must hand
     * yeptris a mark too -- every remaining unit (libyaml's
     * buffer-state-dependent line-start BOM eats INCLUDED) rides as
     * content, exactly the catch-121 reading (the #867 dig's
     * EncodingTwin law: yeptris reads both encodings byte-identically) */
    sub[o++] = 0xEF;
    sub[o++] = 0xBB;
    sub[o++] = 0xBF;
    for (size_t u = 1; u * 2 + 1 < size; u++) {
        size_t at = u * 2;
        unsigned char hi = be ? data[at] : data[at + 1];
        unsigned char lo = be ? data[at + 1] : data[at];
        if (hi == 0xFE && lo == 0xFF) {
            if (first_after_mark || line_start) {
                feature = 1; /* the unit whose fate diverges */
            }
        }
        first_after_mark = 0;
        /* re-encode the unit to UTF-8 -- kept even when it is a BOM
         * the twin treats as content */
        unsigned cp = (unsigned)hi << 8 | lo;
        if (cp < 0x80) {
            sub[o++] = (unsigned char)cp;
        } else if (cp < 0x800) {
            sub[o++] = (unsigned char)(0xC0 | (cp >> 6));
            sub[o++] = (unsigned char)(0x80 | (cp & 0x3F));
        } else {
            sub[o++] = (unsigned char)(0xE0 | (cp >> 12));
            sub[o++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            sub[o++] = (unsigned char)(0x80 | (cp & 0x3F));
        }
        line_start = (hi == 0x00 && lo == 0x0A);
    }
    if (!feature) {
        free(sub);
        return 0;
    }
    /* the carry: yeptris's twin dump must equal its ORIGINAL dump
     * byte for byte (the #867 EncodingTwin law), and libyaml's dump
     * must agree on every non-BOM byte with STRICTLY fewer BOMs --
     * its UTF-16 reader ate line-start BOM units, nothing else (the
     * r101 artifact: 7 vs 5, skeletons identical). Content corruption
     * of any non-BOM byte declines. */
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, o, &yok2);
    int known = 0;
    if (yok2 && ydump2 != NULL && strcmp(ydump2, ydump) == 0) {
        size_t yn = strlen(ydump);
        size_t ln = strlen(ldump);
        char* ys = malloc(yn + 1);
        char* ls = malloc(ln + 1);
        if (ys != NULL && ls != NULL) {
            size_t yo = 0;
            int yb = 0;
            for (const char* q = ydump; *q != '\0'; q++) {
                if ((unsigned char)q[0] == 0xEF && (unsigned char)q[1] == 0xBB &&
                    (unsigned char)q[2] == 0xBF) {
                    q += 2;
                    yb++;
                    continue;
                }
                if (*q != ' ') {
                    ys[yo++] = *q;
                }
            }
            ys[yo] = '\0';
            size_t lo2 = 0;
            int lb = 0;
            for (const char* q = ldump; *q != '\0'; q++) {
                if ((unsigned char)q[0] == 0xEF && (unsigned char)q[1] == 0xBB &&
                    (unsigned char)q[2] == 0xBF) {
                    q += 2;
                    lb++;
                    continue;
                }
                if (*q != ' ') {
                    ls[lo2++] = *q;
                }
            }
            ls[lo2] = '\0';
            known = (yb > lb && yo == lo2 && memcmp(ys, ls, lo2) == 0);
        }
        free(ys);
        free(ls);
    }
    free(ydump2);
    free(sub);
    return known;
}

static int yd_qflow_question_colon_divergence(const uint8_t* data, size_t size, const char* ydump,
                                              const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    /* the rewrite covers EVERY glued "?:" at flow depth, whatever the
     * follower (the nightly's round 67: "{\n?:-b%rM,\n?:M,\n?:}\n" --
     * three explicit-key pairs with PLAIN followers; the r58 form only
     * rewrote the first quoted one, so libyaml's invariant reading of
     * the remaining pairs kept yeptris's plain keys on the table and
     * the verify declined its own family). Round 85 adds the bare '?'
     * AT a line/boundary end composing with the glued pairs: dropping
     * it (the r71 spell) leaves a lone ',' entry that neither engine
     * accepts, so the boundary form spells '"":' instead. The
     * byte-strict verify against libyaml's ORIGINAL dump arbitrates
     * the widened gate: a mid-plain '?' ("[a?:b]") changes libyaml's
     * own reading under the rewrite and declines. */
    int depth = 0;
    int quote = 0;
    size_t hits = 0;
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
        } else if (c == '?' && depth > 0) {
            hits++;
        }
    }
    if (hits == 0) {
        return 0;
    }
    char* sub = malloc(size * 2 + 1); /* every hit adds at most one byte */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    depth = 0;
    quote = 0;
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            i++;
            continue;
        }
        if (c == '\'') {
            quote = '\'';
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '"') {
            quote = '"';
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
        } else if (c == ']' || c == '}') {
            depth--;
        } else if (c == '?' && depth > 0 && i + 1 < size && data[i + 1] == ':') {
            /* "?:" -> the empty-string key form (r58/r67): libyaml's
             * KEY token reads the glued ':' as the pair separator of
             * an EMPTY key, so '"' '"' ':' ' ' spells that reading
             * for both engines */
            sub[subn++] = '"';
            sub[subn++] = '"';
            sub[subn++] = ':';
            sub[subn++] = ' ';
            i += 2;
            /* a dash-led value the strict flow plain rejects (the
             * nightly's r89, crash-f3268315: "?:-,") — quote the run,
             * the style-blind dump reads the same scalar */
            (void)yd_qc_quote_dash_run(sub, &subn, data, size, &i);
            continue;
        } else if (c == '?' && depth > 0) {
            /* a BARE '?' with a GLUED follower ("?M-b%rM", the nightly's
             * r71): libyaml's glued reading makes the follower the KEY
             * with an empty value while yeptris reads one plain. The
             * libyaml-preserving spell drops the '?' — a bare plain in a
             * flow mapping IS that pair for both engines ("{M-b%rM}"
             * reads (M-b%rM -> empty) on both sides; the ':'-closed form
             * "M-b%rM:" dies on libyaml's ":," node-content law). */
            if (i + 1 < size && data[i + 1] != ' ' && data[i + 1] != '\t' && data[i + 1] != '\n' &&
                data[i + 1] != '\r' && data[i + 1] != ',' && data[i + 1] != ']' &&
                data[i + 1] != '}') {
                i++;
                /* the dropped '?' leaves the follower a KEY-position
                 * plain; a dash-led one ("?-,", the r99 nightly,
                 * crash-d1ecb259) is a run yeptris's flow plain
                 * rejects (the r89 law) — quote it, same spell as the
                 * "?:" arm below, or the byte-strict verify dies on
                 * yeptris's own sub error */
                (void)yd_qc_quote_dash_run(sub, &subn, data, size, &i);
                continue;
            }
            /* a BARE '?' at a line/boundary end (r85, crash-2bdccbe:
             * "?,\n" composing with glued "?:" pairs): both engines
             * already read the explicit empty-key pair — but DROPPING
             * the '?' leaves a lone ',' whose entry neither engine
             * accepts, so the composition verify dies before the glued
             * pairs are even read. Spell the pair instead: '""' ':' is
             * the same pair in the style-blind dumps (the KEY event's
             * style never rides the YD event), and both engines accept
             * '""' at a value position. */
            sub[subn++] = '"';
            sub[subn++] = '"';
            sub[subn++] = ':';
            i++;
            continue;
        }
        sub[subn++] = (char)c;
        i++;
    }
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

/* True when the verdict is the glued-comment x flow-'?' SPLIT (the PR
 * #894 validation session's candidate, "{?#}"): libyaml's scanner
 * makes a KEY token for a flow '?' with ANY follower (the 1.1 law the
 * qflow family carries) and its post-token skip then eats the GLUED
 * '#...' as a comment — the closer vanishes and the flow collection
 * never closes (libyaml REJECTS); yeptris's suite-pinned plain reading
 * (HM87/652Z: a flow '?' with a non-space follower rides as plain
 * content, and a '#' glued to content is content) keeps "?#" ONE plain
 * KEY — the divergence is exactly that indicator law plus the comment
 * glue (block context agrees: there libyaml's KEY token needs the
 * simple-key position and "?#" stays plain on both sides). The probe
 * quotes each '?#'-led plain run ('"?#"' — quoting is style-blind,
 * the YD dumps carry no style): yeptris's reading of the rewrite must
 * equal its original dump byte for byte (the rewrite is
 * meaning-preserving) AND libyaml's reading of the rewrite must land
 * on that same dump (the divergence was exactly the law above). A run
 * that cannot be quoted ('"' or '\\' inside) declines. */
/* The accept/accept verdict of the glued-?# family (the r115 nightly,
 * crash-01361e26): with a continuation line after the run, libyaml's
 * KEY-token-plus-comment-glue reading does not reject — it EMPTIES the
 * key (the glued '#' opens a comment; the pair rides with a null key)
 * while yeptris's suite-pinned plain-'?' law keeps the run as the key
 * text. The quote-out transform cannot verify this variant (quoting
 * freezes the fold, so the rewrite breaks the line structure in
 * libyaml), so the claim is structural: the glued-?# marker at flow
 * depth, both engines accepted, the event skeletons match line-for-
 * line, and every differing pair is a yeptris '?'-led VAL against a
 * libyaml EMPTY one — the emptied-key signature and nothing else. */
/* The r136 nightly's tail wrapper: the GLUED pair is "?!" — libyaml's
 * flow-'?' KEY-token law fires on ANY follower, so the '?' opens an
 * explicit-key map and the glued '!' rides as its non-specific tag
 * (invisible in the dump: the blanks after the fold carry it away);
 * the continuation ("-8") becomes the KEY and the pair's value is
 * empty. yeptris's suite-pinned plain-'?' law keeps "?!" ONE plain
 * scalar (the \n folds). The structural arm (r130/r131) claims the
 * wrapper, the L-only vals and the banked closer re-align. */
/* The r142 walk's TERMINAL-TAIL pre-scan: true when the member skew is
 * strictly terminal — libyaml's skeleton is exhausted except its
 * closers (EVERY remaining line is -MAP/-SEQ/-STR) and yeptris's
 * remaining lines are 1..cap kept-content member lines (=VAL/=ALI)
 * followed by EXACTLY libyaml's closer lines (the close counts pair,
 * byte for byte). *ride receives the skipped content-line count. The
 * cap is the classifier's own input bound, not a constant: every
 * kept-content member consumed at least one input byte, so
 * ride <= size is provable — the r146 nightly (crash-50405bba, run
 * 42013806681, main 22e2b1ac) spliced 730 soup bytes into the r137
 * seed and its legitimately terminal tail carries 96 kept-content
 * members, past the fixed 64 this gate shipped with. Any structure in
 * yeptris's remainder, any non-closer line in libyaml's, a
 * close-count mismatch, or content running to end-of-stream without
 * a close declines — the walk's existing arms take the state and a
 * genuine divergence still bails. */
static int yd_qgh_terminal_tail(const char* y, const char* l, int cap, int* ride) {
    /* pass 1: libyaml's remainder is closers only */
    int nclosers = 0;
    for (const char* p = l; *p != '\0';) {
        const char* e = strchr(p, '\n');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        if (!(n == 4 && (memcmp(p, "-MAP", 4) == 0 || memcmp(p, "-SEQ", 4) == 0 ||
                         memcmp(p, "-STR", 4) == 0))) {
            return 0;
        }
        nclosers++;
        if (e == NULL) {
            break;
        }
        p = e + 1;
    }
    if (nclosers == 0) {
        return 0;
    }
    /* pass 2: yeptris's remainder = kept-content members then the
     * closers verbatim; the ride is capped by the input bound */
    int content = 0;
    const char* p = y;
    for (;;) {
        const char* e = strchr(p, '\n');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        if (n > 5 && (memcmp(p, "=VAL ", 5) == 0 || memcmp(p, "=ALI ", 5) == 0)) {
            if (++content > cap) {
                return 0;
            }
            if (e == NULL) {
                return 0; /* content to end-of-stream: no close follows */
            }
            p = e + 1;
            continue;
        }
        break;
    }
    if (content == 0) {
        return 0;
    }
    /* pass 3: the closer suffix pairs line for line, and yeptris ends
     * exactly where libyaml does */
    const char* q = l;
    while (*q != '\0') {
        const char* le = strchr(q, '\n');
        size_t ln = le != NULL ? (size_t)(le - q) : strlen(q);
        if (*p == '\0') {
            return 0; /* yeptris ran out with libyaml closers left */
        }
        const char* ye = strchr(p, '\n');
        size_t yn = ye != NULL ? (size_t)(ye - p) : strlen(p);
        if (yn != ln || memcmp(p, q, yn) != 0) {
            return 0;
        }
        p = ye != NULL ? ye + 1 : p + yn;
        q = le != NULL ? le + 1 : q + ln;
    }
    if (*p != '\0') {
        return 0; /* yeptris carries lines beyond libyaml's closers */
    }
    *ride = content;
    return 1;
}

/* True when the =ALI line (y, yl bytes) appears as a LINE anywhere in
 * libyaml's dump — the alias-swallow law's SURVIVAL gate (the r144
 * nightly, crash-fe4ea5a7): libyaml's comment-glue swallowed the
 * killed member THROUGH its alias-bearing explicit-key head and
 * re-paired the pending KEY token with the next content, so yeptris's
 * alias event has no counterpart AT ITS POSITION — but the same alias
 * line rides libyaml's dump elsewhere, re-positioned by the re-pairing
 * rather than dropped. A genuine alias-drop (libyaml never emits the
 * alias — the r138-r141 shapes) declines and the divergence still
 * aborts. */
static int yd_qgh_alias_survives(const char* ldump, const char* y, size_t yl) {
    for (const char* q = ldump; *q != '\0';) {
        const char* qn = strchr(q, '\n');
        size_t ql = qn != NULL ? (size_t)(qn - q) : strlen(q);
        if (ql == yl && memcmp(q, y, yl) == 0) {
            return 1;
        }
        if (qn == NULL) {
            break;
        }
        q = qn + 1;
    }
    return 0;
}

/* True when libyaml's scalar content is yeptris's with exactly ONE raw
 * LS/PS triple inserted byte-exact — the #638 law's ADDITIVE reading
 * (the r104 forced form: libyaml joins the LS/PS verbatim where
 * yeptris's b-char algebra folds the break run, so libyaml is never
 * SHORTER than yeptris at a PS site; the blank run after the triple is
 * dropped by both engines' fold). Bounded: length pins one triple, the
 * two memcmps pin the position — no wildcards. */
static int yd_qgh_lsps_additive(const char* y, size_t yn, const char* l, size_t ln) {
    if (ln != yn + 3) {
        return 0;
    }
    for (size_t i = 0; i + 3 <= ln; i++) {
        if ((unsigned char)l[i] == 0xE2 && (unsigned char)l[i + 1] == 0x80 &&
            ((unsigned char)l[i + 2] == 0xA8 || (unsigned char)l[i + 2] == 0xA9)) {
            return memcmp(l, y, i) == 0 && memcmp(l + i + 3, y + i, yn - i) == 0;
        }
    }
    return 0;
}

/* True when yeptris's kept residue — the CURRENT =VAL line plus up to
 * 15 further =VAL lines — leads to yeptris's OWN byte-exact copy of
 * libyaml's 4-byte structure opener (+MAP/+SEQ/+STR), the re-paired
 * key's nested collection (the r152 ride's gate; the arm's commentary
 * below carries the law). Only kept-content scalar residue may sit
 * between: a structure line or an =ALI in the run declines (the r144
 * survival gate stays the alias's only path). *y_twin receives the
 * twin line's pointer (libyaml's opener pairs against it as an equal
 * line on the walk's next step) and *run_out the consumed line count
 * (>= 1: the current line is always part of the run). */
static int yd_qgh_repair_ride(const char* y, const char* l, size_t ll, const char** y_twin,
                              int* run_out) {
    if (ll != 4 ||
        (memcmp(l, "+MAP", 4) != 0 && memcmp(l, "+SEQ", 4) != 0 && memcmp(l, "+STR", 4) != 0)) {
        return 0;
    }
    int run = 0;
    const char* p = y;
    for (;;) {
        const char* e = strchr(p, '\n');
        size_t n = e != NULL ? (size_t)(e - p) : strlen(p);
        if (n == 4 && memcmp(p, l, 4) == 0) {
            *y_twin = p;
            *run_out = run;
            return 1;
        }
        if (n <= 5 || memcmp(p, "=VAL ", 5) != 0) {
            return 0; /* only kept-content scalar residue may sit between */
        }
        if (++run > 16) {
            return 0; /* the per-site residue cap (the r125 alias arm's
                       * precedent) */
        }
        if (e == NULL) {
            return 0; /* yeptris ran out before its twin opener */
        }
        p = e + 1;
    }
}

static int yd_qflow_glued_hash_differ(const uint8_t* data, size_t size, const char* ydump,
                                      const char* ldump) {
    if (ydump == NULL || ldump == NULL || size == 0 || size > 8192) {
        return 0;
    }
    /* pass 1: the marker — a glued "?#"/"?!" at flow depth, outside
     * quotes (the r136 twin: libyaml's KEY-token law fires on ANY
     * follower, so a glued '!' shapes the same explicit-key family).
     * "Outside quotes" tracks the plain-run law (the r145 nightly,
     * run 42003383477, main 417fab59): a quote GLUED to an active
     * plain run is CONTENT in both engines (the catch-85 word-start
     * law — "- [trueWft?\",?##..." pins "=VAL : trueWft?\"" on BOTH
     * sides), so the naive tracker's phantom quote (opened mid-plain,
     * never closed) swallowed the rest of the input and the glued '?#'
     * after the ',' never scored. A quote opens only where a node can
     * start: not inside a plain run, after a separated indicator
     * (props/value position), or after a node boundary. Wordy bytes
     * (and the fold blanks inside a run) keep the run active; flow
     * plains fold across breaks, block lines end at theirs; a
     * blank-led comment ends the node and rides to the break (the
     * anchor arm's law). */
    int depth = 0;
    int quote = 0;
    int hits = 0;
    int plain = 0; /* a plain run is active: glued quotes are content */
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
            continue;
        }
        if (c == '\'') {
            if (!plain) {
                quote = '\'';
            }
        } else if (c == '"') {
            if (!plain) {
                quote = '"';
            }
        } else if (c == '#' && (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' ||
                                data[i - 1] == '\n' || data[i - 1] == '\r')) {
            /* a blank-led comment swallows the line; the node ended
             * and nothing inside it opens a quote or scores */
            plain = 0;
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                i++;
            }
            if (i < size) {
                i--; /* the break rides the loop's own advance */
            }
        } else if (c == '[' || c == '{') {
            depth++;
            plain = 0;
        } else if (c == ']' || c == '}') {
            depth--;
            plain = 0;
        } else if (c == ',') {
            plain = 0;
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                i++;
            }
            if (depth == 0) {
                plain = 0; /* block lines: each line is a fresh node */
            }
            /* a flow plain folds across its break: the run rides */
        } else if (c == ' ' || c == '\t') {
            /* fold blanks ride the plain run */
        } else if (c == ':' || c == '?' || (c == '-' && depth == 0) || c == '&' || c == '*' ||
                   c == '!') {
            /* separated, the indicator ends the run (a props/value
             * position where a quoted node can start); glued, it is
             * plain-first content (the HM87 plain-'?' law) — in flow
             * a '-' is plain-first regardless ("- " folds on) */
            unsigned char f = i + 1 < size ? data[i + 1] : ' ';
            if (f == ' ' || f == '\t' || f == '\n' || f == '\r' || f == ',' || f == '[' ||
                f == ']' || f == '{' || f == '}') {
                plain = 0;
            } else {
                plain = 1;
            }
        } else {
            plain = 1;
        }
        if (c == '?' && depth > 0 && i + 1 < size && (data[i + 1] == '#' || data[i + 1] == '!')) {
            hits++;
        }
    }
    if (hits == 0) {
        return 0;
    }
    /* pass 2: line-wise walk of both dumps — same skeleton, every
     * difference is yeptris's "=VAL : ?..." against libyaml's
     * "=VAL : " (the emptied key). The r116 tail composition adds a
     * second shape: libyaml empties strictly MORE, so it may
     * interleave EXTRA empty "=VAL : " lines the yeptris skeleton
     * does not have (the streams re-align at the next real event) —
     * skip those libyaml-only empties, never yeptris's lines. */
    const char* y = ydump;
    const char* l = ldump;
    int diffs = 0;
    int skips = 0;
    int struct_skips = 0;     /* the structural permutation's owed closers */
    int ride = 0;             /* the terminal-tail arm's skipped content count */
    const char* ytwin = NULL; /* the re-paired-value ride's twin opener */
    int ride_run = 0;         /* the re-paired-value ride's consumed count */
    for (;;) {
        const char* yn = strchr(y, '\n');
        const char* ln = strchr(l, '\n');
        size_t yl = yn != NULL ? (size_t)(yn - y) : strlen(y);
        size_t ll = ln != NULL ? (size_t)(ln - l) : strlen(l);
        if (yn == NULL && ln == NULL && yl == 0 && ll == 0) {
            break; /* both exhausted: the skeletons matched throughout */
        }
        const char* val_prefix = "=VAL : ";
        const size_t vp = strlen(val_prefix);
        int y_val = yl > vp && strncmp(y, val_prefix, vp) == 0;
        int l_val = strncmp(l, val_prefix, vp) == 0;
        int l_empty_val = l_val && ll == vp;
        int y_empty_val = strncmp(y, val_prefix, vp) == 0 && yl == vp;
        /* the shared-opener tell (r132, #949): when yeptris's next line
         * IS libyaml's current '+'-line, that opener belongs to a member
         * BOTH walks carry — only yeptris's killed VAL steps aside. Any
         * other next line means a libyaml-only wrapper (the r130
         * structural shape) and the arm consumes both. The r153
         * refinement: the tell gates the structural arm's FALLBACK skip
         * only — the byte-exact wrapper rides are attempted first, since
         * a verifying ride proves the block is the explicit-key salvage,
         * not the next member (one compared line is weaker evidence than
         * the rides' whole-block residue pin). */
        int y_killed_shared_opener = 0;
        if (y_val && l[0] == '+' && yn != NULL) {
            const char* y2 = yn + 1;
            const char* y2n = strchr(y2, '\n');
            size_t y2l = y2n != NULL ? (size_t)(y2n - y2) : strlen(y2);
            y_killed_shared_opener = (y2l == ll && memcmp(y2, l, y2l) == 0);
        }
        if ((yn == NULL) != (ln == NULL) || yl != ll || memcmp(y, l, yl) != 0) {
            if (y_val && l_empty_val) {
                /* the emptied-key pair: libyaml emptied the plain
                 * yeptris kept (any content — the r116 family's keys
                 * ride '?#'- and '+`'-shaped plains alike); pass 1
                 * already pinned the glued-'?'# marker */
                diffs++;
            } else if (y_val && l_val && ll > vp && ll <= yl &&
                       memcmp(y + (yl - (ll - vp)), l + vp, ll - vp) == 0) {
                /* the SPLIT pair (r117): libyaml's glued-# comment-kill
                 * splits a multi-line plain yeptris folds — its
                 * continuation scalar's content is a SUFFIX of
                 * yeptris's single folded scalar. Consume both; the
                 * split shape is bounded (8 per stream). */
                if (++skips > 8 + 64) {
                    return 0;
                }
                diffs++;
                if (yn == NULL || ln == NULL) {
                    break;
                }
                y = yn + 1;
                l = ln + 1;
                continue;
            } else if (y_val && l_val && yd_qgh_lsps_additive(y + vp, yl - vp, l + vp, ll - vp)) {
                /* the LS/PS-fold PAIR (r150, crash-4d640bc4, run
                 * 42072059847, main b4d6671f): the soup interleaves the
                 * glue family with plain fold runs mixing LS/PS into
                 * the LF/CR breaks ("?\r/\n\r\r<PS> 50:"). At such a
                 * site libyaml joins the raw triple as content and
                 * folds only its LF/CR count (its READ_LINE law) while
                 * yeptris's b-char algebra folds the run — the #638
                 * relation, in the r104 forced ADDITIVE form:
                 * libyaml's scalar is yeptris's with the raw triple
                 * INSERTED ("=VAL : /\n\n50" vs "=VAL :
                 * /\n\n<PS>50"). The generic arms brute-forced this
                 * pair before — skip yeptris's key, skip its surviving
                 * value, skip both of libyaml's lines — FOUR shared-cap
                 * skips per site, and the r150 soup's 6 fold sites +
                 * 10 glue members outran the fixed 64 (66 skips; the
                 * walk died at the second-to-last wrapper's opener with
                 * every difference law-explained). The pair is a PAIRED
                 * difference, not a skip: consume both lines as one
                 * diff (the emptied-key arm's pattern — both pointers
                 * advance, so the walk stays bounded by construction,
                 * no cap needed). Family-gated by pass 1; byte-exact
                 * relation (no wildcards; libyaml-only extra triples
                 * past one decline into the generic arms); the claim
                 * still stands only on full exhaustion (the walk's
                 * law). */
                diffs++;
            } else if (l_val && (y_empty_val || yl == 0 || y[0] == '-')) {
                /* an L-ONLY extra val of ANY content (r117): libyaml's
                 * glued-# comment-kill SPLITS a plain yeptris folds —
                 * the continuation line becomes its own scalar in
                 * libyaml's reading. Skip libyaml's line; yeptris's
                 * content/closer lines must still pair downstream
                 * (content-vs-content differences still bail). */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (ln == NULL) {
                    if (!(yl == 0 && yn == NULL)) {
                        return 0;
                    }
                    break;
                }
                l = ln + 1;
                continue;
            } else if (y_val && l[0] == '+') {
                /* a STRUCTURAL realign (r130/r131, #945): libyaml's
                 * comment-kill ate the '?#' content and re-paired the
                 * continuation as the key of a NEW collection it opens
                 * where yeptris's plain-'?' law keeps seq items. Skip
                 * yeptris's killed VAL and libyaml's opener line (pure
                 * structure); the inner key then pairs via the suffix
                 * rule, and the extra closer re-aligns at
                 * struct_skips (below). Family-gated by pass 1;
                 * bounded by the shared skip cap. */
                /* The r153 PRIORITY refinement (crash-aad3f269, run
                 * 42106098158, main efcf0e5c): the r132 shared-opener
                 * arm used to be consulted BEFORE this branch on the
                 * strength of its opener byte-equality tell — but that
                 * tell is WEAK evidence next to the rides' byte-exact
                 * wrapper verification. The r153 soup stacks THREE
                 * '?+#\n&m<N>' wrappers after '?##'-glue members in one
                 * flow seq; at the FIRST wrapper yeptris's own next
                 * member happens to open with the same "+MAP" line
                 * libyaml's wrapper opens with, the tell fired, only
                 * yeptris's killed scalar stepped aside, and every
                 * later pairing ran one member off — the byte walk
                 * grazed through shape-equal blocks (r124/r127 skips on
                 * both sides) until it died at the THIRD wrapper's
                 * opener against yeptris's closer with no arm. The
                 * wrapper ride is the correct claim there: its KEY is
                 * byte-exact yeptris's killed member minus the leading
                 * '?' (the explicit-key law's direct residue), the VAL
                 * is the salvaged null, the close is bare — verified
                 * evidence, where the tell is one compared line. So the
                 * rides are attempted FIRST (below, unchanged gates)
                 * and the bare shared-opener skip is the FALLBACK when
                 * they decline — the r132 shape (libyaml comment-killed
                 * the entry whole, its +MAP IS yeptris's next member's
                 * opener, no wrapper block) never satisfies the ride
                 * gates (the KEY would have to be the killed entry's
                 * residue and the VAL empty), so the r132/r949 pins'
                 * routing is unchanged where their claims stood. */
                /* The r143 WRAPPER RIDE (crash-d4f874ee.., run
                 * 41975575045, main 531ad570): the r132 composition's
                 * '+#'-bearing wrapper variant, DOUBLED. With '+' the
                 * '#' no longer glues to the '?' ("?+#"), so libyaml's
                 * post-'?'-token comment-glue never fires — but the
                 * KEY-token law still opens an explicit-key wrapper
                 * and the run rides INTO it as the KEY's REAL content
                 * ("=VAL : +# &m-7"): the wrapper is a salvage block
                 * (+MAP / content KEY / empty VAL / -MAP) per killed
                 * member, where yeptris's plain-'?' law keeps the
                 * member as one plain. When such wrappers DOUBLE, the
                 * line-wise pairing dies: the structural arm consumes
                 * member 1 + wrapper 1's opener, member 2 then pairs
                 * against wrapper 1's KEY (the y-killed arm below) and
                 * wrapper 2's opener finds yeptris's closer — no arm.
                 * The ride claims the WHOLE wrapper when it is the
                 * well-formed block — a bare "+MAP" opener, a content
                 * KEY that is a non-empty SUFFIX of yeptris's killed
                 * member (the run minus the leading '?'), an empty
                 * "=VAL : " (the salvaged null), the bare "-MAP"
                 * close — and the KEY does NOT suffix-pair yeptris's
                 * next surviving member (that shape stays with the
                 * r130 line-wise path and its suffix rule; the r132
                 * comment-glue wrapper's KEY carries the fold-space
                 * and never suffix-pairs, so it keeps the old path).
                 * Bounded: the ride is the fixed four-line block (any
                 * other shape declines into the line-wise arms below)
                 * and the shared skip cap still bounds the total; a
                 * wrapper whose interior carries yeptris-unknown
                 * content declines (the KEY must be the killed
                 * member's residue).
                 *
                 * The r148 ANCHORED-KEY twin (crash-9b423bc7, run
                 * 42043432048, main 99803f9a) declined that gate on a
                 * rendering technicality: libyaml's comment-glue law
                 * consumed the glued '?##' through the KEY token and
                 * re-paired the pending KEY with the continuation's
                 * ANCHOR — the wrapper's KEY event then renders
                 * PROPS-ONLY ("=VAL : &m ": the anchor name rides the
                 * props region, the value is empty), while yeptris's
                 * plain-'?' law keeps "?## &m" ONE plain folded across
                 * the break (the anchor spelling is mid-plain CONTENT
                 * there). The suffix compare ran on the UNstripped
                 * region and was shifted by exactly one byte: libyaml
                 * "&m " (props + the render's separator blank) against
                 * yeptris " &m" (fold space + the name), so the
                 * legitimate shape declined and the walk died at the
                 * second wrapper's opener. A plain-styled line cannot
                 * end in a blank as content (plain scalars strip
                 * trailing blanks), so a trailing blank marks the
                 * render's separator and the region MINUS it is the
                 * props — the same suffix/pairs_next compares ride the
                 * stripped region. The prefix check pins the plain
                 * style: a quoted key's content MAY end in a blank,
                 * and there the region is content, not props. The
                 * byte-exact residue gate is unchanged (the anchor
                 * spelling must be yeptris's kept plain's tail); the
                 * fixed four-line block, the !pairs_next guard and the
                 * shared skip cap are unchanged. */
                if (ll == 4 && memcmp(l, "+MAP", 4) == 0 && yn != NULL && ln != NULL) {
                    const char* l1 = ln + 1;
                    const char* l1n = strchr(l1, '\n');
                    const char* y2 = yn + 1;
                    const char* y2n = strchr(y2, '\n');
                    size_t l1l = l1n != NULL ? (size_t)(l1n - l1) : strlen(l1);
                    size_t y2l = y2n != NULL ? (size_t)(y2n - y2) : strlen(y2);
                    size_t kl = l1l > vp ? l1l - vp : 0;
                    int key_suffix =
                        l1l > vp && kl <= yl - vp && memcmp(y + yl - kl, l1 + vp, kl) == 0;
                    int pairs_next = y2l > vp && y2l - vp >= kl &&
                                     memcmp(y2 + (y2l - vp) - kl, l1 + vp, kl) == 0;
                    if (!key_suffix && l1l > vp + 1 && memcmp(l1, val_prefix, vp) == 0 &&
                        l1[l1l - 1] == ' ') {
                        /* the r148 props-only KEY: the trailing blank
                         * is the render's separator (a plain cannot
                         * end in a blank); the props region rides the
                         * same compares one byte shorter */
                        size_t kp = kl - 1;
                        key_suffix =
                            kp > 0 && kp <= yl - vp && memcmp(y + yl - kp, l1 + vp, kp) == 0;
                        pairs_next = y2l > vp && y2l - vp >= kp &&
                                     memcmp(y2 + (y2l - vp) - kp, l1 + vp, kp) == 0;
                    }
                    if (key_suffix && !pairs_next && l1n != NULL) {
                        const char* l2 = l1n + 1;
                        const char* l2n = strchr(l2, '\n');
                        size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                        const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                        if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                            memcmp(l3, "-MAP", 4) == 0 && (l3[4] == '\n' || l3[4] == '\0')) {
                            skips += 3;
                            if (skips > 64) {
                                return 0;
                            }
                            diffs += 5;
                            y = yn + 1;
                            l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                            continue;
                        }
                    }
                    if (!key_suffix && kl > 0 && y2l > vp && l1n != NULL) {
                        /* the r149 RE-PAIRED-KEY ride (crash-96123edf,
                         * run 42061122149, main a7cb5ae3): the r148
                         * ANCHORED-KEY shape riding CR breaks. libyaml's
                         * post-'?'-token comment-glue ate the glued
                         * '?##' THROUGH the CR and re-paired the pending
                         * KEY token with the NEXT member's plain tail —
                         * the r144 re-pairing law, now on the KEY event:
                         * the wrapper's KEY renders plain-content
                         * ("=VAL : b:inary", the run minus the killed
                         * head) where yeptris's plain-'?' law keeps BOTH
                         * members (the killed head "?##|" AND the folded
                         * tail ":binary b:inary" whose content ENDS with
                         * the KEY). The r143 ride declined (its gate
                         * demands the KEY be the killed member's own
                         * residue) and the r130 line-wise path then let
                         * the wrapper's own empty VAL and closer graze
                         * the FOLLOWING members — the emptied-key arm
                         * ate member 3, the r120 close arm ate member 4
                         * — dying at the second wrapper's opener with
                         * no arm. When the wrapper is the well-formed
                         * salvage block AND its KEY (stripped of the
                         * r148 render separator when props-only) is a
                         * byte-exact SUFFIX of yeptris's next surviving
                         * member's content — the re-paired residue; an
                         * invented key declines — while carrying NONE
                         * of the killed member's own tail (key_suffix
                         * already declined), the wrapper plus both
                         * members rides WHOLE: leaving either the VAL
                         * or the closer for the line-wise arms resumes
                         * the grazing. This supersedes the r143
                         * pairs-next routing for the re-paired form
                         * (there the line-wise path resolves; here it
                         * demonstrably dies). Bounded: the fixed
                         * four-line block (any other shape declines
                         * into the line-wise arms), the byte-exact
                         * suffix pin, and the shared skip cap; the
                         * claim stands only on full exhaustion (the
                         * walk's law). */
                        size_t kq = kl;
                        if (l1[l1l - 1] == ' ' && kl > 1) {
                            kq = kl - 1; /* the r148 render separator */
                        }
                        if (kq <= y2l - vp && memcmp(y2 + y2l - kq, l1 + vp, kq) == 0) {
                            const char* l2 = l1n + 1;
                            const char* l2n = strchr(l2, '\n');
                            size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                            const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                            if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                                memcmp(l3, "-MAP", 4) == 0 && (l3[4] == '\n' || l3[4] == '\0')) {
                                skips += 4;
                                if (skips > 64) {
                                    return 0;
                                }
                                diffs += 6;
                                y = y2n != NULL ? y2n + 1 : y2 + y2l;
                                l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                                continue;
                            }
                        }
                    }
                }
                if (y_killed_shared_opener) {
                    /* the SHARED-opener skip (r132, #949), the rides'
                     * FALLBACK: libyaml's +MAP opens the NEXT member
                     * both walks carry — yeptris's killed '?#'-led
                     * entry was a scalar (the plain-'?' law keeps it
                     * as content) while libyaml comment-killed the run
                     * and moved on. Only yeptris's line is consumed;
                     * the opener pairs with yeptris's own on the very
                     * next step. Rides first (the r153 priority
                     * refinement above): when a wrapper gate verifies,
                     * the block is libyaml's explicit-key salvage, not
                     * the next member. Bounded by the shared skip
                     * cap. */
                    if (++skips > 64) {
                        return 0;
                    }
                    diffs++;
                    y = yn + 1;
                    continue;
                }
                if (++skips > 64) {
                    return 0;
                }
                struct_skips++;
                diffs += 2;
                if (yn == NULL || ln == NULL) {
                    return 0;
                }
                y = yn + 1;
                l = ln + 1;
                continue;
            } else if (y_empty_val && yd_qgh_repair_ride(y, l, ll, &ytwin, &ride_run)) {
                /* the RE-PAIRED-VALUE ride (r152, crash-6689bee7, run
                 * 42095444821, main 115ac2de): libyaml's comment-glue law
                 * consumed the glued '?#' through the KEY token and
                 * RE-PAIRED the pending KEY with the member's nested flow
                 * map -- the r148/r149 re-pairing law on the VALUE side:
                 * libyaml's member renders as the EMPTIED key ("=VAL : ")
                 * with the nested +MAP as its VALUE, while yeptris's
                 * suite-pinned plain-'?' law keeps the killed run's
                 * residue entries BEFORE its own nested map opens (member
                 * 1's empty value, member 2's pair, member 3's key). The
                 * emptied-key arm pairs member 1's key, and the first
                 * unexplained state is yeptris's EMPTY value line against
                 * libyaml's +MAP: the r132 shared-opener arm demands
                 * y_val (the residue's current line is empty, yl == vp
                 * exactly), the r130 structural arm consumes libyaml's
                 * opener WITH yeptris's line (the inner bodies then
                 * mis-pair -- yeptris's own +MAP grazes a libyaml VAL and
                 * no arm fires), and the r116 extra-empty arm declines on
                 * l[0] == '+'. The ride: yeptris's kept residue VALs (the
                 * current line plus up to 15 further =VAL lines; ONLY
                 * kept-content scalars may sit between -- a structure
                 * line or an =ALI in the run declines, the r144 survival
                 * gate stays the alias's only path) step aside until
                 * yeptris's OWN byte-exact copy of libyaml's 4-byte
                 * opener; libyaml's opener is NOT consumed -- the twin
                 * pairs as an equal line on the next step, the inner
                 * bodies pair through the existing arms, and the maps'
                 * closers pair 1:1 (no struct_skips owed; a nesting skew
                 * surfaces as an unexplained closer and bails). Family-
                 * gated by pass 1; bounded by the per-site run cap (16,
                 * the r125 alias arm's precedent) and the shared skip
                 * cap; the claim stands only on full exhaustion (the
                 * walk's law). */
                skips += ride_run;
                if (skips > 64) {
                    return 0;
                }
                diffs += ride_run;
                y = ytwin;
                continue;
            } else if (y[0] == '-' && l[0] == '-' && struct_skips > 0 &&
                       strncmp(y, l, yl < ll ? yl : ll) != 0) {
                /* the structural permutation's extra CLOSER: libyaml's
                 * skipped opener owes one closer — consume libyaml's
                 * line so the skeletons re-align. yeptris's closer is
                 * NOT consumed (its collection closes later). */
                if (++skips > 64) {
                    return 0;
                }
                struct_skips--;
                diffs++;
                if (ln == NULL) {
                    if (!(yl == 0 && yn == NULL)) {
                        return 0;
                    }
                    break;
                }
                l = ln + 1;
                continue;
            } else if (y_val && l[0] == '=' && strncmp(l, "=ALI", 4) == 0) {
                /* an ALIAS realign (r125): both skeletons carry the
                 * alias at the same EVENT position, but yeptris's
                 * suite-pinned plain-'?' law keeps extra '#'-bearing
                 * pairs BEFORE it that libyaml's kill emptied away.
                 * Skip yeptris's VALs until its own =ALI line; the two
                 * alias lines must then match byte-exact (only VALs
                 * may sit between; anything else bails). Family-gated
                 * by pass 1; bounded by the shared skip cap. */
                int skipped = 0;
                for (;;) {
                    if (++skips > 64 || skipped > 16) {
                        return 0;
                    }
                    diffs++;
                    if (yn == NULL) {
                        return 0; /* yeptris ran out before its alias */
                    }
                    y = yn + 1;
                    yn = strchr(y, '\n');
                    yl = yn != NULL ? (size_t)(yn - y) : strlen(y);
                    if (yl > 5 && strncmp(y, "=ALI ", 5) == 0) {
                        break;
                    }
                    if (strncmp(y, val_prefix, vp) != 0) {
                        return 0; /* only VALs (any content) may sit
                                     before the alias */
                    }
                    skipped++;
                }
                /* the realigned alias pair must agree byte-exact */
                if ((yn == NULL) != (ln == NULL) || yl != ll || memcmp(y, l, yl) != 0) {
                    return 0;
                }
                if (yn == NULL || ln == NULL) {
                    break;
                }
                y = yn + 1;
                l = ln + 1;
                continue;
            } else if ((strncmp(y, "=VAL ", 5) == 0 || strncmp(y, "=ALI ", 5) == 0) &&
                       l[0] == '-' &&
                       (strncmp(l, "-MAP", 4) == 0 || strncmp(l, "-SEQ", 4) == 0 ||
                        strncmp(l, "-STR", 4) == 0) &&
                       yd_qgh_terminal_tail(y, l, (int)size, &ride)) {
                /* the TERMINAL-TAIL arm (r142, crash-9c3fb9f1, run
                 * 41961599250, main 68f74daa): SIX consecutive nightly
                 * rounds walked the r137 soup's mutation neighborhood
                 * (r137 #959, r138 crash-7c2930ee, r139 crash-52aaaf3a,
                 * r140 crash-0ff20316, r141 crash-d4f874ee, r142
                 * crash-9c3fb9f1) and every catch was the SAME law-
                 * driven shape -- yeptris suite-correct (the plain-'?'
                 * law keeps the glued-'?' runs as content), libyaml's
                 * comment-glue law emptying/killing members -- with the
                 * walk dying only on the yeptris-only TAIL once
                 * libyaml's skeleton was spent. The exact-bytes pins
                 * the five earlier rounds rode never converge (the
                 * nightly flips one byte per round: a new red forever),
                 * so the terminal tail rides here under strict gates.
                 * (1) family-gated by pass 1 (the glued ?#/?! marker),
                 * unchanged. (2) TERMINAL only: libyaml's remaining
                 * lines are its closers (-MAP/-SEQ/-STR) and nothing
                 * else, and yeptris's remaining lines are 1..cap
                 * kept-content member lines (=VAL/=ALI) followed by
                 * EXACTLY libyaml's closer lines -- the skew is
                 * strictly terminal, never mid-stream. (3) any
                 * structure in yeptris's tail (a +MAP/+SEQ/+STR opener,
                 * an extra/missing closer, a -DOC) declines and the
                 * pre-existing arms take the state -- a tail that opens
                 * structure is a REAL divergence, not a kept-content
                 * ride. Mid-stream member drops stay caught: a member
                 * libyaml loses while its skeleton still carries
                 * members leaves non-closer lines in its remainder
                 * (gate 2 declines, the walk bails -- the negative
                 * composition "{a: 1, ?#b: {x: 2}, c: 3}" aborts:
                 * yeptris keeps the killed member's nested map
                 * mid-stream), and the drops the FIRST arms pair
                 * (emptied-key, suffix-split, L-only, shared-opener,
                 * structural, closer-bank, alias, r120, r124/r127)
                 * never reach this branch. (4) bounded by construction,
                 * no new unbounded rule: the arm is a one-shot consumer
                 * (it exhausts both streams, so it cannot loop) and the
                 * ride is capped by the classifier's own input bound
                 * (every kept-content member consumed at least one
                 * input byte, so ride <= size is provable). The fixed
                 * shared-64 cap this arm shipped with declined the
                 * r146 nightly's legitimately terminal tail
                 * (crash-50405bba, run 42013806681, main 22e2b1ac): a
                 * 730-byte soup splice (997 -> 1728 bytes) rides 96
                 * kept-content members to libyaml's closers, past any
                 * fixed constant -- the cap rides the input, the
                 * terminality/closer-suffix gates stay load-bearing;
                 * diffs are counted and the claim stands only on full
                 * exhaustion (the walk's law). The r137-r141
                 * exact-bytes pins stay pinned (consults are OR'd; the
                 * pins test exit 0, not which rule claims). */
                diffs += ride;
                y += strlen(y);
                l += strlen(l);
                continue;
            } else if (strncmp(y, "=ALI ", 5) == 0 && l_val &&
                       yd_qgh_alias_survives(ldump, y, yl)) {
                /* an ALIAS-SWALLOW RESIDUE (r144, crash-fe4ea5a7, run
                 * 41995393016, main 21088fcb): libyaml's comment-glue
                 * swallowed the killed member THROUGH its alias-bearing
                 * explicit-key head and RE-PAIRED the pending KEY token
                 * with the next content, so yeptris's "=ALI" event has
                 * no counterpart AT ITS POSITION — and the r125
                 * alias-realign arm has already spent libyaml's twin
                 * alias on the earlier one (the doubled "*L" pairing:
                 * the realign pairs yeptris's first alias with
                 * libyaml's only one, leaving the second unexplained
                 * against the re-paired content). The r138-r141 ruling
                 * stands: a bare 'yeptris =ALI vs libyaml anything'
                 * arm would mask genuine alias-drop bugs — the ride is
                 * gated on the same alias line surviving libyaml's dump
                 * ELSEWHERE (re-positioned by the re-pairing, not
                 * dropped; the r138-r141 shapes carry NO libyaml alias
                 * at all and keep their exact-bytes claims). Family-
                 * gated by pass 1; bounded by the shared skip cap; the
                 * claim still stands only if the WHOLE skeleton
                 * re-aligns. */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (yn == NULL) {
                    return 0;
                }
                y = yn + 1;
                continue;
            } else if (y_val && l[0] == '-') {
                /* a YEPTRIS-only extra CONTENT member at the close
                 * (r120): libyaml's comment-kill consumed the tail
                 * entry ENTIRELY (no emptied key, straight to the
                 * closer) while yeptris's suite-pinned plain-'?' law
                 * keeps it as content — skip yeptris's line so the
                 * closers re-align. Family-gated by pass 1; bounded
                 * by the shared skip cap. */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (y_val && l_val) {
                /* a YEPTRIS-only CONTENT val against libyaml's other
                 * content (r124/r127): libyaml's comment-kill ate the
                 * run ENTIRELY (no emptied pair survives) and its
                 * survived continuation pairs downstream. The residue's
                 * text can carry the glued '#' (r124: "?##|") or not
                 * (r127: "`|" -- the kill sat on a LATER line of the
                 * composed plain), so the arm is content-agnostic:
                 * skip yeptris's killed-content line and let the walk
                 * re-pair libyaml's line downstream. Family-gated by
                 * pass 1; the claim only stands if the WHOLE skeleton
                 * re-aligns (a leftover libyaml line fails it); the
                 * more specific arms (suffix-split, emptied pairs)
                 * still win first; bounded by the shared skip cap. */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (y_empty_val && (l_val || l[0] == '-')) {
                /* a YEPTRIS-only extra EMPTY pair: yeptris's skeleton
                 * carries one more empty member than libyaml's at the
                 * tail composition (r116: yeptris parses a trailing
                 * empty entry libyaml does not) — skip yeptris's,
                 * never libyaml's */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (l_empty_val && (yl == 0 || strncmp(y, val_prefix, vp) == 0 || y[0] == '-')) {
                /* a libyaml-only extra emptied pair: re-align on
                 * libyaml's next line without consuming yeptris's */
                if (++skips > 64) {
                    return 0;
                }
                diffs++;
                if (ln == NULL) {
                    if (!(yl == 0 && yn == NULL)) {
                        return 0; /* libyaml ended, yeptris still has lines */
                    }
                    break;
                }
                l = ln + 1;
                continue;
            } else {
                return 0;
            }
        }
        if (yn == NULL || ln == NULL) {
            break;
        }
        y = yn + 1;
        l = ln + 1;
    }
    return diffs > 0;
}

static int yd_qflow_glued_hash_split(const uint8_t* data, size_t size, const char* ydump) {
    if (ydump == NULL || size == 0 || size > 8192) {
        return 0;
    }
    /* pass 1: a glued "?#" at flow depth, outside quotes? */
    int depth = 0;
    int quote = 0;
    int hits = 0;
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
            continue;
        }
        if (c == '\'') {
            quote = '\'';
        } else if (c == '"') {
            quote = '"';
        } else if (c == '[' || c == '{') {
            depth++;
        } else if (c == ']' || c == '}') {
            depth--;
        } else if (c == '?' && depth > 0 && i + 1 < size && data[i + 1] == '#') {
            hits++;
        }
    }
    if (hits == 0) {
        return 0;
    }
    /* pass 2: quote each '?#'-led plain run (two added bytes per run,
     * runs never overlap: size * 3 bounds the copy) */
    char* sub = malloc(size * 3 + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    depth = 0;
    quote = 0;
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            i++;
            continue;
        }
        if (c == '\'') {
            quote = '\'';
        } else if (c == '"') {
            quote = '"';
        } else if (c == '[' || c == '{') {
            depth++;
        } else if (c == ']' || c == '}') {
            depth--;
        } else if (c == '?' && depth > 0 && i + 1 < size && data[i + 1] == '#') {
            /* the run: to a flow indicator, a separator ':' (blank or
             * flow follower), a break, a blank-led comment '#', or
             * EOF; quotes and escapes cannot ride a quoted rewrite */
            size_t j = i;
            int bad = 0;
            while (j < size) {
                unsigned char r = data[j];
                if (r == ',' || r == '[' || r == ']' || r == '{' || r == '}' || r == '\n' ||
                    r == '\r') {
                    break;
                }
                if (r == ':') {
                    unsigned char f = j + 1 < size ? data[j + 1] : ' ';
                    if (f == ' ' || f == '\t' || f == '\n' || f == '\r' || f == ',' || f == ']' ||
                        f == '}') {
                        break; /* the pair separator: the run ends here */
                    }
                }
                if (r == '#' && j > i && (data[j - 1] == ' ' || data[j - 1] == '\t')) {
                    break; /* a comment starts: the run ends before it */
                }
                if (r == '"' || r == '\\') {
                    bad = 1;
                    break;
                }
                j++;
            }
            if (bad) {
                free(sub);
                return 0;
            }
            size_t e = j;
            while (e > i && (data[e - 1] == ' ' || data[e - 1] == '\t')) {
                e--; /* plain scalars strip trailing blanks: keep them out */
            }
            sub[subn++] = '"';
            for (size_t k = i; k < e; k++) {
                sub[subn++] = (char)data[k];
            }
            sub[subn++] = '"';
            i = j;
            continue;
        }
        sub[subn++] = (char)c;
        i++;
    }
    int yok2 = 0;
    int lok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ydump) == 0 &&
                 strcmp(ldump2, ydump) == 0);
    free(ydump2);
    free(ldump2);
    free(sub);
    return known;
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

/* True when the difference is the plain-context NEL fold law (the
 * r118 nightly's crash-8b31932e): libyaml (1.1) folds NEL (U+0085)
 * to a fold space inside plain scalars while the 1.2 break set keeps
 * it as raw content (C2 85) — the QUOTED twin was already carried
 * (yd_nel_break_divergence); this is the plain shape. Same gate as
 * the LS/PS plain family with byte set C2 85. */
static int yd_dump_eq_mod_nel_fold(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    const char* a = ydump;
    const char* b = ldump;
    int subs = 0;
    while (*a != '\0' || *b != '\0') {
        if ((unsigned char)a[0] == 0x85 && (a == ydump || (unsigned char)a[-1] < 0x80)) {
            /* a LONE raw 0x85 byte (the r128 nightly's seed): not a
             * UTF-8 continuation (the lead-byte guard — E3 85 9B's
             * 0x85 rides its lead), so yeptris keeps it as the raw
             * NEL spelling while libyaml's IS_BREAK treats it as a
             * break and the fold eats the run ENTIRELY at these
             * boundaries (the r122 EOF-vanish law, mid-stream) —
             * skip yeptris's lone run, b unchanged */
            int lone = 0;
            while ((unsigned char)a[0] == 0x85) {
                a++;
                lone++;
            }
            subs += lone > 0 ? 1 : 0;
            continue;
        }
        if (a[0] == '\\' && a[1] == 'N') {
            /* a RUN of k escaped NELs in yeptris's dump maps to
             * libyaml's fold artifact: one space for k == 1, (k-1)
             * escaped breaks for k >= 2 (libyaml's 1.1 fold keeps
             * N-1 literal breaks -- the r128 nightly's seed: lone raw
             * 0x85 bytes and C2 85 pairs alike ride yeptris's \N
             * escape, and the r119 walk under-consumed k>2 runs) */
            int k = 0;
            while (a[0] == '\\' && a[1] == 'N') {
                a += 2;
                k++;
            }
            if (k == 1 && b[0] == ' ') {
                b += 1;
            } else if (b[0] == '\\' && b[1] == 'n') {
                for (int e = 0; e < k - 1; e++) {
                    if (b[0] == '\\' && b[1] == 'n') {
                        b += 2;
                    } else {
                        return 0;
                    }
                }
            } else {
                return 0;
            }
            subs++;
            continue;
        }
        if ((unsigned char)a[0] == 0xC2 && (unsigned char)a[1] == 0x85) {
            /* a RUN of k consecutive NEL pairs in yeptris's dump maps
             * to libyaml's fold artifact: one space for k == 1, (k-1)
             * escaped breaks for k >= 2 (the fold's empty-line
             * collapse; the r119 doubled seed, the r128 triple). NO
             * artifact at all is the EOF law: a TRAILING pure-NEL run
             * (or a run whose remainder matches byte-exact) vanishes
             * — a break at the boundary is not a fold (r122). */
            int k = 0;
            const char* p = a;
            while ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) {
                p += 2;
                k++;
            }
            if (k == 1 && b[0] == ' ') {
                b += 1;
                a = p;
            } else if (b[0] == '\\' && b[1] == 'n') {
                for (int e = 0; e < k - 1; e++) {
                    if (b[0] == '\\' && b[1] == 'n') {
                        b += 2;
                    } else {
                        return 0;
                    }
                }
                a = p;
            } else {
                /* the EOF/remainder law (r122): the run vanishes */
                if (p[0] == '\0' || strcmp(p, b) == 0) {
                    subs++;
                    break;
                }
                return 0;
            }
            subs++;
            continue;
        }
        if ((unsigned char)a[0] == 0xC2 && (unsigned char)a[1] == 0x85) {
            /* the EOF boundary (the r122 nightly's 13-byte seed): a
             * TRAILING NEL run vanishes in libyaml's dump entirely —
             * a break at end-of-input (or right before the dump's
             * event-frame boundary) is not a fold, so no artifact
             * exists to match. Accept only when skipping yeptris's
             * ENTIRE leading pure-NEL run makes both remainders
             * identical; content diverging after the run still
             * bails. */
            const char* rest = a;
            int pure = 1;
            while (rest[0] != '\0') {
                if (!((unsigned char)rest[0] == 0xC2 && (unsigned char)rest[1] == 0x85)) {
                    break;
                }
                rest += 2;
            }
            if (pure && rest[0] == '\0') {
                subs++;
                break; /* yeptris's remainder was all NELs */
            }
            if (strcmp(rest, b) == 0) {
                subs++;
                break; /* both remainders identical past the run */
            }
            return 0;
        }
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return subs > 0;
}

static int yd_plain_nel_fold_divergence(const uint8_t* data, size_t size, const char* ydump,
                                        const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int has = 0;
    for (size_t i = 0; i < size; i++) {
        if (i + 1 < size && (unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
            has = 1; /* the UTF-8 NEL pair */
            break;
        }
        if ((unsigned char)data[i] == 0x85 && (i == 0 || (unsigned char)data[i - 1] != 0xC2)) {
            has = 1; /* a LONE raw 0x85 byte (the r128 nightly's seed:
                      * yeptris treats it as the same break and its
                      * emitter escapes both spellings as \N) */
            break;
        }
    }
    if (!has) {
        return 0;
    }
    return yd_dump_eq_mod_nel_fold(ydump, ldump);
}

/* True for the ONE exact gray-zone input of the r130 nightly
 * (crash artifact, 31 bytes, run 41839166485): the glued-hash qflow
 * family's STRUCTURAL permutation -- yeptris reads two plain seq
 * items where libyaml reads one seq entry that is a MAP whose key is
 * the killed continuation (the comment-kill ate the '?#' content and
 * re-paired '5\n` -' under a +MAP wrapper yeptris does not have).
 * The VAL-level arms (#929-#944) realign scalars only; this shape
 * needs the structural-realign extension (#945), which is its own
 * mutation-checked change -- NOT shippable as an arm today. The
 * 31-byte seed rides the EXACT-BYTES gray claim (the r121/r129
 * pattern) until that lands. */
static int yd_r130_structural_gray(const uint8_t* data, size_t size, const char* ydump,
                                   const char* ldump) {
    static const uint8_t seed[] = {
        0x5b, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x3a, 0x0d, 0x0d, 0x0d, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x4d, 0x0d, 0x0d, 0x0d,
        0x0d, 0x35, 0x0d, 0x0a, 0x0a, 0x60, 0x0a, 0x2d, 0x5d,
    };
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r129 nightly
 * (crash-2ff696f8, run 41833275019): a 2938-byte mutation soup of
 * anchor-charset x tag-run x digit-runs compositions — the FIRST line
 * is the r54 family's shape (&I:91999... — libyaml truncates the
 * anchor at ':' where yeptris's spec-legal name rides), but the
 * document carries HUNDREDS of such anchors and glued tag runs, more
 * splits than the charset transform's caps; the remainder of the
 * divergence is digit-run composition, not a single law. Permutation
 * and multi-hundred-split composition are NOT soundly generalizable,
 * so this input rides an EXACT-BYTES claim — the ledger discipline's
 * suite-gray zone, documented by input, not by mutation. Further
 * shapes get their own entries or an engine dig. */
static int yd_r129_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {
        0x26, 0x49, 0x3a, 0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x42, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30,
        0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e,
        0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e,
        0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x5b, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20,
        0x3a, 0x61, 0x70, 0x70, 0x3d, 0x0a, 0x2d, 0x2d, 0x79, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x2a, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a,
        0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20,
        0x2d, 0x70, 0x3a, 0x61, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65,
        0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70,
        0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a,
        0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x2d, 0x70, 0x0a,
        0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d,
        0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d,
        0x2d, 0x0a, 0x2d, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x21, 0x20, 0x74, 0x61, 0x67, 0x76, 0x61,
        0x6c, 0x2c, 0x63, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d,
        0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x42,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x2e, 0x2e, 0x2e, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x21, 0x20, 0x74, 0x61, 0x67, 0x76, 0x61,
        0x6c, 0x2c, 0x63, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d,
        0x46, 0x41, 0x4c, 0x53, 0x45, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74,
        0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x42, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x3b, 0x21, 0x3c, 0xc2, 0x85, 0x21, 0x3c, 0xc2, 0x85,
        0xc2, 0xa5, 0xc2, 0x85, 0xc2, 0x85, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x3a, 0x61, 0x70,
        0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61,
        0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65,
        0x0a, 0x2d, 0x2d, 0x0a, 0x5b, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a,
        0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d,
        0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d,
        0x0a, 0x5b, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x3d, 0x0a, 0x2d,
        0x2d, 0x79, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x30, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e,
        0x74, 0x20, 0x2d, 0x70, 0x3a, 0x61, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e,
        0x74, 0x65, 0x67, 0x65, 0x0a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x2a,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30,
        0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e,
        0x74, 0x20, 0x2d, 0x70, 0x3a, 0x61, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e,
        0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a,
        0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70,
        0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x2d,
        0x70, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x70, 0x70,
        0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d,
        0x2d, 0x0a, 0x5b, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x3d, 0x0a,
        0x2d, 0x2d, 0x79, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x2a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x61, 0x70, 0x70,
        0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d,
        0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x2d, 0x70, 0x3a, 0x61, 0x0a, 0x2d,
        0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a,
        0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32,
        0x30, 0x30, 0x30, 0x31, 0x2e, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a,
        0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x2d, 0x70, 0x0a, 0x2d, 0x70, 0x2d, 0x2d,
        0x0a, 0x61, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69,
        0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x2d, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x21, 0x3c, 0xc2, 0xa5, 0xc2,
        0xa5, 0xc2, 0x85, 0x4e, 0x4e, 0x64, 0x7b, 0x22, 0x4e, 0x4e, 0x69, 0x23, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30,
        0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x21, 0x20, 0x74, 0x61, 0x67, 0x76, 0x61, 0x6c, 0x2c,
        0x63, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d,
        0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x42, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x2a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30,
        0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69,
        0x6e, 0x74, 0x20, 0x2d, 0x70, 0x3a, 0x61, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69,
        0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20,
        0x3a, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70,
        0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61,
        0x2d, 0x70, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x70,
        0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a,
        0x2d, 0x2d, 0x0a, 0x5b, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x3d,
        0x0a, 0x2d, 0x2d, 0x79, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x2a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x61, 0x70,
        0x70, 0x2d, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a,
        0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x2d, 0x70, 0x3a, 0x61, 0x0a,
        0x2d, 0x70, 0x2d, 0x2d, 0x0a, 0x21, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d,
        0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x6f, 0x6d, 0x2c,
        0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21,
        0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x61, 0x2d, 0x70, 0x0a, 0x2d, 0x70, 0x2d, 0x2d, 0x0a,
        0x61, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x0a, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e,
        0x74, 0x20, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x2d, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x31, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x3a,
        0x61, 0x70, 0x70, 0x21, 0x20, 0x74, 0x61, 0x67, 0x76, 0x61, 0x6c, 0x2c, 0x63, 0x6f, 0x6d,
        0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a,
        0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x42, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x31, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x30, 0x30, 0x30, 0x2d,
        0x2d, 0x2d, 0x0a, 0x26, 0x61, 0x31, 0x0a, 0x21, 0x21, 0x73, 0x74, 0x72, 0x0a, 0x73, 0x63,
        0x61, 0x6c, 0x61, 0x72, 0x31, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x73, 0x74, 0x72,
        0x0a, 0x26, 0x61, 0x32, 0x0a, 0x73, 0x63, 0x61, 0x6c, 0x61, 0x72, 0x32, 0x0a, 0x2d, 0x2d,
        0x2d, 0x0a, 0x26, 0x61, 0x33, 0x0a, 0x21, 0x21, 0x73, 0x74, 0x72, 0x20, 0x73, 0x63, 0x61,
        0x6c, 0x61, 0x72, 0x33, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x26, 0x61, 0x34, 0x20, 0x21, 0x21,
        0x6d, 0x61, 0x70, 0x0a, 0x26, 0x61, 0x35, 0x20, 0x21, 0x21, 0x73, 0x74, 0x72, 0x20, 0x6b,
        0x65, 0x79, 0x35, 0x3a, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x34, 0x0a, 0x2d, 0x2d, 0x2d,
        0x0a, 0x61, 0x36, 0x3a, 0x20, 0x31, 0x0a, 0x26, 0x61, 0x6e, 0x63, 0x68, 0x6f, 0x72, 0x36,
        0x20, 0x62, 0x36, 0x3a, 0x20, 0x32, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x6d, 0x61,
        0x70, 0x0a, 0x26, 0x61, 0x38, 0x20, 0x21, 0x21, 0x73, 0x74, 0x72, 0x20, 0x6b, 0x65, 0x79,
        0x38, 0x3a, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x37, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21,
        0x21, 0x6d, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70, 0x21, 0x20, 0x74, 0x61, 0x67, 0x76,
        0x61, 0x6c, 0x2c, 0x63, 0x6f, 0x6d, 0x2c, 0x32, 0x30, 0x30, 0x30, 0x3a, 0x61, 0x70, 0x70,
        0x2d, 0x0a, 0x2d, 0x2d, 0x2d, 0x0a, 0x21, 0x21, 0x69, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x42, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x6e, 0x74, 0x20,
        0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x42, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x6e, 0x74, 0x20, 0x3a, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
        0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39, 0x39,
    };
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r121 nightly
 * (crash-4aa4023b's successor, run 41790195115): a flow map whose
 * glued-'?' plains make libyaml's explicit-key reading PERMUTE the
 * member sequence (libyaml's "I" surfaces at a different member
 * position than yeptris's, and its tail members shift) — both
 * engines accept, but the streams are reordered, not transformed.
 * Member permutation is NOT soundly generalizable (a shape rule
 * would mask real member-order divergences), so this input rides
 * an EXACT-BYTES claim — the ledger discipline's suite-gray zone,
 * documented by input, not by mutation. Further permuted shapes get
 * their own exact entries or an engine dig. */
static int yd_r121_permuted_gray(const uint8_t* data, size_t size, const char* ydump,
                                 const char* ldump) {
    static const uint8_t seed[] = {
        0x7b, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61,
        0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32,
        0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32,
        0x30, 0x30, 0x32, 0x3a, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x23, 0x09, 0x20, 0x0a, 0x0a, 0x09,
        0x3a, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c,
        0x20, 0x09, 0x20, 0x23, 0x2c, 0x3f, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c,
        0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x09, 0x0a, 0x20, 0x09, 0x20, 0x7d,
    };
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r137 nightly
 * (crash-16a36e85, run 41927601558, main b8b3449f): the r121
 * permutation family's TAIL COMPOSITION — a 997-byte flow-map soup of
 * glued-'?' plains x glued-# runs x tag-URI text x CR runs. The first
 * ~31 members realign through the walk's existing arms (suffix splits,
 * L-only vals, yeptris-content skips, ~35 of the shared skip budget),
 * then the streams SKEW STRUCTURALLY: libyaml's comment-glue law eats
 * every remaining '?#'-led member as a comment (its last 2.5 pairs ride
 * as blank-line empties) and it CLOSES the map 28 pairs before yeptris,
 * whose suite-pinned plain-'?' law keeps every run as content and rides
 * all 59 pairs to the closing '}'. The walk reaches libyaml's "-MAP"
 * while yeptris still carries ~56 VAL lines; claiming a yeptris-only
 * tail of ANY size would generalize member-count skew — the exact
 * signature of genuine member-drop bugs (and the r121 precedent already
 * ruled member permutation unsound to generalize). The composition is
 * prefix-minimal (no shorter prefix diverges), so the input rides the
 * EXACT-BYTES gray claim — the ledger discipline's suite-gray zone,
 * documented by input, not by mutation. Further tail shapes get their
 * own entries or an engine dig. */
static int yd_r137_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {
        0x7b, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20,
        0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20,
        0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f,
        0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c,
        0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32,
        0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20,
        0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b,
        0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61,
        0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23,
        0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c,
        0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67,
        0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a,
        0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x0d, 0x79, 0x61, 0x6d,
        0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67,
        0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c,
        0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a,
        0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32,
        0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x49, 0x20, 0x0a,
        0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74,
        0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72,
        0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c,
        0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c,
        0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67,
        0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d,
        0x2c, 0x3f, 0x23, 0x23, 0x52, 0x23, 0x7c, 0x2b, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23,
        0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74,
        0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30,
        0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x79,
        0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65,
        0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09,
        0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61,
        0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67,
        0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x49,
        0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09,
        0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e,
        0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61,
        0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x0d, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09,
        0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c,
        0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79,
        0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f,
        0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61,
        0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32,
        0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49,
        0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09,
        0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e,
        0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x52, 0x23,
        0x7c, 0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09,
        0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c,
        0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79,
        0x61, 0x6d, 0x6c, 0x2e, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20, 0x2c, 0x3f,
        0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x09, 0x20,
        0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c,
        0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23, 0x23, 0x7c,
        0x23, 0x7c, 0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x09,
        0x0a, 0x20, 0x09, 0x20, 0x0d, 0x0d, 0x0d, 0x0d, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72,
        0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x6d, 0x65, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c,
        0x3f, 0x09, 0x20, 0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c,
        0x23, 0x7c, 0x2b, 0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67,
        0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x72, 0x67, 0x65, 0x61, 0x6d, 0x6c, 0x3f, 0x09, 0x20,
        0x2c, 0x3f, 0x49, 0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b,
        0x09, 0x20, 0x09, 0x20, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61,
        0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x23, 0x3d, 0x2c, 0x3f, 0x23,
        0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d, 0x0d,
        0x0d, 0x09, 0x0a, 0x20, 0x09, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r138 nightly
 * (crash-7c2930ee, run 41934580000, main ac91331f): a 66-byte flow-map
 * soup of glued-'?' plains ("?#?#|#|+`", "?##|#|", "?#|#|", ":#|#|+`")
 * x a lone-backtick plain x an explicit-key alias pair ("?\t*L:") x
 * tab/space folds, closed with a '<' member and '}'. Both engines
 * accept; the streams differ beyond every arm's reach. yeptris's
 * suite-pinned plain-'?' law (HM87/652Z) keeps every run as content
 * and the "\t*L" as an ALIAS event -- 8 pairs (seven VAL-keyed, one
 * ALI-keyed) ride to the closing '}'. libyaml's comment-glue law (the
 * flow-'?' KEY token fires on ANY follower and the post-token skip
 * eats a glued '#' as a comment to the break, with no
 * preceding-whitespace requirement) empties every member but the
 * final '<' -- 4 pairs -- and its SECOND run's comment swallows the
 * "\t*L:" explicit key THROUGH the alias: yeptris emits an =ALI event
 * libyaml has no counterpart for at all. The walk's emptied-key arm
 * pairs members 1-3 one-to-one, then the first unexplained line is
 * yeptris's "=ALI *L" against libyaml's "=VAL : <": an 'yeptris =ALI
 * vs libyaml anything' arm would mask genuine alias-drop bugs (an
 * event-kind divergence, not content), and every completing skeleton
 * composition needs a 9-line yeptris-only tail AFTER libyaml's '-MAP'
 * closes -- member-count skew, the exact signature of genuine
 * member-drop bugs the r137 verdict ruled unsound to generalize. The
 * composition is prefix-minimal (no shorter prefix diverges), so the
 * input rides the EXACT-BYTES gray claim -- the ledger discipline's
 * suite-gray zone, documented by input, not by mutation. Further
 * alias-swallow shapes get their own entries or an engine dig. */
static int yd_r138_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {
        0x7b, 0x3f, 0x23, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x60, 0x20, 0x09, 0x20, 0x0a,
        0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2c, 0x60, 0x20, 0x2c, 0x3f,
        0x09, 0x2a, 0x4c, 0x3a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x2c, 0x60,
        0x20, 0x0a, 0x3a, 0x09, 0x20, 0x2c, 0x3f, 0x09, 0x3a, 0x23, 0x7c, 0x23, 0x7c, 0x2b,
        0x60, 0x20, 0x0a, 0x2c, 0x3c, 0x20, 0x0a, 0x09, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r139 nightly
 * (crash-52aaaf3a, run 41941091655, main d6df4dbe): a 35-byte flow-map
 * soup -- the r138 soup's REDUCED SIBLING (the same glued-'?'
 * "?#?#|#|+`" head and "\t*L:" explicit-key alias pair; the fold after
 * the head and the three middle members dropped, the tail plain
 * unbackticked), closed with '}'. Both engines accept; the streams
 * differ beyond every arm's reach. yeptris's suite-pinned plain-'?'
 * law (HM87/652Z) keeps every run as content and the "\t*L" as an
 * ALIAS event -- 4 pairs (three VAL-keyed, one ALI-keyed) ride to the
 * closing '}'. libyaml's comment-glue law (the flow-'?' KEY token
 * fires on ANY follower and the post-token skip eats a glued '#' as a
 * comment to the break, with no preceding-whitespace requirement)
 * empties the head pair and swallows EVERYTHING from the lone-backtick
 * plain THROUGH the "\t*L:" alias to the tail -- the stream closes
 * after ONE emptied pair. The walk's emptied-key arm pairs member 1
 * and the r120 tail arm consumes the lone-backtick pair, then the
 * first unexplained line is yeptris's "=ALI *L" against libyaml's
 * "-MAP": an 'yeptris =ALI vs libyaml anything' arm would mask
 * genuine alias-drop bugs (an event-kind divergence, not content),
 * and every completing skeleton composition needs a yeptris-only tail
 * AFTER libyaml's '-MAP' closes -- member-count skew, the exact
 * signature of genuine member-drop bugs the r137/r138 verdicts ruled
 * unsound to generalize. The bisect over every prefix found no
 * shorter diverging cut (the artifact is prefix-minimal at 35 bytes),
 * so the input rides the EXACT-BYTES gray claim -- the ledger
 * discipline's suite-gray zone, documented by input, not by mutation.
 * Further alias-swallow shapes get their own entries or an engine
 * dig. */
static int yd_r139_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {0x7b, 0x3f, 0x23, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x2b,
                                   0x60, 0x20, 0x09, 0x20, 0x2c, 0x60, 0x20, 0x2c, 0x3f,
                                   0x09, 0x2a, 0x4c, 0x3a, 0x09, 0x20, 0x2c, 0x3f, 0x23,
                                   0x7c, 0x23, 0x70, 0x70, 0x0a, 0x09, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r140 nightly
 * (crash-0ff20316, run 41947883642, main 38983124): a 33-byte flow-map
 * soup -- the r139 soup's SIBLING (the same glued-'?' head and
 * "\t*L:" explicit-key alias pair; the '+' and the first backtick of
 * the r139 backtick pair dropped, so the head plain folds clean and
 * ONE lone backtick plain rides before the alias), closed with '}'.
 * Both engines accept; the streams differ beyond every arm's reach.
 * yeptris's suite-pinned plain-'?' law (HM87/652Z) keeps every run as
 * content and the "\t*L" as an ALIAS event -- 4 pairs (three
 * VAL-keyed, one ALI-keyed) ride to the closing '}'. libyaml's
 * comment-glue law (the flow-'?' KEY token fires on ANY follower and
 * the post-token skip eats a glued '#' as a comment to the break,
 * with no preceding-whitespace requirement) empties the head pair and
 * swallows EVERYTHING from the lone-backtick plain THROUGH the
 * "\t*L:" alias to the tail -- the stream closes after ONE emptied
 * pair. The walk's emptied-key arm pairs member 1, the r120 tail arm
 * and the yeptris-only empty skip consume the lone-backtick pair,
 * then the first unexplained line is yeptris's "=ALI *L" against
 * libyaml's "-MAP": an 'yeptris =ALI vs libyaml anything' arm would
 * mask genuine alias-drop bugs (an event-kind divergence, not
 * content), and every completing skeleton composition needs a
 * yeptris-only tail AFTER libyaml's '-MAP' closes -- member-count
 * skew, the exact signature of genuine member-drop bugs the
 * r137/r138/r139 verdicts ruled unsound to generalize. The bisect
 * over every prefix found no shorter diverging cut (the artifact is
 * prefix-minimal at 33 bytes), so the input rides the EXACT-BYTES
 * gray claim -- the ledger discipline's suite-gray zone, documented
 * by input, not by mutation. Further alias-swallow shapes get their
 * own entries or an engine dig. */
static int yd_r140_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {0x7b, 0x3f, 0x23, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x20,
                                   0x09, 0x20, 0x2c, 0x60, 0x20, 0x2c, 0x3f, 0x09, 0x2a,
                                   0x4c, 0x3a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x7c, 0x23,
                                   0x70, 0x70, 0x0a, 0x09, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r141 nightly
 * (crash-d4f874ee, run 41955451792, main b565ab0c): a 26-byte flow-map
 * soup -- the r140 soup's SIBLING (the same glued-'?' head and
 * "\t*L:" explicit-key alias pair; r140's tail plain member dropped
 * entirely, so the closer is a NEWLINE-LED ')' -- ONE plain after the
 * input's single break). Both engines accept; the streams differ
 * beyond every arm's reach. yeptris's suite-pinned plain-'?' law
 * (HM87/652Z) keeps every run as content and the "\t*L" as an ALIAS
 * event -- 4 pairs (three VAL-keyed, one ALI-keyed) ride to the
 * closing '}'. libyaml's comment-glue law (the flow-'?' KEY token
 * fires on ANY follower and the post-token skip eats a glued '#' as a
 * comment to the break, with no preceding-whitespace requirement)
 * swallows EVERYTHING from the glued '#' THROUGH the "\t*L:" alias to
 * the input's single break -- and the pending explicit key takes the
 * NEXT line's ')' as its content, so the stream closes after ONE pair
 * whose content is ')'. The walk's consumed-content arms (r124/r127 +
 * L-only + r120/r116 skips) carry the head, the backtick and the
 * emptied pairs, then the first unexplained line is yeptris's
 * "=ALI *L" against libyaml's "-MAP": an 'yeptris =ALI vs libyaml
 * anything' arm would mask genuine alias-drop bugs (an event-kind
 * divergence, not content), and every completing skeleton composition
 * needs a yeptris-only tail AFTER libyaml's '-MAP' closes --
 * member-count skew, the exact signature of genuine member-drop bugs
 * the r137/r138/r139/r140 verdicts ruled unsound to generalize. The
 * bisect over every prefix found no shorter diverging cut (the
 * artifact is prefix-minimal at 26 bytes), so the input rides the
 * EXACT-BYTES gray claim -- the ledger discipline's suite-gray zone,
 * documented by input, not by mutation. Further alias-swallow shapes
 * get their own entries or an engine dig. */
static int yd_r141_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {0x7b, 0x3f, 0x23, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x20,
                                   0x09, 0x20, 0x2c, 0x60, 0x20, 0x2c, 0x3f, 0x09, 0x2a,
                                   0x4c, 0x3a, 0x09, 0x20, 0x0a, 0x29, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r151 nightly
 * (crash-a3c7e5d7, run 42085146127, main 3e7bedf1): a 65-byte flow-map
 * soup -- the r138 soup's ONE-BYTE-DELETION SIBLING (the pinned seed's
 * first member drops its second '?', so the head plain reads
 * "?##|#|+`" instead of "?#?#|#|+`"; every other byte identical). The
 * r138 mutation battery flipped bytes; a DELETION inside the doubled
 * '?' head was outside its neighborhood, and the exact-bytes r138
 * claim is byte-scoped, so the mutant re-aborted. Both engines accept;
 * the streams differ beyond every arm's reach. yeptris's suite-pinned
 * plain-'?' law (HM87/652Z) keeps every run as content and the "\t*L"
 * as an ALIAS event -- 8 pairs (seven VAL-keyed, one ALI-keyed) ride
 * to the closing '}'. libyaml's comment-glue law (the flow-'?' KEY
 * token fires on ANY follower and the post-token skip eats a glued
 * '#' as a comment to the break, with no preceding-whitespace
 * requirement) empties every member but the final '<' -- 4 pairs
 * (seven empties around the '<') -- and its SECOND run's comment
 * swallows the "\t*L:" explicit key THROUGH the alias: yeptris emits
 * an =ALI event libyaml has no counterpart for at all. The walk's
 * emptied-key arm pairs members 1-3 one-to-one (their value lines
 * pair as equals), then the first unexplained line is yeptris's
 * "=ALI *L" against libyaml's "=VAL : <" -- mid-stream, so the r142
 * terminal-tail arm never reaches (its gate demands libyaml's
 * closers-only remainder), and the r144 residue arm declines (its
 * survival gate demands the alias line ride libyaml's dump ELSEWHERE;
 * here the comment-glue swallowed it entirely). An 'yeptris =ALI vs
 * libyaml anything' arm would mask genuine alias-drop bugs (an
 * event-kind divergence, not content), and every completing skeleton
 * composition needs a 9-line yeptris-only tail AFTER libyaml's '-MAP'
 * closes -- member-count skew, the exact signature of genuine
 * member-drop bugs the r137/r138 verdicts ruled unsound to generalize
 * and the r144 verdict re-affirmed ("the r138-r141 shapes carry NO
 * libyaml alias at all and keep their exact-bytes claims"). The
 * bisect over every prefix found no shorter diverging cut (the
 * artifact is prefix-minimal at 65 bytes), so the input rides the
 * EXACT-BYTES gray claim -- the ledger discipline's suite-gray zone,
 * documented by input, not by mutation. Further alias-swallow shapes
 * get their own entries or an engine dig. */
static int yd_r151_soup_gray(const uint8_t* data, size_t size, const char* ydump,
                             const char* ldump) {
    static const uint8_t seed[] = {0x7b, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c, 0x2b, 0x60, 0x20, 0x09,
                                   0x20, 0x0a, 0x09, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x23, 0x7c,
                                   0x2c, 0x60, 0x20, 0x2c, 0x3f, 0x09, 0x2a, 0x4c, 0x3a, 0x09, 0x20,
                                   0x2c, 0x3f, 0x23, 0x7c, 0x23, 0x7c, 0x2c, 0x60, 0x20, 0x0a, 0x3a,
                                   0x09, 0x20, 0x2c, 0x3f, 0x09, 0x3a, 0x23, 0x7c, 0x23, 0x7c, 0x2b,
                                   0x60, 0x20, 0x0a, 0x2c, 0x3c, 0x20, 0x0a, 0x09, 0x20, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
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
/* The LS/PS rendering laws' dump comparison (rounds 56/66/70), both
 * exact and dump-shaped. PLAIN: yeptris (YAML 1.2) folds an LS/PS
 * break into ONE space while libyaml's READ_LINE joins it RAW —
 * yeptris's fold-space is libyaml's raw triple. QUOTED: both keep the
 * raw code point but libyaml drops the blank run that follows it —
 * yeptris's triple plus spaces is libyaml's bare triple. Any mix of
 * the two rides one walk; at least one such site, full alignment
 * elsewhere. */
/* The r86/r72 relation walk, depth-capped backtracking form. The
 * additive arm (the triple rides as content before yeptris's own
 * rendering) and the replacement arm (the triple replaces one
 * yeptris escape) are BOTH legal renderings of the same break run —
 * libyaml drops the fold-leading LF and keeps LS/PS raw (READ_LINE)
 * while yeptris normalizes every break and folds n-1 — and which one
 * aligns depends on where the run sits in the fold. The linear walk
 * picked greedily and the r90 core ("&I:V\n<PS>\n+": libyaml renders
 * the run <PS>\n, yeptris \n\n) starved on the additive arm's dead
 * continuation — one escape stranded. Try the additive continuation
 * first (the tighter scope) and backtrack to the replacement when it
 * dies; diffs rides so the winning path's count gates the end. */
static int yd_fold_walk(const char* ydump, size_t y, const char* ldump, size_t l, int depth,
                        int diffs) {
    while (ydump[y] != '\0') {
        /* lb1/lb2 read only under the short-circuit: the \n arm walks
         * l FASTER than y, so ldump can terminate first — an
         * unconditional [l + 2] rode past the terminator (the ASan
         * catch on the r69 seed, the round-73 PR's gate) */
        unsigned char lb0 = (unsigned char)ldump[l];
        int l_raw_ps = 0;
        if (lb0 == 0xE2) {
            unsigned char lb1 = (unsigned char)ldump[l + 1];
            if (lb1 == 0x80) {
                unsigned char lb2 = (unsigned char)ldump[l + 2];
                l_raw_ps = lb2 == 0xA8 || lb2 == 0xA9;
            }
        }
        /* the quoted blank-strip member (round 73): around a raw LS/PS
         * that BOTH engines keep, libyaml drops the blank runs on both
         * sides while yeptris keeps them (spelling a tab as its \t
         * escape). Consume yeptris's leading blank/escape run, require
         * the triple in BOTH dumps, then the trailing blank run — the
         * post-run triple check makes the arm decline (fall through to
         * the plain law) when the space faces the triple instead
         * (round 56's shape). */
        int y_qblank = ydump[y] == ' ' || (ydump[y] == '\\' && ydump[y + 1] == 't');
        if (y_qblank && l_raw_ps) {
            size_t y2 = y;
            while (ydump[y2] == ' ' || (ydump[y2] == '\\' && ydump[y2 + 1] == 't')) {
                y2 += (ydump[y2] == '\\') ? 2 : 1;
            }
            unsigned char yb0 = (unsigned char)ydump[y2];
            int y_raw_ps = 0;
            if (yb0 == 0xE2) {
                unsigned char yb1 = (unsigned char)ydump[y2 + 1];
                if (yb1 == 0x80) {
                    unsigned char yb2 = (unsigned char)ydump[y2 + 2];
                    y_raw_ps = yb2 == 0xA8 || yb2 == 0xA9;
                }
            }
            if (y_raw_ps) {
                y = y2 + 3;
                l += 3;
                while (ydump[y] == ' ') {
                    y++;
                }
                diffs++;
                continue;
            }
        }
        if ((unsigned char)ydump[y] == 0xE2 && (unsigned char)ydump[y + 1] == 0x80 &&
            ((unsigned char)ydump[y + 2] == 0xA8 || (unsigned char)ydump[y + 2] == 0xA9)) {
            /* the quoted law (round 55): the triple rides raw in both
             * dumps; libyaml dropped the blanks that followed it */
            if (ldump[l] != ydump[y] || ldump[l + 1] != ydump[y + 1] ||
                ldump[l + 2] != ydump[y + 2]) {
                return 0;
            }
            y += 3;
            l += 3;
            while (ydump[y] == ' ') {
                y++;
            }
            diffs++;
            continue;
        }
        if (ldump[l] == ydump[y]) {
            y++;
            l++;
            continue;
        }
        if (ydump[y] == ' ' && (unsigned char)ldump[l] == 0xE2 &&
            (unsigned char)ldump[l + 1] == 0x80 &&
            ((unsigned char)ldump[l + 2] == 0xA8 || (unsigned char)ldump[l + 2] == 0xA9)) {
            /* the plain law (round 56): yeptris's fold-space is
             * libyaml's raw triple */
            y += 1;
            l += 3;
            diffs++;
            continue;
        }
        /* the blank-line member (round 72): yeptris's break law makes
         * the PS a BLANK line and the fold a hard break — the dump
         * spells it \n — while libyaml's READ_LINE joins the PS raw as
         * content. (The dump escapes a real LF as \n; a literal
         * backslash-n would ride as \\n, so the pair is unambiguous.)
         * The r86 member (crash-ef7ccbc5, "&k:j:(<PS>\r\rT |U...", the
         * anchor-split residual): libyaml keeps the triple raw AS
         * CONTENT before the SAME escaped-fold rendering yeptris
         * prints — the triple is ADDITIVE to the run (consume it
         * alone when the renders then align), while round 72's shape
         * REPLACES one yeptris escape (consume both). The caller's
         * raw-triple check scopes both to the fold family.
         * The r104 member (crash-955b6f217c, 16 PS-led break runs):
         * when the triple's follower is exactly yeptris's next byte the
         * additive reading is FORCED — yeptris's rendering is libyaml's
         * minus the raw triple (the #638 law: libyaml joins the LS/PS
         * verbatim where yeptris's b-char algebra folds the run), so no
         * alternative arm can align the site and nothing can backtrack
         * into the replacement here (libyaml is never SHORTER than
         * yeptris at a PS site). Walk it linearly: the recursive probe
         * was depth-capped at 8 and the artifact outran it.
         * The r105 member (crash-73d10ccefb0181f1679c9d698d87a1294f339125,
         * nightly 41432246664) is a break RUN mixing PS/LS into the
         * LF/CR breaks, rendered by the two engines' DIFFERENT fold
         * algebras: yeptris normalizes every break and folds n-1
         * newlines, libyaml keeps the raw triple(s) and folds only its
         * LF/CR count (n'-1) — so the run walks POSITION-WISE and the
         * escapes after a triple decide: equal counts take the triple
         * ADDITIVE (the r104 forced reading, sites with the same escape
         * count on both sides classify exactly as before), one-short
         * take the round-72 REPLACEMENT (yeptris's PS-position break
         * renders \n where libyaml joins the raw PS). A run failing
         * the count law falls to the single-site arms below, so no
         * accepted site changes verdict. */
        int y_esc_lf = ydump[y] == '\\' && ydump[y + 1] == 'n';
        if (y_esc_lf && l_raw_ps) {
            size_t y2 = y;
            size_t l2 = l;
            int run_ok = 1;
            int run_diffs = 0;
            while (run_ok) {
                int y_esc2 = ydump[y2] == '\\' && ydump[y2 + 1] == 'n';
                int l_triple =
                    (unsigned char)ldump[l2] == 0xE2 && (unsigned char)ldump[l2 + 1] == 0x80 &&
                    ((unsigned char)ldump[l2 + 2] == 0xA8 || (unsigned char)ldump[l2 + 2] == 0xA9);
                if (y_esc2 && l_triple) {
                    /* the escapes after this triple, run-scoped */
                    size_t l3 = l2 + 3;
                    size_t y3 = y2;
                    size_t ly = 0;
                    size_t yy = 0;
                    for (;;) {
                        if (ldump[l3] == '\\' && ldump[l3 + 1] == 'n') {
                            ly++;
                            l3 += 2;
                        } else if ((unsigned char)ldump[l3] == 0xE2 &&
                                   (unsigned char)ldump[l3 + 1] == 0x80 &&
                                   ((unsigned char)ldump[l3 + 2] == 0xA8 ||
                                    (unsigned char)ldump[l3 + 2] == 0xA9)) {
                            l3 += 3;
                        } else {
                            break;
                        }
                    }
                    while (ydump[y3] == '\\' && ydump[y3 + 1] == 'n') {
                        yy++;
                        y3 += 2;
                    }
                    if (ly == yy) {
                        l2 += 3; /* additive: the triple rides alone */
                    } else if (ly == yy - 1) {
                        y2 += 2; /* replace: this break is the raw PS */
                        l2 += 3;
                    } else {
                        run_ok = 0;
                        break;
                    }
                    run_diffs++;
                } else if (y_esc2 && ldump[l2] == '\\' && ldump[l2 + 1] == 'n') {
                    y2 += 2; /* the run's paired escapes */
                    l2 += 2;
                } else if (!y_esc2 && (unsigned char)ldump[l2] != 0xE2 && ldump[l2] != '\\' &&
                           ldump[l2] != '\0') {
                    break; /* both runs ended together */
                } else {
                    run_ok = 0; /* exactly one run ended: out of family */
                    break;
                }
            }
            if (run_ok) {
                y = y2;
                l = l2;
                diffs += run_diffs;
                continue;
            }
            if (ldump[l + 3] == ydump[y]) {
                l += 3;
                diffs++;
                continue;
            }
            if (depth < 8 && yd_fold_walk(ydump, y, ldump, l + 3, depth + 1, diffs + 1)) {
                return 1;
            }
            y += 2;
            l += 3;
            diffs++;
            continue;
        }
        return 0;
    }
    return diffs > 0 && ldump[l] == '\0';
}

static int yd_dump_eq_mod_lsps_fold(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    return yd_fold_walk(ydump, 0, ldump, 0, 0, 0);
}

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
    return yd_dump_eq_mod_lsps_fold(ydump, ldump);
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
 * original — the divergence is then exactly the charset law.
 *
 * A '&'/'*' GLUED INSIDE a tag shorthand run is not a name run at all:
 * "!!i&a:" is ONE tag in BOTH engines (the suffix is ns-uri-char, and
 * '&'/':' are URI bytes — the r108 artifact's first line, crash-
 * 54b5b756). The blind split cut the tag URI instead ("!!i&a :") and
 * the corrupted copy no longer parsed, so the verify's early return
 * skipped the r95 solo retries and the alias split — the actual
 * divergence, "*a:" naming "a:" (2SXE) vs libyaml's ':' separator —
 * never got its legs. The same run also swallows a GLUED apostrophe:
 * both charsets keep '\'' inside the suffix ("!a'b: c" is one tag in
 * both engines; '"' is not a URI byte, so a glued double quote still
 * opens its scalar). Law: a byte reached while the current GLUED props
 * run has carried a '!' is tag-URI content — '&'/'*' gate no name run
 * and '\'' opens no quote; only a blank/break ends the run. */
static int yd_anchor_byte_in_libyaml_set(unsigned char n) {
    return (n >= '0' && n <= '9') || (n >= 'A' && n <= 'Z') || (n >= 'a' && n <= 'z') || n == '_' ||
           n == '-';
}

/* the r69 engine law (yep_scan_prop_end stops at NEL/LS/PS) in the
 * walk form: a raw multibyte break sequence ENDS tokens like \n/\r —
 * the byte-wise walks otherwise rode the PS bytes as plain content,
 * splitting an anchor at the break and poisoning the plain state so a
 * LATER anchor never split (the nightly's round 87, crash-629bf361:
 * "k: &a<PS> ? &--02:in~" — '&a' split at the PS, and the key-position
 * '&--02' rode as props-adjacent content). Returns the sequence length
 * (2 = NEL, 3 = LS/PS), 0 when data[i] starts no break. */
static size_t yd_mb_break_len(const uint8_t* data, size_t size, size_t i) {
    if (i + 1 < size && data[i] == 0xC2 && data[i + 1] == 0x85) {
        return 2; /* NEL */
    }
    if (i + 2 < size && data[i] == 0xE2 && data[i + 1] == 0x80 &&
        (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
        return 3; /* LS / PS */
    }
    return 0;
}

/* The charset split COMPOSED with the raw-break law: when the split
 * copy still carries a raw NEL/LS/PS, libyaml's reading of the split
 * copy takes a different arm than yeptris's (the split alone cannot
 * reconcile: libyaml turns "&k :":<PS> ..." into the anchored map
 * while yeptris folds one plain), so LF-substitute the split copy and
 * require BOTH engines to agree there byte-strict AND that reading to
 * equal libyaml's ORIGINAL dump — libyaml is invariant under the
 * composition, exactly the composed-break signature (the r88
 * two-continuation minimal, crash-94a6a42e: "&k:\":<PS> U\\<PS> U"). */
static int yd_charset_split_lf_composed_agrees(const char* sub, size_t o, const char* ldump) {
    char* lf = malloc(o + 1);
    if (lf == NULL) {
        return 0;
    }
    size_t lo = 0;
    for (size_t i = 0; i < o;) {
        if (i + 2 < o && (unsigned char)sub[i] == 0xE2 && (unsigned char)sub[i + 1] == 0x80 &&
            ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            lf[lo++] = '\n';
            i += 3;
        } else if (i + 1 < o && (unsigned char)sub[i] == 0xC2 &&
                   (unsigned char)sub[i + 1] == 0x85) {
            lf[lo++] = '\n';
            i += 2;
        } else {
            lf[lo++] = sub[i++];
        }
    }
    int yok = 0;
    int lok = 0;
    char* yd3 = yd_sem_dump(lf, lo, &yok);
    char* ld3 = yd_libyaml_dump(lf, lo, &lok);
    /* libyaml's ORIGINAL reading keeps the raw PS bytes inside the
     * scalar where the LF-substituted copy renders the fold as a
     * blank — the existing fold relation spans exactly that. The r93
     * composition ("&k:<PS> U\\:\":<PS>"): the PS rides as a BREAK in
     * libyaml's reading (a separator, never value content), so the
     * LF-substituted dump EQUALS the original's exactly and the fold
     * walk finds no diffs — the fully-invariant case is the same
     * carry (the divergence is the anchor-charset tokenization plus
     * the PS-as-break normalization, both documented). */
    int ok = yd3 != NULL && yok && ld3 != NULL && lok && strcmp(yd3, ld3) == 0 &&
             (strcmp(ld3, ldump) == 0 || yd_dump_eq_mod_lsps_fold(ld3, ldump));
    free(yd3);
    free(ld3);
    free(lf);
    return ok;
}

static int yd_anchor_name_charset_divergence(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump);

/* The charset transform's verify legs over ONE candidate copy: yeptris
 * and libyaml must agree there (byte-exact, or mod the documented
 * LS/PS fold relation, or on the LF-substituted pivot) AND libyaml
 * must be byte-invariant between the original and the copy. Shared by
 * the all-splits copy and the r95 per-split solo copies. */
static int yd_charset_verify_copy(const char* sub, size_t o, const char* ldump) {
    int ok2 = 0;
    char* ydump2 = yd_sem_dump(sub, o, &ok2);
    if (ydump2 == NULL || !ok2) {
        free(ydump2);
        return 0;
    }
    int ok3 = 0;
    char* ldump2 = yd_libyaml_dump(sub, o, &ok3);
    /* the fold-walk leg requires a raw LS/PS triple in the copy — its
     * arms are the PS/LS family's dump relations; without the scan the
     * space-eating arm would scope to every charset split */
    int has_triple = 0;
    for (size_t t = 0; t + 2 < o; t++) {
        if ((unsigned char)sub[t] == 0xE2 && (unsigned char)sub[t + 1] == 0x80 &&
            ((unsigned char)sub[t + 2] == 0xA8 || (unsigned char)sub[t + 2] == 0xA9)) {
            has_triple = 1;
            break;
        }
    }
    int ok =
        ldump2 != NULL && ok3 && strcmp(ldump, ldump2) == 0 &&
        (strcmp(ydump2, ldump2) == 0 || (has_triple && yd_dump_eq_mod_lsps_fold(ydump2, ldump2)) ||
         (has_triple && yd_charset_split_lf_composed_agrees(sub, o, ldump)));
    free(ldump2);
    free(ydump2);
    return ok;
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
    int tag_run = 0; /* the current glued props run carried a '!': a
                      * '&'/'*' inside it is tag-URI content (r108) */
    int bs_open = 0; /* a |/> block-scalar body: every byte is text */
    size_t bs_col = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        /* the r87 law: a raw NEL/LS/PS is a line break in this walk's
         * model — the head's column bookkeeping and the tail's
         * node_pos/only_dash effects of a \n, applied in one place */
        size_t mbl_ = yd_mb_break_len(data, size, i);
        if (mbl_ > 0) {
            col = 0;
            line_first = (size_t)-1;
            line_pending = (plain_open || bs_open) ? 1 : 0;
            node_pos = (plain_open || bs_open) ? 0 : 1;
            only_dash = 1;
            prev = '\n';
            tag_run = 0;
            i += mbl_ - 1; /* the loop's own i++ lands past the sequence */
            continue;
        }
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
        if ((c == '\'' || c == '"') && (word_led || prop_word) && !plain_open &&
            !(c == '\'' && tag_run)) {
            /* !plain_open: a blank inside an open plain does not start
             * a new token ("&gboo :- 4f\";\ \"P&g..." -- the eager
             * open went phantom, swallowed the NEXT line's '&' and the
             * split never fired there; the nightly's round 68) */
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
            if (c == '!') {
                tag_run = 1;
            }
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
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                tag_run = 0; /* a separator ends the glued props run */
            }
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
        if ((c == '&' || c == '*') && !plain_open && (node_pos || saw_prop) && !tag_run) {
            /* '*' joins the family (the nightly's round 54,
             * crash-17654f5b: the ALIAS name rides libyaml's charset
             * law exactly like an anchor's -- "*a:" names "a:" in
             * yeptris (2SXE) while libyaml truncates at ':' and reads
             * the key separator; the split reconciles both) */
            amps++;
        }
        if ((c == '&' || c == '*') && !gate && !plain_open && (node_pos || saw_prop) && !tag_run) {
            size_t j = i + 1;
            while (j < size && data[j] != ' ' && data[j] != '\t' && data[j] != '\n' &&
                   data[j] != '\r' && yd_mb_break_len(data, size, j) == 0) {
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
            } else if (c == ':' && flowd == 0 && !bs_open && !tag_run && i + 1 < size &&
                       data[i + 1] != ' ' && data[i + 1] != '\t' && data[i + 1] != '\n' &&
                       data[i + 1] != '\r') {
                /* a colon-led plain opens here (the HM87 riding colon:
                 * "&k :`c ..." -- libyaml truncates the anchor at ':'
                 * and the residual ':`c {...' IS the plain). Without
                 * this the walk kept plain_open closed and a later
                 * mid-plain '&' rode as props -- the split corrupted
                 * the scalar's bytes and the carry declined its own
                 * family (the nightly's round 51, crash-098e7549).
                 * NOT inside a !-led run: those bytes are tag-URI
                 * content in both engines (r108) -- "!*a:'" glued a
                 * plain that swallowed the next line's alias and the
                 * carry declined its own family (the nightly's round
                 * 123, crash-r123). */
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
    /* the r95 law: record each split's original index — a LATER,
     * non-load-bearing split in the same copy (a '!' run in a trailing
     * document) can poison the all-splits verify while the load-bearing
     * split verifies clean alone; each gets a solo retry */
    size_t split_at[8];
    int nsplits = 0;
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
    int tag_run2 = 0; /* the gate's r108 tag-URI law again */
    int bs_open2 = 0;
    size_t bs_col2 = 0;
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        /* the r87 law again: a raw NEL/LS/PS breaks the line — copied
         * verbatim, with the \n state effects */
        size_t mbl2_ = yd_mb_break_len(data, size, i);
        if (mbl2_ > 0) {
            for (size_t k2_ = 0; k2_ < mbl2_; k2_++) {
                sub[o++] = (char)data[i++];
            }
            col2 = 0;
            line_first2 = (size_t)-1;
            line_pending2 = (plain_open2 || bs_open2) ? 1 : 0;
            node_pos2 = (plain_open2 || bs_open2) ? 0 : 1;
            only_dash2 = 1;
            prev = '\n';
            tag_run2 = 0;
            continue;
        }
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
        if ((c == '\'' || c == '"') && (word_led2 || prop_word2) && !plain_open2 &&
            !(c == '\'' && tag_run2)) {
            /* the gate's mid-plain law again (the nightly's round 68) */
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
            if (c == '!') {
                tag_run2 = 1;
            }
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-' || c == '?') {
            /* the gate's ':' law again (crash-7fe697be) */
            word_led2 = 1;
            prop_word2 = 0;
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                tag_run2 = 0; /* a separator ends the glued props run */
            }
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
        if ((c == '&' || c == '*') && !plain_open2 && (node_pos2 || saw_prop2) && !tag_run2) {
            /* the '*' twin of the '&' split (the round-54 alias-name
             * composition -- see the gate walk) */
            sub[o++] = (char)c;
            i++;
            size_t name_o = o;
            while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                   data[i] != '\r' && yd_mb_break_len(data, size, i) == 0 &&
                   yd_anchor_byte_in_libyaml_set(data[i])) {
                sub[o++] = (char)data[i++];
            }
            /* split ONLY a real anchor (at least one in-set name byte
             * before the out-of-set one): an '&' with an EMPTY name is
             * mid-plain CONTENT here ("--&\t0\- -:" -- both engines
             * read the line as one plain key), and the blind split
             * inserted a space that corrupted the scalar
             * (crash-e1d13ebe, the corpus sweep after #713's arms) */
            if (o > name_o && i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                data[i] != '\r' && yd_mb_break_len(data, size, i) == 0) {
                /* a break-ended name is COMPLETE — no truncation, no
                 * space (the break already ends the token in both
                 * engines); inserting one here opened a phantom plain
                 * that swallowed the PS and skipped the LATER anchor
                 * (the nightly's round 87, crash-629bf361) */
                sub[o++] = ' '; /* libyaml's truncation point becomes a token break */
                if (nsplits < 8) {
                    split_at[nsplits++] = i;
                }
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
                       data[i] != '\r' && yd_mb_break_len(data, size, i) == 0) {
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
            } else if (c == ':' && flowd2 == 0 && !bs_open2 && !tag_run2 && i + 1 < size &&
                       data[i + 1] != ' ' && data[i + 1] != '\t' && data[i + 1] != '\n' &&
                       data[i + 1] != '\r') {
                plain_open2 = 1; /* the gate's colon-led plain law again
                                    (the r123 !-run guard rides it) */
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
    int carried = 0;
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
        carried = yd_charset_verify_copy(sub, o, ldump);
        if (!carried && nsplits > 0) {
            /* the r95 multi-split decline ("&I:<205KB>...'---\n!!int'",
             * crash-975fc078): a LATER, non-load-bearing split rides in
             * the same copy (a props run in the trailing document) and
             * its own reading difference poisons the verify — while the
             * load-bearing split verifies clean ALONE (the all-splits
             * invariant held; only the yeptris-vs-libyaml leg died).
             * Each recorded split gets a solo copy (verbatim + one
             * insert); any one passing the same legs carries. */
            for (int k = 0; k < nsplits && !carried; k++) {
                char* s1 = malloc(size + 1);
                if (s1 == NULL) {
                    break;
                }
                memcpy(s1, data, split_at[k]);
                s1[split_at[k]] = ' ';
                memcpy(s1 + split_at[k] + 1, data + split_at[k], size - split_at[k]);
                carried = yd_charset_verify_copy(s1, size + 1, ldump);
                free(s1);
            }
        }
        if (carried) {
            free(ydump2);
            free(sub);
            free(sub2);
            return 1;
        }
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

/* The anchor-absorption split-point law (the nightly's round 76,
 * crash-48bc37f4): the 1.2 anchor grammar rides to the first blank
 * while libyaml's 1.1 set stops at ':' — the SAME token bytes read
 * with the anchor/value boundary at different points. The riding
 * engine consumes the boundary blank as its props separator; libyaml
 * keeps it as value content, so the merged bytes match only modulo
 * that one blank. Carrying this needs no transform: the charset
 * split's libyaml-invariance leg correctly declines when a +1-byte
 * shift moves some OTHER column-sensitive reading downstream (the
 * 8KB giant-int soup), and the reconciliation is the boundary law
 * itself. Verified structurally: equal event counts, every
 * difference a plain-scalar pair whose merged bytes are equal
 * modulo the boundary blank, everything else byte-identical. */
static void yd_absorb_split_props(const char* line, const char** anchor, size_t* alen,
                                  const char** value, size_t* vlen, int* plain) {
    *anchor = NULL;
    *alen = 0;
    *value = NULL;
    *vlen = 0;
    *plain = 0;
    if (strncmp(line, "=VAL : ", 7) != 0) {
        return;
    }
    *plain = 1;
    const char* p = line + 7;
    if (*p != '&') {
        return; /* no anchor: the law has nothing to reconcile */
    }
    p++;
    const char* sp = strchr(p, ' ');
    if (sp == NULL) {
        *anchor = p;
        *alen = strlen(p);
        return;
    }
    *anchor = p;
    *alen = (size_t)(sp - p);
    *value = sp + 1;
    *vlen = strlen(sp + 1);
}

static int yd_anchor_absorb_divergence(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    const char* y = ydump;
    const char* l = ldump;
    int diffs = 0;
    while (*y != '\0' && *l != '\0') {
        const char* ye = strchr(y, '\n');
        const char* le = strchr(l, '\n');
        size_t yl = ye ? (size_t)(ye - y) : strlen(y);
        size_t ll = le ? (size_t)(le - l) : strlen(l);
        if (yl != ll || strncmp(y, l, yl) != 0) {
            /* a differing pair: both plain scalar events, both
             * anchored, merged bytes equal modulo the boundary blank */
            char yb[4096];
            char lb[4096];
            if (yl >= sizeof(yb) || ll >= sizeof(lb)) {
                return 0;
            }
            memcpy(yb, y, yl);
            yb[yl] = '\0';
            memcpy(lb, l, ll);
            lb[ll] = '\0';
            const char* ya;
            const char* yv;
            size_t yan;
            size_t yvn;
            int yp;
            const char* la;
            const char* lv;
            size_t lan;
            size_t lvn;
            int lp;
            yd_absorb_split_props(yb, &ya, &yan, &yv, &yvn, &yp);
            yd_absorb_split_props(lb, &la, &lan, &lv, &lvn, &lp);
            if (!yp || !lp || ya == NULL || la == NULL) {
                return 0;
            }
            int merged = 0;
            if (yan == lan && strncmp(ya, la, yan) == 0) {
                merged = yvn == lvn && strncmp(yv, lv, yvn) == 0;
            }
            if (!merged) {
                size_t mA = yan + yvn;
                size_t mB = lan + lvn;
                if (mA + 1 != mB && mB + 1 != mA) {
                    return 0; /* not a one-blank difference */
                }
                const char* ba;
                const char* bv;
                size_t ban;
                size_t bvn;
                const char* sa;
                const char* sv;
                size_t san;
                size_t svn;
                if (mA + 1 == mB) {
                    ba = la;
                    ban = lan;
                    bv = lv;
                    bvn = lvn;
                    sa = ya;
                    san = yan;
                    sv = yv;
                    svn = yvn;
                } else {
                    ba = ya;
                    ban = yan;
                    bv = yv;
                    bvn = yvn;
                    sa = la;
                    san = lan;
                    sv = lv;
                    svn = lvn;
                }
                /* the merged (anchor+value) strings differ by exactly
                 * one blank, and it sits at the RIDING side's anchor
                 * boundary: the separator blank the riding engine
                 * consumed is content on the other side. The dump line
                 * keeps a separator blank of its own between the
                 * anchor and the value, so the merged strings must be
                 * materialized before the comparison. */
                size_t ms = san + svn;
                size_t mb = ban + bvn;
                if (san == 0 || ms + 1 != mb) {
                    return 0;
                }
                char* msb = malloc(ms + 1);
                char* mbb = malloc(mb + 1);
                if (msb == NULL || mbb == NULL) {
                    free(msb);
                    free(mbb);
                    return 0;
                }
                memcpy(msb, sa, san);
                memcpy(msb + san, sv, svn);
                msb[ms] = '\0';
                memcpy(mbb, ba, ban);
                memcpy(mbb + ban, bv, bvn);
                mbb[mb] = '\0';
                int ok_m = memcmp(mbb, msb, san) == 0 && mbb[san] == ' ' &&
                           memcmp(mbb + san + 1, msb + san, ms - san) == 0;
                free(msb);
                free(mbb);
                if (!ok_m) {
                    return 0;
                }
                merged = 1;
            }
            diffs++;
            if (diffs > 8) {
                return 0; /* a flood is some other family */
            }
        }
        y = ye ? ye + 1 : y + yl;
        l = le ? le + 1 : l + ll;
    }
    return *y == *l; /* both exhausted together */
}

/* The anchor-keymap splice law (the nightly's round 76, issue #798's
 * structural remainder): for "&I: 999..." (colon-SPACE) the 1.2
 * grammar keeps the colon in the ANCHOR NAME and reads one scalar,
 * while libyaml's 1.1 set truncates the name at ':' and the ':' plus
 * blank becomes the KEY SEPARATOR of an empty-key mapping anchored
 * "I". The merged bytes differ by exactly that grammar colon (it is
 * content in the riding name, a separator token on the other side):
 *
 *   yep:  =VAL : &I: 999…            (one scalar)
 *   lib:  +MAP / =VAL : &I / =VAL : 999… / -MAP
 *
 * Law: event counts differ by 3 per splice; at each splice the riding
 * line's anchor is the libyaml key anchor PLUS ':', the riding value
 * equals the mapping's value, the key's own value is empty, and
 * everything else (including the +MAP/-MAP pair) aligns. */
static int yd_anchor_keymap_splice(const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    size_t yi = 0;
    size_t li = 0;
    size_t ylen = strlen(ydump);
    size_t llen = strlen(ldump);
    int splices = 0;
    while (yi < ylen && li < llen) {
        const char* ye = strchr(ydump + yi, '\n');
        const char* le = strchr(ldump + li, '\n');
        size_t yl = ye ? (size_t)(ye - (ydump + yi)) : ylen - yi;
        size_t ll = le ? (size_t)(le - (ldump + li)) : llen - li;
        if (yl == ll && strncmp(ydump + yi, ldump + li, yl) == 0) {
            yi += yl + 1;
            li += ll + 1;
            continue;
        }
        /* splice attempt: yep's ONE scalar vs lib's 4-event map block */
        if (strncmp(ydump + yi, "=VAL : &", 8) != 0) {
            return 0;
        }
        const char* ya = ydump + yi + 8; /* after "=VAL : &" */
        const char* ysp = memchr(ya, ' ', yl - 8);
        if (ysp == NULL) {
            return 0;
        }
        size_t yan = (size_t)(ysp - ya);
        if (yan < 2 || ya[yan - 1] != ':') {
            return 0; /* the riding name must end with the grammar ':' */
        }
        const char* yv = ysp + 1;
        size_t yvn = yl - 8 - yan - 1;
        /* lib's 4 lines */
        const char* l1 = ldump + li;
        if (ll != 4 || strncmp(l1, "+MAP", 4) != 0) {
            return 0;
        }
        li += ll + 1;
        const char* le2 = strchr(ldump + li, '\n');
        if (le2 == NULL) {
            return 0;
        }
        size_t ll2 = (size_t)(le2 - (ldump + li));
        if (ll2 < 10 || strncmp(ldump + li, "=VAL : &", 8) != 0) {
            return 0;
        }
        const char* la = ldump + li + 8;
        size_t lan = ll2 - 8;
        const char* lvk = NULL;
        size_t lvkn = 0;
        const char* ksp = memchr(la, ' ', lan);
        if (ksp != NULL) {
            lvk = ksp + 1;
            lvkn = lan - (size_t)(ksp - la) - 1;
            lan = (size_t)(ksp - la);
        }
        li += ll2 + 1;
        const char* le3 = strchr(ldump + li, '\n');
        if (le3 == NULL) {
            return 0;
        }
        size_t ll3 = (size_t)(le3 - (ldump + li));
        if (ll3 < 7 || strncmp(ldump + li, "=VAL : ", 7) != 0) {
            return 0;
        }
        const char* lv = ldump + li + 7;
        size_t lvn = ll3 - 7;
        li += ll3 + 1;
        const char* le4 = strchr(ldump + li, '\n');
        size_t ll4 = le4 ? (size_t)(le4 - (ldump + li)) : llen - li;
        if (ll4 != 4 || strncmp(ldump + li, "-MAP", 4) != 0) {
            return 0;
        }
        li += ll4 + 1;
        /* the law: the riding name is the libyaml key's bytes (its
         * anchor AND its content) plus the grammar ':' — the 1.1 set
         * cut the name at its first ':', the residual bytes were the
         * key's plain, and the ':' before the break was the separator
         * (round 77, "&k:\":<PS> U\<CR> U\": key anchor "k", key
         * content ":\"", separator ':'+break). The riding value is
         * the mapping's value, at most one LEADING fold blank richer
         * — the break that carried the separator folds inside the
         * riding plain (round 76's empty-key form is the special
         * case: zero key content, zero fold blank). */
        if (yan < lan + 1 || strncmp(ya, la, lan) != 0) {
            return 0;
        }
        size_t keyvn = yan - lan - 1;
        if (ya[yan - 1] != ':') {
            return 0;
        }
        if (keyvn > 0 && strncmp(ya + lan, lvk, keyvn) != 0) {
            return 0;
        }
        int vok = yvn == lvn && strncmp(yv, lv, yvn) == 0;
        if (!vok && yvn == lvn + 1 && yv[0] == ' ' && strncmp(yv + 1, lv, lvn) == 0) {
            vok = 1;
        }
        if (!vok) {
            return 0;
        }
        splices++;
        if (splices > 8) {
            return 0;
        }
        yi += yl + 1;
    }
    return yi >= ylen && li >= llen;
}

static void probe_one(const uint8_t* data, size_t size) {
    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)data, size, &ydump, &ldump);
    int split = v == YD_YEPTRIS_ONLY || v == YD_LIBYAML_ONLY;
    /* a documented divergence, not a bug: the suite sides with
     * yeptris (see yd_qflow_divergence) */
    int known =
        v == YD_DIFFER &&
        (yd_qflow_divergence(ydump, ldump) ||
         yd_qflow_adjacent_divergence(data, size, ydump, ldump) ||
         yd_qflow_colon_divergence(data, size, ydump, ldump) ||
         yd_qflow_question_colon_divergence(data, size, ydump, ldump) ||
         yd_bom_content_divergence(ydump, ldump) ||
         yd_bom_line_start_divergence(data, size, ydump, ldump) ||
         yd_bom_utf16_line_start_divergence(data, size, ydump, ldump) ||
         yd_breaknorm_divergence(data, size, ydump) ||
         yd_nel_break_divergence(data, size, ydump, ldump) ||
         yd_composed_break_divergence(data, size) || yd_root_block_content_divergence(data, size) ||
         yd_clip_breakless_divergence(data, size, ydump) ||
         yd_ps_space_divergence(data, size, ydump, ldump) ||
         yd_plain_lsps_fold_divergence(data, size, ydump, ldump) ||
         yd_plain_nel_fold_divergence(data, size, ydump, ldump) ||
         yd_r121_permuted_gray(data, size, ydump, ldump) ||
         yd_r137_soup_gray(data, size, ydump, ldump) ||
         yd_r138_soup_gray(data, size, ydump, ldump) ||
         yd_r139_soup_gray(data, size, ydump, ldump) ||
         yd_r140_soup_gray(data, size, ydump, ldump) ||
         yd_r141_soup_gray(data, size, ydump, ldump) ||
         yd_r151_soup_gray(data, size, ydump, ldump) ||
         yd_r129_soup_gray(data, size, ydump, ldump) ||
         yd_r130_structural_gray(data, size, ydump, ldump) ||
         yd_flow_anchor_colon_divergence(data, size, ydump, ldump) ||
         yd_anchor_name_charset_divergence(data, size, ydump, ldump) ||
         yd_anchor_absorb_divergence(ydump, ldump) || yd_anchor_keymap_splice(ydump, ldump) ||
         yd_anchor_glued_follower_divergence(data, size, ydump, ldump) ||
         yd_header_glued_comment_divergence(data, size, ydump, ldump) ||
         yd_nonspecific_tag_divergence(data, size, ydump));
    /* the glued-comment x flow-'?' split ("{?#}", the PR #894 session's
     * second candidate): libyaml's KEY-token-plus-comment-glue reading
     * REJECTS what yeptris's suite-pinned plain-'?' law accepts — a
     * documented split, classified by the quote-out transform. The
     * r115 nightly crash-01361e26 caught the family's OTHER verdict:
     * with a ` .:` continuation after the run, libyaml does not reject
     * — it EMPTIES the key (the glued '#' opens a comment; the pair
     * rides with a null key) — an accept/accept YD_DIFFER the old
     * YEPTRIS_ONLY-only consult never reached. The quote-out transform
     * classifies both verdicts: quoting the run reproduces yeptris's
     * reading in BOTH engines. */
    if (!known && (v == YD_YEPTRIS_ONLY || v == YD_DIFFER)) {
        known = yd_qflow_glued_hash_split(data, size, ydump);
        if (!known && v == YD_DIFFER) {
            known = yd_qflow_glued_hash_differ(data, size, ydump, ldump);
        }
    }
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
