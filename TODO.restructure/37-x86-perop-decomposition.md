# 37 — The x86 per-op deficit: decompose, then fix

Status: in progress

## Evidence so far

| platform | our min | JSON min | delta |
| --- | --- | --- | --- |
| arm64 (macos) | 0.653 | 0.778 | −16% (we win) |
| x86_64 (ubuntu) | 1.003 | 0.797 | +26% (we lose) |

GC is no longer the story (min-of-200 excludes GC windows). The
deficit is steady-state per-op cost ON X86 ONLY.

## Suspects (never isolated)

1. **Token cache cost asymmetry**: interning keys/short values pays
   FNV + probe + memcmp per token (~15k lookups/parse) to save an
   allocation per HIT and (keys only) the per-aset hash computation
   (shared frozen strings memoize their hash; fresh strings pay it).
   Net sign may differ by arch/allocator.
2. **Cross-DSO kernel calls**: every token crosses the
   libyeptris.dylib boundary (~17k calls/parse). The earlier
   "inline copies = noise" experiment was run on arm64 AND left the
   string kernel (the majority of calls) un-inlined — inconclusive
   for x86.
3. **Struct/memset costs**: the 24KB cache memset per parse (small,
   but measurable at 1ms scale).

## The experiment (CI referee)

1. `Native.scan_time(json, n)` — the pure grammar walk (null-vtable
   `yeptris_visit_json`): decomposes scan vs materialize.
2. Cache knob: `Native.cache_mode=` / `YEPTRIS_NATIVE_CACHE` =
   `:on` | `:off` (off = fresh strings everywhere; keys pay per-aset
   hashing like JSON.parse).
3. CI matrix rows for cache on/off; profile prints scan-only time.

## Acceptance

- The decomposition table identifies the x86 regressor with data.
- Fix lands behind that evidence (item 38), gate margins re-read.

## Outcome (the referee's decomposition)

| knobs | scan-only | mean ratio | h2h |
| --- | --- | --- | --- |
| ubuntu disable/bulk/cache=on (GATE) | 0.356 ms | **0.745x** | **200/200** |
| macos none/bulk/cache=on (GATE) | 0.395 ms | 0.759x | 169/200 |
| ubuntu cache=off | 0.351 ms | 1.311x | 98/200 |
| macos cache=off | 0.534 ms | 1.509x | 16/200 |
| ubuntu aset | 0.456 ms | 1.072x | 100/200 |

1. Suspect 1 RULED OUT: the token cache is a large NET WIN on BOTH
   platforms (off regresses ubuntu 0.99x->1.31x, mac 0.76x->1.51x).
2. Suspect 2 RULED OUT: scan-only is 0.35-0.40 ms INCLUDING every
   cross-DSO kernel call - comfortably faster than JSON.parse's whole
   call. The calls are not the gap.
3. The deficit was the cached path's per-token byte-wise FNV loop
   (~15k tokens/parse). Fixed in item 38: the leptris nametab
   8-byte-prefix hash. ubuntu GATE went 0.96-0.99x -> **0.745x,
   200/200 head-to-head**.
