/* tree.c — yep_event stream → yaml-test-suite event-tree format. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

static void tput(yts_tree* t, const char* s, size_t n) {
    if (t->len + n + 1 > t->cap) {
        t->cap = t->cap ? t->cap * 2 : 1024;
        while (t->len + n + 1 > t->cap) {
            t->cap *= 2;
        }
        t->buf = realloc(t->buf, t->cap);
    }
    memcpy(t->buf + t->len, s, n);
    t->len += n;
    t->buf[t->len] = '\0';
}

static void tputs(yts_tree* t, const char* s) {
    tput(t, s, strlen(s));
}

static void tindent(yts_tree* t) {
    for (int i = 0; i < t->depth; i++) {
        tputs(t, " ");
    }
}

/* Suite value escaping: \n \r \t \\ become their backslash forms. */
static void tvalue(yts_tree* t, const yep_event* ev) {
    int dq = (ev->style == YEP_STYLE_DOUBLE_QUOTED);
    for (uint32_t i = 0; i < ev->value.len; i++) {
        char c = ev->value.p[i];
        if (c == '\\') {
            tputs(t, "\\\\");
        } else if (c == '\n') {
            tputs(t, "\\n");
        } else if (c == '\r') {
            tputs(t, "\\r");
        } else if (c == '\t') {
            /* double-quoted values keep the escape; others visualize */
            tputs(t, dq ? "\\t" : "——»");
        } else {
            tput(t, &c, 1);
        }
    }
}

/* Tags arrive RESOLVED from the engine (%TAG handles, !! shorthand);
 * the adapter only renders <...>. */
static void ttag(yts_tree* t, yep_view tag) {
    tputs(t, " <");
    tput(t, tag.p, tag.len);
    tputs(t, ">");
}

static void tprops(yts_tree* t, const yep_event* ev) {
    if (ev->anchor.len > 0) {
        tputs(t, " &");
        tput(t, ev->anchor.p, ev->anchor.len);
    }
    if (ev->tag.len > 0) {
        ttag(t, ev->tag);
    }
}

void yts_tree_init(yts_tree* t) {
    t->buf = NULL;
    t->len = 0;
    t->cap = 0;
    t->depth = 0;
}

int yts_tree_on_event(void* ctx, const yep_event* ev) {
    yts_tree* t = (yts_tree*)ctx;
    switch (ev->type) {
    case YEP_EV_STREAM_START:
        tputs(t, "+STR\n");
        t->depth = 1;
        break;
    case YEP_EV_STREAM_END:
        tputs(t, "-STR\n");
        t->depth = 0;
        break;
    case YEP_EV_DOCUMENT_START:
        tindent(t);
        tputs(t, "+DOC");
        if (ev->style == 1) {
            tputs(t, " ---");
        }
        tputs(t, "\n");
        t->depth = 2;
        break;
    case YEP_EV_DOCUMENT_END:
        t->depth = 1;
        tindent(t);
        tputs(t, "-DOC");
        if (ev->style == 1) {
            tputs(t, " ...");
        }
        tputs(t, "\n");
        break;
    case YEP_EV_SEQ_START:
        tindent(t);
        tputs(t, "+SEQ");
        if (ev->flow) {
            tputs(t, " []");
        }
        tprops(t, ev);
        tputs(t, "\n");
        t->depth++;
        break;
    case YEP_EV_SEQ_END:
        t->depth--;
        tindent(t);
        tputs(t, "-SEQ");
        if (ev->flow) {
            tputs(t, " []");
        }
        tputs(t, "\n");
        break;
    case YEP_EV_MAP_START:
        tindent(t);
        tputs(t, "+MAP");
        if (ev->flow) {
            tputs(t, " {}");
        }
        tprops(t, ev);
        tputs(t, "\n");
        t->depth++;
        break;
    case YEP_EV_MAP_END:
        t->depth--;
        tindent(t);
        tputs(t, "-MAP");
        if (ev->flow) {
            tputs(t, " {}");
        }
        tputs(t, "\n");
        break;
    case YEP_EV_SCALAR:
        tindent(t);
        tputs(t, "=VAL");
        tprops(t, ev);
        switch (ev->style) {
        case YEP_STYLE_SINGLE_QUOTED:
            tputs(t, " '");
            break;
        case YEP_STYLE_DOUBLE_QUOTED:
            tputs(t, " \"");
            break;
        case YEP_STYLE_LITERAL:
            tputs(t, " |");
            break;
        case YEP_STYLE_FOLDED:
            tputs(t, " >");
            break;
        default:
            tputs(t, " :");
            break;
        }
        tvalue(t, ev);
        tputs(t, "\n");
        break;
    case YEP_EV_ALIAS:
        tindent(t);
        tputs(t, "=ALI *");
        tput(t, ev->value.p, ev->value.len);
        tputs(t, "\n");
        break;
    default:
        break;
    }
    return 0;
}

void yts_tree_free(yts_tree* t) {
    free(t->buf);
    t->buf = NULL;
}
