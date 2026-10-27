# 03 — C: json.c's per-call stop-set builds become constants

## Why
The same waste scan.c had: json.c builds its 32-byte stop bitmap per
scan (clear + adds). The stop sets are compile-time constants of the
JSON grammar.

## Plan
- Generate the bitmaps from the definition (set[c>>3] |= 1<<(c&7)),
  pin with a unit test against a runtime build (the scan.c lesson:
  hand-written bitmaps drift silently).

## Acceptance
- jsonsuite 95/95 accept + 188/188 reject; the full 232+ gates

## Status: COMPLETE (v0.1.9)
jsonsuite 95/95 + 188/188 inside the 232 gates; the strict-JSON
string path measures 479 MB/s on a string-heavy corpus.
