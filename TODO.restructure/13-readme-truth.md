# 13 — Ruby README truth pass

## Why (three real defects, one runnable-broken)
1. The dump example is ILLEGAL on Ruby 3.x: `dump("name" => x,
   tags: [...])` — a trailing symbol key makes Ruby split the
   literal into positional + keywords -> ArgumentError (verified).
2. The Roadmap lists Phases B/C/D as future — all shipped (the
   Psych namespace, .tml corpora, lockstep releases; symbol audit).
3. "The native library is vendored in the platform gems" — the
   shipped gem loads a system/YEPTRIS_LIB_PATH library; say so.

## Plan
- brace the example hash; verify every snippet RUNS
- Roadmap -> a Shipped section; correct the library-loading text

## Acceptance
- every README example executed against the current gem, verbatim

## Status: COMPLETE (merged to main)
Every README example executed verbatim against the current gem.
