# 07 — Python: _as_bytes types its inputs (no attribute probing)

## Why
`getattr(yaml, "read", None)` is Python's respond_to?. File-likes
have a type: io.IOBase (covers file, BytesIO, StringIO via the
text wrapper).

## Plan
- isinstance arms: bytes/bytearray, str, io.IOBase (read + decode
  when text-mode); everything else raises TypeError.

## Acceptance
- 55/55; a test pins str/bytes/file/BytesIO/TypeError paths

## Status: COMPLETE (wheel 0.1.9.1)
isinstance(io.IOBase) replaces the getattr probe; 56/56 incl. the
new all-paths input test. The ctypes hasattr(_lib, ...) feature
detect is the DOCUMENTED EXCEPTION: binary capability probing of
the native library, not type duck-probing.
