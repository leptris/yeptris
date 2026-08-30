/* port.h — portability shims only; nothing YAML-specific lives here.
 *
 * Naming law: public API symbols are yeptris_*; internal symbols are yep_*.
 */
#ifndef YEP_PORT_H
#define YEP_PORT_H

#include <stddef.h>
#include <stdint.h>

#define YEP_UNUSED(x) ((void)(x))

#define YEP_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Architecture gates for the AOT SIMD TUs (TODO.impl/04). Both sides of
 * every extern pairing (TU and dispatch) use the same guards, so the link
 * always resolves regardless of which TUs CMake compiled. */
#if defined(__x86_64__) || defined(__i386__) || defined(_M_X64) || defined(_M_IX86)
#define YEP_ARCH_X86 1
#endif
#if defined(__aarch64__) || defined(__ARM_NEON) || defined(_M_ARM64)
#define YEP_ARCH_AARCH64 1
#endif

#endif /* YEP_PORT_H */
