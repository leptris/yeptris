/* cpu.c — feature detection via compiler builtins (no inline asm, no
 * platform headers). SSE2 is architectural on x86-64; NEON on AArch64.
 * Advanced flags come from __builtin_cpu_supports where available.
 */

#include <stdatomic.h>

#include "cpu.h"

#if defined(__x86_64__) || defined(__i386__)
#define YEP_X86 1
#endif

#if defined(__aarch64__) || defined(__ARM_NEON)
#define YEP_ARM_NEON_OK 1
#endif

static yep_cpu_features yep_cpu_compute(void) {
    yep_cpu_features f = {0};

#if defined(YEP_X86)
    f.sse2 = 1; /* architectural on x86-64 */
#if defined(__GNUC__) || defined(__clang__)
    f.sse3 = (unsigned)__builtin_cpu_supports("sse3");
    f.ssse3 = (unsigned)__builtin_cpu_supports("ssse3");
    f.sse41 = (unsigned)__builtin_cpu_supports("sse4.1");
    f.popcnt = (unsigned)__builtin_cpu_supports("popcnt");
    f.avx = (unsigned)__builtin_cpu_supports("avx");
    f.avx2 = (unsigned)__builtin_cpu_supports("avx2");
    f.bmi1 = (unsigned)__builtin_cpu_supports("bmi");
    f.bmi2 = (unsigned)__builtin_cpu_supports("bmi2");
#endif
#elif defined(YEP_ARM_NEON_OK)
    f.neon = 1;
#if defined(__ARM_FEATURE_CRC32)
    f.crc32 = 1;
#endif
#endif

    return f;
}

static _Atomic yep_cpu_features yep_cpu_cached;

yep_cpu_features yep_cpu_detect(void) {
    yep_cpu_features f = atomic_load_explicit(&yep_cpu_cached, memory_order_acquire);
    if (!f.detected) {
        f = yep_cpu_compute();
        f.detected = 1;
        atomic_store_explicit(&yep_cpu_cached, f, memory_order_release);
    }
    return f;
}
