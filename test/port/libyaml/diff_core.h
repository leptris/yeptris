/* diff_core.h — the shared differential classifier (TODO.impl/17C/19).
 *
 * ONE classification over (yeptris via yd_sem_dump, libyaml via the
 * same dump format): yepdiff the dev tool and fuzz_diff the fuzz
 * harness both call it — the verdict logic exists once. */
#ifndef YD_DIFF_CORE_H
#define YD_DIFF_CORE_H

#include <stddef.h>

typedef enum {
    YD_EQUAL = 0,
    YD_DIFFER,
    YD_BOTH_ERROR,
    YD_YEPTRIS_ONLY,
    YD_LIBYAML_ONLY,
} yd_verdict;

/* Dumps (malloc'd, caller frees both; NULL-safe) mirror the verdict:
 * present only for the side that parsed. */
yd_verdict yd_diff_classify(const char* in, size_t n, char** yep_dump, char** ly_dump);

const char* yd_verdict_name(yd_verdict v);

/* Loads "<id> <note>" lines (comments/# skipped); returns the count,
 * or -1 when the path cannot be read. Capacity: 256 ids. */
int yd_ledger_load(const char* path);

/* Is this corpus basename (id, "#N" suffix included) ledgered as an
 * upstream deviation? */
int yd_ledger_has(const char* corpus_id);

#endif /* YD_DIFF_CORE_H */
