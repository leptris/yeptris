# 33 — x86_64 shared-runner gap: yeptris JSON slower than the bundled ext

Status: complete

## Evidence

- Dev machine (arm64, Apple Silicon, controlled): order-alternating
  interleaved profile — yeptris min 0.637 / med 0.674 / mean 0.680 ms
  vs JSON.parse 0.670/0.966/0.910 — mean 0.75x FASTER, head-to-head
  370/400.
- GitHub ubuntu runner (2 vCPU x86_64, shared): mean 2.296 ms vs
  JSON.parse 1.548 ms — ~1.48x SLOWER (CI run 34123337860). macOS
  runner: PASS.

This matches the downstream user report ("~2x slower in
steady-state on a loaded machine").

## Hypotheses (to profile ON the failing shape — x86_64, contended)

1. GC pause/re-enable: after re-enable, the deferred collection may
   land inside the measured window; on 2 contended cores that
   collection is expensive. JSON.parse amortizes inside its own
   allocator behavior. Probe: measure with GC.stress disabled but
   counts around the re-enable boundary.
2. Build tuning: the runner compiles the extension with mkmf
   defaults (+ our -O3); Ruby's bundled JSON ext was built with the
   interpreter's full flag set. Probe: compare with -march=native /
   release flags on a matching x86 box.
3. x86_64-specific: our interner/FNV + bulk_insert path may behave
   differently vs arm64 (cache line size, hash latency). Profile
   with perf on linux-x86_64.

## Plan

1. Reproduce on a controllable x86_64 box (bare metal if possible;
   otherwise a dedicated runner with load recorded).
2. Profile (perf/rbspy-equivalent at the C level); land the fix
   behind the profile's evidence, not guesses.
3. Re-run benchmark/json_profile.rb there; ledger both platforms.

## Acceptance

- Parity or better on x86_64 shared hardware, or a ledgered,
  evidenced explanation of the platform asymmetry.
- The perf tripwire spec's CI printout becomes the standing record
  of both platforms' ratios.

## Outcome (2026-09-07, items 37-38)

ACCEPTED, with the mechanism found and fixed — not by the guessed
hypotheses but by the decomposition:

- Hypothesis 1 (GC pause/re-enable window): partially right — item
  34 proved the page-growth mechanism (+478 pages/50 iters under
  disable) and item 35 shipped per-arch GC defaults.
- Hypothesis 2 (build tuning): wrong.
- Hypothesis 3 (arch-specific cost): RIGHT in essence — the cached
  token path's byte-wise FNV loop. Item 38's 8-byte-prefix hash took
  the ubuntu gate from 0.96-0.99x to **0.745x, 200/200 head-to-
  head** — parity-or-better exceeded; we now BEAT the bundled ext by
  25% on the same shared runners that reproduced the user's report.
