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
    char out[8192];
    size_t o = 0;
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
    /* the yeptris dump, split into lines for the parallel walk */
    const char* ylines[512];
    size_t ylens[512];
    int yn = 0;
    p = ydump;
    while (*p != '\0' && yn < 512) {
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
        if (lens[i] != ylens[j] || memcmp(lines[i], ylines[j], lens[i]) != 0) {
            return 0;
        }
        j++;
    }
    if (j != yn) {
        return 0; /* trailing yeptris lines: not the ?-family shape */
    }
    return 1;
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
/* True when the difference is the anchor-charset family's COLON-PAIR
 * member (the nightly's round 29, crash-47eb4054: "&k2: {\n}" and
 * "&k2: [1,\n2]"): the suite keeps ':' in anchor names (2SXE), so
 * yeptris reads the name run "&k2:" whole and parses an anchored flow
 * NODE, while libyaml truncates the name at ':' and reads a PAIR
 * (anchored empty key -> flow value). The existing charset carry's
 * extended set includes ':' (2SXE), so it never gates this shape.
 * Transform: split the name from its trailing colon-pair with one
 * space ("&k2:" -> "&k2 :"); when yeptris's dump of that equals
 * libyaml's dump of the original AND both engines agree on the split,
 * the divergence is exactly this family. */
static int yd_anchor_colon_pair_divergence(const uint8_t* data, size_t size, const char* ydump,
                                           const char* ldump) {
    if (ydump == NULL || ldump == NULL || size < 3) {
        return 0;
    }
    int quote = 0;
    int word_led = 1;
    int prop_word = 0;
    size_t split_at = (size_t)-1; /* the ':' offset to space out */
    for (size_t i = 0; i < size; i++) {
        unsigned char c = data[i];
        if (quote != 0) {
            if (quote == '\'' && c == '\'') {
                quote = 0;
            } else if (quote == '"' && c == '\\' && i + 1 < size) {
                i++;
            } else if (quote == '"' && c == '"') {
                quote = 0;
            }
            word_led = 0;
            prop_word = 0;
            continue;
        }
        if ((c == '\'' || c == '"') && (word_led || prop_word)) {
            quote = c;
            word_led = 0;
            prop_word = 0;
            continue;
        }
        if (c == '&' && split_at == (size_t)-1) {
            /* the name run: prop chars (the yeptris set, ':' included)
             * up to a blank/break; gate when it ends at a ':' whose
             * follower is blank/EOL -- the PAIR separator shape */
            size_t j = i + 1;
            while (j < size &&
                   !(data[j] == ' ' || data[j] == '\t' || data[j] == '\n' || data[j] == '\r' ||
                     data[j] == ',' || data[j] == '#' || data[j] == '[' || data[j] == ']' ||
                     data[j] == '{' || data[j] == '}')) {
                j++;
            }
            /* the run itself consumes the ':' (the yeptris name set
             * keeps it): the PAIR shape is a run ENDING in ':' whose
             * follower is blank/EOL */
            if (j > i + 1 && j < size && data[j - 1] == ':' &&
                (j >= size || data[j] == ' ' || data[j] == '\t' || data[j] == '\n' ||
                 data[j] == '\r')) {
                split_at = j - 1;
            }
            word_led = 0;
            prop_word = 1;
            i = j > i ? j - 1 : i;
            continue;
        }
        if (c == '&' || c == '!') {
            prop_word = 1;
            word_led = 0;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == '[' ||
                   c == ']' || c == '{' || c == '}' || c == ':' || c == '-' || c == '?') {
            word_led = 1;
            prop_word = 0;
        } else {
            word_led = 0;
            prop_word = 0;
        }
    }
    if (split_at == (size_t)-1) {
        return 0;
    }
    /* the split alone often still rejects in yeptris: the flow
     * value's continuation/closer lines sit at the key's column, the
     * 9C9N floor carry's shape (composed carries compose — catch
     * 166's law). Indent every non-blank line after the anchor's line
     * through the flow's first line-start closer by two spaces; the
     * dump equality below guards the composition (a semantic change
     * declines the carry). */
    size_t closer = split_at;
    for (size_t i = split_at; i < size; i++) {
        if (data[i] == '\n' && i + 1 < size) {
            size_t ls = i + 1;
            size_t ind = 0;
            while (ls + ind < size && (data[ls + ind] == ' ' || data[ls + ind] == '\t')) {
                ind++;
            }
            if (ls + ind < size && (data[ls + ind] == ']' || data[ls + ind] == '}')) {
                closer = ls + ind; /* the closer byte */
                break;
            }
        }
    }
    size_t bound = closer < size ? closer + 1 : size;
    char* sub = malloc(size + 3);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    memcpy(sub, data, split_at);
    o = split_at;
    sub[o++] = ' ';
    int at_line_start = 0; /* the anchor's own line: no indent */
    int in_span = 1;       /* indenting starts after the anchor's break */
    for (size_t i = split_at; i < bound; i++) {
        unsigned char c = data[i];
        if (in_span && c != '\n') {
            size_t ls = i;
            int blank_line = 1;
            for (size_t k = i; k < bound && data[k] != '\n'; k++) {
                if (data[k] != ' ' && data[k] != '\t') {
                    blank_line = 0;
                    break;
                }
            }
            if (!blank_line) {
                sub[o++] = ' ';
                sub[o++] = ' ';
                in_span = 0; /* indent once per line */
            }
            (void)ls;
            (void)at_line_start;
        }
        if (c == '\n' && i + 1 < bound) {
            in_span = 1;
        }
        sub[o++] = (char)c;
    }
    memcpy(sub + o, data + bound, size - bound);
    o += size - bound;
    int ok2 = 0;
    char* ydump2 = yd_sem_dump(sub, o, &ok2);
    free(sub);
    if (ydump2 == NULL || !ok2) {
        free(ydump2);
        return 0;
    }
    int known = strcmp(ydump2, ldump) == 0;
    free(ydump2);
    return known;
}

static int yd_ps_space_divergence(const uint8_t* data, size_t size, const char* ydump,
                                  const char* ldump);
static int yd_anchor_colon_pair_divergence(const uint8_t* data, size_t size, const char* ydump,
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
                header = (m == le);
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
    /* quotes open only at token starts (the catch-85 word-start law):
     * a mid-word quote is plain CONTENT ("v'"), and the naive tracker
     * swallowed the rest of the input — the '&' after it never gated
     * (the nightly's round 23, crash-c1dc27cf). A quote glued to a
     * props run ("&a\"x") still opens: props end at indicators. */
    int word_led = 1;  /* the next quote-ish byte starts a token */
    int prop_word = 0; /* inside/at the end of a & or ! run */
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
            word_led = 0;
            prop_word = 0;
            prev = (char)c;
            continue;
        }
        if ((c == '\'' || c == '"') && (word_led || prop_word)) {
            quote = c;
            word_led = 0;
            prop_word = 0;
            prev = (char)c;
            continue;
        }
        if (c == '&' || c == '!') {
            prop_word = 1;
            word_led = 0;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == '[' ||
                   c == ']' || c == '{' || c == '}' || c == ':' || c == '-' || c == '?') {
            word_led = 1;
            prop_word = 0;
        } else {
            word_led = 0;
            prop_word = 0;
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
            continue;
        }
        if (c == '&') {
            amps++;
        }
        if (c == '&' && !gate) {
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
    for (size_t i = 0; i < size;) {
        unsigned char c = data[i];
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
            i++;
            continue;
        }
        if (c == '&' || c == '!') {
            prop_word2 = 1;
            word_led2 = 0;
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ',' || c == '[' ||
                   c == ']' || c == '{' || c == '}' || c == ':' || c == '-' || c == '?') {
            word_led2 = 1;
            prop_word2 = 0;
        } else {
            word_led2 = 0;
            prop_word2 = 0;
        }
        if (c == '#' && (prev == ' ' || prev == '\t' || prev == '\n' || prev == '\r')) {
            /* verbatim through the comment's line break, then keep
             * scanning: anchors on later lines still split (the gate
             * walks the same set) */
            while (i < size && data[i] != '\n' && data[i] != '\r') {
                sub[o++] = (char)data[i++];
            }
            prev = '\n';
            continue;
        }
        if (c == '&') {
            sub[o++] = (char)c;
            i++;
            while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                   data[i] != '\r' && yd_anchor_byte_in_libyaml_set(data[i])) {
                sub[o++] = (char)data[i++];
            }
            if (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                data[i] != '\r') {
                sub[o++] = ' '; /* libyaml's truncation point becomes a token break */
                while (i < size && data[i] != ' ' && data[i] != '\t' && data[i] != '\n' &&
                       data[i] != '\r') {
                    sub[o++] = (char)data[i++];
                }
            }
            prev = ' ';
            continue;
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
    char* sub = malloc(size + 1);
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
static int yd_qflow_adjacent_divergence(const uint8_t* data, size_t size, const char* ldump) {
    if (ldump == NULL || size == 0 || size > 8192) {
        return 0;
    }
    char* sub = malloc(size + 1);
    if (sub == NULL) {
        return 0;
    }
    size_t o = 0;
    int found = 0;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '?' && i + 1 < size && (data[i + 1] == '"' || data[i + 1] == '\'')) {
            sub[o++] = ' ';
            found = 1;
        } else {
            sub[o++] = (char)data[i];
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
    int known =
        v == YD_DIFFER &&
        (yd_qflow_divergence(ydump, ldump) || yd_qflow_adjacent_divergence(data, size, ldump) ||
         yd_qflow_colon_divergence(data, size, ydump, ldump) ||
         yd_bom_content_divergence(ydump, ldump) || yd_breaknorm_divergence(data, size, ydump) ||
         yd_composed_break_divergence(data, size) || yd_root_block_content_divergence(data, size) ||
         yd_clip_breakless_divergence(data, size, ydump) ||
         yd_ps_space_divergence(data, size, ydump, ldump) ||
         yd_anchor_colon_pair_divergence(data, size, ydump, ldump) ||
         yd_flow_anchor_colon_divergence(data, size, ydump, ldump) ||
         yd_anchor_name_charset_divergence(data, size, ydump, ldump) ||
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
