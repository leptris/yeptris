/* Dumps the recorder's events (with marks) as the same JSON shape the
 * psych dumper emits — the differ compares them directly. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <yeptris/events.h>

static const char* cb_of(uint8_t t) {
    switch (t) {
    case YEPTRIS_EV_STREAM_START: return "start_stream";
    case YEPTRIS_EV_STREAM_END: return "end_stream";
    case YEPTRIS_EV_DOCUMENT_START: return "start_document";
    case YEPTRIS_EV_DOCUMENT_END: return "end_document";
    case YEPTRIS_EV_SEQUENCE_START: return "start_sequence";
    case YEPTRIS_EV_SEQUENCE_END: return "end_sequence";
    case YEPTRIS_EV_MAPPING_START: return "start_mapping";
    case YEPTRIS_EV_MAPPING_END: return "end_mapping";
    case YEPTRIS_EV_SCALAR: return "scalar";
    case YEPTRIS_EV_ALIAS: return "alias";
    default: return "?";
    }
}

int main(int argc, char** argv) {
    (void)argc;
    printf("{\n");
    for (int a = 1; argv[a] != NULL; a++) {
        FILE* f = fopen(argv[a], "rb");
        if (f == NULL) continue;
        fseek(f, 0, SEEK_END);
        long n = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buf = malloc((size_t)n + 1);
        if (fread(buf, 1, (size_t)n, f) != (size_t)n) return 1;
        buf[n] = 0;
        fclose(f);
        YeptrisRecorder rec = yeptris_recorder_new();
        YeptrisStatus st = yeptris_recorder_feed(rec, buf, (size_t)n, 1);
        size_t count = 0;
        const YeptrisEventRecord* rs = yeptris_recorder_records(rec, &count);
        printf("\"%s\": [", argv[a]);
        size_t i = 0;
        if (st != YEPTRIS_OK) {
            printf("[\"ERROR\", \"status %d\"]", (int)st);
            i = 1; /* later entries join with the comma separator */
        }
        for (; i < count; i++) {
            printf("%s[\"%s\", %u, %u, %u, %u]", i ? ", " : "", cb_of(rs[i].type),
                   rs[i].line - 1, rs[i].col - 1,
                   rs[i].end_line - 1, rs[i].end_col - 1);
        }
        printf("]%s\n", argv[a + 1] ? "," : "");
        yeptris_recorder_free(rec);
        free(buf);
    }
    printf("}\n");
    return 0;
}
