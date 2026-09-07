# 35 — The GC default: per-arch, evidence-picked

Status: complete

## Decision (three CI rounds)

- aarch64/darwin: `none` — +0 heap pages, the stdlib's GC cadence;
  0.718–0.749× mean, h2h up to 91%.
- x86_64: `disable` — fresh-VM page economics; 0.911–0.961×.
- `YEPTRIS_NATIVE_GC=none` documented for LOADED x86 boxes (the
  +478-pages/50-iters growth is exactly what contention punishes).
- `:start` (in-window gc_start) measured catastrophic (8×) — dead.

Implemented in ext/yeptris_native/json_ruby.c (YEP_GC_DEFAULT by
arch; env override; Native.gc_mode introspection).
