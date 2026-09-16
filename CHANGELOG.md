# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.5.2] - 2026-09-16
### Fixed
- Empty built collections ride the key line flow (`k: []` / `k: {}`)
  — the 0.5.1 seq-indent change had sent an empty built sequence down
  the next-line path, emitting a bare `[]` at the mapping's own
  column, which is invalid YAML (pyyaml rejects it; caught by
  yeptris-py's random-document differential). Non-empty sequences
  keep the key-column indent.

## [0.5.1] - 2026-09-16
### Fixed
- libyaml/Psych dump parity (yeptris-ruby#95, PR #284): a plain-styled
  scalar that cannot ride plain but needs no escapes (digit-leading
  strings like `5014`, indicator leads) now single-quotes — libyaml's
  choose_scalar_style; double quotes stay reserved for text that
  actually requires escapes. A block sequence nested under a mapping
  key now indents at the key's column (`a:\n- 1\n- 2\n`) as libyaml
  does; mappings still indent +2. With the Ruby binding's
  explicit_doc_start wiring, `Yeptris::Psych.dump` is byte-identical
  to stdlib Psych for the relaton matrix in that issue.

## [0.5.0] - 2026-09-16
### Added
- `yeptris_emit_options.explicit_doc_start` — libyaml's
  implicit=false parity: a nonzero value puts the leading `---`
  marker on every document, with scalar roots riding the marker line
  (`--- 42`) exactly as Psych/libyaml do. Size-versioned (callers
  compiled against the older struct keep the implicit single-document
  behavior — guarded by offsetof+size, pinned by a test). Default
  unchanged. The four JSON serializer entries zero it explicitly.
### Performance
- The `on_scalar` direct sink path (TODO.restructure/79 slice one):
  `emit_now` routes scalar events straight to the DOM's node build
  when the sink provides the hook — no event-struct fill through the
  switch, no indirect dispatch. The engine still resolves tags (one
  typing SSOT); recorder/values sinks keep the unchanged event path.
  Neutral on block-heavy under load (the hot pairs already bypass
  events via `on_block_pair`); the groundwork for the loop fusion.

## [0.4.0] - 2026-09-16
### Changed
- **BREAKING — tape v2 records** (TODO.restructure/85, the verdict
  six/seven campaign): `yeptris_json_tape` records are THREE columns
  (kinds/offs/lens — 9 bytes per record, was 20; the vals column is
  gone). `YEP_T_TRUE`/`YEP_T_FALSE` replace `YEP_T_BOOL` (the kind IS
  the value); container links ride the offs column (OPEN.off = its
  CLOSE index, CLOSE.off = its OPEN); numbers record spans at parse
  (the lean `yep_json_number_shape` validates grammar without
  magnitude work) and convert at materialize via the new
  `yeptris_tape_convert` (bulk, parallel-to-record int64/double
  arrays; `int_min` is discovered then; the tape retains `_src` so
  the call is self-contained). Bindings update in lockstep
  (yeptris-ruby 0.4.0.1).
### Added
- The flow-kernel chunk classifier as a kernels-table slot
  (`json_chunk`): quote/backslash/structural/value-start/C0 masks per
  32-byte chunk — scalar reference, NEON + AVX2 twins, one dispatch
  point, differential-pinned. Stage 1 alone: 598 -> 1294 MB/s.
- `yep_json_number_shape`: the number grammar walk without conversion
  (the lazy tape's arm; same accepts, rejects, and advance as the
  full scan).
- The structural indexer `json_stage1` (the simdjson token-contract
  front, gated research route): 64-byte blocks, escape-scanner borrow
  trick + sequential prefix-xor in u64 chains, operators + string
  opens + scalar-run starts as u32 positions. NEON + AVX2 twins,
  differential-pinned. The two-stage route it feeds stays gated
  (verdict seven: the fused walk remains the optimum for the
  strict-validation contract).
### Performance
- The JSON tape entry: 510 -> 546 MB/s on json-doc (packed literal
  compares, punctuation inlining in the fused walk — commas/colons
  consumed by the value arms; ~400k loop iterations gone). PERF-LEDGER
  verdicts five through seven bound the exploration: simdjson DOM
  measured directly at 1017-1039 MB/s on this corpus.

## [0.3.0] - 2026-09-15
### Added
- CBOR (RFC 8949) decode over the shared DOM (TODO.cbor/01):
  `yeptris_cbor_decode` builds the same tree `yeptris_parse_json`
  builds — zero-copy string spans, tag chains on the node's tag field,
  one data item per call, byte-offset errors. Definite + indefinite
  lengths, half/single/double floats (the RFC's Appendix D decoder,
  libm-free), bignums kept as tag+bytes, strict mode (minimal length
  arguments, text-string map keys). RFC 8949 Appendix A pins every
  vector; Appendix F's non-well-formed corpus pins the rejects; a
  300k-iteration ASAN random-bytes fuzz smoke is clean. Compiles only
  with YEPTRIS_WITH_CBOR (default ON).
- CBOR encode over the shared DOM (TODO.cbor/02):
  `yeptris_cbor_encode` / `_into` — the item-13 discipline (one
  exact-sizing walk, one allocation, linear writes), minimal length
  arguments everywhere, preferred floats (smallest of half/single/
  double that round-trips; NaN canonicalizes to f9 7e00;
  round-to-nearest-even double->half in-tree, libm-free), and
  `YEPTRIS_CBOR_CANONICAL` — RFC 8949 s4.2.1 core deterministic
  profile with map keys sorted bytewise on their encoded forms. The
  02 stability contract holds (repeated encodes byte-identical,
  200k-iteration ASAN roundtrip fuzz clean); Appendix A re-encodes
  byte-exact except the ledgered divergences, each pinned:
  beyond-int64 integers -> preferred floats, indefinite -> definite,
  multi-width NaN/Infinity -> shortest, non-text scalars as map keys
  -> text diagnostics (undefined/simple(N) recognized back — the
  diagnostic mapping is two-way), ASCII-valid byte strings -> text.
  Invalid-UTF-8 tag-str scalars re-encode as byte strings (mt2), so
  binary data round-trips byte-exact. Aliases and non-decimal tag
  text encode to YEPTRIS_ERROR_UNSUPPORTED.
- CBOR Sequences (RFC 8742, TODO.cbor/03):
  `yeptris_cbor_decode_sequence` iterates concatenated top-level items
  — one document per item, callback-owned, nonzero aborts; the error
  channel carries the failing item's index and byte offset; an empty
  sequence is valid. `yeptris_cbor_encode_sequence`/`_into` concatenate
  per-item encodings under the same sizing contract. Gates: item-by-item
  tree identity vs standalone decodes, truncation-index errors, the
  boundary-split differential at every cut, and a 20k-run ASAN random
  split fuzz — 0 mismatches.
- CBOR conformance + differentials + fuzz (TODO.cbor/06): the codec's
  verification matrix. fuzz_cbor joins the item-19 harness (invariant
  traps, seed corpus of the RFC 8949 Appendix A + F vectors — checked
  in via scripts/gen-cbor-seeds.py — libFuzzer build + the nightly
  job). Differential vs Python cbor2 (209 shared cases: 177 canonical
  byte-exact, 18 both-reject, 14 divergences ALL within seven
  documented waiver classes — bytes-as-text, int-width, tag-semantics,
  non-text-keys, duplicate-keys, trailing-bytes, depth-cap; zero
  unclassified) and an informational pass against the Ruby cbor gem
  (which additionally accepts two RFC-invalid inputs the ledger
  records). The cbor_canon driver backs both scripts.
- CBOR benchmarks (TODO.cbor/07): the matrix gains a CBOR-vs-JSON tier
  on the same DOM (decode/encode ratios, encoded size, MB/s; rides the
  item-18 artifact). First artifacts: size 0.48-0.52x JSON (target
  met); decode 0.92-0.95x and encode 0.64-1.01x against the 2x/1.5x
  targets — the ledger records the measured reasons (numbers-as-text
  DOM conversion; the O(n^2) canonical-map walks were found BY the
  tier and fixed: encode was 0.09x before the linearization).

### Changed
- scan_stats (NEON) accumulates vertically — pairwise add-accumulate
  into u16 lanes with one horizontal reduce per 4095-chunk batch,
  replacing ten per-chunk UADDV reductions (the leptris count-char
  lesson). The first cut wrapped: vaddvq_u16 returns uint16_t, so a
  4096-chunk dense batch sums to 65536 and reduces to zero — the
  differential's new 64KB/128KB boundary battery caught it. Kernel
  A/B 1.29x on every shape (3.5 → 4.5 GB/s).

## [0.2.7] - 2026-09-14
### Fixed
- The JSON writer's escape table was off by one: `\b` sat at index
  7 but 0x08 is index 8, so backspace emitted `\u0008` instead of
  `\b` (the serialbench byte-parity report). Float exponents now
  pad to two digits (1.0e-07 — Float#to_s/JSON.generate shape);
  canonical output stays a fixed point.

## [0.2.6]## [0.2.6] - 2026-09-14
### Changed
- simdjson's eight-digits SWAR (their parse_eight_digits_unrolled,
  verbatim constants) rides yep_json_number_scan's integer loop:
  eight branch-free ops per 8 digits, the overflow-promotion contract
  intact. Long-number JSON (12-18 digit ids/ns) 2.2x on the tape;
  short-number corpora unchanged (the window never fires).

## [0.2.5]## [0.2.5] - 2026-09-14
### Changed
- TODO.restructure/86 wave 3 (simdjson front): the structural index
  measured DEAD for the tape in both materializations (u32 offset
  array; simdjson's bitmask shape) — gap validation IS the walker's
  fused ws-skip relocated, so the index removes no scan work and
  adds a pass. The verdict is ledgered; the honest levers left are
  the record append and the number kernel. What survived the build:
- Fixed (fuzz-found): yep_json_document and the strict walker
  ACCEPTED a container at map-key position, building inconsistent
  trees — all three grammar machines now reject it (json-suite-strict
  pins unchanged 283/283). tape-diff gains a permanent 2M-case
  status+content parity fuzz; JsonTape pins the gap classes and
  tabs-as-ws parity.
- Milestone (the ledger's standing table): ryml fully beaten — all
  eight shapes over parity on the CI referee (anchor-heavy 0.90x ->
  1.23x across the 0.2.x line).

## [0.2.4]## [0.2.4] - 2026-09-14
### Changed
- TODO.restructure (the all-platforms round): MSVC is a first-class
  build — windows-latest joins the test matrix (302/302 there), the
  ISA selection follows CMAKE_OSX_ARCHITECTURES (x86_64 cross-builds
  on arm Macs link the AVX2 TU, not NEON), the u128 float printer
  rides one portable helper surface, the pool/mapindex mutexes share
  common/mutex.h (SRWLOCK on Windows), the corpus walkers get a
  dirent compat shim, and every checkout/clone pins eol=lf (Windows
  autocrlf corrupted the corpora). The Python wheels matrix grows
  linux-aarch64 and macOS x86_64 (cross) — see yeptris-py.

## [0.2.3] - 2026-09-13
### Changed
- TODO.restructure/84 follow-up (the ledger's 2026-09-14 wave): the
  line-facts short-line gate is ONE capped SWAR walk (8 bytes per
  step, one data-dependent exit) replacing the 32-byte probe plus
  scalar rescan — short lines were scanned twice. anchor-heavy +7%,
  block-heavy +12%, wide-mapping +15% on the same machine. The SWAR
  equality test is the carry-free form (the classic subtract-based
  haszero false-flags '!' after a space run — pinned by the corpus
  and the extended LineFacts alphabet).

## [0.2.1] - 2026-09-13
### Changed
- TODO.restructure/84: the line-facts vector gate tests the LINE, not
  the remaining buffer — a 32-byte break probe routes short lines to
  the scalar fused walk (76's gate was true for every line but the
  document last; the vector sweep was anchor-heavy's hottest entry).

## [0.2.0] - 2026-09-13
### Fixed
- TODO.restructure/82: x86 kernel dispatch via raw cpuid —
  __builtin_cpu_supports returned 0 for AVX/AVX2 on the CI Xeons, so
  every ubuntu benchmark had silently run the SCALAR table. With the
  kernels live, the CI referee (first honest table): flow-json 1.34x,
  json-doc 1.46x, flow-single 1.12x, deep-nesting 1.10x,
  scalar-heavy 1.03x, wide-mapping 1.01x vs ryml (block 0.98x,
  anchor 0.91x). Also fixes the latent AVX2 stopset signedness bug
  the live differential caught the same minute (a group's 8th member
  set lane-mask 0x80, which signed cmpgt_epi8 read as no-hit).

## [0.2.2] - 2026-09-13
### Added
- TODO.restructure/85: the JSON tape — `yeptris_parse_json_tape`
  drives the strict fused walk into compact parallel columns (kinds,
  offsets, lengths, values carved from one block; strings stay
  zero-copy spans, numbers convert inline, container records link
  their matching index for O(1) skipping). parse_json_tape 301 MB/s
  vs parse_json 207 MB/s on json-doc (0.30x vs simdjson, from
  0.20x) — the DOM build eliminated, the walk is the floor. Gates:
  a tape-vs-DOM differential (`tape-diff`, 340 cases) pins record
  stream == tree for every JSONTestSuite document plus status
  parity with parse_json; 13 JsonTape unit specs pin the record
  contract.
- TODO.restructure/83 (issue #238): the schema-descriptor
  materialization API — `yeptris_schema_load` walks a
  caller-compiled descriptor (flat 24-byte plan nodes; kinds
  SCALAR/SEQUENCE/MAPPING/CALLBACK; shared child slices give
  two-names-one-slot merges) and fills typed columns; the
  intermediate generic document never exists. Whole-document
  results in caller-allocated buffers; `YEPTRIS_DESC_ABI` gates in
  lockstep; new status `YEPTRIS_ERROR_SCHEMA` names a missing
  REQUIRED node. docs/schema-abi.md is the co-design doc for the
  committed consumer.

## [0.1.32] - 2026-09-13
### Changed
- TODO.restructure/81 stage 2: the strict fused JSON walk — the walker
  enforces RFC 8259 keys through a strict bit (the lenient flow class
  keeps unquoted YAML keys); yeptris_parse_json's clean route is one
  SIMD gate pass plus ONE strict walk that validates and builds.
  parse_json 157 -> 203 MB/s (+29 percent), 0.20x vs simdjson.

## [0.1.31] - 2026-09-13
### Changed
- TODO.restructure/81: the JSON-field campaign opens — simdjson
  (pinned tree) joins the bench references with the json-doc corpus
  and an order-alternating referee; the clean gate skips the
  whole-document UTF-8 pass in yeptris_parse_json (measured: no
  movement, kept as pass-elimination). The build-then-validate
  fusion was rejected by json-suite-strict (the DOM builder's walker
  is the lenient flow class) — the strict validator rules first.
- TODO.restructure/80: bench artifacts state the active kernel table
  and CPU vector flags; the head-to-head tables ride the markdown.
- TODO.restructure/79: the fused block engine (the last >10 percent
  structural lever) is specced with scope estimate.

## [0.1.30] - 2026-09-12
### Changed
- TODO.restructure/78: the dash-item batch — `on_block_item` mirrors
  the pair contract for classified `- value` lines (block-pair-diff
  extended with dash corpora; the fast-path surface is now
  pair/open/item).
- TODO.restructure/77: key-typing deferral closed BLOCKED by the
  binding audit — the Ruby materializer reads key tag_id for `<<`
  merge detection and typed keys (Psych parity). Consumers recorded.

## [0.1.29] - 2026-09-12
### Changed
- TODO.restructure/76: the line-facts architecture — `yep_line_facts`
  (end/indent/plain-stop) computed once per line (scalar for short
  spans, NEON/AVX2 kernels gated at >=64B); line info and the line
  shape both derive from it, and the shape's key span comes from the
  stop byte instead of re-walking the span. anchor-heavy +13 percent
  absolute (99-103 -> 116 MB/s same-machine); the vector sweep below
  64B measured 2x SLOWER than the tight scalar loops and is gated —
  the dead end is recorded in the perf ledger.

## [0.1.28] - 2026-09-12
### Changed
- TODO.restructure/73: alias names borrow in both paths (the event
  path copied every alias name into the DOM string arena — 266k
  copies per anchor-heavy parse); node init is one 48-byte template
  copy.
- TODO.restructure/74: `stopset_find` / `find_not` walk 32 bytes per
  NEON iteration (16-byte epilogue keeps every prefix exact).
- TODO.restructure/75: the engine's unwind loop hoists the dash-blank
  test and loads the top frame once per iteration.

## [0.1.27] - 2026-09-12
### Changed
- TODO.restructure/68: SIMD `stopset_find` — the stop class becomes a
  precompiled `yep_stopset` (bitmap truth + nibble-class tables),
  with NEON `tbl` and AVX2 `pshufb` kernels; the scalar bitmap walk
  was 9-22 percent of every losing shape (kernel 2156 -> 621 samples
  on scalar-heavy).
- TODO.restructure/69: BOTH `gate_scan` vector masks were broken since
  the kernel landed — AVX2 OR'd `~allow` in (the gate tripped on every
  input, running the whole-document SWAR validator on every x86 parse
  — the ubuntu scalar-heavy asymmetry), NEON ANDed the allow mask with
  zero (every TAB/LF/CR tripped it), and both missed byte 0x1F (a raw
  0x1F bypassed printable validation — closed). `SimdText.GateScan`
  now pins kernel == naive. The validator rides 32-byte gate chunks
  with the exact per-byte walk only for dirty chunks.
- TODO.restructure/70: `yep_dnode` 56 -> 48 bytes — node line/col was
  written-never-read; the fused flow walker drops per-token line
  bookkeeping. The event stream remains the position SSOT.
- TODO.restructure/71: core12 single-branch shape (word leads return
  without the walk: 649 -> 249 samples on anchor-heavy), the engine's
  repeat-alias memo (merge-key YAML), and anchor-table first-alloc
  sizing (no 18-step growth cascade per anchor-heavy parse).
- TODO.restructure/72: e_node audit — it is the nested-map opener, not
  a fast-arm miss; the engine-loop rework is deferred with numbers.
- The interleaved h2h referee alternates order per round (yeptris-first
  handed ryml the warmed core; ±0.2 of swing was that bias).

## [0.1.26] - 2026-09-12
### Changed
- TODO.restructure/67: the resolver first-byte gate (a lead byte
  that can begin no core word is a string in one compare — the
  reject chain was 6.5 percent of deep-nesting on the ubuntu
  profile) and the last scan_stats routes (parse_json, values) ride
  heuristic sizing like the main path. Local head-to-head:
  flow-single 1.08x, flow-json 1.47x, deep-nesting 1.01x.

## [0.1.25] - 2026-09-12
### Changed
- TODO.restructure/66: the stats pre-pass is dead — the ubuntu perf
  profile named the whole-buffer multi-class sweep (~12-13 percent
  on the asymmetry shapes). The encoding gate rides a dedicated
  gate_scan kernel; DOM sizing rides length heuristics; the nametab
  reserve keeps a memchr-chain amp count. The 18B allocation table
  is unchanged. Local head-to-head: deep-nesting 1.00x,
  flow-single 0.96x, flow-json 1.37x.

## [0.1.24] - 2026-09-11
### Changed
- TODO.restructure/64-2a: the 56-byte node — kind/style/flow/
  implicit pack into one bitfield byte and the mutation-only
  attached/depth fields leave the record for lazily-grown side
  tables (parse writes five fewer bytes per node, two fewer stores
  per link). Local head-to-head: flow-json 1.46x, anchor-heavy
  1.12x. The node-size gate is 56B.

## [0.1.23] - 2026-09-11
### Changed
- TODO.restructure/63 phase 1: the key-event defer — classified
  lines stop building-and-discarding the key event (48 stores per
  line); it constructs only on emitting paths, from the pre-fold
  line fact. Local head-to-head: block-heavy 1.03x, flow-json 1.25x.

## [0.1.22] - 2026-09-10
### Changed
- The resolver number hook: spans the walker already validated as
  strict numbers skip resolve()'s digit re-walk (the walker reports
  is_float as a fact; the schema decides the tag — the typing SSOT
  holds). Local head-to-head: flow-json 2.04x, flow-single 1.00x,
  deep-nesting 1.03x vs rapidyaml.

## [0.1.21] - 2026-09-10
### Changed
- TODO.restructure/58: line-fact memo seeding (block lines were
  double-scanned) and short-span scalar walks (the SIMD stopset
  dispatch cost more than the 2-8 byte key scans it served; scalar
  walk below the same 64-byte gate scan_line uses). deep-nesting
  0.59 -> ~1.0x, flow-single ~0.99x, block-heavy ~0.98x on local
  head-to-head medians. Unsigned stopset indexing in the short-span
  walk (multibyte UTF-8 shifted signed-char negative — found by CI
  sanitizers).

## [0.1.20] - 2026-09-10
### Fixed
- The zero-copy borrow (TODO.restructure/56): dom->input_base is now
  set BEFORE the engine run — input-slice scalars, keys, and anchor
  names are views instead of arena copies. (The document already kept
  the transcoded buffer alive for this; the ordering contradicted the
  design.)

### Added
- TODO.restructure/57: on_block_open — a `key:` line opening a fresh
  mapping builds the map and key nodes in one sink call; the engine
  pushes its frame silently. block-pair-diff extended.

### Changed
- flow-json 1.05-1.69x, block-heavy/anchor-heavy/wide-mapping cross
  1.0x on CI head-to-head medians; deep-nesting, scalar-heavy and
  flow-single remain the active campaign (TODO.prompt.md wave 3).

## [0.1.19] - 2026-09-10
### Added
- TODO.restructure/53: fused validate+build — one walk per
  JSON-class flow span. The sink contract is a trio (build/commit/
  rollback); the DOM stages scratch nodes past ncount, pairs them on
  its live stack, and commits the root only after the engine's
  grammar checks pass — every fallback stays byte-identical. The
  grammar walker is scan/json.c's SSOT; the engine's runtime
  max-depth threads through.

- The bounded-parse law: every DOM growth path is capped by input
  size (nodes ≤ len + 1024, docs/anchors sparser, arena 128×) — a
  parse over N bytes allocates O(N) whatever the bug, failing
  YEPTR_MEMORY instead of ballooning; the quadratic %TAG-expansion
  DoS is bounded the same way. MemGuard specs pin the caps.

### Changed
- flow-json runs 1.11x (mac) / 1.15x (ubuntu) of rapidyaml on the CI
  head-to-head medians — the first ryml-comparable shape fully won.
  Sink literals are designated initializers throughout.

## [0.1.18] - 2026-09-10
### Added
- TODO.restructure/49: the one-pass line classifier — scan emits
  per-line shape facts once; the engine's fast arms dispatch a line
  in one decision (plain/alias/anchor/flow values, dash entries),
  strict bail on every deviation. 21 new specs pin the classes.

- TODO.restructure/50: the flow sink fast path — `yep_sink` gains an
  optional `on_flow_json`; the DOM builds engine-validated JSON-class
  spans directly through its own placement laws (the parse_json
  jbuilder unified onto them). PERMANENT flow-direct-diff gate.

- TODO.restructure/54: the block pair fast path — the classified
  `key: value` line is offered whole to the sink (`on_block_pair`);
  the DOM builds both nodes with one resolver decision per scalar.
  PERMANENT block-pair-diff gate (tree comparator shared:
  test/flow/tree_diff.h).

- The benchmark matrix carries an interleaved head-to-head table vs
  rapidyaml (median of per-round ratios) — the campaign referee
  (separate-phase best-of rides phase bias; runners are bimodal).

### Fixed
- Key-anchored scalars (`&a: key`) now bind their anchor ordinal in
  the DOM — their aliases previously resolved to an unwritten slot
  (node 0 or heap garbage; found by flow-direct-diff on 2SXE/E76Z).

### Changed
- flow-json (block-of-flow) runs 0.50x -> ~0.95x of rapidyaml on
  the CI head-to-head medians; wide-mapping and flow-single moved up
  (0.67->0.74x, 0.51->0.80x mac). Full parity remains on the board
  (TODO.restructure/53, 55).

## [0.1.17] - 2026-09-09
### Changed
- TODO.restructure/45 Phase-B slice one: single-line flow spans
  (the dominant `key: {…}` / `- {…}` shapes) skip the flow_enforce
  rescan and the per-event line bookkeeping in the engine's flow
  kernel — one memchr decides. Fresh-runner CI bench now carries
  the rapidyaml columns: block-heavy runs 1.47x FASTER than ryml.

- The escape grammar is ONE authority: `yep_json_string`'s escape
  validation extracted to a single validator (TODO.restructure/47's
  survivor; the structural-index descent itself was measured dead —
  every benchmark shape regressed — and reverted, ledgered in full).

## [0.1.16] - 2026-09-08
### Added
- TODO.restructure/46: `qbc_find` — a one-pass SIMD string-stop
  kernel (first quote, backslash, or C0 control; AVX2/NEON/scalar
  TUs + dispatch) now backs `yep_json_string`. Hardened
  differential + C0-deep-in-vector-path rejection tests; measured
  +2% on 55-byte-string JSON cells, flat elsewhere (the ledger
  records the host-materialization verdict).

## [0.1.15] - 2026-09-08
### Added
- TODO.restructure/43 (canon parity): Psych's sexagesimal weights —
  the fold is now weight-based (component e weighs 60^|e-2|, sign on
  the first component): 2-component values are H:M (`1:30` = 5400,
  not 90; `-1:30` = -1800; `1:30.5` = 5430.0); 3-component unchanged.
- Beyond-int64 integers marshal as Integer (Psych parity): the
  emitter rebuilds Bignum limbs from the decimal text under Psych's
  integer shape (no leading zeros, `_`/`,` separators).

### Changed
- yeptris_node_int/float delegate to the yep_num_* kernels — the
  typed accessors carried a THIRD copy of the number conversion
  (clean + bases + inf/nan + sexagesimal); numbers.c is the one
  fold (MECE), ~150 duplicated lines deleted.

- TODO.restructure/41 (Python binding): yeptris.json.loads — the
  native JSON loader over the exported scan kernels beats json.loads
  (CI-gated 1.00: ubuntu 0.785x mean 198/200, macos 0.699x 181/200;
  was ~15x behind). The ledger records the intern-tax and
  NaN-parity corrections; the next lever (kernel TUs compiled into
  the extension) is scoped for item 42's wave.

- Release integrity (TODO.restructure/40): `scripts/smoke-gem.sh` —
  every gem (ruby + platform) is installed into an isolated GEM_HOME
  and must pass the binding artifact battery BEFORE `gem push`; the
  binding CI checks out the C core at the newest RELEASE TAG (the
  artifact users get); branch protection on both repos makes red
  merges structurally impossible.

## [0.1.14] - 2026-09-08
### Fixed
- core_12 marshal typing (TODO.restructure/32 completion): the parse's
  schema is now a document property (`yeptris_document.schema`, set by
  both parse paths — strict JSON is core by construction), and the
  Marshal emitter threads it: Psych's dot-required float quirk applies
  ONLY under `YEPTRIS_SCHEMA_11_COMPAT`. Under core, `1e3` marshals as
  `{"k"=>1000.0}` (spec 10.3.2: the core float regexp has an OPTIONAL
  dot); under compat it stays `{"k"=>"1e3"}`, byte-identical to Psych.
  Regression test: `Marshal.SchemaConditionedFloatTyping` (both the
  direct `yeptris_marshal` and `yeptris_marshal_node` paths).
- The Ruby binding's core12 spec suite ran green only against a
  working-tree C build; the shipped 0.1.13.x gems vendor C v0.1.13,
  which lacked this fix — the native YAML.load path returned the
  compat typing under core_12. Fixed by this change; the next lockstep
  gem release carries it.

## [0.1.13] - 2026-09-07
### Added
- `yep_json_number_scan` (scan/json.c): the JSON number grammar walk
  FUSED with conversion — one scan validates and converts (integer
  fast-path into int64, INT64_MIN exact; `*is_float` reports the text
  shape: 0=int, 1=float, 2=integer-beyond-int64 with an approximate
  `*dv`). `yep_json_number` is now a thin wrapper. The Ruby native
  materializer's number path is one call — no more validate-then-
  reconvert double walk.
- `scripts/build-native-asset.sh` + release.yml: every GitHub Release
  carries a prebuilt native-materializer tarball for the publish
  runner's platform (build recipe versioned in scripts/, never inline
  workflow YAML).

## [0.1.12] - 2026-09-07
### Added
- The visit API (`yeptris/visit.h`): `yeptris_visit`,
  `yeptris_visit_json` (fused RFC 8259 scan — no DOM, no records),
  `yeptris_visit_node`; the JSON scan kernels and number converters
  are exported for host materializers. The Ruby native materializer
  riding this beats JSON.parse (mean 0.72x on the 152 KB corpus).

## [0.1.11] - 2026-09-07
### Added
- `yeptris_marshal`/`yeptris_marshal_node`/`yeptris_marshal_free`
  (TODO.restructure/21): the C side converts value records into Ruby
  Marshal 4.8 bytes; the binding materializes the whole object graph
  with one `Marshal.load` call. ~10× faster than the columnar walk on
  JSON-shaped input and ~5× on YAML, ~50× on the per-node DOM walk
  (`Node#to_ruby` becomes bulk). Alias identity preserved through `@`
  links; merge keys and timestamps return `ERROR_UNSUPPORTED` for the
  record-walk fallback.

### Changed
- The value-drain entry (`yep_values_from_input`) sniffs strict-JSON
  (`{`/`[` as the first non-space byte) and routes through the JSON
  scanner + DOM linearizer on the same path the YAML engine takes;
  any grammar surprise defers to the engine. Records stay byte-
  identical across routes (the Ruby binding's 2.7k-corpus
  differential pins the equivalence).

### Fixed
- DOM `lin_node` linearizer: pending anchors decorate the value that
  *follows* them, even inside a container; nested anchor bindings no
  longer clobber the outer pending index (ASAN caught a stale-index
  write on `&a [&b x]`).
- Marshal `anchor_find`: YAML lets a later `&anchor` shadow an earlier
  one of the same name; lookup scans newest-first
  (libyaml snapshot 3GZX).

## [0.1.10] - 2026-09-06
### Fixed
- MECE: the `\n`/`\r` break stop set has ONE home (scan.c) — the
  quoted-scalar path rebuilt a bit-for-bit duplicate per quote.

## [0.1.9] - 2026-09-06
### Performance
- The JSON string stop set becomes a constant (34 bitmap bits were
  cleared and set per string scan); strict-JSON strings at 479 MB/s.
- The plain-scalar stop sets become constants (clear + 4-9 adds per
  scan, twice per line). scalar-heavy parse reaches 3.36x libyaml —
  the fastest engine yet. Both bitmaps pinned against runtime builds
  (a hand-written drift ends every plain scalar early).

## [0.1.8] - 2026-09-06
### Performance
- Plain-value spans scan once: e_plain_multiline takes the caller's
  span (every plain value was scanned twice — colon decision, then
  the fold). All shapes at their bests (scalar 3.23x).

## [0.1.7] - 2026-09-05
### Fixed
- `stopset_find`'s differential test hardening (rare-byte sets let
  broken vector paths pass); the nibble-method SIMD attempt was
  built, measured a net loss, reverted — analysis ledgered.

## [0.1.6] - 2026-09-05
### Fixed
- UBSan-caught UB: NULL+0 pointer arithmetic in the zero-entry
  columnar drain and the scan_stats tails (both guarded).

## [0.1.5] - 2026-09-05
### Performance
- Prefix-slot interner: 16-byte slots carry the key's first-8-byte
  prefix + length + value INLINE — one cache line per probe for
  keys <=8 bytes (nametab-GET misses were ~14% of anchor-heavy
  parse). Best absolute times on every shape at release.

## [0.1.4] - 2026-09-04
### Performance
- scan_line rides the SIMD kernels for line end + indent behind a
  64-byte span gate (below it, dispatch overhead beats the loop).
- Fused pre-scan: one SIMD pass computes the ten occurrence counts
  plus the printable/ASCII flags (four full-buffer passes before).

## [0.1.3] - 2026-09-04
### Added
- `yeptris_value_drain_columns`: the value stream as parallel typed
  buffers carved from one allocation — column i is byte-faithful to
  record i (equivalence-pinned).

## [0.1.2] - 2026-09-04
### Fixed
- The pend anchor id rides the pend view: a props-only line before
  its node ("--- &id001" / "- *id001") carried the anchor view with
  a zero id — aliases to it failed with an empty error. Found by the
  Ruby port's self-referencing-structures spec; four-case C
  regression test added.

## [0.1.1] - 2026-09-04
### Performance
- Engine pass 1: anchor ordinals end to end, interner pre-sizing
  from the '&' count, word-at-a-time hash + inline view equality,
  per-line scan memo, DOM node-hint floor, constant-size resolver
  word checks. anchor-heavy 1.27x -> 1.96x libyaml.
### Fixed
- The line-scan memo could rewind a mid-line position and spin
  forever (found by the roundtrip harness on a directive +
  inline-comment document); loop heads that legitimately see mid-line
  positions keep the scan-from-pos semantics.

## [0.1.0] - 2026-09-03

### Added

- Bootstrap scaffold (TODO.impl/01): CMake build (C11, LTO for Release,
  ASAN/TSAN options, scoped warnings), public header skeleton (`yeptris.h`
  umbrella, opaque pointer-sized handles, pinned status enum, generated
  version header), CLI with command registry (`yeptris version`), ABI
  pinning test, CI workflows (test matrix + ASAN), `validate.sh` and
  `bump-version.sh`.
