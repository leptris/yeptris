/* plan.c — the compiled plan walk (TODO.restructure/87 slice one,
 * yeptris#293). Compile: the spec parses through the strict JSON DOM,
 * then lands as a flat column array + the collection path. Walk: one
 * linear pass over the tape's records — find the rows container,
 * count rows, then fill the typed columns per row by comparing key
 * spans against the planned leaf names (no host dispatch, no tree). */

#include <stdlib.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/json.h>
#include <yeptris/plan.h>

#include "doc.h"
#include "dom/dom.h"
#include "scan/json.h"

/* sview decode (the writer's wv, over the document's two regions) */
static yep_view sv_view(const yep_dom* d, yep_sview sv) {
    yep_view v = {NULL, 0};
    if (sv.len == 0) {
        return v;
    }
    v.len = sv.len;
    v.p = ((sv.off & YEP_SV_INPUT) ? d->str : d->input_base) + (sv.off & YEP_SV_OFF);
    return v;
}

typedef struct {
    char* name;
    uint32_t name_len;
    int kind;
} yep_plan_col;

struct yeptris_plan {
    yep_plan_col* cols;
    size_t ncols;
    char* path; /* the root map's key holding the rows; "" for seq root */
    size_t path_len;
};

typedef struct {
    int kind;
    int64_t* ints;
    double* floats;
    uint32_t* offs;
    uint32_t* lens;
    uint8_t* nulls;
} yep_result_col;

struct yeptris_plan_result {
    size_t rows;
    size_t ncols;
    yep_result_col* cols;
    void* block; /* one carved allocation backs every array */
};

YEPTRIS_API yeptris_plan* yeptris_plan_compile(const char* spec, size_t len, YeptrisStatus* st) {
    if (st != NULL) {
        *st = YEPTRIS_OK;
    }
    if (spec == NULL && len != 0) {
        if (st != NULL) {
            *st = YEPTRIS_ERROR_ARG;
        }
        return NULL;
    }
    YeptrisStatus pst = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_json(spec, len, &pst);
    if (doc == NULL) {
        if (st != NULL) {
            *st = pst;
        }
        return NULL;
    }
    const yeptris_document* d = (const yeptris_document*)doc;
    const yep_dnode* root = yep_dom_node(d->dom, d->dom->docs[0]);
    yeptris_plan* plan = calloc(1, sizeof(*plan));
    if (plan == NULL) {
        goto mem;
    }
    if (root == NULL || root->kind != 2) { /* the spec root is a mapping */
        goto bad;
    }
    /* kind: "seq" (rows under the path key or the root itself) or
     * "map" (rows under the path key); children: the leaf columns */
    const char* path = NULL;
    uint32_t path_len = 0;
    const yep_dnode* children = NULL;
    int saw_kind = 0;
    uint32_t pair = root->first_child;
    while (pair != UINT32_MAX) {
        const yep_dnode* kn = yep_dom_node(d->dom, pair);
        const yep_dnode* vn = yep_dom_node(d->dom, kn->next_sibling);
        yep_view kv = sv_view(d->dom, kn->value);
        yep_view vv = sv_view(d->dom, vn->value);
        if (kv.len == 4 && memcmp(kv.p, "kind", 4) == 0) {
            saw_kind = 1;
            if (!(vv.len == 3 && (memcmp(vv.p, "seq", 3) == 0 || memcmp(vv.p, "map", 3) == 0))) {
                goto bad;
            }
        } else if (kv.len == 4 && memcmp(kv.p, "path", 4) == 0) {
            if (vn->kind != 0) {
                goto bad;
            }
            path = vv.p;
            path_len = vv.len;
        } else if (kv.len == 8 && memcmp(kv.p, "children", 8) == 0) {
            if (vn->kind != 1) { /* children is a sequence */
                goto bad;
            }
            children = vn;
        } else {
            goto bad; /* unknown spec key */
        }
        pair = vn->next_sibling;
    }
    if (!saw_kind || children == NULL) {
        goto bad;
    }
    if (path != NULL) {
        plan->path = malloc(path_len + 1);
        if (plan->path == NULL) {
            goto mem;
        }
        memcpy(plan->path, path, path_len);
        plan->path[path_len] = 0;
        plan->path_len = path_len;
    } else {
        plan->path = calloc(1, 1);
        if (plan->path == NULL) {
            goto mem;
        }
    }
    plan->ncols = children->count;
    if (plan->ncols == 0) {
        goto bad;
    }
    plan->cols = calloc(plan->ncols, sizeof(*plan->cols));
    if (plan->cols == NULL) {
        goto mem;
    }
    uint32_t cn = children->first_child;
    for (size_t i = 0; i < plan->ncols; i++) {
        const yep_dnode* leaf = yep_dom_node(d->dom, cn);
        if (leaf == NULL || leaf->kind != 2) {
            goto bad;
        }
        const char* name = NULL;
        uint32_t name_len = 0;
        int kind = -1;
        uint32_t lp = leaf->first_child;
        while (lp != UINT32_MAX) {
            const yep_dnode* lk = yep_dom_node(d->dom, lp);
            const yep_dnode* lv = yep_dom_node(d->dom, lk->next_sibling);
            yep_view lkv = sv_view(d->dom, lk->value);
            yep_view lvv = sv_view(d->dom, lv->value);
            if (lkv.len == 4 && memcmp(lkv.p, "name", 4) == 0) {
                if (lv->kind != 0 || lvv.len == 0) {
                    goto bad;
                }
                name = lvv.p;
                name_len = lvv.len;
            } else if (lkv.len == 4 && memcmp(lkv.p, "kind", 4) == 0) {
                if (lvv.len == 3 && memcmp(lvv.p, "int", 3) == 0) {
                    kind = YEP_PLAN_INT;
                } else if (lvv.len == 5 && memcmp(lvv.p, "float", 5) == 0) {
                    kind = YEP_PLAN_FLOAT;
                } else if (lvv.len == 3 && memcmp(lvv.p, "str", 3) == 0) {
                    kind = YEP_PLAN_STR;
                } else if (lvv.len == 4 && memcmp(lvv.p, "bool", 4) == 0) {
                    kind = YEP_PLAN_BOOL;
                } else {
                    goto bad;
                }
            } else {
                goto bad;
            }
            lp = lv->next_sibling;
        }
        if (name == NULL || kind < 0) {
            goto bad;
        }
        plan->cols[i].name = malloc(name_len + 1);
        if (plan->cols[i].name == NULL) {
            goto mem;
        }
        memcpy(plan->cols[i].name, name, name_len);
        plan->cols[i].name[name_len] = 0;
        plan->cols[i].name_len = name_len;
        plan->cols[i].kind = kind;
        cn = leaf->next_sibling;
    }
    if (cn != UINT32_MAX) {
        goto bad; /* more children than counted */
    }
    yeptris_document_free(doc);
    return plan;

mem:
    if (st != NULL) {
        *st = YEPTRIS_ERROR_MEMORY;
    }
    goto out;
bad:
    if (st != NULL) {
        *st = YEPTRIS_ERROR_ARG;
    }
out:
    if (doc != NULL) {
        yeptris_document_free(doc);
    }
    yeptris_plan_free(plan);
    return NULL;
}

YEPTRIS_API void yeptris_plan_free(yeptris_plan* plan) {
    if (plan == NULL) {
        return;
    }
    for (size_t i = 0; i < plan->ncols; i++) {
        free(plan->cols[i].name);
    }
    free(plan->cols);
    free(plan->path);
    free(plan);
}

YEPTRIS_API size_t yeptris_plan_column_count(const yeptris_plan* plan) {
    return plan == NULL ? 0 : plan->ncols;
}

/* Finds the rows container's OPEN record: the root OPEN's child
 * matching the plan path (a map root), or the root OPEN itself (a
 * seq root). Returns the record index of the container OPEN, or 0
 * (the DOC record) when the shape disagrees. */
static size_t find_rows_open(const yeptris_json_tape* t, const yeptris_plan* plan) {
    size_t root = 1; /* the root OPEN (tape records start after DOC) */
    if (root >= t->count) {
        return 0;
    }
    if (t->kinds[root] != YEP_T_SEQ_OPEN && t->kinds[root] != YEP_T_MAP_OPEN) {
        return 0;
    }
    if (plan->path_len == 0 && t->kinds[root] == YEP_T_SEQ_OPEN) {
        return root; /* seq root: rows are the root's children */
    }
    if (t->kinds[root] != YEP_T_MAP_OPEN) {
        return 0;
    }
    /* map root: find the path key's value. Pairs are (STR key, value
     * record); a container value spans to its CLOSE (offs link) */
    size_t i = root + 1;
    while (i < t->count && i < t->offs[root]) {
        if (t->kinds[i] != YEP_T_STR) {
            return 0; /* not a key position */
        }
        if (t->lens[i] == plan->path_len &&
            memcmp((const char*)t->_src + t->offs[i], plan->path, plan->path_len) == 0) {
            size_t v = i + 1;
            if (v >= t->count) {
                return 0;
            }
            return v; /* the value OPEN (or scalar) record */
        }
        i++; /* the value */
        uint8_t vk = t->kinds[i];
        if (vk == YEP_T_MAP_OPEN || vk == YEP_T_SEQ_OPEN) {
            i = t->offs[i]; /* skip to its CLOSE */
        }
        i++;
    }
    return 0;
}

YEPTRIS_API yeptris_plan_result*
yeptris_tape_plan_walk(const yeptris_json_tape* tape, const yeptris_plan* plan, YeptrisStatus* st) {
    if (st != NULL) {
        *st = YEPTRIS_OK;
    }
    if (tape == NULL || plan == NULL || tape->count < 2) {
        if (st != NULL) {
            *st = YEPTRIS_ERROR_ARG;
        }
        return NULL;
    }
    size_t rows_open = find_rows_open(tape, plan);
    if (rows_open == 0) {
        if (st != NULL) {
            *st = YEPTRIS_ERROR_PARSE; /* document shape disagreement */
        }
        return NULL;
    }
    uint8_t rows_kind = tape->kinds[rows_open];
    if (rows_kind != YEP_T_SEQ_OPEN && rows_kind != YEP_T_MAP_OPEN) {
        if (st != NULL) {
            *st = YEPTRIS_ERROR_PARSE;
        }
        return NULL;
    }
    size_t rows_close = tape->offs[rows_open];

    /* pass one: count the row MAP_OPENs directly inside the container */
    size_t rows = 0;
    {
        int depth = 0;
        for (size_t i = rows_open + 1; i < rows_close; i++) {
            uint8_t k = tape->kinds[i];
            if (depth == 0 && k == YEP_T_MAP_OPEN) {
                rows++;
            }
            if (k == YEP_T_MAP_OPEN || k == YEP_T_SEQ_OPEN) {
                depth++;
            } else if (k == YEP_T_CLOSE) {
                depth--;
            }
        }
    }

    /* one carved block: cols array + per-column typed arrays */
    size_t ncols = plan->ncols;
    size_t col_bytes = ncols * sizeof(yep_result_col);
    size_t need =
        col_bytes + (ncols * rows) * (sizeof(int64_t) + sizeof(uint32_t) + sizeof(uint32_t) + 1);
    char* block = malloc(need + 8 * sizeof(double));
    if (block == NULL) {
        if (st != NULL) {
            *st = YEPTRIS_ERROR_MEMORY;
        }
        return NULL;
    }
    yeptris_plan_result* r = calloc(1, sizeof(*r));
    if (r == NULL) {
        free(block);
        if (st != NULL) {
            *st = YEPTRIS_ERROR_MEMORY;
        }
        return NULL;
    }
    r->rows = rows;
    r->ncols = ncols;
    r->block = block;
    r->cols = (yep_result_col*)block;
    char* p = block + col_bytes;
    for (size_t c = 0; c < ncols; c++) {
        yep_result_col* col = &r->cols[c];
        col->kind = plan->cols[c].kind;
        col->nulls = (uint8_t*)p;
        memset(col->nulls, 1, rows); /* missing until matched */
        p += rows;
        if (col->kind == YEP_PLAN_FLOAT) {
            col->floats = (double*)p;
            p += rows * sizeof(double);
        } else {
            col->ints = (int64_t*)p; /* int/bool share; str uses offs/lens */
            p += rows * sizeof(int64_t);
            if (col->kind == YEP_PLAN_STR) {
                col->offs = (uint32_t*)p;
                p += rows * sizeof(uint32_t);
                col->lens = (uint32_t*)p;
                p += rows * sizeof(uint32_t);
            }
        }
    }

    /* pass two: fill. Rows are MAP_OPEN..CLOSE at container depth 0;
     * each pair's key span matches a planned leaf name. */
    const char* src = (const char*)tape->_src;
    size_t row = 0;
    int depth = 0;
    int row_start = 0;
    for (size_t i = rows_open + 1; i < rows_close && row < rows; i++) {
        uint8_t k = tape->kinds[i];
        if (k == YEP_T_MAP_OPEN || k == YEP_T_SEQ_OPEN) {
            if (depth == 0 && k == YEP_T_MAP_OPEN) {
                row_start = 1; /* inside a row */
            }
            depth++;
            continue;
        }
        if (k == YEP_T_CLOSE) {
            depth--;
            if (depth == 0 && row_start) {
                row++;
                row_start = 0;
            }
            continue;
        }
        if (depth != 1 || !row_start || k != YEP_T_STR) {
            continue; /* not a key position inside a row map */
        }
        /* the key: match against the plan leaves */
        int col_idx = -1;
        for (size_t c = 0; c < ncols; c++) {
            if (tape->lens[i] == plan->cols[c].name_len &&
                memcmp(src + tape->offs[i], plan->cols[c].name, plan->cols[c].name_len) == 0) {
                col_idx = (int)c;
                break;
            }
        }
        if (col_idx < 0) {
            continue; /* not a planned leaf: skip its value below */
        }
        size_t v = i + 1;
        if (v >= rows_close) {
            break;
        }
        uint8_t vk = tape->kinds[v];
        yep_result_col* col = &r->cols[col_idx];
        switch (vk) {
        case YEP_T_NULL:
            break; /* stays null */
        case YEP_T_TRUE:
        case YEP_T_FALSE:
            if (col->kind == YEP_PLAN_BOOL) {
                col->ints[row] = vk == YEP_T_TRUE ? 1 : 0;
                col->nulls[row] = 0;
            } else if (col->kind == YEP_PLAN_STR) {
                col->offs[row] = tape->offs[v];
                col->lens[row] = tape->lens[v];
                col->nulls[row] = 0;
            }
            break;
        case YEP_T_STR:
            if (col->kind == YEP_PLAN_STR) {
                col->offs[row] = tape->offs[v];
                col->lens[row] = tape->lens[v];
                col->nulls[row] = 0;
            }
            break;
        case YEP_T_INT:
        case YEP_T_FLOAT:
        case YEP_T_NUM: {
            size_t j = 0;
            int64_t iv = 0;
            double dv = 0;
            int shape = 0;
            if (!yep_json_number_scan(src + tape->offs[v], tape->lens[v], &j, &shape, &iv, &dv) ||
                j != tape->lens[v]) {
                goto bad_row_value;
            }
            if (col->kind == YEP_PLAN_INT && shape == 0) {
                col->ints[row] = iv;
                col->nulls[row] = 0;
            } else if (col->kind == YEP_PLAN_FLOAT) {
                col->floats[row] = shape == 0 ? (double)iv : dv;
                col->nulls[row] = 0;
            } else if (col->kind == YEP_PLAN_STR) {
                col->offs[row] = tape->offs[v];
                col->lens[row] = tape->lens[v];
                col->nulls[row] = 0;
            }
            break;
        }
        default:
            /* container or mismatched scalar at a planned leaf: mark
             * the row value unusable but keep walking */
            break;
        }
        i = v; /* the value record is consumed */
        continue;
    bad_row_value:
        i = v;
        continue;
    }
    return r;
}

YEPTRIS_API void yeptris_plan_result_free(yeptris_plan_result* r) {
    if (r == NULL) {
        return;
    }
    free(r->block);
    free(r);
}

YEPTRIS_API size_t yeptris_plan_result_rows(const yeptris_plan_result* r) {
    return r == NULL ? 0 : r->rows;
}

YEPTRIS_API int yeptris_plan_result_kind(const yeptris_plan_result* r, size_t col) {
    if (r == NULL || col >= r->ncols) {
        return -1;
    }
    return r->cols[col].kind;
}

#define YEP_RESULT_ARR(name, type)                                                                 \
    YEPTRIS_API const type* yeptris_plan_result_##name(const yeptris_plan_result* r, size_t col) { \
        if (r == NULL || col >= r->ncols) {                                                        \
            return NULL;                                                                           \
        }                                                                                          \
        return r->cols[col].name;                                                                  \
    }

YEP_RESULT_ARR(ints, int64_t)
YEP_RESULT_ARR(floats, double)
YEP_RESULT_ARR(nulls, uint8_t)

YEPTRIS_API const uint32_t* yeptris_plan_result_str_offs(const yeptris_plan_result* r, size_t col) {
    if (r == NULL || col >= r->ncols) {
        return NULL;
    }
    return r->cols[col].offs;
}

YEPTRIS_API const uint32_t* yeptris_plan_result_str_lens(const yeptris_plan_result* r, size_t col) {
    if (r == NULL || col >= r->ncols) {
        return NULL;
    }
    return r->cols[col].lens;
}
