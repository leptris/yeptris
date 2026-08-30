/* suite.h — yaml-test-suite src/ frontmatter loader (TODO.impl/16).
 *
 * Independent of the library under test: a tiny hand parser for the
 * suite's src/<id>.yaml shape (one sequence item mapping with `|`
 * block fields), so a parser bug cannot corrupt the harness itself.
 */
#ifndef YTS_SUITE_H
#define YTS_SUITE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char id[8];
    char* yaml;  /* input (never NULL) */
    char* tree;  /* expected event tree */
    char* fail;  /* "true" when the input must fail to parse */
    char* error; /* legacy expected-error marker */
} yts_case;

/* Loads every src/<id>.yaml case; returns count, *out malloc'd array
 * (free with yts_free). Returns -1 on I/O failure. */
long yts_load(const char* src_dir, yts_case** out);
void yts_free(yts_case* cases, long n);

#ifdef __cplusplus
}
#endif

#endif /* YTS_SUITE_H */
