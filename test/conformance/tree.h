/* tree.h — the event-tree adapter (TODO.impl/16): the single place that
 * knows the yaml-test-suite's test.event format. Our yep_event stream in,
 * indented suite-format text out.
 */
#ifndef YTS_TREE_H
#define YTS_TREE_H

#include <stddef.h>

#include "parse/engine.h"
#include "parse/events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char* buf;
    size_t len, cap;
    int depth; /* collection/doc nesting for indentation */
} yts_tree;

void yts_tree_init(yts_tree* t);
int yts_tree_on_event(void* ctx, const yep_event* ev);
void yts_tree_free(yts_tree* t);

#ifdef __cplusplus
}
#endif

#endif /* YTS_TREE_H */
