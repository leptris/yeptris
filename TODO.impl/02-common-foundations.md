# TODO.impl/02 — Common foundations: port, chartype, cpu, string view, error channel

Status: done · Depends: 01 · Layer: `src/yeptris/common` · PLAN.md phase: 0

## Goal

The primitives every other module builds on, each with exactly one owner:
platform portability, CPU feature detection, the byte-classification truth
table, the single string representation, and the internal error channel.

## Deliverables

- `common/port.h` — C99 portability shims only (nothing YAML-specific).
- `common/cpu.{h,c}` — feature detection (SSE2/AVX2/NEON flags), one-time
  lazy init, dispatch table consumed by 04. Port from leptris `common/cpu.c`.
- `common/chartype.{h,c}` — **the** byte classification truth table: a
  256-entry flags table (bit per class: space, tab, LF, CR, line-break,
  blank, flow-indicator, indicator-c, indicator-combining (`-?:`),
  printable, BOM, high-byte). This table is the semantic SSOT; SIMD kernels
  in 04 implement these same classes with vector compares — the differential
  tests in 04 prove the two representations agree byte-for-byte.
- `common/string_view.{h,c}` — `YepView { const char* p; uint32_t len; }`.
  **One string representation everywhere**: borrowed views into the input,
  or views into the document string arena after fold/escape copies. No
  module may store `(char*, int)` pairs or C strings internally.
  Helpers: `yep_view_eq`, `yep_view_hash` (FNV-1a 32/64; anchor + key
  interning depend on it), `yep_view_starts_with`.
- `common/error.{h,c}` — internal error record `{code, line, col, offset,
  msg[192]}` + X-macro declaring the error-code enum and its string table
  side-by-side (adding a code = one X-macro line). Channels: thread-local
  last-error + per-document slot (the leptris dual-channel contract);
  public API surface maps internal codes to public `YeptrisStatus`.

## Design decisions

- OCP: new byte classes = new bit in chartype (one line) + kernel agreement
  test picks it up; new error codes = one X-macro line.
- MECE: chartype answers "what class of byte is this"; scan (06) answers
  "where do structures start"; nothing else re-classifies bytes.
- No `errno`, no printf-style formatting on the hot path; messages are
  composed once at error creation.

## Phases

A. port.h, cpu, string_view with unit tests.
B. chartype table + golden tests against the YAML spec character productions
   (c-indicator set: `-?:,[]{}#&*!|>'"%@\``; printability rules incl. C0/C1
   exclusions and the TAB special cases).
C. error channel + thread-isolation tests (two threads, interleaved
   failures, no cross-talk).

## Acceptance

- Unit tests green; chartype table matches spec productions exactly
  (test cases derived from yaml-test-suite character sections).
- Thread-isolation test passes under TSAN (leptris `test/concurrency`
  pattern).

## References

- `~/src/leptris/leptris/src/leptris/common/{port.h,cpu.c,string_view.h,chartype.h}`.
- `~/src/leptris/leptris/src/leptris/error.c` (dual-channel pattern).
