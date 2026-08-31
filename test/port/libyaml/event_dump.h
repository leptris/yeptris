/* event_dump.h — canonical event-stream dump (TODO.impl/17).
 *
 * SSOT for the differential format: both sides (libyaml golden
 * generator and the yeptris runner) adapt their events into
 * yd_event and render through yd_line, so a diff between the two
 * programs is a semantic diff, never a formatting diff.
 *
 * One line per event, no indentation:
 *   +STR / -STR
 *   +DOC | +DOC --- / -DOC | -DOC ...
 *   +MAP [+{} ] [&anchor] [<tag>] / -MAP        ({} when flow)
 *   +SEQ +[[] ] [&anchor] [<tag>] / -SEQ
 *   =VAL <style :'"|>> [&anchor] [<tag>] <escaped>
 *   =ALI *name
 *   !ERROR  (terminal line on a failed parse)
 */
#ifndef YD_EVENT_DUMP_H
#define YD_EVENT_DUMP_H

#include <stdio.h>
#include <string.h>

typedef enum {
    YD_STREAM_START,
    YD_STREAM_END,
    YD_DOC_START, /* explicit_doc */
    YD_DOC_END,   /* explicit_doc */
    YD_MAP_START, /* flow, anchor, tag */
    YD_MAP_END,
    YD_SEQ_START, /* flow, anchor, tag */
    YD_SEQ_END,
    YD_SCALAR, /* style, anchor, tag, value */
    YD_ALIAS   /* anchor */
} yd_type;

typedef enum { YD_PLAIN = 0, YD_SINGLE, YD_DOUBLE, YD_LITERAL, YD_FOLDED } yd_style;

typedef struct {
    yd_type type;
    yd_style style;     /* scalar */
    int flow;           /* collection */
    int explicit_doc;   /* document */
    const char* anchor; /* NULL when absent */
    const char* tag;    /* NULL when absent */
    const char* value;  /* scalar bytes (not NUL-terminated) */
    size_t value_len;
} yd_event;

/* Appends the event's line (with trailing \n) to out. */
static void yd_escape_append(const char* v, size_t n, char* out, size_t cap, size_t* len) {
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)v[i];
        const char* r = NULL;
        char hex[8];
        switch (c) {
        case '\\':
            r = "\\\\";
            break;
        case '\n':
            r = "\\n";
            break;
        case '\r':
            r = "\\r";
            break;
        case '\t':
            r = "\\t";
            break;
        default:
            if (c < 0x20 || c == 0x7f) {
                snprintf(hex, sizeof(hex), "\\x%02x", c);
                r = hex;
            }
            break;
        }
        if (r != NULL) {
            size_t rl = strlen(r);
            if (*len + rl < cap) {
                memcpy(out + *len, r, rl);
                *len += rl;
            }
        } else if (*len + 1 < cap) {
            out[(*len)++] = (char)c;
        }
    }
}

/* Renders one event line into out (cap bytes); returns line length. */
static size_t yd_line(const yd_event* ev, char* out, size_t cap) {
    size_t len = 0;
    char props[512];
    size_t pl = 0;
    props[0] = '\0';
    if (ev->anchor != NULL) {
        pl += (size_t)snprintf(props + pl, sizeof(props) - pl, " &%s", ev->anchor);
    }
    if (ev->tag != NULL) {
        pl += (size_t)snprintf(props + pl, sizeof(props) - pl, " <%s>", ev->tag);
    }
    switch (ev->type) {
    case YD_STREAM_START:
        len = (size_t)snprintf(out, cap, "+STR\n");
        break;
    case YD_STREAM_END:
        len = (size_t)snprintf(out, cap, "-STR\n");
        break;
    case YD_DOC_START:
        len = (size_t)snprintf(out, cap, "+DOC%s\n", ev->explicit_doc ? " ---" : "");
        break;
    case YD_DOC_END:
        len = (size_t)snprintf(out, cap, "-DOC%s\n", ev->explicit_doc ? " ..." : "");
        break;
    case YD_MAP_START:
        len = (size_t)snprintf(out, cap, "+MAP%s%s\n", ev->flow ? " {}" : "", props);
        break;
    case YD_MAP_END:
        len = (size_t)snprintf(out, cap, "-MAP\n");
        break;
    case YD_SEQ_START:
        len = (size_t)snprintf(out, cap, "+SEQ%s%s\n", ev->flow ? " []" : "", props);
        break;
    case YD_SEQ_END:
        len = (size_t)snprintf(out, cap, "-SEQ\n");
        break;
    case YD_ALIAS:
        len = (size_t)snprintf(out, cap, "=ALI *%s\n", ev->anchor ? ev->anchor : "");
        break;
    case YD_SCALAR: {
        static const char sc[5] = {':', '\'', '"', '|', '>'};
        size_t w = (size_t)snprintf(out, cap, "=VAL %c%s ", sc[ev->style], props);
        yd_escape_append(ev->value, ev->value_len, out, cap, &w);
        if (w < cap) {
            out[w++] = '\n';
        }
        len = w;
        break;
    }
    }
    return len;
}

#endif /* YD_EVENT_DUMP_H */
