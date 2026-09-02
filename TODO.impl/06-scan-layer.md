# TODO.impl/06 — Scan layer: line table, indentation, indicators, spans

Status: active (v1 shipped) · Depends: 04, 05 · Layer: `src/yeptris/scan` · PLAN.md phase: 1

## Goal

Turn bytes into **structure facts** — and nothing else. The scan layer owns
line geometry and span location; the engine (07) owns grammar. This is the
YAML analogue of leptris's SIMD structural scanning, adapted from tag-based
to indentation-based structure.

## Deliverables

- `scan/line_scan.{h,c}` — streaming line scanner over a bounded window:
  for each line produce `YepLineInfo { uint32_t offset; uint16_t indent;
  uint16_t flags; }` with flags: BLANK, COMMENT_ONLY, DOC_MARKER, DIRECTIVE,
  SEQ_ENTRY, HAS_ANCHOR/TAG/ALIAS, FLOW_HINT (first char is `[`/`{`),
  TAB_INDENT_ERROR. Indent = offset of first non-space (`find_not`);
  a tab before content sets the error flag with exact position (cheap for
  us, expensive in libyaml).
- `scan/plain_scan.{h,c}` — plain-scalar span location in block context via
  `stopset_find` (`: ` / ` #` / line-break / flow stop set per context
  mode); returns whether the span terminated at a colon (→ engine decides
  simple-key without rescanning — **no token queue, no lookahead buffer**,
  the libyaml token machinery is replaced by span facts).
- `scan/quoted_scan.{h,c}` — `quote_scan` wrapper producing (span,
  has_escape, terminated); unterminated → error with line/col.
- `scan/block_scalar_scan.{h,c}` — `|`/`>` leader parse (indent digit,
  chomping `+`/`-`) and content span enumeration per line (indent >
  parent-indent rule, blank-line handling).
- `scan/sizing.{h,c}` — fused `copy_count3`-based counters (`\n`, `:`,
  quotes, anchors) fed to `yeptris_arena_reserve` (03): the single-pass
  sizing story.
- Window contract for streaming: the scanner holds `[line_start,
  line_end + lookahead)`; chunked feed carries partial-line state; a tab or
  colon arriving in the *next* chunk must still classify the *current* line
  correctly (decision held until line end or first non-space — tested with
  adversarial chunk splits in item 19).

## Design decisions

- MECE law: scan emits facts (`YepLineInfo`, spans, counters). It never
  decides node types, never allocates (beyond its window), never reports
  grammar errors — only geometry errors (tab-in-indent, unterminated quote).
- Simple-key length limit (1024 chars per spec) is enforced here as a span
  fact, so the engine gets `KEY_TOO_LONG` for free.
- Line facts are produced lazily (one at a time or small batches) — bounded
  memory in streaming mode; no whole-document table required.

## Phases

A. `line_scan` + golden line tables over the yaml-test-suite inputs
   (offset/indent/flags compared against generated fixtures).
B. `plain_scan`/`quoted_scan`/`block_scalar_scan` + span goldens.
C. `sizing` fused counters wired to arena reserve; allocation-count gate.

## Acceptance

- Golden corpus green (includes every tab/error case from the test suite).
- Allocation gate: 0 allocations per line scanned after window init.
- Ledger: line-table throughput ≥ 2 GB/s pure SIMD path on block-style
   corpus (establishes the kernel baseline before engine work).

## References

- `~/src/leptris/leptris/src/leptris/flat/direct_parse.c` (single-pass
  structural scanning precedent + the TODO 193 two-pass-is-dead lesson).
- `~/src/external/simdjson/include/simdjson/generic/` (structural-index
  concepts), `~/src/external/libyaml/src/scanner.c` (line/indent semantics
  to match in compat mode), yaml-test-suite (fixtures).

## v1 shipped (2026-08-30)

- `scan.{h,c}`: line facts (offset/end/indent/flags incl. TAB, DOC markers,
  directives, comments), plain-span scan (block + flow stop sets, colon /
  comment terminators, trailing trim), quoted-span scan (via the
  quote_scan kernel), break-length helper (\n, \r\n, \r), key-start
  classification. All span scans ride the SIMD kernels.

## Remaining phases (next work)

1. ~~Arena-sizing fusion~~ — LANDED 2026-09-02 as yep_dom_prepare
   (dom.c): three SIMD count3 passes feed one reserve (node capacity
   from structural bytes; arena capacity only when content copies —
   borrowed-only docs allocate no arena). The formula has ONE home;
   parse_impl and the memory bench call the same seam. Peak
   heap/input: scalar-heavy 6.15x -> 4.33x, flow-json 32x -> 19.4x,
   deep-nesting 15.5x -> 10.9x (ledger). The 0-allocs-after-reserve
   ideal remains open: undershooting shapes still grow (deep-nesting
   allocs 54 -> 80 — reserve + chain when hints miss).
2. Streaming window contract: the scanner currently assumes a whole
   buffer; chunk-boundary state-carry lands with item 12's feed API.
3. Line-table throughput gate (≥2 GB/s ledger entry) once sizing is wired.
4. NEL/LS/PS line-break recognition (currently \n/\r\n/\r only).
