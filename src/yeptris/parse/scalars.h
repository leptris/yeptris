/* scalars.h — scalar finishing (TODO.impl/09): raw spans → final content.
 *
 * Copy policy: single-line plain and clean single-quoted scalars stay
 * borrowed views (the ENGINE decides; nothing here allocates for them).
 * Everything that changes bytes (escapes, folding, block content) is
 * assembled into the engine's finish pool. Escape truth is declared here
 * ONCE; the emitter (13) imports these tables.
 */
#ifndef YEP_SCALARS_H
#define YEP_SCALARS_H

#include <stddef.h>

#include "common/string_view.h"
#include "memory/pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Finishes a double-quoted content span: escape sequences (\n \t \\ \" \x
 * \u \U …), backslash-at-EOL continuation, and multi-line folding.
 * Returns a pool-owned buffer of out_len bytes. */
char* yep_finish_double(const char* p, uint32_t start, uint32_t end, int multiline, yep_pool* pool,
                        uint32_t* out_len);

/* Finishes a single-quoted content span: '' → ' '; multi-line folding.
 * Returns a pool-owned buffer, or NULL when nothing changed (borrow ok). */
char* yep_finish_single(const char* p, uint32_t start, uint32_t end, int multiline, yep_pool* pool,
                        uint32_t* out_len);

/* One content line of a multi-line plain scalar or folded block scalar. */
typedef struct yep_fold_line {
    yep_view content;       /* trimmed line content (may be empty for blanks) */
    uint32_t breaks_before; /* line breaks between the previous content line and this one */
    int more_indented;      /* folded blocks: line is more indented than the block indent */
} yep_fold_line;

/* Plain-scalar folding: 1 break → ' ', n>1 breaks → (n-1) '\n'.
 * Empty-content lines contribute only their breaks. */
char* yep_fold_plain(const yep_fold_line* lines, size_t n, yep_pool* pool, uint32_t* out_len);

/* Block scalar assembly. folded=0 literal (every break is '\n'), else
 * folded (plain-fold rules; breaks around more-indented lines stay '\n').
 * chomp: 0 clip (single trailing '\n'), 1 strip, 2 keep (all
 * trailing_breaks newlines). trailing_breaks = breaks after the last
 * content line. */
char* yep_finish_block(const yep_fold_line* lines, size_t n, int folded, int chomp,
                       uint32_t trailing_breaks, yep_pool* pool, uint32_t* out_len);

#ifdef __cplusplus
}
#endif

#endif /* YEP_SCALARS_H */
