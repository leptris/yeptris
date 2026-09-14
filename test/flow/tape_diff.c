/* tape_diff.c — the tape-vs-DOM differential (TODO.restructure/85).
 *
 * PERMANENT gate: every corpus document runs through
 * yeptris_parse_json AND yeptris_parse_json_tape — statuses must
 * match exactly, and where both succeed the tape's record stream must
 * equal the DOM tree: OPEN/CLOSE nesting against the child chains,
 * scalar kinds against node style + tag id, span bytes against value
 * bytes (escaped strings keep raw spans by contract — those compare
 * on structure only, counted). The same discipline as
 * flow-direct-diff. */

#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris/dom.h>
#include <yeptris/json.h>
#include <yeptris/tape.h>

#include "doc.h"

static char* slurp(const char* path, size_t* len) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) {
        fclose(f);
        return NULL;
    }
    char* buf = (char*)malloc((size_t)n + 1);
    if (buf == NULL || fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *len = (size_t)n;
    return buf;
}

static int g_fail = 0;
static int g_cases = 0;
static int g_escaped_skips = 0;

/* the DOM side as an event stream: ENTER per node in tree order, LEAVE
 * when a container's children run out — iterative (corpus inputs reach
 * the depth cap; a permanent gate never rides the C stack) */
typedef struct {
    const yep_dom* d;
    struct {
        uint32_t id;   /* the container */
        uint32_t next; /* its next child, UINT32_MAX when spent */
    }* st;
    size_t n;
    size_t cap;
} dom_it;

enum { EV_ENTER = 0, EV_LEAVE, EV_END };

static int dom_it_push(dom_it* it, uint32_t id) {
    if (it->n >= it->cap) {
        size_t cap = it->cap * 2;
        void* ns = realloc(it->st, cap * sizeof(it->st[0]));
        if (ns == NULL) {
            return 0;
        }
        it->st = ns;
        it->cap = cap;
    }
    it->st[it->n].id = id;
    it->st[it->n].next = it->d->nodes[id].first_child;
    it->n++;
    return 1;
}

static int dom_it_next(dom_it* it, uint32_t* id) {
    if (it->n == 0) {
        return EV_END;
    }
    size_t top = it->n - 1;
    if (it->st[top].next == UINT32_MAX) {
        *id = it->st[top].id;
        it->n--;
        return EV_LEAVE;
    }
    uint32_t cur = it->st[top].next;
    it->st[top].next = it->d->nodes[cur].next_sibling;
    *id = cur;
    if ((it->d->nodes[cur].kind == YEP_DOM_MAPPING || it->d->nodes[cur].kind == YEP_DOM_SEQUENCE) &&
        !dom_it_push(it, cur)) {
        return EV_END; /* OOM: fail via stream truncation */
    }
    return EV_ENTER;
}

static int scalar_kind(const yep_dnode* n) {
    if (n->style == YEP_STYLE_DOUBLE_QUOTED) {
        return YEP_T_STR;
    }
    switch (n->tag_id) {
    case YEPTRIS_TAG_NULL:
        return YEP_T_NULL;
    case YEPTRIS_TAG_BOOL:
        return YEP_T_BOOL;
    case YEPTRIS_TAG_INT:
        return YEP_T_INT;
    case YEPTRIS_TAG_FLOAT:
        return YEP_T_FLOAT;
    default:
        return -1;
    }
}

/* one DOM node against one tape record; escaped strings compare on
 * kind only (the tape keeps the raw span by contract) */
static int enter_matches(const yep_dom* d, uint32_t id, const yeptris_json_tape* t, size_t i,
                         const char* buf, const char* name) {
    const yep_dnode* n = &d->nodes[id];
    if (n->kind == YEP_DOM_MAPPING || n->kind == YEP_DOM_SEQUENCE) {
        uint8_t want = n->kind == YEP_DOM_MAPPING ? YEP_T_MAP_OPEN : YEP_T_SEQ_OPEN;
        if (t->kinds[i] != want) {
            fprintf(stderr, "TAPE-DIFF %s: node %u kind %u record %zu %u\n", name, id,
                    (unsigned)n->kind, i, t->kinds[i]);
            return 0;
        }
        return 1;
    }
    int want = scalar_kind(n);
    if (want < 0 || t->kinds[i] != (uint8_t)want) {
        fprintf(stderr, "TAPE-DIFF %s: scalar node %u (tag %u style %u) record %zu %u\n", name, id,
                (unsigned)n->tag_id, (unsigned)n->style, i, t->kinds[i]);
        return 0;
    }
    yep_view v = yep_dom_view(d, n->value);
    if (v.p == NULL && v.len != 0) {
        fprintf(stderr, "TAPE-DIFF %s: node %u value view unresolved\n", name, id);
        return 0;
    }
    if (want == YEP_T_STR && memchr(buf + t->offs[i], '\\', t->lens[i]) != NULL) {
        g_escaped_skips++; /* raw span by contract: bytes not comparable */
        return 1;
    }
    if (v.len != t->lens[i] || (v.len > 0 && memcmp(v.p, buf + t->offs[i], v.len) != 0)) {
        fprintf(stderr, "TAPE-DIFF %s: value node %u len %u/%u off %u\n", name, id, v.len,
                t->lens[i], t->offs[i]);
        return 0;
    }
    return 1;
}

static int tape_matches_dom(const yep_dom* d, const yeptris_json_tape* t, const char* buf,
                            const char* name) {
    if (d->dcount != 1 || t->count < 2 || t->kinds[0] != YEP_T_DOC) {
        fprintf(stderr, "TAPE-DIFF %s: shape dcount %u count %zu\n", name, d->dcount, t->count);
        return 0;
    }
    uint32_t root = d->docs[0];
    if (!enter_matches(d, root, t, 1, buf, name)) {
        return 0;
    }
    dom_it it = {d, NULL, 0, 64};
    it.st = malloc(it.cap * sizeof(it.st[0]));
    int ok = it.st != NULL;
    if (ok && (d->nodes[root].kind == YEP_DOM_MAPPING || d->nodes[root].kind == YEP_DOM_SEQUENCE)) {
        ok = dom_it_push(&it, root);
    }
    size_t i = 2;
    while (ok) {
        uint32_t id;
        int ev = dom_it_next(&it, &id);
        if (ev == EV_END) {
            ok = (i == t->count); /* both streams spent together */
            if (!ok) {
                fprintf(stderr, "TAPE-DIFF %s: tape has %zu records, DOM ends at %zu\n", name,
                        t->count, i);
            }
            break;
        }
        if (i >= t->count) {
            fprintf(stderr, "TAPE-DIFF %s: DOM outlives the tape at %zu\n", name, i);
            ok = 0;
            break;
        }
        if (ev == EV_LEAVE) {
            if (t->kinds[i] != YEP_T_CLOSE) {
                fprintf(stderr, "TAPE-DIFF %s: want CLOSE at %zu got %u\n", name, i, t->kinds[i]);
                ok = 0;
                break;
            }
            i++;
        } else {
            if (!enter_matches(d, id, t, i, buf, name)) {
                ok = 0;
                break;
            }
            i++;
        }
    }
    free(it.st);
    return ok;
}

static void diff_one(const char* name, const char* buf, size_t len) {
    g_cases++;
    YeptrisStatus ds = YEPTRIS_OK;
    yeptris_document* doc = (yeptris_document*)yeptris_parse_json(buf, len, &ds);
    yeptris_json_tape tape;
    YeptrisStatus ts = yeptris_parse_json_tape(buf, len, &tape);
    if (ds != ts) {
        fprintf(stderr, "TAPE-DIFF %s: status %d vs %d\n", name, ds, ts);
        g_fail++;
    } else if (ds == YEPTRIS_OK) {
        if (!tape_matches_dom(doc->dom, &tape, buf, name)) {
            g_fail++;
        }
    }
    yeptris_document_free((YeptrisDocument)doc);
    yeptris_tape_free(&tape);
}

static const char* k_pins[] = {
    "[1, \"a\", true, null, 2.5]",
    "{\"id\": 7, \"name\": \"alpha\", \"vals\": [1, 2, 3], \"ok\": true}",
    "{\"a\": [1, {\"b\": null}]}",
    "[[[[1, 2]]], [[3, 4]]]",
    "[\"esc: a\\\"b\\\\c\\n\\t\\u0041\"]",
    "[\"\\u00e9\\u65e5\\u672c\\u8a9e\"]",
    "[0, -0, 1e-3, 1E+2, 3.14159, -2.5e10]",
    "[9223372036854775807, -9223372036854775808]",
    "42",
    "\"hi\"",
    "true",
    "null",
    "-3.5",
    "[]",
    "{}",
    "[\n  1,\n  2\n]\n",
    "[\t1\t]",
    "{a: 1}", /* not strict JSON: both reject */
    "[1,]",
    "[1] tail",
    "",
    " ",
    NULL,
};

static unsigned long ti_next(unsigned long* s) {
    *s ^= *s << 13;
    *s ^= *s >> 7;
    *s ^= *s << 17;
    return *s;
}

/* random status parity over the JSON-class alphabet (the indexed walk
 * pins: accept exactly what parse_json accepts, byte for byte) */
static void fuzz_parity(void) {
    static const char alpha[] = {'{', '}', '[', ']', ':', ',', '"', '1', '2', '-',  '.', 'e',
                                 't', 'r', 'u', 'f', 'a', 'l', 's', 'n', ' ', '\\', 'x', '\t'};
    unsigned long s = 42;
    char buf[64];
    for (long t = 0; t < 2000000; t++) {
        size_t len = ti_next(&s) % 48;
        for (size_t i = 0; i < len; i++) {
            buf[i] = (char)alpha[ti_next(&s) % (sizeof(alpha) - 1)];
        }
        g_cases++;
        YeptrisStatus ds = YEPTRIS_OK;
        yeptris_document* doc = (yeptris_document*)yeptris_parse_json(buf, len, &ds);
        yeptris_json_tape tape;
        YeptrisStatus ts = yeptris_parse_json_tape(buf, len, &tape);
        if (ds != ts) {
            fprintf(stderr, "TAPE-DIFF fuzz: status %d vs %d buf=[", ds, ts);
            for (size_t i = 0; i < len; i++) {
                fputc(buf[i] >= 0x20 ? buf[i] : '.', stderr);
            }
            fprintf(stderr, "]\n");
            g_fail++;
        } else if (ds == YEPTRIS_OK && doc != NULL) {
            if (!tape_matches_dom(doc->dom, &tape, buf, "fuzz")) {
                fprintf(stderr, "TAPE-DIFF fuzz buf=[");
                for (size_t i = 0; i < len; i++) {
                    fputc(buf[i] >= 0x20 ? buf[i] : '.', stderr);
                }
                fprintf(stderr, "] (len %zu)\n", len);
                g_fail++;
            }
        }
        yeptris_document_free((YeptrisDocument)doc);
        yeptris_tape_free(&tape);
        if (g_fail > 8) {
            return;
        }
    }
}

int main(int argc, char** argv) {
    for (int i = 0; k_pins[i] != NULL; i++) {
        diff_one(k_pins[i], k_pins[i], strlen(k_pins[i]));
    }
    fuzz_parity();
    int files = 0;
    for (int a = 1; a < argc; a++) {
        DIR* dir = opendir(argv[a]);
        if (dir == NULL) {
            continue;
        }
        struct dirent* ent;
        while ((ent = readdir(dir)) != NULL) {
            size_t n = strlen(ent->d_name);
            if (n < 5 || strcmp(ent->d_name + n - 5, ".json") != 0) {
                continue;
            }
            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", argv[a], ent->d_name);
            size_t len = 0;
            char* buf = slurp(path, &len);
            if (buf != NULL) {
                diff_one(path, buf, len);
                free(buf);
                files++;
            }
        }
        closedir(dir);
    }
    printf("tape-diff: %d cases (%d corpus files), %d escaped-span skips, %d failures\n", g_cases,
           files, g_escaped_skips, g_fail);
    return g_fail != 0;
}
