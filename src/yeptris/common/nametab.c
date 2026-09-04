/* nametab.c — open-addressing view→id interner (TODO.impl/18A).
 *
 * FNV-1a with a murmur-style finalizer (FNV alone has weak low bits,
 * fatal under power-of-two masking); linear probing for cache locality.
 * Rehashes at 70% load. Replaces the O(n) anchor scans that made
 * anchor-heavy documents quadratic (40k anchors: 0.5 MB/s → linear).
 */

#include "common/nametab.h"

#include <string.h>

#define NT_MIN_CAP 64u
#define NT_LOAD_PCT 70u

static uint64_t hash_round(uint64_t h, uint64_t k) {
    h ^= k;
    h *= 0x9E3779B97F4A7C15ull;
    h ^= h >> 29;
    return h;
}

/* Word-at-a-time, constant-size loads only (a variable-length memcpy
 * lowers to a memmove call — one per probe cost ~3% of anchor-heavy
 * parse). Hash values are build-internal: nothing persists them. */
static uint64_t load_small(const char* p, uint32_t n) {
    uint64_t k = 0;
    if (n & 4) {
        uint32_t v;
        memcpy(&v, p, 4);
        k = v;
        p += 4;
    }
    if (n & 2) {
        uint16_t v;
        memcpy(&v, p, 2);
        k = (k << 16) | v;
        p += 2;
    }
    if (n & 1) {
        k = (k << 8) | (unsigned char)*p;
    }
    return k;
}

uint32_t yep_view_hash(yep_view s) {
    uint64_t h = 0x9E3779B97F4A7C15ull ^ (uint64_t)s.len;
    if (s.len <= 8) {
        h = hash_round(h, load_small(s.p, s.len));
    } else if (s.len <= 16) {
        uint64_t k0;
        memcpy(&k0, s.p, 8);
        h = hash_round(hash_round(h, k0), load_small(s.p + 8, s.len - 8));
    } else {
        uint64_t k0, kn;
        memcpy(&k0, s.p, 8);
        memcpy(&kn, s.p + s.len - 8, 8);
        h = hash_round(hash_round(hash_round(h, k0), kn), (uint64_t)s.len * 0x85EBCA77ull);
    }
    h ^= h >> 32;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 29;
    return (uint32_t)(h ^ (h >> 32));
}

int yep_nametab_init(yep_nametab* t, const yep_allocator* sys) {
    if (t == NULL || sys == NULL) {
        return 0;
    }
    t->sys = sys;
    t->slots = yep_alloc(sys, NT_MIN_CAP * sizeof(*t->slots));
    t->keys = yep_alloc(sys, NT_MIN_CAP * sizeof(*t->keys));
    t->values = yep_alloc(sys, NT_MIN_CAP * sizeof(*t->values));
    if (t->slots == NULL || t->keys == NULL || t->values == NULL) {
        yep_nametab_free(t);
        return 0;
    }
    memset(t->slots, 0, NT_MIN_CAP * sizeof(*t->slots));
    t->count = 0;
    t->cap = NT_MIN_CAP;
    return 1;
}

void yep_nametab_free(yep_nametab* t) {
    if (t == NULL) {
        return;
    }
    yep_free(t->sys, t->slots);
    yep_free(t->sys, t->keys);
    yep_free(t->sys, t->values);
    t->slots = NULL;
    t->keys = NULL;
    t->values = NULL;
    t->count = 0;
    t->cap = 0;
}

/* Reinserts every entry into newcap slots; call only with a power of two. */
static int nt_rehash(yep_nametab* t, uint32_t newcap) {
    uint32_t* slots = yep_alloc(t->sys, newcap * sizeof(*slots));
    if (slots == NULL) {
        return 0;
    }
    memset(slots, 0, newcap * sizeof(*slots));
    uint32_t mask = newcap - 1;
    for (uint32_t i = 0; i < t->count; i++) {
        uint32_t j = yep_view_hash(t->keys[i]) & mask;
        while (slots[j] != 0) {
            j = (j + 1) & mask;
        }
        slots[j] = i + 1;
    }
    yep_free(t->sys, t->slots);
    t->slots = slots;
    t->cap = newcap;
    return 1;
}

static int nt_grow(yep_nametab* t) {
    uint32_t newcap = t->cap * 2;
    yep_view* keys = yep_alloc(t->sys, newcap * sizeof(*keys));
    uint32_t* values = yep_alloc(t->sys, newcap * sizeof(*values));
    if (keys == NULL || values == NULL) {
        yep_free(t->sys, keys);
        yep_free(t->sys, values);
        return 0;
    }
    if (t->count > 0) {
        memcpy(keys, t->keys, (size_t)t->count * sizeof(*keys));
        memcpy(values, t->values, (size_t)t->count * sizeof(*values));
    }
    yep_free(t->sys, t->keys);
    yep_free(t->sys, t->values);
    t->keys = keys;
    t->values = values;
    return nt_rehash(t, newcap);
}

int yep_nametab_reserve(yep_nametab* t, uint32_t n) {
    if (t == NULL || t->cap == 0) {
        return 0;
    }
    if ((uint64_t)n * 100 <= (uint64_t)t->cap * NT_LOAD_PCT) {
        return 1; /* already room for n at load <= NT_LOAD_PCT */
    }
    uint64_t need = ((uint64_t)n * 100 + NT_LOAD_PCT - 1) / NT_LOAD_PCT;
    uint32_t newcap = NT_MIN_CAP;
    while ((uint64_t)newcap < need) {
        newcap *= 2;
    }
    yep_view* keys = yep_alloc(t->sys, newcap * sizeof(*keys));
    uint32_t* values = yep_alloc(t->sys, newcap * sizeof(*values));
    if (keys == NULL || values == NULL) {
        yep_free(t->sys, keys);
        yep_free(t->sys, values);
        return 0;
    }
    if (t->count > 0) {
        memcpy(keys, t->keys, (size_t)t->count * sizeof(*keys));
        memcpy(values, t->values, (size_t)t->count * sizeof(*values));
    }
    yep_free(t->sys, t->keys);
    yep_free(t->sys, t->values);
    t->keys = keys;
    t->values = values;
    return nt_rehash(t, newcap);
}

uint32_t yep_nametab_get(const yep_nametab* t, yep_view key) {
    if (t == NULL || t->cap == 0) {
        return YEP_NAMETAB_NIL;
    }
    uint32_t mask = t->cap - 1;
    uint32_t j = yep_view_hash(key) & mask;
    for (;;) {
        uint32_t entry = t->slots[j];
        if (entry == 0) {
            return YEP_NAMETAB_NIL;
        }
        if (yep_view_eq(t->keys[entry - 1], key)) {
            return t->values[entry - 1];
        }
        j = (j + 1) & mask;
    }
}

void yep_nametab_clear(yep_nametab* t) {
    if (t == NULL || t->cap == 0) {
        return;
    }
    memset(t->slots, 0, (size_t)t->cap * sizeof(*t->slots));
    t->count = 0;
}

int yep_nametab_set(yep_nametab* t, yep_view key, uint32_t value) {
    if (t == NULL || t->cap == 0) {
        return 0;
    }
    uint32_t mask = t->cap - 1;
    uint32_t j = yep_view_hash(key) & mask;
    for (;;) {
        uint32_t entry = t->slots[j];
        if (entry == 0) {
            break;
        }
        if (yep_view_eq(t->keys[entry - 1], key)) {
            t->values[entry - 1] = value; /* last definition wins */
            return 1;
        }
        j = (j + 1) & mask;
    }
    if ((uint64_t)(t->count + 1) * 100 > (uint64_t)t->cap * NT_LOAD_PCT) {
        if (!nt_grow(t)) {
            return 0;
        }
        mask = t->cap - 1;
        j = yep_view_hash(key) & mask;
        while (t->slots[j] != 0) {
            j = (j + 1) & mask;
        }
    }
    t->keys[t->count] = key;
    t->values[t->count] = value;
    t->slots[j] = t->count + 1;
    t->count++;
    return 1;
}
