# 81 — the JSON-field campaign (vs simdjson)

## Baseline (measured, 2026-09-13, M-series, order-alternating referee)

| measure | MB/s |
|---|---|
| yeptris_parse_json (json-doc corpus) | ~157 |
| simdjson DOM parse | ~990 |

**0.16x.** The json-doc corpus (one strict-JSON array of the same
object mix as flow-json) is generated in bench_matrix; simdjson is
pinned at 3839ac681a4a6b4fd09b4d7e03229b35bcd909d7 (singleheader
amalgamation, CI-wired beside ryml).

## Stage 1 (landed): the ASCII gate skips the UTF-8 pass

Gate-clean JSON is pure printable ASCII — the whole-document
`yep_utf8_validate` pass is skipped. Measured: NO movement (157 →
157): the SWAR validator was already ~4 GB/s. Negative result, kept
(it removes a pass on the largest inputs and costs one SIMD sweep).

The attempted build-then-validate fusion was killed by
json-suite-strict: the DOM builder's walker is the LENIENT YAML flow
class — it accepted 5 pinned rejects (non-string keys among them).
The strict validator must rule; recorded so the fusion is only ever
retried with a strict-mode walker.

## Stage 2 (the design): strict-mode fused walker

`yep_json_document` is a separate scalar state machine; the DOM
builder drives `yep_json_walk` (lenient). Fuse: a `strict` flag on
the walk (string-only keys, single root value, the RFC 8259 rules
json_document enforces) makes the BUILD the validator — one pass.
Then SIMD the walker's token scanning (qbc/number spans already have
kernels; the structural step loop is scalar).

## Stage 3: the materialization gap

simdjson's tape is ~4-8 B/element; our DOM is 48 B/node + views +
resolver. Even at parity per byte scanned we cannot reach 6x with
that ratio — the JSON-mode end state is a compact-tape DOCUMENT
VARIANT (the recorder's fixed-size records are the existing
precedent), with the full DOM built only on host access. Boarded
here, not started.

## Standing

The referee table ships in every bench run (`vs simdjson`); CI
artifacts now state the kernel table + CPU flags (item 80).
