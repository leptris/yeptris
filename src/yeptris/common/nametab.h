/* nametab.h — open-addressing view→id interner (TODO.impl/18A).
 *
 * The single primitive for name-keyed registries: engine anchor names,
 * DOM anchor binding, mapping-key interning. Keys are borrowed views —
 * the referenced storage must outlive the table (parse input or a
 * document-owned pool). No deletes, so no tombstones; upsert of an
 * existing key overwrites in place (last definition wins).
 */
#ifndef YEP_NAMETAB_H
#define YEP_NAMETAB_H

#include <stdint.h>

#include "common/string_view.h"
#include "memory/allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

/* get() miss sentinel; never a stored value (ids are array indices). */
#define YEP_NAMETAB_NIL UINT32_MAX

typedef struct yep_nametab {
    const yep_allocator* sys;
    uint32_t* slots; /* entry id + 1; 0 = empty */
    yep_view* keys;  /* borrowed, insertion-ordered */
    uint32_t* values;
    uint32_t count; /* live entries */
    uint32_t cap;   /* slot count, power of two */
} yep_nametab;

/* Returns 0 only on allocation failure (table unchanged). */
int yep_nametab_init(yep_nametab* t, const yep_allocator* sys);
void yep_nametab_free(yep_nametab* t);

/* Insert or overwrite. Returns 0 on allocation failure. */
int yep_nametab_set(yep_nametab* t, yep_view key, uint32_t value);

/* Drops all entries, keeping the allocations. */
void yep_nametab_clear(yep_nametab* t);

/* YEP_NAMETAB_NIL when absent. */
uint32_t yep_nametab_get(const yep_nametab* t, yep_view key);

#ifdef __cplusplus
}
#endif

#endif /* YEP_NAMETAB_H */
