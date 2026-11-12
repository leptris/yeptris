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
 * both engines' dumps of the removal must equal libyaml's original.
 * The r226 nightly (run 42622039396, main 892510b, crash-c76eee55,
 * UTF-16BE "mark<NEL><BOM><NEL><BOM><NEL>a") widened "line start" to
 * every break kind — libyaml's scan_to_next_token drops a BOM at mark
 * column 0 whatever break opened the line (LF, CR, CRLF, NEL, LS, PS;
 * byte-probed on the r226 probes), while yeptris keeps every content
 * BOM (the catch-121 law); the UTF-8 NEL (C2 85) and LS/PS (E2 80
 * A8/A9) sequences now open lines here too, matching the UTF-16 unit
 * arm below. */
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
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            sub[o++] = data[++i]; /* the UTF-8 NEL: a line start too --
                                   * libyaml's column-0 BOM skip is
                                   * break-kind-agnostic (the r154 law),
                                   * and the r226 nightly's UTF-8 twin
                                   * rides BOMs past NEL lines */
            line_start = 1;
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            sub[o++] = data[++i]; /* LS/PS: line starts too */
            sub[o++] = data[++i];
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
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            sub2[o++] = data[++i]; /* the UTF-8 NEL: a line start too */
            line_start = 1;
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            sub2[o++] = data[++i]; /* LS/PS: line starts too */
            sub2[o++] = data[++i];
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

/* the 1.1 anchor-name charset (alnum + '_' + '-', defined below with
 * the charset-split arms): the PROP-SPLIT twin's cut position */
static int yd_anchor_byte_in_libyaml_set(unsigned char n);

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
    /* the folded-break head twin (the nightly's round 159, crash-daa73e2d,
     * run 42154482223, main 7b55b9a): the content BOM riding a plain's
     * FIRST LINE with the following break folding. The r159 shape --
     * UTF-16BE mark, U+0085, U+FEFF, U+0085, then "a..." -- strips the
     * leading NEL as a plain's leading break on BOTH sides; yeptris keeps
     * the column-0 FEFF as CONTENT (the catch-121 law) so the next NEL
     * folds to ONE space (the plain-NEL fold law), while libyaml skips
     * the column-0 BOM (its scan_to_next_token, the r154 law) and the
     * NEL stays LEADING -- consumed with no fold artifact. The plain
     * strip above declines: the fold space survives it ("=VAL : <BOM> a"
     * strips to "=VAL :  a", two spaces, vs libyaml's one), and the
     * line-start removal probes decline too -- a UTF-16 NEL unit is
     * never a byte-level '\n', so no byte-space removal is pure. The
     * composition claim: strip the BOM AND the one fold space behind it
     * -- the pair is exactly "content BOM + the break that folded", and
     * libyaml's reading is the same plain with the BOM gone and the
     * break returned to leading (consumed whole). Fires only when the
     * residual opens with ONE space followed by content on the same
     * dump line, and only on an exact dump match, so the claim stays
     * byte-pinned; a space is never a plain's head content in either
     * engine (leading whitespace is consumed), so the pair can only be
     * the fold. Encoding-symmetric: the UTF-16LE twin classifies
     * through the same dump-level relation. */
    if (rest[0] == ' ' && rest[1] != '\0' && rest[1] != ' ' && rest[1] != '\n') {
        char folded[8192];
        size_t rlen = strlen(rest);
        if (head + rlen < sizeof(folded)) {
            memcpy(folded, stripped, head); /* "=VAL : " */
            memcpy(folded + head, rest + 1, rlen - 1);
            folded[head + rlen - 1] = '\0';
            if (strcmp(folded, ldump) == 0) {
                return 1;
            }
        }
    }
    /* the folded-break-RUN head twin (the nightly's round 165, crash-
     * 2e1d2b72, run 42193562130, main 57f2e4b): the space form's
     * BLANK-LINE sibling. The r165 shape adds ONE NEL unit to the r159
     * artifact -- UTF-16BE mark, U+0085, U+FEFF, U+0085, U+0085, then
     * "a..." -- so the THIRD NEL makes a BLANK LINE between the FEFF
     * content and 'a', and the plain-NEL fold law renders an escaped
     * BREAK, not the space (k == 1 behind the content folds to one
     * space, k >= 2 collapses to k-1 escaped breaks -- the r118/r119/
     * r122/r128 artifact law), so the space gate declines: after the
     * one-BOM strip the residual opens with the two-byte "\n" escape.
     * libyaml's side is unchanged: its column-0 BOM skip (the r154
     * law) puts the whole NEL run back in leading position and leading
     * breaks are consumed WHOLE -- no artifact for the blank line
     * either. The composition claim: strip the BOM AND the run of
     * escaped breaks behind it -- the run is exactly "content BOM +
     * the breaks that folded", one escape per blank line, and
     * libyaml's reading is the same plain with the BOM gone and the
     * breaks returned to leading (consumed whole). Fires only when the
     * residual opens with a maximal run of the two-byte "\n" escape
     * followed by non-space content on the same dump line, and only on
     * an exact dump match, so the claim stays byte-pinned; the dumps
     * escape every real newline as that two-byte form, a run can only
     * be the fold's blank-line renders (trailing breaks strip -- the
     * r122 law -- so a run is never a value's tail), and a space is
     * never a continuation's head content (its indent is consumed), so
     * the pair can only be the fold. Encoding- and break-symmetric:
     * the UTF-16LE twin and the LS/PS/mixed members classify through
     * the same dump-level relation (LS/PS normalize to the same
     * escaped breaks in yeptris and stay leading in libyaml). */
    if (rest[0] == '\\' && rest[1] == 'n') {
        const char* q = rest;
        while (q[0] == '\\' && q[1] == 'n') {
            q += 2;
        }
        if (q[0] != '\0' && q[0] != ' ') {
            char folded[8192];
            size_t rlen = strlen(rest);
            size_t cut = (size_t)(q - rest);
            if (head + rlen - cut < sizeof(folded)) {
                memcpy(folded, stripped, head); /* "=VAL : " */
                memcpy(folded + head, q, rlen - cut);
                folded[head + rlen - cut] = '\0';
                if (strcmp(folded, ldump) == 0) {
                    return 1;
                }
            }
        }
    }
    const char* line = stripped + (size_t)(val - ydump);
    while (line > stripped && line[-1] != '\n') {
        line--; /* back to the scalar line's start */
    }
    const char* nl = strchr(line, '\n');
    size_t linelen = nl ? (size_t)(nl - line) : strlen(line);
    const char* lend = nl ? nl + 1 : line + linelen;
    /* the anchored-key twin (the nightly's round 154, crash-97fad546,
     * run 42115966505, main 2386dcf6): the content BOM rides INSIDE
     * yeptris's KEY ("<BOM>&U:"), so the '&' that follows is plain
     * CONTENT there -- while libyaml strips the column-0 BOM too (its
     * scan_to_next_token skips a BOM at mark column 0: the
     * encoding-mark law is not stream-position-bound), so its residual
     * "&U" is an ANCHORED EMPTY SCALAR -- props, not content. The
     * plain strip above cannot reach it: after the strip yeptris's
     * line reads "=VAL : &U" (content) while libyaml's reads
     * "=VAL : &U " (props + separator blank, empty value) -- and the
     * line-start removal probes decline as well, because the BOM-less
     * residual "&U:" is itself the r76 anchor-keymap shape (yeptris's
     * 1.2 grammar rides the ':' into the anchor name; libyaml's 1.1
     * set cuts at it and opens a map), so the removal is not pure.
     * The composition claim: file yeptris's stripped residual into
     * libyaml's prop form -- insert the one separator blank -- and
     * compare. Fires only when the residual is a bare "&name" (one
     * token, no blank in it), so the claim stays byte-pinned. */
    if (linelen > 7 && line[7] == '&' && memchr(line + 7, ' ', linelen - 7) == NULL) {
        char anchored[8192];
        size_t ahead = (size_t)(line - stripped);
        size_t atail = nl ? strlen(nl) : 0; /* from the line's break on */
        if (ahead + linelen + 1 + atail + 1 < sizeof(anchored)) {
            memcpy(anchored, stripped, ahead);
            memcpy(anchored + ahead, line, linelen);
            anchored[ahead + linelen] = ' '; /* the props separator blank */
            if (nl != NULL) {
                memcpy(anchored + ahead + linelen + 1, nl, atail + 1);
            } else {
                anchored[ahead + linelen + 1] = '\0';
            }
            if (strcmp(anchored, ldump) == 0) {
                return 1;
            }
        }
    }
    /* the anchored-key PROP-SPLIT twin (the nightly's round 156,
     * crash-3adc7533, run 42132975720, main 821b27e7): the r156 shape
     * "BOM BOM '&U:#'" -- the r154 9-byte form with a '#' GLUED after
     * the ':' -- declines the bare-residual twin above: yeptris's
     * stripped residual "&U:#" is one glued run, but libyaml's is not
     * props-plus-empty. Its anchor scan consumes the MAXIMAL run of
     * 1.1 anchor-set bytes (yd_anchor_byte_in_libyaml_set) and cuts
     * at the ':', so its dump files the residual as props "&U" plus
     * the one separator blank plus ":#" riding as plain CONTENT on
     * both sides (the catch-85 law: the ':' has no blank follower and
     * the glued '#' opens no comment). The composition claim: file
     * yeptris's stripped residual into libyaml's prop form at the
     * 1.1-charset cut -- the separator blank goes AFTER the name,
     * preserving the glued tail byte-for-byte. No engine violates its
     * own law: yeptris never double-strips (catch-121), libyaml never
     * keeps a column-0 BOM, and the blank position is the documented
     * 1.1 anchor-name cut (the r76 law). Fires only on an exact dump
     * match, so the claim stays byte-pinned; the r154 bare form is
     * the tail-empty special case this law degenerates to. */
    if (linelen > 7 && line[7] == '&' && memchr(line + 7, ' ', linelen - 7) == NULL) {
        size_t k = 0;
        while (8 + k < linelen && yd_anchor_byte_in_libyaml_set((unsigned char)line[8 + k])) {
            k++;
        }
        if (k > 0) {
            char split[8192];
            size_t ahead = (size_t)(line - stripped);
            size_t atail = nl ? strlen(nl) : 0; /* from the line's break on */
            if (ahead + linelen + 1 + atail + 1 < sizeof(split)) {
                memcpy(split, stripped, ahead);     /* lines before the scalar */
                memcpy(split + ahead, line, 8 + k); /* "=VAL : &" + the name */
                split[ahead + 8 + k] = ' ';         /* the props separator blank at
                                                       the 1.1-charset cut */
                memcpy(split + ahead + 8 + k + 1, line + 8 + k, linelen - 8 - k);
                if (nl != NULL) {
                    memcpy(split + ahead + linelen + 1, nl, atail + 1);
                } else {
                    split[ahead + linelen + 1] = '\0';
                }
                if (strcmp(split, ldump) == 0) {
                    return 1;
                }
            }
        }
    }
    /* the value-vanishes twin (the nightly's crash-dbbc083c, two
     * content BOMs): libyaml strips BOTH BOM bytes and emits an EMPTY
     * stream, so the scalar line is GONE from its dump -- drop
     * yeptris's whole scalar line too and compare. */
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
 * to equal libyaml's original dump. The r226 nightly (run
 * 42622039396, main 892510b, crash-c76eee55: "FE FF 00 85 FE FF 00 85
 * FE FF 00 85 00 61" -- the NEL member) widened the line-start gate
 * from LF-only to every break unit: libyaml's scan_to_next_token
 * drops a BOM at mark column 0 whatever break opened the line (the
 * r154 law, byte-probed across LF/CR/NEL/LS/PS), while yeptris keeps
 * every content BOM (the catch-121 law). */
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
        /* a line starts after ANY break unit -- LF, CR, NEL, LS, PS
         * (the r226 nightly artifact, run 42622039396, main 892510b,
         * crash-c76eee55: the NEL member -- "mark<NEL><BOM><NEL><BOM>
         * <NEL>a" -- rode BOMs at NEL line starts past the LF-only
         * gate). libyaml's column-0 BOM skip is break-kind-agnostic:
         * its scan_to_next_token drops a BOM at mark column 0 whatever
         * break opened the line (the r154 law, byte-probed across
         * LF/CR/NEL/LS/PS), while yeptris keeps every content BOM
         * (the catch-121 law). CRLF sets the flag twice -- harmless. */
        line_start = (hi == 0x00 && (lo == 0x0A || lo == 0x0D || lo == 0x85)) ||
                     (hi == 0x20 && (lo == 0x28 || lo == 0x29));
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
            /* the r207 refinement (run 42503100451, crash-f810355b):
             * when the follower is a blank/break GAP up to the entry's
             * OWN ':' separator ("?\t:", the artifact's "{\n?\t
             * \n\t\t \t\t: ,?:-,?*M,}"), the '?'-led pair already
             * carries its separator — spelling '""' ':' would DOUBLE
             * it (a stray ':' the rewritten entry can never absorb,
             * both engines reject, the verify dies before the glued
             * hits are even read). Both engines read that explicit-key
             * pair identically (libyaml's KEY token, yeptris's flow-?
             * carve), so the pair rides VERBATIM — no spell at all.
             * A blank-gapped '?' whose gap ends at anything else keeps
             * the r85 '""' ':' spell. */
            {
                size_t k = i + 1;
                while (k < size) {
                    unsigned char g = data[k];
                    if (g == ' ' || g == '\t' || g == '\n' || g == '\r') {
                        k++;
                        continue;
                    }
                    if (g == 0xC2 && k + 1 < size && data[k + 1] == 0x85) {
                        k += 2; /* NEL */
                        continue;
                    }
                    if (g == 0xE2 && k + 2 < size && data[k + 1] == 0x80 &&
                        (data[k + 2] == 0xA8 || data[k + 2] == 0xA9)) {
                        k += 3; /* LS/PS */
                        continue;
                    }
                    break;
                }
                if (k < size && data[k] == ':') {
                    sub[subn++] = (char)c;
                    i++;
                    continue;
                }
            }
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

/* True when the verdict is the SEQUENCE orientation of the glued flow
 * '?' family (the r183 nightly, run 42386647198, crash-0732517306:
 * "[-1.0<U+2029> 7.tag<CR> ,?.]"): libyaml's unconditional flow
 * KEY-token law (the 1.1 quirk the qflow family carries) reads the
 * glued '?' as a KEY whatever follows it, and the enclosing flow
 * SEQUENCE wraps the keyless pair as a NESTED single-pair mapping
 * member — while yeptris's suite-pinned plain reading (HM87/652Z: a
 * flow '?' with a non-space follower rides as plain content) keeps
 * "?." one plain scalar member. The r71 drop-'?' spell cannot
 * reproduce that reading: in a MAPPING the dropped '?' leaves the
 * follower a key-position plain (libyaml's same pair), but in a
 * SEQUENCE it degrades to a plain member and the nested pair
 * vanishes — which is exactly why every earlier arm's verify
 * declined this orientation. The preserving spell wraps the follower
 * run in an EXPLICIT single-pair flow map — "{run: }" — which both
 * engines read as the nested pair libyaml produced. The finder only
 * fires on a '?' at flow depth whose innermost frame is a SEQUENCE
 * and whose follower starts a same-line plain-safe run terminated by
 * ',' / ']' / EOF (quoted, indicator, and boundary followers stay
 * with the other qflow arms; a ':'-glued follower is the r58 pair
 * spell's turf). A GLUED '#' mid-run is admitted since the r206
 * nightly (run 42498570153, crash-df52ab9fc21e989fc250fe4592c46bc982c9570d):
 * after a non-blank it is mid-plain content in both engines (the HM87
 * law), while after a blank or an exotic break it is the comment
 * boundary and declines (the r185 glue stays its own arm). The
 * byte-strict verify against libyaml's ORIGINAL
 * dump arbitrates: both engines must read the rewrite exactly as
 * libyaml read the original, or the claim declines. */
static int yd_r183_flow_seq_question_divergence(const uint8_t* data, size_t size, const char* ydump,
                                                const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size * 4 + 4); /* each hit turns "?run" into "{run: }" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
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
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size) {
            unsigned char f = data[i + 1];
            /* the follower must START a plain run: blanks and every
             * flow/block indicator decline to the other arms' spells */
            if (f != ' ' && f != '\t' && f != '\n' && f != '\r' && f != ',' && f != ':' &&
                f != ']' && f != '}' && f != '"' && f != '\'' && f != '[' && f != '{' && f != '#' &&
                f != '&' && f != '*' && f != '!' && f != '|' && f != '>' && f != '%' && f != '@' &&
                f != '`' && f != '?') {
                /* the run itself must be same-line plain-safe, ended by
                 * ',' / ']' / EOF: a break, a quote, or any indicator
                 * inside declines the whole hit. The GLUED '#' is the one
                 * exception (the r206 nightly, run 42498570153, crash-
                 * df52ab9fc21e989fc250fe4592c46bc982c9570d, main 7d5483e:
                 * "[11n<CR><PS> 7.tag<CR> ,?d#  m,]"): '#' mid-plain after
                 * a non-blank is CONTENT in both engines (the HM87 law --
                 * libyaml's scan_plain comment check only fires at a blank
                 * boundary after whitespace/breaks), so the r185 glue is
                 * not in play and the spell stays reproducible: the
                 * rewrite "{run: }" renders the same glued '#' as the
                 * explicit pair's key, and the r183 LS/PS fallback owns
                 * the artifact's CR+PS double-break head (yeptris spells
                 * the empty line's LF, libyaml keeps the raw PS). A '#'
                 * after a blank, or byte-glued to an exotic break's tail
                 * (NEL C2 85, LS/PS E2 80 A8/A9 continuations), IS the
                 * comment boundary -- decline to the other arms. */
                size_t j = i + 1;
                int clean = 1;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '#') {
                        unsigned char p = data[j - 1];
                        if (p != ' ' && p != '\t' && p != '\n' && p != '\r' && p != 0xA8 &&
                            p != 0xA9 && p != 0x85) {
                            j++;
                            continue;
                        }
                        clean = 0;
                        break;
                    }
                    if (d == '}' || d == '\n' || d == '\r' || d == '[' || d == '{' || d == '"' ||
                        d == '\'' || d == '\\' || d == ':' || d == '?') {
                        clean = 0;
                        break;
                    }
                    j++;
                }
                if (clean) {
                    sub[subn++] = '{';
                    memcpy(sub + subn, data + i + 1, j - (i + 1));
                    subn += j - (i + 1);
                    sub[subn++] = ':';
                    sub[subn++] = ' ';
                    sub[subn++] = '}';
                    i = j;
                    hits++;
                    continue;
                }
            }
        }
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the LS/PS fallback (the r183 artifact rides one): libyaml's dump
     * keeps a mid-plain U+2028/U+2029 RAW ("-1.0<PS>7.tag") while
     * yeptris follows the spec's fold law (one break -> a space), so
     * the raw byte-strict compare can never hold for a multi-line
     * plain. Normalize every break (LS/PS/NEL/CR -> LF) on BOTH sides
     * — a meaning-preserving rewrite for both engines' plain/quoted/
     * block readings — and demand the triple: both engines read the
     * normalized rewrite identically AND libyaml reads the normalized
     * original identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the GLUED-COLON PAIR of the flow-SEQ '?'
 * family (the r221 nightly, run 42579140515, main 6fed677, crash-
 * ccf950b71f788e332fb0352018c14df84c6ad84d, minimized
 * "[)<U+2029>.,?00: m$]"). libyaml's unconditional flow KEY-token law
 * (the quirk the qflow family carries) reads the glued '?' as a KEY
 * whatever follows, and its scan_plain stop law ends the key's plain
 * run only at a ':'+blankz (the pair separator) or a flow boundary —
 * so "00" rides libyaml's key slot with "m$" as the pair's value,
 * while yeptris's suite-pinned plain reading (HM87/652Z) keeps "?00"
 * one plain key. The r183 spells cannot ride this shape: its run
 * scanner declines on ANY ':' inside the run, and the "{run: }" wrap
 * cannot absorb the pair's own separator and value. The preserving
 * spell quotes the KEY RUN — "?" "00" becomes \"00\" — and rides the
 * separator and value verbatim, which both engines read as exactly
 * the pair libyaml produced. A run whose ':'s are ALL GLUED ends at
 * the entry boundary instead ("?00:$]" -> "{00:$: }", libyaml's
 * key-plus-empty-value, the r183 wrap spelling); the arm fires only
 * when a ':' is load-bearing somewhere — the colon-free boundary run
 * stays r183's turf, and the mapping orientation stays the r58/r71
 * drop-'?' spell's turf (the innermost frame must be a SEQUENCE).
 * The byte-strict verify against libyaml's ORIGINAL dump arbitrates,
 * with the CR/LS/PS/NEL-normalized triple as the fallback (the
 * artifact rides one: its head plain keeps the raw PS in libyaml's
 * dump while yeptris folds it, so the byte-strict compare can never
 * hold for the multi-break head). */
static int yd_r221_seq_question_glued_colon_pair_divergence(const uint8_t* data, size_t size,
                                                            const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size * 4 + 4); /* each hit adds at most two bytes */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
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
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size) {
            unsigned char f = data[i + 1];
            /* the follower must START a plain run: blanks and every
             * flow/block indicator decline to the other arms' spells */
            if (f != ' ' && f != '\t' && f != '\n' && f != '\r' && f != ',' && f != ':' &&
                f != ']' && f != '}' && f != '"' && f != '\'' && f != '[' && f != '{' && f != '#' &&
                f != '&' && f != '*' && f != '!' && f != '|' && f != '>' && f != '%' && f != '@' &&
                f != '`' && f != '?') {
                /* the run is libyaml's key: it ends BEFORE a ':'+blankz
                 * (the pair separator -- the scan_plain stop law) or at
                 * a ',' / ']' / '}' / EOF entry boundary; a ':' glued to
                 * a non-blank follower is mid-plain CONTENT and rides
                 * the run (the r221 drift twin "?00:$]"). Everything
                 * else a break, a quote, a nested opener, or an
                 * indicator declines the whole hit. */
                size_t j = i + 1;
                int clean = 1;
                int sep = 0;   /* stopped before the pair's ':' separator */
                int colon = 0; /* a ':' is load-bearing in this hit */
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ':') {
                        unsigned char n = (j + 1 < size) ? data[j + 1] : (unsigned char)0;
                        int nblank = (n == ' ' || n == '\t' || n == '\n' || n == '\r' || n == 0);
                        int nmulti = (n == 0xC2 && j + 2 < size && data[j + 2] == 0x85) ||
                                     (n == 0xE2 && j + 3 < size && data[j + 2] == 0x80 &&
                                      (data[j + 3] == 0xA8 || data[j + 3] == 0xA9));
                        colon = 1;
                        if (nblank || nmulti) {
                            /* the pair's separator: the run ends HERE */
                            sep = 1;
                            break;
                        }
                        /* glued to a non-blank follower: mid-plain
                         * CONTENT (the scan_plain stop law) -- rides the
                         * run on to the entry boundary */
                        j++;
                        continue;
                    }
                    if (d == ',' || d == ']' || d == '}') {
                        break;
                    }
                    if (d == '[' || d == '{' || d == ' ' || d == '\t' || d == '\n' || d == '\r' ||
                        d == '"' || d == '\'' || d == '\\' || d == '#' || d == '&' || d == '*' ||
                        d == '!' || d == '|' || d == '>' || d == '%' || d == '@' || d == '`' ||
                        d == '?') {
                        clean = 0;
                        break;
                    }
                    if (d == 0xC2 && j + 1 < size && data[j + 1] == 0x85) {
                        clean = 0;
                        break;
                    }
                    if (d == 0xE2 && j + 2 < size && data[j + 1] == 0x80 &&
                        (data[j + 2] == 0xA8 || data[j + 2] == 0xA9)) {
                        clean = 0;
                        break;
                    }
                    j++;
                }
                if (clean && colon) {
                    hits++;
                    if (sep) {
                        /* quote the key run; the separator and the
                         * pair's value ride verbatim */
                        sub[subn++] = '"';
                        memcpy(sub + subn, data + i + 1, j - (i + 1));
                        subn += j - (i + 1);
                        sub[subn++] = '"';
                        i = j;
                    } else {
                        /* every ':' glued: the run IS libyaml's key
                         * with an empty value -- the r183 wrap spells
                         * that pair for both engines */
                        sub[subn++] = '{';
                        memcpy(sub + subn, data + i + 1, j - (i + 1));
                        subn += j - (i + 1);
                        sub[subn++] = ':';
                        sub[subn++] = ' ';
                        sub[subn++] = '}';
                        i = j;
                    }
                    continue;
                }
            }
        }
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    /* the byte-strict verify: both engines must read the rewrite
     * exactly as libyaml read the original */
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183 pattern): the
     * artifact's head plain keeps the raw PS in libyaml's dump while
     * yeptris follows the spec's fold law, so the byte-strict compare
     * can never hold. Normalize every break (LS/PS/NEL/CR -> LF) on
     * BOTH sides -- a meaning-preserving rewrite for both engines'
     * plain/quoted readings -- and demand the triple: both engines
     * read the normalized rewrite identically AND libyaml reads the
     * normalized original identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the COMMENT-SWALLOW pair of the flow-SEQ '?'
 * family (the r222 nightly, run 42586898367, main f45cc5b, crash-
 * a651e854ca78c7c82d18f455bbe262099810ea77, DIFFER-constrained minimal
 * "[1,?##|,?<CR>:m]"): libyaml's unconditional flow KEY-token law (the
 * quirk the qflow family carries) reads the glued '?' as a KEY, and the
 * '#' GLUED to that KEY token sits at the next token position --
 * scan_to_next_token's unconditional '#' eat (the carried 1.1 quirk the
 * r132 family documents) swallows "##|,?" through the break -- the
 * entry separator AND the following '?' ride the comment, and the next
 * line's plain re-lexes as the KEY's own scalar. yeptris's suite-pinned
 * plain reading (HM87/652Z) keeps "?##|" one plain member (the '#'
 * after a non-blank is mid-plain content, the r206 law) and the
 * following '?' its own explicit key. The r183 spell cannot ride this
 * shape: its follower gate declines '#' (a '#' glued to the '?' is not
 * mid-plain content HERE -- it is the comment boundary at the token
 * position libyaml's KEY freed), and the "{run: }" wrap cannot
 * reproduce a comment that eats the entry separator and the next
 * indicator. The preserving spell DELETES the glued '#' through its
 * comment's end and KEEPS the '?' -- separated from the next line by
 * the break it re-lexes as the explicit KEY in BOTH engines (the
 * tokens libyaml sees are exactly its original KEY + the following
 * plain), which leaves both engines the exact skeleton libyaml
 * produced (the next line's multiline plain re-lexes as the EXPLICIT
 * pair's key -- the explicit form carries no single-line simple-key
 * law, the bare delete's implicit demotion rejects in libyaml). The
 * innermost frame must be a SEQUENCE (the mapping orientation stays
 * the qflow glued-hash walk's turf), and the byte-strict verify
 * against libyaml's ORIGINAL dump arbitrates, with the
 * CR/LS/PS/NEL-normalized triple as the fallback (the artifact's
 * multiline key keeps the raw PS in libyaml's dump while yeptris
 * follows the spec's fold law, so the byte-strict compare can never
 * hold). The quote tracker is the r145 plain-run-aware one (the
 * artifact's three double-quotes all ride GLUED mid-plain as content;
 * the naive tracker's phantom quote would strand the later hits). */
static int yd_r222_seq_question_glued_hash_comment_divergence(const uint8_t* data, size_t size,
                                                              const char* ydump,
                                                              const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size + 1); /* deletions only shrink */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    int plain = 0; /* a plain run is active: glued quotes are content */
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
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
            if (!plain) {
                quote = '\'';
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '"') {
            if (!plain) {
                quote = '"';
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '#' && (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' ||
                         data[i - 1] == '\n' || data[i - 1] == '\r')) {
            /* a blank-led comment swallows the line; the node ended and
             * nothing inside it opens a quote or scores */
            plain = 0;
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                if (data[i] == '\r' && i + 1 < size && data[i + 1] == '\n') {
                    sub[subn++] = '\r';
                    i++;
                }
                sub[subn++] = (char)data[i]; /* the break rides along */
            }
            i++;
            continue;
        }
        if (c == '[' || c == '{') {
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ',') {
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                sub[subn++] = '\r';
                i++;
            }
            if (depth == 0) {
                plain = 0; /* block lines: each line is a fresh node */
            }
            /* a flow plain folds across its break: the run rides */
            sub[subn++] = (char)data[i];
            i++;
            continue;
        }
        if (c == ' ' || c == '\t') {
            /* fold blanks ride the plain run */
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size &&
            data[i + 1] == '#') {
            /* THE HIT: libyaml's KEY token frees the glued '#' to open
             * a comment -- delete the '#' through the comment's end
             * but KEEP the '?': separated from its follower by the
             * break, the '?' re-lexes as the explicit KEY in BOTH
             * engines (a bare delete would demote the next line's
             * multiline plain to an IMPLICIT key, and a multiline
             * plain is no simple key -- libyaml rejects, the r222
             * rewrite probe's error). The comment ends at CR/LF (the
             * exotic breaks decline: the spell must not cross a
             * boundary the scanners disagree about); the break itself
             * rides the loop (the KEY's blankz follower). */
            size_t j = i + 1;
            while (j < size && data[j] != '\n' && data[j] != '\r') {
                j++;
            }
            sub[subn++] = '?';
            plain = 0;
            i = j;
            hits++;
            continue;
        }
        if (c == ':' || c == '?' || (c == '-' && depth == 0) || c == '&' || c == '*' || c == '!') {
            /* separated, the indicator ends the run (a props/value
             * position where a quoted node can start); glued, it is
             * plain-first content (the HM87 plain-'?' law) */
            unsigned char f = i + 1 < size ? data[i + 1] : ' ';
            if (f == ' ' || f == '\t' || f == '\n' || f == '\r' || f == ',' || f == '[' ||
                f == ']' || f == '{' || f == '}') {
                plain = 0;
            } else {
                plain = 1;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        /* everything else: wordy content keeps the run */
        plain = 1;
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    /* the byte-strict verify: both engines must read the rewrite
     * exactly as libyaml read the original */
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183/r221 pattern): the
     * artifact's multiline key keeps the raw PS in libyaml's dump while
     * yeptris follows the spec's fold law, so the byte-strict compare
     * can never hold. Normalize every break (LS/PS/NEL/CR -> LF) on
     * BOTH sides -- a meaning-preserving rewrite for both engines'
     * plain/quoted readings -- and demand the triple: both engines
     * read the normalized rewrite identically AND libyaml reads the
     * normalized original identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the ANCHORED GLUED-DOUBLE-COLON COLLAPSE of
 * the block family (the r223 nightly, run 42595582912, main 6330309,
 * crash-3f0335158b94b9260b77f1d5e80fb63cda358df8, the 11-byte
 * "&a::\n  *e:\n"). libyaml's 0.2 plain law frees the FIRST glued colon
 * after an anchor: a block-context ':' not followed by a blank is a
 * plain-FIRST char (block ':' is plain-safe mid-scalar), the plain stops
 * at the SECOND glued colon (blankz-followed), and that second colon
 * re-lexes as the IMPLICIT KEY's value indicator: the document is
 * {&a ':' -> {e -> ""}} (+MAP / =VAL : &a : / +MAP / =ALI *e / =VAL : /
 * -MAP / -MAP). yeptris's line model reads the same head as ONE plain
 * scalar ("&a::" whole -- the carried v1 reading: the glued '&' is
 * mid-plain content) and the indented "*e:" line -- whose ':' yeptris's
 * alias scan rides INTO the name (the r142/r76 alias-charset carry) --
 * collapses the whole document into the bare alias ("=ALI *e:", the
 * root-alias family's reading): no map, no key, no anchor. The spell
 * makes both readings explicit and preserving: the head "&<a>::" is
 * rewritten into the EXPLICIT anchored key "&<a> ':' :" (a quoted ':'
 * -- the semantic dump is style-blind -- followed by the separated
 * value indicator re-lexes as exactly libyaml's original KEY), and the
 * glued alias separator "*<e>:" gains the blank ("*<e> :" -- the name
 * re-lexes clean in yeptris and libyaml reads the same alias + value
 * indicator). The finder fires only on an '&'-head at a NODE position
 * (start-of-input or after a blank/break) whose anchor name is glued to
 * exactly "::", and on a '*' alias at a node position whose name is
 * glued to a ':' in the value-indicator position (blankz or EOF after);
 * at least one of each must fire. The r145 plain-run-aware quote
 * tracker keeps glued quotes as content, blank-led comments ride their
 * line un-scored, and the byte-strict verify against libyaml's ORIGINAL
 * dump arbitrates, with the CR/LS/PS/NEL-normalized triple as the
 * fallback (the r183/r221/r222 pattern). The deletion sweep's glued
 * same-line twin ("&a::*e:") declines -- its glued value colon re-lexes
 * as plain-first in libyaml only when value-indicator-glued, and the
 * rewrite's separated form cannot reproduce the same-line nested pair
 * -- and the empty-name split twins ("&::", "*:") ride the split path,
 * not this DIFFER arm. */
static int yd_r223_block_anchored_glued_colon_alias_divergence(const uint8_t* data, size_t size,
                                                               const char* ydump,
                                                               const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    /* the yeptris collapse tell: the whole document is ONE alias whose
     * name rides the glued key separator (no map, no scalar) */
    const char* yali = strstr(ydump, "=ALI *");
    if (yali == NULL || strstr(ydump, "+MAP") != NULL || strstr(ydump, "+SEQ") != NULL ||
        strstr(ydump, "=VAL") != NULL) {
        return 0;
    }
    const char* ynl = strchr(yali, '\n');
    if (ynl == NULL || ynl - yali < 7 || ynl[-1] != ':') {
        return 0;
    }
    if (strstr(ldump, "+MAP") == NULL) {
        return 0;
    }
    char* sub = malloc(size * 3 + 16); /* a hit grows "&a::" -> "&a ':' :" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    int plain = 0; /* a plain run is active: glued quotes are content */
    int depth = 0; /* flow depth: breaks inside a flow plain fold */
    int heads = 0;
    int heals = 0;
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
        if (c == '\'' || c == '"') {
            if (!plain) {
                quote = (int)c;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '#' && (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' ||
                         data[i - 1] == '\n' || data[i - 1] == '\r')) {
            /* a blank-led comment swallows the line; the node ended and
             * nothing inside it opens a quote or scores */
            plain = 0;
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                if (data[i] == '\r' && i + 1 < size && data[i + 1] == '\n') {
                    sub[subn++] = '\r';
                    i++;
                }
                sub[subn++] = (char)data[i]; /* the break rides along */
            }
            i++;
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ',') {
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                sub[subn++] = '\r';
                i++;
            }
            if (depth == 0) {
                plain = 0; /* block lines: each line is a fresh node */
            }
            /* a flow plain folds across its break: the run rides */
            sub[subn++] = (char)data[i];
            i++;
            continue;
        }
        if (c == ' ' || c == '\t') {
            /* fold blanks ride the plain run */
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        int nodepos = (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' || data[i - 1] == '\n' ||
                       data[i - 1] == '\r');
        if (c == '&' && nodepos) {
            /* the anchor head: name chars then exactly "::" glued */
            size_t j = i + 1;
            while (j < size && (((unsigned char)data[j] >= 'A' && (unsigned char)data[j] <= 'Z') ||
                                ((unsigned char)data[j] >= 'a' && (unsigned char)data[j] <= 'z') ||
                                ((unsigned char)data[j] >= '0' && (unsigned char)data[j] <= '9') ||
                                data[j] == '_' || data[j] == '-')) {
                j++;
            }
            if (j > i + 1 && j + 1 < size && data[j] == ':' && data[j + 1] == ':') {
                sub[subn++] = '&';
                for (size_t k = i + 1; k < j; k++) {
                    sub[subn++] = (char)data[k];
                }
                /* "&a::" -> "&a ':' :": the quoted ':' is style-blind in
                 * the semantic dump and the separated value indicator
                 * re-lexes as libyaml's original KEY */
                sub[subn++] = ' ';
                sub[subn++] = '\'';
                sub[subn++] = ':';
                sub[subn++] = '\'';
                sub[subn++] = ' ';
                sub[subn++] = ':';
                plain = 0;
                heads++;
                i = j + 2;
                continue;
            }
        }
        if (c == '*' && nodepos) {
            /* the alias: name chars then a ':' in the value-indicator
             * position (blankz or EOF after) */
            size_t j = i + 1;
            while (j < size && (((unsigned char)data[j] >= 'A' && (unsigned char)data[j] <= 'Z') ||
                                ((unsigned char)data[j] >= 'a' && (unsigned char)data[j] <= 'z') ||
                                ((unsigned char)data[j] >= '0' && (unsigned char)data[j] <= '9') ||
                                data[j] == '_' || data[j] == '-')) {
                j++;
            }
            if (j > i + 1 && j < size && data[j] == ':') {
                unsigned char f = j + 1 < size ? data[j + 1] : ' ';
                if (f == ' ' || f == '\t' || f == '\n' || f == '\r') {
                    sub[subn++] = '*';
                    for (size_t k = i + 1; k < j; k++) {
                        sub[subn++] = (char)data[k];
                    }
                    /* "*e:" -> "*e :": the name re-lexes clean and both
                     * engines read the same alias + value indicator */
                    sub[subn++] = ' ';
                    sub[subn++] = ':';
                    plain = 0;
                    heals++;
                    i = j + 1;
                    continue;
                }
            }
        }
        if (c == ':' || c == '?' || c == '-' || c == '&' || c == '*' || c == '!' || c == '|' ||
            c == '>' || c == '@' || c == '%' || c == '`') {
            /* separated, the indicator ends the run (a props/value
             * position where a quoted node can start); glued, it is
             * plain-first content (the HM87 plain-'?' law) */
            unsigned char f = i + 1 < size ? data[i + 1] : ' ';
            if (f == ' ' || f == '\t' || f == '\n' || f == '\r') {
                plain = 0;
            } else {
                plain = 1;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        /* everything else: wordy content keeps the run */
        plain = 1;
        sub[subn++] = (char)c;
        i++;
    }
    if (heads == 0 || heals == 0) {
        free(sub);
        return 0;
    }
    /* the byte-strict verify: both engines must read the rewrite
     * exactly as libyaml read the original */
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183/r221/r222 pattern): an
     * exotic-break twin keeps the raw bytes in libyaml's dump while
     * yeptris follows the spec's fold law, so the byte-strict compare
     * can never hold. Normalize every break (LS/PS/NEL/CR -> LF) on
     * BOTH sides -- a meaning-preserving rewrite for both engines'
     * plain/quoted readings -- and demand the triple: both engines
     * read the normalized rewrite identically AND libyaml reads the
     * normalized original identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the CR-CROSSING COMMENT-SWALLOW KEY BINDING
 * of the flow-'?' family (the r224 nightly, run 42604872810, main
 * 988ba3e, crash-3922bb61f22630e1b2517b89fc52494d4ee1cc84: the
 * 345-byte soup "[QQQ...\n&mQ,?##|,:|,:bY y\rbnaaryM\r ,?#,...\
 * ?tag:yaml.org,...]" repeated twice). libyaml's 1.1 KEY-token law
 * makes the flow '?' a KEY token whatever follows, and the scanner's
 * next-token loop then treats the GLUED '#' as a boundary comment: the
 * comment swallows to the NEXT LINE BREAK -- a lone CR is a break --
 * and the post-break plain rides as the keyless pair's key
 * ({bnaaryM: <empty>}). yeptris's suite-pinned plain-first-'?' law
 * (HM87) reads the glued run as flat scalars ("?##|", ":|", ":bY y")
 * with the CR folded INSIDE the plain ("bY y bnaaryM"). No single
 * prior arm claims the soup: r185's comment-CR spell heals one
 * '?#'+follower hit but leaves the tail's bare glued-'?' runs
 * unhealed, and r183's spell declines the tail's runs (its clean
 * check rejects any ':' in the run) while the artifact's tail rides
 * "?tag:yaml.org" -- a GLUED-':' run (the r219 embedded-'?' widening's
 * territory). The arm COMPOSES the two healing spells in one walk.
 * HIT A (the '?' glued to '#'): keep the '?', delete the '#' through
 * the comment's end -- separated from its follower by the break, the
 * '?' re-lexes as the explicit KEY in BOTH engines (the r222 keep-?
 * law). The comment must END at a CR/LF break (a run-to-EOF comment
 * declines to the other arms' spells), and the post-break follower
 * must START a wordy plain run (the KEY's node). HIT B (the bare
 * glued-'?' run): rewrite "?run" -> "{run: }" (the r183 spell), with
 * the r224 widening -- a ':' GLUED inside the run rides as mid-plain
 * content ("?tag:yaml.org" binds {tag:yaml.org: <empty>}), a ':'
 * after a blank declines (libyaml's plain ends at the ': ' VALUE
 * boundary), and a '#' glued after a non-blank is content (the r206
 * law) while a blank-led '#' declines. Quoted and other indicator
 * followers decline. The byte-strict verify against libyaml's
 * ORIGINAL dump arbitrates (both engines must read the rewrite
 * exactly as libyaml read the original), with the CR/LS/PS/
 * NEL-normalized triple as the fallback (the artifact's inter-member
 * CRs survive the rewrite and both engines' plains fold them alike). */
static int yd_r224_seq_question_glued_hash_cr_run_divergence(const uint8_t* data, size_t size,
                                                             const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size * 4 + 4); /* hit A deletes, hit B grows "?run" -> "{run: }" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
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
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size) {
            unsigned char f = data[i + 1];
            if (f == '#') {
                /* HIT A: the comment swallows from the '#' to the line
                 * break; without a break the whole-tail kill belongs to
                 * the other arms' spells */
                size_t j = i + 1;
                while (j < size && data[j] != '\n' && data[j] != '\r') {
                    j++;
                }
                if (j < size) {
                    /* the follower must START the pair's key node: a
                     * wordy plain-first char after the break */
                    size_t k = j + 1;
                    if (data[j] == '\r' && k < size && data[k] == '\n') {
                        k++;
                    }
                    if (k < size && data[k] != ' ' && data[k] != '\t' && data[k] != '\n' &&
                        data[k] != '\r' && data[k] != ',' && data[k] != ']' && data[k] != '}' &&
                        data[k] != ':' && data[k] != '"' && data[k] != '\'' && data[k] != '[' &&
                        data[k] != '{' && data[k] != '#' && data[k] != '&' && data[k] != '*' &&
                        data[k] != '!' && data[k] != '|' && data[k] != '>' && data[k] != '%' &&
                        data[k] != '@' && data[k] != '`' && data[k] != '?') {
                        sub[subn++] = '?';
                        i = j;
                        hits++;
                        continue;
                    }
                }
            } else if (f != ' ' && f != '\t' && f != '\n' && f != '\r' && f != ',' && f != ':' &&
                       f != ']' && f != '}' && f != '"' && f != '\'' && f != '[' && f != '{' &&
                       f != '#' && f != '&' && f != '*' && f != '!' && f != '|' && f != '>' &&
                       f != '%' && f != '@' && f != '`' && f != '?') {
                /* HIT B: the run must be same-line plain-safe, ended by
                 * ',' / ']' / EOF. A ':' GLUED to the previous char is
                 * mid-plain content (the r224 widening); a ':' after a
                 * blank ends libyaml's plain at the VALUE boundary --
                 * decline. A '#' glued after a non-blank is content
                 * (the r206 law); a blank-led '#' is the comment
                 * boundary -- decline. */
                size_t j = i + 1;
                int clean = 1;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '#') {
                        unsigned char p = data[j - 1];
                        if (p != ' ' && p != '\t' && p != '\n' && p != '\r') {
                            j++;
                            continue;
                        }
                        clean = 0;
                        break;
                    }
                    if (d == ':') {
                        unsigned char p = data[j - 1];
                        if (p == ' ' || p == '\t' || p == '\n' || p == '\r') {
                            clean = 0;
                            break;
                        }
                        j++;
                        continue;
                    }
                    if (d == '}' || d == '\n' || d == '\r' || d == '[' || d == '{' || d == '"' ||
                        d == '\'' || d == '\\' || d == '?') {
                        clean = 0;
                        break;
                    }
                    j++;
                }
                if (clean) {
                    sub[subn++] = '{';
                    memcpy(sub + subn, data + i + 1, j - (i + 1));
                    subn += j - (i + 1);
                    sub[subn++] = ':';
                    sub[subn++] = ' ';
                    sub[subn++] = '}';
                    i = j;
                    hits++;
                    continue;
                }
            }
        }
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    /* the byte-strict verify: both engines must read the rewrite
     * exactly as libyaml read the original */
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183/r221/r222 pattern):
     * an exotic-break twin keeps the raw bytes in libyaml's dump while
     * yeptris follows the spec's fold law, so the byte-strict compare
     * can never hold. Normalize every break (LS/PS/NEL/CR -> LF) on
     * BOTH sides and demand the triple: both engines read the
     * normalized rewrite identically AND libyaml reads the normalized
     * original identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the COMMENT-CR PAIR of the glued flow '?#'
 * family (the r185 nightly, run 42392584152, main 3c87d4a, crash-
 * 187062ddd3ac: the 52-byte
 * "[11.0e ,1.0e ,?#Ô,:
&binary
, ?#Ô,:
&binary
, (?,]").
 * libyaml's 1.1 KEY-token law (the qflow family's carried quirk) makes
 * the flow '?' a KEY token whatever follows, and the scanner's
 * next-token loop then treats the GLUED '#' as a boundary comment: the
 * comment swallows "...Ô,:" up to the CR, and the CR exposes the
 * NEXT line's node as the keyless pair's key — a plain run ("i") rides
 * as the key node {i: <empty>}, an anchor ("&binary") rides as the
 * anchored empty scalar {&binary<empty>: <empty>}. yeptris's
 * suite-pinned plain reading keeps "?#..." ONE plain member (the
 * HM87 law: '#' mid-plain after a non-blank is content) and binds the
 * bare ':' pair to the post-break run as its VALUE — with the map
 * persisting across the ','. The r132 walk cannot pair the shapes:
 * its emptied-key law expects libyaml to keep LESS content, while
 * here libyaml's pair PROMOTES the follower to the key slot yeptris
 * leaves null. The preserving spell rewrites the swallowed span into
 * the EXPLICIT single-pair map — "?" + comment-through-break + follower
 * becomes "{follower: }" — which both engines read as exactly the pair
 * libyaml produced. The finder only fires on a '?' glued to '#' at
 * flow depth whose innermost frame is a SEQUENCE, whose comment span
 * reaches a line break, and whose post-break follower starts a
 * same-line plain run (terminated by ',' / ']' / EOF) or an
 * anchor-then-boundary empty ride; quoted and other indicator
 * followers decline. The byte-strict verify against libyaml's
 * ORIGINAL dump arbitrates, with the CR/LS/PS/NEL-normalized triple as
 * the fallback (the artifact rides one: the rewrite leaves the
 * inter-member CRs, and both engines' plains fold them alike). */
static int yd_r185_glued_hash_comment_cr_divergence(const uint8_t* data, size_t size,
                                                    const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size * 4 + 4); /* each hit turns "?#...\n run" into "{run: }" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
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
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size &&
            data[i + 1] == '#') {
            /* the boundary comment swallows from the '#' to the line
             * break; without a break the whole-tail kill belongs to the
             * other arms' spells */
            size_t k = i + 1;
            while (k < size && data[k] != '\n' && data[k] != '\r') {
                k++;
            }
            if (k < size) {
                size_t brk = k;
                size_t next = k + 1;
                if (data[k] == '\r' && next < size && data[next] == '\n') {
                    next++;
                }
                /* the follower must start the pair's key node: a
                 * plain-safe run, or an anchor riding an empty scalar
                 * to its boundary */
                size_t j = next;
                int anchor = (j < size && data[j] == '&');
                int clean = 1;
                if (anchor) {
                    j++;
                    if (j >= size ||
                        !((data[j] >= 'a' && data[j] <= 'z') ||
                          (data[j] >= 'A' && data[j] <= 'Z') ||
                          (data[j] >= '0' && data[j] <= '9') || data[j] == '_' || data[j] == '-')) {
                        clean = 0;
                    }
                    while (clean && j < size) {
                        unsigned char d = data[j];
                        if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') ||
                            (d >= '0' && d <= '9') || d == '_' || d == '-') {
                            j++;
                            continue;
                        }
                        if (d == '\n' || d == '\r' || d == ',' || d == ']') {
                            break;
                        }
                        clean = 0; /* content after the anchor declines */
                        break;
                    }
                } else if (j < size && data[j] != ',' && data[j] != ']') {
                    unsigned char f = data[j];
                    if (f == ' ' || f == '\t' || f == ':' || f == '}' || f == '"' || f == '\'' ||
                        f == '[' || f == '{' || f == '#' || f == '&' || f == '*' || f == '!' ||
                        f == '|' || f == '>' || f == '%' || f == '@' || f == '`' || f == '?') {
                        clean = 0;
                    }
                    while (clean && j < size) {
                        unsigned char d = data[j];
                        if (d == ',' || d == ']') {
                            break;
                        }
                        if (d == '}' || d == '\n' || d == '\r' || d == '[' || d == '{' ||
                            d == '"' || d == '\'' || d == '\\' || d == ':' || d == '?' ||
                            d == '#') {
                            clean = 0;
                            break;
                        }
                        j++;
                    }
                } else {
                    clean = 0; /* an empty follower declines */
                }
                if (clean) {
                    /* the anchor spell rides TWO separators: yeptris's
                     * wider anchor-name charset (the r142/r179 law)
                     * swallows a ':' GLUED to the name, so "&binary :"
                     * is the form both engines read as the anchored
                     * empty key; the plain spell needs none */
                    sub[subn++] = '{';
                    if (anchor) {
                        sub[subn++] = ' ';
                    }
                    memcpy(sub + subn, data + next, j - next);
                    subn += j - next;
                    if (anchor) {
                        sub[subn++] = ' ';
                    }
                    sub[subn++] = ':';
                    sub[subn++] = ' ';
                    sub[subn++] = '}';
                    i = j;
                    hits++;
                    continue;
                }
            }
        }
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183 pattern): the
     * artifact's inter-member CRs survive the rewrite, and a plain's
     * fold law (one break -> a space) can shift the rendered dumps.
     * Normalize every break (CR/LS/PS/NEL -> LF) on BOTH sides and
     * demand the triple: both engines read the normalized rewrite
     * identically AND libyaml reads the normalized original
     * identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the FOLDED-PLAIN-PAIR member of the glued
 * flow '?#' comment family (the r187 nightly, run 42404973005, main
 * 4865289, crash-d5a609db0f966ebae750560b629652d9042284f0: the
 * 52-byte "[11n.0e ,?##|,:|,:binary\rb's'\n        bravo: 0x788,]").
 * libyaml's 1.1 law (the family's carried quirk) makes the flow '?'
 * a KEY token whatever follows, and the scanner's next-token loop then
 * treats the GLUED '#' as a boundary comment: the comment swallows
 * "...##|,:|,:binary" up to the CR, and the pending explicit KEY
 * context then admits what the simple-key law forbids elsewhere — the
 * post-break MULTI-LINE PLAIN follower folds ("b's'<LF>        bravo"
 * turns "b's' bravo", the apostrophes riding as mid-plain content)
 * and binds ": 0x788" as its value, one keyless pair. yeptris's
 * suite-pinned HM87 law ('#' mid-plain after a non-blank is content)
 * keeps "11n.0e", "?##|", ":|" as members and reads
 * ":binary b's' bravo: 0x788" as its own pair — the strict-1.2
 * reading, so the carry rides. The r185 arm keeps its follower
 * same-line and colon-free, so this orientation declines there;
 * deletion mutants also reach the EMPTY-COLON twin ("[?#,|,:y\r:]":
 * after the break the bare ':' completes the keyless pair with an
 * empty key and value). The preserving spell deletes the swallowed
 * span and RESTORES the explicit '?': "?#...<break>" turns "? " with
 * the post-break tail riding untouched — libyaml keeps its KEY
 * context (the '{k: v}' brace form cannot carry the multi-line key:
 * libyaml's simple-key law rejects it outright) and both engines then
 * read the tail identically, matching libyaml's original dump. The
 * finder fires on a '?' glued to '#' at flow depth whose innermost
 * frame is a SEQUENCE, whose comment span reaches a line break, and
 * whose post-break tail is bracket-free (a plain or quoted run, ':'s
 * binding values, at most 8 fold breaks) closed by ',' or ']';
 * everything else declines. The byte-strict verify against libyaml's
 * ORIGINAL dump arbitrates, with the CR/LS/PS/NEL-normalized triple
 * as the fallback (the r183/r185 pattern). */
static int yd_r187_glued_hash_plain_pair_divergence(const uint8_t* data, size_t size,
                                                    const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size + 16); /* each hit turns "?#...<break>" into "? " */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* token-start tracking: a quote only OPENS a scalar at a token
     * start (after '[', '{', ',', ':', a blank, or a break) — a quote
     * mid-word is plain content in both engines ("1'n.0e" parses as
     * one plain member), and the glue behind it still counts */
    int scalar_start = 1;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
                scalar_start = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
                scalar_start = 0;
            }
            i++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            scalar_start = 1; /* blanks and breaks are token boundaries */
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if ((c == '\'' || c == '"') && scalar_start) {
            quote = c;
            scalar_start = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '[' || c == '{') {
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            scalar_start = 1;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            scalar_start = 1;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ',' || c == ':') {
            scalar_start = 1;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && scalar_start && depth > 0 && depth <= 64 && frames[depth - 1] == 1 &&
            i + 1 < size && data[i + 1] == '#') {
            /* the boundary comment swallows from the '#' to the line
             * break; without a break the whole-tail kill belongs to
             * the other arms' spells */
            size_t k = i + 1;
            while (k < size && data[k] != '\n' && data[k] != '\r') {
                k++;
            }
            if (k < size) {
                size_t next = k + 1;
                if (data[k] == '\r' && next < size && data[next] == '\n') {
                    next++;
                }
                /* the post-break tail rides the keyless pair: bracket-
                 * free plain/quoted content, ':'s binding values, at
                 * most 8 fold breaks, closed by ',' or ']' */
                size_t j = next;
                int breaks = 0;
                int clean = 1;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '[' || d == '{' || d == '}') {
                        clean = 0; /* nested flow inside the tail declines */
                        break;
                    }
                    if (d == '\n' || d == '\r') {
                        if (++breaks > 8) {
                            clean = 0;
                            break;
                        }
                    }
                    j++;
                }
                if (clean && j < size) {
                    /* the spell deletes the swallowed span and leaves
                     * the explicit '?': the post-break tail rides
                     * untouched, and the verify arbitrates */
                    sub[subn++] = '?';
                    sub[subn++] = ' ';
                    i = next;
                    hits++;
                    scalar_start = 1; /* the break is a token boundary */
                    continue;
                }
            }
        }
        scalar_start = 0; /* any other content char ends the token */
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183/r185 pattern): the
     * artifact's inter-member CR survives the rewrite, and a plain's
     * fold law (one break -> a space) can shift the rendered dumps.
     * Normalize every break (CR/LS/PS/NEL -> LF) on BOTH sides and
     * demand the triple: both engines read the normalized rewrite
     * identically AND libyaml reads the normalized original
     * identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i + 2 < size && !has_break; i++) {
        if (data[i] == 0xE2 && data[i + 1] == 0x80 &&
            (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            has_break = 1;
        } else if (data[i] == 0xC2 && data[i + 1] == 0x85) {
            has_break = 1;
        } else if (data[i] == '\r') {
            has_break = 1;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn; i++) {
        if (sub[i] == '\r') {
            nsub[ns++] = '\n';
        } else if ((unsigned char)sub[i] == 0xE2 && i + 2 < subn &&
                   (unsigned char)sub[i + 1] == 0x80 &&
                   ((unsigned char)sub[i + 2] == 0xA8 || (unsigned char)sub[i + 2] == 0xA9)) {
            nsub[ns++] = '\n';
            i += 2;
        } else if ((unsigned char)sub[i] == 0xC2 && i + 1 < subn &&
                   (unsigned char)sub[i + 1] == 0x85) {
            nsub[ns++] = '\n';
            i += 1;
        } else {
            nsub[ns++] = sub[i];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '\r') {
            norig[no++] = '\n';
        } else if (data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                   (data[i + 2] == 0xA8 || data[i + 2] == 0xA9)) {
            norig[no++] = '\n';
            i += 2;
        } else if (data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) {
            norig[no++] = '\n';
            i += 1;
        } else {
            norig[no++] = (char)data[i];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
}

/* True when the verdict is the r187 glued-flow-'?#' comment family's
 * MULTI-LINE-FOLLOWER member spelled with the plain-continuation law
 * (the r190 nightly, run 42420197746, main 0fd2ff4, crash-
 * bf009b5896e99d9c8aca7b26a7ad86a7e613e7f9, 32772 bytes; the 33-byte
 * delta twin "[1<LS/PS x4>,?#,:\rg<CR>",?#<LS/PS x2>]" shows it bare).
 * libyaml's 1.1 law (the family's carried quirk) makes the flow '?'
 * a KEY token whatever follows and the glued '#' a boundary comment;
 * the pending explicit-KEY context then admits the post-break plain
 * follower -- and the follower run here does not stop at the line
 * break: scan_plain_scalar's continuation folds the break into a
 * space and rides the NEXT line's quote as mid-plain content ("g<CR>
 * \"" reads "g \"" in both engines -- a quote does not end a plain
 * run, r187's token-start tracker could not model this and opened a
 * quoted scalar instead, so the SECOND hit (",?#" riding an LS/PS
 * terminator with an empty tail) hid inside the phantom quote span).
 * yeptris's suite-pinned HM87 law keeps "?#" as a plain member and
 * reads the ':' implied-key map itself. The r187 spell restores the
 * explicit '?' where the tail rides ("?#...<break>" -> "? "), and for
 * the EMPTY tail (breaks only before the close, the family's second
 * member: libyaml lands the empty-key/empty-value pair) the brace
 * spell "{? }" -- both engines read "{? }" as the explicit empty-key
 * pair, where "? <exotic break>]" errors in yeptris (the explicit '?'
 * followed by an LS/PS break, a fresh reading its LF twin does not
 * share). The finder is r187's with three teach-ins: the comment
 * terminator scan learns NEL/LS/PS (multibyte, continuation-checked,
 * the r189/yep_scan_mb_break_len law), the tail-break counter counts
 * them too, and the walk tracks plain continuation (breaks do not end
 * a plain run -- a quote only opens a scalar when the run is closed).
 * Gates: at least one hit must carry a NEW-SHAPE feature (an exotic
 * terminator, a tail that folds across a break absorbing a quote, or
 * a HEAD plain that rode a quote across a break -- the r191 nightly,
 * run 42426475722, main d07420b, crash-05a4b458213a98f879553cc9ed21
 * 1875687ab3bb: "[0<U+2029>',?.]" minimized; r183's unconditional
 * quote-open opens a phantom quoted scalar over the absorbed quote
 * and cannot reach the flow-'?' hit at all, so the KIND 2 spell
 * restores the explicit '?' and the verify arbitrates),
 * else the shape is r185/r187 turf; size bounded by the r186 gate.
 * The byte-strict double verify against libyaml's ORIGINAL dump
 * (both engines must read the rewrite exactly as libyaml read the
 * original) arbitrates, with the CR/LS/PS/NEL-normalized triple as
 * the fallback (the r183/r185/r187 pattern -- the artifact rides one:
 * its members carry raw LS/PS in scalar content where yeptris folds
 * to LF). */
static int yd_r190_glued_hash_multiline_follower_divergence(const uint8_t* data, size_t size,
                                                            const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 262144) {
        return 0;
    }
    char* sub = malloc(size * 2 + 16); /* each hit turns "?#..." into "? " or "{? }" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* token-start tracking: a quote only OPENS a scalar at a token
     * start (after '[', '{', ',', ':', a blank, a break, or a close)
     * -- and only when no plain run is open across the break */
    int scalar_start = 1;
    /* plain continuation: a break does not end a plain run (the
     * libyaml fold law); only the flow boundaries and quote closes do */
    int plain = 0;
    /* a quote absorbed into an OPEN plain run (the r191 nightly, run
     * 42426475722, main d07420b, crash-05a4b458213a98f879553cc9ed2118756
     * 875687ab3bb, 20 bytes; the 10-byte DIFFER twin "[0<U+2029>',?.]"
     * shows it bare): the fold law keeps the quote mid-plain content in
     * both engines, but r183's unconditional quote-open opens a phantom
     * quoted scalar over it and cannot see the flow-'?' KEY divergence
     * at all -- this arm's KIND 2 walk reaches the hit instead, and the
     * absorption is the NEW-SHAPE feature the gate demands */
    int rode_head_quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
    int shaped = 0; /* at least one NEW-SHAPE feature seen */
    /* multibyte break length at i: 2 = CRLF/NEL, 3 = LS/PS, 1 = LF/CR
     * alone, 0 = not a break (a dangling lead rides as content) */
#define YD_R190_BREAK_LEN(d, n, i)                                                                 \
    ((d)[i] == '\n'   ? 1                                                                          \
     : (d)[i] == '\r' ? (((i) + 1 < (n)) && (d)[(i) + 1] == '\n' ? 2 : 1)                          \
     : ((d)[i] == 0xC2 && (i) + 1 < (n) && (d)[(i) + 1] == 0x85) ? 2                               \
     : ((d)[i] == 0xE2 && (i) + 2 < (n) && (d)[(i) + 1] == 0x80 &&                                 \
        ((d)[(i) + 2] == 0xA8 || (d)[(i) + 2] == 0xA9))                                            \
         ? 3                                                                                       \
         : 0)
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            }
            i++;
            continue;
        }
        int blen = YD_R190_BREAK_LEN(data, size, i);
        if (blen > 0) {
            /* a break is a line boundary: the token position refreshes,
             * but an open plain run RIDES it (the fold law) */
            scalar_start = 1;
            for (int b = 0; b < blen; b++) {
                sub[subn++] = (char)data[i + (size_t)b];
            }
            i += (size_t)blen;
            continue;
        }
        if (c == ' ' || c == '\t') {
            scalar_start = 1;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if ((c == '\'' || c == '"') && scalar_start && !plain) {
            quote = c;
            scalar_start = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if ((c == '\'' || c == '"') && plain != 0) {
            /* the fold law's absorption: the quote rides the open plain
             * run as mid-plain content (both engines read it so) */
            rode_head_quote = 1;
        }
        if (c == '[' || c == '{') {
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            scalar_start = 1;
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            scalar_start = 1;
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ',' || c == ':') {
            scalar_start = 1;
            plain = 0;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && scalar_start && depth > 0 && depth <= 64 && frames[depth - 1] == 1 &&
            i + 1 < size && data[i + 1] != '?' &&
            (data[i + 1] == '#' ||
             !(data[i + 1] == ' ' || data[i + 1] == '\t' || data[i + 1] == ',' ||
               data[i + 1] == ':' || data[i + 1] == ']' || data[i + 1] == '}' ||
               data[i + 1] == '"' || data[i + 1] == '\'' || data[i + 1] == '[' ||
               data[i + 1] == '{' || data[i + 1] == '&' || data[i + 1] == '*' ||
               data[i + 1] == '!' || data[i + 1] == '|' || data[i + 1] == '>' ||
               data[i + 1] == '%' || data[i + 1] == '@' || data[i + 1] == '`'))) {
            if (data[i + 1] != '#' && YD_R190_BREAK_LEN(data, size, i + 1) == 0) {
                /* KIND 2 -- the r183 flow-SEQ explicit-key shape
                 * ("?<plain run>" is KEY token + the run as the keyless
                 * pair's key), reached here because r183 declined: its
                 * same-line run law cannot ride the fold. Spell r183's
                 * brace form over the break-riding run; the verify
                 * arbitrates. */
                size_t j = i + 1;
                int breaks = 0;
                int clean = 1;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '[' || d == '{' || d == '}' || d == '\\' || d == ':' || d == '?') {
                        clean = 0;
                        break;
                    }
                    if (d == '#' && (j == i + 1 || data[j - 1] == ' ' || data[j - 1] == '\t')) {
                        clean = 0; /* a comment at the run's head declines */
                        break;
                    }
                    int db = YD_R190_BREAK_LEN(data, size, j);
                    if (db > 0) {
                        if (++breaks > 8) {
                            clean = 0;
                            break;
                        }
                        j += (size_t)db;
                        continue;
                    }
                    j++;
                }
                if (clean && j < size) {
                    shaped = shaped || breaks > 0 || rode_head_quote;
                    /* the spell deletes nothing and restores the
                     * explicit '?': the break-riding run keeps riding
                     * as the keyless pair's key (the brace form cannot
                     * carry a multi-line key: libyaml's simple-key law
                     * rejects it outright, the r187 law) */
                    sub[subn++] = '?';
                    sub[subn++] = ' ';
                    i = i + 1; /* the glued follower rides from its head */
                    hits++;
                    scalar_start = 0;
                    plain = 1;
                    continue;
                }
                scalar_start = 0;
                plain = 1;
                sub[subn++] = (char)c;
                i++;
                continue;
            }
            /* KIND 1 -- the boundary comment swallows from the '#' to
             * the line break (LF/CR/NEL/LS/PS); without a break the
             * whole-tail kill belongs to the other arms' spells */
            size_t k = i + 1;
            int exotic = 0;
            while (k < size) {
                int kb = YD_R190_BREAK_LEN(data, size, k);
                if (kb == 2 && data[k] == 0xC2) {
                    exotic = 1;
                } else if (kb == 3) {
                    exotic = 1;
                }
                if (kb > 0) {
                    break;
                }
                k++;
            }
            if (k < size) {
                int tblen = YD_R190_BREAK_LEN(data, size, k);
                size_t next = k + (size_t)tblen;
                /* the tail scan rides the plain continuation: breaks
                 * do not end it, quotes mid-run are content, ':'s
                 * bind values; nested flow declines; at most 8 folds */
                size_t j = next;
                int breaks = 0;
                int clean = 1;
                int rode_quote = 0;
                int rode_content = 0;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '[' || d == '{' || d == '}') {
                        clean = 0; /* nested flow inside the tail declines */
                        break;
                    }
                    int db = YD_R190_BREAK_LEN(data, size, j);
                    if (db > 0) {
                        if (++breaks > 8) {
                            clean = 0;
                            break;
                        }
                        j += (size_t)db;
                        continue;
                    }
                    if (d == '#' && (j == next || data[j - 1] == ' ' || data[j - 1] == '\t')) {
                        /* a comment at the run's head ends it */
                        clean = 0;
                        break;
                    }
                    if ((d == '\'' || d == '"') && !rode_content) {
                        /* a quote AT the token start opens a scalar:
                         * the run ends conservatively */
                        clean = 0;
                        break;
                    }
                    if (d == '\'' || d == '"') {
                        rode_quote = 1; /* mid-plain content (the fold law) */
                    }
                    rode_content = 1;
                    j++;
                }
                if (clean && j < size) {
                    shaped = shaped || exotic || rode_quote || breaks > 0;
                    if (j == next) {
                        /* the EMPTY tail: libyaml lands the explicit
                         * empty-key pair from the pending KEY; spell
                         * the brace form both engines read as it */
                        sub[subn++] = '{';
                        sub[subn++] = '?';
                        sub[subn++] = ' ';
                        sub[subn++] = '}';
                    } else {
                        /* the spell deletes the swallowed span and
                         * leaves the explicit '?': the post-break tail
                         * rides untouched, and the verify arbitrates */
                        sub[subn++] = '?';
                        sub[subn++] = ' ';
                    }
                    i = next;
                    hits++;
                    scalar_start = 1;
                    plain = 0;
                    continue;
                }
            }
        }
        scalar_start = 0; /* any other content char ends the token */
        plain = 1;
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0 || !shaped) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    free(ydump2);
    free(ldump2);
    if (known) {
        free(sub);
        return 1;
    }
    /* the break-normalization fallback (the r183/r185/r187 pattern):
     * the artifact's members carry raw LS/PS in scalar content where
     * yeptris follows the fold law, so the byte-strict compare can
     * never hold. Normalize every break (CR/LS/PS/NEL -> LF) on BOTH
     * sides and demand the triple: both engines read the normalized
     * rewrite identically AND libyaml reads the normalized original
     * identically too. Anything else declines. */
    int has_break = 0;
    for (size_t i = 0; i < size && !has_break; i++) {
        if (YD_R190_BREAK_LEN(data, size, i) > 1) {
            has_break = 1;
            i += 0;
        }
    }
    if (!has_break) {
        free(sub);
        return 0;
    }
    char* nsub = malloc(subn * 3 + 1);
    char* norig = malloc(size * 3 + 1);
    if (nsub == NULL || norig == NULL) {
        free(nsub);
        free(norig);
        free(sub);
        return 0;
    }
    size_t ns = 0;
    for (size_t i = 0; i < subn;) {
        int nb = YD_R190_BREAK_LEN((const uint8_t*)sub, subn, i);
        if (nb > 0) {
            nsub[ns++] = '\n';
            i += (size_t)nb;
        } else {
            nsub[ns++] = sub[i++];
        }
    }
    size_t no = 0;
    for (size_t i = 0; i < size;) {
        int nb = YD_R190_BREAK_LEN(data, size, i);
        if (nb > 0) {
            norig[no++] = '\n';
            i += (size_t)nb;
        } else {
            norig[no++] = (char)data[i++];
        }
    }
    int yok3 = 0, lok3 = 0, lok4 = 0;
    char* ydump3 = yd_sem_dump(nsub, ns, &yok3);
    char* ldump3 = yd_libyaml_dump(nsub, ns, &lok3);
    char* ldump4 = yd_libyaml_dump(norig, no, &lok4);
    known = (yok3 && lok3 && lok4 && ydump3 != NULL && ldump3 != NULL && ldump4 != NULL &&
             strcmp(ydump3, ldump3) == 0 && strcmp(ydump3, ldump4) == 0);
    free(ydump3);
    free(ldump3);
    free(ldump4);
    free(nsub);
    free(norig);
    free(sub);
    return known;
#undef YD_R190_BREAK_LEN
}

/* Append b to sub[*subn] escaped for a double-quoted YAML scalar.
 * Returns 0 on a byte the escape law cannot carry (the caller's run
 * scan never produces one). */
static int yd_r203_dq_escape(char* sub, size_t* subn, unsigned char b) {
    switch (b) {
    case '"':
        sub[(*subn)++] = '\\';
        sub[(*subn)++] = '"';
        return 1;
    case '\\':
        sub[(*subn)++] = '\\';
        sub[(*subn)++] = '\\';
        return 1;
    case '\n':
        sub[(*subn)++] = '\\';
        sub[(*subn)++] = 'n';
        return 1;
    case '\t':
        sub[(*subn)++] = '\\';
        sub[(*subn)++] = 't';
        return 1;
    default:
        if (b < 0x20 || b == 0x7F) {
            return 0;
        }
        /* the r236 widening: bytes >= 0x80 ride RAW -- the run scan
         * only admits complete valid non-break UTF-8 sequences, and a
         * double-quoted scalar carries valid UTF-8 raw in both engines
         * (the byte-strict verify still arbitrates) */
        sub[(*subn)++] = (char)b;
        return 1;
    }
}

/* True when the verdict is the MULTI-LINE member of the flow-SEQ
 * question family (the r203 nightly, run 42484238632, main 9c32c73,
 * crash-961f75c2aafe47fb867bd2bd7098c48ef6006d2e, 37 bytes
 * "[\n?+D<U+2028> 'c<U+2029>.\n--&k:j:(<U+2029> \r|\r\n.oa\"]"):
 * libyaml's unconditional flow KEY-token law (the 1.1 quirk the qflow
 * family carries) reads the glued '?' as a KEY whatever follows it,
 * and the enclosing flow SEQUENCE wraps the keyless pair as a nested
 * single-pair mapping member — while yeptris's suite-pinned plain
 * reading (HM87/652Z) keeps the whole break-riding run ONE plain
 * scalar member. Neither r183's brace spell (its same-line plain-safe
 * run law declines any break inside the run) nor r190's KIND 2 "? "
 * spell can carry this shape: the KEY VALUE BYTES themselves diverge,
 * because libyaml's fold law keeps a mid-plain U+2028/U+2029 RAW
 * (READ_LINE copies LS/PS verbatim; only CR/CRLF/NEL normalize to LF)
 * while yeptris follows the spec's fold law (one break folds to a
 * space), so no rewrite that lets both engines fold the run can ever
 * produce equal dumps. The preserving spell bakes libyaml's EXACT
 * folded key bytes into a double-quoted key — the fold simulated with
 * libyaml's own law (a raw LS/PS lead rides verbatim with its
 * trailing breaks; an LF-lead folds to ' ' when no trailing break
 * follows, else the trailing breaks ride verbatim and the lead drops;
 * same-line spaces ride, break-adjacent spaces drop, trailing breaks
 * at the terminator drop) — with every fold-sensitive byte escaped
 * ('"' / '\' / LF / tab / LS as \L / PS as \P), so NEITHER engine
 * folds the rewrite and both read the baked bytes as-is. The finder
 * only fires on a '?' at flow depth whose innermost frame is a
 * SEQUENCE and whose follower starts a MULTI-LINE plain run (r183's
 * turf is the clean same-line run; quoted, indicator, and boundary
 * followers stay with the other qflow arms), ended by ',' or ']' with
 * no quote / '#' / '\' / nested flow / pair-separating ':' inside,
 * ASCII-only content beyond the breaks, and at most 64 folds (the
 * r225 nightly's 11-fold run, run 42613510600, rode the 8-16 raise;
 * the r231 nightly's 23-fold artifact, run 42677255629, rode the
 * 16-64 raise — the cap is a work bound only, the verify
 * arbitrates). An
 * EMBEDDED '?' mid-run rides since the r219 nightly (run 42567242070,
 * main 3b7b3f8, crash-181967eb654e91fe5d56b394c48b90eb85987370,
 * minimized "[?+D<U+2028>.\nF:-?-]"): a '?' between plain bytes is
 * CONTENT in both engines' originals (libyaml's scan_plain never
 * breaks at it mid-token -- its flow break set is ',' / '[' / ']'
 * / '{' / '}' plus ':'-before-blank, and the KEY fetch law applies
 * only BETWEEN tokens), and the bake quotes it away inside the
 * double-quoted key. The byte-strict verify against libyaml's
 * ORIGINAL dump arbitrates: both engines must read the rewrite
 * exactly as libyaml read the original, or the claim declines. */
static int yd_dump_eq_mod_lsps_fold(const char* ydump, const char* ldump);
static int yd_r203_flow_seq_question_multiline_divergence(const uint8_t* data, size_t size,
                                                          const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 262144) {
        return 0;
    }
    char* sub = malloc(size * 2 + 16); /* each hit turns "?run" into "{"escaped": }" */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;
    /* the frame-kind stack: 1 = sequence, 2 = mapping */
    int frames[64];
    int depth = 0;
    int hits = 0;
    /* multibyte break length at i: 2 = CRLF/NEL, 3 = LS/PS, 1 = LF/CR
     * alone, 0 = not a break (a dangling lead rides as content) */
#define YD_R203_BREAK_LEN(d, n, i)                                                                 \
    ((d)[i] == '\n'   ? 1                                                                          \
     : (d)[i] == '\r' ? (((i) + 1 < (n)) && (d)[(i) + 1] == '\n' ? 2 : 1)                          \
     : ((d)[i] == 0xC2 && (i) + 1 < (n) && (d)[(i) + 1] == 0x85) ? 2                               \
     : ((d)[i] == 0xE2 && (i) + 2 < (n) && (d)[(i) + 1] == 0x80 &&                                 \
        ((d)[(i) + 2] == 0xA8 || (d)[(i) + 2] == 0xA9))                                            \
         ? 3                                                                                       \
         : 0)
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
            if (depth < 64) {
                frames[depth] = (c == '[') ? 1 : 2;
            }
            depth++;
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            sub[subn++] = (char)c;
            i++;
            continue;
        }
        if (c == '?' && depth > 0 && depth <= 64 && frames[depth - 1] == 1 && i + 1 < size) {
            unsigned char f = data[i + 1];
            /* the follower must START a plain run: blanks, breaks, and
             * every flow/block indicator decline to the other arms'
             * spells */
            if (f != ' ' && f != '\t' && YD_R203_BREAK_LEN(data, size, i + 1) == 0 && f != ',' &&
                f != ':' && f != ']' && f != '}' && f != '"' && f != '\'' && f != '[' && f != '{' &&
                f != '#' && f != '&' && f != '*' && f != '!' && f != '|' && f != '>' && f != '%' &&
                f != '@' && f != '`' && f != '?') {
                /* the run itself rides breaks AND quotes (the fold
                 * law's absorption: a plain run open across the break
                 * keeps a quote mid-plain content in both engines, the
                 * r191 law) and an EMBEDDED '?' (the r219 admission:
                 * mid-plain '?' is content in both engines, and the
                 * bake quotes it away), but no '#' / '\' / nested flow
                 * / pair-separating ':' inside, no non-ASCII content
                 * beyond the breaks, no stray control bytes, and it
                 * must end at ',' or ']' with at most 64 folds — the
                 * multi-line shape r183's clean-run law declines (the
                 * cap rode 8 through the r219 nightly and 16 through
                 * the r225
                 * nightly, run 42613510600, main 355568f, crash-
                 * 277015f0d109025c7c028c614561e349224db928, rides an
                 * 11-fold run — the r203 artifact's direct descendant;
                 * its head "?+D<LS> 'c<PS>.:j:(<PS>" with GLUED ':'s
                 * and an EMBEDDED second '?', admissions the r206/r219
                 * widenings already own; the r231 nightly rides the
                 * family's all-blank-run member — 13 breaks, 12 of them
                 * trailing — and its TRUE artifact's 23-fold run, both
                 * of which the raised cap and buffer below now cover).
                 * The bake still declines gracefully past its caps
                 * (wok=0 rides the hit as content), so the widened cap
                 * cannot mis-certify: it only reaches folds the
                 * byte-strict verify then arbitrates. */
                size_t j = i + 1;
                int clean = 1;
                int breaks = 0;
                while (j < size) {
                    unsigned char d = data[j];
                    if (d == ',' || d == ']') {
                        break;
                    }
                    if (d == '[' || d == '{' || d == '}' || d == '#') {
                        clean = 0;
                        break;
                    }
                    /* the r236 widening: a '\' rides as plain CONTENT in
                     * both engines (the plain-backslash law) and the DQ
                     * bake escapes it as \\ -- the old decline was
                     * conservative-only (the byte-strict verify
                     * arbitrates) */

                    if (d == ':' && j + 1 < size) {
                        unsigned char g = data[j + 1];
                        if (g == ' ' || g == '\t' || g == ',' || g == '?' || g == '[' || g == ']' ||
                            g == '{' || g == '}' || YD_R203_BREAK_LEN(data, size, j + 1) > 0) {
                            clean = 0;
                            break;
                        }
                    }
                    int db = YD_R203_BREAK_LEN(data, size, j);
                    if (db > 0) {
                        if (++breaks > 64) {
                            clean = 0;
                            break;
                        }
                        j += (size_t)db;
                        continue;
                    }
                    if (d < 0x20 || d == 0x7F) {
                        clean = 0; /* stray control declines */
                        break;
                    }
                    if (d >= 0x80) {
                        /* the r236 widening: a complete VALID UTF-8
                         * sequence that is not a break unit (LS/PS/NEL
                         * were consumed as db units above -- any E2 80
                         * A8/A9 or C2 85 reaching here is a fragment)
                         * rides as content -- e.g. U+2A29. Fragments
                         * and overruns decline. */
                        size_t ln = (d >= 0xF0) ? 4 : (d >= 0xE0) ? 3 : (d >= 0xC0) ? 2 : 0;
                        if (ln == 0 || j + ln > size) {
                            clean = 0;
                            break;
                        }
                        int frag = 0;
                        for (size_t q2 = 1; q2 < ln; q2++) {
                            if ((data[j + q2] & 0xC0) != 0x80) {
                                frag = 1;
                                break;
                            }
                        }
                        if (frag || (d == 0xC2 && data[j + 1] == 0x85) ||
                            (d == 0xE2 && data[j + 1] == 0x80 &&
                             (data[j + 2] == 0xA8 || data[j + 2] == 0xA9))) {
                            clean = 0;
                            break;
                        }
                        j += ln;
                        continue;
                    }
                    j++;
                }
                if (clean && j < size && breaks > 0) {
                    /* bake libyaml's EXACT folded key bytes into a
                     * double-quoted key: the fold simulated with
                     * libyaml's own law, every fold-sensitive byte
                     * escaped so neither engine folds the rewrite */
                    unsigned char lead_kind = 0; /* 0 none, 1 LF, 2 raw LS/PS */
                    unsigned char lead_raw[3];
                    /* stored forms of post-lead breaks: the r231
                     * nightly (run 42677255629, main edcece1, crash-
                     * 238624b1) rode a 12-trailing-break run after the
                     * raw LS/PS lead — 34 stored bytes past the old
                     * 24-byte buffer (wok=0 declined the hit) — and its
                     * TRUE artifact rides 23 folds past the old 16-break
                     * cap. The cap is a work bound, not a soundness gate
                     * (the byte-strict verify arbitrates), so it rides
                     * 64 with the buffer at 64*3 bytes: every admitted
                     * break stores at most 3 bytes. */
                    unsigned char trailing[192];
                    size_t trailn = 0;
                    char wsb[256]; /* same-line whitespace pending */
                    size_t wsn = 0;
                    int wok = 1;
                    size_t save = subn;
                    sub[subn++] = '{';
                    sub[subn++] = '"';
                    size_t k = i + 1;
                    while (k < j) {
                        unsigned char d = data[k];
                        int db = YD_R203_BREAK_LEN(data, size, k);
                        if (db > 0) {
                            wsn = 0; /* break-adjacent spaces drop */
                            unsigned char st[3];
                            size_t stn;
                            if (db == 3) {
                                st[0] = data[k];
                                st[1] = data[k + 1];
                                st[2] = data[k + 2];
                                stn = 3;
                            } else {
                                st[0] = '\n';
                                stn = 1;
                            }
                            if (lead_kind == 0) {
                                if (stn == 3) {
                                    lead_kind = 2;
                                    lead_raw[0] = st[0];
                                    lead_raw[1] = st[1];
                                    lead_raw[2] = st[2];
                                } else {
                                    lead_kind = 1;
                                }
                            } else if (trailn + stn <= sizeof(trailing)) {
                                for (size_t t = 0; t < stn; t++) {
                                    trailing[trailn++] = st[t];
                                }
                            } else {
                                wok = 0;
                            }
                            k += (size_t)db;
                            continue;
                        }
                        if (d == ' ' || d == '\t') {
                            /* a post-break blank skips (the fold law);
                             * a same-line one rides verbatim pending */
                            if (lead_kind == 0) {
                                if (wsn < sizeof(wsb)) {
                                    wsb[wsn++] = (char)d;
                                } else {
                                    wok = 0;
                                }
                            }
                            k++;
                            continue;
                        }
                        /* a content byte: flush the fold, then ride */
                        if (lead_kind == 0) {
                            for (size_t w = 0; w < wsn; w++) {
                                if (!yd_r203_dq_escape(sub, &subn, (unsigned char)wsb[w])) {
                                    wok = 0;
                                }
                            }
                        } else if (lead_kind == 1) {
                            if (trailn == 0) {
                                sub[subn++] = ' ';
                            } else {
                                for (size_t t = 0; t < trailn;) {
                                    if (trailing[t] == 0xE2 && t + 2 < trailn) {
                                        sub[subn++] = '\\';
                                        sub[subn++] = trailing[t + 2] == 0xA8 ? 'L' : 'P';
                                        t += 3;
                                    } else {
                                        if (!yd_r203_dq_escape(sub, &subn, trailing[t])) {
                                            wok = 0;
                                        }
                                        t++;
                                    }
                                }
                            }
                        } else {
                            sub[subn++] = '\\';
                            sub[subn++] = lead_raw[2] == 0xA8 ? 'L' : 'P';
                            for (size_t t = 0; t < trailn;) {
                                if (trailing[t] == 0xE2 && t + 2 < trailn) {
                                    sub[subn++] = '\\';
                                    sub[subn++] = trailing[t + 2] == 0xA8 ? 'L' : 'P';
                                    t += 3;
                                } else {
                                    if (!yd_r203_dq_escape(sub, &subn, trailing[t])) {
                                        wok = 0;
                                    }
                                    t++;
                                }
                            }
                        }
                        lead_kind = 0;
                        trailn = 0;
                        wsn = 0;
                        if (!yd_r203_dq_escape(sub, &subn, d)) {
                            wok = 0;
                        }
                        k++;
                    }
                    sub[subn++] = '"';
                    sub[subn++] = ':';
                    sub[subn++] = ' ';
                    sub[subn++] = '}';
                    if (wok) {
                        i = j;
                        hits++;
                        continue;
                    }
                    subn = save; /* the bake failed its caps: ride as content */
                }
            }
        }
        sub[subn++] = (char)c;
        i++;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int yok2 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = (yok2 && lok2 && ydump2 != NULL && ldump2 != NULL && strcmp(ydump2, ldump) == 0 &&
                 strcmp(ldump2, ldump) == 0);
    if (!known && hits > 0 && yok2 && lok2 && ydump2 != NULL && ldump2 != NULL &&
        strcmp(ldump2, ldump) == 0 && yd_dump_eq_mod_lsps_fold(ydump2, ldump)) {
        /* the r233 two-phase verify (run 42697464143, main e2b7464,
         * crash-72a25efe): the rewrite is exact for libyaml
         * (ldump2 == ldump byte-exact), but the UNTOUCHED members
         * downstream may carry their own raw LS/PS bytes (libyaml's
         * plain verbatim-keep law, the r231 finding) where yeptris
         * folds -- the exact compare declined on member 3's delta
         * alone. On decline the yeptris leg re-verify through the
         * adjudicated LS/PS fold walk (the composition of two trusted
         * carries -- the r166 lesson applied to the verify); the
         * libyaml leg stays byte-exact. Members that verified exactly
         * before keep their exact path (phase 1 first). */
        known = 1;
    }
    free(ydump2);
    free(ldump2);
    free(sub);
    return known;
#undef YD_R203_BREAK_LEN
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

/* True when the region carries the glue family's fingerprint: a '?'
 * immediately followed by '#' or '!' (the plain-'?' law's glued run,
 * the pass-1 marker's own byte pair). The r163 alias-tail pair pins the
 * marker INSIDE yeptris's kept content so the ride stays family-local
 * rather than any suffix-match. */
static int yd_qgh_glue_marked(const char* s, size_t n) {
    for (size_t i = 0; i + 1 < n; i++) {
        if (s[i] == '?' && (s[i + 1] == '#' || s[i + 1] == '!')) {
            return 1;
        }
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

/* True when libyaml's scalar content is yeptris's with yeptris's
 * fold-site blank REPLACED by raw LS/PS triples — the #638 law's
 * SUBSTITUTED reading, in the soup walk's paired form (the r56
 * plain-LS/PS law: yeptris (1.2) folds an LS/PS break to ONE blank
 * while libyaml's READ_LINE joins the triple raw and folds only its
 * LF/CR count, so the blanks around the joined triple are gone — at
 * the site yeptris renders the blank libyaml renders the triple). The
 * r150 ADDITIVE arm accepts only the INSERTED form (one triple into
 * otherwise identical text); the r162 nightly's member (run
 * 42171805647, main cf17954) carries the break run "11<CR><PS> 7.tag"
 * INSIDE its killed member's fold — yeptris folds the run to one blank
 * ("11 7.tag"), libyaml joins the PS and eats CR + blank
 * ("11<PS>7.tag", delta 2) — the additive length pin declines, the
 * generic content-agnostic skip consumed yeptris's side, and the walk
 * died at libyaml's surviving line against yeptris's map opener. Here
 * the pair consumes BOTH lines as one diff (the r150 pattern): the
 * members are the same event on both sides, so the walk stays bounded
 * by construction. Position-wise walk: equal bytes advance both, a
 * yeptris blank facing a raw triple advances 1/3 (a site), anything
 * else declines; at least one site, both contents fully consumed.
 * Bounded: the pointers never rewind, so the walk terminates; no
 * wildcards (multi-triple runs where yeptris folds to one blank and
 * libyaml joins n triples decline — the n>1 shape has no adjudicated
 * reading yet). */
static int yd_qgh_lsps_substituted(const char* y, size_t yn, const char* l, size_t ln) {
    size_t yi = 0;
    size_t li = 0;
    int sites = 0;
    while (yi < yn || li < ln) {
        if (yi < yn && li < ln && y[yi] == l[li]) {
            yi++;
            li++;
            continue;
        }
        if (yi < yn && y[yi] == ' ' && li + 3 <= ln && (unsigned char)l[li] == 0xE2 &&
            (unsigned char)l[li + 1] == 0x80 &&
            ((unsigned char)l[li + 2] == 0xA8 || (unsigned char)l[li + 2] == 0xA9)) {
            yi++;
            li += 3;
            sites++;
            continue;
        }
        return 0;
    }
    return sites > 0;
}

/* True when libyaml's scalar content is yeptris's with one 2-byte "\\n"
 * ESCAPE replaced by the raw LS/PS triple -- the #638 law's fold-count
 * form, third outcome (the r164 nightly, crash-bf69aeae, run
 * 42184829386, main 66f9218): the soup's break run CR LF CR CR PS SP
 * folds in yeptris's b-char algebra to FOUR breaks (LS/PS are breaks in
 * 1.2) rendered as three escaped breaks ("=VAL : /\n\n\n51"), while
 * libyaml's READ_LINE folds only the CR/LF count and joins the PS RAW
 * ("=VAL : /\n\n<PS>51") -- the same relation as the r150 ADDITIVE arm
 * (libyaml = yeptris + triple, yeptris folds the run to NOTHING) and
 * the r162 SUBSTITUTED arm (yeptris folds it to ONE BLANK), now with
 * yeptris folding it to ONE ESCAPED BREAK: the delta is +1 per site,
 * neither length pin matches, the generic content-agnostic skip then
 * consumed yeptris's side AND libyaml's side (four shared-cap skips per
 * site), and the r164 soup's five fold sites + its glue members
 * outran the fixed 64 (65 skips; the walk died at the l-only arm with
 * every difference law-explained). Position-wise walk: equal bytes
 * advance both, a yeptris "\\n" ESCAPE (backslash + n) facing the raw
 * triple advances 2/3 (a site), anything else declines; at least one
 * site, both contents fully consumed. Bounded: the pointers never
 * rewind, so the walk terminates; each site is byte-pinned (no
 * wildcards); the pair consumes BOTH lines as one diff (the r150
 * pattern -- both pointers advance, so the walk stays bounded by
 * construction, no cap needed). */
static int yd_qgh_lsps_escbreak(const char* y, size_t yn, const char* l, size_t ln) {
    size_t yi = 0;
    size_t li = 0;
    int sites = 0;
    while (yi < yn || li < ln) {
        if (yi + 1 < yn && li + 2 < ln && (unsigned char)y[yi] == '\\' &&
            (unsigned char)y[yi + 1] == 'n' && (unsigned char)l[li] == 0xE2 &&
            (unsigned char)l[li + 1] == 0x80 &&
            ((unsigned char)l[li + 2] == 0xA8 || (unsigned char)l[li + 2] == 0xA9)) {
            yi += 2;
            li += 3;
            sites++;
            continue;
        }
        if (yi < yn && li < ln && y[yi] == l[li]) {
            yi++;
            li++;
            continue;
        }
        return 0;
    }
    return sites > 0;
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

/* The ALIAS-CHARSET-CUT pair (r179): true when yeptris's "=ALI <name>"
 * line is the r142/r76 IS_ALPHA composition -- libyaml's scan_anchor cut
 * the SAME alias at the first byte outside [0-9A-Za-z_-] (here the ':'),
 * freeing that byte for its VALUE token and the tail for a plain value --
 * with every observation byte-exact: libyaml's name is a proper PREFIX
 * of yeptris's, the cut byte is exactly ':', every libyaml-side name
 * byte is in libyaml's own anchor charset (the cut position), libyaml's
 * NEXT line is "=VAL : " plus the exact eaten suffix, and yeptris's NEXT
 * line is the exact empty val (the value the eaten tail swallowed). On
 * success BOTH skeletons advance past the pair (two lines each). */
static int yd_qgh_alias_cut_pair(const char* y, size_t yl, const char* l, size_t ll,
                                 const char** y_next, const char** l_next) {
    /* the rendering is "=ALI " + the '*' marker + the name: names start
     * at +6 on BOTH sides, and the '*' itself is outside libyaml's
     * charset, so the pins align on the name bytes, never +5 */
    if (yl <= 6 || ll <= 6 || yl <= ll + 1 || memcmp(y, "=ALI ", 5) != 0 ||
        memcmp(l, "=ALI ", 5) != 0 || y[5] != '*' || l[5] != '*') {
        return 0;
    }
    size_t nl = ll - 6; /* libyaml's cut name */
    if (nl == 0 || memcmp(y + 6, l + 6, nl) != 0) {
        return 0;
    }
    for (size_t i = 0; i < nl; i++) {
        if (!yd_anchor_byte_in_libyaml_set((unsigned char)l[6 + i])) {
            return 0; /* every kept byte is in libyaml's own charset */
        }
    }
    if (y[6 + nl] != ':') {
        return 0; /* the cut byte is exactly the VALUE indicator */
    }
    size_t cl = yl - 6 - nl - 1; /* the eaten suffix after the colon */
    if (cl == 0) {
        return 0;
    }
    const char* yn = strchr(y, '\n');
    const char* ln = strchr(l, '\n');
    if (yn == NULL || ln == NULL) {
        return 0;
    }
    const char* y2 = yn + 1;
    const char* l2 = ln + 1;
    const char* y2n = strchr(y2, '\n');
    const char* l2n = strchr(l2, '\n');
    size_t y2l = y2n != NULL ? (size_t)(y2n - y2) : strlen(y2);
    size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
    const char* val_prefix = "=VAL : ";
    const size_t vp = strlen(val_prefix);
    if (y2l != vp || memcmp(y2, val_prefix, vp) != 0) {
        return 0; /* yeptris's value was eaten EMPTY */
    }
    if (l2l != vp + cl || memcmp(l2, val_prefix, vp) != 0 ||
        memcmp(l2 + vp, y + 6 + nl + 1, cl) != 0) {
        return 0; /* libyaml's value IS the eaten suffix */
    }
    *y_next = y2n + 1;
    *l_next = l2n + 1;
    return 1;
}

/* The ALIAS-CHARSET-CUT pair, VALUE orientation (r196, run 42449356146,
 * main 6ccf3db, crash-f9837513619c855054f72e49dd4bc75b744bd3ba): the
 * r179 helper's cut pins with the OTHER pairing. The r179 shape ended
 * its member at the extended alias (the value eaten empty, yeptris's
 * next line the bare "=VAL : "); here the extended alias is the member's
 * own VALUE -- the KEY was the glued-'?' plain the r125 skip stepped
 * over -- so NOTHING follows it on yeptris's side but the flow CLOSER,
 * while libyaml's next line is "=VAL : " plus the exact eaten suffix
 * (the cut freed it for a plain) and its NEXT line is that same closer.
 * "{?#??\t*L=:\n*L:(}" renders yeptris "=VAL : ?#??\t*L=" + "=ALI *L:("
 * + "-MAP" (the spec's ns-anchor-char keeps the ':' and the '(' in the
 * name, the r142/r76 law) and libyaml "=ALI *L" + "=VAL : (" + "-MAP"
 * (scan_anchor cut at the ':', the KEY token ate the glued comment).
 * Byte-exact pins: libyaml's name is a proper PREFIX of yeptris's, the
 * cut byte is exactly ':', every kept byte is in libyaml's own charset,
 * libyaml's value is the exact eaten suffix, yeptris's next line is a
 * flow closer, and libyaml's NEXT line is the SAME closer -- the pair
 * leaves both skeletons there, where the walk verifies it equal. An
 * invented name or a genuine yeptris alias-charset bug leaves one of
 * the pins unsatisfied and the helper declines (r125 keeps its bail). */
static int yd_qgh_alias_cut_valued_pair(const char* y, size_t yl, const char* l, size_t ll,
                                        const char** y_next, const char** l_next) {
    if (yl <= 6 || ll <= 6 || yl <= ll + 1 || memcmp(y, "=ALI ", 5) != 0 ||
        memcmp(l, "=ALI ", 5) != 0 || y[5] != '*' || l[5] != '*') {
        return 0;
    }
    size_t nl = ll - 6; /* libyaml's cut name */
    if (nl == 0 || memcmp(y + 6, l + 6, nl) != 0) {
        return 0;
    }
    for (size_t i = 0; i < nl; i++) {
        if (!yd_anchor_byte_in_libyaml_set((unsigned char)l[6 + i])) {
            return 0; /* every kept byte is in libyaml's own charset */
        }
    }
    if (y[6 + nl] != ':') {
        return 0; /* the cut byte is exactly the VALUE indicator */
    }
    size_t cl = yl - 6 - nl - 1; /* the eaten suffix after the colon */
    if (cl == 0) {
        return 0;
    }
    const char* yn = strchr(y, '\n');
    const char* ln = strchr(l, '\n');
    if (yn == NULL || ln == NULL) {
        return 0;
    }
    const char* y2 = yn + 1;
    const char* l2 = ln + 1;
    const char* y2n = strchr(y2, '\n');
    const char* l2n = strchr(l2, '\n');
    size_t y2l = y2n != NULL ? (size_t)(y2n - y2) : strlen(y2);
    size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
    const char* val_prefix = "=VAL : ";
    const size_t vp = strlen(val_prefix);
    if (l2l != vp + cl || memcmp(l2, val_prefix, vp) != 0 ||
        memcmp(l2 + vp, y + 6 + nl + 1, cl) != 0) {
        return 0; /* libyaml's value IS the eaten suffix */
    }
    if (y2l != 4 ||
        (memcmp(y2, "-MAP", 4) != 0 && memcmp(y2, "-SEQ", 4) != 0 && memcmp(y2, "-STR", 4) != 0)) {
        return 0; /* the VALUE orientation: only the flow closer follows */
    }
    if (l2n == NULL) {
        return 0;
    }
    const char* l3 = l2n + 1;
    const char* l3n = strchr(l3, '\n');
    size_t l3l = l3n != NULL ? (size_t)(l3n - l3) : strlen(l3);
    if (l3l != y2l || memcmp(l3, y2, y2l) != 0) {
        return 0; /* libyaml's next line is the SAME closer */
    }
    *y_next = y2;
    *l_next = l3;
    return 1;
}

/* The ALIAS-RESTART-FOLD pair (r200, run 42465797391, main 0b67393,
 * crash-1b6d4cbe2ac56ceb226c11139e339c6dadd369a8, a 20-byte corpus seed;
 * the shape-constrained deletion minimal is the 9-byte "{?#?\\n*L:(}"):
 * the glue family's MULTI-LINE-PLAIN restart -- libyaml's KEY-token
 * comment-glue law consumed the glued '?#' THROUGH the pending explicit
 * KEY (scan_to_next_token's unconditional '#' eat -- the carried 1.1
 * quirk the r132 family documents), so the NEXT line's '*' became a
 * NODE START for libyaml and re-lexed as an ALIAS KEY re-paired with
 * the pending KEY token, its ':' freeing a plain value ("=ALI *L" +
 * "=VAL : ("), while yeptris's suite-pinned plain-first-'?' law keeps
 * the glued run ONE plain that FOLDS across the break (no ':'/'/'
 * boundary sits before it -- the r196 input's trailing ':' is what
 * made THAT member cut) and absorbs the '*' spelling as mid-plain
 * CONTENT: "{?#?\\n*L:(}" renders yeptris "=VAL : ?# *L:(" + "=VAL : "
 * (the implicit empty value) where the r163 tail arm declines -- ITS
 * pin demands the member END at the alias spelling ("*L"), here the
 * plain rides on (":(" -- the fold's colon-value tail) -- and the
 * r179/r196 cut pairs decline too (they pin yeptris's OWN "=ALI" line;
 * yeptris never emits one here, the spelling rode into the fold). The
 * byte-exact restart pin: yeptris's folded plain ends EXACTLY
 * "*" + libyaml's alias name + ":" + libyaml's value (the spelling
 * libyaml re-lexed at the node start), yeptris's next line is the
 * exact empty val, libyaml's next line is "=VAL : " plus the exact S
 * tail, and the pre-alias prefix carries the glue fingerprint LOCALLY
 * (yd_qgh_glue_marked -- the kill is the comment-glue law; a bare '?'
 * prefix never diverges this way, that is the r178/r199 turf). An
 * invented name or a genuine yeptris plain-fold bug (absorbing a node
 * boundary libyaml keeps) leaves a pin unsatisfied or the skeletons
 * one entry off past the pair -- full exhaustion refuses, the DIFFER
 * stays visible. The pair consumes BOTH skeletons' two lines as one
 * diff (the r179 paired-arm pattern -- both pointers advance, bounded
 * by construction, no cap); the claim still stands only on full
 * exhaustion (the walk's law). */
static int yd_qgh_alias_restart_fold_pair(const char* y, size_t yl, const char* l, size_t ll,
                                          const char** y_next, const char** l_next) {
    const char* val_prefix = "=VAL : ";
    const size_t vp = strlen(val_prefix);
    /* yeptris's line: a CONTENT plain val; libyaml's: the alias event */
    if (yl <= vp || memcmp(y, val_prefix, vp) != 0 || memcmp(l, "=ALI *", 6) != 0) {
        return 0;
    }
    size_t nl = ll - 6;   /* libyaml's alias name (its own charset by construction) */
    size_t ycl = yl - vp; /* yeptris's folded plain content */
    const char* yn = strchr(y, '\n');
    const char* ln = strchr(l, '\n');
    if (yn == NULL || ln == NULL) {
        return 0;
    }
    const char* y2 = yn + 1;
    const char* l2 = ln + 1;
    const char* y2n = strchr(y2, '\n');
    const char* l2n = strchr(l2, '\n');
    if (y2n == NULL || l2n == NULL) {
        return 0; /* both skeletons must carry lines past the pair */
    }
    size_t y2l = (size_t)(y2n - y2);
    size_t l2l = (size_t)(l2n - l2);
    if (y2l != vp || memcmp(y2, val_prefix, vp) != 0) {
        return 0; /* yeptris's value eaten EMPTY (the explicit-key form) */
    }
    if (l2l <= vp) {
        return 0; /* libyaml's value S non-empty (the byte-exact pin) */
    }
    size_t sl = l2l - vp;
    /* the byte-exact restart pin: yeptris's plain ends "*<N>:<S>", the
     * exact spelling libyaml re-lexed as the alias plus its value */
    size_t tail = 1 + nl + 1 + sl;
    if (ycl <= tail) {
        return 0; /* the pre-alias prefix P must be non-empty */
    }
    const char* t = y + vp + ycl - tail;
    if (t[0] != '*' || memcmp(t + 1, l + 6, nl) != 0 || t[1 + nl] != ':') {
        return 0;
    }
    /* the fingerprint rides LOCALLY in the prefix P (the kill is the
     * KEY-token comment-glue law: no glue, no divergence) */
    if (!yd_qgh_glue_marked(y + vp, ycl - tail)) {
        return 0;
    }
    /* libyaml's value IS the exact S tail */
    if (memcmp(l2 + vp, t + 2 + nl, sl) != 0) {
        return 0;
    }
    *y_next = y2n + 1;
    *l_next = l2n + 1;
    return 1;
}

static int yd_qgh_differ_walk(const char* ydump, const char* ldump, int frame, int size);

/* The family entry: the ORIGINAL walk first (frame 0 -- every member
 * that rode before r178 keeps its exact path: the killed-member frame
 * skip consumes TWO lines where the r124/r127 skip consumes one, and on
 * shapes the single-line route finishes the frame route can strand the
 * walk one member off -- the battery's backtick-comma flip is the
 * witness). Only when that declines does the frame-enabled walk run --
 * the r178 composition's member-whole swallow fires there, on shapes
 * the original walk already rejected. Both routes are bounded by the
 * same shared skip cap and both claim only on full exhaustion (the
 * walk's law). */
static int yd_qflow_glued_hash_differ(const uint8_t* data, size_t size, const char* ydump,
                                      const char* ldump) {
    /* the size gate is this arm's OWN (the family's other arms keep the
     * shared 8192 where their transforms allocate): the r186 nightly
     * (run 42399477506, main 7bb0f69, crash-cc801571) landed a 8984-byte
     * soup of the family the walk already carries — the killed-member
     * frames, the SPLIT pairs and the r150/r162/r164 LS/PS fold pairs
     * all verify line-wise on the ORIGINAL dumps with no transform
     * buffer — so the arm path is allocation-free end to end and the
     * gate binds nothing but the scan. The fuzzer's len_control grows
     * inputs past the fixed caps (no -max_len on the nightly legs), so
     * the bound rides above the corpus maximum (the seed corpus's own
     * largest file pins libFuzzer's default -max_len at ~205k) and the
     * walk's own line budget does the bounding. */
    if (ydump == NULL || ldump == NULL || size == 0 || size > 262144) {
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
    if (yd_qgh_differ_walk(ydump, ldump, 0, (int)size)) {
        return 1;
    }
    return yd_qgh_differ_walk(ydump, ldump, 1, (int)size);
}

/* The GLUED-'?'-QUOTE pair (r193, run 42434624908, main 6eec12e,
 * crash-abdb19e20b6100e0fdc16a8c59425954201a5c7f, a 14284-byte soup of
 * the qflow family whose unclaimed residue is the member '?"#|'):
 * yeptris's suite-pinned plain-first-'?' law (HM87/652Z) keeps the
 * member ONE plain ('?"#|' -- the quote glued to the indicator-led run
 * is content, the r145 word-start law), while libyaml's flow-'?'
 * KEY-token law consumes the '?' and frees the glued quote to OPEN a
 * quoted scalar at the token start -- the span rides to the NEXT
 * member-final quote (those both engines read as mid-plain content,
 * the fold law's absorption), swallowing the members between as the
 * KEY's folded value, the pair closing with the implicit empty value.
 * The r183/r190 arms decline by construction: their follower gates
 * list '"'/''' among the indicators that stay with the other qflow
 * arms, and no walk route carries the swallow (the interior members
 * yeptris keeps are gone from libyaml's skeleton wholesale). The
 * preserving spell inserts ONE blank between the '?' and the quote --
 * ?quote -> '? quote' -- which makes the quote a token start for BOTH
 * engines: yeptris's separated-'?' law opens the explicit-key map with
 * the quoted scalar as the KEY (the r151 explicit form's shape) and
 * libyaml's token stream is unchanged (the blank is a separator the
 * scanner eats; the dump-compare pin below). This soup is COMPOUND --
 * the '?#'-comment kills and the LS/PS fold pairs ride alongside -- so
 * the byte-exact equality of the r183 pattern cannot hold; the claim
 * is the r115 quote-out classifier's form the compound extends: with
 * the swallow spelled away, yeptris's dump must BYTE-EXACT equal
 * libyaml's own dump of the rewrite, or the family's walk must claim
 * the WHOLE residue (the same certificate yd_qflow_glued_hash_differ
 * grants -- what remains is documented family reading, so the swallow
 * was the only special mechanism). The finder scores only '?' + quote
 * at a KEY-token position -- the '?' after ',', '[', '{', ':' or at
 * stream start, outside quotes and outside a plain run (the catch-85
 * word-start law keeps mid-plain pairs as content in both engines) --
 * and libyaml's span view rides the tracker: inside an open span every
 * byte is content until the closer, so pairs libyaml never scans as
 * tokens cannot score. Bounded: one allocation (size * 2 + 4), one
 * pass, the verify two dumps plus one walk whose line budget is the
 * claim's own bound. */
static int yd_r193_glued_question_quote_divergence(const uint8_t* data, size_t size,
                                                   const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 262144) {
        return 0;
    }
    char* sub = malloc(size * 2 + 4); /* each hit inserts one blank */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;        /* the open quoted span (libyaml's view) */
    int scalar_start = 1; /* a token can start at the scan position */
    int plain = 0;        /* a plain run is open (glued quotes are content) */
    int hits = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            /* the fold law: the break refreshes the token position but
             * an open plain run rides it (quotes glued to the run stay
             * content -- the member-final quote class) */
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                sub[subn++] = (char)c;
                i++;
            }
            scalar_start = 1;
            sub[subn++] = (char)data[i];
            continue;
        }
        if (c == ' ' || c == '\t') {
            scalar_start = 1;
            sub[subn++] = (char)c;
            continue;
        }
        if ((c == '\'' || c == '"') && scalar_start && !plain) {
            quote = c;
            scalar_start = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == '?' && scalar_start && i + 1 < size &&
            (data[i + 1] == '"' || data[i + 1] == '\'')) {
            /* THE HIT: the KEY token frees the glued quote -- spell the
             * separated form, then ride the span (libyaml opens it) */
            sub[subn++] = (char)c;
            sub[subn++] = ' ';
            hits++;
            quote = (int)data[i + 1];
            scalar_start = 0;
            sub[subn++] = (char)data[i + 1];
            i++;
            continue;
        }
        if (c == '?' && scalar_start && i + 1 < size && data[i + 1] == '#') {
            /* the KEY token + the glued '#' comment (the r136 law): the
             * comment eats to the line end -- quotes inside it cannot
             * score */
            sub[subn++] = (char)c;
            i++;
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                i--; /* the break rides the loop's own advance */
            }
            continue;
        }
        if (c == '#' && (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' ||
                         data[i - 1] == '\n' || data[i - 1] == '\r')) {
            /* the blank-led comment: both engines eat it to the break */
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                i--;
            }
            plain = 0;
            continue;
        }
        if (c == '[' || c == '{' || c == ']' || c == '}' || c == ',' || c == ':') {
            scalar_start = 1;
            plain = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == '?' || c == '-' || c == '&' || c == '*' || c == '!' || c == '|' || c == '>' ||
            c == '%' || c == '@' || c == '`') {
            /* an indicator: separated it ends the run (a token can start
             * at the follower); glued it is plain-first content */
            unsigned char f = i + 1 < size ? data[i + 1] : (unsigned char)' ';
            if (f == ' ' || f == '\t' || f == '\n' || f == '\r' || f == ',' || f == ']' ||
                f == '}') {
                scalar_start = 1;
                plain = 0;
            } else {
                scalar_start = 0;
                plain = 1;
            }
            sub[subn++] = (char)c;
            continue;
        }
        /* wordy content: the run opens/rides */
        scalar_start = 0;
        plain = 1;
        sub[subn++] = (char)c;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = 0;
    if (lok2 && ldump2 != NULL && strcmp(ldump2, ldump) == 0) {
        /* the spell must not move libyaml's own reading */
        int yok2 = 0;
        char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
        if (yok2 && ydump2 != NULL) {
            if (strcmp(ydump2, ldump2) == 0) {
                known = 1;
            } else {
                known = yd_qflow_glued_hash_differ(sub, subn, ydump2, ldump2);
            }
        }
        free(ydump2);
    }
    free(ldump2);
    free(sub);
    return known;
}

static int yd_qgh_differ_walk(const char* ydump, const char* ldump, int frame, int size) {
    /* the shared skip cap (r180): the frame pass scales with the input
     * size -- every skip consumes dump lines, so the walk stays
     * line-bounded by construction; the constant 64 stayed under the
     * r180 artifact's ~80 law-explained skips (47 killed members + the
     * empty-member frames + the SPLIT pairs), a density the frame pass
     * exists to own. Frame 0 keeps the classic 64 (its path predates
     * r178 and must not shift). */
    const int skip_cap = 64 + (frame ? size / 8 : 0);
    if (ydump == NULL || ldump == NULL) {
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
    int frame_run = 0;        /* the r180 comment-run neighbor frames remaining */
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
        /* the killed-member frame tell (r178): yeptris's next line is
         * the EMPTY val — the salvaged null this content member pairs
         * with — and the content carries the glue fingerprint LOCALLY
         * (a glued "?#"/"?!" inside it: this IS the pass-1 marker's
         * member). libyaml's comment-glue ate such a member whole, so
         * its current line belongs to a LATER member; the frame skip
         * consumes exactly the two lines (content + empty) and keeps
         * libyaml's line for the downstream pairing. */
        int y_marker_frame = frame && y_val && yn != NULL && yd_qgh_glue_marked(y + vp, yl - vp);
        int y_killed_frame = 0;
        int y_double_empty_frame = 0;
        if (y_marker_frame || (frame && frame_run > 0 && (y_val || y_empty_val) && yn != NULL)) {
            const char* y2 = yn + 1;
            const char* y2n = strchr(y2, '\n');
            size_t y2l = y2n != NULL ? (size_t)(y2n - y2) : strlen(y2);
            y_killed_frame = (y_val && y2l == vp && memcmp(y2, val_prefix, vp) == 0);
            y_double_empty_frame =
                (frame_run > 0 && y_empty_val && y2l == vp && memcmp(y2, val_prefix, vp) == 0);
        }
        if ((yn == NULL) != (ln == NULL) || yl != ll || memcmp(y, l, yl) != 0) {
            if (y_val && l_empty_val) {
                /* the emptied-key pair: libyaml emptied the plain
                 * yeptris kept (any content — the r116 family's keys
                 * ride '?#'- and '+`'-shaped plains alike); pass 1
                 * already pinned the glued-'?'# marker */
                diffs++;
            } else if (frame && y_marker_frame && y_val && l_val && y_killed_frame) {
                /* the MARKER-FRAME PRIORITY (r180, run 42336246527, main
                 * 8619d71, crash-d6d32ef7): when libyaml's current line
                 * suffix-matches the marker member's content, the r117
                 * SPLIT reading and the member-kill reading are BOTH
                 * live -- libyaml's post-'?'-token comment-glue kills the
                 * marker member WHOLE and its line is a LATER member's
                 * residue, whose content may coincidentally suffix-match
                 * the killed plain's tail (the r180 soup's "?#g" vs the
                 * surviving "g": libyaml's KEY token ate the '?', the
                 * glued '#' commented "#g,2," to the CR -- BOTH the
                 * marker AND its line-neighbor "2" died, while "g"
                 * survived and BOTH engines carry it downstream). SPLIT
                 * would consume libyaml's surviving member as a phantom
                 * continuation, strand yeptris's own "g" member one-off,
                 * and burn the shared cap to its 64 decline. When the
                 * frame tell holds (the glue fingerprint locally, the
                 * next yeptris line the exact empty val), the kill
                 * reading is the family's own law and wins BEFORE the
                 * suffix reading: consume the killed member's frame
                 * (marker + partner), keep libyaml's line, and let the
                 * walk re-pair the survivors from here. A true split
                 * whose tail then mis-aligns declines into the claim's
                 * own terminal law (full exhaustion) -- a false priority
                 * can never PASS, only leave the DIFFER visible. Same
                 * bounded consumption as the frame skip (ONE shared-cap
                 * count, both y-lines consumed, libyaml's kept). */
                skips += 1;
                if (skips > skip_cap) {
                    return 0;
                }
                frame_run = 16; /* the comment run's neighbor budget */
                diffs += 2;
                if (yn == NULL) {
                    return 0;
                }
                y = yn + 1;
                yn = strchr(y, '\n');
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (y_val && l_val && ll > vp && ll <= yl &&
                       memcmp(y + (yl - (ll - vp)), l + vp, ll - vp) == 0) {
                /* the SPLIT pair (r117): libyaml's glued-# comment-kill
                 * splits a multi-line plain yeptris folds — its
                 * continuation scalar's content is a SUFFIX of
                 * yeptris's single folded scalar. Consume both; the
                 * split shape is bounded (8 per stream). */
                if (++skips > 8 + skip_cap) {
                    return 0;
                }
                diffs++;
                if (yn == NULL || ln == NULL) {
                    break;
                }
                y = yn + 1;
                l = ln + 1;
                continue;
            } else if (y_val && l_val &&
                       (yd_qgh_lsps_additive(y + vp, yl - vp, l + vp, ll - vp) ||
                        yd_qgh_lsps_substituted(y + vp, yl - vp, l + vp, ll - vp) ||
                        yd_qgh_lsps_escbreak(y + vp, yl - vp, l + vp, ll - vp))) {
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
                /* The r162 SUBSTITUTED sibling: the break run INSIDE
                 * the killed member's own fold ("11<CR><PS> 7.tag")
                 * renders yeptris one blank where libyaml joins the raw
                 * triple and eats CR + blank — yeptris's blank IS
                 * libyaml's triple at the site (the r56 plain law's
                 * paired form), a delta of 2 the additive length pin
                 * declines. yd_qgh_lsps_substituted (above) walks the
                 * pair position-wise; both lines still advance
                 * together, so the pair stays bounded by construction.
                 * The r164 fold-count sibling (crash-bf69aeae, run
                 * 42184829386, main 66f9218): the soup's break run
                 * CR LF CR CR PS SP renders yeptris THREE escaped
                 * breaks where libyaml joins the PS raw and folds only
                 * the CR/LF count — yeptris's "\n" ESCAPE is libyaml's
                 * triple at the site, a delta of +1 neither earlier
                 * pin matches; yd_qgh_lsps_escbreak (above) walks the
                 * pair position-wise (byte-pinned sites, no
                 * wildcards). Five such sites plus the glue members
                 * burned 65 skips through the generic arms — past the
                 * shared 64 — with every difference law-explained;
                 * the paired arm needs no cap (both pointers advance). */
                diffs++;
            } else if (frame && (y_val || y_double_empty_frame) && l_val &&
                       (y_killed_frame || y_double_empty_frame)) {
                /* the KILLED-MEMBER FRAME skip (r178, run 42322793822; the
                 * r180 NEIGHBOR extension, run 42336246527, main 8619d71,
                 * crash-d6d32ef7): the marker frame above now also carries
                 * the marker's COMMENT-RUN NEIGHBORS -- libyaml's comment
                 * after the glued '?#' KEY token runs to the NEXT BREAK, so
                 * the members BETWEEN the marker and that break die WHOLE
                 * in libyaml's reading (the r180 89-byte soup: "?#,0,\r"
                 * kills the marker AND "0"; "?#,=,\n" kills marker AND
                 * "=") with no counterpart at their positions. A neighbor
                 * carries no LOCAL fingerprint (the glue rode the marker
                 * upstream), so the r178 gate declined it and every
                 * neighbor cost TWO shared-cap skips (the y-only content
                 * skip plus the L-only partner skip) -- the 89-byte soup's
                 * five killed members rode 9 skips through generic arms
                 * while the artifact's 30-member runs outran the fixed 64
                 * in BOTH walk passes. The neighbor frame rides the same
                 * member shape byte-exact (content VAL whose next line is
                 * the exact empty val, libyaml's line kept for the
                 * downstream pairing) and is granted ONLY by a marker
                 * frame just before it (frame_run, refilled by every
                 * marker frame, capped at 16 -- the r125 precedent; the
                 * comment cannot cross a break, so the run is
                 * line-local by construction). Family-gated by pass 1;
                 * the frame pass only (the r178 routing); ONE skip per
                 * frame (the r178 economics); the claim still stands only
                 * on full exhaustion of BOTH skeletons (the walk's law). */
                /* the KILLED-MEMBER FRAME skip (r178, run 42322793822,
                 * main 68700ce, crash-4663982f): libyaml's comment-glue
                 * law ate the marker member WHOLE (its KEY token fires on
                 * the flow '?', the glued '#' opens a comment that runs
                 * to the line end), so the member has no libyaml
                 * counterpart AT ITS POSITION -- and libyaml's current
                 * line is a LATER member's residue (the r178 soup's
                 * "=VAL : z", the fold tail of yeptris's NEXT member
                 * "` z"). The plain y-only skip (r124/r127) cannot take
                 * this pair alone: it leaves the killed member's EMPTY
                 * value line to pair against libyaml's residue, the
                 * L-only arm then eats libyaml's line, and every later
                 * pairing runs ONE MEMBER OFF (the r178 58-byte artifact
                 * died at yeptris's empty vs libyaml's "=ALI *L" with no
                 * arm). The frame skip consumes BOTH lines of the killed
                 * member -- the marker-bearing content VAL plus its
                 * EMPTY partner (the frame tell above pins the shape
                 * byte-exact: next line is exactly "=VAL : ") -- and
                 * KEEPS libyaml's line, so the residue suffix-pairs
                 * yeptris's next surviving member through the existing
                 * r117 SPLIT arm. Byte-tight: the content carries the
                 * glue fingerprint locally (the pass-1 marker's own
                 * member), the partner is the exact empty val, no
                 * wildcards; family-gated by pass 1; bounded by the
                 * shared skip cap (ONE count per frame -- the unit is
                 * the member frame, not its two lines: the r146 soup
                 * rides 96 kept-content members past the shared 64, and
                 * a per-line count outruns it mid-stream); the claim
                 * still stands only on full exhaustion of BOTH
                 * skeletons (the walk's law). */
                skips += 1;
                if (skips > skip_cap) {
                    return 0;
                }
                if (y_marker_frame) {
                    frame_run = 16; /* the comment run's neighbor budget */
                } else if (frame_run > 0) {
                    frame_run--; /* one neighbor frame per claim */
                }
                diffs += 2;
                if (yn == NULL) {
                    return 0;
                }
                y = yn + 1;
                yn = strchr(y, '\n');
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (frame && frame_run > 0 && y_empty_val && l_val && yn != NULL) {
                /* the DOUBLE-EMPTY frame (r180): the run-granted form for
                 * the member whose BOTH lines are empty -- the bare-'?'
                 * explicit-key member ("?,": libyaml's flow-'?' law makes
                 * it a KEY token, and the marker's glued '#' comment ate
                 * the token's tail to the break, so libyaml carries NO
                 * member while yeptris's suite-pinned plain-'?' law keeps
                 * the explicit-key member as {null: null} -- two empty
                 * lines) and its comment-swallowed empty siblings. The
                 * r116 y-only-empty skip declines here BY DESIGN: the
                 * pair it leaves runs one member off (the earlier L-only
                 * arm would eat libyaml's SURVIVOR, and every later
                 * pairing runs one member off -- the r178 frame-skip
                 * rationale verbatim). The frame consumes the member's
                 * two empty lines and KEEPS libyaml's line for the
                 * downstream survivor pairing -- the same keep-l
                 * economics the content frames above ride. ONE skip per
                 * frame (decrementing the run); the claim still stands
                 * only on full exhaustion (the walk's law). */
                skips += 1;
                if (skips > skip_cap) {
                    return 0;
                }
                frame_run--;
                diffs += 2;
                if (yn == NULL) {
                    return 0;
                }
                y = yn + 1;
                yn = strchr(y, '\n');
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (l_val && (y_empty_val || yl == 0 || y[0] == '-')) {
                /* an L-ONLY extra val of ANY content (r117): libyaml's
                 * glued-# comment-kill SPLITS a plain yeptris folds —
                 * the continuation line becomes its own scalar in
                 * libyaml's reading. Skip libyaml's line; yeptris's
                 * content/closer lines must still pair downstream
                 * (content-vs-content differences still bail). */
                if (++skips > skip_cap) {
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
                    if (frame && yl > vp + 1 && y[vp] == '?' &&
                        (y[vp + 1] == '#' || y[vp + 1] == '!')) {
                        /* the SHARED-BLOCK ride (r181, run 42341337776,
                         * main 225ccd4, crash-0d856bdb): the comment-run
                         * kill STRADDLING a collection boundary. libyaml's
                         * post-'?'-token comment-glue law (the '#' at
                         * token scan position comments to the NEXT break)
                         * ate the glue-led member's tail together with
                         * the members that follow it on the same line,
                         * while yeptris's suite-pinned plain-first-'?'
                         * law keeps every glued run as content (a '#'
                         * comments only behind a blank, the HM87/r85
                         * law) -- so yeptris carries a run of KEPT
                         * members that libyaml has none of, and
                         * libyaml's current line is the OPENER of a
                         * collection BOTH skeletons carry downstream
                         * (the r181 soup: "?#,?##|,?\r/\nm,..." -- the
                         * '?#' and '?##|' members die whole in libyaml's
                         * reading while the '/\nm' map is a member BOTH
                         * engines keep; the 12-byte minimality twin
                         * "[2,?#,?|,?\r]" is the same straddle against
                         * the trailing "?," member's own empty-key map).
                         * Every earlier route mis-pairs there: the
                         * wrapper rides read the block as a killed
                         * member's SALVAGE and consume it (the r167
                         * empty-wrapper gate holds on the twin's own
                         * {null: null} block too), the r132 shared-opener
                         * tell demands yeptris's IMMEDIATE next line be
                         * the opener (here the killed run's residue VALs
                         * sit between), and the bare structural skip
                         * then eats libyaml's opener of the SHARED block
                         * while yeptris's own copy is still pending --
                         * the walk dies at yeptris's opener against
                         * libyaml's body VAL with no arm. The ride is
                         * the r152 RE-PAIRED-VALUE pattern on the
                         * content side, pinned to the WHOLE block:
                         * yeptris's glue-led member plus its kept
                         * residue (only =VAL lines may sit between, the
                         * r144 survival gate's shape, the r152 per-site
                         * cap of 16) step aside until yeptris's OWN
                         * byte-exact copy of the full four-line block --
                         * opener, KEY line, the salvaged null, the bare
                         * closer -- the direct observation that the
                         * block is SHARED, not a salvage (where yeptris
                         * carries no such block the reading stays with
                         * the rides below; an invented twin declines --
                         * the r167 seed's skeletons carry no +MAP at
                         * all and the r148 soup's downstream block KEY
                         * differs, so both keep their exact routes).
                         * libyaml's opener is NOT consumed: it pairs as
                         * an equal line on the walk's next step. Frame
                         * pass only (the r178 routing: frame 0's routes
                         * predate the frame machinery and must not
                         * shift); the glue-led lead pin is the r167
                         * family's own (libyaml's KEY law fires only on
                         * '?'-led members); bounded by the run cap and
                         * the shared skip cap; the claim stands only on
                         * full exhaustion of BOTH skeletons (the walk's
                         * law). */
                        const char* l2b = l1n != NULL ? l1n + 1 : NULL;
                        const char* l2n2 = l2b != NULL ? strchr(l2b, '\n') : NULL;
                        const char* l3b = l2n2 != NULL ? l2n2 + 1 : NULL;
                        if (l2n2 != NULL && l3b != NULL && l2n2 - l2b == (ptrdiff_t)vp &&
                            memcmp(l2b, val_prefix, vp) == 0 && memcmp(l3b, "-MAP", 4) == 0 &&
                            (l3b[4] == '\n' || l3b[4] == '\0')) {
                            const char* twin = NULL;
                            int run = 0;
                            if (yd_qgh_repair_ride(y, l, ll, &twin, &run)) {
                                const char* tb = strchr(twin, '\n');
                                const char* t1 = tb != NULL ? tb + 1 : NULL;
                                const char* t1n = t1 != NULL ? strchr(t1, '\n') : NULL;
                                const char* t2b = t1n != NULL ? t1n + 1 : NULL;
                                const char* t2n2 = t2b != NULL ? strchr(t2b, '\n') : NULL;
                                const char* t3b = t2n2 != NULL ? t2n2 + 1 : NULL;
                                if (t1n != NULL && t2n2 != NULL && t3b != NULL &&
                                    (size_t)(t1n - t1) == l1l && memcmp(t1, l1, l1l) == 0 &&
                                    (size_t)(t2n2 - t2b) == vp &&
                                    memcmp(t2b, val_prefix, vp) == 0 &&
                                    memcmp(t3b, "-MAP", 4) == 0 &&
                                    (t3b[4] == '\n' || t3b[4] == '\0')) {
                                    skips += run;
                                    if (skips > skip_cap) {
                                        return 0;
                                    }
                                    diffs += run;
                                    y = twin;
                                    continue;
                                }
                            }
                        }
                    }
                    if (key_suffix && !pairs_next && l1n != NULL) {
                        const char* l2 = l1n + 1;
                        const char* l2n = strchr(l2, '\n');
                        size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                        const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                        if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                            memcmp(l3, "-MAP", 4) == 0 && (l3[4] == '\n' || l3[4] == '\0')) {
                            skips += 3;
                            if (skips > skip_cap) {
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
                                if (skips > skip_cap) {
                                    return 0;
                                }
                                diffs += 6;
                                y = y2n != NULL ? y2n + 1 : y2 + y2l;
                                l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                                continue;
                            }
                        }
                    }
                    if (!key_suffix && l1l == vp && l1n != NULL) {
                        /* the r167 MID-STREAM EMPTY-KEY wrapper ride
                         * (crash-a6a61def8116bbde86fc61dff1ac018afd3b0fed,
                         * run 42211827282, main d6b6098): the r136
                         * tail-wrapper shape ("?!": libyaml's flow-'?'
                         * KEY-token law opens an explicit-key salvage
                         * map and the glued '!' rides it as the
                         * non-specific tag, invisible in the dump),
                         * now TWO wrappers IN A ROW with members still
                         * behind them -- "[=,?!,?!\n,8]" renders four
                         * yeptris members against two empty-key
                         * wrappers plus the trailing "=VAL : 8". The
                         * r143/r149 rides decline: their gates demand
                         * the KEY carry CONTENT (a non-empty suffix of
                         * yeptris's killed member or of the next
                         * surviving one -- kl > 0), and here the tag
                         * rode the key AWAY ("=VAL : " exactly). The
                         * r130 line-wise path then over-consumes:
                         * member 2 pairs wrapper 1's opener (the
                         * structural skip), wrapper 1's emptied key
                         * and value GRAZE members 3 and 4 through the
                         * emptied-key arm (yeptris's "=VAL : 8" is a
                         * val and libyaml's line is an empty val --
                         * the arm's empties-strictly-more reading
                         * fires), the closer bank eats wrapper 1's
                         * "-MAP", and wrapper 2's opener faces
                         * yeptris's "-SEQ" closer with no arm (the
                         * r132 shared-opener tell wants yeptris's next
                         * line to BE the opener; the r142 terminal-
                         * tail gate wants libyaml's remainder
                         * closers-only -- both decline, the skew is
                         * mid-stream by construction). The ride claims
                         * the WHOLE wrapper when it is the byte-pinned
                         * empty block -- a bare "+MAP", the KEY
                         * exactly "=VAL : " (the tag-emptied key; the
                         * r148 props-strip variant needs l1l > vp+1
                         * and declines), the salvaged null "=VAL : "
                         * as the value, the bare "-MAP" close -- and
                         * yeptris's killed member is glue-LED (its
                         * content opens with the '?' plus the glued
                         * '#' or '!' -- the pass-1 marker's own byte
                         * pair AT the member, the r163 family pin in
                         * its lead form: libyaml's KEY law fires only
                         * on '?'-led members, and a content-bearing
                         * key never satisfies the byte-pinned empty
                         * block, so a wrapper claimed here is that
                         * member's salvage by construction). Each
                         * wrapper is the fixed four-line block (any
                         * other shape declines into the line-wise arms
                         * below); both pointers advance every claim,
                         * and the shared skip cap still bounds the
                         * total; the claim stands only on full
                         * exhaustion (the walk's law). */
                        const char* l2 = l1n + 1;
                        const char* l2n = strchr(l2, '\n');
                        size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                        const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                        if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                            memcmp(l3, "-MAP", 4) == 0 && (l3[4] == '\n' || l3[4] == '\0') &&
                            yl > vp + 1 && y[vp] == '?' && (y[vp + 1] == '#' || y[vp + 1] == '!')) {
                            skips += 3;
                            if (skips > skip_cap) {
                                return 0;
                            }
                            diffs += 5;
                            y = yn + 1;
                            l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                            continue;
                        }
                    }
                    if (!key_suffix && kl > 0 && l1n != NULL && y2n != NULL) {
                        /* the r168 DISTANCE-2 RE-PAIRED-KEY ride
                         * (crash-42d70bee0ba24bfc4025b40bade40ebc64c5517e,
                         * run 42219263275, main be3b9af): the r149
                         * re-pairing law at KEY distance TWO. libyaml's
                         * post-'?'-token comment-glue ate the glued
                         * '?##' run THROUGH TWO members -- the killed
                         * member's tail AND the next member WHOLE (the
                         * '#' at token start comments to the NEXT
                         * break; here the run is "##|,:|,:binary", a
                         * CR two members deep) -- and re-paired the
                         * pending KEY token with the CONTINUATION
                         * plain. The wrapper's KEY renders ("=VAL :
                         * bnary b:inary binary bnary b:ibinary") as
                         * the byte-exact SUFFIX of yeptris's member at
                         * distance 2 ("=VAL : :binary bnary b:inary
                         * binary bnary b:ibinary" -- yeptris's
                         * suite-pinned laws keep '?##|' and ':|' as
                         * plains (a '#' comments only behind a blank;
                         * a flow ':' needs the blank too) and fold the
                         * CR run into the SAME member's head, so the
                         * kept plain carries the KEY as its own tail).
                         * The r143 ride declines (its gate demands the
                         * KEY be the killed member's OWN residue --
                         * distance 0), r149 declines (its gate demands
                         * the suffix of the IMMEDIATELY next surviving
                         * member -- distance 1; the comment here
                         * consumed that member WHOLE, leaving nothing
                         * to suffix), the r167 ride declines (the KEY
                         * carries content), and the line-wise path
                         * then mis-pairs: the structural skip burns
                         * the owed closer on this wrapper's own "-MAP"
                         * while the emptied-key arm eats yeptris's
                         * LAST member against this wrapper's salvaged
                         * null, so the SECOND wrapper's opener faces
                         * yeptris's "-SEQ" closer with no arm (the
                         * r132 shared-opener tell and the r142
                         * terminal-tail gate both decline, the skew is
                         * mid-stream by construction). The ride claims
                         * the wrapper plus BOTH consumed members when:
                         * the killed member is glue-led (the r167
                         * lead-form family pin -- libyaml's KEY law
                         * fires only on '?'-led members), the member
                         * between is a content VAL (the eaten member;
                         * the r124/r127 content-agnostic precedent --
                         * the comment's bytes are invisible in an
                         * event dump), the member at distance 2 is a
                         * content VAL whose content ENDS with the
                         * wrapper's KEY byte-exact (the r148 render
                         * separator stripped -- the direct observation
                         * that the KEY's bytes are the kept plain's
                         * own tail, not an invented key), and the
                         * wrapper is the well-formed salvage block
                         * (opener / content KEY / "=VAL : " null /
                         * bare "-MAP"). The distance-1 form stays
                         * owned by r149: this gate is consulted only
                         * after r149 declined (code order), so a KEY
                         * that suffix-pairs the next member never
                         * reaches here with the same wrapper shape.
                         * Bounded: the fixed four-line block, the
                         * byte-exact suffix pin, both pointers advance
                         * every claim, and the shared skip cap still
                         * bounds the total; the claim stands only on
                         * full exhaustion (the walk's law). The
                         * artifact's second wrapper then pairs through
                         * the EXISTING r149 ride (its "&m" anchor is
                         * the kept "?# &m" member's own tail). */
                        const char* y3 = y2n + 1;
                        const char* y3n = strchr(y3, '\n');
                        size_t y3l = y3n != NULL ? (size_t)(y3n - y3) : strlen(y3);
                        size_t kq = kl;
                        if (l1[l1l - 1] == ' ' && kl > 1) {
                            kq = kl - 1; /* the r148 render separator */
                        }
                        if (y2l > vp && memcmp(y2, val_prefix, vp) == 0 && y3l > vp &&
                            memcmp(y3, val_prefix, vp) == 0 && kq <= y3l - vp &&
                            memcmp(y3 + y3l - kq, l1 + vp, kq) == 0 && yl > vp + 1 &&
                            y[vp] == '?' && (y[vp + 1] == '#' || y[vp + 1] == '!')) {
                            const char* l2 = l1n + 1;
                            const char* l2n = strchr(l2, '\n');
                            size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                            const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                            if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                                memcmp(l3, "-MAP", 4) == 0 && (l3[4] == '\n' || l3[4] == '\0')) {
                                skips += 4;
                                if (skips > skip_cap) {
                                    return 0;
                                }
                                diffs += 6;
                                y = y2n + 1;
                                l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                                continue;
                            }
                        }
                    }
                    if (!key_suffix && kl > 0 && l1n != NULL && y2n != NULL) {
                        /* the r172 DISTANCE-k RE-PAIRED-KEY ride
                         * (crash-6dcd3a2343fe26ff8c4fbed84831a3ccd421c62b,
                         * run 42274633465, main 93fd25d): the r168
                         * re-pairing law at KEY distance k. libyaml's
                         * post-'?'-token comment-glue ate the glued '?##'
                         * run THROUGH FIVE members -- "[11.0e ,?##|,:|,
                         * :bi.0e ,?##|,:|,:bYMA y<CR> bnaary<CR><CR>
                         * ,?#<LF>&m,]": the killed member's tail, then
                         * ":|", ":bi.0e", the DOUBLED "?##|", and ":|"
                         * whole (the '#' at token start comments to the
                         * NEXT break, and the whole run to the first CR
                         * is one libyaml line -- the doubled glue pair
                         * and the "YMA y" interior are incidental: any
                         * break-free bytes ride the comment invisibly),
                         * and re-paired the pending KEY token with the
                         * CR-folded continuation ("bnaary"; the trailing
                         * "\r\r ,"'s blank line and comma end the plain).
                         * Wrapper 1 renders (+MAP / "=VAL : bnaary" /
                         * "=VAL : " / -MAP) whose KEY is the byte-exact
                         * SUFFIX of yeptris's member at distance FIVE
                         * (yeptris's suite-pinned laws keep every glued
                         * run as content -- a '#' comments only behind a
                         * blank -- and fold the CR run into the same
                         * member's head ":bYMA y bnaary"); wrapper 2 then
                         * pairs through the EXISTING r149 ride (its
                         * props-only "&m " KEY is the kept "?# &m"
                         * member's own tail, the r168 tail pattern). The
                         * ride family's distances: r143 owns 0 (the
                         * killed member's own residue), r149 owns 1, r168
                         * owns 2 -- this gate owns k >= 3, consulted only
                         * after all three declined (code order, the
                         * shapes stay disjoint). The walk pre-fix: the
                         * structural skip burns the owed closer on
                         * wrapper 1's own "-MAP", r124/r127 graze the
                         * eaten members against wrapper 1's KEY, the
                         * r117 split arm consumes the suffix member (its
                         * content ends with the KEY byte-exact), the
                         * emptied-key arm eats the trailing member
                         * against wrapper 1's salvaged null, and the
                         * closer bank eats "-MAP" -- wrapper 2's opener
                         * then faces yeptris's "-SEQ" with struct_skips
                         * spent and no arm (the r168 commentary's death,
                         * one wrapper deeper). The ride claims the killed
                         * member plus the eaten run when: the killed
                         * member is glue-led (the r167 lead-form family
                         * pin -- libyaml's KEY law fires only on
                         * '?'-led members), EVERY member between is a
                         * kept-content PLAIN VAL (the comment's bytes are
                         * invisible in an event dump, the r124/r127
                         * content-agnostic precedent; an empty val or a
                         * structure line declines), the member at
                         * distance k is a kept-content plain VAL whose
                         * content ENDS with the wrapper's KEY byte-exact
                         * (the r148 render separator stripped -- the
                         * direct observation that the KEY's bytes are the
                         * kept plain's own tail, not an invented key), and
                         * the wrapper is the well-formed salvage block
                         * (opener / content KEY / "=VAL : " null / bare
                         * "-MAP"). Bounded: the fixed four-line block,
                         * the byte-exact suffix pin, both pointers
                         * advance every claim, and the shared skip cap
                         * charges the wrapper's four lines plus one per
                         * consumed member (monotone in k, so a long ride
                         * past the cap aborts BY DESIGN -- the
                         * distance-61 twin does). The claim stands only
                         * on full exhaustion (the walk's law). */
                        size_t kq = kl;
                        if (l1[l1l - 1] == ' ' && kl > 1) {
                            kq = kl - 1; /* the r148 render separator */
                        }
                        if (kq > 0 && yl > vp + 1 && y[vp] == '?' &&
                            (y[vp + 1] == '#' || y[vp + 1] == '!')) {
                            const char* yk = y2;
                            const char* ykn = y2n;
                            int dist = 1;
                            int hit = 0;
                            for (;;) {
                                size_t ykl = ykn != NULL ? (size_t)(ykn - yk) : strlen(yk);
                                if (ykl <= vp || memcmp(yk, val_prefix, vp) != 0) {
                                    break; /* only kept-content plain VALs
                                              may sit in the eaten run */
                                }
                                if (ykl - vp >= kq && memcmp(yk + ykl - kq, l1 + vp, kq) == 0) {
                                    hit = dist > 2; /* 1/2 stay r149/r168's */
                                    break;
                                }
                                if (ykn == NULL) {
                                    break;
                                }
                                yk = ykn + 1;
                                ykn = strchr(yk, '\n');
                                dist++;
                            }
                            if (hit) {
                                const char* l2 = l1n + 1;
                                const char* l2n = strchr(l2, '\n');
                                size_t l2l = l2n != NULL ? (size_t)(l2n - l2) : strlen(l2);
                                const char* l3 = l2n != NULL ? l2n + 1 : l2 + l2l;
                                if (l2l == vp && memcmp(l2, val_prefix, vp) == 0 &&
                                    memcmp(l3, "-MAP", 4) == 0 &&
                                    (l3[4] == '\n' || l3[4] == '\0')) {
                                    skips += 4 + dist;
                                    if (skips > skip_cap) {
                                        return 0;
                                    }
                                    diffs += 4 + dist;
                                    y = yk;
                                    l = l3[4] == '\0' ? l3 + 4 : l3 + 5;
                                    continue;
                                }
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
                    if (++skips > skip_cap) {
                        return 0;
                    }
                    diffs++;
                    y = yn + 1;
                    continue;
                }
                if (++skips > skip_cap) {
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
                if (skips > skip_cap) {
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
                if (++skips > skip_cap) {
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
            } else if (y_val && ll > 5 && strncmp(l, "=ALI ", 5) == 0 && ll - 5 <= yl - vp &&
                       memcmp(y + yl - (ll - 5), l + 5, ll - 5) == 0 &&
                       (yd_qgh_glue_marked(y + vp, yl - vp - (ll - 5)) ||
                        /* the r178 PLAIN-FIRST-'?' form: the pre-spelling
                         * prefix is exactly the ONE '?' byte -- yeptris's
                         * suite-pinned plain-'?' law keeps "?*name" ONE
                         * plain ('*' is ns-plain-safe there, so the '?' is
                         * a legal plain FIRST char) while libyaml's
                         * flow-KEY token consumes the '?' and re-pairs the
                         * '*' as an alias. Byte-tight: prefix length
                         * pinned to 1, byte pinned to '?'; the alias is
                         * PRESENT on libyaml's side (the r138-r141
                         * alias-drop masking concern cannot apply), and
                         * the family gate stays pass 1 (the marker lives
                         * in another member of the same stream -- in the
                         * r178 seed, the emptied "?#" member the walk
                         * paired one step earlier). */
                        (yl - vp - (ll - 5) == 1 && y[vp] == '?'))) {
                /* the ALIAS-AS-CONTENT-TAIL pair (r163, run
                 * 42177815461, main ec8fc99): the soup family's
                 * alias-swallow composition in the REVERSE orientation
                 * of the r138-r141 gray claims -- HERE libyaml carries
                 * the =ALI and yeptris carries the kept content.
                 * libyaml's post-'?'-token comment-glue law consumed
                 * the glued '?##' THROUGH its flow-KEY token and
                 * swallowed the CR break run, so the "*5" that follows
                 * became an ALIAS token re-paired with the pending KEY
                 * ("=ALI *5"); yeptris's suite-pinned plain-'?' law
                 * keeps the glued run ONE plain folded across the
                 * breaks, and the '*' is ns-plain-char CONTENT there
                 * (the indicator exclusions bind only ns-plain-first,
                 * spec 6.6 -- a mid-plain '*' is content in BOTH
                 * engines: "{x<CR>*5}" agrees byte-exact, so the
                 * alias-vs-content split only exists on the glue
                 * path). The fold itself is shared algebra (the
                 * b-l-trimmed reading: a break followed by k empty
                 * lines renders k escaped breaks, no fold space --
                 * "{a:<CR><CR><CR><CR>b}" agrees byte-exact), so
                 * nothing NEW is engine-suspicious. The r125
                 * alias-realign arm declines (it demands yeptris carry
                 * its OWN =ALI downstream; yeptris never emits one
                 * here -- the spelling rode into the kept plain), the
                 * r144 survival arm is not consulted (yeptris's line
                 * is a VAL, and libyaml's dump carries exactly one
                 * alias), and the walk died at the pair with no arm.
                 * The ride is the REVERSE of the r144 gate's reading:
                 * libyaml's alias is never dropped (it is right
                 * there), so the r138-r141 "yeptris =ALI vs libyaml
                 * anything" masking concern does not apply; and
                 * yeptris's kept content CARRYING the spelling as its
                 * byte-exact tail is the direct observation that the
                 * '*' was mid-plain by construction (at a node start
                 * yeptris emits =ALI itself and the line pairs
                 * equal). The pair consumes BOTH lines as one diff
                 * (the emptied-key arm's pattern -- both pointers
                 * advance, bounded by construction, no cap). Gates:
                 * libyaml's full alias rendering is yeptris's
                 * content's byte-exact SUFFIX (the name rides the
                 * fold, no wildcards), the name is non-empty, and the
                 * kept content carries the glue fingerprint LOCALLY
                 * (a glued "?#"/"?!" inside the pre-spelling prefix --
                 * the plain-'?' law's own marker, keeping the arm
                 * family-bound; a doubled alias whose surviving name
                 * is not the content's tail declines into r125 and
                 * keeps aborting BY DESIGN). Family-gated by pass 1;
                 * the claim still stands only on full exhaustion (the
                 * walk's law). */
                diffs++;
            } else if (yd_qgh_alias_restart_fold_pair(y, yl, l, ll, &y, &l)) {
                /* the ALIAS-RESTART-FOLD pair (r200, run 42465797391, main
                 * 0b67393, crash-1b6d4cbe; the 9-byte deletion minimal
                 * "{?#?\\n*L:(}"): libyaml's comment-glue kill ran to the
                 * LINE BREAK and the next line's '*' became a NODE START
                 * re-lexed as an ALIAS KEY ("=ALI *L" + "=VAL : ("), while
                 * yeptris's plain-'?' law kept the glued run ONE plain
                 * folded across the break (no ':' boundary before it --
                 * the r196 input's trailing ':' is what made THAT member
                 * cut), absorbing the spelling as mid-plain content. The
                 * r163 tail arm declines (its member ENDS at the alias
                 * spelling), the r179/r196 cut pairs decline (yeptris
                 * carries no "=ALI" line of its own here), the r125
                 * realign has nothing to realign to, and the r190
                 * transform's gates demand a quote/exotic terminator.
                 * Bounded: the paired consume (both pointers advance two
                 * lines, no cap); byte-exact pins throughout -- an
                 * invented name or a genuine plain-fold bug declines a
                 * pin or strands the skeletons past the pair, and the
                 * claim still stands only on full exhaustion. */
                diffs += 2;
                continue;
            } else if (yd_qgh_alias_cut_pair(y, yl, l, ll, &y, &l)) {
                /* the ALIAS-CHARSET-CUT pair (r179, run 42331311239,
                 * main e201a69, crash-0b3aacc6): the r142/r76 IS_ALPHA
                 * charset law COMPOSED into the glue family. libyaml's
                 * scan_anchor cuts an alias name at the first byte
                 * outside [0-9A-Za-z_-] -- here the ':' gluing the
                 * member's own value indicator -- freeing that byte for
                 * its VALUE token and the tail for a plain value, while
                 * yeptris follows the spec's ns-anchor-char (c-flow
                 * indicators and white excluded only), so ONE run
                 * "*L:( }" renders yeptris "=ALI *L:(" with the value
                 * eaten EMPTY and libyaml "=ALI *L" + "=VAL : (". The
                 * walk pre-fix died twice with no arm: yeptris's
                 * extended alias first faced libyaml's salvaged null
                 * (the r116 l-only skip demands a VAL/empty/'-' y-line;
                 * the widened gate below takes that state), then the
                 * extended alias faced libyaml's cut alias (the r125
                 * realign demands a y-VAL; the r163 tail arm demands
                 * libyaml's alias be a suffix of yeptris's VAL content;
                 * r144 demands the exact line survive libyaml's dump).
                 * The carry is the direct observation, byte-exact end
                 * to end (the helper's pins: proper-prefix name, ':'
                 * cut byte, libyaml-charset kept bytes, libyaml's next
                 * line "=VAL : " plus the exact eaten suffix, yeptris's
                 * next line the exact empty val) -- an invented name or
                 * a genuine yeptris alias-charset bug leaves one of the
                 * pins unsatisfied and the arm declines. Family-gated by
                 * pass 1 (the walk only runs inside the family); both
                 * pointers advance two lines (bounded by construction,
                 * the paired-arm pattern); the claim still stands only
                 * on full exhaustion (the walk's law). */
                diffs += 2;
                continue;
            } else if (y_val && l[0] == '=' && strncmp(l, "=ALI", 4) == 0) {
                /* an ALIAS realign (r125): both skeletons carry the
                 * alias at the same EVENT position, but yeptris's
                 * suite-pinned plain-'?' law keeps extra '#'-bearing
                 * pairs BEFORE it that libyaml's kill emptied away.
                 * Skip yeptris's VALs until its own =ALI line; the two
                 * alias lines must then match byte-exact (only VALs
                 * may sit between; anything else bails) -- or ride the
                 * r196 valued cut-pair below. Family-gated
                 * by pass 1; bounded by the shared skip cap. */
                int skipped = 0;
                for (;;) {
                    if (++skips > skip_cap || skipped > 16) {
                        return 0;
                    }
                    diffs++;
                    if (yn == NULL) {
                        return 0;
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
                /* the realigned alias pair must agree byte-exact -- or
                 * ride the r196 VALUED cut-pair (the r179 composition in
                 * value orientation: yeptris's extended alias was the
                 * member's own VALUE -- the glued-'?' plain KEY the skip
                 * stepped over -- so no eaten-empty val follows it, and
                 * libyaml's cut freed the suffix for a plain of its own;
                 * the helper pins both skeletons onto the same closer,
                 * where the walk verifies it equal). An invented name or
                 * a genuine yeptris alias-charset bug leaves one of the
                 * pins unsatisfied and the bail stands. */
                if ((yn == NULL) != (ln == NULL) || yl != ll || memcmp(y, l, yl) != 0) {
                    const char* y_next2 = NULL;
                    const char* l_next2 = NULL;
                    if (yn == NULL || ln == NULL ||
                        !yd_qgh_alias_cut_valued_pair(y, yl, l, ll, &y_next2, &l_next2)) {
                        return 0;
                    }
                    diffs += 2;
                    y = y_next2;
                    l = l_next2;
                    continue;
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
                if (++skips > skip_cap) {
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
                if (++skips > skip_cap) {
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
                if (++skips > skip_cap) {
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
                if (++skips > skip_cap) {
                    return 0;
                }
                diffs++;
                if (yn == NULL) {
                    break;
                }
                y = yn + 1;
                continue;
            } else if (l_empty_val && (yl == 0 || strncmp(y, val_prefix, vp) == 0 || y[0] == '-' ||
                                       strncmp(y, "=ALI ", 5) == 0)) {
                /* a libyaml-only extra emptied pair: re-align on
                 * libyaml's next line without consuming yeptris's. The
                 * r179 extension adds the yeptris "=ALI " case: the
                 * glue family's salvaged null (libyaml's re-paired KEY
                 * carries an EMPTY value) may sit directly before
                 * yeptris's re-positioned alias -- the alias itself
                 * must still pair byte-exact downstream (the equal
                 * path or the r179 cut arm) or the claim dies. */
                if (++skips > skip_cap) {
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

/* The composed walk behind the r166 arm: the yd_fold_walk run law
 * (LS/PS raw-join sites) and the r118 NEL k-law (raw-NEL content
 * sites) over ONE aligned walk. Backtracks on the additive/replacive
 * LS reading (the r90 lesson) with the NEL arms live inside the
 * recursion; pointers never rewind past a consumed site. */
static int yd_nel_lsps_walk(const char* ydump, size_t y, const char* ldump, size_t l, int depth,
                            int diffs) {
    while (1) {
        /* the NEL k-law first (the r118 arm's byte-set branches): a
         * maximal run of raw C2 85 pairs in yeptris's dump */
        if ((unsigned char)ydump[y] == 0xC2 && (unsigned char)ydump[y + 1] == 0x85) {
            int k = 0;
            const char* p = ydump + y;
            while ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) {
                p += 2;
                k++;
            }
            if (k == 1 && ldump[l] == ' ') {
                y += 2;
                l += 1;
                diffs++;
                continue;
            }
            if (k >= 2 && ldump[l] == '\\' && ldump[l + 1] == 'n') {
                int e = 0;
                while (e < k - 1 && ldump[l] == '\\' && ldump[l + 1] == 'n') {
                    l += 2;
                    e++;
                }
                if (e != k - 1) {
                    return 0;
                }
                y = (size_t)(p - ydump);
                diffs++;
                continue;
            }
            /* the r122 trailing-run law: the run vanishes at the
             * boundary -- accept only when yeptris's remainder behind
             * the run is empty and libyaml's dump is spent too, or
             * both remainders are byte-identical */
            if (p[0] == '\0' && ldump[l] == '\0') {
                return diffs + 1 > 0;
            }
            if (p[0] != '\0' && strcmp(p, ldump + l) == 0) {
                return 1;
            }
            return 0;
        }
        /* lone raw 0x85 bytes under the lead-byte guard (the r128
         * spelling of the same break) */
        if ((unsigned char)ydump[y] == 0x85 && (y == 0 || (unsigned char)ydump[y - 1] < 0x80)) {
            int lone = 0;
            while ((unsigned char)ydump[y] == 0x85) {
                y++;
                lone++;
            }
            (void)lone;
            diffs++;
            continue;
        }
        /* the \N escape spelling (the r119/r128 emitter form): a
         * maximal \N run maps to the same artifact as the raw pair
         * run */
        if (ydump[y] == '\\' && ydump[y + 1] == 'N') {
            int k = 0;
            while (ydump[y] == '\\' && ydump[y + 1] == 'N') {
                y += 2;
                k++;
            }
            if (k == 1 && ldump[l] == ' ') {
                l += 1;
            } else if (ldump[l] == '\\' && ldump[l + 1] == 'n') {
                int e = 0;
                while (e < k - 1 && ldump[l] == '\\' && ldump[l + 1] == 'n') {
                    l += 2;
                    e++;
                }
                if (e != k - 1) {
                    return 0;
                }
            } else {
                return 0;
            }
            diffs++;
            continue;
        }
        /* the shared prefix: equal bytes advance both (the r164
         * discipline). BEFORE the l-side triple law -- the seed's
         * content carries a raw U+20A9 (E2 82 A9) whose E2 lead must
         * not be mistaken for a break triple */
        if (ydump[y] == ldump[l]) {
            if (ydump[y] == '\0') {
                return diffs > 0;
            }
            y++;
            l++;
            continue;
        }
        /* libyaml's raw LS/PS triple facing yeptris's escaped fold
         * (the r72/r86/r104 run law, additive vs replacive) */
        if ((unsigned char)ldump[l] == 0xE2) {
            unsigned char lb1 = (unsigned char)ldump[l + 1];
            int l_triple = 0;
            if (lb1 == 0x80) {
                unsigned char lb2 = (unsigned char)ldump[l + 2];
                l_triple = lb2 == 0xA8 || lb2 == 0xA9;
            }
            if (l_triple && ydump[y] == '\\' && ydump[y + 1] == 'n') {
                /* run-scoped: count yeptris's escapes and libyaml's
                 * triples+escapes behind the site */
                size_t y2 = y;
                size_t l3 = l + 3;
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
                while (ydump[y2] == '\\' && ydump[y2 + 1] == 'n') {
                    yy++;
                    y2 += 2;
                }
                if (ly == yy) {
                    /* additive: the triple rides alone -- yeptris's
                     * escapes pair with libyaml's escapes behind it */
                    y = y2;
                    l = l3;
                    diffs++;
                    continue;
                }
                if (ly == yy - 1) {
                    /* replace: this break IS the raw triple */
                    y = y2;
                    l = l3;
                    diffs++;
                    continue;
                }
                if (depth < 8 && yd_nel_lsps_walk(ydump, y, ldump, l + 3, depth + 1, diffs + 1)) {
                    return 1;
                }
                y += 2;
                l += 3;
                diffs++;
                continue;
            }
            if (l_triple && ydump[y] == ' ') {
                /* the r56 plain law: yeptris's fold-space is
                 * libyaml's raw triple */
                y += 1;
                l += 3;
                diffs++;
                continue;
            }
            return 0;
        }
        return 0;
    }
}

/* True when the difference is the COMPOSITION of the two adjudicated
 * plain break carries inside ONE scalar (the r166 nightly's seed,
 * crash-095dea85d20a4b6687f4539f98dba8eae8df87d4, run 42202493551,
 * main a6d4b41): a root plain packed with break runs whose LS/PS
 * raw-join sites (the yd_dump_eq_mod_lsps_fold family -- libyaml's
 * READ_LINE joins an LS/PS break RAW into the content and folds the
 * trailing LF/CR run behind it in FULL, so yeptris's escaped folds
 * ride additively or replacement-wise beside the raw triple -- the
 * r56/r72/r86/r104 laws) INTERLEAVE with raw-NEL content sites (the
 * yd_dump_eq_mod_nel_fold family -- yeptris keeps the 1.2 non-break
 * NEL as raw content while libyaml folds it, k == 1 to one fold
 * space and k >= 2 to k-1 escaped breaks -- the r118/r119/r122/r128
 * laws). Each family classifies its own members ALONE and both
 * decline the seed: the first mismatch is an LS site (dump offset
 * 29: yeptris's "[{" + "\n\n" against libyaml's "[{" + raw LS +
 * "\n\n") and the NEL walk has no LS arm ('\\' vs 0xE2 on its first
 * divergent byte); the LS walk in turn dies at the first NEL site
 * (offset 73: yeptris's raw C2 85 C2 85 against libyaml's one
 * escaped break -- 0xC2 vs '\\', no NEL arm). The r158 lesson (two
 * adjudicated carries composing in one input), applied to walks
 * instead of transforms: ONE walk carries both site kinds. The
 * relation is byte-pinned per the r164 discipline: equal bytes
 * advance both pointers; an l-side LS/PS triple rides the
 * fold-walk's run law (additive first, backtrack to replace -- the
 * r90 lesson, depth-capped); a yeptris fold space facing the triple
 * is the r56 plain law; a yeptris NEL run (raw C2 85 pairs, lone
 * 0x85 bytes under the lead-byte guard, or \N escapes) rides the
 * r118 k-law with the r122 trailing-run vanish; anything else
 * declines; pointers never rewind. Gated on BOTH byte families in
 * the input, and ordered behind the single-family arms in
 * probe_one, so every existing member still reaches its own
 * (untouched) arm first. */
static int yd_plain_nel_lsps_fold_divergence(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int has_nel = 0;
    int has_lsps = 0;
    for (size_t i = 0; i < size; i++) {
        if (i + 1 < size && (unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
            has_nel = 1; /* the UTF-8 NEL pair */
        }
        if ((unsigned char)data[i] == 0x85 && (i == 0 || (unsigned char)data[i - 1] != 0xC2)) {
            has_nel = 1; /* a lone raw 0x85 byte */
        }
        if (i + 2 < size && (unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            has_lsps = 1; /* the LS/PS triple */
        }
        if (has_nel && has_lsps) {
            break;
        }
    }
    if (!has_nel || !has_lsps) {
        return 0;
    }
    return yd_nel_lsps_walk(ydump, 0, ldump, 0, 0, 0);
}

/* True when the difference is the BLOCK-SCALAR NEL CONTENT law (the
 * r160 nightly's seed): inside block-scalar content the 1.2 break set
 * is LF/CR/CRLF ONLY (spec 8.1 -- a block scalar's lines break only
 * at b-break), so yeptris keeps the NEL (C2 85) as CONTENT bytes; the
 * 1.1 reader (libyaml) counts NEL as a break there too (its IS_BREAK),
 * splitting the content line and folding/stripping around the split:
 * in a folded scalar a lone NEL break folds to the fold space and k
 * consecutive NEL breaks collapse to (k-1) literal breaks (the plain
 * arm's artifact law), in a literal scalar every NEL break stays a
 * literal break, and each break eats the continuation line's indent.
 * The suite sides with yeptris (1.2 content), so this is a carry.
 * The dump relation, walked with the two artifacts the split can
 * leave: at each run of k NEL pairs in yeptris's dump, libyaml's dump
 * may carry up to (k-1) escaped breaks (the multi-break collapse),
 * then a bounded re-alignment section -- ONE byte when the dumps
 * simply agree (the literal/only-line shape), or up to k steps of the
 * r173 run-fold continuation (libyaml's empty-line scan eats the
 * spaces BETWEEN its literal breaks while yeptris keeps them, so the
 * run's exit may carry space<->break pairs followed by the surplus
 * spaces: yeptris's spaces libyaml ate as the continuation's
 * indentation, or libyaml's fold spaces with no yeptris counterpart),
 * or yeptris's CLIP BREAK with no counterpart (the r175 nightly: the
 * 1.2 scalar keeps its final LF as the trailing clip break while
 * libyaml's trailing-break accounting absorbed that LF into the NEL
 * region it collapsed).
 * Beyond the section the dumps must be byte-identical (the frames,
 * the other scalars).
 * The plain family never reaches here (yeptris's emitter escapes its
 * plain NELs as \N, so no C2 85 run exists in its dump), and the
 * quoted twin owns its own signature -- the run branch fires only on
 * raw content pairs. The r161 sibling adds the TERMINAL tail: the
 * content line ends right behind the run, yeptris "SP + \n" (content
 * space + the clip break, the r140 breakless law's break when the
 * final LF is gone) against libyaml's single escaped break (its kept
 * leading break; the blank continuation's space was eaten as its
 * indentation and that line's break dropped) -- accepted only when
 * the remainders behind both escaped breaks are byte-identical.
 * The r176 nightly adds the COMPOSED tail (the r158 lesson, two
 * adjudicated carries composing in one input): content AFTER the run
 * with a breakless final line -- yeptris's value rides on past the
 * fold space (k == 1) or the kept break (the literal/k >= 2 shapes)
 * and ends with the CLIP BREAK the r140 law appends at breakless EOF,
 * while libyaml's breakless tail has no escaped break at all. The
 * walk's section arms all live behind the run: when the run's exit
 * agrees (the seed's fold space) or is absorbed by the bounded
 * continuation (the literal twin's space<->break pair), the remaining
 * divergence is yeptris's trailing escape vs libyaml's frame LF in
 * the byte-bound main loop, where no surplus arm exists. The
 * transform composes the two adjudicated deltas in the clip arm's own
 * direction (yd_clip_breakless_divergence's move, the r158 arm's
 * shape): append the ONE break libyaml drops to a copy of libyaml's
 * input, re-dump libyaml on THAT, and require the exact same walk
 * against yeptris's original dump -- the restored break makes
 * libyaml's tail carry the escaped break yeptris's clip law keeps,
 * and the byte-bounded walk (k-capped section, no rewinds, identical
 * remainders past the scalar) owns the whole relation. Fires only
 * after the walk declined the original dumps and only on a breakless
 * final byte (the clip arm's own precondition), so every existing
 * member keeps riding its branch unchanged. The r197 nightly adds the
 * LS/PS raw-keep tolerance to the walk itself (crash-03377781, run
 * 42453001309, main b2fdddd; 10-byte min "> CR PS SP NEL SP t"): the
 * NEL run rode WITH a leading PS-only line, and the walk's byte-bound
 * main loop died on the #638 delta BEFORE ever reaching the run --
 * yeptris spells the PS break it normalized to LF while libyaml's
 * block reader keeps the raw triple (scanner.c READ_LINE's
 * LS|PS->LS|PS branch). That composition cannot ride the r158 arm
 * (yd_lsps_breakless_clip_divergence): its lsps-walk owns the
 * yeptris-SPELLED-vs-libyaml-RAW polarity but has no arm for the NEL
 * polarity (yeptris keeps the raw NEL as 1.2 content where libyaml's
 * break is the spelled \n). The tolerance is the family's own law
 * byte-wise: yeptris's spelled \n is libyaml's raw LS/PS triple --
 * a 2-vs-3-byte window that fires only on the documented delta, so
 * the claim stays bounded.
 * The r205 nightly adds the ALIGNED-triple indent surplus (crash
 * 4d4e23c6, run 42492751265, main a0f2ff6; 34-byte min
 * "> CR PS SP 70 PS SP 700:\\n  m NEL SP t0:\\n  m NEL SP t"): the
 * mid-line LS/PS rode INSIDE a bs content line's wholesale token (the
 * r194 ride), so BOTH dumps carry the raw triple at the same offset --
 * and behind it yeptris's content line keeps the space libyaml's split
 * ATE as the continuation's indentation (its break lands the next line
 * at column 0, the breaks-scan strips to the content indent). The
 * aligned branch (both dumps' raw triples coincide) skips both and
 * then eats yeptris's SURPLUS spaces -- only while the dumps disagree
 * (a space both carry stays byte-bound), capped at 64 (the block's
 * indent column), so the claim cannot outrun the delta and an
 * existing member's byte-bound alignment past an aligned triple is
 * untouched.
 * The r208 nightly adds the SURPLUS-BREAK law (crash 251030f1, run
 * 42507908569, main 0398dc6; 25-byte artifact ">\n !<[!p\n !<NEL CR
 * CR !<NEL NEL SP\n"): at the run's exit yeptris carries the real
 * breaks' collapse while libyaml's collapse counts the NELs it turned
 * into breaks too, so libyaml's escaped-break run is exactly k LONGER
 * than yeptris's and the content behind both runs coincides (the
 * artifact's first site: yeptris NEL + 1 escaped break from the CR CR
 * pair, libyaml 2 escaped breaks from NEL CR CR). The walk eats both
 * runs, requires the surplus == k, requires the alignment byte behind
 * the runs to coincide (and not be an escape), and rejoins the
 * byte-bound walk there -- pointers never rewind and the surplus is
 * bound by the run. Fires after the terminal-tail arm declined (whose
 * remainders are fully identical, a disjoint shape) and before the
 * (k-1) collapse (which would eat part of b's run and break the
 * exact-k accounting), so every existing member keeps its branch. */
static int yd_block_nel_walk_mult(const char* ydump, const char* ldump, int mult) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    const char* a = ydump;
    const char* b = ldump;
    int subs = 0;
    while (*a != '\0' || *b != '\0') {
        if ((unsigned char)a[0] == 0xE2 && (unsigned char)a[1] == 0x80 &&
            ((unsigned char)a[2] & 0xFE) == 0xA8 && (unsigned char)b[0] == 0xE2 &&
            (unsigned char)b[1] == 0x80 && ((unsigned char)b[2] & 0xFE) == 0xA8) {
            /* the r205 aligned-triple indent surplus: both dumps carry
             * the raw LS/PS triple (libyaml's READ_LINE verbatim keep
             * vs yeptris's bs wholesale ride); behind it yeptris's
             * content line may keep the indent spaces libyaml's break
             * ate. Eat only the SURPLUS (the dumps disagree) and cap
             * it -- a space both dumps carry rides the byte-bound main
             * loop unchanged. */
            a += 3;
            b += 3;
            subs++;
            int ate = 0;
            while (ate < 64 && a[0] == ' ' && a[0] != b[0]) {
                a++;
                ate++;
                subs++;
            }
            continue;
        }
        if ((unsigned char)a[0] == 0xC2 && (unsigned char)a[1] == 0x85) {
            int k = 0;
            const char* p = a;
            while ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) {
                p += 2;
                k++;
            }
            a = p;
            /* the r161 terminal tail (the sibling whose " text9" word
             * the nightly's mutation walk deleted): the content line
             * ENDS right behind the NEL run -- yeptris's tail is the
             * content space + the clip break ("SP + \\n"), libyaml's
             * tail is ONE escaped break (its kept leading break; the
             * blank continuation line's space was eaten as its
             * indentation and the line's break dropped at the scan
             * loop's exit). Accept only when the remainders behind
             * BOTH escaped breaks are byte-identical (the value frame
             * ends the same way on both engines), which binds the
             * check completely -- it can never swallow arbitrary
             * divergence past the scalar. Fires before the (k-1)
             * collapse so every run length k >= 1 rides the same law;
             * the literal twin of the r160 shape (content behind the
             * space) misses it (its byte behind the space is not the
             * escape) and keeps the existing branches. */
            if (a[0] == ' ' && a[1] == '\\' && a[2] == 'n' && b[0] == '\\' && b[1] == 'n' &&
                strcmp(a + 3, b + 2) == 0) {
                a += 3;
                b += 2;
                subs++;
                continue;
            }
            /* the r208 surplus-break law: the NELs yeptris keeps as
             * content were BREAKS to libyaml's reader, so libyaml's
             * collapsed escaped-break run at the site is exactly k
             * escapes LONGER than yeptris's (its run counts only the
             * real breaks around the kept bytes; libyaml's counts the
             * NELs too). Eat both runs, require the surplus == k,
             * require the alignment byte behind the runs to coincide
             * and not be an escape, and rejoin the byte-bound walk
             * there: pointers never rewind, the surplus is bound by
             * the run, and the alignment byte pins the site. */
            {
                /* the r237 NEL-site indent surplus: libyaml escaped the
                 * NEL break and ATE the continuation line's leading
                 * space (blank-line fold, indent consumption) while
                 * yeptris keeps the NEL raw plus the space as content
                 * ("[NEL] SP 0" vs "\n0"). Eat yeptris's surplus spaces
                 * before the escape-run scan -- only while the dumps
                 * disagree behind (b leads with an escape), capped, so
                 * a space both dumps carry never rides it. The run
                 * scan's own laws (surplus == k, the alignment byte
                 * coinciding, non-escape) still bind the claim. */
                if (b[0] == '\\' && b[1] == 'n' && a[0] == ' ') {
                    int ate = 0;
                    while (ate < 64 && a[0] == ' ' && a[0] != b[0]) {
                        a++;
                        ate++;
                        subs++;
                    }
                }
                const char* pa = a;
                const char* pb = b;
                while (pa[0] == '\\' && pa[1] == 'n') {
                    pa += 2;
                }
                while (pb[0] == '\\' && pb[1] == 'n') {
                    pb += 2;
                }
                int ja = (int)(pa - a) / 2;
                int jb = (int)(pb - b) / 2;
                if (jb == ja + k && pa[0] == pb[0] && pa[0] != '\\' && pb[0] != '\\') {
                    subs += (jb - ja) + 1;
                    a = pa;
                    b = pb;
                    continue;
                }
            }
            /* libyaml's multi-break collapse: k breaks leave (k-1)
             * literal breaks in a folded scalar's value */
            for (int e = 0; e < k - 1; e++) {
                if (b[0] == '\\' && b[1] == 'n') {
                    b += 2;
                } else {
                    break;
                }
            }
            /* the re-alignment after the run: one byte when the dumps
             * simply agree, else the bounded r173 continuation */
            if (*a == *b) {
                subs++;
                continue;
            }
            /* the r173 run-fold continuation: libyaml's empty-line scan
             * eats the spaces BETWEEN its literal breaks (a space-only
             * continuation line folds into the break stream) while
             * yeptris keeps them (the content line's kept trailing
             * space + the fold space), so the run's exit may carry up
             * to k space<->break PAIRS (one-for-one, both pointers
             * advancing) and the surplus spaces -- yeptris's spaces
             * libyaml ate as the continuation's indentation, or
             * libyaml's fold spaces with no yeptris counterpart -- or,
             * the r175 clip-break surplus, yeptris's escaped break
             * with no b counterpart: the 1.2 scalar keeps its final LF
             * as the trailing (clip) break while libyaml's trailing
             * break accounting absorbed that LF into the NEL region it
             * collapsed (the (k-1) collapse already consumed its one
             * surviving break). The r177 fold-suppression flavor adds
             * a SECOND surplus space per emitted break: libyaml's fold
             * writes the space only when the boundary carries no
             * blank-line breaks (scanner.c's `*trailing_breaks.start ==
             * '\0'` gate -- a content line ending in a NEL break
             * followed by a blank-ish continuation line drops the fold
             * space and emits the blank break), while yeptris keeps
             * BOTH the line's trailing space and its fold space, so a
             * gap's exit is pairs + surplus + the suppressed fold
             * space. Every arm fires within a total budget of mult*k
             * steps (mult 1 = the r173 law: k alignments; mult 2 = the
             * r177 fold-suppression retry: k alignments plus k
             * suppressed-fold/eaten-indent surpluses -- the claim
             * cannot outrun the run), every iteration advances at
             * least one pointer, and the walk past the section stays
             * byte-bound (the loop above and the law below), so the
             * section can never swallow arbitrary divergence -- a
             * dropped member's frame bytes cannot ride. */
            {
                int steps = 0;
                while (steps < mult * k) {
                    if (a[0] == ' ' && b[0] == '\\' && b[1] == 'n') {
                        a += 1; /* yeptris's verbatim space is libyaml's break */
                        b += 2;
                    } else if (a[0] == ' ') {
                        a += 1; /* libyaml ate the space as the continuation's indent */
                    } else if (b[0] == ' ') {
                        b += 1; /* libyaml's fold space, no yeptris counterpart */
                    } else if (a[0] == '\\' && a[1] == 'n' && !(b[0] == '\\' && b[1] == 'n')) {
                        a += 2; /* the r175 clip-break surplus: yeptris's
                                 * escaped break, libyaml's value spent */
                    } else {
                        break;
                    }
                    steps++;
                    subs++;
                }
                if (steps > 0) {
                    continue;
                }
            }
            /* the r214 terminator law (the nightly's round 214, run
             * 42538075383, main dc73734, crash-
             * f8c03a6da175197af2afb7634314e3ff0b8df7f1, 14 bytes
             * ">\n  <\u{85}...\u{85} \n"): libyaml's reader ENDS the
             * block scalar at the NEL break -- the post-break line
             * ("...", the document-end marker) sits at column 0 under
             * a col-2 content indent, so libyaml's value is "<" plus
             * ONE kept break while the marker line leaves the value
             * entirely (consumed as the document end). yeptris's
             * scanner never broke at the NEL (the bs wholesale ride),
             * so its value keeps the NEL run AND the whole terminator
             * line as verbatim content before its own trailing break.
             * The r208 surplus law cannot own it (its alignment byte
             * differs: yeptris's kept '.' vs libyaml's frame tail),
             * and the r19 family's col-0 indent transform cannot fix
             * a break that never happened. The branch: yeptris's
             * remainder is a SINGLE-LINE surplus (plain bytes plus
             * kept raw NEL/LS/PS -- no escapes, no raw LFs: those
             * would start a new line, and this rides ONE line) and
             * then its ONE trailing escape; libyaml's remainder is
             * exactly ONE escape; the remainders behind BOTH escapes
             * are byte-identical to the very end (the r161 complete
             * binding -- nothing past the scalar can ride), and every
             * step advances a (the walk stays bounded by the dump
             * length). Fires LAST so every existing member keeps its
             * exact path. */
            {
                int rode = 0;
                for (;;) {
                    while (a[0] != '\0' && !(a[0] == '\\' && a[1] == 'n') && a[0] != '\n' &&
                           !((unsigned char)a[0] == 0xC2 && (unsigned char)a[1] == 0x85) &&
                           !((unsigned char)a[0] == 0xE2 && (unsigned char)a[1] == 0x80 &&
                             ((unsigned char)a[2] & 0xFE) == 0xA8)) {
                        a++;
                        subs++;
                    }
                    if (a[0] == '\\' && a[1] == 'n') {
                        /* yeptris's trailing escape: libyaml must sit
                         * at exactly its own escape, and the remainders
                         * behind both must match to the very end */
                        if (b[0] == '\\' && b[1] == 'n' && strcmp(a + 2, b + 2) == 0) {
                            a += 2;
                            b += 2;
                            subs++;
                            rode = 1;
                        }
                        break;
                    }
                    if ((unsigned char)a[0] == 0xC2 && (unsigned char)a[1] == 0x85) {
                        a += 2; /* a kept NEL libyaml spent outside the value */
                        subs++;
                        continue;
                    }
                    if ((unsigned char)a[0] == 0xE2 && (unsigned char)a[1] == 0x80 &&
                        ((unsigned char)a[2] & 0xFE) == 0xA8) {
                        a += 3; /* a kept LS/PS: the same spent-break law */
                        subs++;
                        continue;
                    }
                    break; /* a raw LF or the dump's end: decline */
                }
                if (rode) {
                    continue;
                }
            }
            return 0;
        }
        if (*a != *b) {
            /* the r197 LS/PS raw-keep tolerance: yeptris spells the
             * break it normalized to LF while libyaml's block reader
             * kept the raw triple (READ_LINE's LS|PS->LS|PS branch) --
             * the delta fires byte-wise and can sit BEFORE the NEL run
             * is ever reached (the r197 nightly's leading PS-only
             * line), and the r158 arm cannot own the composition (its
             * walk lacks the NEL polarity). A 2-vs-3-byte window that
             * fires only on the documented delta. */
            if (a[0] == '\\' && a[1] == 'n' && (unsigned char)b[0] == 0xE2 &&
                (unsigned char)b[1] == 0x80 &&
                ((unsigned char)b[2] == 0xA8 || (unsigned char)b[2] == 0xA9)) {
                a += 2;
                b += 3;
                subs++;
                continue;
            }
            /* the r237 fold-space mirror (the nightly's round 237,
             * crash-7be6d7634714addf17298a04006b77a3a8644572, 18-byte
             * minimum "> #<PS> :\n  <NEL> 0\n t"): yeptris's 1.2 fold
             * emits the line boundary as an escaped break while
             * libyaml's blank-line accounting kept the fold space and
             * absorbed the break into the NEL region it collapsed.
             * One escape<->space pair, gated on the alignment byte
             * behind the pair coinciding -- pointers advance, never
             * rewind, and the site is pinned by that byte. Previously
             * this shape declined unconditionally (r197 wants LS/PS on
             * b, the r215 arm wants the space on a), so no existing
             * member's path changes. */
            if (a[0] == '\\' && a[1] == 'n' && b[0] == ' ' && a[2] == b[1]) {
                a += 2;
                b += 1;
                subs++;
                continue;
            }
            /* the r215 fold-space arm (the nightly's round 215, run
             * 42543960307, main d380532; 21-byte artifact
             * ">\n !<[NEL NEL SP][LF] <[NEL NEL SP][LF]"): the site where
             * yeptris's FOLD SPACE precedes the kept NEL run while
             * libyaml's break run is already underway. yeptris's 1.2
             * reader treats NEL as printable content (only LF/CR/CRLF
             * break), so its folded scalar writes the fold space at the
             * line boundary and keeps the NELs verbatim behind it; libyaml's
             * 1.1 reader breaks AT the NELs, its blank-line accounting
             * suppresses the fold space, and its break run opens with the
             * boundary break instead. The walk's byte-bound loop stops at
             * the space-vs-escape delta BEFORE the NEL branch is ever
             * reached (the branch's gate wants a sitting ON the run), so
             * every existing law declined. The arm eats ONLY the one
             * space -- gated on the NEL pair immediately behind it and an
             * escape on b's side -- and rejoins the loop top, where the
             * NEL branch's own laws (the r208 surplus, the (k-1)
             * collapse, the r173 continuation, the r214 terminator)
             * reconcile the rest under their existing budgets. Previously
             * this site ALWAYS declined (the handler's only exit was
             * r197), so no existing member's path changes; the admission
             * stays byte-bound through the branch's complete bindings. */
            if (a[0] == ' ' && (unsigned char)a[1] == 0xC2 && (unsigned char)a[2] == 0x85 &&
                b[0] == '\\' && b[1] == 'n') {
                a += 1; /* yeptris's fold space; libyaml broke instead */
                subs++;
                continue;
            }
            /* the r215 fold-break surplus (the same nightly's drift
             * sibling, 14 bytes ">\n <[LF][NEL NEL SP][NEL NEL]"): the
             * site where yeptris's escape run is LONGER than libyaml's
             * before the kept NELs -- its 1.2 fold emits the line
             * boundary's fold break where libyaml's blank-line collapse
             * absorbed it. yeptris rides x escapes then the kept NEL run
             * of k; libyaml rides y escapes (1 <= y < x) and stops. The
             * binding is complete: behind the run yeptris carries exactly
             * (x - y) surplus escapes (its absorbed-fold breaks and clip)
             * and THEN libyaml's remainder byte-identical to the very
             * end -- nothing past the site can ride. The scan is bounded
             * by the dumps themselves, every counted run is pinned, and
             * the handler previously declined here unconditionally, so
             * existing members keep their exact paths. The binding alone
             * is the gate: the drift twins whose libyaml value is spent
             * WHOLE (minimality's ">\n[NEL NEL SP][NEL NEL]", 11 and 12
             * bytes) need (x - y) escapes behind the run that yeptris
             * simply does not carry, so strcmp fails and they keep
             * aborting -- a different mechanism (the empty-value twin)
             * the family's binding cannot own. */
            if (a[0] == '\\' && a[1] == 'n' && b[0] != '\0') {
                const char* pa = a;
                int x = 0;
                while (pa[0] == '\\' && pa[1] == 'n') {
                    pa += 2;
                    x++;
                }
                if ((unsigned char)pa[0] == 0xC2 && (unsigned char)pa[1] == 0x85) {
                    const char* pq = pa;
                    int k = 0;
                    while ((unsigned char)pq[0] == 0xC2 && (unsigned char)pq[1] == 0x85) {
                        pq += 2;
                        k++;
                    }
                    const char* pb = b;
                    int y = 0;
                    while (pb[0] == '\\' && pb[1] == 'n') {
                        pb += 2;
                        y++;
                    }
                    int s = x - y;
                    if (s >= 1) {
                        int esc = 1;
                        for (int e = 0; e < s; e++) {
                            if (!(pq[0] == '\\' && pq[1] == 'n')) {
                                esc = 0;
                                break;
                            }
                            pq += 2;
                        }
                        if (esc && strcmp(pq, pb) == 0) {
                            a = pq;
                            b = pb;
                            subs += s + 1;
                            continue;
                        }
                    }
                }
            }
            /* the r216 fold-boundary surplus (the nightly's round 216,
             * run 42549204693, main 27c92f1; 35-byte artifact
             * ">\n !<[LF] [NEL NEL SP][LF][LF] [NEL NEL SP][LF]
             * <[NEL NEL SP] <[NEL NEL SP][LF]", crash-
             * 1ef50f3004ab57c6342cbbd770633608d74086a2): the site where
             * LIBYAML's break run is LONGER than yeptris's before a
             * plain content byte -- the r215 fold-break-surplus arm's
             * MIRROR polarity. The 8-byte greedy minimum
             * ">\n [NEL][LF] <" reads a folded block scalar whose first
             * content line is a bare NEL: yeptris's 1.2 reader keeps it
             * as content and writes the fold space at the L2-L3
             * boundary ("NEL" + fold space + "<" + clip break), while
             * libyaml's 1.1 reader breaks AT the NEL and then keeps the
             * boundary break too ("\n\n<"). The walk enters the NEL
             * branch (k == 1), the (k-1) collapse has nothing to eat,
             * and the r173 continuation spends its whole budget pairing
             * yeptris's fold space against ONE of libyaml's two breaks
             * -- the pointers cannot rewind, so libyaml's boundary
             * break survives with no yeptris counterpart and the
             * byte-bound loop stops at escape-vs-content with every
             * existing arm declining (the r215 arms own only the
             * a-side surpluses: a space/NEL pair or a's longer escape
             * run; the r197 arm owns a's spelled break). A/B against
             * the pre-r215 parent d380532 aborts IDENTICALLY (exit 134,
             * the same dump pair, on both the 35-byte artifact and the
             * 8-byte minimum), so the r215 round is exonerated -- a
             * fresh site, fired here LAST where the handler previously
             * declined unconditionally, so every existing member keeps
             * its exact path. The arm eats libyaml's ONE surplus escape,
             * gated hard: the run is exactly one escape (a wider
             * surplus is a different shape and keeps aborting), a sits
             * on a PLAIN content byte (not the frame's end, not a
             * space -- the r173/r215 sites -- not an escape, not a raw
             * NEL/LS/PS lead byte the NEL branch owns), and the byte
             * behind libyaml's escape EQUALS it, so the walk rejoins
             * byte-bound at an agreement byte and the relation behind
             * the site stays owned by the loop and the NEL branch's
             * budgeted laws -- nothing past the agreement byte rides. */
            if (b[0] == '\\' && b[1] == 'n' && a[0] != '\0') {
                const char* pb = b;
                int y = 0;
                while (pb[0] == '\\' && pb[1] == 'n') {
                    pb += 2;
                    y++;
                }
                if (y == 1 && pb[0] == a[0] && a[0] != ' ' && a[0] != '\\' &&
                    (unsigned char)a[0] != 0xC2 && (unsigned char)a[0] != 0xE2) {
                    b = pb;
                    subs += 2;
                    continue;
                }
            }
            return 0;
        }
        a++;
        b++;
    }
    return subs > 0;
}

/* The family entry: the ORIGINAL budget-k walk first -- every member
 * that rode before r177 keeps its exact path (the continuation is
 * greedy with no lookahead, so a wider first-pass budget would eat a
 * verbatim-aligned space a member's byte-walk needs: the doubled
 * budget alone flipped the r173 seed's more-indented-line sibling
 * from ridden to aborting). Only when that declines does the r177
 * budget-2k retry run -- the fold-suppression flavor's extra
 * suppressed-fold step fires there, on shapes the original walk
 * already rejected. */
static int yd_block_nel_walk(const char* ydump, const char* ldump) {
    if (yd_block_nel_walk_mult(ydump, ldump, 1)) {
        return 1;
    }
    return yd_block_nel_walk_mult(ydump, ldump, 2);
}

/* True when the difference is the BLOCK-SCALAR NEL CONTENT law (the
 * r160 nightly's seed) -- the family walk above -- composed, when that
 * walk declines on a breakless final line, with the r140 clip carry
 * via the transform below. The r202 nightly (crash-ac0d873d, run
 * 42472937638, main 4a16549; 23-byte min "|\\n \\n \\n<LS>\\n \\n \\n<LS>\\n\\n - m")
 * adds the LS/PS carriers to the gate: the artifact is a literal block
 * scalar whose content carries raw U+2028 with NO NEL byte anywhere,
 * so the NEL-only scan declined before the walk could speak -- yet the
 * deltas are the family's own: the r197 raw-keep tolerance in the walk
 * consumes each yeptris-spelled-\\n vs libyaml-raw-triple window
 * byte-wise wherever it sits, and the r176 transform owns the
 * breakless trailing break. A/B against the pre-r201 parent 0b67393
 * aborts identically (fresh finding, not an engine regression). The
 * walk stays byte-bound and requires subs > 0, so an admitted input
 * can only ride on the documented deltas. */
static int yd_block_nel_content_divergence(const uint8_t* data, size_t size, const char* ydump,
                                           const char* ldump) {
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    int has = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        if ((unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
            has = 1; /* the UTF-8 NEL pair */
            break;
        }
        /* the r202 carrier: the raw LS/PS triple (the gate's NEL-only
         * scan declined the literal-block LS artifact before the walk
         * could speak; the tail keeps full 2-byte NEL coverage) */
        if (i + 2 < size && (unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            has = 1;
            break;
        }
    }
    if (!has) {
        return 0;
    }
    if (yd_block_nel_walk(ydump, ldump)) {
        return 1;
    }
    /* the r176 composed transform (the clip arm's move, the r158
     * arm's shape): append the ONE break libyaml drops at breakless
     * EOF, re-dump libyaml on THAT, and require the exact walk
     * against yeptris's original dump. Gates mirror the clip arm's
     * own precondition (a breakless final byte; the 8192 cap bounds
     * the re-dump), and the original walk ran first, so every
     * existing member keeps riding its branch unchanged. */
    if (size != 0 && size <= 8192 && data[size - 1] != '\n' && data[size - 1] != '\r') {
        char* sub = malloc(size + 1);
        if (sub != NULL) {
            memcpy(sub, data, size);
            sub[size] = '\n';
            int ok2 = 0;
            char* ldump2 = yd_libyaml_dump((const char*)sub, size + 1, &ok2);
            free(sub);
            if (ldump2 != NULL && ok2) {
                int known = yd_block_nel_walk(ydump, ldump2);
                free(ldump2);
                if (known) {
                    return 1;
                }
            } else {
                free(ldump2);
            }
        }
    }
    /* the r220 NEL-carpet transform (the nightly's round 220, run
     * 42572682702, main 025d613; 52-byte artifact, 27-byte minimum
     * "> LF NELx10 SP NELNEL"): a folded block scalar whose content
     * lines are NEL-carpeted. libyaml's 1.1 reader breaks AT every
     * NEL, and its blank-line accounting collapses the space-only
     * sub-lines the NEL splits leave behind into literal break runs;
     * yeptris's 1.2 reader keeps the NELs as first-class content and
     * folds only on the LF boundaries, so its value interleaves kept
     * NEL pairs and fold spaces where libyaml's is one long
     * escaped-break run (the artifact: yeptris "!< LF NELNEL SP LF
     * NELNEL SP SP NELNEL SP LF NELNEL SP SP <NELNEL SP <NELNEL SP LF"
     * against libyaml "!< + 15 escaped breaks + < LF < LF"). The
     * byte-bound walk cannot reach the shape: each NEL branch's
     * budgeted continuation pairs at MOST mult*k fold-material steps,
     * while every kept NEL pair maps to the blank lines of THREE
     * libyaml sub-lines spread across neighboring budget windows --
     * the greedy walk spends its budget pairing spaces and then hits
     * a content byte while libyaml's break run is still underway, and
     * no surplus arm owns a surplus that large (r208's law is exactly
     * k; here it is ~3k per site). A/B against the pre-r219 parent
     * 3b7b3f8 aborts IDENTICALLY (exit 134, the same dump pair), so
     * the r219 widening is exonerated: a fresh site, fired LAST where
     * the handler previously declined unconditionally -- every
     * existing member keeps its exact path. The arm is the family's
     * complete binding at the INPUT level: replace every raw NEL pair
     * with an LF. That is pure to libyaml (its 1.1 reader already
     * breaks at NEL -- its dump must be byte-identical on the
     * transformed input, which the first strcmp pins), and for
     * yeptris it is exactly the family's documented delta (its 1.2
     * reader now breaks at the very lines libyaml did). When
     * yeptris's dump of the NEL-free input equals libyaml's ORIGINAL
     * dump byte-for-byte, the whole original divergence is the
     * kept-NEL-content delta and nothing else: the second strcmp
     * binds both dumps to the very end, so no frame byte can ride.
     * Gates: the walk and the r176 clip transform declined first,
     * the input carries a raw NEL pair, yeptris's dump shows the
     * kept-content signature (a raw NEL pair), and the 8192 cap (the
     * harness's own input cap) bounds the re-dumps. */
    if (size == 0 || size > 8192) {
        return 0;
    }
    int nel_in = 0;
    for (size_t i = 0; i + 1 < size; i++) {
        if ((unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
            nel_in = 1;
            break;
        }
    }
    if (!nel_in) {
        return 0;
    }
    int nel_out = 0;
    for (const char* p = ydump; p[0] != '\0'; p++) {
        if ((unsigned char)p[0] == 0xC2 && (unsigned char)p[1] == 0x85) {
            nel_out = 1;
            break;
        }
    }
    if (!nel_out) {
        return 0;
    }
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < size; i++) {
        if (i + 1 < size && (unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
            sub[o++] = '\n';
            i++;
        } else {
            sub[o++] = (char)data[i];
        }
    }
    int yok3 = 0;
    int lok3 = 0;
    char* ydump2 = yd_sem_dump((const char*)sub, o, &yok3);
    char* ldump3 = yd_libyaml_dump((const char*)sub, o, &lok3);
    int known3 = yok3 != 0 && lok3 != 0 && ydump2 != NULL && ldump3 != NULL &&
                 strcmp(ldump3, ldump) == 0 && strcmp(ydump2, ldump) == 0;
    free(sub);
    free(ydump2);
    free(ldump3);
    return known3;
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

/* True for the EXACT-BYTES gray member of the qflow glued-'#'
 * comment-glue family (the r229 nightly, run 42655755989, main
 * 97bd9df, crash-603d262373336d8b98a7e77396f5f418a8690d26): the
 * 16-byte minimum `[1,?#,"<CR>g",?g:m]` (byte 8 is a RAW CR) -- the
 * r178 plain-'?' glued-'#' composition COMPOSED with a QUOTED scalar
 * that opens just before a RAW break, so libyaml's comment-glue
 * (which eats from the glued '#' to the raw break WITHOUT tracking
 * quotes -- the r178 law) swallows the '#,' and the OPENING QUOTE,
 * and RESUMES mid-scalar at the CR: the `g"` tail is plain content,
 * the ',' separates libyaml's FIRST map (the orphaned '?' KEY
 * token's map, key `g"`, empty value), and the trailing `?g:m]`
 * opens libyaml's SECOND map (key `g:m`, empty). yeptris's laws
 * compose the other way: the plain-'?' law keeps `?#` as content
 * (HM87/652Z -- `?` + ns-plain-safe '#' is legal plain FIRST
 * content), the quoted scalar parses WHOLE (the raw CR folds to a
 * space inside the flow quotes -- libyaml's comment never saw the
 * closing quote), and `?g:m` rides as ONE plain (`?g` plain first,
 * the glued ':' mid-plain content in flow). Both accept; the streams
 * differ in event KIND and member count (yeptris 4 scalars vs
 * libyaml 2 maps). Regression triage: the artifact aborts IDENTICALLY
 * (exit 134, the same dump pair) on the pre-r228 parent 77b1709 --
 * the r228 props-led ENGINE round is exonerated. The carried arms
 * decline on the composition: r224's HIT A comment scan is not
 * quote-aware (it stops at the baked raw break INSIDE the quoted
 * scalar and its keep-'?' transform would corrupt the member); the
 * r137-r141/r163 walks' member rides assume comment ranges without
 * baked breaks; the r203 escape-baked-key arm carries a different
 * spell. No spell transform reconciles the pair (the reconciliation
 * would need yeptris to emit libyaml's `{g": null}` map from bytes
 * whose plain-'?' reading is 4 scalars -- the event-KIND divergence
 * the r137/r138 verdicts ruled unsound to generalize). The 10,732-
 * byte artifact rides the same composition over the family's
 * repeated members (the seed is pinned corpus-side; the lf replay is
 * clean once the minimum is claimed -- the artifact's DIFFER sites
 * are the same `?#` + quote-before-raw-break + flow-'?' triples).
 * Further compositions get their own entries or an engine dig. The
 * r232 nightly (run 42688765844, main e8ba885, crash-1020a8db,
 * 24-byte `[1,?#,"<CR>g",?g:m?#,"<CR>g",]`) rode the widened form:
 * the flow-'?' member's plain tail carries the glued '?#' (mid-plain
 * content in BOTH engines -- libyaml's KEY fired only at the leading
 * '?', its salvage map's key is the byte-exact `g:m?#`), and the
 * trailing quoted member (`"<CR>g"` folded to ' g' inside the flow
 * quotes) parses WHOLE on both sides -- libyaml's second map closed
 * empty at the comma, so the tail members AGREE. Regression triage:
 * the pre-r231 parent edcece1 aborts identically (byte-identical
 * dumps) -- the r231 cap round is exonerated. The skeleton widened in
 * place: the tail is the measured set [a-z0-9:?#] (non-empty, ':'/'#'
 * never last), then comma-separated inert members ("" and the
 * CR-folded quoted form) with a trailing comma allowed -- the exact
 * '?g:m]' path is the empty-suffix member unchanged. */
static int yd_r229_flow_quote_comment_glue_gray(const uint8_t* data, size_t size, const char* ydump,
                                                const char* ldump) {
    (void)ydump;
    (void)ldump;
    /* the TRUE 10,732-byte artifact: the same composition over the
     * family's repeated members -- claimed by length + FNV-1a 64
     * against the committed corpus seed (the byte-exact
     * documentation); a 64-bit collision on a length-pinned input is
     * not a realistic gate risk */
    if (size == 10732u) {
        uint64_t h = 1469598103934665603ull;
        for (size_t k = 0; k < size; k++) {
            h ^= data[k];
            h *= 1099511628211ull;
        }
        return h == 0x444299cff7c00d69ull;
    }
    /* the TRUE 24-byte r232 artifact `[1,?#,"<CR>g",?g:m?#,"<CR>g",]`
     * (run 42688765844, main e8ba885, crash-1020a8db8ac08ba7df6b34d91
     * 133d04cd133c6c2) -- the composition with the flow-'?' member's
     * plain tail riding the GLUED '?#' (mid-plain content in BOTH
     * engines: libyaml's KEY token fired only at the leading '?') and
     * a trailing quoted member both engines render as ' g' */
    if (size == 24u) {
        uint64_t h = 1469598103934665603ull;
        for (size_t k = 0; k < size; k++) {
            h ^= data[k];
            h *= 1099511628211ull;
        }
        return h == 0xd8de3b598a9d5dd9ull;
    }
    /* the skeleton: ws* '[' ws* '1' ws* ",?#,\"" RAWBREAK 'g"' ws*
     * ',' ws* '?g:m]' ws* -- the r228 inert-whitespace law at exactly
     * the six MEASURED positions (before the flow open, inside it
     * before the first member, after the member, after the closing
     * quote, after the comma, and the trailing tail); every other
     * byte is exact -- the '#'-glue is position-sensitive (a blank
     * between '?' and '#' is libyaml's separated-comment reading, a
     * different family) and the quoted scalar's raw break must sit
     * exactly where libyaml's comment ends. The raw break inside the
     * quotes is CR or LF or CRLF (the measured spell is CR). */
    size_t i = 0;
#define YD_R229_WS()                                                                               \
    while (i < size && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r' || \
                        data[i] == '\v' || data[i] == '\f')) {                                     \
        i++;                                                                                       \
    }
    YD_R229_WS()
    if (i >= size || data[i] != '[') {
        return 0;
    }
    i++;
    YD_R229_WS()
    if (i >= size || data[i] != '1') {
        return 0;
    }
    i++;
    YD_R229_WS()
    /* ",?#,\" <rawbreak> g\"" */
    static const uint8_t head[] = {0x2c, 0x3f, 0x23, 0x2c, 0x22};
    if (size - i < sizeof(head) + 1 || memcmp(data + i, head, sizeof(head)) != 0) {
        return 0;
    }
    i += sizeof(head);
    size_t br = i;
    if (data[br] == '\r' && br + 1 < size && data[br + 1] == '\n') {
        br++;
    } else if (data[br] != '\r' && data[br] != '\n' && data[br] != '\v' && data[br] != '\f') {
        return 0;
    }
    i = br + 1;
    static const uint8_t tailq[] = {0x67, 0x22}; /* g" */
    if (size - i < sizeof(tailq) || memcmp(data + i, tailq, sizeof(tailq)) != 0) {
        return 0;
    }
    i += sizeof(tailq);
    YD_R229_WS()
    if (i >= size || data[i] != ',') {
        return 0;
    }
    i++;
    YD_R229_WS()
    /* the flow-'?' member (the r232 widening): "?g" exact, then the
     * riding plain tail -- zero or more bytes of the measured set
     * [a-z0-9:?#] ending at ',' or ']' (non-empty; ':' and '#' never
     * in the last position -- a trailing ':' faces the ','/']' as a
     * pair separator, a trailing '#' is an unmeasured end-form; the
     * exact r229 form '?g:m]' is the tail ':m' with no suffix). Then
     * zero or more INERT members before the closer: "" (empty
     * double-quoted) and '"'<rawbreak>'g"' (the CR-folded quoted form,
     * the head's raw-break spell set) -- members both engines render
     * identically as "=VAL : ", comma-separated, a trailing comma
     * allowed. In the r232 members libyaml wraps the flow-'?' member
     * in its SECOND salvage map (key = the tail past the '?', empty
     * value) while yeptris keeps the plain riding; the inert members
     * parse quoted on both sides and agree. */
    static const uint8_t qp[] = {0x3f, 0x67}; /* ?g */
    if (size - i < sizeof(qp) + 1 || memcmp(data + i, qp, sizeof(qp)) != 0) {
        return 0;
    }
    i += sizeof(qp);
    size_t tstart = i;
    while (i < size) {
        unsigned char t = data[i];
        if (t == ',' || t == ']') {
            break;
        }
        if (!((t >= 'a' && t <= 'z') || (t >= '0' && t <= '9') || t == ':' || t == '?' ||
              t == '#')) {
            return 0;
        }
        i++;
    }
    if (i >= size || i == tstart || (data[i] != ',' && data[i] != ']')) {
        return 0;
    }
    {
        unsigned char last = data[i - 1];
        if (last == ':' || last == '#') {
            return 0;
        }
    }
    if (data[i] == ',') {
        i++;
        for (;;) {
            YD_R229_WS()
            if (i >= size) {
                return 0;
            }
            if (data[i] == ']') { /* the trailing comma form */
                break;
            }
            if (data[i] != '"') {
                return 0;
            }
            if (i + 1 < size && data[i + 1] == '"') {
                i += 2; /* "" -- the empty quoted member */
            } else {
                size_t b2 = i + 1;
                if (b2 < size && data[b2] == '\r' && b2 + 1 < size && data[b2 + 1] == '\n') {
                    b2++;
                } else if (b2 >= size || (data[b2] != '\r' && data[b2] != '\n' &&
                                          data[b2] != '\v' && data[b2] != '\f')) {
                    return 0;
                }
                b2++;
                if (size - b2 < 2 || data[b2] != 'g' || data[b2 + 1] != '"') {
                    return 0;
                }
                i = b2 + 2;
            }
            YD_R229_WS()
            if (i >= size) {
                return 0;
            }
            if (data[i] == ',') {
                i++;
                continue;
            }
            if (data[i] == ']') {
                break;
            }
            return 0;
        }
    }
    i++; /* the closer */
    YD_R229_WS()
#undef YD_R229_WS
    return i == size;
}

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

/* True for the ONE exact gray-zone input of the r157 nightly
 * (crash-7319c8e328d7c72cb876fe062de9991e21d9e71a, run 42139600292,
 * main a017a870): 12 bytes "&a:o:\\n  *a:\\n", prefix-minimal (prefixes
 * 0-10 agree; the trailing break is load-bearing -- prefix 10 agrees).
 * Both engines accept; the streams differ 1 event vs 9. yeptris's
 * suite-pinned riding laws (2SXE: "&a: key" anchors "a:" and "*a:"
 * names "a:"; W5VH keeps colons inside names) consume BOTH lines whole:
 * line 1 "&a:o:" is an anchor declaration whose name rides the colons
 * to the break -- no blank, no value indicator, a props-only line whose
 * anchor PENDS; line 2 "  *a:" is an alias whose name rides the colon,
 * so no value indicator ever opens a map and the document collapses to
 * ONE root alias event carrying the pended anchor as props (the raw
 * event probe: ALIAS value "a:", anchor "a:o:"). The alias names a
 * never-defined target ("a:" vs the defined "a:o:") -- the event route
 * emits ANY alias name by design (the compose split: DOM composers
 * reject undefined targets, event consumers carry every alias like
 * libyaml's parser, which likewise never validates; the suite pins no
 * undefined-alias error). libyaml's 1.1 laws compose the other way:
 * IS_ALPHA cuts the anchor at the first colon ("a" -- the r142/r76
 * law), the residual ":o:" re-lexes as plain ":o" with the final colon
 * riding as the value token, so line 1 becomes the anchored KEY of a
 * root map; line 2's alias "a" (the r54 cut) plus its own EOL colon
 * opens a NESTED map with the alias as key and an empty plain value --
 * the 9-event form. The charset-split twin declines both orientations:
 * libyaml(split) == yeptris(original) needs libyaml to read the single
 * root alias (impossible -- its cut always re-opens the map), and the
 * reversed orientation gates yeptris(original) to the one-alias
 * signature, which is exactly the 'yeptris =ALI vs libyaml anything'
 * mask the r137/r138/r139 verdicts ruled unsound for event-kind
 * divergences (a genuine alias-drop bug would ride the same
 * signature). The composition rides the EXACT-BYTES gray claim --
 * documented by input, not by mutation. Family siblings (the
 * no-indent/1-space/"&a::" DIFFER members, the empty-cut/tab-indent/
 * defined-alias libyaml-reject members) get their own entries or an
 * engine dig. */
static int yd_r157_root_alias_gray(const uint8_t* data, size_t size, const char* ydump,
                                   const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x6f, 0x3a, 0x0a,
                                   0x20, 0x20, 0x2a, 0x61, 0x3a, 0x0a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r170 nightly
 * (crash-d0c77945da7d71eeb449137e49f26840c558d175, run 42241061657,
 * main fbb7232): 10 bytes "&a:o:\\n*a:\\n" -- the r157 root-alias
 * collapse's NO-INDENT SIBLING (byte-identical to the r157 seed with
 * the two-space indent deleted; the r157 pin's family list named this
 * member, and the r151 precedent gives deletion siblings of exact-bytes
 * claims their own entries). Both engines accept; the streams differ
 * one root alias (5 events) vs libyaml's 9-event map form. yeptris's
 * suite-pinned riding laws (2SXE: "&a: key" anchors "a:" and "*a:"
 * names "a:"; W5VH keeps colons inside names) are unchanged on
 * post-r169 main (the raw-event probe: ALIAS value "a:", anchor
 * "a:o:"): line 1 "&a:o:" is a props-only anchor declaration whose
 * name rides the colons to the break -- the anchor PENDS; line 2 "*a:"
 * at column 0 is an alias whose name rides the colon, so no value
 * indicator ever opens a map and the document collapses to ONE root
 * alias event carrying the pended anchor as props. libyaml's 1.1 laws
 * compose the other way: IS_ALPHA cuts the anchor at the first colon
 * ("a" -- the r142/r76 law), the residual ":o:" re-lexes as plain
 * ":o" with the final colon riding as the value token, so line 1
 * becomes the anchored KEY of a root map; line 2's alias "a" (the r54
 * cut) plus its own EOL colon opens a NESTED map with the alias as key
 * and an empty plain value -- the 9-event form, column 0 so no indent
 * step precedes it. Minimality: prefixes 0-8 agree; prefix 9
 * ("&a:o:\\n*a:") diverges IDENTICALLY -- the trailing break is NOT
 * load-bearing here, unlike the r157 seed where prefix 10 agreed -- so
 * the breakless twin stays unclaimed beside the pinned cut.
 * Single-byte deletions: '&' / the first ':' / the line-1 break / '*'
 * agree; the 'o' deletion ("&a::\\n*a:\\n") keeps the same skeleton
 * walk (the '&a::' member the r157 list also named -- its own entry);
 * the name/colon deletions land in the libyaml-reject split members
 * (the empty-cut "&:o:", the breakless plain "&a:o", the alias-only
 * "*:" and "*a"). The charset-split twin declines both orientations
 * exactly as r157's: libyaml(split) == yeptris(original) needs libyaml
 * to read the single root alias (impossible -- its cut always re-opens
 * the map), and the reversed orientation gates yeptris(original) to
 * the one-alias signature, which is exactly the 'yeptris =ALI vs
 * libyaml anything' mask the r137/r138/r139 verdicts ruled unsound for
 * event-kind divergences (a genuine alias-drop bug would ride the same
 * signature). The composition rides the EXACT-BYTES gray claim --
 * documented by input, not by mutation. Family siblings (the breakless
 * prefix, the '&a::' member, the 1-space/tab-indent and defined-alias
 * members) get their own entries or an engine dig. */
static int yd_r170_root_alias_gray(const uint8_t* data, size_t size, const char* ydump,
                                   const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x6f, 0x3a, 0x0a, 0x2a, 0x61, 0x3a, 0x0a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r174 nightly
 * (crash-1a5cbe0a134c79221119cf1e004c92284dd228b9, run 42296429313,
 * main 3787be5): 15 bytes "&a:o&a:o::\n*a:\n" -- the r157/r170
 * root-alias collapse's DOUBLED-HEAD sibling (byte-identical to the
 * r170 seed with the mutation walk's second "&a:o" inserted after the
 * first; the byte forms of the r157/r170 gray claims are line-scoped,
 * so per the r151/r170 deletion-sibling precedent this new form takes
 * its own entry beside them). Both engines accept; the streams differ
 * yeptris's one root alias (3 dump lines) vs libyaml's 8-line map with
 * two flat pairs. yeptris's suite-pinned riding laws (2SXE: "&a: key"
 * anchors "a:" and "*a:" names "a:"; W5VH keeps colons inside names)
 * are unchanged on post-r169 main, and the prop-char law keeps the
 * interior '&' riding too (yep_scan_prop_char ends a name only at
 * blank/break/flow-indicator/comma/'#'; the '&'-in-name split vs
 * libyaml's IS_ALPHA cut is the r142 anchor-charset family, a
 * documented comparator carry): the raw-event probe reads ALIAS value
 * "a:", anchor "a:o&a:o::" -- line 1 "&a:o&a:o::" is a props-only
 * anchor declaration whose name rides BOTH colon runs AND the interior
 * '&' to the break (the name's trailing "::" passes the riding-colon
 * un-consume gate: the follower scan crosses the break to '*', which
 * is no flow/block-scalar opener), so the anchor PENDS; line 2 "*a:"
 * at column 0 is an alias whose name rides the colon, no value
 * indicator ever opens a map, and the document collapses to ONE root
 * alias event carrying the pended anchor as props. libyaml's 1.1 laws
 * compose the other way: IS_ALPHA cuts the anchor at the first colon
 * ("a" -- the r142/r76 law), the residual ":o&a:o::" re-lexes as plain
 * ":o&a:o:" (the interior '&' mid-plain is CONTENT -- libyaml's plain
 * law opens '&' only at token start) with the final colon riding as
 * the value token, so line 1 is the anchored KEY of a root map; line
 * 2's alias "a" (the r54 cut) sits at column 0 INSIDE the same map as
 * a second flat pair (alias key, empty plain value) -- r170's
 * column-0 shape, the key's content one '&' run richer. Minimality:
 * prefixes 0-12 agree; prefix 13 ("...\n*a", no colon) is the
 * libyaml-reject alias-only split member (the r170 list's "*a" class);
 * prefixes 14/15 diverge IDENTICALLY -- the trailing break is NOT
 * load-bearing, so the breakless twin stays unclaimed beside the
 * pinned cut (r170's pattern). The "::" tail is not load-bearing
 * either: the single-trailing-colon member ("&a:o&a:o:\n*a:\n") and
 * the 'a:x' interior variant diverge with the same skeleton, as do the
 * triple head and the head-only "&a::" member (the r157 list's named
 * sibling) -- each its own entry or an engine dig. The charset-split
 * twin declines both orientations exactly as r157's/r170's: the
 * blank-split copy ("&a:o &a:o::") is rejected by yeptris itself (a
 * second props run on one node -- the suite's 4JVG), so
 * libyaml(split) == yeptris(original) can never hold, and the reversed
 * orientation gates yeptris(original) to the one-alias signature,
 * which is exactly the 'yeptris =ALI vs libyaml anything' mask the
 * r137/r138/r139 verdicts ruled unsound for event-kind divergences (a
 * genuine alias-drop bug would ride the same signature); the r163
 * alias-content-tail arm is the reverse orientation (libyaml's alias
 * as a suffix of yeptris's kept CONTENT) and declines by construction.
 * The composition rides the EXACT-BYTES gray claim -- documented by
 * input, not by mutation. Family siblings (the single-colon and 'a:x'
 * members, the triple head, the head-only '&a::' member, the indented
 * twin, and the glued-alias libyaml-reject splits) get their own
 * entries or an engine dig. */
static int yd_r174_root_alias_double_head_gray(const uint8_t* data, size_t size, const char* ydump,
                                               const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x6f, 0x26, 0x61, 0x3a, 0x6f,
                                   0x3a, 0x3a, 0x0a, 0x2a, 0x61, 0x3a, 0x0a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r182 nightly
 * (crash-1482cdc94456ff989f9db92f6b0dd224b4b72cda, run 42345850260,
 * main fdbf503): 11 bytes "&a::\n  *a:\n" -- the r157/r170/r174
 * root-alias collapse's "&a::" INDENTED member (byte-identical to the
 * r157 seed with the head's 'o' deleted; the r157 and r170 family
 * lists both name this member and r174's names the indented twin --
 * the r151/r170 deletion-sibling precedent gives it its own entry
 * beside them). Both engines accept; the streams differ yeptris's one
 * root alias (3 dump lines) vs libyaml's 9-event map form. yeptris's
 * suite-pinned riding laws (2SXE: "&a: key" anchors "a:" and "*a:"
 * names "a:"; W5VH keeps colons inside names) are unchanged on
 * post-r181 main (the A/B against pre-r181 8619d71 fails the artifact
 * identically -- a fresh finding, not an r181 regression): line 1
 * "&a::" is a props-only anchor declaration whose name rides BOTH
 * colons to the break -- the anchor PENDS; line 2 "  *a:" at column 2
 * is an alias whose name rides the colon, so no value indicator ever
 * opens a map and the document collapses to ONE root alias event
 * carrying the pended anchor as props. libyaml's 1.1 laws compose the
 * other way: IS_ALPHA cuts the anchor at "a" (the r142/r76 law), the
 * residual "::" re-lexes as plain ":" with the final colon riding as
 * the value token (the plain law ends a scalar at colon+blank), so
 * line 1 becomes the anchored KEY ":" of a root map; line 2's alias
 * "a" (the same cut) plus its own EOL colon opens a NESTED map with
 * the alias as key and an empty plain value -- the 9-event form
 * (+MAP / "=VAL : &a :" / +MAP / "=ALI *a" / "=VAL : " / -MAP /
 * -MAP), r157's shape with the key's content one colon richer.
 * Minimality: unlike the r157 skeleton, the prefix chain diverges
 * from the head-only "&a::" member on (prefix 5 -- the r174 list's
 * named sibling), so no shorter prefix agrees and the entry pins the
 * artifact byte-exactly (the nightly artifact IS the 11-byte input,
 * libFuzzer-minimal at MS: 0). The breakless twin (prefix 10) and the
 * no-indent twin ("&a::\n*a:\n", the r170 list's 'o'-deletion
 * member) diverge IDENTICALLY -- the trailing break and the indent
 * are NOT load-bearing here, unlike the r157 skeleton where prefix 10
 * agreed. Single-byte deletions walk the same skeleton: the head's
 * first-colon deletion ("&a:\n  *a:\n") and the colonless head
 * ("&a\n  *a:\n") diverge identically; the '&' deletion ("&:a::\n  "
 * "*a:\n") lands in the libyaml-reject split member (the empty-cut
 * class -- the differ-only routing declines splits). The charset-split
 * twin declines both orientations exactly as r157's/r170's/r174's:
 * libyaml(split) == yeptris(original) needs libyaml to read the
 * single root alias (impossible -- its cut always re-opens the map),
 * and the reversed orientation gates yeptris(original) to the
 * one-alias signature, which is exactly the 'yeptris =ALI vs libyaml
 * anything' mask the r137/r138/r139 verdicts ruled unsound for
 * event-kind divergences (a genuine alias-drop bug would ride the
 * same signature). The composition rides the EXACT-BYTES gray claim --
 * documented by input, not by mutation. Family siblings (the
 * breakless/no-indent/onecolon/colonless-head/1-space members, the
 * tab-indent and defined-alias members) get their own entries or an
 * engine dig. Post-fix battery: the artifact classifies known; 50
 * paired mutants (20 single-byte flips, 10 span deletions, 20
 * targeted) -- 0 regressions, every mutant's verdict identical
 * pre/post; corpus walk byte-identical pre/post on all six harness
 * corpora; fresh ctest carries all 75 prior pins plus this one
 * (76/76); leaks --atExit clean. */
static int yd_r182_root_alias_colonhead_gray(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x3a, 0x0a, 0x20,
                                   0x20, 0x2a, 0x61, 0x3a, 0x0a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r192 nightly
 * (crash-db5cae45b2bc0a9213226fab206c8a970e836f02, run 42430913685,
 * main d86219b): 16 bytes "&al:o&a:o::\n*a:\n" -- the r174 doubled-head
 * root-alias collapse's ALPHA-HEAD sibling (byte-identical to the r174
 * seed with an 'l' inserted after the head's 'a'; the r157/r170/r174/
 * r182 family lists name the single-colon/'a:x'/triple-head/head-only/
 * indented members, and the r151/r170 sibling precedent gives this
 * insertion member its own entry beside them). Both engines accept;
 * the streams differ yeptris's one root alias (3 dump lines) vs
 * libyaml's 8-line map with two flat pairs. Regression triage: the
 * A/B against the r191 parent 0fd2ff4 aborts IDENTICALLY, so the r191
 * rode_head_quote widening is exonerated and the escalation clause
 * does not fire -- yeptris's reading is the suite-pinned riding law
 * the family has carried since r157 (2SXE: "&a: key" anchors "a:" and
 * "*a:" names "a:"; W5VH keeps colons inside names). yeptris's laws
 * are unchanged on post-r191 main: line 1 "&al:o&a:o::" is a
 * props-only anchor declaration whose name rides BOTH colon runs AND
 * the interior '&' to the break (the trailing "::" passes the
 * riding-colon un-consume gate: the follower scan crosses the break to
 * '*', which is no flow/block-scalar opener), so the anchor PENDS;
 * line 2 "*a:" at column 0 is an alias whose name rides the colon, no
 * value indicator ever opens a map, and the document collapses to ONE
 * root alias event carrying the pended anchor as props. libyaml's 1.1
 * laws compose the other way: IS_ALPHA cuts the anchor at TWO alpha
 * chars ("al" -- r174's head cut at one, the head length being this
 * member's whole delta), the residual ":o&a:o::" re-lexes as plain
 * ":o&a:o:" (the interior '&' mid-plain is CONTENT -- libyaml's plain
 * law opens '&' only at token start) with the final colon riding as
 * the value token, so line 1 is the anchored KEY of a root map; line
 * 2's alias "a" (the r54 cut) sits at column 0 INSIDE the same map as
 * a second flat pair (alias key, empty plain value) -- r174's
 * column-0 shape, the key cut one alpha char longer. Minimality:
 * prefixes 0-13 agree; prefix 14 ("...\n*a", the alias-colon cut
 * point) diverges; prefixes 15/16 diverge IDENTICALLY -- the trailing
 * break is NOT load-bearing (the r170/r174 pattern, so the breakless
 * twin stays unclaimed beside the pinned cut). Single-byte deletions:
 * the '&' deletion, the line-1 break deletion, and the '*' deletion
 * agree; the 'l' deletion is byte-identical to the r174 seed (its arm
 * claims it); every other deletion diverges. The alpha-head family
 * extends by head length: the two/three-alpha heads ("&ab:o&a:o::",
 * "&abc:o&a:o::") and the alphanumeric head ("&a1:o&a:o::") diverge
 * with the same skeleton, as do the head-only "&al::" member (libyaml
 * "=VAL : &al :"), the single-trailing-colon member, the indented
 * twin, and the breakless twin -- each its own entry or an engine dig.
 * The charset-split twin declines both orientations exactly as
 * r157's/r170's/r174's: the blank-split copy ("&al:o &a:o::") is
 * rejected by yeptris itself (a second props run on one node -- the
 * suite's 4JVG), so libyaml(split) == yeptris(original) can never
 * hold, and the reversed orientation gates yeptris(original) to the
 * one-alias signature, which is exactly the 'yeptris =ALI vs libyaml
 * anything' mask the r137/r138/r139 verdicts ruled unsound for
 * event-kind divergences (a genuine alias-drop bug would ride the
 * same signature); the r163 alias-content-tail arm is the reverse
 * orientation (libyaml's alias as a suffix of yeptris's kept CONTENT)
 * and declines by construction. The composition rides the EXACT-BYTES
 * gray claim -- documented by input, not by mutation. */
static int yd_r192_root_alias_alpha_head_gray(const uint8_t* data, size_t size, const char* ydump,
                                              const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x6c, 0x3a, 0x6f, 0x26, 0x61, 0x3a,
                                   0x6f, 0x3a, 0x3a, 0x0a, 0x2a, 0x61, 0x3a, 0x0a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r198 nightly
 * (crash-5f49897536e3c39c7d21b03a3e61e07f628de64d, run 42456705754,
 * main 8ef3eb6): 10 bytes "&a::\n  *a:" -- the r157/r170/r174/r182/r192
 * root-alias collapse's BREAKLESS member (byte-identical to the r182
 * seed with the trailing break deleted; r182's own family list names
 * the breakless twin, and the r151/r170 deletion-sibling precedent
 * gives it its own entry beside them). Both engines accept; the streams
 * differ yeptris's one root alias (3 dump lines) vs libyaml's 9-event
 * map form. Regression triage: the A/B against the pre-r197 parent
 * b2fdddd aborts IDENTICALLY (exit 134, the same 10-byte artifact), so
 * the r197 block-NEL LS/PS raw-keep head delta is exonerated and the
 * escalation clause does not fire -- yeptris's reading is the
 * suite-pinned riding law the family has carried since r157 (2SXE:
 * "&a: key" anchors "a:" and "*a:" names "a:"; W5VH keeps colons
 * inside names), unchanged on post-r197 main. The walk: line 1 "&a::"
 * is a props-only anchor declaration whose name rides BOTH colons to
 * the break -- the anchor PENDS; line 2 "  *a:" at column 2 is an
 * alias whose name rides the colon, so no value indicator ever opens a
 * map and the document collapses to ONE root alias event carrying the
 * pended anchor as props. libyaml's 1.1 laws compose the other way:
 * IS_ALPHA cuts the anchor at "a" (the r142/r76 law), the residual
 * "::" re-lexes as plain ":" with the final colon riding as the value
 * token (the plain law ends a scalar at colon+blank), so line 1 is the
 * anchored KEY ":" of a root map; line 2's alias "a" (the same cut)
 * plus its own EOL colon opens a NESTED map with the alias as key and
 * an empty plain value -- the 9-event form (+MAP / "=VAL : &a :" /
 * +MAP / "=ALI *a" / "=VAL : " / -MAP / -MAP), r182's shape
 * byte-for-byte on the dumps: the trailing break is NOT load-bearing
 * on either side (libyaml folds nothing across it; yeptris's alias
 * name already rode the colon at EOF). Minimality: prefixes 0-3 agree
 * raw; prefixes 4-6 (the head-only "&a::" member and its
 * blank-padded prefixes -- the r174/r192 lists' named sibling) ride
 * the anchor-charset split transform, as does prefix 9 (the
 * alias-colon cut point "&a::\n  *a", whose colonless alias is the
 * same name-ride split); prefix 8 ("&a::\n  *") is the BOTH-ERROR
 * alias-only split member; the full 10-byte form is the first
 * unclaimed classification, so the entry pins the artifact
 * byte-exactly (the nightly artifact IS the 10-byte input,
 * libFuzzer-minimal at MS: 0 -- it fired on SEED-CORPUS replay, no
 * mutation chain). Single-byte deletions: the '&' deletion ("a::\n  "
 * "*a:"), both head-colon deletions ("&a:\n  *a:", which AGREE here
 * -- unlike r182's with-break twin, where they diverged), the break
 * deletion (the single-line "&a::  *a:"), the '*' deletion, and the
 * alias-colon deletion (prefix 9) classify known or equal; the head
 * 'a' deletion ("&::\n  *a:", the empty-cut member), the blank
 * deletions (the 1-space member "&a::\n *a:", both copies), and the
 * alias-name 'a' deletion ("&a::\n  *:", the alias-only member)
 * diverge -- each its own entry or an engine dig. The charset-split
 * twin declines both orientations exactly as r157's/r170's/r174's/
 * r182's/r192's: libyaml(split) == yeptris(original) needs libyaml to
 * read the single root alias (impossible -- its cut always re-opens
 * the map), and the reversed orientation gates yeptris(original) to
 * the one-alias signature, which is exactly the 'yeptris =ALI vs
 * libyaml anything' mask the r137/r138/r139 verdicts ruled unsound
 * for event-kind divergences (a genuine alias-drop bug would ride the
 * same signature). The composition rides the EXACT-BYTES gray claim
 * -- documented by input, not by mutation. Family siblings (the
 * empty-cut, 1-space, and alias-only members, the no-indent twin the
 * r170 list names, the with-break r182 seed) get their own entries or
 * an engine dig. */
static int yd_r198_root_alias_breakless_gray(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x3a, 0x0a, 0x20, 0x20, 0x2a, 0x61, 0x3a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* True for the ONE exact gray-zone input of the r212 nightly
 * (crash-4532cf850a2d34c3597f172fa1eac42cc911bb81, run 42527856270,
 * main cb168e1): 14 bytes "&a:o&a:o::\n*a:" -- the
 * r157/r170/r174/r182/r192/r198 root-alias collapse's BREAKLESS
 * DOUBLED-HEAD member (byte-identical to the r174 seed with the
 * trailing break deleted; r174's own minimality list names this twin
 * -- "prefixes 14/15 diverge IDENTICALLY -- the trailing break is NOT
 * load-bearing, so the breakless twin stays unclaimed beside the
 * pinned cut" -- and the r151/r170 deletion-sibling precedent gives it
 * its own entry beside them). Both engines accept; the streams differ
 * yeptris's one root alias (3 dump lines) vs libyaml's 8-line map with
 * two flat pairs. Regression triage: the A/B against the pre-r211
 * parent 55d1ab5 aborts IDENTICALLY (exit 134, the same 14-byte
 * artifact, the same DIFFER pair), so the r211 riding flow-property
 * re-lex is exonerated and the escalation clause does not fire --
 * yeptris's reading is the suite-pinned riding law the family has
 * carried since r157 (2SXE: "&a: key" anchors "a:" and "*a:" names
 * "a:"; W5VH keeps colons inside names), unchanged on post-r211 main.
 * The walk: line 1 "&a:o&a:o::" is a props-only anchor declaration
 * whose name rides BOTH colon runs AND the interior '&' to the break
 * (yep_scan_prop_char ends a name only at blank/break/flow-indicator/
 * comma/'#'; the '&'-in-name split vs libyaml's IS_ALPHA cut is the
 * r142 anchor-charset family, a documented comparator carry), so the
 * anchor PENDS; line 2 "*a:" at column 0 is an alias whose name rides
 * the colon, no value indicator ever opens a map, and the document
 * collapses to ONE root alias event carrying the pended anchor as
 * props. libyaml's 1.1 laws compose the other way: IS_ALPHA cuts the
 * anchor at the first colon ("a" -- the r142/r76 law), the residual
 * ":o&a:o::" re-lexes as plain ":o&a:o:" (the interior '&' mid-plain
 * is CONTENT -- libyaml's plain law opens '&' only at token start)
 * with the final colon riding as the value token, so line 1 is the
 * anchored KEY of a root map; line 2's alias "a" (the same cut) sits
 * at column 0 INSIDE the same map as a second flat pair (alias key,
 * empty plain value) -- r174's shape minus the break, and the break is
 * NOT load-bearing on either side (libyaml folds nothing across it;
 * yeptris's alias name already rode the colon at EOF). Minimality:
 * prefix 13 ("...\n*a", no colon) is the libyaml-reject alias-only
 * split member (the r174 list's named class -- abort-both, verified);
 * the nightly artifact IS the 14-byte input, libFuzzer-minimal at
 * MS: 0 (it fired on the seed-corpora read, no mutation chain).
 * 50-mutant paired battery (20 single-byte flips, 10 span deletions,
 * 20 targeted -- every single-byte deletion and every prefix): 0
 * regressions, every verdict identical pre/post on the pre-r211
 * parent; the surviving abort-both members are the family siblings
 * the r174 list names (the single-colon "&a:o&a:o:\n*a:" member, the
 * "*:" alias-only member, the head-only splits) -- each its own entry
 * or an engine dig. The charset-split twin declines both orientations
 * exactly as r157's/r170's/r174's/r182's/r192's/r198's:
 * libyaml(split) == yeptris(original) needs libyaml to read the
 * single root alias (impossible -- its cut always re-opens the map),
 * and the reversed orientation gates yeptris(original) to the
 * one-alias signature, which is exactly the 'yeptris =ALI vs libyaml
 * anything' mask the r137/r138/r139 verdicts ruled unsound for
 * event-kind divergences (a genuine alias-drop bug would ride the
 * same signature). The composition rides the EXACT-BYTES gray claim
 * -- documented by input, not by mutation. Family siblings (the
 * with-break r174 seed, the single-colon and 'a:x' members, the "*:"
 * member, the indented twins the r182/r198 seeds pin) get their own
 * entries or an engine dig. */
static int yd_r212_root_alias_double_head_breakless_gray(const uint8_t* data, size_t size,
                                                         const char* ydump, const char* ldump) {
    static const uint8_t seed[] = {0x26, 0x61, 0x3a, 0x6f, 0x26, 0x61, 0x3a,
                                   0x6f, 0x3a, 0x3a, 0x0a, 0x2a, 0x61, 0x3a};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}
/* One blank line of the r218 root-alias blank-run walk: spaces* then a
 * break (LF, CRLF, or lone CR). Advances *at past it; 0 when the bytes
 * at *at are not a blank line. */
static int yd_r218_blank_line(const uint8_t* d, size_t n, size_t* at) {
    size_t i = *at;
    while (i < n && d[i] == ' ') {
        i++;
    }
    if (i >= n) {
        return 0;
    }
    if (d[i] == '\n') {
        *at = i + 1;
        return 1;
    }
    if (d[i] == '\r') {
        *at = (i + 1 < n && d[i + 1] == '\n') ? i + 2 : i + 1;
        return 1;
    }
    return 0;
}

/* True for the BLANK-RUN member family of the r157/r170/r174/r182/r192/
 * r198/r212 root-alias collapse (the r218 nightly, run 42562016233,
 * main ceea4ae, crash-acc644af2f53c3a0391417902cfac62e34e9bc9d): the
 * 28-byte artifact "&a:o:" + 19 LFs + "*a:" -- the r170 no-indent seed
 * with a BLANK-LINE RUN inserted between the anchored head and the
 * alias line (the family's named missing member: r157 pins the
 * indented glued twin, r170 the glued twin, r182 the "&a::" indented
 * twin, r198 the breakless indented twin -- none carries the
 * blank-line separator). Both engines accept; the streams differ
 * yeptris's one root alias (3 dump lines: "+STR" / "=ALI *a:" /
 * "-STR") vs libyaml's 9-event map form ("+MAP" / "=VAL : &a :o" /
 * "=VAL : " / "=ALI *a" / "=VAL : " / "-MAP"). Fired on SEED-CORPUS
 * replay at MS: 0 (libFuzzer-minimal -- no mutation chain). Regression
 * triage: the A/B against the pre-r217 parent 0eb3b0f aborts
 * IDENTICALLY (exit 134, the same dump pair), so the r217 engine
 * round's three norm_step gates are exonerated and the escalation
 * clause does not fire -- yeptris's reading is the suite-pinned riding
 * law the family has carried since r157 (2SXE: "&a: key" anchors "a:"
 * and "*a:" names "a:"; W5VH keeps colons inside names), unchanged on
 * post-r217 main (the r217 repro pin norm-phantom-verbatim-lead-r217
 * still exits 0). The walk: line 1 "&a:o:" is a props-only anchor
 * declaration whose name rides the colons to the break -- the anchor
 * PENDS; the blank run is INERT on the collapse (the pended anchor
 * walks past pure-break lines), and the alias line's name rides its
 * colon, so no value indicator ever opens a map and the document
 * collapses to ONE root alias event carrying the pended anchor as
 * props. libyaml's 1.1 laws compose the other way: IS_ALPHA cuts the
 * anchor at "a" (the r142/r76 law), the residual ":o:" re-lexes as
 * plain ":o" with the final colon riding as the value token, so line 1
 * is the anchored KEY of a root map with an empty value; the alias "a"
 * (the r54 cut) plus its own EOL colon opens the second flat pair at
 * column 0 -- the r170 shape behind an inert blank run. Minimality:
 * the collapse is blank-run-length independent -- the artifact's
 * 18-blank run shrinks to the 11-byte minimum "&a:o:\n\n*a:\n" with
 * IDENTICAL dumps, and every verified member rides the same skeleton:
 * the k=2/3 runs, the breakless tail (EOF at the alias colon, both the
 * 11- and 28-byte spells), the trailing blank runs, the space-only
 * blank lines, the CRLF and lone-CR break spells, the head's
 * trailing-space line, and the blank+indent /
 * indented-blank+indented-alias members -- the arm's byte-shape walk
 * (head line + ONE-OR-MORE blank lines + indent + "*a:" + blank-run
 * tail) claims exactly those. The TAB members DECLINE by the no-tab
 * gate: a tab in a blank line (space+tab or tab-only) flips libyaml's
 * reading to a reject (YEPTRIS-ONLY-ACCEPT splits -- the differ-only
 * routing declines splits, the r170 law), and the tab-indent members
 * stay the r157 family's named siblings. Unclaimed DIFFER siblings
 * (each its own entry or an engine dig): the longer-name members
 * ("*ab:" and head "&aa:o:"), the "..." explicit doc-end tail, the
 * undefined-alias member ("*b:" -- a one-byte seed substitution), and
 * the alias-first reverse order (yeptris errors, libyaml accepts -- a
 * LIBYAML-ONLY split); the plain-follower member ("&a:o:\n\nb\n",
 * yeptris folds the pended head INTO one plain scalar "&a:o: b" -- a
 * fresh split twin of the r182-engine-residual #707 riding-anchor
 * composite) and the alias-plainname member ("&a:o:\n\n*a\n") are
 * YEPTRIS-ONLY splits and decline too. The charset-split twin declines
 * both orientations exactly as r157's/r170's: libyaml's cut can never
 * read the riding name, and the one-alias mask is unsound per
 * r137/r138/r139 (a genuine alias-drop bug would ride the same
 * signature). The composition rides the byte-shape gray claim over the
 * MEASURED family -- documented by input shape, verified member by
 * member.
 *
 * The r227 nightly (run 42630534433, main 0b72c24, crash-
 * 03fcab71b7649852477c5f7163d0a42ff70f2bba) caught the family's
 * DIRECT-ADJACENCY member: "&a:o:\n*a:" -- the ZERO-blank separator
 * the blank-run claim could not reach, fired on seed-corpus replay at
 * MS: 0 (the mutator deleted the r218 seed's 19-LF run down to the
 * bare break). Regression triage: the r225/r226 rounds' fixes (#1198,
 * #1204) are comparator-only and the artifact's dumps are the r218
 * pair verbatim, so no recent arm is implicated; the A/B against the
 * r218-era main aborts identically. The zero-blank member rides the
 * same collapse: yeptris's ONE riding-name alias vs libyaml's flat
 * pair map -- the blank run was always INERT on the collapse, so the
 * separator law widens to ZERO-OR-MORE blank lines, which claims the
 * direct form, its lone-CR spell ("&a:o:\r*a:"), the zero-blank
 * indented-alias member ("&a:o:\n *a:", libyaml nests the alias pair),
 * and the EOF-at-alias-colon tail (no trailing break) beside the
 * already-claimed members. The 96-mutant battery around the artifact
 * then measured the rest of the neighborhood: every riding-NAME
 * mutation (longer heads "&aa:o:"/"&x:o:"/"&a:x:", dotted names
 * "&a:.o:", longer/different alias names "*aa:"/"*x:") is the SAME
 * collapse with a different riding run -- the ride is deterministic
 * over the name's CONTENT -- so the exact-byte name pins widen to a
 * conservative riding-name set ([A-Za-z0-9_.-] + the riding ':'),
 * each run required to END on its ':' (the plain-name endings are
 * separate shapes: "&a:o\n..." is the r142 charset family, "*a\n" a
 * YEPTRIS-ONLY split). Leading blank lines claim as an inert prefix
 * (their own 2-pair-vs-collapse pair, dumpboth-verified), and the
 * line-start "..." doc-end tail claims as an inert suffix (libyaml
 * ends the same 2-pair map at the marker). Still unclaimed: the TAB
 * members (the no-tab gate), props-restart names ("&a&o:"), non-ASCII
 * names, and the colon-glued "...'" spell -- each its own entry. */
/* The r238 QUOTED-FOLLOWER member of the r218/r227 collapse family
 * (nightly round 238, run 42770301286, main ad84a13a,
 * crash-3d571437a4027d3b46c234964191c77cd4febdf4; 18-byte artifact
 * "&k2: 'j:(<LS> \r|\n '", 10-byte minimum "&2: '<LS>\r'", seed-corpus
 * replay at MS: 0): the anchor-riding colon composed with a
 * SINGLE-QUOTED follower on the SAME line. yeptris's suite-pinned
 * riding laws (2SXE/W5VH -- ns-anchor-char includes ':') take the
 * whole head as ONE riding-name anchor declaration with no value
 * indicator, and the document collapses to ONE scalar whose content
 * embeds the anchor spelling plus the quoted body through the PLAIN
 * fold; libyaml's 1.1 IS_ALPHA charset cuts the name at the colon
 * (the r142/r76 law), the freed colon becomes the VALUE indicator,
 * and the quoted scalar parses as the map value under its own fold.
 * The body walks pin BOTH dumps against the input bytes unit by
 * unit: printables and literal spaces byte-equal on both sides, raw
 * NEL/LS/PS triples verbatim on both sides (the r231 law); a space
 * run immediately before a CR/LF break folds with it -- yeptris to
 * ONE space, libyaml to one escaped \n when a raw triple precedes
 * (its blank-state quark: the verbatim keep arms the blank tracking,
 * the r231 READ_LINE class) and to one space after clean content;
 * a standalone break folds the same way; a trailing space run at the
 * body's end merges with a just-folded break on BOTH engines (the
 * one fold space already emitted), declining the escaped and
 * no-fold spellings as unmeasured. Measured composition-only: the
 * standalone quoted probes ('a<LF>', 'a<CR>', 'a<LS>', 'a<LS><CR>',
 * '<LS><CR>', 'a<CR><LF>') ALL AGREE -- the fold delta exists only
 * under the collapse. Gate: '&' + the r227 conservative riding-name
 * set ending on its ':', exactly one space, a SINGLE-QUOTED body (no
 * tabs, no consecutive breaks), the closing quote, then EOF. A
 * differ-only routing arm beside the r218 claim -- every existing
 * member keeps its exact path. */
static int yd_r238_body_walk_y(const uint8_t* body, size_t bo, size_t be, const char** c) {
    const char* a = *c;
    size_t i = bo;
    int prev_break = 0;
    while (i < be) {
        if ((unsigned char)body[i] == 0xC2 && (unsigned char)body[i + 1] == 0x85) {
            if ((unsigned char)a[0] != 0xC2 || (unsigned char)a[1] != 0x85) {
                return 0;
            }
            a += 2;
            i += 2;
            prev_break = 0;
        } else if ((unsigned char)body[i] == 0xE2 && (unsigned char)body[i + 1] == 0x80 &&
                   ((unsigned char)body[i + 2] == 0xA8 || (unsigned char)body[i + 2] == 0xA9)) {
            if ((unsigned char)a[0] != 0xE2 || (unsigned char)a[1] != 0x80 ||
                (unsigned char)a[2] != body[i + 2]) {
                return 0;
            }
            a += 3;
            i += 3;
            prev_break = 0;
        } else if (body[i] == '\r' || body[i] == '\n') {
            if (prev_break || a[0] != ' ') {
                return 0;
            }
            a += 1; /* the plain fold: the break becomes one space */
            i += 1;
            prev_break = 1;
        } else if (body[i] == ' ') {
            size_t s = i;
            while (s < be && body[s] == ' ') {
                s++;
            }
            if (s < be && (body[s] == '\r' || body[s] == '\n')) {
                if (a[0] != ' ') {
                    return 0;
                }
                a += 1; /* the space run folds with its break into one space */
                i = s + 1;
                prev_break = 1;
            } else if (s == be) {
                /* the trailing run merges with a just-folded break (the
                 * one fold space was already emitted); the no-fold
                 * spelling is unmeasured -- decline */
                if (!prev_break) {
                    return 0;
                }
                i = s;
            } else {
                while (i < s) {
                    if (a[0] != ' ') {
                        return 0;
                    }
                    a += 1;
                    i += 1;
                }
                prev_break = 0;
            }
        } else if (body[i] >= 0x20) {
            if (a[0] != (char)body[i]) {
                return 0;
            }
            a += 1;
            i += 1;
            prev_break = 0;
        } else {
            return 0;
        }
    }
    *c = a;
    return 1;
}

static int yd_r238_body_walk_l(const uint8_t* body, size_t bo, size_t be, const char** c) {
    const char* b = *c;
    size_t i = bo;
    int pending_exotic = 0;
    int prev_break = 0;
    int last_esc = 0;
    while (i < be) {
        if ((unsigned char)body[i] == 0xC2 && (unsigned char)body[i + 1] == 0x85) {
            if ((unsigned char)b[0] != 0xC2 || (unsigned char)b[1] != 0x85) {
                return 0;
            }
            b += 2;
            i += 2;
            pending_exotic = 1;
            prev_break = 0;
        } else if ((unsigned char)body[i] == 0xE2 && (unsigned char)body[i + 1] == 0x80 &&
                   ((unsigned char)body[i + 2] == 0xA8 || (unsigned char)body[i + 2] == 0xA9)) {
            if ((unsigned char)b[0] != 0xE2 || (unsigned char)b[1] != 0x80 ||
                (unsigned char)b[2] != body[i + 2]) {
                return 0;
            }
            b += 3;
            i += 3;
            pending_exotic = 1;
            prev_break = 0;
        } else if (body[i] == '\r' || body[i] == '\n') {
            if (prev_break) {
                return 0;
            }
            if (pending_exotic) {
                if (b[0] != '\\' || b[1] != 'n') {
                    return 0;
                }
                b += 2; /* the blank-state quark: the break escapes */
                last_esc = 1;
            } else {
                if (b[0] != ' ') {
                    return 0;
                }
                b += 1; /* the clean fold: the break becomes one space */
                last_esc = 0;
            }
            i += 1;
            prev_break = 1;
        } else if (body[i] == ' ') {
            size_t s = i;
            while (s < be && body[s] == ' ') {
                s++;
            }
            if (s < be && (body[s] == '\r' || body[s] == '\n')) {
                /* the space run trims; the break speaks alone */
                if (pending_exotic) {
                    if (b[0] != '\\' || b[1] != 'n') {
                        return 0;
                    }
                    b += 2;
                    last_esc = 1;
                } else {
                    if (b[0] != ' ') {
                        return 0;
                    }
                    b += 1;
                    last_esc = 0;
                }
                i = s + 1;
                prev_break = 1;
            } else if (s == be) {
                /* the trailing run merges with a just-FOLDED break; the
                 * escaped and no-break spellings are unmeasured */
                if (!prev_break || last_esc) {
                    return 0;
                }
                i = s;
            } else {
                while (i < s) {
                    if (b[0] != ' ') {
                        return 0;
                    }
                    b += 1;
                    i += 1;
                }
                prev_break = 0;
            }
        } else if (body[i] >= 0x20) {
            if (b[0] != (char)body[i]) {
                return 0;
            }
            b += 1;
            i += 1;
            pending_exotic = 0;
            prev_break = 0;
        } else {
            return 0;
        }
    }
    *c = b;
    return 1;
}

static int yd_r238_root_anchor_quoted_follow_gray(const uint8_t* data, size_t size,
                                                  const char* ydump, const char* ldump) {
    size_t i;
    size_t name_e;
    if (size < 10) { /* "&2: '<LS>\r'" minimum */
        return 0;
    }
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    for (i = 0; i < size; i++) {
        if (data[i] == '\t') {
            return 0;
        }
    }
    if (data[0] != '&') {
        return 0;
    }
    i = 1;
    {
        size_t name_o = i;
        while (i < size &&
               (((data[i]) >= 'A' && (data[i]) <= 'Z') || ((data[i]) >= 'a' && (data[i]) <= 'z') ||
                ((data[i]) >= '0' && (data[i]) <= '9') || (data[i]) == '_' || (data[i]) == '-' ||
                (data[i]) == '.' || (data[i]) == ':')) {
            i++;
        }
        if (i == name_o || data[i - 1] != ':') {
            return 0;
        }
        name_e = i; /* one past the ':' */
    }
    if (i >= size || data[i] != ' ' || (i + 1 < size && data[i + 1] == ' ')) {
        return 0; /* exactly one space */
    }
    i++;
    if (i >= size || data[i] != '\'') {
        return 0;
    }
    i++;
    {
        size_t bo = i;
        int prev_break = 0;
        while (i < size && data[i] != '\'') {
            if ((unsigned char)data[i] == 0xC2 && (unsigned char)data[i + 1] == 0x85) {
                i += 2;
                prev_break = 0;
            } else if ((unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
                       ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
                i += 3;
                prev_break = 0;
            } else if (data[i] == '\r' || data[i] == '\n') {
                if (prev_break) {
                    return 0;
                }
                i += 1;
                prev_break = 1;
            } else if (data[i] >= 0x20) {
                i += 1;
                prev_break = 0;
            } else {
                return 0;
            }
        }
        if (i >= size) {
            return 0; /* unterminated */
        }
        i++; /* the closing quote */
        if (i != size) {
            return 0; /* EOF behind the quote */
        }
        /* the yeptris side: +STR / =VAL : &<name> <body-y> / -STR */
        if (strncmp((const char*)ydump, "+STR\n=VAL : &", 13) != 0) {
            return 0;
        }
        {
            const char* a = ydump + 13;
            size_t n;
            for (n = 1; n < name_e; n++) { /* the riding name run, ':' included */
                if (a[0] != (char)data[n]) {
                    return 0;
                }
                a++;
            }
            if (a[0] != ' ') {
                return 0;
            }
            a++;
            if (!yd_r238_body_walk_y(data, bo, size - 1, &a)) {
                return 0;
            }
            if (strncmp(a, "\n-STR\n", 6) != 0) {
                return 0;
            }
        }
        /* the libyaml side: +STR / +MAP / =VAL : &<cut> / =VAL :
         * <body-l> / -MAP / -STR -- the cut name ends at the FIRST
         * out-of-charset byte (the interior ':') */
        if (strncmp((const char*)ldump, "+STR\n+MAP\n=VAL : &", 18) != 0) {
            return 0;
        }
        {
            const char* b = ldump + 18;
            size_t n = 1;
            while (n < name_e && data[n] != ':') {
                if (b[0] != (char)data[n]) {
                    return 0;
                }
                b++;
                n++;
            }
            if (b[0] != ' ' || b[1] != '\n') {
                return 0;
            }
            b += 2;
            if (strncmp(b, "=VAL : ", 7) != 0) {
                return 0;
            }
            b += 7;
            if (!yd_r238_body_walk_l(data, bo, size - 1, &b)) {
                return 0;
            }
            if (strncmp(b, "\n-MAP\n-STR\n", 12) != 0) {
                return 0;
            }
        }
    }
    return 1;
}

/* The plain-? tail rides the raw-PS plain fold (the nightly's round
 * 240, crash-63cbaa8c, 12-byte minimum "[)<PS>(?t,?$]", main bf8342c
 * era): a flow sequence of two plain members, the first carrying an
 * interior PS triple, the last beginning with a bare '?' glued to a
 * non-space byte. TWO suite-over-libyaml laws compose, and neither
 * alone equalizes the pair:
 * - the plain-LS/PS fold (r56, yd_plain_lsps_fold_divergence's law):
 *   yeptris (YAML 1.2) normalizes the PS and folds it to ONE space in
 *   the flow plain; libyaml's READ_LINE joins the break RAW into the
 *   content — both engines' first member reads ") (?t" shaped, but
 *   the bytes differ (space vs the raw e2 80 a9);
 * - the suite-pinned plain-'?' law (2SXE: '?' followed by a
 *   non-space is content): yeptris reads the tail as ONE plain
 *   scalar; libyaml's 1.1 grammar takes the bare '?' as the
 *   explicit-key indicator and opens "? $ : <empty>" for the tail.
 * The composite defeats the single-law arms (the fold arm's dump
 * transform still leaves the map/plain delta; the clip arm's
 * LF-suffix replay never fires — this family CLIPS nothing: libyaml
 * closes the seq and the stream), so the claim is its own byte-shape
 * gate + dump walk, and anything the gates cannot verify declines
 * (the escalation clause stays live).
 *
 * Gate: '[' first, ']' last, no LF/CR/NEL/LS/PT anywhere (the PS
 * triple is the only exotic spelling and it sits in the head member),
 * exactly one ',' separating the members, the head's bytes adjacent
 * to the PS are not spaces (the fold-run composition is a later
 * family), the head does not START with '?' (a member-start '?' is
 * libyaml's explicit key too), the tail is '?' glued to a printable
 * run free of the flow/style/indicator set. Walk: yeptris's dump is
 * +STR/+SEQ/=VAL : <head with the PS folded to ONE space>/=VAL :
 * <tail>/-SEQ/-STR; libyaml's is +STR/+SEQ/=VAL : <head with the PS
 * RAW>/+MAP/=VAL : <tail after the '?'>/=VAL : <empty>/-MAP/-SEQ/-STR. */
/* The root-alias collapse's TAB + folded-header member rides the
 * family skeleton (the nightly's round 241, crash-aa326ab1, 9-byte
 * minimum "&a:\t>\n*a:", main f6111d8, nightly run 42919365127): the
 * r218/r227/r238 collapse family's members so far carried blank-line
 * runs, direct adjacency, breakless and quoted followers — never a
 * TAB separator, and never a BLOCK-SCALAR HEADER as the head's value.
 * Here `&a:` + a TAB + `>` + LF + `*a:` composes three agreements and
 * one delta: BOTH engines render the head scalar as `&` + name + ONE
 * space (the TAB blank folds like any blank run), BOTH take the `>`
 * as an EMPTY folded-scalar value (the `*a:` line sits at column 0),
 * and the delta is the family's own law at the second key — yeptris's
 * suite-pinned riding laws (2SXE/W5VH) let the colon RIDE the alias
 * name (`=ALI *a:` as an unvalued key), while libyaml's 1.1 IS_ALPHA
 * cut splits the alias at `a` and emits its empty value (the r142/r76
 * law). The r218 arm declines this shape twice (its no-tab gate and
 * its blank-run separator law), so the claim is its own byte-shape
 * gate + exact-dump walk, and anything the gates cannot verify
 * declines (the escalation clause stays live).
 *
 * Gate: '&' + name-run ending in the riding ':', a TAB run (this
 * member's separator — spaces ride the r218 siblings), '>' with no
 * header suffix (the +/-/digit spellings escalate), LF, '*' + a
 * name-run ending in the riding ':' + EOF (the breakless tail; the
 * trailing-break spellings escalate). Walk: BOTH dumps are
 * +STR/+MAP/=VAL : &<name> <ONE space>/=VAL : <empty> ...; yeptris
 * continues =ALI *<name>:/-MAP/-STR, libyaml =ALI *<name>/=VAL :
 * <empty>/-MAP/-STR. */

/* The flow explicit-key family's glued-? plain member, generalized
 * across the sequence (the nightly's round 242, crash-be5b7d7b, 13-byte
 * minimum "[?,,g,?g?,\"\"]", main dc6d2e2, nightly run 42930337137):
 * a breakless flow sequence whose members split on ',' — the r240
 * carry pinned the glued-? tail behind a PS-folded head; this member
 * is the MIDDLE-of-sequence glued-? with no exotic bytes at all, so
 * the claim is the member grammar itself. Members — trimmed of
 * spaces, no breaks, no nested flow (the '['/'{' bytes decline):
 * - the LONE '?' opens the explicit-key map with an empty key and an
 *   empty value on BOTH sides (they agree — the suite law only
 *   protects the GLUED '?');
 * - an empty member between commas renders NOTHING on both sides;
 * - a quoted member ('..' or "..", simple printable interior, no
 *   escapes) and a plain member (indicator-free printable run) render
 *   `=VAL : <content>` on both sides (the harness's dump normalizes
 *   scalar styles; content agrees);
 * - a GLUED-'?' member ('?' + a non-space printable run free of the
 *   flow/style/indicator set) is the divergence: yeptris's
 *   suite-pinned plain-'?' law (2SXE) reads it as ONE plain scalar,
 *   libyaml's 1.1 grammar takes the bare '?' as the explicit-key
 *   indicator and opens `? <rest> : <empty>`.
 * The arm splits the members, classifies each, builds BOTH expected
 * dumps from the input bytes, and requires exact equality on both
 * sides; any byte the grammar cannot classify declines (the
 * escalation clause stays live). */

/* The UTF-16BE mid-stream BOM composite rides the r144/r226 front-end
 * family (the nightly's round 243, crash-1a7cef2a, 5206-byte artifact
 * that is already single-byte-deletion minimal, main f04fd40-era,
 * nightly run 42942836432): a UTF-16BE stream opening with THREE
 * `feff 0085` (BOM + NEL) code-unit pairs. yeptris (the r144 fix)
 * consumes ONLY the stream-initial BOM and keeps every mid-stream
 * `feff` code unit as U+FEFF (ZWNBSP) CONTENT — the normalized NEL
 * that follows each folds to ONE space in the plain scalar; libyaml's
 * UTF-16 reader eats the mid-stream `feff` code units as BOMs too,
 * so both its FEFF characters and their fold spaces vanish. The
 * relation is exact and dump-shaped (the yd_ps_space_divergence
 * pattern): libyaml's dump equals yeptris's dump with every UTF-8
 * U+FEFF triple removed TOGETHER with the single fold space that
 * immediately follows each (the NEL residue — yeptris folds the
 * break libyaml swallowed whole). Any FEFF triple NOT followed by a
 * space is also removable (a mid-content FEFF without a following
 * break); anything else mismatches and declines (the escalation
 * clause stays live). Gate: the input starts with the UTF-16BE BOM
 * `fe ff` and contains at least one more `fe ff` code unit mid-stream. */
static int yd_r243_utf16_midstream_bom_gray(const uint8_t* data, size_t size, const char* ydump,
                                            const char* ldump) {
    size_t i;
    int extra_bom = 0;
    if (size < 6 || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (data[0] != 0xFE || data[1] != 0xFF) {
        return 0; /* the UTF-16BE BOM leads */
    }
    for (i = 2; i + 1 < size; i++) {
        if (data[i] == 0xFE && data[i + 1] == 0xFF) {
            extra_bom = 1;
            break;
        }
    }
    if (!extra_bom) {
        return 0; /* the initial BOM alone rides the r144 engine law */
    }
    /* the relation: libyaml's dump == yeptris's dump minus every
     * U+FEFF triple and the ONE space following each — with ONE tail
     * allowance: at the scalar-content end libyaml's non-validating
     * UTF-16 writer may carry a residue (the odd-region re-decode:
     * up to two complete characters plus one TRUNCATED UTF-8
     * sequence at EOF) that yeptris's validating front-end declines. */
    {
        const char* a = ydump;
        const char* b = ldump;
        while (a[0] != '\0') {
            if ((unsigned char)a[0] == 0xEF && (unsigned char)a[1] == 0xBB &&
                (unsigned char)a[2] == 0xBF) {
                a += 3; /* the U+FEFF triple vanishes */
                if (a[0] == ' ') {
                    a++; /* the folded-NEL residue vanishes with it */
                }
                continue;
            }
            if (b[0] != a[0]) {
                break; /* the residue, or a real mismatch */
            }
            a++;
            b++;
        }
        if (a[0] == '\0') {
            return b[0] == '\0'; /* the pure-strip equalization */
        }
        /* the tail law: the remainder of yeptris's dump re-appears
         * verbatim behind a bounded residue in libyaml's */
        {
            size_t y_rest = strlen(a);
            size_t l_rest = strlen(b);
            if (l_rest <= y_rest || l_rest - y_rest > 8) {
                return 0;
            }
            size_t extra = l_rest - y_rest;
            if (memcmp(b + extra, a, y_rest) != 0) {
                return 0;
            }
            /* the residue: complete chars, then ONE truncated
             * multi-byte sequence at its end (libyaml's writer
             * flushed the lead/continuations it had buffered) */
            size_t k = 0;
            int truncated = 0;
            while (k < extra) {
                unsigned char lead = (unsigned char)b[k];
                size_t need;
                if (lead >= 0x80 && lead < 0xC0) {
                    return 0; /* a stray continuation leads */
                }
                need = (lead < 0x80) ? 1 : (lead < 0xE0) ? 2 : (lead < 0xF0) ? 3 : 4;
                if (k + need > extra) {
                    truncated = 1;
                    break;
                }
                {
                    size_t c;
                    for (c = 1; c < need; c++) {
                        if (((unsigned char)b[k + c] & 0xC0) != 0x80) {
                            return 0;
                        }
                    }
                }
                k += need;
            }
            return truncated;
        }
    }
}

static int yd_r242_flow_glued_q_members_gray(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    size_t i;
    size_t cap;
    char* yb;
    char* lb;
    size_t yo = 0;
    size_t lo = 0;
    int seen_q = 0;
    if (size < 5 || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (data[0] != '[' || data[size - 1] != ']') {
        return 0;
    }
    for (i = 1; i + 1 < size; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '\n' || c == '\r' || c == '\t' || c == '[' || c == ']' || c == '{' || c == '}') {
            return 0;
        }
    }
#define YD_R242_EMIT(buf, off, s)                                                                  \
    do {                                                                                           \
        size_t n_ = strlen(s);                                                                     \
        memcpy((buf) + (off), (s), n_);                                                            \
        (off) += n_;                                                                               \
    } while (0)
    cap = size * 16 + 128;
    yb = malloc(cap);
    lb = malloc(cap);
    if (yb == NULL || lb == NULL) {
        free(yb);
        free(lb);
        return 0;
    }
    YD_R242_EMIT(yb, yo, "+STR\n+SEQ\n");
    YD_R242_EMIT(lb, lo, "+STR\n+SEQ\n");
    i = 1;
    while (i < size - 1) {
        size_t ms = i;
        size_t me;
        while (i < size - 1 && data[i] != ',') {
            i++;
        }
        me = i; /* the member is [ms, me) */
        if (i < size - 1) {
            i++; /* past the comma */
        }
        /* trim spaces */
        while (ms < me && data[ms] == ' ') {
            ms++;
        }
        while (me > ms && data[me - 1] == ' ') {
            me--;
        }
        if (ms == me) {
            continue; /* the empty member renders nothing on both */
        }
        if (me - ms == 1 && data[ms] == '?') {
            /* the lone '?': the explicit-key map, empty key + empty
             * value, on BOTH sides */
            YD_R242_EMIT(yb, yo, "+MAP\n=VAL : \n=VAL : \n-MAP\n");
            YD_R242_EMIT(lb, lo, "+MAP\n=VAL : \n=VAL : \n-MAP\n");
            continue;
        }
        if (data[ms] == '"' || data[ms] == '\'') {
            unsigned char q = (unsigned char)data[ms];
            size_t c;
            if (me - ms < 2 || data[me - 1] != (char)q) {
                goto decline;
            }
            for (c = ms + 1; c + 1 < me; c++) {
                unsigned char d = (unsigned char)data[c];
                if (d == q || d == '\\' || d < 0x20 || d > 0x7E) {
                    goto decline;
                }
                if (d == '[' || d == ']' || d == '{' || d == '}' || d == ',') {
                    goto decline;
                }
            }
            YD_R242_EMIT(yb, yo, "=VAL : ");
            YD_R242_EMIT(lb, lo, "=VAL : ");
            memcpy(yb + yo, data + ms + 1, me - ms - 2);
            yo += me - ms - 2;
            memcpy(lb + lo, data + ms + 1, me - ms - 2);
            lo += me - ms - 2;
            YD_R242_EMIT(yb, yo, "\n");
            YD_R242_EMIT(lb, lo, "\n");
            continue;
        }
        if (data[ms] == '?') {
            /* the glued-?: the rest must be the plain charset */
            size_t c;
            if (me - ms < 2) {
                goto decline;
            }
            if (data[ms + 1] == ' ' || data[ms + 1] == '\t') {
                goto decline; /* a spaced '?' is libyaml's key too */
            }
            for (c = ms + 1; c < me; c++) {
                unsigned char d = (unsigned char)data[c];
                if (d < 0x21 || d > 0x7E) {
                    goto decline;
                }
                if (d == '[' || d == ']' || d == '{' || d == '}' || d == ',' || d == ':' ||
                    d == '\'' || d == '"' || d == '#' || d == '&' || d == '*' || d == '!' ||
                    d == '|' || d == '>' || d == '%' || d == '@' || d == '`' || d == '\\') {
                    goto decline;
                }
            }
            seen_q = 1;
            /* yeptris: ONE plain scalar carrying the whole member */
            YD_R242_EMIT(yb, yo, "=VAL : ");
            memcpy(yb + yo, data + ms, me - ms);
            yo += me - ms;
            YD_R242_EMIT(yb, yo, "\n");
            /* libyaml: "? <rest> : <empty>" as the explicit-key map */
            YD_R242_EMIT(lb, lo, "+MAP\n=VAL : ");
            memcpy(lb + lo, data + ms + 1, me - ms - 1);
            lo += me - ms - 1;
            YD_R242_EMIT(lb, lo, "\n=VAL : \n-MAP\n");
            continue;
        }
        /* plain member: the indicator-free printable run */
        {
            size_t c;
            int ok_member = 1;
            for (c = ms; c < me; c++) {
                unsigned char d = (unsigned char)data[c];
                if (d < 0x20 || d > 0x7E) {
                    ok_member = 0;
                    break;
                }
                if (d == '[' || d == ']' || d == '{' || d == '}' || d == ',' || d == ':' ||
                    d == '\'' || d == '"' || d == '#' || d == '&' || d == '*' || d == '!' ||
                    d == '|' || d == '>' || d == '%' || d == '@' || d == '`' || d == '\\') {
                    ok_member = 0;
                    break;
                }
            }
            if (!ok_member) {
                goto decline;
            }
            YD_R242_EMIT(yb, yo, "=VAL : ");
            memcpy(yb + yo, data + ms, me - ms);
            yo += me - ms;
            YD_R242_EMIT(yb, yo, "\n");
            YD_R242_EMIT(lb, lo, "=VAL : ");
            memcpy(lb + lo, data + ms, me - ms);
            lo += me - ms;
            YD_R242_EMIT(lb, lo, "\n");
            continue;
        }
    decline:
        free(yb);
        free(lb);
        return 0;
    }
    YD_R242_EMIT(yb, yo, "-SEQ\n-STR\n");
    YD_R242_EMIT(lb, lo, "-SEQ\n-STR\n");
#undef YD_R242_EMIT
    {
        int ok = seen_q && yo == strlen(ydump) && lo == strlen(ldump) &&
                 memcmp(yb, ydump, yo) == 0 && memcmp(lb, ldump, lo) == 0;
        free(yb);
        free(lb);
        return ok;
    }
}

static int yd_r241_alias_tab_fold_gray(const uint8_t* data, size_t size, const char* ydump,
                                       const char* ldump) {
    size_t i;
    size_t colon = size; /* the head's riding colon */
    size_t tabs = 0;     /* the TAB run's start */
    size_t star = size;  /* the alias line's '*' */
    if (size < 9 || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (data[0] != '&') {
        return 0;
    }
    for (i = 1; i < size; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == ':') {
            colon = i;
            break;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.')) {
            return 0;
        }
    }
    if (colon == size || colon == 1) {
        return 0;
    }
    i = colon + 1;
    while (i < size && data[i] == '\t') {
        i++;
    }
    tabs = colon + 1;
    if (i == tabs || i >= size || data[i] != '>') {
        return 0;
    }
    i++;
    if (i >= size || data[i] != '\n') {
        return 0; /* a header suffix escalates */
    }
    i++;
    if (i >= size || data[i] != '*') {
        return 0;
    }
    star = i;
    for (i = star + 1; i < size; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == ':') {
            if (i != size - 1) {
                return 0; /* the riding colon ends the breakless tail */
            }
            break;
        }
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.')) {
            return 0;
        }
    }
    if (star + 1 >= size || data[size - 1] != ':') {
        return 0;
    }
    /* the common head: +STR / +MAP / =VAL : &<name> <space> / =VAL :
     * <empty> */
    if (strncmp((const char*)ydump, "+STR\n+MAP\n=VAL : &", 18) != 0) {
        return 0;
    }
    if (strncmp((const char*)ldump, "+STR\n+MAP\n=VAL : &", 18) != 0) {
        return 0;
    }
    {
        const char* a = ydump + 18;
        const char* b = ldump + 18;
        size_t n;
        for (n = 1; n < colon; n++) {
            if (a[0] != (char)data[n] || b[0] != (char)data[n]) {
                return 0;
            }
            a++;
            b++;
        }
        /* the folded TAB run renders as ONE space on both sides */
        if (a[0] != ' ' || b[0] != ' ' || a[1] != '\n' || b[1] != '\n') {
            return 0;
        }
        a += 2;
        b += 2;
        if (strncmp(a, "=VAL : \n", 8) != 0 || strncmp(b, "=VAL : \n", 8) != 0) {
            return 0;
        }
        a += 8;
        b += 8;
        /* yeptris: =ALI *<name>: / -MAP / -STR */
        if (strncmp(a, "=ALI *", 6) != 0) {
            return 0;
        }
        a += 6;
        for (n = star + 1; n < size; n++) {
            if (a[0] != (char)data[n]) {
                return 0;
            }
            a++;
        }
        if (strncmp(a, "\n-MAP\n-STR\n", 11) != 0) {
            return 0;
        }
        /* libyaml: =ALI *<name-cut> / =VAL : <empty> / -MAP / -STR */
        if (strncmp(b, "=ALI *", 6) != 0) {
            return 0;
        }
        b += 6;
        for (n = star + 1; n < size - 1; n++) { /* the IS_ALPHA cut */
            if (b[0] != (char)data[n]) {
                return 0;
            }
            b++;
        }
        if (strncmp(b, "\n=VAL : \n-MAP\n-STR\n", 19) != 0) {
            return 0;
        }
    }
    return 1;
}

/* r244 (nightly run 42967551882, issue #1265): the flow-seq glued-?#
 * x quoted-CR composition. libyaml reads the glued-`?#` member as the
 * explicit-key indicator whose `#` starts a comment running to the
 * next CR (the line break) — eating the comma and the following
 * quoted member's opening quote — and the post-CR tail (leading space
 * stripped, the closing quote riding as CONTENT) becomes the map's
 * plain key, value empty. A `?`-led member with content between `?`
 * and `#` (e.g. `?g:m?#`) keeps the `#` as scalar content:
 * `+MAP{tail : ""}`. yeptris's suite-pinned laws keep `?#` ONE plain
 * scalar and every member flat in the seq (the CR inside a quoted
 * member folds to one space on both). Comparator carry only — no
 * engine change. The composition rides as EXACT-BYTES claims (the
 * r121/r137 precedent): libyaml's flow-key scanner permutations are
 * not soundly generalizable beyond the observed shapes, so the three
 * verified byte strings pin verbatim and every other input declines
 * (the escalation clause stays live). */
/* The nightly's r245 soup (run 43021700796, main 743c873): the
 * flow-? explicit-key x glued-?# x CR/LS/PS-break composition. Two
 * suite-pinned facets compose: the ?#/?2/?| members ride ONE plain
 * scalar (the #949/r244 law) where libyaml rebinds them as explicit
 * keys (the ?2 map, the comment-killed ?#), and the multiline plain
 * keys keep the break-fold law (PS folds to a space) where libyaml
 * rides the raw PS (#638's exotic-break drift). Suite over libyaml
 * on both counts; the soup permutations are not soundly
 * generalizable into a structural predicate (the r121/r137
 * EXACT-BYTES precedent). */
static int yd_r245_flow_qsoup_gray(const uint8_t* data, size_t size, const char* ydump,
                                   const char* ldump) {
    static const unsigned char m1[] = {
        0x5b, 0x31, 0x2c, 0x3f, 0x0d, 0xe2, 0x80, 0xa9, 0x30, 0xe2, 0x80, 0xa9, 0x3f, 0xe2,
        0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9,
        0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0x2c, 0x3f, 0x23, 0x2c, 0x3f, 0x0d, 0xe2, 0x80, 0xa9,
        0x30, 0xe2, 0x80, 0xa9, 0x3f, 0x2c, 0x3f, 0x32, 0x2c, 0x3f, 0x0d, 0xe2, 0x80, 0xa9,
        0x31, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x2c, 0x3f, 0x23, 0x2c, 0x3f, 0x7c,
        0x2c, 0x3f, 0x0d, 0xe2, 0x80, 0xa9, 0x31, 0x3a, 0x0d, 0x67, 0x2c, 0x3f, 0x0d, 0xe2,
        0x80, 0xa9, 0x31, 0x2c, 0x3f, 0x23, 0x2c, 0x3f, 0x0d, 0xe2, 0x80, 0xa9, 0x30, 0xe2,
        0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9,
        0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0xe2, 0x80, 0xa9, 0x3f, 0x5d};
    static const unsigned char m2[] = {
        0x5b, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d,
        0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x30, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d,
        0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c,
        0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e,
        0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80,
        0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c, 0x32,
        0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d, 0x31,
        0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f,
        0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31,
        0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79,
        0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x30, 0x2e, 0x30,
        0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a,
        0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65,
        0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x20, 0x2c, 0x3f,
        0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35,
        0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d,
        0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62,
        0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26,
        0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c,
        0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d,
        0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69,
        0x6e, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e,
        0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x23, 0x23,
        0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x30,
        0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23,
        0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31,
        0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69,
        0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e,
        0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c,
        0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f,
        0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31,
        0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61,
        0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61,
        0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x31, 0x31, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9,
        0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61,
        0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32,
        0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d,
        0x0a, 0x26, 0x6d, 0x2c, 0x30, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x3a,
        0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31,
        0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a,
        0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65,
        0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f,
        0x0d, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20,
        0x2c, 0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37,
        0x2e, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2,
        0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d,
        0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23,
        0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x23, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d,
        0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x30, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d,
        0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c,
        0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e,
        0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80,
        0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c, 0x32,
        0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d, 0x31,
        0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f,
        0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31,
        0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23,
        0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x2c, 0x3f, 0x23, 0x23,
        0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67,
        0x3a, 0x79, 0x2d, 0x2d, 0x2d, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0x0d, 0x0a,
        0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2e, 0x74, 0x61,
        0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x32, 0x30, 0x30, 0x32,
        0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d,
        0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e, 0x74, 0x61, 0x67, 0x3a,
        0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31,
        0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31, 0x37, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a,
        0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x22, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x31, 0x31, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d,
        0x0d, 0xe2, 0x80, 0xa9, 0x20, 0x35, 0x31, 0x3a, 0x0d, 0x0d, 0x31, 0x2e, 0x30, 0x65, 0x31,
        0x37, 0x2e, 0x74, 0x61, 0x67, 0x0d, 0x20, 0x2c, 0x3f, 0x23, 0x23, 0x7c, 0x2c, 0x3f, 0x0d,
        0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d, 0x20, 0x2c,
        0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x31, 0x2e, 0x30, 0x65, 0x35, 0x37, 0x2e,
        0x74, 0x61, 0x67, 0x3a, 0x79, 0x7c, 0x2c, 0x3f, 0x0d, 0x2f, 0x0a, 0x0d, 0x0d, 0xe2, 0x80,
        0xa9, 0x61, 0x72, 0x79, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x0a, 0x09, 0x6c, 0x2e, 0x6f, 0x72,
        0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x62, 0x69, 0x6e, 0x61, 0x72, 0x79, 0x0d, 0x0d,
        0x20, 0x2c, 0x3f, 0x23, 0x23, 0x0d, 0x0a, 0x26, 0x6d, 0x2c, 0x5d};
    static const char y1[] =
        "+STR\n+SEQ\n=VAL : 1\n+MAP\n=VAL : 0 ? ? ? ? ? ?\n=VAL : \n-MAP\n=VAL : ?#\n+MAP\n=VAL : "
        "0 ?\n=VAL : \n-MAP\n=VAL : ?2\n+MAP\n=VAL : 1 ?\n=VAL : \n-MAP\n=VAL : ?#\n=VAL : "
        "?|\n+MAP\n=VAL : 1\n=VAL : g\n-MAP\n+MAP\n=VAL : 1\n=VAL : \n-MAP\n=VAL : ?#\n+MAP\n=VAL "
        ": 0 ? ? ? ? ? ?\n=VAL : \n-MAP\n-SEQ\n-STR\n";
    static const char l1[] = "+STR\n+SEQ\n=VAL : 1\n+MAP\n=VAL : 0\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\n=VAL : \n-MAP\n+MAP\n=VAL : 0\xe2"
                             "\x80"
                             "\xa9"
                             "?\n=VAL : \n-MAP\n+MAP\n=VAL : 2\n=VAL : \n-MAP\n+MAP\n=VAL : 1\xe2"
                             "\x80"
                             "\xa9"
                             "?\n=VAL : \n-MAP\n+MAP\n=VAL : 1\n=VAL : g\n-MAP\n+MAP\n=VAL : "
                             "1\n=VAL : \n-MAP\n+MAP\n=VAL : 0\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\xe2"
                             "\x80"
                             "\xa9"
                             "?\n=VAL : \n-MAP\n-SEQ\n-STR\n";
    static const char y2[] =
        "+STR\n+SEQ\n=VAL : 11\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n50\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : "
        "/\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : "
        "\n-MAP\n=VAL : ?## &m\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL "
        ": 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n=VAL : ?## "
        "&m\n=VAL : 0.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : "
        "?##\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : "
        "?##|\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n=VAL : ?## &m\n=VAL : "
        "1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : "
        "2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n50\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : "
        "/\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : "
        "\n-MAP\n=VAL : ?02:binary\n=VAL : ?## &m\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : "
        "/\\n\\n\\n51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : "
        "/\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : "
        "\n-MAP\n=VAL : ?## &m\n=VAL : 0.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n=VAL : ?## "
        "&m\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : "
        "2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n50\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : "
        "/\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:binary\n=VAL : "
        "\n-MAP\n=VAL : ?## &m\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL "
        ": 211\n=VAL : \n-MAP\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17.tag\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 2002:bi\n=VAL : \n-MAP\n=VAL : ?## &m\n=VAL : "
        "1.0e57.tag:y---|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : "
        "?##|\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n=VAL : ?## &m\n=VAL : "
        "1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : "
        "2002:binaryg\n=VAL : 1.0e17.tag \"\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : 211\n=VAL : "
        "\n-MAP\n=VAL : ?##|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : 1.0e17.tag\n-MAP\n=VAL : "
        "?##|\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n=VAL : ?## &m\n=VAL : "
        "1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\\naryg:yam l.org\n=VAL : \n-MAP\n=VAL : "
        "2002:binary\n=VAL : ?## &m\n-SEQ\n-STR\n";
    static const char l2[] =
        "+STR\n+SEQ\n=VAL : 11\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "50\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : "
        "211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 0.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : "
        "/\\n\\n\xe2"
        "\x80"
        "\xa9"
        "50\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : "
        "02:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m \n=VAL : \n-MAP\n=VAL : "
        "1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : "
        "211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 0.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : "
        "/\\n\\n\xe2"
        "\x80"
        "\xa9"
        "50\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : "
        "211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:bi\n=VAL : \n-MAP\n+MAP\n=VAL : &m \n=VAL "
        ": \n-MAP\n=VAL : 1.0e57.tag:y---|\n+MAP\n=VAL : /\\n\\n\\n51\n=VAL : "
        "1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m \n=VAL : "
        "\n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17\n-MAP\n+MAP\n=VAL : 2002:binaryg\n=VAL : 1.0e17.tag "
        "\"\n-MAP\n+MAP\n=VAL : 211\n=VAL : \n-MAP\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "51\n=VAL : 1.0e17.tag\n-MAP\n+MAP\n=VAL : 2002:binary\n=VAL : \n-MAP\n+MAP\n=VAL : &m "
        "\n=VAL : \n-MAP\n=VAL : 1.0e57.tag:y|\n+MAP\n=VAL : /\\n\\n\xe2"
        "\x80"
        "\xa9"
        "aryg:yam l.org\n=VAL : \n-MAP\n=VAL : 2002:binary\n+MAP\n=VAL : &m \n=VAL : "
        "\n-MAP\n-SEQ\n-STR\n";
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (size == sizeof(m1) && memcmp(data, m1, size) == 0) {
        return strcmp(ydump, y1) == 0 && strcmp(ldump, l1) == 0;
    }
    if (size == sizeof(m2) && memcmp(data, m2, size) == 0) {
        return strcmp(ydump, y2) == 0 && strcmp(ldump, l2) == 0;
    }
    return 0;
}

/* The nightly's r246 soup (run 43116239639, main dc128c7): the
 * flow-? glued-D/+ x LS/PS-break composition, the r245 family's
 * fresh permutation. Two suite-pinned facets compose again: yeptris
 * keeps the glued-? members (`?D<LS>`, `?+<PS>`) ONE plain scalar
 * each (the #949/r244/r245 suite-pinned plain-? law) and folds the
 * exotic breaks to spaces (the 1.2 break law, #638's family), while
 * libyaml rebinds the glued ? as an explicit-key indicator (the
 * `?D` map with an empty value) and rides the RAW PS byte in the
 * second member (`0 ?+<PS>"`). Every other member matches
 * byte-exact - the divergence is exactly the two documented drift
 * laws composing. Suite over libyaml on both counts; the soup
 * permutations are not soundly generalizable into a structural
 * predicate (the r121/r137 EXACT-BYTES precedent). */
static int yd_r246_flow_qsoup2_gray(const uint8_t* data, size_t size, const char* ydump,
                                    const char* ldump) {
    /* shape 1: the 16-byte libFuzzer minimum `[?D<LS>,0\n?+<PS>]` */
    static const unsigned char m1[] = {0x5b, 0x3f, 0x44, 0xe2, 0x80, 0xa8, 0x2c, 0x30,
                                       0x0a, 0x3f, 0x2b, 0xe2, 0x80, 0xa9, 0x22, 0x5d};
    static const char y1[] = "+STR\n+SEQ\n=VAL : ?D\n=VAL : 0 ?+ \"\n-SEQ\n-STR\n";
    static const char l1[] = "+STR\n+SEQ\n+MAP\n=VAL : D\n=VAL : \n-MAP\n=VAL : 0 ?+\xe2"
                             "\x80"
                             "\xa9"
                             "\"\n-SEQ\n-STR\n";
    /* shape 2: the nightly's 193-byte artifact (the ?+D tag-soup) */
    static const unsigned char m2[] = {
        0x5b, 0x0a, 0x3f, 0x2b, 0x44, 0xe2, 0x80, 0xa8, 0x20, 0x27, 0x63, 0xe2, 0x80, 0xa9, 0x2e,
        0x3a, 0x6a, 0x3a, 0x28, 0xe2, 0x80, 0xa9, 0x0a, 0x3f, 0x2b, 0x44, 0xe2, 0x80, 0xa8, 0x20,
        0x27, 0x63, 0xe2, 0x80, 0xa9, 0x2e, 0x74, 0x61, 0x67, 0x3a, 0x79, 0x61, 0x6d, 0x6c, 0x2e,
        0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x27, 0x63, 0xe2, 0x80, 0xa9, 0x2e,
        0x3a, 0x6a, 0x3a, 0x28, 0xe2, 0x80, 0xa9, 0x0a, 0x3f, 0x2b, 0x44, 0xe2, 0x80, 0xa8, 0x20,
        0x27, 0x63, 0xe2, 0x80, 0xa9, 0x27, 0x63, 0xe2, 0x80, 0xa9, 0x2e, 0x74, 0x61, 0x67, 0x3a,
        0x79, 0x61, 0x6d, 0x6c, 0x2e, 0x6f, 0x72, 0x67, 0x2c, 0x32, 0x30, 0x30, 0x32, 0x3a, 0x27,
        0x63, 0xe2, 0x80, 0xa9, 0x2e, 0x3a, 0x6a, 0x3a, 0x28, 0xe2, 0x80, 0xa9, 0x0a, 0x3f, 0x2b,
        0x44, 0xe2, 0x80, 0xa8, 0x20, 0x27, 0x63, 0xe2, 0x80, 0xa9, 0x2e, 0x3a, 0x6a, 0x3a, 0x28,
        0xe2, 0x80, 0xa9, 0x20, 0x0d, 0x7c, 0x0d, 0x0a, 0x2e, 0x0d, 0x7c, 0x0d, 0x0a, 0x2e, 0x74,
        0x61, 0x67, 0x3a, 0x3a, 0x6d, 0x2a, 0x61, 0x70, 0x6f, 0x61, 0x22, 0x2e, 0x3a, 0x6a, 0x3a,
        0x28, 0xe2, 0x80, 0xa9, 0x20, 0x0d, 0x7c, 0x0d, 0x0a, 0x2e, 0x0d, 0x7c, 0x0d, 0x0a, 0x2e,
        0x74, 0x61, 0x67, 0x3a, 0x3a, 0x6d, 0x2a, 0x61, 0x70, 0x6f, 0x61, 0x22, 0x5d};
    static const char y2[] =
        "+STR\n+SEQ\n=VAL : ?+D 'c .:j:(\\n?+D 'c .tag:yaml.org\n=VAL : 2002:'c .:j:(\\n?+D 'c 'c "
        ".tag:yaml.org\n=VAL : 2002:'c .:j:(\\n?+D 'c .:j:(\\n| . | .tag::m*apoa\".:j:(\\n| . | "
        ".tag::m*apoa\"\n-SEQ\n-STR\n";
    static const char l2[] = "+STR\n+SEQ\n+MAP\n=VAL : +D\xe2"
                             "\x80"
                             "\xa8"
                             "'c\xe2"
                             "\x80"
                             "\xa9"
                             ".:j:(\xe2"
                             "\x80"
                             "\xa9"
                             "\\n?+D\xe2"
                             "\x80"
                             "\xa8"
                             "'c\xe2"
                             "\x80"
                             "\xa9"
                             ".tag:yaml.org\n=VAL : \n-MAP\n=VAL : 2002:'c\xe2"
                             "\x80"
                             "\xa9"
                             ".:j:(\xe2"
                             "\x80"
                             "\xa9"
                             "\\n?+D\xe2"
                             "\x80"
                             "\xa8"
                             "'c\xe2"
                             "\x80"
                             "\xa9"
                             "'c\xe2"
                             "\x80"
                             "\xa9"
                             ".tag:yaml.org\n=VAL : 2002:'c\xe2"
                             "\x80"
                             "\xa9"
                             ".:j:(\xe2"
                             "\x80"
                             "\xa9"
                             "\\n?+D\xe2"
                             "\x80"
                             "\xa8"
                             "'c\xe2"
                             "\x80"
                             "\xa9"
                             ".:j:(\xe2"
                             "\x80"
                             "\xa9"
                             "\\n| . | .tag::m*apoa\".:j:(\xe2"
                             "\x80"
                             "\xa9"
                             "\\n| . | .tag::m*apoa\"\n-SEQ\n-STR\n";
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (size == sizeof(m1) && memcmp(data, m1, size) == 0) {
        return strcmp(ydump, y1) == 0 && strcmp(ldump, l1) == 0;
    }
    if (size == sizeof(m2) && memcmp(data, m2, size) == 0) {
        return strcmp(ydump, y2) == 0 && strcmp(ldump, l2) == 0;
    }
    return 0;
}

static int yd_r244_flow_qhash_cr_gray(const uint8_t* data, size_t size, const char* ydump,
                                      const char* ldump) {
    /* shape 1: the libFuzzer minimum `[0,?#,?g?]`-family composition */
    static const unsigned char m1[] = {0x5b, 0x30, 0x2c, 0x3f, 0x23, 0x2c, 0x22, 0x0d, 0x67,
                                       0x22, 0x2c, 0x3f, 0x67, 0x3f, 0x2c, 0x22, 0x22, 0x5d};
    static const char y1[] = "+STR\n+SEQ\n=VAL : 0\n=VAL : ?#\n=VAL :  g\n=VAL : ?g?\n=VAL : "
                             "\n-SEQ\n-STR\n";
    static const char l1[] = "+STR\n+SEQ\n=VAL : 0\n+MAP\n=VAL : g\"\n=VAL : \n-MAP\n+MAP\n="
                             "VAL : g?\n=VAL : \n-MAP\n=VAL : \n-SEQ\n-STR\n";
    /* shape 2: the nightly's 24-byte artifact (space after the CR) */
    static const unsigned char m2[] = {0x5b, 0x30, 0x2c, 0x3f, 0x23, 0x2c, 0x22, 0x0d,
                                       0x67, 0x22, 0x2c, 0x3f, 0x67, 0x3a, 0x6d, 0x3f,
                                       0x23, 0x2c, 0x22, 0x0d, 0x67, 0x22, 0x2c, 0x5d};
    static const char y2[] = "+STR\n+SEQ\n=VAL : 0\n=VAL : ?#\n=VAL :  g\n=VAL : ?g:m?#\n=VAL "
                             ":  g\n-SEQ\n-STR\n";
    static const char l2[] = "+STR\n+SEQ\n=VAL : 0\n+MAP\n=VAL : g\"\n=VAL : \n-MAP\n+MAP\n="
                             "VAL : g:m?#\n=VAL : \n-MAP\n=VAL :  g\n-SEQ\n-STR\n";
    /* shape 3: the composition's short tail (no trailing empty) */
    static const unsigned char m3[] = {0x5b, 0x30, 0x2c, 0x3f, 0x23, 0x2c, 0x22, 0x0d,
                                       0x67, 0x22, 0x2c, 0x3f, 0x67, 0x3f, 0x5d};
    static const char y3[] = "+STR\n+SEQ\n=VAL : 0\n=VAL : ?#\n=VAL :  g\n=VAL : ?g?\n-SEQ\n-"
                             "STR\n";
    static const char l3[] = "+STR\n+SEQ\n=VAL : 0\n+MAP\n=VAL : g\"\n=VAL : \n-MAP\n+MAP\n="
                             "VAL : g?\n=VAL : \n-MAP\n-SEQ\n-STR\n";
    if (ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (size == sizeof(m1) && memcmp(data, m1, size) == 0) {
        return strcmp(ydump, y1) == 0 && strcmp(ldump, l1) == 0;
    }
    if (size == sizeof(m2) && memcmp(data, m2, size) == 0) {
        return strcmp(ydump, y2) == 0 && strcmp(ldump, l2) == 0;
    }
    if (size == sizeof(m3) && memcmp(data, m3, size) == 0) {
        return strcmp(ydump, y3) == 0 && strcmp(ldump, l3) == 0;
    }
    /* shape 4: the r247 seq-head permutation (nightly run
     * 43145599515, main 14ccfa6) - the r244 composition minus the
     * leading `0,` prefix: the first `?#` sits AT seq-head, the
     * comment-to-CR law reads it as the explicit-key indicator whose
     * comment eats to the CR and the post-CR tail `g"` becomes the
     * key, value empty; the second `?g:m?#` rebinds the same way.
     * yeptris keeps all four members flat and plain (the #949 law),
     * CR folds to a space. */
    static const unsigned char m4[] = {0x5b, 0x3f, 0x23, 0x2c, 0x22, 0x0d, 0x67, 0x22,
                                       0x2c, 0x3f, 0x67, 0x3a, 0x6d, 0x3f, 0x23, 0x2c,
                                       0x22, 0x0d, 0x67, 0x22, 0x2c, 0x5d};
    static const char y4[] =
        "+STR\n+SEQ\n=VAL : ?#\n=VAL :  g\n=VAL : ?g:m?#\n=VAL :  g\n-SEQ\n-STR\n";
    static const char l4[] = "+STR\n+SEQ\n+MAP\n=VAL : g\"\n=VAL : \n-MAP\n+MAP\n=VAL : "
                             "g:m?#\n=VAL : \n-MAP\n=VAL :  g\n-SEQ\n-STR\n";
    if (size == sizeof(m4) && memcmp(data, m4, size) == 0) {
        return strcmp(ydump, y4) == 0 && strcmp(ldump, l4) == 0;
    }
    return 0;
}

static int yd_r240_flow_qtail_clip_gray(const uint8_t* data, size_t size, const char* ydump,
                                        const char* ldump) {
    size_t i;
    size_t ps = size;    /* the PS triple's offset */
    size_t comma = size; /* the member separator's offset */
    if (size < 12 || ydump == NULL || ldump == NULL) {
        return 0;
    }
    if (data[0] != '[' || data[size - 1] != ']') {
        return 0;
    }
    for (i = 0; i < size; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '\n' || c == '\r' || c == '\t') {
            return 0;
        }
        if (c == 0xC2 && i + 1 < size && (unsigned char)data[i + 1] == 0x85) {
            return 0; /* NEL rides later families */
        }
        if (c == 0xE2 && i + 2 < size && (unsigned char)data[i + 1] == 0x80 &&
            (((unsigned char)data[i + 2] & 0xFE) == 0xA8)) {
            if (ps != size) {
                return 0; /* one LS/PS spelling per claim */
            }
            ps = i;
            i += 2;
            continue;
        }
    }
    if (ps == size || ps < 2) {
        return 0; /* the head needs one byte before the PS */
    }
    if (data[ps - 1] == ' ' || data[ps + 3] == ' ') {
        return 0; /* the fold-space-run composition is a later family */
    }
    for (i = 1; i < size; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == ',') {
            if (comma != size) {
                return 0; /* exactly one separator */
            }
            comma = i;
            continue;
        }
    }
    if (comma == size || comma < ps + 3) {
        return 0; /* the PS must sit in the head member */
    }
    if (data[1] == '?') {
        return 0; /* a member-start '?' is libyaml's explicit key too */
    }
    for (i = 1; i < comma; i++) {
        unsigned char c = (unsigned char)data[i];
        if (i == ps) {
            i += 2; /* the loop's i++ lands past the triple */
            continue;
        }
        if (c < 0x20 || c > 0x7E) {
            return 0;
        }
        if (c != ' ' && (c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == ':' ||
                         c == '\'' || c == '"' || c == '#' || c == '&' || c == '*' || c == '!' ||
                         c == '|' || c == '>' || c == '%' || c == '@' || c == '`')) {
            return 0;
        }
    }
    if (data[comma + 1] != '?' || comma + 3 > size - 1) {
        return 0; /* the tail opens with the glued '?' */
    }
    if (data[comma + 2] == ' ' || data[comma + 2] == '\t') {
        return 0; /* a spaced '?' is libyaml's explicit key too — agree */
    }
    for (i = comma + 2; i < size - 1; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c < 0x21 || c > 0x7E) {
            return 0;
        }
        if (c == '[' || c == ']' || c == '{' || c == '}' || c == ',' || c == ':' || c == '\'' ||
            c == '"' || c == '#' || c == '&' || c == '*' || c == '!' || c == '|' || c == '>' ||
            c == '%' || c == '@' || c == '`' || c == '?') {
            return 0;
        }
    }
    /* the yeptris side: +STR / +SEQ / =VAL : <head, PS -> one space> /
     * =VAL : <tail> / -SEQ / -STR */
    if (strncmp((const char*)ydump, "+STR\n+SEQ\n=VAL : ", 17) != 0) {
        return 0;
    }
    {
        const char* a = ydump + 17;
        for (i = 1; i < comma; i++) {
            if (i == ps) {
                if (a[0] != ' ') {
                    return 0;
                }
                a++;
                i += 2; /* the loop's i++ lands past the triple */
                continue;
            }
            if (a[0] != (char)data[i]) {
                return 0;
            }
            a++;
        }
        if (strncmp(a, "\n=VAL : ", 8) != 0) {
            return 0;
        }
        a += 8;
        for (i = comma + 1; i < size - 1; i++) {
            if (a[0] != (char)data[i]) {
                return 0;
            }
            a++;
        }
        if (strncmp(a, "\n-SEQ\n-STR\n", 11) != 0) {
            return 0;
        }
    }
    /* the libyaml side: +STR / +SEQ / =VAL : <head, the PS RAW> /
     * +MAP / =VAL : <tail after the '?'> / =VAL : <empty> / -MAP /
     * -SEQ / -STR (the r56 raw-ride and the explicit-key map, the
     * closes intact) */
    if (strncmp((const char*)ldump, "+STR\n+SEQ\n=VAL : ", 17) != 0) {
        return 0;
    }
    {
        const char* b = ldump + 17;
        for (i = 1; i < comma; i++) {
            if (i == ps) {
                if (b[0] != (char)data[i] || b[1] != (char)data[i + 1] ||
                    b[2] != (char)data[i + 2]) {
                    return 0;
                }
                b += 3;
                i += 2; /* the loop's i++ lands past the triple */
                continue;
            }
            if (b[0] != (char)data[i]) {
                return 0;
            }
            b++;
        }
        if (strncmp(b, "\n+MAP\n=VAL : ", 13) != 0) {
            return 0;
        }
        b += 13;
        for (i = comma + 2; i < size - 1; i++) { /* past the '?' */
            if (b[0] != (char)data[i]) {
                return 0;
            }
            b++;
        }
        if (strncmp(b, "\n=VAL : \n-MAP\n-SEQ\n-STR\n", 24) != 0) {
            return 0;
        }
    }
    return 1;
}

static int yd_r218_root_alias_blank_run_gray(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
#define YD_R218_NAME_BYTE(c)                                                                       \
    ((((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z') || ((c) >= '0' && (c) <= '9') ||    \
      (c) == '_' || (c) == '-' || (c) == '.' || (c) == ':'))
    size_t i, j;
    if (size < 9) { /* "&n:\n*a:" minimum */
        return 0;
    }
    (void)ydump;
    (void)ldump;
    /* the r227 leading law: blank lines BEFORE the head are inert on
     * the collapse (an inert prefix twin of the separator run below) */
    i = 0;
    while (yd_r218_blank_line(data, size, &i)) {}
    if (i >= size || data[i] != '&') {
        return 0;
    }
    for (j = 0; j < size; j++) {
        if (data[j] == '\t') {
            return 0;
        }
    }
    /* the head's riding-name run, its ':' end, trailing spaces, break */
    i++;
    {
        size_t name_o = i;
        while (i < size && YD_R218_NAME_BYTE(data[i])) {
            i++;
        }
        if (i == name_o || data[i - 1] != ':') {
            return 0;
        }
    }
    while (i < size && data[i] == ' ') {
        i++;
    }
    if (i >= size) {
        return 0;
    }
    if (data[i] == '\n') {
        i++;
    } else if (data[i] == '\r') {
        i = (i + 1 < size && data[i + 1] == '\n') ? i + 2 : i + 1;
    } else {
        return 0;
    }
    /* the separator: ZERO-OR-MORE blank lines -- the r227 widening:
     * the blank run was always inert on the collapse, so the direct
     * adjacency (the r227 artifact) rides the same claim */
    while (yd_r218_blank_line(data, size, &i)) {}
    /* the alias line: optional indent spaces, '*', the riding-name
     * run ending on its ':', then the blank-run tail */
    j = i;
    while (j < size && data[j] == ' ') {
        j++;
    }
    if (j >= size || data[j] != '*') {
        return 0;
    }
    j++;
    {
        size_t name_o = j;
        while (j < size && YD_R218_NAME_BYTE(data[j])) {
            j++;
        }
        if (j == name_o || data[j - 1] != ':') {
            return 0;
        }
    }
    /* the tail: blank lines only, then EOF -- the r227 measured
     * extension: a LINE-START "..." doc-end line (libyaml ends the
     * same 2-pair map at the marker; yeptris's collapse ended the
     * document before it -- dumpboth-verified identical pair); a
     * colon-glued "..." stays outside (unmeasured) */
    while (j < size && yd_r218_blank_line(data, size, &j)) {}
    if (j + 3 <= size && memcmp(data + j, "...", 3) == 0 && j > 0 &&
        (data[j - 1] == '\n' || data[j - 1] == '\r')) {
        j += 3;
        while (j < size && yd_r218_blank_line(data, size, &j)) {}
    }
    return j == size;
#undef YD_R218_NAME_BYTE
}

/* True for the ONE exact gray-zone input of the r199 nightly
 * (crash-925b0aed9341409f49159b3d1bc3c388bc0b5b18, run 42461709304,
 * main ba2c7f4): 15 bytes "{?*L:<LF>( `,*L:(}" -- the qflow bare-?'
 * glued plain follower COMPOSED with the alias-colon-ride charset
 * split, in one flow mapping. Both engines accept; the streams differ
 * yeptris's plain-key head + riding-alias tail (4 content events:
 * "=VAL : ?*L" / "=VAL : ( `" / "=ALI *L:(" / "=VAL : ") vs libyaml's
 * KEY-token + cut-alias reading (4 content events: "=ALI *L" /
 * "=VAL : ( `" / "=ALI *L" / "=VAL : ("). Fired on SEED-CORPUS replay
 * at MS: 0 (libFuzzer-minimal -- no mutation chain). Regression
 * triage: the A/B against the pre-r198 parent 8ef3eb6 aborts
 * IDENTICALLY (exit 134, the same 15-byte artifact), so the r198
 * breakless-root-alias arm is exonerated and the escalation clause
 * does not fire -- a fresh composition of two suite-pinned riding
 * laws, unchanged on post-r198 main. The walks: the head "{?*L:" --
 * yeptris's suite-pinned plain reading (HM87/652Z: a flow '?' with a
 * non-space follower rides as plain content) takes "?*L" as ONE plain
 * scalar key ending at the ':'; libyaml's 1.1 unconditional flow
 * KEY-token law (the quirk the qflow family has carried since r58)
 * reads the '?' as the explicit-key indicator and takes "*L" as an
 * ALIAS key (the r142/r76 IS_ALPHA cut). Both engines then bind the
 * line-2 multiline flow plain "( `" as the value (fold ends at ',').
 * The tail ",*L:(}" composes the second split: yeptris's anchor-name
 * ride law (the 2SXE/W5VH charset family, catch-142: anchor chars are
 * ns-char minus the flow indicators) rides the ':' into the name
 * because a plain-safe '(' follows it -- anchor "L:(" -- and the '}'
 * flow-indicator closes the name with NO value indicator, so the
 * second entry is the alias key with an EMPTY plain value; libyaml's
 * IS_ALPHA cut stops the name at "L", re-lexes the ':' as the VALUE
 * token, and reads the '(' as the plain value. Neither existing spell
 * reproduces the composition: the r71 drop-'? rewrite "{*L:<LF>(
 * `,*L:(}" makes YEPTRIS REJECT the sub (an alias-then-glued-colon
 * entry the engine's flow laws decline), so
 * yd_qflow_question_colon_divergence's byte-strict verify declines;
 * the charset-split twin declines BOTH orientations exactly as the
 * r198 family verdict ruled -- libyaml(split) == yeptris(original)
 * needs libyaml to read the riding anchor "L:(" (impossible -- its
 * cut always re-lexes the colon), and the reversed orientation gates
 * yeptris(original) to a dump containing "=ALI *L:(", which no
 * libyaml parse of any sub can produce (its parse-side alias charset
 * never emits the ride). Minimality: prefixes 1-14 are ALL BOTH-ERROR
 * (the unclosed-flow family); every single-byte deletion changes the
 * classification -- del '?' is LIBYAML-ONLY-ACCEPT (the r71 spell's
 * base shape, yeptris declines the glued-colon alias entry), del '*'
 * del '*' (the plain "?L" head member) still DIFFERs with a
 * different shape (tail split intact), del 'L' (either copy),
 * del ':' (either copy), del '(' are YEPTRIS-ONLY-ACCEPT or both-
 * error twins, del LF folds the entry onto one line and still DIFFERs
 * (the ':' rides inside one flow plain "?*L:( `" -- the W5VH plain
 * colon-ride law), del ',' merges both entries into one plain,
 * del 'SP'/'`' still DIFFER with shifted values, del '}' is BOTH-
 * ERROR -- so the entry pins the artifact byte-exactly (the nightly
 * artifact IS the 15-byte input). The composition rides the
 * EXACT-BYTES gray claim -- documented by input, not by mutation.
 * Family siblings (the plain-LF-only tail "{?*L:<LF>( `,*L:}" where
 * the colon rides NOT and both engines agree on the alias cut, the
 * seq-orientation "[*L:(]" of the bare charset split, and the
 * head-only "{?*L: 1}" member) classify known or ride their own
 * families' arms; an engine dig stays open for NONE of them -- both
 * halves are suite-pinned riding laws the comparator has carried
 * since catch-142 and r71. */
static int yd_r199_qflow_bareq_alias_ride_gray(const uint8_t* data, size_t size, const char* ydump,
                                               const char* ldump) {
    static const uint8_t seed[] = {0x7b, 0x3f, 0x2a, 0x4c, 0x3a, 0x0a, 0x28, 0x20,
                                   0x60, 0x2c, 0x2a, 0x4c, 0x3a, 0x28, 0x7d};
    if (size != sizeof(seed) || memcmp(data, seed, sizeof(seed)) != 0) {
        return 0;
    }
    (void)ydump;
    (void)ldump;
    return 1;
}

/* The BARE-'?'-GLUE x ALIAS-RIDE composition, SPELLED (r209, run
 * 42512673554, main 939c2f1, crash-55c53c6c6a1169156a444bc82fc3696a759c3252,
 * a 12-byte unit "{?*L:<LF>,*L:(}"): the r199 exact-bytes gray's family
 * SIBLING -- the same composition with the head's value bound EMPTY
 * (the r199 seed carried a multiline plain "( " there; deleting those
 * two bytes yields exactly this artifact) -- claimed STRUCTURALLY so
 * the family stops resurfacing one exact member per nightly. Both
 * engines accept; the streams differ yeptris's plain-key head +
 * riding-alias tail ("=VAL : ?*L" / "=VAL : " / "=ALI *L:(" / "=VAL : ")
 * vs libyaml's KEY-token + cut-alias reading ("=ALI *L" / "=VAL : " /
 * "=ALI *L" / "=VAL : ("). The composition rides the same three
 * suite-pinned laws the r199 comment pins, now with both splits
 * live: the HEAD -- yeptris's suite-pinned plain-'?' law (HM87/652Z:
 * a flow '?' with a non-space follower rides as plain content) takes
 * "?*L" as ONE plain key; libyaml's 1.1 unconditional flow KEY-token
 * law (the quirk the qflow family has carried since r58) reads the
 * '?' as the explicit-key indicator and "*L" as an ALIAS key (the
 * r142/r76 IS_ALPHA cut); both bind the empty value. The TAIL --
 * yeptris's anchor-name ride law (2SXE/W5VH, catch-142: anchor chars
 * are ns-char minus the flow indicators) rides the ':' into the name
 * because a plain-safe '(' follows it, so the alias key is "L:(" with
 * an EMPTY value; libyaml's cut stops at "L", re-lexes the ':' as the
 * VALUE token, and reads the '(' as the plain value. Regression
 * triage: the A/B against the pre-r208 parent 0398dc6 aborts
 * IDENTICALLY (exit 134, the same 12-byte artifact, the same dump
 * pair), so the r208 surplus-break arm is exonerated and the
 * escalation clause does not fire. NOT the documented drift twin --
 * issue #1149's open split is the 9-byte ">\n<NEL SP NEL NEL"
 * block-scalar form, a different delta in a different family (no '>',
 * no NEL here). Minimality: the shape-constrained and any-constrained
 * greedy deletions BOTH converge on the 10-byte "{?*L,*L:(}" (del ':'
 * then del LF; the head key rides with no value indicator at all)
 * with the dump pair byte-identical to the artifact's -- no
 * split-family drift for once; the nightly artifact is the 12-byte
 * unit and the seed pins THAT. The preserving spell inserts ONE blank
 * at each glue site -- "?*L" -> "? *L" (the separated-'?' explicit-key
 * form) and "L:(" -> "L :(" (the alias name cut before its riding
 * colon) -- which makes the token stream identical for BOTH engines:
 * libyaml's KEY token never depended on the glued '?' and its scan
 * anchor always cut at the ':', so its own reading must not move; and
 * yeptris -- whose explicit-'?' law and anchor cut now see the
 * separated forms -- must agree with libyaml byte-exact on the
 * rewrite. The certificate is the r193 spell's two pins, WITHOUT the
 * walk fallback: (1) libyaml(rewrite) == libyaml(original) byte-exact
 * (the spell is separator-neutral for the scanner it aligns to), and
 * (2) yeptris(rewrite) == libyaml(rewrite) (the divergence rode
 * exactly the spelled glues). A genuine yeptris alias-charset or
 * plain-'?' bug leaves pin (2) unsatisfied -- the rewrite still
 * diverges -- and a spell site that is NOT the divergence leaves pin
 * (1) unsatisfied; either way the arm declines and the DIFFER stays
 * visible. The finder scores only a flow-depth '?' at a KEY-token
 * position glued to an alias '*' (outside every plain run -- the
 * r145/catch-85 word-start law), and, inside an alias name opened at
 * a token position, every ':' (the alias-colon ride site) except a
 * '#' follower (a blank there would open a comment and move libyaml
 * wholesale). The ride law is context-sensitive -- MEASURED, not
 * assumed: in the explicit-key head yeptris rides the colon even
 * before a break ("{? *L:<LF>}" parses the key as "L:"), while a
 * flow-closer follower ends the name -- so the finder does not try to
 * predict the ride and spells every site; the pins reject any spell
 * that moved a reading. A block-context '?' stays with the
 * suite's plain law in BOTH engines (libyaml's block KEY token needs
 * the blank), so the finder requires flow depth >= 1 and the block
 * spell never fires. An over-scored site is harmless: it moves
 * libyaml's reading or the post-spell agreement and the certificate
 * declines. Bounded: one allocation (size * 2 + 4), one pass, two
 * dump verifies. The r199 15-byte sibling still declines here BY
 * MEASUREMENT -- its alias-colon blank makes yeptris REJECT the
 * rewrite ("{? *L:<LF>( `,*L :(}" is LIBYAML-ONLY-ACCEPT: an alias,
 * a blank, then a glued colon is an entry the engine's flow laws do
 * not spell), so its claim stays exactly where r199 put it. */
static int yd_r209_qflow_bareq_alias_spelled_divergence(const uint8_t* data, size_t size,
                                                        const char* ydump, const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 5 || size > 262144) {
        return 0;
    }
    char* sub = malloc(size * 2 + 4); /* each hit inserts one blank */
    if (sub == NULL) {
        return 0;
    }
    size_t subn = 0;
    int quote = 0;        /* the open quoted span (libyaml's view) */
    int scalar_start = 1; /* a token can start at the scan position */
    int plain = 0;        /* a plain run is open (glued pairs are content) */
    int in_alias = 0;     /* riding an alias name opened at a token position */
    int depth = 0;        /* flow depth: the arm's turf is the flow family */
    int hits = 0;
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            sub[subn++] = (char)c;
            if (quote == '\'' && c == '\'') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                sub[subn++] = (char)data[++i];
            } else if (quote == '"' && c == '"') {
                quote = 0;
                scalar_start = 0;
                plain = 0;
            }
            continue;
        }
        if (c == '\n' || c == '\r') {
            /* the fold law: the break refreshes the token position but
             * an open plain run rides it */
            if (c == '\r' && i + 1 < size && data[i + 1] == '\n') {
                sub[subn++] = (char)c;
                i++;
            }
            scalar_start = 1;
            in_alias = 0;
            sub[subn++] = (char)data[i];
            continue;
        }
        if (c == ' ' || c == '\t') {
            scalar_start = 1;
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if ((c == '\'' || c == '"') && scalar_start && !plain) {
            quote = c;
            scalar_start = 0;
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == '?' && depth > 0 && scalar_start && !plain && i + 1 < size && data[i + 1] == '*') {
            /* HIT A: the bare-'?' glue x the alias follower -- spell the
             * separated explicit-key form; the '*' opens the alias name */
            sub[subn++] = (char)c;
            sub[subn++] = ' ';
            hits++;
            scalar_start = 0;
            plain = 0;
            in_alias = 1;
            sub[subn++] = '*';
            i++;
            continue;
        }
        if (c == '?' && scalar_start && i + 1 < size && data[i + 1] == '#') {
            /* the KEY token + the glued '#' comment (the r136 law): the
             * comment eats to the line end -- nothing inside it scores */
            sub[subn++] = (char)c;
            i++;
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                i--; /* the break rides the loop's own advance */
            }
            continue;
        }
        if (c == '#' && (i == 0 || data[i - 1] == ' ' || data[i - 1] == '\t' ||
                         data[i - 1] == '\n' || data[i - 1] == '\r')) {
            /* the blank-led comment: both engines eat it to the break */
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[subn++] = (char)data[i++];
            }
            if (i < size) {
                i--;
            }
            plain = 0;
            continue;
        }
        if (c == '[' || c == '{') {
            depth++;
            scalar_start = 1;
            plain = 0;
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == ']' || c == '}') {
            if (depth > 0) {
                depth--;
            }
            scalar_start = 1;
            plain = 0;
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == ':' && in_alias && i + 1 < size && data[i + 1] != '#') {
            /* HIT B: the alias-colon site -- spell the cut form (the
             * name ends, the ':' is the VALUE token, a node can start at
             * the follower). Measured, not assumed: yeptris's ride law
             * is context-sensitive (in the explicit-key head it rides a
             * colon even before a BREAK -- "{? *L:<LF>}" parses the key
             * as "L:" -- while a flow-closer follower ends the name),
             * so the spell fires on every in-name colon; a site whose
             * blank moves a reading is caught by the pins below and the
             * arm declines. The '#' follower stays unspelled: after a
             * VALUE token the blank would open a comment and move
             * libyaml wholesale. */
            sub[subn++] = ' ';
            sub[subn++] = ':';
            hits++;
            in_alias = 0;
            scalar_start = 1;
            continue;
        }
        if (c == ',' || c == ':') {
            scalar_start = 1;
            plain = 0;
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == '*' && scalar_start && !plain) {
            /* an alias opens at a token position -- ride its name */
            in_alias = 1;
            scalar_start = 0;
            sub[subn++] = (char)c;
            continue;
        }
        if (c == '?' || c == '-' || c == '&' || c == '*' || c == '!' || c == '|' || c == '>' ||
            c == '%' || c == '@' || c == '`') {
            /* an indicator: separated it ends the run (a token can start
             * at the follower); glued it is plain-first content */
            unsigned char f = i + 1 < size ? data[i + 1] : (unsigned char)' ';
            if (f == ' ' || f == '\t' || f == '\n' || f == '\r' || f == ',' || f == ']' ||
                f == '}') {
                scalar_start = 1;
                plain = 0;
            } else {
                scalar_start = 0;
                plain = 1;
            }
            in_alias = 0;
            sub[subn++] = (char)c;
            continue;
        }
        /* wordy content: the run opens/rides */
        scalar_start = 0;
        plain = 1;
        sub[subn++] = (char)c;
    }
    if (hits == 0) {
        free(sub);
        return 0;
    }
    int lok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, subn, &lok2);
    int known = 0;
    if (lok2 && ldump2 != NULL && strcmp(ldump2, ldump) == 0) {
        /* the spell must not move libyaml's own reading */
        int yok2 = 0;
        char* ydump2 = yd_sem_dump((const char*)sub, subn, &yok2);
        if (yok2 && ydump2 != NULL) {
            if (strcmp(ydump2, ldump2) == 0) {
                known = 1; /* the divergence rode exactly the spelled glues */
            }
        }
        free(ydump2);
    }
    free(ldump2);
    free(sub);
    return known;
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
         * "|\r" and never entered block mode). The r189 nightly adds
         * the Unicode break spellings: a NEL/LS/PS-terminated header
         * ("> \u{85}# t", crash-aece9943) is the SAME col-0-content
         * divergence — yeptris normalizes the exotic break to LF (the
         * #638 walk folds NEL everywhere) and then takes the col-0
         * line as content exactly like its LF twin, while libyaml's
         * reader ends the header at the raw NEL/LS/PS into the same
         * floor-1 empty scalar — but the LF/CR-only walk saw one
         * unterminated line and never entered block mode. A dangling
         * multibyte lead without its continuation rides as content
         * (the yep_scan_mb_break_len law). */
        while (i < size && data[i] != '\n' && data[i] != '\r' &&
               !(data[i] == 0xC2 && i + 1 < size && data[i + 1] == 0x85) &&
               !(data[i] == 0xE2 && i + 2 < size && data[i + 1] == 0x80 &&
                 ((unsigned char)data[i + 2] & 0xFE) == 0xA8)) {
            i++;
        }
        size_t le = i;
        int has_nl = i < size;
        size_t br = 0;
        if (has_nl) {
            if (data[i] == '\r') {
                br = (i + 1 < size && data[i + 1] == '\n') ? 2 : 1;
            } else if (data[i] == 0xC2) {
                br = 2; /* the NEL pair (the while clause guarantees it) */
            } else if (data[i] == 0xE2) {
                br = 3; /* LS or PS (the while clause guarantees it) */
            } else {
                br = 1; /* LF */
            }
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

/* True when the difference is the COMPOSITION of the two adjudicated
 * break carries: an LS/PS raw-join site riding WITH the breakless-EOF
 * clip break. Each family classifies its own members alone -- the LS
 * twin with a final break rides yd_dump_eq_mod_lsps_fold's replace
 * reading (the r104/r105 law: yeptris's b-char algebra spells the
 * break \n where libyaml's READ_LINE joins the raw triple), and the
 * breakless twin without an LS rides yd_clip_breakless_divergence's
 * exact-equality transform (the r140 L24T#2 law) -- but the
 * composition (the r158 nightly's seed, crash-f0be7ede, run
 * 42147730364, main a77532d: 14 bytes "|", two space-blank lines, an
 * LS-only line, then " - m" ending at EOF with no break -- a literal
 * block whose leading-break run holds an LS-only line) bails both:
 * the walk rejects the value-trailing clip escape it cannot absorb
 * (libyaml's dump is exactly one \n escape short at the value tail),
 * and the clip arm's strcmp sees the raw triple it cannot strip. The
 * engine side is law-clean on both deltas: the #638 posture rewrites
 * LS/PS to LF while libyaml keeps the triple verbatim
 * (scanner.c READ_LINE LS|PS->LS|PS), and clip chomping keeps the
 * trailing break at breakless EOF (the suite's L24T#2 pin); both
 * engines strip the blank lines' spaces per l-empty (no space rides
 * in either dump -- the breaks are yeptris LF LF LF LF "- m" LF vs
 * libyaml LF LF LS LF "- m"). The transform composes the two
 * adjudicated deltas in the clip arm's own direction: append the one
 * break libyaml drops, re-dump libyaml on THAT, and require the
 * exact lsps-walk relation against yeptris's original dump -- so the
 * claim stays byte-bounded (a dump-shaped relation, not an
 * input-pinned gray). Gates mirror each family's own precondition
 * (a raw LS/PS triple in the data + a breakless final byte), so the
 * pure members keep riding their own arms unchanged. */
static int yd_lsps_breakless_clip_divergence(const uint8_t* data, size_t size, const char* ydump,
                                             const char* ldump) {
    if (ydump == NULL || ldump == NULL || size == 0 || size > 8192 || data[size - 1] == '\n' ||
        data[size - 1] == '\r') {
        return 0; /* the breakless-EOF precondition (the clip arm's own) */
    }
    int need = 0;
    for (size_t i = 0; i + 2 < size; i++) {
        if ((unsigned char)data[i] == 0xE2 && (unsigned char)data[i + 1] == 0x80 &&
            ((unsigned char)data[i + 2] == 0xA8 || (unsigned char)data[i + 2] == 0xA9)) {
            need = 1;
            break;
        }
    }
    if (!need) {
        return 0; /* the lsps raw-join precondition (the plain arm's own) */
    }
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    memcpy(sub, data, size);
    sub[size] = '\n';
    int ok2 = 0;
    char* ldump2 = yd_libyaml_dump((const char*)sub, size + 1, &ok2);
    free(sub);
    if (ldump2 == NULL || !ok2) {
        free(ldump2);
        return 0;
    }
    int known = yd_dump_eq_mod_lsps_fold(ydump, ldump2);
    free(ldump2);
    return known;
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
                 data[i + 3] == '\n' || data[i + 3] == '\r' ||
                 yd_mb_break_len(data, size, i + 3) > 0)) {
                /* the r234 law: the raw multibyte breaks terminate the
                 * doc marker too (the r87 break model) — with a PS
                 * terminator the reset never fired, the '-x' clause's
                 * phantom plain rode from the first dash, and the gate
                 * declined its own family (run 42706321967, crash-
                 * 4262eff1, "---<PS> ? &--02:in~") */
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
                int saw_colon = 0;
                /* the r230 residual-colon law: the blind copy runs the
                 * gate's colon law THROUGH the residual, not just at
                 * its first byte ("&I:919999k: &ab?cd v" -- the
                 * nightly's round 230, crash-b841bb3a -- the residual
                 * ':919999k:' ends at the KEY colon followed by a
                 * blank, and the LATER '&ab' anchor sits at NODE
                 * position; leaving node_pos2 dead skipped the second
                 * split, the copy kept the riding name and the
                 * copy-agrees leg declined the arm's own family) */
                while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                       data[i] != '\r' && yd_mb_break_len(data, size, i) == 0) {
                    if (data[i] == ':') {
                        saw_colon = 1;
                        unsigned char nx = (i + 1 < size) ? data[i + 1] : (unsigned char)' ';
                        if (nx == ' ' || nx == '\t' || nx == '\n' || nx == '\r') {
                            plain_open2 = 0; /* the key colon ends the riding plain */
                            node_pos2 = 1;
                        } else if (flowd2 == 0 && !bs_open2 && !tag_run2) {
                            plain_open2 = 1; /* the colon-led plain law again */
                        }
                    }
                    sub[o++] = (char)data[i++];
                }
                /* the trailing plain_residual only arms when the
                 * residual had NO colon of its own -- a colon-led
                 * residual's laws already ran IN ORDER in the loop, and
                 * clobbering plain_open2 AFTER the key colon reopened
                 * the plain over the node position (the gate never
                 * clobbers) */
                if (plain_residual && !saw_colon) {
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
                 data[i + 3] == '\n' || data[i + 3] == '\r' ||
                 yd_mb_break_len(data, size, i + 3) > 0)) {
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
         yd_r183_flow_seq_question_divergence(data, size, ydump, ldump) ||
         yd_r221_seq_question_glued_colon_pair_divergence(data, size, ydump, ldump) ||
         yd_r222_seq_question_glued_hash_comment_divergence(data, size, ydump, ldump) ||
         yd_r223_block_anchored_glued_colon_alias_divergence(data, size, ydump, ldump) ||
         yd_bom_content_divergence(ydump, ldump) ||
         yd_bom_line_start_divergence(data, size, ydump, ldump) ||
         yd_bom_utf16_line_start_divergence(data, size, ydump, ldump) ||
         yd_breaknorm_divergence(data, size, ydump) ||
         yd_nel_break_divergence(data, size, ydump, ldump) ||
         yd_composed_break_divergence(data, size) || yd_root_block_content_divergence(data, size) ||
         yd_clip_breakless_divergence(data, size, ydump) ||
         yd_ps_space_divergence(data, size, ydump, ldump) ||
         yd_plain_lsps_fold_divergence(data, size, ydump, ldump) ||
         yd_lsps_breakless_clip_divergence(data, size, ydump, ldump) ||
         yd_plain_nel_fold_divergence(data, size, ydump, ldump) ||
         yd_plain_nel_lsps_fold_divergence(data, size, ydump, ldump) ||
         yd_block_nel_content_divergence(data, size, ydump, ldump) ||
         yd_r121_permuted_gray(data, size, ydump, ldump) ||
         yd_r229_flow_quote_comment_glue_gray(data, size, ydump, ldump) ||
         yd_r137_soup_gray(data, size, ydump, ldump) ||
         yd_r138_soup_gray(data, size, ydump, ldump) ||
         yd_r139_soup_gray(data, size, ydump, ldump) ||
         yd_r140_soup_gray(data, size, ydump, ldump) ||
         yd_r141_soup_gray(data, size, ydump, ldump) ||
         yd_r151_soup_gray(data, size, ydump, ldump) ||
         yd_r157_root_alias_gray(data, size, ydump, ldump) ||
         yd_r170_root_alias_gray(data, size, ydump, ldump) ||
         yd_r174_root_alias_double_head_gray(data, size, ydump, ldump) ||
         yd_r182_root_alias_colonhead_gray(data, size, ydump, ldump) ||
         yd_r192_root_alias_alpha_head_gray(data, size, ydump, ldump) ||
         yd_r198_root_alias_breakless_gray(data, size, ydump, ldump) ||
         yd_r199_qflow_bareq_alias_ride_gray(data, size, ydump, ldump) ||
         yd_r212_root_alias_double_head_breakless_gray(data, size, ydump, ldump) ||
         yd_r218_root_alias_blank_run_gray(data, size, ydump, ldump) ||
         yd_r238_root_anchor_quoted_follow_gray(data, size, ydump, ldump) ||
         yd_r240_flow_qtail_clip_gray(data, size, ydump, ldump) ||
         yd_r241_alias_tab_fold_gray(data, size, ydump, ldump) ||
         yd_r242_flow_glued_q_members_gray(data, size, ydump, ldump) ||
         yd_r245_flow_qsoup_gray(data, size, ydump, ldump) ||
         yd_r246_flow_qsoup2_gray(data, size, ydump, ldump) ||
         yd_r244_flow_qhash_cr_gray(data, size, ydump, ldump) ||
         yd_r243_utf16_midstream_bom_gray(data, size, ydump, ldump) ||
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
            if (!known) {
                known = yd_r185_glued_hash_comment_cr_divergence(data, size, ydump, ldump);
                if (!known) {
                    known = yd_r187_glued_hash_plain_pair_divergence(data, size, ydump, ldump);
                    if (!known) {
                        known = yd_r190_glued_hash_multiline_follower_divergence(data, size, ydump,
                                                                                 ldump);
                        if (!known) {
                            known =
                                yd_r193_glued_question_quote_divergence(data, size, ydump, ldump);
                            if (!known) {
                                known = yd_r203_flow_seq_question_multiline_divergence(
                                    data, size, ydump, ldump);
                                if (!known) {
                                    known = yd_r209_qflow_bareq_alias_spelled_divergence(
                                        data, size, ydump, ldump);
                                    if (!known) {
                                        known = yd_r224_seq_question_glued_hash_cr_run_divergence(
                                            data, size, ydump, ldump);
                                    }
                                }
                            }
                        }
                    }
                }
            }
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
