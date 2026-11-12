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

/* Appends pre + the NUL-terminated s into out at off, keeping out
 * NUL-terminated at the written end, and returns the WOULD-BE length
 * (the snprintf discipline the clamps in yd_line ride). The props
 * appends below were snprintf " &%s" / " <%s>" forms — gcc 14 (the
 * riscv64 leg's trixie toolchain) proves pl, which rides those
 * would-be lengths, can leave a later append less room than an
 * unknown %s may write and -Werror=format-truncation kills the build
 * (gcc 13's x86_64 legs never saw it). The byte loop makes the bound
 * explicit; output is byte-identical to the snprintf forms. */
static size_t yd_str_append(const char* pre, const char* s, char* out, size_t cap, size_t off) {
    size_t w = off;
    size_t n = strlen(pre) + strlen(s);
    if (w < cap) {
        for (const char* p = pre; *p != '\0' && w + 1 < cap; p++) {
            out[w++] = *p;
        }
        for (; *s != '\0' && w + 1 < cap; s++) {
            out[w++] = *s;
        }
        out[w] = '\0';
    }
    return n;
}

/* Renders one event line into out (cap bytes); returns line length. */
static size_t yd_line(const yd_event* ev, char* out, size_t cap) {
    size_t len = 0;
    char props[8192];
    size_t pl = 0;
    props[0] = '\0';
    if (ev->anchor != NULL) {
        pl += yd_str_append(" &", ev->anchor, props, sizeof(props), pl);
    }
    if (ev->tag != NULL) {
        pl += yd_str_append(" <", ev->tag, props, sizeof(props), pl);
        if (pl + 1 < sizeof(props)) {
            props[pl++] = '>';
        }
        if (pl < sizeof(props)) {
            props[pl] = '\0';
        }
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
        /* snprintf's return is the WOULD-BE length: clamp to what the
         * buffer actually holds or the callers memcpy past it (the
         * nightly's round 59 follow-up: a 4.3KB props line vs the
         * 4096-byte line buffer) */
        len = w < cap ? w : cap - 1;
        break;
    }
    }
    /* snprintf's return is the WOULD-BE length, and EVERY branch rides it
     * (+MAP/+SEQ props lines too — the nightly's round 61, a 4971-char
     * mapping tag): clamp to what the buffer actually holds or the
     * callers memcpy past it. */
    return len < cap ? len : cap - 1;
}

#endif /* YD_EVENT_DUMP_H */
