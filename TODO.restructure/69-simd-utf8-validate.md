# 69 — SIMD UTF-8 / c-printable validation

## Problem

`validate()` in `encoding/utf8_validate.c` runs once per parse over the
whole document whenever the parse-time `gate_scan` sees non-ASCII (every
multibyte corpus — the gate conflates ctrl-chars with legal UTF-8, so a
single `é` sends the entire input through the fallback). The 8-byte SWAR
path breaks at the first non-ASCII byte; the rest walks the per-byte
lead/continuation DFA:

| shape | samples | share |
|---|---|---|
| scalar-heavy | 443 | ~4.5% |
| block-heavy  | 396 | ~4.0% |
| anchor-heavy | 152 | ~4.4% |

ryml does no input validation on its happy path — we must make ours
near-free instead of skipping it (conformance is priority 1; the validator
also reports line/col for bad input, which stays).

## Design

Lemire/Keiser–Thornton-class SIMD validator, both ISAs:

- **UTF-8 well-formedness:** first continuation range via the two-shuffle
  nibble classification (vqtbl1q_u8 / pshufb), overlong + surrogate +
  >U+10FFFF by the standard error-intersection method. 16/32 bytes per
  iteration; the scalar DFA remains for: tails, the byte carrying the
  error, and the error *position* report (SIMD says "chunk has error",
  scalar re-walks only that chunk).
- **c-printable ASCII:** the existing SWAR tests vectorize directly
  (`c < 0x20`, `c == 0x7F` — cmpgt/cmpeq + OR + movemask); multibyte code
  points pass the DFA and are checked printable by the same scalar walk
  (rare: only the chunk containing the cp).
- Kernel lives in `encoding/` (charset ownership — MECE), dispatched via
  `yep_text_active()` alongside the scan kernels. The validator keeps its
  exact error-position contract; differential specs pin SIMD == scalar
  over the fuzz/adversarial corpus already used by the encoding specs.

## Acceptance

- Encoding differential specs green on both ISAs; error positions
  byte-identical to the scalar validator on the error corpus.
- Full ctest green; sanitizers green.
- h2h: multibyte shapes (block/scalar/anchor all carry multibyte content)
  measurably up; ledger entry with before/after.

## Closure (2026-09-12) — root cause was two broken gate masks

Profiles on PURE-ASCII corpora showed `validate` hot (4-8%) — impossible
via the parse gate unless gate_scan itself was wrong. It was:

- AVX2: `_mm256_andnot(allow, ones)` = ~allow was OR'd in unconditionally
  → gate answered "ruling needed" for EVERY input → the whole-document
  SWAR validator ran on every parse. This was the ubuntu scalar-heavy
  asymmetry (CI 0.41-0.54x vs local 0.78x).
- NEON: the allow mask was ANDed with a zero vector ("no-op to keep
  intent") → TAB/LF/CR flagged → every YAML document tripped the gate.
- BOTH: the C0 bound was `< 0x1F` (0x1F itself slipped through both) — a
  raw 0x1F in otherwise-clean input bypassed printable validation
  entirely. Conformance hole, now closed (bound is 0x20; scalar answers
  match).

No differential spec existed for gate_scan — both kernels shipped
broken through v0.1.26. SimdText.GateScan now pins kernel == naive over
targeted gate bytes (TAB/LF/CR/0x1F/DEL/non-ASCII/embedded NUL), every
prefix length, and 200 random buffers.

The validator itself: SWAR head replaced by the 32-byte gate chunks with
the exact `step()` walk for dirty chunks (resuming at sequence
boundaries — the old SWAR loop broke at the first non-ASCII byte for the
rest of the document).

Measured (interleaved h2h, same session as 68): block 0.63 → 0.82x,
scalar-heavy 0.78 → 0.98x, wide 0.90 → 1.23x, deep 0.91 → 1.06x,
anchor 0.45 → 0.60x, flow-json 0.74 → 0.82x, flow-single 0.83 → 0.81x.
(Ambient clock shift moved absolutes; the interleaved median is the
referee.)
