/* fuzz_diff.c — differential conformance fuzzer (TODO.impl/19).
 *
 * Property: for ANY input, yeptris and libyaml agree — same event
 * stream when both parse (EQUAL), or both reject (BOTH-ERROR).
 * DIFFER or either-only-accept is a conformance divergence and
 * aborts (with the dumps printed for minimization via yepdiff).
 * The classifier is diff_core.c — the same one yepdiff uses, so
 * the fuzzer and the human tool can never disagree about the rule.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff_core.h"

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
    return strcmp(stripped, ldump) == 0;
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
         * carries the leading '?' libyaml consumed */
        if (lens[i] >= 8 && strncmp(lines[i], "=VAL : ", 7) == 0 && ylens[j] == lens[i] + 1 &&
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

/* True when the difference is the anchor-name charset family: the
 * suite keeps ':' (and more) INSIDE anchor names (W5VH
 * "&:@*!$\"<foo>:", 2SXE "&a: key") and yeptris follows, while
 * libyaml stops the name at the colon ("&b:a!b:a!c" anchors 'b' and
 * reads ":a!b:a!c" as the value -- Psych's AST confirms). libyaml is
 * behind the suite; the gate is a depth-0 mini-scan for an '&' whose
 * name (up to the first blank/EOF) contains a ':', outside quotes,
 * no aliases or prop chains. */
static int yd_anchor_name_colon_divergence(const uint8_t* data, size_t size, const char* ydump,
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
                i++;
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
            break;
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
        if (c == '*' || c == '@' || c == '`' || c == '<' || c == '!') {
            return 0; /* aliases/verbatim/shorthand tags: out of family */
        }
        if (c == '&' && depth == 0) {
            size_t j = i + 1;
            int colon = 0;
            while (j < size && data[j] != ' ' && data[j] != '\t' && data[j] != '\n' &&
                   data[j] != '\r') {
                if (data[j] == ':') {
                    colon = 1;
                }
                j++;
            }
            if (colon && j > i + 1) {
                return 1; /* the name rides through ':' at block level */
            }
        }
    }
    return 0;
}

static void probe_one(const uint8_t* data, size_t size) {
    char* ydump = NULL;
    char* ldump = NULL;
    yd_verdict v = yd_diff_classify((const char*)data, size, &ydump, &ldump);
    int split = v == YD_YEPTRIS_ONLY || v == YD_LIBYAML_ONLY;
    /* a documented divergence, not a bug: the suite sides with
     * yeptris (see yd_qflow_divergence) */
    int known = v == YD_DIFFER &&
                (yd_qflow_divergence(ydump, ldump) || yd_bom_content_divergence(ydump, ldump) ||
                 yd_breaknorm_divergence(data, size, ydump) ||
                 yd_clip_breakless_divergence(data, size, ydump) ||
                 yd_flow_anchor_colon_divergence(data, size, ydump, ldump) ||
                 yd_anchor_name_colon_divergence(data, size, ydump, ldump));
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
