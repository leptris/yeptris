# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed

- **engine: the value-position props gap crosses a raw NEL/LS/PS like a line break (#818, the nightly's r87/r88)** — with a colon-quote key (`kek:": &a<PS> ? &--02:in~`) the value plain-read instead of anchoring: `e_props` returned at the anchor name, the gap's break byte was fed to the plain reader, and `&a ? ...` became one scalar where libyaml opens the anchored map. Two line-model gaps: `e_at_eol` knew only `\n`/`\r`/comment (libyaml: NEL/LS/PS are IS_BREAK), and `e_parse_value`'s following-lines loop did not treat a break byte at its head as a line end, so the props-only line re-entered `e_node` on the break (a depth-guard loop) unless the break was pre-consumed — which then made a `---` marker the scalar's VALUE. The loop head now consumes the raw break exactly as its BLANK branch consumes an LF-ended empty line. Pinned by `Parse.ValuePropsGapCrossesARawMultibyteBreak`: the r74 marker twins, the r87 staircase row and its artifact, and the r88 two-continuation minimal each reproduce their LF twin's event stream byte-for-byte (positions included), and the staircase value anchor opens a map. The anchor-NAME charset divergence (yeptris keeps `:` in names, libyaml stops at it) is unchanged and stays carried by the comparator's charset split.

## [0.6.51] - 2026-10-30

### Fixed

- **fuzz: the charset-carry walks learn the r69 multibyte-break law (the nightly's round 87, part one)** — crash-629bf361 (`ke&&k:": &---<PS> ? &--02:in~`, 28 bytes) decomposed into TWO layers: a comparator walk-model gap (LANDED here) and an engine route split (filed as #818). The walks treated the raw PS bytes as plain content: the first anchor (`&a`/`&---`) split AT the break (a break-ended name is already complete — no truncation exists), the phantom residual opened a plain that swallowed the break, and the key-position anchor (`&--02`) never split. Both walks now end tokens at raw NEL/LS/PS sequences exactly like the engine's own `yep_scan_prop_end` has since r69/#778 (`yd_mb_break_len`): the break is a line break in the walk model, break-ended anchor names take no split, and the gate/transform scans stop there. Gates: the minimals (`k: &a<PS> ? &--02:in~` class) carry with byte-verified splits, the 405-snapshot corpus + the r66/r85/r86 seeds + the qflow family replay clean under ASan, clang-format clean. The artifact stays carried-open on #818's engine dig (yeptris's `+MAP &a` vs one plain is a structural event-count difference no split reconciles).
- **marshal: the strict-dup scan stops false-flagging arrays with repeated values (yeptris-ruby#333)** — the value stream's slot rule toggled the key parity for SEQUENCE entries too: every odd-position array element carried `is_key=1`, so the marshal route's strict-dup pre-scan answered `duplicate key in object` for plain JSON like `["a","b","a"]` — JSON.generate output cannot contain duplicate keys, and the Windows marshal lanes (no native ext, `MARSHAL_STRICT_DUP_KEYS` on) rejected valid OpenAPI documents (the yeptris#680 class resurfacing through the value stream's own parity, not through a key table). The slot rule now honors the frame kind: a sequence entry never takes a key slot and never toggles the parity (`is_seq[]` tracked at both producers — the event transform and the DOM linearizer). The marshal writer's own pairing was always seq-safe, so emitted bytes are unchanged; only the scan's input was corrupted. Pinned by `Marshal.StrictDuplicateKeysSequenceEntriesNeverFlag` (odd-position repeats pass in three nestings, same-key-different-maps passes, true map duplicates still raise).

### Added

- **#742: the bench matrix's CBOR tier tracks the decode-only SAX face** — a new `decode-only SAX MB/s (#742)` column rides the CBOR-vs-JSOn table: the same canonical corpus decoded through `yeptris_cbor_decode_sax` into a counting sink (nothing built — the scanner's own speed, the like-for-like answer to tinycbor's get_next loop), interleaved best-of beside the DOM decode row. Local quick run: 223-233 MB/s SAX vs 80-88 MB/s DOM decode on the tier's shapes (~2.7x, and past the tinycbor row's class) — the CI matrix now referees the decode-only campaign per platform.

### Fixed

- **fuzz: the charset-split fold walk carries the raw-PS-as-content rendering (the nightly's round 86)** — crash-ef7ccbc5 (31 bytes: `&k:j:(<PS>\r\rT |U\n%- |V|r\n\n - 21\n`) is the anchor-charset split composing with the plain-LS/PS fold family at a NEW rendering: in the split residual libyaml keeps the PS raw AS CONTENT before the same escaped-fold newlines yeptris prints (`(<triple>\n\n` vs `(\n\n`), so the triple is ADDITIVE to the rendering — round 72's arm assumed it REPLACES one yeptris escape. The fold walk now consumes the additive triple alone when the renders then align byte-for-byte (and round 72's replacement form when they don't), scoped by a raw-triple scan on the split copy plus the walk's existing libyaml-invariance and full-tail-consumption legs. Engine untouched: the suite stays the conformance bar; the charset truncation and fold laws are the documented divergences. Seed `parse/anchor-charset-ps-crcr-additive.bin`. Gates: the crash carries, the 405-snapshot corpus + the r66/r85 family shapes + the prior catch seeds all replay clean under ASan (pure file-arg replay — the `-runs=0` dir walk started mutating on a single-file corpus), clang-format clean.
- **fuzz: the flow-`?` comparator carve now carries the bare-`?`-at-a-boundary composition** — the nightly's round 85 (crash-2bdccbe, 14 bytes: `{\n?,\n?:M,\n?:}\n`) is the r58/r67 glued-`?:` family composing with a BARE `?` line: the existing carve's r71 spell (drop the `?`, the follower is libyaml's KEY content) turns the valid `?,\n` into a lone `,` entry that neither engine accepts, so the byte-strict verify died before the glued pairs were even read. The boundary form now spells `"":` — the same pair in the style-blind dumps (the KEY event's style never rides the YD event) and accepted by both engines — while the glued r71 spell is preserved for `?`-with-follower. The engine itself is untouched: the suite remains the conformance bar (HM87/652Z pin the plain reading of `?x`; `FlowQPlainPerSuite`/`FlowQQuotedFollowerStaysPlain`/`FlowQColonKeyStaysPlain` stand), libyaml's divergent glued reading stays carried. Seed `parse/flow-q-bare-glued-mix.bin`. Gates: the crash + r67/r71 compositions + the 8-shape `?`-family all carry or agree, the 210-file parse-corpus replay clean under ASan at the CI stack, 618/618 ctest.

### Added

- **#742: `yeptris_cbor_decode_sax` — the CBOR decode-only face goes public** — the sink vtable the DOM builder drives (`yep_cbor_sink`) was already an exported seam; the slice makes it the public SAX surface: `yeptris_cbor_sax` (ctx + seven callbacks) in `<yeptris/cbor.h>`, one call, no document, no nodes — items land in consumer callbacks as they parse, with the DOM's exact rendered text forms, `is_key` parity, tag chains, and indefinite-string concatenation. Measured justification (perf ledger 2026-10-29): the scan-only route reads 270 MB/s vs the DOM entry's 128 on the same corpus — DOM construction is over half the decode lane — so validation, counting, and streaming transforms belong here. `consumed` walks RFC 8742 sequences without a DOM; the internal seam aliases the public struct (`typedef yeptris_cbor_sax yep_cbor_sink`), so `yeptris_cbor_decode_sax` is a straight passthrough and the ABI is unchanged. Pinned by eight `CborSax` tests (callback shapes, caps, tag chain, indefinite rules, the consumed walk, sink-failure→MEMORY, the ARG contract, strict mode).

## [0.6.50] - 2026-10-29

### Fixed

- **perf(#474): the tape entry's NEON gate pre-pass moves to the reject path** — the fresh `sample` profile of a sustained `yeptris_parse_json_tape` loop (json-doc corpus, 847 MB/s) put 94% of the lane in the fused walk and 5.5% in `yep_neon_gate_scan` — the entry's full-input non-ASCII pre-pass that ran before every parse even though the fused walk already rejects tabs and non-ASCII bytes inline (the clean_only contract parse_json adopted; the tape entry never took it). The pre-pass now runs after a walk reject: accepted documents skip it entirely, and the record walk plus the encoding-owed decision read `gated` only when actually taken — every error precedence below is unchanged (the validating sequence consumes the same bit; ill-formed UTF-8 still answers ENCODING with its offset, grammar-invalid non-ASCII still answers "not strict JSON"). Local A/B max-of-rounds: 906.7 → 964.8 MB/s (**1.064x**), moving the local tape h2h vs simdjson to ~0.87x. Gates: 609/609 ctest, an old-vs-new route parity gate over 16 edge classes x strict+lenient (32 rows: statuses and tape hashes identical — the accept paths, the encoding-error contract, the scalar roots, the tab and trailing-garbage rejects), ASan clean, 0 leaks.

## [0.6.49] - 2026-10-29

### Added

- **yeptris-ruby#322: `yeptris_cbor_from_marshal` — the CBOR dump face's marshal route** — the CBOR twin of `yeptris_json_from_marshal`: Ruby Marshal 4.8 bytes IN, one CBOR data item OUT, walked entirely in C (no Document, no YAMLTree, no per-node host crossings). It reproduces the per-node route's byte law exactly: the `@`-link registry (arrays/hashes/strings/floats/bignums register; an `I` wrapper registers once, so a wrapped string is one slot; a bignum holds a slot though no link targets it), string value encoding rules (ASCII-8BIT, UTF-8, US-ASCII, named encodings), the compat-11 key resolution law, the reserved `undefined`/`simple(N)` ledger, duplicate-key refusal, bignum int64/float re-render in both directions, and the canonical sort by encoded key bytes (stable; insertion order preserved otherwise). Anything with no exact fast-path spelling answers `YEPTRIS_ERROR_UNSUPPORTED` and the binding keeps its walk. Byte parity pinned by `test/unit/test_cbor_marshal.cpp`; a 6k-value random differential sweep against the per-node route (both canonical modes) found 0 mismatches, and the binding's 5k-row `CBOR.dump` drops from 84-141 ms to 17-35 ms.

### Fixed

- **perf(#739): the JSON write lane rides its own tree walk — and the compact entry joins the one-pass route** — slice 3 of the issue's profile: in JSON mode every collection is flow and the YAML machinery (props lines, style analysis, the explicit-key law, pretty depth) never fires, yet the generic `emit_node -> emit_flow` path re-tested all of it per node and looked every child up twice. `emit_json_node`/`emit_json_flow` emit the same bytes with one dispatch per node and two per map pair (pretty output keeps the generic path — its depth bookkeeping is the point there); `yeptris_serialize_json_ex` and `yep_serialize_json_compact` drop the dry exact-sizing pass for the one growable wet pass `yeptris_serialize_json` already rides. Local A/B: compact ~3x, default ~1.2x; the serialbench matrix is the referee. One contract alignment rides along: the compact/EX entries now honor the unserializable law like `yeptris_serialize_json` (NULL) instead of returning the unspellable bytes — the anchor-charset fuzz-corpus documents flip from garbage JSON to a clean failure. Byte-identical outputs: 608/608 ctest, old-vs-new FNV parity over the bench corpora, 30 hand edge shapes (anchors/tags/aliases/complex keys/pretty/compact) and the fuzz parse corpus, ASan clean, 0 leaks.
- **perf(#742): the YAML write lane stops re-walking every key and every plain-unsafe scalar** — the fresh `sample` profile of a sustained `yeptris_serialize` loop (3.7 MB wide-mapping corpus) put 59% of the lane in re-derivation: `key_written_over_limit`'s factor-4 escape estimate walked every map key's bytes (22%), and `sc_route` ran the SWAR plain-safety walk a SECOND time on every plain-UNSAFE scalar (the case-1 arm re-tested bytes the head check had already rejected). Cuts: the estimate takes the exact bound — a prop-less key of ≤255 raw bytes can never exceed the 1024 written-byte law (2 + 4×255 = 1022), so two field checks replace the walk; the plain verdict is computed at most once per sc_route call and reused in the case-1 arm; and `yep_style_plain_safe` grows the all-boring accept — a span of pure `[0-9A-Za-z_]` makes every rule in the function vacuous (no indicators, no blanks at the ends, no BOM, no `': '`/`' #'` adjacency, no breaks, no `---`/`...` marker), so one SWAR range test per word accepts without the six-mask walk. All three are pure-perf (byte-identical outputs). Local A/B min-of-rounds: 1.28-1.30x on the same corpus (the serialbench matrix is the referee). Gates: 590/590 ctest, new `KeyLawLengthBoundaryStaysByteStable` pin (the 255/256 boundary plus a node-API >1024 key riding the explicit form), an exhaustive old-vs-new `yep_style_plain_safe` verdict diff (every string of length ≤4 over a 38-byte tricky alphabet — 2.1M cases — plus 400k random 5-32-byte strings: identical hashes), old-vs-new FNV parity across all 9 bench corpora × 6 emit routes, the 30 hand edge shapes, and the fuzz parse corpus (1018 rows), ASan clean, 0 leaks.
- **perf(#742): the CBOR decode lane's int path stops paying twice for every integer** — the `sample` profile of a sustained `yeptris_cbor_decode` loop (1.44 MB json-doc corpus) put ~26% of the lane in `dom_sink_int`: every integer was formatted into a stack buffer by the per-digit `%10`/`/10` loop, then copied through the text sink into the arena. Cuts: `cbor_u64dec` renders two digits per divide from a 200-byte pair table (halves the divides), and `dom_sink_int` formats directly into the arena via `yep_dom_str_tail`/`commit` — no stack buffer, no second copy, bytes identical. Post-cut profile: the int lane drops to ~10%; the scanner (`cbor_item`) is now the decode lane's top cost (the decode-only-entry lever noted on the issue). Byte-identical outputs: 589/589 ctest, old-vs-new FNV parity on the decoded corpus's YAML and JSON serializations plus the int-boundary forms, ASan clean, 0 leaks.
- **fuzz: documents still carrying raw NEL/LS/PS parse on the engine route** — the nightly's round 75 (crash-2b7a62e9, 32 bytes: `k: --:"a<LS> 'c<PS>d'b"` + line 2) tripped fuzz_roundtrip's parseable-must-marshal invariant. A mid-plain quote fools the quote-aware break normalizer (#638): the PS survives raw inside what it believes is a quoted region, and the packed-tape fused runner (#378) — whose plain walk predates the round-74 multibyte break cut — rode it into the value, so `yeptris_parse_ex` accepted what libyaml rejects ("while scanning a simple key") and what the engine's own generic route rejects. `yep_engine_normalize_breaks` now reports whether raw sequences survived, and `parse_impl` hands those documents to the engine route (which owns the cut); legal riders (a raw LS inside a quoted scalar, the r55 join-raw law) keep parsing. Pinned by `KeptRawUnicodeBreaksRideTheEngineRoute`. Gates: r75 + all prior rounds' min cores carry; parse + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses; ctest 587/587; 0 leaks. Seed `parse/r75-tape-raw-ps.bin`.

## [0.6.47] - 2026-10-27

### Fixed

- **#782: plain scalars end at NEL/LS/PS before a document marker** — the nightly's round 74 (`crash-39b14958`, 12 bytes: `&k:":(PS---`): a plain value rode the multibyte break straight through it and absorbed a following `---` line as content, where libyaml ends the document at the marker. `yep_scan_break_len` and `yep_scan_plain` honor the multibyte breaks (the law `yep_scan_prop_end` already applied to anchor names); `yep_scan_advance_line` counts them so line attribution keeps pace. The PS/LS/NEL twins and the plain byte-break shape are pinned by a unit test.

- **#322: the marshal reader's object registry matches Ruby's slot law** — the `@` links index a 0-based registry fed in stream order (`[`, `{`, `"`, `f`, `l` and anything `I`-wrapped register once per construct; the root included; bignums hold a slot though no link ever targets one). The pre-fix model shifted every link the moment a document repeated a key — the users-style rows (every field key an `@` link after row 1) now round-trip byte-identically to `JSON.generate`; ivar pairs parse-and-discard, the bignum payload decodes as 16-bit LE limbs, and the memo memmove hands back the writer reservation it consumed.

## [0.6.46] - 2026-10-27

### Fixed

- **fuzz: the quoted blank-strip law rides the LS/PS dump walk** — the nightly's round 73 (crash-7db98deb, 30 bytes: `k: "a<TAB><LS> 'c<PS>d'b"\nj: '<PS>d'`). Around a raw LS/PS that BOTH engines keep (the r55 quoted law), libyaml drops the blank runs on BOTH sides while yeptris keeps them — spelling its tab as the `\t` escape. The walk consumes yeptris's leading blank/escape run, requires the triple in both dumps (so the round-56 plain shape — a space FACING the triple — still falls to its own arm), then the trailing blank run. Gates: r73 + all prior rounds' min cores carry; parse + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r73-quoted-blank-strip.bin`.

- **perf(#352): the parse-time exotic-byte fact bit lands — the serialize lanes +11-14%** — the ledger's deferred lane 2: the dnode's last free bitfield bit (`no_exotic`) records at every value assignment whether the scalar is clean of the dq-forced byte set (NEL/LS/PS/BOM/noncharacters), and the serializer's `sc_route` trusts it instead of rescanning per derive. The predicate moved to the scan SSOT (`yep_scan_dq_forced`) so the emit check and the parse fact cannot drift; mutation-built scalars default to unknown (scan) — no behavior change off the parse path. Interleaved A/B on fresh binaries: json-users serialize +11%, scalar-heavy +14%, block-heavy +3%, parse lanes neutral (the ledger's 2026-10-26 entry). Byte-identical outputs: 579/579 ctest, the conformance suite, the corpus walks, 0 leaks.

- **scan: an anchor/tag name run ends at NEL/LS/PS (the nightly's round 69)** — `yep_scan_prop_end` classified name bytes through the byte-level table, which cannot see a multibyte break, so `&k:":(<PS>...` swallowed the PS INTO the anchor name (and every simple `&k<LS>x` shape clipped only by luck of a downstream re-clip — shape-dependent readings). The walk now stops at the NEL/LS/PS byte sequences per libyaml's IS_BLANKZ anchor law (the suite's ns-anchor-char excludes breaks); non-break multibyte names (the W5VH law) keep riding. Law change: `&<PS>` is now an EMPTY anchor name and the parse rejects it (libyaml: "did not find expected alphabetic or numeric character") — the round-39/45 emit tests' parse-OK + serialize-fails pins move to the parse-error law. Seed `parse/r69-prop-name-multibyte-break.bin`.

- **fuzz: the LS/PS dump walk gains the quoted-vanish arm; the `?:` rewrite generalizes to every flow-depth '?' (rounds 70/71)** — the r69 input is a composite: the anchor-charset ':' split reconciles the name reading, and the residual quoted scalar then differs by the round-55 law (libyaml drops the blank run after a raw LS/PS it kept; yeptris keeps the blanks). `yd_dump_eq_mod_lsps_fold` now carries both documented renderings in one walk (plain fold-space ↔ raw triple; raw triple + kept blanks ↔ bare triple), and `yd_qflow_question_colon_divergence` grew a BARE-'?' arm: libyaml's glued reading makes the follower the KEY with an empty value ("{\n?M-b%rM,\n?:M,\n?:}"), so the spell drops the '?' — a bare plain in a flow mapping IS that pair for both engines ("{M-b%rM}" reads (M-b%rM -> empty) on both sides; the ':'-closed form dies on libyaml's ":," node-content law). Seeds `parse/r71-qflow-bare-question.bin`; parse (201) + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses; ctest 579/579; leaks 0.

- **fuzz: the anchor-split's quote tracker respects the open plain** — the round-68 live-fuzz find (63 bytes, two `- &gboo:- 4f";\ "P&g:bo-...` seq entries). A blank inside an open plain does not start a new token, but the charset scanner's quote clause opened on `word_led` alone — the first line's ` "; ` pair went phantom, swallowed the SECOND line's `&`, that anchor never split, and yeptris's long-name reading stayed on the table for the byte-strict verify to decline. Both walks (gate + copy) now require `!plain_open` before a quote opens; a genuine quoted token after a key colon or a seq dash still opens (plain_open is 0 there). Gates: r68 + all prior rounds' min cores carry; parse + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r68-anchor-split-plain-quote.bin`.

- **fuzz: the `?:` explicit-key rewrite covers every flow-depth hit, whatever the follower** — the nightly's round 67 (crash-ce09e3f8, 20 bytes: `{\n?:-b%rM,\n?:M,\n?:}\n`). The r58 form rewrote only the FIRST `?:` and only when a quote followed; this member has three glued `?:` pairs with PLAIN followers, so libyaml's invariant reading of the untouched pairs kept yeptris's plain keys (`?:-b%rM` as a key) on the table and the verify declined its own family. The walk now counts every quoted-free `?:` at flow depth and rewrites each into the empty-string-key form (`"" : `) in one pass — a `size*2+1` buffer against the two-bytes-per-hit bound. The byte-strict verify against libyaml's ORIGINAL dump arbitrates the widened gate: a mid-plain `?` (`[a?:b]`) changes libyaml's own reading under the rewrite and declines. Gates: r67 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r67-qflow-question-colon-plain.bin`.

- **fuzz: the anchor-split composite can leave the plain-LS/PS fold law on the table** — the nightly's round 66 (crash-601c1e24, 20 bytes: `&k:j:(<PS> UT\r|\r\n.V|`). The ':' split of `yd_anchor_name_charset_divergence` reconciled the anchor reading (yeptris names `k:`, libyaml truncates at `:`), but the PS inside the residual plain then folded in yeptris only — the props-led composite leg's strict `strcmp` declined its own family and the raw DIFFER aborted the seed walk. The leg's engine-agreement check now also accepts the r56 fold-walk signature (dumps byte-equal except yeptris's fold-space where libyaml carries the raw LS/PS triple, at least one triple, full alignment elsewhere); libyaml's byte-invariance under the split stays strict. `yd_plain_lsps_fold_divergence` now calls the same extracted walk. Gates: r66 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip + feed corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r66-lsps-anchor-split.bin`.

- `e_flow_floor_ok` (the JSON fast path's 9C9N flow-floor gate) now derives line starts with the scan break SSOT — `\n` and lone `\r`, not `\n` alone. A flow whose last content break was a lone CR escaped the closer-line indent check, so whole parses (span ≥ 24 → fast path) accepted what windowed parses (general kernel) rejected with `inconsistent indentation` — a chunked-feed identity break (issue #771, nightly r65). The fused-sink branch's single-line test likewise treats any `\r` as multi-line now.

## [0.6.45] - 2026-10-26
### Changed

- **fuzz: the flow-'?' transform only spaces node-position hits** — the nightly's round 64 (crash-45b843db's sibling, 30 bytes: `{?\"a\":/*com?\"a\":/*comment*!!\"}` — the explicit-key `?\"a\"` at map entry PLUS a second `?\"a\"` mid-plain in the value). `yd_qflow_adjacent_divergence`'s transform spaced EVERY `?`-before-quote; spacing the value's own `?\"` changed libyaml's bytes and the insensitivity check declined its own family. The transform now tracks flow node position (after `{`/`[`/`,` with only blanks between) and spaces only indicator hits — the value's mid-token `?\"` rides untouched. Gates: r64 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r64-qflow-nodepos.bin`.

- **fuzz: the BOM-indent law rides the comparator's second probe** — the nightly's round 63 (crash-c0850abf's sibling, 24 bytes: `a:\na:\n<BOM> a:\na:\n<BOM>.b:\n\n` — line-start BOMs as MAP KEYS). libyaml strips a line-start BOM but still COUNTS it for indentation, so `<BOM>.b:` lands indented (a nested map) while yeptris's BOM-as-content keeps the key at column 0. The r60 removal probe loses the nesting; `yd_bom_line_start_divergence` now also tries the replacement probe — each line-start BOM becomes ONE space, preserving the indent — and requires both engines' dumps of it to equal libyaml's original. Gates: r63 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r63-bom-indent-key.bin`.

- **fuzz: the glued-anchor-follower law rides the comparator** — the nightly's round 62 (crash-45b843db, 61 bytes, seq entry `- &-:x+`). libyaml's anchor charset is `[0-9A-Za-z_-]` and it ACCEPTS a non-blank (the `:`) right after the name — `&-` anchors `-` and the plain `:x+` follows; yeptris's props scan requires a separator after the name, and its plain-first guard bails the WHOLE token to plain (`&-:x+`). The r54 anchor-charset carry cannot reach it: that family has yeptris reading the LONGER (colon-riding) name, while here yeptris reads NO anchor at all. The new `yd_anchor_glued_follower_divergence` spaces the follower out (`&- :x+` — both engines then read anchor `-` + plain `:x+`) and requires both engines' dumps of the spacing to equal libyaml's original; block context only, gated on node position, with the catch-85 word-start quote law (a mid-word quote like `k-'` is plain content). Gates: r62 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r62-anchor-glued-follower.bin`.

- **fuzz: `yd_line`'s clamp covers every branch** — the nightly's round 61 (crash-c0850abf, a 4.9KB `!!map----...` tag on a MAPPING): the round-59 clamp only covered the `=VAL` arm, but every branch rides snprintf's would-be length — a `+MAP` props line of 4971 chars against the 4096-byte line buffer sent the same callers memcpy past it (ASan read overflow, CI seed-corpus walk). The clamp now lives once at the return, covering `+MAP`/`+SEQ`/`=ALI` alike. Gates: r61 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r61-map-props-line.bin`.

- **fuzz: the line-start BOM law rides the comparator** — the nightly's round 60 (crash-0f393f90, 18 bytes: `\r!= \n<BOM>\ra\r\nt\r%\r\r!`). libyaml strips a U+FEFF that OPENS ANY LINE (its encoding-mark law is not stream-position-bound); yeptris keeps it as content — and the fold then differs too (yeptris's BOM is first-class content with a fold space after it, libyaml's leading break folds away entirely), which is why the dump-level strip of `yd_bom_content_divergence` could not reach it. The new `yd_bom_line_start_divergence` removes every line-start UTF-8 BOM from the input and requires both engines' dumps of the removal to equal libyaml's original (the removal is pure to libyaml — it already stripped them). Gates: r60 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r60-bom-line-start.bin`.

- **fuzz: the comparator's props buffers no longer manufacture long-tag divergences** — the nightly's round 59 (crash-dc4d99f4, 546 bytes: `!<tag:yaml.org,2002c...c>`, a 543-char verbatim tag). BOTH engines parse it identically (verbatim tag, empty scalar) at every length — the manufactured DIFFER was the comparator's own asymmetry: the yeptris dump side DROPPED tags past its 512-byte copy buffer (`tbuf[512]`, `len < sizeof(tbuf)`) while the libyaml dump side TRUNCATED the same tag (`props[512]` + `tbuf[512]` snprintf), so a tag crossing 512 bytes turned agreement into a find. All props buffers grew past the harness input cap (`tbuf`/`vbuf` 8192, `abuf` 2048, `props` 8192) — long anchors get the same protection. The growth exposed a latent dump-layer bug, fixed in the same commit: `yd_line` returns snprintf's WOULD-BE length, so a 4.3KB props line against the 4096-byte line buffer made the callers memcpy past it (ASan read overflow, corpus input with a long `!!map---` tag) — the return is clamped to what the buffer holds. Gates: r59 + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r59-long-verbatim-tag.bin`.

- **fuzz: the flow-'?' + direct-colon family rides the comparator** — the nightly's round 58 (crash-40e24e7f, 9 bytes: `{\r?:\":b\"}`; the CR is not load-bearing — the one-line `{?:\"b\"}` fires identically). libyaml's scanner makes `?` a KEY token even GLUED to its follower, and the `:` immediately after then reads as the VALUE indicator of an implicit-empty key, so `{?:\"b\"}` parses as `{~: b}`; yeptris's pinned plain reading (HM87/652Z) keeps `?` as plain content, so the whole token is the KEY and the value is empty. The probe rewrites `?:` into the quoted-empty-key form (`{\"\": \"b\"}`-shaped) both engines read identically — libyaml REJECTS the bare `{ :X}` empty implicit key outright, and yeptris reads a colon there as plain content, so `:` alone cannot carry the rewrite — and requires both engines' dumps of it to equal libyaml's original. Gates: r58 min + all prior rounds' min cores carry (r44 = the lenient-parse split); parse + roundtrip corpora walk clean under ASan+libFuzzer on rebuilt harnesses. Seed `parse/r58-qflow-question-colon.bin`.

- **fuzz: the plain-LS/PS fold law rides the comparator** — the nightly's round 56 (crash-ecd39c56, 28 bytes: `k: Y#<LS> 'c<PT>d'b"\nj: '<PS>d'`). In PLAIN scalars the engines break differently: yeptris (YAML 1.2, b-as-line-feed) folds an LS/PS break exactly like a newline (one break -> a fold space), while libyaml's READ_LINE joins an LS/PS break RAW into the content (its fold law only folds breaks that start with `\n`), so the blanks that followed it vanish (`scanner.c` READ_LINE: `LS|PS -> LS|PS`). The quoted sibling was already carried (`yd_ps_space_divergence`); this is the plain sibling — libyaml's dump must equal yeptris's with every fold-space that faces a raw LS/PS triple consumed as that triple. The r56 shape also needed BOTH lines because the old whole-input substitution broke the quoted PS (kept raw by both engines). Companion fix in the same comparator: `yd_qflow_adjacent_divergence`'s space-the-`?` transform allocated `size + 1` but writes two bytes per `?`+quote hit — two hits overflowed the buffer (ASan write, local corpus `|\r\r#?????'#??'#`); sized to the real bound now, and the header-glue transform's per-line growth likewise. Gates: r56 min + all prior rounds' min cores carry (r44 = the lenient-parse split); roundtrip corpus 14332 clean and the 15k parse corpus walks clean under ASan+libFuzzer with the rebuilt harness (the earlier "clean" walk had ridden a stale binary). Seed `parse/r56-plain-lsps-fold.bin`.
## [0.6.44] - 2026-10-25
### Changed

- **perf(#739): the json write lane — one-pass serialize, run-batched + SWAR-scanned strings, and the json tree walker** — the serialize lane ran the emitter twice (dry exact-sizing + wet write; the #352 slice-2 shape), wrote every string byte through per-byte calls, and walked the yaml dispatcher's ~15 decision layers per node. Now: ONE wet pass into a growable buffer, run-batched + SWAR escape-scans (8 bytes/round), and a dedicated `emit_json_value` walker (one dispatch per node). Measured on the canonical medium fixture: serialize 16 MB/s -> 221 MB/s (14.2x), the generation leg 66 -> 783 ips (11.9x), byte-identical output (fixed-point harness + shape parity + full battery + conformance).

## [0.6.43] - 2026-10-25
### Changed

- **fuzz: the plain law outranks the dash law in the charset walks** — the nightly's round 57 (crash-bc242984, 39 bytes -> 33: `k: &ab?c ...\n-- [!!, &----?x- -:\n`). The `-x` content law (the round-31 finding) opened the plain at the `--` and dropped node_pos — but the walks' SECOND node_pos block re-raised it for `-`+only_dash (the seq-entry clause runs last), so a mid-plain `[` re-armed flow and the `&`-after-comma split the PLAIN's bytes (a one-space divergence, and the gate/transform pair under-counted `amps` against it — the #676 sizing class one byte from resurfacing). The node_pos law is now guarded by the plain law (`!plain_open`) in both walks; seq-entry dashes (no plain open) keep their node position. Gates: r57 + all prior rounds' artifacts carry (r44 = the lenient-parse split; r56 = the open plains family); snapshot verdict set byte-identical; ctest 579/579; parse + roundtrip corpora (~29k inputs) walk clean under ASan. Seed `parse/r57-plain-law-dash-precedence.bin`.

- **fuzz: the anchor-charset family rides alias names too** — the nightly's round 54 (crash-17654f5b, 31 bytes -> 26: `&a: \nfkey: &a vf\no:\n  *a:\n`). The 2SXE law gives ALIAS names the colon-riding charset exactly like anchors — `*a:` names `a:` in yeptris while libyaml truncates at the `:` and reads the key separator — but the charset carry split only `&` runs: the `*` fell to the walks' word arm (a phantom plain opened, the split never fired, amps under-counted). `*` now joins the props-run class in both walks and the gate/transform split alias runs; the verify is unchanged (libyaml invariant under the split AND both engines agree on it — the composition's `&a: ` empty-key hoisting rides along). Gates: r54 + all prior rounds' artifacts carry (r44 = the lenient-parse split; r56 = the open plain-LS/PS family); snapshot verdict set byte-identical; ctest 579/579; parse + roundtrip corpora (~29k inputs) walk clean under ASan. Seed `parse/r54-alias-colon-charset.bin`.

 **fuzz: the root-block carry's header law rides a trailing comment** — the nightly's round 55 (crash-5b569810, 59 bytes -> 5: `| #\n#`). In a block scalar every line is literal text, so the `#`-led line is CONTENT (the suite's DK3J law, the #683 carry) — but the carry's header detection required END OF LINE after the indicator/blanks, and the soup's header carries a trailing COMMENT (`| #`), so the walk never entered block mode and the col-0 content line lost its indent fix. The header law now accepts the comment (glued or after blanks — libyaml's own header law, the #650/#662 families); the transform indents the content and libyaml agrees. Gates: r55 + all prior rounds' artifacts carry (r44 = the lenient-parse split; r54 = the open alias-key engine bug); snapshot verdict set byte-identical; ctest 579/579; parse + roundtrip corpora (~29k inputs) walk clean under ASan. Seed `parse/r55-header-comment-root-block.bin`.

## [0.6.42] - 2026-10-24
### Changed

- **emit: the writer's grow/flush checks run per refill, not per byte** — the #739 lane-1 writer cut (issue #739; the slice-one profile's wr_byte lane, 18%). A `reserve` on the writer guarantees writable bytes at `p + len`; `wr_byte`/`wr_put` take the grow/flush path once per refill instead of once per byte, and the watermark law is unchanged in spirit (append-only output — a sink just sees chunks up to one reserve past the mark). The fused member write keeps the reservation honest. One-route serialize (json-users DOM, 48.4 MB output): 211 -> 280 MB/s (+33%). Gates: ctest 579/579; 15,113 parse seeds + 14,332 roundtrip seeds (byte-stability) walk clean; pin leaks clean.

- **fuzz: the flow-'?' carry keeps the indicator when spacing it out** — the nightly's round 53 (crash-52c8d1ef, 40 bytes -> 7: `{?"\n":}`). The adjacent carry's transform DROPPED the `?` (`{?"a":x}` -> `{ "a":x}`) — libyaml-neutral only while the pair's value is non-empty; with an EMPTY value the drop turned the explicit key into a bare member and libyaml errored ("did not find expected node content"), so the carry declined its own family. The transform now inserts a space AFTER the `?` — `?`+blank is the same explicit-key token to libyaml, so its dump stays unchanged for the same-line shapes too, and the empty-value form (`{? "\n":}`) parses to the identical stream. Gates: r53 + all prior rounds' artifacts carry; snapshot verdict set byte-identical (405 inputs); ctest 579/579; parse + roundtrip corpora (~29k inputs) walk clean under ASan. Seed `parse/r53-qfold-quoted-key.bin`.

## [0.6.41] - 2026-10-24
### Changed

- **fuzz: the anchor-charset walk opens the colon-riding plain** — the nightly's round 51 (crash-098e7549, 23 bytes: `&k:`c {&k:`c {\n  }\n  }\n`). yeptris is suite-correct twice over — the anchor name carries the `:` (2SXE) and the backtick — so `{` opens flow and the trees are nested anchored maps; libyaml truncates the name at `:` (its [0-9A-Za-z_-] set) and its 1.1-lenient plain start swallows the rest as ONE anchored scalar. The charset carry's split (`&k:`c` -> `&k :`c`) verified for the FIRST occurrence — but the walk's position model never opened the plain at the residual (`:` + non-blank, the HM87 riding colon), so the SECOND mid-plain `&` rode as props, the blind split corrupted the scalar's bytes and the carry declined its own family. The gate walk now opens the plain at a block-context `:` + non-blank, and the transform's split arm takes the residual token's first-byte law itself (a `:` with a blank follower — `&kk: |` — stays the key separator, not a plain). Gates: r51 + all prior rounds' artifacts; snapshot + crash-seed verdict sets byte-identical except the new shape; ctest 578/578; parse + roundtrip corpora (~29k inputs) walk clean under ASan. Seed `parse/r51-reserved-anchor-colon-plain.bin`.

### Fixed

- **parse: a `!!`-led tag survives a redefined primary `%TAG` handle** — the nightly's round 52 (crash-763bb852, 556 bytes -> 18: `%TAG ! !p/\n---\n!!i`). The secondary handle is fixed shorthand (spec 6.8.2.2): it resolves through its OWN `%TAG !!` entry or the `tag:yaml.org,2002:` default, never through the redefined primary — libyaml reads `tag:yaml.org,2002:i`, yeptris composed `!p/!i`. The resolver's longest-match loop now skips the length-1 `!` entry for `!!`-led tags (the `!!` entry and the default fallback are untouched; `%TAG !! e2/` still overrides). Pin `TagResolve.SecondarySurvivesPrimaryRedefinition`; gates: r52 artifact + all prior rounds carry; ctest 579/579; snapshot + crash-seed verdict sets identical; corpora walk clean under ASan; pin leaks clean. Seed `parse/r52-secondary-tag-handle.bin`.

- **fuzz: the four qflow spill folds are removed — the engine owns the law now** — the re-siding after the #741 engine fix (the `?` empty-key pair completing across its entry separator). The r45 spilled twin pair, the r45/r49 value-side pair fold, the r48 spilled first map, and the r48 map-split transfer existed to paper over that lifecycle one face at a time; with the engine fixed, every round's artifact (r45-r50) matches natively without them. The value-position colon rider, the in-map spill run, and the qcolon walk stay — distinct families (the HM87 colon law and the `?[break],:` multi-line reading still carry there). Gates: r45-r50 artifacts + all prior rounds' shapes carry; ctest 578/578; snapshot + crash-seed verdict sets identical; parse + roundtrip corpora (~29k inputs) walk clean under ASan.

- **fuzz: the qflow walk's value-side pair fold requires a non-empty line** — the nightly's round 49 (crash-27edc507, 10 bytes: `{\n?(\n,\n?\n}`). The key-position `?-`plain rider (`?(` -> libyaml's explicit key `(`, the suite's plain-first law) was ALREADY covered by the key-line twin arm — but the r48 value-side pair fold misfired in front of it: when libyaml's OWN line is an EMPTY value, the arm's "yeptris two ahead matches libyaml's line" condition matched an empty yeptris line too, folding yeptris's REAL pairs and desynchronizing the walk. The arm now requires libyaml's line to carry content (`lens[i] > 7`) — the spill fold only makes sense in front of one. Gates: r49 + every prior round's shape carry; ctest 575/575; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r49-explicit-key-qmark-plain.bin`.

- **fuzz: the qflow walk folds the map-split composition** — the nightly's round 48 (crash-248cf333, 83 bytes). The soup's `? ,: X` members split the maps differently: yeptris keeps the spilled pairs inside ONE map (`{null:.00, null:null, null:9}` — the r45/r46 spill laws composing) while libyaml splits them across maps (`{null:.00}, {null:9}`). Four folds carry the family: the leading null:null block before libyaml's own map, the in-map value-side pair, the trailing run of null:null pairs (generalized from the r46 single pair), and the pair-transfer — libyaml's next null-key map block consumed against yeptris's next pair when the closer isn't aligned (the run-fold's `folded` gate had wrongly blocked it; the alignment check is the closer itself). Gates: r48 + every prior round's shape carry; ctest 575/575; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r48-map-split-composition.bin`.

- **fuzz: the raw-NEL line-break carry** — the nightly's round 47 (crash-48e46b87, 16 bytes: `-'{\n"<<o\y\r<NEL>\t\r`). YAML 1.1 (libyaml) counts NEL (U+0085) as a line break — CR+NEL inside a double-quoted scalar normalizes to one `\n` — while YAML 1.2 drops NEL from the break set, so yeptris folds the CR to the flow-fold space. The two readings are the schema difference: the carry walks both dumps with escape-aware lookahead (a real newline renders as the two-char `\n`; the fold space as one ` `) and carries when every difference is exactly that pair, gated on the input containing the raw NEL bytes. Gates: r47 + every prior round's shape carry; ctest 575/575; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r47-nel-break-law.bin`.

## [0.6.40] - 2026-10-24
### Changed

- **fuzz: the qflow walk folds the value-position colon rider and the in-map spill** — the nightly's round 46 (crash-d91c7ad7, 10 bytes: `[?[] :b,]`). Two suite-pinned readings compose: after an explicit key's value, libyaml consumes the leading `:` of a plain-first `:b` as the pair separator while the suite keeps the WHOLE `:b` as the plain (HM87 class) — libyaml's (value `b`) folds onto yeptris's (value `:b`); and the trailing `,` spills a second implicit member inside the SAME map (the r45 law at pair granularity) — a full null:null pair or, when the closer arrives before any value, a lone null KEY line. The spill fold falls through to the closer's own equality (the r45 alignment lesson); the bypass form that skipped it was the first cut's bug. Gates: r46 + every prior round's shape carry; ctest 575/575; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r46-explicit-key-colon-plain.bin`.

### Fixed### Changed

- **fuzz: the qflow walk folds the spilled twin pair** — the nightly's round 45 (crash-89973d9d, 16 bytes -> 6: `[? ,:]`). The `:` after the `,` that closed an explicit-key pair spills a SECOND null:null pair in yeptris — the suite's reading (the r38 spill law) — while libyaml keeps one map. The qflow parallel walk folds the duplicate when the libyaml side has hit a closer and yeptris still holds an exact duplicate of the four-line null:null block; the fold falls through to the closer's own equality so the tail stays aligned. Gates: r45 + every prior round's shape carry; ctest 575/575; snapshot + crash-seed verdict sets identical to the predecessor (three pre-existing lenient-parse flags byte-identical on both sides). Seed `r45-qflow-spill-pair.bin`.

### Fixed### Fixed

- **emit: the anchor-name spell laws reach the line-tail props path** — the round-45 bonus catch (crash-5f467aa0, 285 bytes -> 12: `8,":: &<PS>\n-`, found by a local fuzz session). The reader folds a PS-named anchor onto the FOLLOWING collection, and the collection's props spell through `emit_props_tail` ("`!<t> &a`" after "key:"/"-") — a path that bypassed every anchor-name law and wrote the PS raw. The re-parse's break normalization turns the raw PS into a hard break: pass2 sees `&<break>` and dies. The tail-colon law (#730), the normalize-subject law (#723), and the collection law (round 31) now live in one shared check that every anchor write path routes through. Pin `AnchorNameNormalizeSubjectOnLineTailPropsIsUnserializable`; seeds `roundtrip/r45-props-tail-anchor-core.bin` + the artifact.

### Changed### Fixed

- **emit: an anchor name ending in ':' fails the serialize when the scalar spells in block style** — the nightly's round 44 (crash-07ad9ecc, 1928 bytes -> 8: `&:\n! |\n:\n`). The round-31 tail-colon law failed the serialize for COLLECTIONS (every spelling of `&: <collection>` re-lexes differently) but exempted scalars — "`k: &n: v` rides byte-stably". The exemption overreached: it holds only for INLINE spells. A BLOCK-style scalar (the reader folds a tagged literal across the props lines: tag `!`, anchor `:`, value `:\n`) spells `! &: |` — the header sits where the separator should be, and the re-parse reads a different tree (the compat reader rejects the form outright). There is no spell: the serialize fails, which the roundtrip harness treats as a skip. The inline rider stays byte-stable. Pin `AnchorNameTailColonBlockStyleIsUnserializable`; seed `roundtrip/r44-tail-colon-anchor-block-style.bin`.

### Changed

- **fuzz: the qflow parallel walk heap-sizes its line arrays** — the nightly's round 43 (crash-710e5302, 2379 bytes). The flow-`?` mega-soup's dumps walk 644/557 lines, but `yd_qflow_divergence`'s line arrays capped at 512: the walk silently truncated mid-stream, every member it then saw was covered, yet the cap's misaligned tail hit a plain mismatch and the carry declined its own family (every one of the artifact's 28 divergence hunks is the covered single-pair fold). The arrays are now heap-sized to the true line counts, freed at every exit. Gates: r43 + r42 + r41 + #718 + r40 + the #707 soup + crash-3f2e419b all carry; ctest 573/573; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r43-qflow-line-cap.bin`.

### Fixed

- **fuzz: a document marker must END at blank/EOL to be one** — the nightly's round 42 (crash-851decac, 26 bytes: `k: &ab?c ...\n---!, &--&-:`). The coll_col law's marker arm fired on `---!` — a marker GLUED to content (no trailing blank): `---!, ...` is a plain starting with content dashes in BOTH engines, but the walk treated it as a document marker, cleared the open plain, and the `!` re-armed props — the later `&--&-` split mid-key and the carry declined its own family. The marker arm now requires the marker to END at blank/EOL/EOF; a glued `---x` falls through to the content-dash law (the line's plain opens at the dash and everything after stays content). Gates: r42 + r41 + #718 + r40 + the #707 soup + crash-3f2e419b all carry; ctest 573/573; snapshot + crash-seed verdict sets identical to the predecessor. Seed `r42-glued-marker-not-marker.bin`.

### Fixed

- **fuzz: the plain-continuation law rides the enclosing construct's column** — the nightly's round 41 (crash-7d17a81e, 10 bytes: `- :"\n- &g:`). The first cut of the #718 law treated a line at column 0 as "the plain rides" whenever the plain's own line also started at column 0 — correct only at DOCUMENT root (libyaml's indent is -1) and wrong inside a block collection: a seq entry's plain ENDS at the col-0 next entry, so the `&`-led second line is a fresh node position and its anchor must split (`&g:` -> `&g :`, both engines then agree and libyaml is invariant). The resolution now compares against `coll_col` — the enclosing construct's column (the seq dash / explicit-key `?` / mapping key at its line start; -1 at root). Two follow-on laws fell out of the seed sweep: a plain can START with content `-`/`?` (`-- !!map---!` — only `- ` / `? ` are constructs), which opens the line's plain and keeps a later `&` content (crash-3f2e419b re-verified — it also re-carries crash-f1215da4), and construct columns are recorded only for real constructs (`-`/`?` followed by blank/EOL). Gates: r41 + #718 + r40 + the #707 anchor-soup all carry; ctest 573/573; snapshot + crash-seed verdict sets identical to the predecessor (three documented flags aside, byte-identical to main). Seed `r41-seq-entry-riding-anchor-colon.bin`.

### Fixed

- **fuzz: the anchor-charset walks track block plain continuation** — issue #718 (crash-6bad313dd, 11 bytes: `&o:q r\n&g&`). The gate and transform walks reset node position at every break, so the `&`-led CONTINUATION line of a multi-line plain looked like a props position and the transform split it — corrupting the scalar (`&g &`) and making the carry decline its own family (the first split `&o:q` -> `&o :q` was right). Both walks now track a block plain-open state with column bookkeeping: a plain opens at the first content byte outside a props run, a line more indented than the plain's line rides the plain (its `&` is CONTENT in both engines), a line at/below the plain's column ends it (a root-level plain's column is 0 and swallows every line — libyaml's indent is -1, `a\n- b` is ONE scalar), a key colon (`:` + blank/EOL) ends it, and a `|`/`>` header at node position switches to a block-scalar body whose bytes are text until a line at/below the header. The walk law was checked against the libyaml oracle (Psych): `k: v\n  &x z` -> `{"k" => "v &x z"}`, `k: v\n&x z` -> error, `a\n- b` -> `"a - b"`. The #718 repro now carries (`yeptris(split) == libyaml(original)`); the r40 artifact still carries; ctest 573/573; the snapshot and crash-seed verdict sets are byte-identical to the predecessor.

### Fixed

- **fuzz: the anchor-charset carry continues a tag-led props run** — the nightly's round 40 (crash-fe3b5522, 66 bytes: `!str-f &U:ba:\n- 0baJb:0U:ba:\n- 4-ba:J::*...`). The gate and transform walks tracked node position, and a props run RESETS at blanks — so in the tag-led composite `!str-f &U:ba:` the `&` landed at node_pos 0 (the last content byte was the tag's `f`) and both walks skipped it: no carry, straight to the raw divergence. Blanks after a `&`/`!` now KEEP a props-continuation flag (a props run spans its blanks — `!tag &anchor` is one run) until content clears it, and the `&` gates on `node_pos || saw_prop` in both walks. The composite's PROPS-LED form also gained its third verify: when yeptris reads the whole riding name onto the FOLLOWING collection (no pair) while libyaml truncates the name at the `:` (pair built), the carry builds the split (`!str-f &U :ba:`), requires yeptris to parse it, and requires BOTH engines' dumps of the split to agree with libyaml's own dump of the original — libyaml is invariant under the split (its reading truncates at the `:` either way). Seed `round40-anchor-charset-tagprops.bin`.

### Fixed

- **parse: a flow sequence's plain collection entry no longer plants a stale pair bit** — the corpus sweep after #713's carries (crash-8106600e, 43 bytes: `[1100, 120,20,[000, 1- 9]\n \n, 120,  ? 20:50 ,0,3- 9]`). The flow loop's collection-as-value arm wrote `pair_value = 1` unconditionally — in a SEQUENCE with no open pair that bit is stale, and a LATER `?`-opened pair wrap (`? 20:50 ,`) inherited it: the pair's comma flush then skipped the implicit null VALUE event entirely (yeptris `{20:50}` with no value event vs libyaml `{20:50 => nil}`). The write is now gated on `pair_open`. Unit pin pins both the artifact shape and the minimal twin (`[? 20:50 , 0]`).

- **parse: the break normalizer resets its flow state at document markers** — the nightly's round 35 (crash-ef534bdf, 2505 bytes → 22: `1,"\n--- 2[\"\n--- T \"<LS>`). The normalize walk's `in_flow` survived the `---` marker: the parser treats markers as unconditional document boundaries (its post-cut reset wipes the flow state), but the walk carried an unclosed `[` from an EARLIER document into the next one, where every blank then re-armed token starts (the in-flow law) and the mid-plain quote of `--- T "` opened a phantom quoted region — the trailing LS stayed RAW and the plain's trailing space survived in the whole parse while every chunked window (a fresh probe per run) folded it. The walk resets `in_flow` at every marker (the in-quote rescue, the block-fold's marker arm, and the doc-inline twin — which now also fires inside an open flow, matching the parser's unconditional boundary). Unit pin pins the whole-vs-chunked identity across cuts; feed corpus seeds `marker-resets-normalize-flow.bin` + the artifact.

### Fixed

- **parse: a block's tab-carrying content line counts its own break toward keep** — the nightly's round 37 (the `fCsK...` artifact, 14 bytes: `|+\n\n >\t\n   \t \n`). The block reader's tab-content arm (a whitespace-only line whose tab follows leading spaces) consumed its line's own break via the line-done helper without counting it toward `trailing_breaks` — the content-line arm scans it, and the spaces-past-indent twin does too — so a keep block that ENDED on such a line spelled one break short (libyaml keeps the final break). The arm now scans the break like its siblings. **The round-32 emit compensation is reverted**: the extra break spelled for tab-carrying ws-only last lines (`written N+1 reads N`) existed to mask this reader bug, and with the reader fixed it made every pass grow one break (the #709/#711 pins flip to write-N-reads-N and stay green as stability gates). Reader pin pins the exact keep value; seed `block-keep-tab-line-eof-break.bin`.

### Fixed

- **emit: an anchor name carrying a normalize-subject byte fails the serialize** — the nightly's round 39 (crash-8df48a71, 979 bytes → 12: `P,": &`). Pass1 escaped the KEY's PS as `\P` but spelled the ANCHOR name raw (`&<PS>`); the reader's break normalization folds a raw PS to `\n` before the parse, so pass2 saw `&\n` — an anchor with a broken/empty name — and the re-parse died. YAML has no quoted anchor names: the anchor emitter now fails the serialize (the #708 unspellable law) when a name carries a raw LS/PS/NEL or a hard break. Pin pins the PS core, the NEL twin, and the plain-name rider; seed `roundtrip/anchor-name-ps-unspellable.bin`.

### Changed

- **fuzz: the qflow walk folds the '?-colon-value' member** — the nightly's round 38 (a 24-byte mutation: `{\n?:2,\n  foo :2,\n ---barM,\n}`). libyaml makes the flow `?` an explicit-key indicator with ANY follower, so `?:2` reads `{empty => 2}`; the suite's plain-first law (HM87/58MP) keeps `?:2` ONE plain key with an empty value — the `:` rides yeptris's plain. The qflow walk's new arm folds libyaml's (empty key, V) onto yeptris's (`?:V` key, empty value). Seed `flow-q-colon-key-value.bin`; the 15100-seed corpus walks clean through the release, libFuzzer, and ASan builds.

### Changed

- **fuzz: the flow-? quoted-follower carry wraps the plain span for the seq-entry twin** — the nightly's round 36 (crash-ce698341, 8 bytes: `[?"\\l"]`). yeptris reads the suite's plain-first law (`?"` adjacent = one plain, HM87/652Z); libyaml makes an explicit pair whose double-quoted key is escape-processed (`\\l` -> `\l`). The catch-149 carry spaces the `?` out and requires libyaml's dump unchanged — correct in a flow MAP, but in a flow SEQUENCE spacing collapses libyaml's pair to a bare scalar, so the insensitivity check correctly declines. The carry's second try wraps the whole plain span in SINGLE quotes instead (single-quoted escaping keeps the backslashes raw — exactly yeptris's plain reading) and verifies libyaml(wrap) == yeptris(original). Seed `flow-q-quote-escape-plain.bin`; corpus 15097 walks clean through the release, libFuzzer, and ASan builds.

### Changed

- **fuzz: the anchor-charset/qflow comparators track flow depth and token starts like the engine** — the corpus sweep's composites (crash-7fe697be 195 B → core `- :"\n- &g:`; crash-283aaadd 291 B → core `/]":\n &4:`; crash-e1d13ebe; crash-2bbb3db7) exposed four walk imprecisions, each swallowing a later `&` or mis-splitting a plain: (1) the qflow walk lacked the `?`-single-pair block arm and the BREAK-led spill twin (a `:` after `? ,` at a line break spills a SECOND implicit pair, not a plain), so an artifact composing both members defeated each walk alone; (2) the anchor-charset transform split `&` runs with EMPTY names — mid-plain content (`--&\t0`) — corrupting the scalar (the split now requires at least one in-set name byte); (3) `:` no longer arms a word start (a quote glued to the separator, `- :"`, is plain-first content per HM87 — the eager open swallowed the rest of the input); (4) the flow indicators arm only at node position or inside open flow (`/]"` is ONE plain — block-plain content absorbs `]`). The full 15090-seed corpus now walks clean through the release, ASan, and libFuzzer builds.

## [0.6.39] - 2026-10-23
### Fixed

- **parse: the anchor-yield follower scan crosses comments** — the nightly's round 32 (crash-d580b467, 71 bytes → 8: `&: #\n|\nA`). The flow/block-header follower law (0.6.37) stopped its blank/break scan at a `#`: `&kk: #comment` with the block header on a later line kept the colon riding onto the anchor name, and the two routes spelled the pair differently per pass (pass1 `&kk: |` vs pass2 `&kk "": |` — byte instability; the shrunk twin `&:` yields an empty anchor name and now rejects exactly like libyaml). The follower scan treats a comment as blank-equivalent: skip it to its break and keep scanning. Round-33's all-whitespace tab-led keep blocks closed as fixed by 0.6.37's block-scalar law (`#711`).

## [0.6.38] - 2026-10-23
### Fixed

- **emit: a keep block's tab-carrying CONTENT last line spells its trail breaks 1:1** — the nightly's round 33 (yeptris#711, crash-306b67eb, 59 bytes; minimized 15: `|+\n \n \t:\n\t \n\t\n\n`). The #709 extra-break law keyed on ANY tab in the last body line, but the reader swallows the own break only of a WHITESPACE-ONLY tab-led last line — a content line that merely carries a tab ("\t:") reads 1:1, so the extra break made the keep value GROW one break per pass (16 → 17 → 18 bytes across passes). The law now gates on the last line being all-whitespace; the round-32 pin (the ws-only twin) is unchanged. Corpus seed `block-keep-ws-composite-tab-content.bin`; a two-pass stability pin joins the round-32 test.

- **parse: an anchor name ending at a riding colon yields to the block-scalar-header and next-line laws** — the nightly's round 31 (crash-4cbeede1, 45 bytes → `keey1: aga |\n     '&tirel\n&kk: |\n      &tirelue9`; crash-c2232e17 → `&k:\n\n {\n}`). The flow-follower yield (0.6.37) missed two twins: a `|`/`>` header after `&kk: ` kept the colon riding onto the anchor name (`kk:`), so the props-led shape parsed as ONE anchored block scalar — the pair frame and the implicit-null key event vanished (the map emitted a dangling half pair); and a follower on a LATER line (`&k:` then a break and ` {`) rode the colon and DROPPED the flow map outright. The follower set now includes the block headers, and the follower scan crosses blanks and breaks: the colon re-lexes as the key indicator, the anchor names the implicit-null KEY, and the collection/scalar on the next line is the value, matching libyaml exactly. The plain-led twins keep riding exactly where the ledger pins them. Unit tests pin the literal and folded trees; corpus seeds `anchor-colon-block-header.bin`, `anchor-colon-nextline-flow.bin`.

- **emit: an anchor whose name ends in ':' on a collection fails the serialize instead of emitting an unstable form** — the nightly's round 31 (crash-66a513ea, 396 bytes → 33: `:\n[]\n{}\n:` ×3 + `: &:\n- []`). The fused route glues the riding anchor onto the following collection; every spelling of `&:` directly before a flow (`? &: [[]]`, `k: &: [[1]]`) re-lexes as props + the key indicator — the riding/yield law reads the colon as the separator at the follower — so the emitted form parsed to a different tree (byte instability, pass1 427 vs pass2 430). The anchor emitter now marks the shape unserializable: serialize fails rather than emitting a form the parser cannot read back. Scalar nodes keep riding raw (`k: &n: v` is byte-stable, 2SXE). Unit test pins the artifact tree, the unglued twin, and the scalar twin; the artifact rides the fuzz corpus.

- **emit: the keep-chomp spelling of a tab-carrying whitespace-only last line writes one extra break** — the nightly's round 32 (crash-90c98918, 26 bytes → 10: `|+\n:\n \t\n\n\n`). The reader swallows the own break of a whitespace-only more-indented last line that carries a TAB (the tab-indent law shifts the line's content start): written N reads N-1 there, while a spaces-only last line reads 1:1. The emitter spelled the keep value with `trail` breaks either way, so pass1 re-parsed one break short and pass2 demoted the header to clip — a byte instability. The block writer now writes the extra break exactly for the tab case. Unit test pins the tab twin (stable), the spaces twin (stable), and the demotion is gone; corpus seeds `block-keep-tab-last-line.bin`, `block-keep-space-last-line.bin`.

## [0.6.37] - 2026-10-22
### Fixed

- **parse: an anchor name ending at a riding colon yields to the flow-follower law** — the nightly's round 29 (yeptris#700, crash-47eb4054, 39 bytes → `&k2: [1,\n2]`). The riding-colon law (2SXE) swallowed the colon into the anchor name, so the props-led flow parsed as a ROOT collection carrying the anchor — the map frame and the implicit-null key event vanished entirely, diverging from libyaml, which ACCEPTS the shape (the anchor names the implicit-null KEY; the flow is the value). The anchor scan now un-consumes the colon when its follower is a flow collection: the colon re-lexes as the key indicator, the existing colon arm builds the anchored null key, and the ordinal registers under the true name. The plain-led twins keep riding exactly where libyaml rejects them (2SXE's `&a: key: v`, W5VH's value position, colon runs, the double-anchor law — all pinned); the col-0 flow continuation still rides the ledgered 9C9N floor, so the anchored twin rejects like its unanchored one and the nightly verdict shifts DIFFER → the non-aborting libyaml-only-accept split. Corpus seeds `anchor-colon-flow-*.bin`; unit tests pin both laws.

- **parse: a directive-looking line never parts from the `---` the cut chooses** — the nightly's round 27 (crash-95bbd95f, 2072 bytes → 21: `i\n"\n...\n%TAG ! "\n--- `). The step scan's quote tracking can close a quoted token where the parser reads content (or vice versa), so a `%`-led line rides an open quote the scan believes in; the marker arm's `saw_directive` guard never arms, and the cut at the following `---` strands the directive — the parsed window then rejects a directive with no document start while the whole route accepts. The marker cut now also requires that the byte before the marker's line is not `%`: the check is byte-level, independent of the quote tracking's verdict. Whole and every split now agree; feed corpus seed `directive-marker-cut.bin`.

## [0.6.36] - 2026-10-22
### Fixed

- **parse: the break normalizer honors the doc-marker quote rescue in whole-buffer runs** — the nightly's round 25 (crash-27a77050, 219 bytes → 14: `>\n: '\n---\na<U+2029>`). A col-0 `---` rescues an unterminated quote (the parser ends the document there — the post-cut reset's "clean state" law), but the whole route's normalize probe kept classifying post-marker bytes as quoted content, so the trailing PS rode RAW into a plain scalar (`v = "a"+PS`) while the chunked route's fresh post-cut probe folded it (`v = "a"`) — whole-vs-chunked identity broken. The probe now closes an open quote at the marker and arms line-start state on breaks inside the quoted walk; the #638 law itself is untouched (a genuinely quoted LS/PS still stays raw on both routes). Feed corpus + 4,000-input LS/PS/NEL-heavy randomized sweep: zero divergences; full battery 562/562.

- **parse: a truncated explicit-key pair completes its null at the right level** — the nightly's round 24 (crash-bcc0f52c, 278 bytes → `? a:\n   ? b<EOF>` / `? a:\n        ? 9\n        :`). Two state bugs in the '?'-pending stack: `e_pair_value_done` popped the DEEPER pair's pending slot (its depth check ran AFTER the value parse, which had opened the deeper frames), and the ':'-line completion cleared the shared `q_value_pending` flag while the stack still held the outer entry — each stranding a null that libyaml emits inside its own map. The pop now keys on the pair's level captured BEFORE the value parse; the ':'-line arm maintains the `pending = stack non-empty` invariant the flush itself uses.

### Fixed

- the anchor-charset carry's quote tracker now follows the word-start law: a mid-word quote is plain CONTENT (`v'`), so the tracker no longer swallows the rest of the input as an open quote — the `&` run after a plain key ending in an apostrophe gates and splits again (the nightly's round 23, crash-c1dc27cf: `v': \n&1: []` composed the ledgered 2SXE colon-name split with an ungated shape)

### Added

- **JSON: the pretty lane goes public** — `yeptris_serialize_json_pretty` (json.h) emits `JSON.pretty_generate`'s shape (one entry per line, `": "` after keys, empty collections inline) with a caller-chosen indent unit (NULL/two spaces default); the internal pretty writer (json-c PRETTY, yajl beautify) keeps its exact bytes on the default path (yeptris-ruby#304)

### Fixed

- the internal pretty JSON serializer materializes the lazy DOM: json-c's PRETTY (`json_object_to_json_string_ext`) on a never-queried document dereferenced the unmaterialized DOM and crashed (found probing the ruby binding's pretty slice)

### Fixed

- the anchor-charset carry now composes with the breakless-EOF clip carry: `&kk: |` plus a content line at EOF keeps BOTH suite divergences at once (the ':' anchor name per 2SXE and the clip break), and each single carry's transform left the other difference — the carry now also requires the split copy with a break appended to parse with no divergence (the nightly's round 20, catch 166, crash-e5098af3)

- the UTF-16 breaknorm carry's plain-FOLD arm now carries every fold shape: U+2028/9 units inside a plain scalar fold per the spec's b-char law (one break to a space, a run of n to n-1 newlines) while this libyaml keeps them raw — the decode with each LS/PS substituted by a literal newline re-enters libyaml's OWN fold law (1 break folds to space, n to n-1), so one substitution covers the single PS (the nightly's round 21, catch 167) and the PS PS double-break composite of #685; when the input also carries content BOMs, libyaml's extra stream+document-start BOM strip composes with the BOM-content carry on the substituted dump

- **parse: the bare non-specific tag `!` no longer resolves through `%TAG`** — spec 7.3.3 exempts `!` from directives, and the suite pins the event as `<!>` (8MK2/52DL/S4JQ: `! a` → `=VAL <!> :a`); `%TAG ! x:` + `- ! v` had attached the directive prefix to the scalar (the nightly's round 22, crash-d6be3347). The residual libyaml plain-drop is carried by the new non-specific-tag comparator (the nightly's round 22, catch 168)

## [0.6.35] - 2026-10-21
### Fixed

- carried the root-block col-0 content family for CRLF/breakless tails: the fuzz comparator's line walk split only at `\n`, so `"|\r\n##"`'s header read as `|\r`, the root-block carry declined its own shape, and the nightly aborted on a divergence the suite adjudicates (DK3J: zero-indented block scalars are content; libyaml's min-1 clamp empties them). The walk now ends lines at CR too and copies the terminator verbatim (the nightly's round 19, catch 165, crash-e029a445)

- **marshal: the strict duplicate-key scan is per-map and its table
  grows** — `yeptris_marshal_node_ex`'s `YEPTRIS_MARSHAL_STRICT_DUP_KEYS`
  pre-scan (0.6.34) kept ONE table cleared on `MAP_OPEN`, so a parent
  key matching a key inside a preceding child object raised a false
  `duplicate key in object` (every lane without a native materializer,
  i.e. Windows, rejected ordinary schemas such as `{"a":{"b":1},"b":2}`),
  and a parent duplicate after a nested object went unseen. The table
  also never grew past its 16 initial slots: a 17th distinct key in one
  object probed forever. The scan now keeps one table per open
  container on a frame stack and rehashes at load factor 1/2. C pins
  `Marshal.StrictDuplicateKeysArePerMap`,
  `Marshal.StrictDuplicateKeysScanGrowsPastSixteenKeys`.
- fixed the emitter riding plain over a scalar value that carries a raw LS/PS/NEL — since the quote-aware normalizer (#638) quoted scalars keep those bytes, and a plain containing one re-parses as a line break, breaking the serialize∘parse byte-stability invariant (the nightly's round 18, crash-1e79b2e3). `yep_style_plain_safe` now refuses plain for any span holding a multibyte break, so the value rides quoted (escaped `\L`/`\P`)

- fixed the anchor-charset carry's comment law: both the gate and the transform stopped scanning at the first `#` comment instead of the line break, so an anchor like `&4:` on a later line never gated and the family declined its own shape (the nightly's round 17, catch 163, crash-39eb5a17)
- **the break normalizer is now quote-aware (#638)** — LS/PS (U+2028/U+2029) stay raw inside single/double-quoted scalars (libyaml's READ_LINE keeps them as content; the JSON suite's y_string_u+2028_line_sep / y_string_u+2029_par_sep demand it), while NEL keeps folding everywhere and block-context folds are unchanged. The context walk mirrors step_scan's quote/comment/verbatim/flow arms minus the cut machinery, with the engine's value-position law (`k: 'x'` opens, the block plain continuation `a 'b'` does not) and the header-line-indent block-scalar floor
- fixed the explicit-key pending guard for dash-led plain keys probing the byte after the line start instead of the byte after the dash — at indent>0 a sibling like `---v: 1` was swallowed as the pending key's content and a props-pending anchored key (`? &e`) never emitted (the nightly's round 16, catch 162)
- **resolve: the compat-11 float law requires the dot and a signed
  exponent** — the decimal branch counted ANY exponent as a float, so
  compat-mode `1e0` / `1.5e2` typed as floats while psych's scanner
  (and libyaml's 1.1 float regex) keep them strings: only `1.5`,
  `.5`, and signed-exponent `1.5e+2` are floats. The leak surfaced as
  yeptris-ruby#293 (a host String "1e0" CBOR-dumped as float 1.0,
  360 checksum values per serialbench `large`). Core-12 keeps the
  spec law (unsigned exponents are floats). C pin
  `Cbor.SchemaTypingKeepsHostStringsText`; additive
  `yeptris_document_new_schema` lets the dump faces build
  compat-typed documents, and mutation typing now rides the
  document's resolver (`yep_dom_resolver`) instead of a hardcoded
  core-12.
- **parse: an explicit key's separator may follow on a later flow
  line** — `{?{a}\n:b}` read `:b` as a fresh plain entry and dropped
  the pending entry's value (libyaml: key `{a: null}`, value `b`).
  s-separate spans line breaks in flow, so after a `?`-key the loop
  top now consumes the separator when one is pending; the post-node
  colon law never sees a colon at the node start. Unit pin
  `Parse.ExplicitKeySeparatorOnLaterLine`; the artifact passes
  natively (the nightly's round 14, run 39545175099,
  crash-8e502adb).
- **parse: the seq pair-wrap consumes its explicit key's flush
  separator** (#671) — `[? &a :b,]` read the value as `:b`: the
  JSON-style `{a:b}` law lets the entry separator sit flush against
  the value, and the map frame's pending-key law consumed it, but the
  seq single-pair wrap read it as plain content. Explicit-key pairs
  now mark their frame and consume the flush colon; an IMPLICIT key's
  separator is already spent at the value slot, so `[s: :v]` keeps
  its leading-colon plain (the HM87 law; libyaml rejects it — a
  documented split). Unit pin
  `Parse.ExplicitKeyJsonColonInSeqPair`; found as the catch-160
  residual (the nightly's run 39539425897, crash-f43c05a9), whose
  artifact now passes natively.
- **fuzz_diff: the anchor-charset split composes with the flow-'?'
  colon families** — the nightly's round 13 (run 39539425897,
  crash-f43c05a9) diverged on `? &a:b&` inside a flow sequence: the
  catch-158 split turns it into `? &a :b&`, and the residual is
  yeptris's seq pair-wrap keeping the separator `:` in the value
  (`:b&` vs libyaml's `b&`) — a real engine gap, filed as #671 (the
  map frame consumes it: `{? &a :b}` is EQUAL). The carry composes:
  the split's verification now also accepts the qcolon walk's two
  twins (the `:X`-key form and the new value-side form), and the
  gate no longer bails on `*` (mid-plain content — the artifact's
  own first item). Seed `anchor-charset-qflow-pair.bin`.
- **fuzz_diff: the flow-'?' carry covers the empty key** — libyaml
  makes ANY flow '?' the explicit-key indicator, so `{?:\n}` reads an
  empty key; the suite's ns-plain-first law (HM87 `[?x]`, JR7V
  `{key?: value }`) keeps `?` plain, and yeptris reads key `?` with an
  empty value. The catch-142 carry's key-line twin required a
  non-empty libyaml key (`=VAL : x`); the twin now also accepts the
  empty-key form (`=VAL : ` vs `=VAL : ?`) — the nightly's run
  39534504716, crash-d176f548. Seed `flow-q-empty-key.bin`.
- **fuzz_diff: the anchor-charset carry covers every out-of-set byte** —
  libyaml's anchor name charset is `[0-9A-Za-z_-]` (IS_ALPHA); the suite
  keeps every ns-anchor-char inside the name (W5VH, 2SXE), so
  `k: &ab?cd v` anchors `ab?cd` in yeptris while libyaml anchors `ab`
  and reads `?cd v` as the value (the nightly's run 39520357518,
  crash-d5237f57; the round-9 colon carry covered only `:`). The carry
  now splits every quote-free `&` name run at libyaml's truncation
  point (`&ab?cd` → `&ab ?cd`) and verifies libyaml's dump of the
  ORIGINAL equals yeptris's dump of the SPLIT — the divergence is then
  exactly the charset law. Seed `anchor-name-charset-split.bin`.
- **parse: a wrapped seq-pair key resets the pair's value slot** —
  `[[a]: ]` (a flow sequence entry that is a single-pair map whose key
  is a flow collection and whose value position is empty) dropped the
  implicit null value: wrapping the buffered item as the pair key set
  `pair_open` but kept the stale `pair_value` the collection's own open
  had recorded, so the boundary flush believed the pair complete.
  libyaml emits the null; the catch-157 artifact (`…]]: , [ c]]`)
  diverged on exactly that event. The double-colon guard is unchanged
  (it reads the stale bit before the wrap). Unit pin
  `Parse.FlowSeqPairCollectionKeyEmptyValueKeepsNull`; seed
  `flow-seq-pair-empty-value.bin` (the nightly's run 39513390372,
  crash-ac4b705c).
- **parse: a node takes one anchor and one tag** — the property scan kept
  the LAST property, so `&va2: &node2 # comment` (a colon-bearing anchor
  name per 2SXE, then a second anchor) silently anchored the map below
  it, while libyaml read an empty anchored pair; the line-shape fast arm
  likewise read a follower `&` as plain text (`a: &x &y b`). Both are
  the suite's 4JVG ("Scalar value with two anchors", fail) error now.
  Unit pin `Parse.NodeCarriesOneAnchorAndOneTag`; seed
  `double-anchor-props.bin` (the nightly's run 39506629301,
  crash-896ac2df).
- **feed: the flow pre-scan defers a depth overflow** — a flow that
  exceeds the depth guard errored in the pre-scan BEFORE the flow's open
  events in the whole parse (2 events), while the chunked feed had
  already streamed them (4). The pre-scan now takes the NOT-A-KEY
  deferral (the unterminated-quote law); the real parse streams the open
  events and then fails at the same depth. The depth-guard contract is
  unchanged (nightly run 39502647277, crash-cf503577; seeds
  `deep-flow-prescan-depth.bin`).
- **parse: `%TAG` validates its handle and prefix** — the handle was
  taken verbatim, so `%TAG !' 2` registered and the whole parse accepted
  a directive libyaml rejects; and a `#` glued to the prefix was read as
  a comment, letting `%TAG !m! -#…` through and splitting the
  whole/chunked verdicts (the nightly's run 39497331520,
  crash-9052f1ea). The handle must now be `!`, `!!`, or `!word!`
  (word = alnum/-), a prefix is required, and a `#` inside the prefix
  token is a BAD_DIRECTIVE (libyaml's whitespace rule). Pinned in
  `Parse.TagDirectiveHandleAndPrefixValidated`; seeds
  `bad-tag-directive-split.bin`.
- **fuzz-nightly: the ASan harnesses ride a 32 MB stack** — ASan's parser
  frames are ~8 KB per nesting level, so a 762-level input overflowed the
  runner's 8 MB soft stack before the 1000-level depth guard could reject
  it (the nightly's run 39490412519, crash-542ee272; Release errors at
  762:3, the guard contract is unchanged). The harness steps raise the
  stack; seed `deep-value-chain.bin`.
- **fuzz_diff: the glued-header-comment carry covers header indicators** —
  `|+#~ c: d` glues `#` to the chomping indicator. libyaml reads the
  header + comment (the spec's separation may be empty) and yields an
  empty scalar, while yeptris's comment law requires a preceding blank
  (the line is plain content). The catch-632 comparator now locates the
  `#` after any chomping/indent indicators and spaces it out before
  comparing (the nightly's run 39478141022, crash-89bff209; seed
  `header-indicator-glued-comment.bin`).
- **fuzz_diff: carry the flow explicit-key colon family** — a flow
  explicit key starting with `:` adjacent to a non-space
  (`{?` + newline + `:cic l}`) is a PLAIN scalar per YAML 1.2
  `ns-plain-first`, which the suite pins (HM87 `[:x]` -> `=VAL ::x`;
  58MP `{x: :x}` -> `=VAL ::x`). libyaml's scanner makes a VALUE token
  of the leading `:` (empty key plus the value `cic l`). The new
  comparator folds libyaml's empty-key/value pair into yeptris's `:X`
  key line (the nightly's run 39469989528, crash-575cf6ed; seed
  `flow-q-colon-key.bin`).
- **feed: a directive line split from its `---` keeps the document's
  start line** — `doc_begin` is a buffer OFFSET, reset at every stepped
  run, so a step cut landing between a `%TAG` line and its `---` made
  the chunked DOCUMENT_START fall back to the marker line (whole 85:1
  vs chunked 86:1; the nightly's run 39460938869, crash-d29fe7c0,
  hand-shrunk 6707 -> 506 bytes). The engine also carries the
  directive's stream-absolute LINE across runs now, and `DOCUMENT_START`
  uses it when the offset is gone. Pinned in
  `Stepping.DirectiveLineAndMarkerAcrossAChunkCut` and the
  `directive-marker-cut-split.bin` seeds.
- **fuzz_diff: carry the flow-'?' quoted-follower composition** — libyaml
  makes a KEY token for a flow `?` with any follower (`{?"a": 1}` reads
  key `a`), while YAML 1.2's `ns-plain-first` admits `?"a"` as a plain
  scalar (`"` is `ns-plain-safe` in flow); the suite pins the plain
  reading for plain followers (HM87/652Z). The new comparator spaces
  the `?` out of the input and requires libyaml's dump to be unchanged,
  proving the divergence is only the key indicator (the nightly's run
  39452709881, crash-4490f97c; seed `flow-q-quoted-follower.bin`).
- **feed: a block-header lookalike key no longer hides its flow from
  the cut scanner** — `| : [` is a plain mapping key whose value opens
  a flow, but `step_scan` read the line as a block-scalar header,
  skipped the `[`, and cut at the following `---` while the flow was
  still open. The whole parse then buffered the flow to the depth cap
  while the chunked parse errored at 2:1, one event apart (fuzz_feed's
  identity invariant; #645, `depth-cap-marker-cut-split.bin`). The
  header is now validated against its grammar — indicators,
  separation, optional comment, then EOL — before the line is
  skipped. Pinned in `Stepping.BlockHeaderLookalikeKeyDoesNotHideFlow`
  (every split).
- **parse: a flow collection after an empty block value is a KEY, not
  the value** — `a:\n{----}: 1` attached the flow mapping as `a`'s
  value and then failed on its key colon, emitting the wrong tree
  (`{"a": {"----": null}, "1": null}`; libyaml reads `{"a": null,
  {"----": null} => 1}`; the nightly's run 39432839624,
  crash-c0169e98). The flow-at-the-parent-column arm now probes
  whether the collection is followed by a same-line `:` and closes
  the pending value empty first. Pinned in
  `Parse.FlowKeyAfterEmptyValueClosesThePair` (flow-map and flow-seq
  keys, the anchored-empty-value variant, and the `k:\n[a]`
  value-form regression).
- **quoted scalars: an escaped line break keeps the breaks after it** —
  the continuation path consumed the escaped break, then folded the
  remaining run with the plain N-1 rule, so `"a\` + two breaks + `b"`
  produced one newline too few (`" a\n\n\n:b"` instead of `" a\n\n\n\n:b"`).
  libyaml and libfyaml agree: the escaped break is removed, but every
  following break — whitespace-only lines included — is one newline,
  and spaces before the backslash stay content. Pinned in
  `Parse.QuotedEscapedBreakCountsFollowingBreaks` (the nightly's run
  39391528633, crash-3b662f50).
- **fuzz_diff: carry the composed content-BOM x break family** — a
  content U+FEFF riding the encoding mark and LS/PS normalization were
  each carried alone, but not their composition: libyaml drops the
  BOM (a line-start BOM is skipped) while yeptris keeps it as content,
  so the break after it folds to a space in yeptris and is a leading
  break in libyaml (crash-feee50a2: `fe ff fe ff 00 0a 2d 00`;
  crash-52fe8bd6: the same plus U+FFF9 and PS PS). The new comparator
  rebuilds the input (UTF-16 decoded, the leading content BOM dropped,
  LS/PS -> `\n`) and requires both engines to agree on it.
- **fuzz_diff: carry the root block scalar's column-0 content** —
  content at column 0 under a column-0 `|`/`>` header is content
  (suite W4TN, M7A3, T26H, DWX9, FP8R, DK3J), but libyaml ends the
  scalar at the first unindented line and reports an empty value
  (crash-5caf49d3: `|-` + `##`). The comparator indents the column-0
  content lines (changing no content byte) and requires libyaml's dump
  of that to equal yeptris's dump of the original.
- **emitter: a long single-quoted key rides the explicit form** — the
  simple-key length estimate counted an apostrophe once, but the
  single-quoted render doubles it, so a 900-byte key carrying 140
  quotes wrote 1044 bytes and the emission could not re-parse
  ("simple key exceeds 1024 characters"; fuzz_roundtrip's
  crash-b8a18e43, the nightly's run 39421661425). The estimator now
  treats `'` as expandable, promoting the key to the explicit
  `? key` form like the other over-limit families. Pinned in
  `Parse.SingleQuotedLongKeyRidesTheExplicitForm`.

## [0.6.34] - 2026-10-20
### Fixed
- **JSON floats carry the json gem's byte shape** — the JSON writers
  (`yeptris_serialize_json[_ex]`, `serialize_json_compact/pretty`)
  re-printed `YEPTRIS_TAG_FLOAT` scalars through the shortest
  printer, emitting the YAML/Psych shape (`1.0e-07`, `1.0e+20`)
  where the json gem (fpconv, json >= 2.10) writes
  `0.0000001`/`1e+20` — so every stdlib-generated document with an
  exponent-range float failed byte comparison (yeptris#651; the gate
  for lutaml/canon's `format_json` lane). The canonical-scalar JSON
  route now prints through the vendored fpconv (`yep_d2s_json`,
  grisu2 digits + fpconv emit windows, Boost license attribution
  preserved) — grisu2 is not always-shortest, so the clean-room
  printer cannot be reshaped to match. YAML emission is unchanged
  (shortest shape, Psych parity). Non-finite JSON spelling
  (`.inf` vs the gem's `Infinity` under allow_nan) stays a separate
  family. Pinned in `EmitJson.FloatByteShape` (windows, subnormals,
  and the byte-stable reparse property).
- **fuzz_diff: carry the glued-indicator-comment family** — a line
  starting with `>` or `|` IMMEDIATELY followed by `#` (the
  nightly's run 39258457271, crash-d4bb362b): libyaml reads header +
  comment (the spec's s-separate-in-line may be empty) and yields an
  empty scalar; yeptris's comment law requires a preceding blank, so
  the line is plain content. The suite has no glued-indicator test;
  the comparator spaces the glue out and requires libyaml's dump to
  be byte-identical (its reading was never space-sensitive).
- **fuzz_diff: the LS/PS family's UTF-16 twin** — through the BOM
  transcode front-end the raw bytes never contain the UTF-8 LS/PS
  triple, so the break-normalization comparator missed the family
  (the nightly's run 39258457271, crash-bad91ebf: "fe ff 2d 3a 20 28
  ..." — UTF-16BE whose U+2028 is the 20 28 unit). The comparator
  now decodes BOM'd UTF-16 to UTF-8 with LS/PS substituted and
  applies the same transform law.
- **the tagged-quoted-key props-only walk** — the props-only pre-walk
  under a pending key walked straight through an opening quote (`"` is
  not a flow indicator), read the quoted key's interior ` #` as a
  comment and rejected the keyed pair (`x:\n!t "a #b": y`) — so the
  emitter's own output for the crasher's map failed yeptris's
  re-parse (the nightly's run 39223056891, crash-7987a601). Quotes
  now break the walk as content; libyaml accepts the shape. Pinned in
  `Parse.TaggedQuotedKeyWithSpaceHash`.

## [0.6.33] - 2026-10-19
### Fixed
Post-0.6.32 fuzz catches and the tag-props family (each pinned;
crashers in the regression corpus).
- **fuzz_diff: carry the anchor-name-charset family** — the suite
  keeps ':' INSIDE anchor names (W5VH "&:@*!\"<foo>:", 2SXE
  "&a: key") and yeptris follows; libyaml stops the name at the
  colon ("&b:a!b:a!c" anchors 'b' and reads the rest as the value).
  libyaml is behind the suite; the family rides a depth-0 mini-scan
  comparator (&-names containing ':' at block level, quotes and
  alias/tag chains excluded).
- **fuzz_diff: carry the flow-anchor-colon family** — inside a flow
  collection libyaml reads "&name:" as an anchored empty key
  ("[&a: x]" -> {anchored-empty => x}); yeptris's designed lenience
  keeps "&" as plain content in flow (anchors ride block and refuse
  flow). The suite has zero flow-&-colon cases, so neither side is
  suite-backed; the family rides a depth-tracking mini-scan
  comparator (quotes, aliases, and prop chains excluded).
- **fuzz_diff: carry the breakless-EOF clip family** — libyaml drops
  clip chomping's trailing break when the input's last line carries
  none ("--- >\n fo" is "fo"), but the suite pins the break's
  presence with no final break in the source (L24T#2 "Trailing line
  of spaces", "x\n \n"); the suite is the conformance bar, so
  yeptris keeps it and the family rides a comparator: libyaml
  re-dumped on the input with '\n' appended must equal yeptris's
  dump exactly.
- **feed: the chunk cut scanner counts the flow '?' map** — the
  scanner's flow depth tracked only brackets, but a flow '?'
  opens the explicit-key map the engine stays inside: the scanner
  believed the flow closed and cut at a '---' the engine read as
  flow content, and the lone '-' at the chunk edge then errored
  with a different trailing record than the whole parse
  ("&a[? b '[a']?\r---\n", the nightly's run 39091724384). The
  scanner now counts the flow '?'; over-counting only suppresses
  cuts (the safe direction).
- **tape: the second-root dedent law survives a pending root value** —
  a root pair whose value container follows (" k: &a" → the pending
  arm) never recorded the root's pair column, so a col-0 later line
  read as "root pairs at column 0" and the tape accepted the second
  root the engine (and Psych) reject; the walk now tracks
  root_pair_col (the first root pair's own column) beside root_open
  and rejects any dedent below it. content_col itself stays
  untouched: arming it in the pending arm dissolved the " k: \n
  j: w" siblings into four root scalars (the next nightly's
  crash-753ce4c1).
- **parse: a bare flow collection key carries its null value** —
  "{{}}" parsed as a pair whose value vanished; libyaml/Psych treat a
  collection closing with no ':' as an implicit key and emit the
  null ("{{}}" is { {}: null }, "{[a], z: 1}" is {[a]: null,
  z: 1}). A key-candidacy frame flag recorded at open time now feeds
  both the ',' and the close boundary, which synthesize the implicit
  scalar (value-position collections are untouched).
- **parse: flow plain folding counts every break** — the flow-plain
  continuation loop hardcoded one break per piece, so blank lines
  folded like a single break ("[0\r\r)]" parsed as "0 )" where the
  plain law folds 1 break to a space and n>1 breaks to n-1 newlines,
  "0\n)", matching Psych); the loop now counts every break up to the
  next piece, trailing spaces of the piece's own line included.
- **parse: "-:" is a plain key, not explicit-key content** — the
  '?'-fix's dash carve-out only split on the first byte; a dash with
  a non-space follower is a plain-key pair, so "?\n-:" and the CR
  twin kept losing the {null: null} pair. Dash+space seq entries stay
  content (the catch-38154662070 shape).
- **parse: a '?'-led line at the key's own column is never explicit-
  key content** — the continuation swallowed it as a folded piece and
  the pending pair lost its null value ("?\n?:" and the CR twin
  parsed as one pair where libyaml yields {null: null} plus
  {"?": null}); the content now ends and the main loop re-dispatches.
- **engine: the UTF-16/32 transcode scratch is no longer freed under
  the run that uses it** — engine_run_impl reset `enc_buf` at entry,
  but the front-end moved above it (the catch-117 reshuffle) and
  hands it the very buffer the run then parses: every transcoded
  input was a use-after-free (ASan, 4-byte artifact `ff fe fe fe`).
  The scratch is front-end-owned now; only `norm_buf` stays per-run.
- **parse: a comment line ends a plain without folding** — a `#`
  opening a continuation line went through the plain scan as an empty
  piece that still took a fold slot, so the fold's break space rode
  into the value ("a\n#" parsed as "a " where libyaml ends the
  scalar at the comment).
- **parse: a break inside a comment arms the line-start state** — the
  comment-internal NEL/LS/PS branch ended the comment but left
  `at_break` disarmed, so the post-break '#' read as content and a
  closer inside it closed the scan's flow while the engine's stayed
  open; the next `---` cut landed inside that flow.
- **fuzz_diff: the UTF-16 content-BOM family is a documented
  divergence** — libyaml's UTF-16 reader drops a U+FEFF following the
  encoding mark while its own UTF-8 path keeps it; yeptris is
  consistent across encodings (one mark stripped, content BOMs ride —
  the catch-121 law) and the differential now recognizes the shape.
- **fuzz_diff: the flow-'?' family is a documented divergence** —
  pinned libyaml makes a KEY token for a flow '?' with any follower
  ("[?p]" -> {p: null}) while yaml-test-suite pins the plain reading
  (HM87/652Z) and yeptris follows the suite; the differential now
  recognizes exactly this shape instead of failing the round.
- **parse: keep-chomping keeps complete blank lines only** — a final
  blank line with no break at EOF contributed a phantom kept break
  ("|+\n " parsed as "\n" where libyaml ends the scalar empty);
  complete blanks still keep their breaks ("|+\n\n\n" is "\n\n",
  oracle-checked).
- **numbers: the sexagesimal fold carries the ERANGE law** — every
  fold step (the first component's ×10, the ×3600 weight, the ×60
  group and the adds) overflowed `long long` unchecked where the
  plain path rejects unrepresentable values via strtoll; UBSan caught
  a 52-digit component on the combined-build nightly, and the fold
  now returns not-an-int like everything else. (The catch-124 fix
  guarded the hours loop but left the GROUP component's ×10 raw —
  caught one round later.)
- **parse: (NULL, 0) is an empty range, not NULL + 0** — the anchor
  pre-count computed `data + data_len` on a NULL buffer, which is UB
  by the standard even for a zero offset; UBSan caught it on the
  seed corpus in the first nightly under the combined ASan+UBSan fuzz
  build (the teptris shape doing its job).
- **engine: the fused walk's nested flow opener marks the opener** —
  the nested start rode the old +1 (one past the indicator) while the
  outer opener and the general kernel mark the opener itself; a ≥24-
  byte whole span routes fused while its <24-byte chunked prefix
  routes general, so `[[]` disagreed by one column.
- **engine: a lone UTF-8 BOM is an empty stream** — the encoding
  front-end exempted UTF-8 heads, so the raw entries (push, the
  libyaml differential) parsed a lone U+FEFF as a scalar where
  libyaml's reader ends the stream. The front-end now runs once per
  engine and strips any head BOM; pre-stripping callers (parse, the
  value drains) mark it done, so mid-stream U+FEFF content and
  chunked-run heads are untouched.
- **emit: a flow alias whose name ends in ':' is unserializable** —
  in flow a trailing ':' is the key/value separator, so the re-parse
  truncated the alias name and the block definition went unmatched
  ("alias references an undefined anchor"). Anchor names have no
  quoting: serialize now refuses such documents (the established
  unspellable-anchor law) instead of emitting a form that can never
  re-parse. Block spellings ride raw (2SXE/Y2GN).
- **feed: the chunk-cut scanner breaks on NEL/LS/PS** — the engine
  normalizes those breaks to \n before the parse, but the scan walked
  raw bytes with only \n/CR checks, and its 0xC2/0xE2 compares were
  dead on a signed char; a comment after a multibyte break went
  unseen and the cut landed inside a flow the engine still had open
  (whole vs chunked disagreed). The scan breaks identically and an
  `at_break` flag carries line-start across the multibyte gap.
- **transcode harness: re-encode the document, not the file bytes** —
  the equality property fed a leading UTF-8 BOM into the UTF-16/32
  re-encoding, where the encoding mark became a literal U+FEFF
  character and every variant differed from the base by construction.
  The engine's once-only BOM strip is correct; the harness now
  re-encodes the document content (run 38707248713).
- **tape: a col-0 tab-led continuation line inside a flow span defers
  to the engine** — a giant flow key pushed the doc past the lenient
  walk's 256-byte classify budget; the walk treated the tab-led line
  as flow separation while the marshal drain's engine applied the
  Y79Y#4 indentation law, so parse accepted and marshal refused
  ("tab character used as indentation"). The walk now defers
  newline+col-0-tab spans to the engine, whose law rejects them —
  the suite's verdict (run 39013934405).
- **engine: the encoding front-end rides yep_engine_run, and only for
  UTF-16/32 marks** — the libyaml differential's sem dump rides
  yep_engine_run directly (bypassing every public entry), so the
  lone-BOM family re-diverged after the push fix: the raw-engine
  consumers fed non-UTF-8 bytes to the scanner (whole +STR/=VAL/-STR
  vs libyaml +STR/-STR). The front-end (sniff, transcode,
  printable-validate) now lives in the engine itself, and it acts on
  UTF-16/32 marks only -- a UTF-8-encoded U+FEFF at the head may be
  content, and stripping it ate the document (the round-118 BOM pin).
- **emitter: the block map's explicit-key promotion consults the DQ
  predicate** — a tagged single-line block key whose content carries
  U+FEFF (or NEL/LS/PS/noncharacters) was promoted to the explicit
  "? key" form while sc_route demoted its render to double-quoted
  (the BOM re-parses as an encoding mark): round two parsed a QUOTED
  key and rode simple, one render behind (whole 20 bytes vs 16 on
  ":\n! |-\n \uFEFF" — run 39005989761). The promotion now skips
  DQ-demoting content via the shared sc_dq_forced scan.
- **engine: the directive name is validated** — libyaml's law is
  [0-9A-Za-z_-]+ followed by a blank or the line end; the lenient
  skip let "%]" ride as a legal directive line whose
  DOCUMENT_START inclusion (the leading-directive-lines mark)
  diverged across the step boundary: the whole parse marked doc 2 at
  the directive's line, the chunked at the '---' (whole 3:1 vs
  chunked 4:1 — run 39003631538). The catch-85 fixture's %Y'AML
  became %Y-AML for the same law.
- **engine: the fast path's key deferral skips tabs** — the check
  for a key-context colon after a flow collection skipped only
  spaces, so "[]\t\t: |-" committed the flow and then errored on the
  colon it could not read as a separator, while the sub-24-byte chunk
  run took the general kernel and parsed the pair (whole st=1 n=4 vs
  chunked st=1 n=9 — run 39001396706).
- **push: the encoding front-end rides yeptris_push_parse too** —
  parse.c and the value drains sniff the BOM and transcode UTF-16/32,
  but the push event route fed raw bytes to the engine: a lone
  UTF-16LE BOM parsed as a garbage scalar where libyaml's reader
  yields an empty stream (fuzz_diff: +STR/=VAL/-STR vs +STR/-STR —
  run 38997741246).
- **transcode harness: the equality property rides the BOM variants
  only** — a BOM-less UTF-16/32 byte run can parse as UTF-8 garbage by
  construction (a 1-char document's UTF-16LE bytes ARE a valid YAML
  scalar), so its outcome is unbounded; crash-safety is the only
  property there and the BOM variants carry the meaningful equality
  (run 38994713064).
- **pre-scan: a stray flow close defers instead of erroring** — the
  flow-span pre-scan's ']'/'}' handlers read their frame stack with no
  empty guard, so a stray close after a balanced flow read out of
  bounds and aborted the whole parse before a single event emitted,
  while the chunked feed had already committed the prefix runs (whole
  n=2 vs chunked n=4 on "{\"\":\"{\"}\n---\n\"]" — run 38992357043).
  A stray close or a kind mismatch defers to the general path.
- **feed: the cut scanner counts every brace and hoists the directive
  case out of its block branch** — the token-start gate could not
  separate a flow opener from a content brace ("k: {" — the ':'
  arrives with tok_start clear — put the cut inside the '---' marker;
  whole errored at 2:8, chunked at 2:1). The scan cannot make that
  call locally, so every opener counts (over-counting only suppresses
  cuts — a missed cut degrades toward whole-parse semantics, the
  invariant's safe direction) and the '%' case rides above the block
  branch, pairing a directive with its '---' regardless of the brace
  count (runs 38768645888, 38834514526, 38898618551, 38989832309).
- **feed: the cut scanner's opener gate also reads the after-property
  state** — an anchor/tag/alias name ends at a flow indicator, so
  "&b[" is a real flow open with tok_start clear: the gate blocked it,
  the scan stayed in block across the flow, and the chunk cut landed
  inside it (whole streamed 44 events before its error, chunked 12
  fewer). The gate reads word_led beside tok_start (run
  38898618551).
- **feed: the cut scanner's flow-opener gate reads tok_start before
  the token block consumes it** — the gate added for the mid-word
  brace read the flag after the token block had cleared it, so '['
  never opened flow in the scan: the chunk cut landed inside a flow
  (at the '---' marker) and the chunked error boundary sat a line
  above the whole parse's (run 38834514526).
- **feed: a brace mid-word no longer puts the cut scanner in flow** —
  the block plain scalar "U{" counted as a flow opener, the scan
  stayed in flow across the following directive line (whose marker
  logic lives in the block branch), and the chunk cut landed between
  the directive and its '---': the intermediate run rejected a legal
  multi-document stream ("invalid or unsupported directive") while the
  whole parse accepted, and the DOCUMENT_START mark lost the leading
  directive lines across the step boundary. The opener gate reads
  tok_start as of the byte's arrival (the separator block sets it for
  the same byte — run 38768645888).
- **flow simple-key law: the 1024 budget counts characters from the
  key's start to the colon, on every route** — the old raw-span checks
  measured bytes at the scalar and missed two whole families: a
  multibyte code point counts once (a 2-byte char at byte delta 1025
  is legal, exactly the nightly's round-108 shrink), and a key that
  follows a container value was never checked at all (the tape walkers
  left the map frame's key slot pinned after OPEN/CLOSE records). The
  law now lives at the colon in the fused pass-2 scanner and the
  general flow kernel, in the DOM JSON builder (which defers over-
  limit keys to the engine), and in both tape key walkers; all three
  routes answer identically and match Psych's boundary cell by cell
  (run 38713500461).
- **feed: an unterminated quote inside an unbalanced flow span defers
  in the pre-scan** — the pre-scan only decides the key question, but
  its quote branch errored on a bad quote BEFORE the flow's first
  events, so the whole buffer's stream lost the map open and its
  first entry while the chunked feed had already emitted theirs. The
  pre-scan answers NOT-A-KEY and the real parse streams then errors
  (run 38688200266).
- **parse: the "..." marker's line ends at a lone `\r`** — the doc-end
  marker's trailing-content check skipped `\r` as whitespace, so the
  next line's `---` read as junk on the marker's own line and the
  whole parse errored while the chunked feed split the lines
  correctly (run 38684686593).
- **feed: an unterminated flow's event streams now match across
  chunkings** — the whole parse's flow pre-scan errored BEFORE the
  flow's first events while the chunked feed had already streamed the
  map open and its first entry, so the two routes diverged at the
  error boundary. The pre-scan defers in both modes (the real parse
  errors on the unclosed span), and the flow plain fold ends at real
  document markers while directive-like '%' lines stay content
  mid-flow (XLQ9) (run 38678519554).
- **parse: a separating ':' directly before a quoted flow value** —
  the flow pre-scan's token-start rule required a blank or
  flow-indicator after ':', so `:"["` (valid JSON and YAML flow) read
  as mid-token: the value's quotes stopped being scanned as strings
  and its close quote surfaced as an unterminated opener. The lazy
  tape route accepted while the marshal route's kernel rejected — a
  parseable input failed `yeptris_marshal` (run 38673766448).
- **parse: a key indicator cannot be a pair's value; an odd child
  pairs with a null** — the main dispatch's ':' line parsed its value
  in the dash context, so ": ?" accepted a key indicator as node
  content, and stacked explicit-key pairs left an unpaired child the
  DOM dropped: an anchored node lost its anchor and the later alias
  emitted a dangling reference the re-parse rejected. The colon's own
  line is now marked (libyaml rejects ": ?"), a later-line same-depth
  pending explicit pair completes with a null value (libyaml's
  same-indent KEY rule), and the DOM pairs an odd child with a null
  instead of dropping it (run 38653909827).
- **parse: a flow explicit key with no ':' line keeps its key** — the
  flow walker dropped the parsed key event and emitted a fresh empty
  scalar in its slot: `[? x]` became `{"": ~}` (the key's content
  lost), `? &a` lost its anchor (a later backward alias then failed
  the placed-anchor check — catch 94's rejection was this bug in
  disguise), and `? [a]` / `? {b: 1}` as a seq entry leaked the
  collection as a plain entry or vanished outright as a map key. The
  parsed key rides the slot, and an explicit-key collection completes
  its pair with a null value (run 38645996340).
- **parse: an alias to a defined-but-unplaced anchor rejects** — the
  props scan registers an anchor the moment it is scanned, but its
  node may be placed later (a props anchor riding past other
  placements): the parse accepted the document while the emit
  re-ordered the anchor and the alias, and the re-parse rejected the
  output ("alias references an undefined anchor"). Anchors are now
  marked placed at every placement site (`emit_now`, the classified
  pair's fused placement, the flow commit's subtree range) and an
  alias to a defined-but-unplaced anchor is a forward reference —
  rejected like libyaml's "found undefined alias" (nightly runs
  38541356056 + 38590936636, catch 94; round 100's artifact rode in
  corpus/parse/).
- **parse: the fast path's single-line check treats any CR as
  multi-line** — the check memchr'd only for `\n`, so a `{\r\r\r\r}`
  span (valid JSON: `\r` is whitespace) skipped the per-event line
  walk entirely and the `MAP_END` marked on line 1, while the
  general kernel (and libyaml) counted four breaks (`MAP- 5:1`).
  A span carrying any `\r` now takes the walk (nightly run
  38585830922, catch 98).
- **parse: the fast path's line walk counts a lone CR at the walk
  boundary** — `yep_scan_advance_line` cannot peek past its walk end
  (`p[to]` — it does not know the buffer length), so a lone `\r`
  sitting exactly at the boundary never advanced the line: the JSON
  fast path's flow END marked one line low (`[\n  \r ]|.` → SEQ_END
  on line 2) while the chunked cut (the general kernel) and libyaml
  marked line 3. The engine's `jx_advance_line` wrapper corrects the
  boundary case with its own length (nightly run 38563005475,
  catch 97).
- **scan: the line walk counts a lone CR** — `yep_scan_advance_line`
  (the JSON fast path's and the DOM builder's shared line walk)
  counted only `\n`, so a lone `\r` inside a JSON-class flow
  (`[\n \r ]` is valid JSON: `\r` is JSON whitespace) never
  advanced the line: whole mode's event marks sat one line low while
  the chunked prefix (the general kernel, which walks `\r`) marked
  correctly — same bytes, two mark sets. The engine's backward
  mark walk aligns too (nightly run 38549939326, catch 96).
- **parse: a col-0 document marker ends a root block scalar even
  before any content** — `>\n--- ` whole-parsed as ONE document (the
  marker line became the block's content) while the chunked cut at
  the marker produced the correct two documents, and libyaml agrees
  with the chunked reading: the block is empty and `---` opens a
  second (empty) document. A %-directive line keeps its
  first-line-is-content rule (the suite's M7A3/W4TN blocks whose
  value is `%!PS-Adobe-2.0 ...`); only `---`/`...` are unconditional
  boundaries (nightly run 38544774709, catch 95).
- **parse: step_scan tracks flow-value quotes and verbatim tags** —
  the incremental cut scanner's token-start rule after a plain-led
  key left the quote at a flow VALUE position untracked, so the cut
  landed inside the quoted span (`{f '}` + `---` + `'}'`: whole saw
  the far close and swallowed the `---` marker, chunked errored on
  the unterminated prefix); and a cut inside a `!` verbatim tag
  (`{ !< }` + `---`) made the partial prefix's flow pre-scan fail
  before `MAP_START` emitted while the whole buffer's succeeded. In a
  flow a space now always re-arms the token start, `!` verbatim tags
  are tracked (and a possibly-truncated `!` holds the tail), and the
  flow pre-scan defers unclosed collections on partial feeds while
  complete-buffer errors stay (nightly runs 38530395946 and
  38534965021, catches 92 and 93).
- **parse: the break normalizer keeps the input's own length** — the
  NEL/LS/PS copy is null-terminated and the run measured it with
  `strlen`, so an input carrying a NUL byte of its own was truncated
  at the first one: `\0\n---\r<U+2028>` whole-parsed as an empty
  stream (the line separator triggered the copy, the NUL cut it)
  while a chunked feed cut before the separator and never copied —
  two different streams for the same bytes (nightly run 38524928004,
  catch 91). The copied length is the normalizer's own count now.
- **parse: the flow key lookahead restores the line state with the
  cursor** — the key detection's whitespace lookahead crosses the
  newline after a flow value hunting for a `:`, and the no-colon
  rollback put the cursor back but left `e->line` one high: a
  multi-line flow collection's END (and every mark after it) parsed
  one line low in any chunked feed, while whole feeds took the JSON
  fast path (its own line walk) and never noticed — whole and chunked
  streams disagreed on the same bytes (nightly run 38518507064,
  catch 90).
- **parse: the JSON fast paths are byte-for-byte JSON** — the flow
  walk's lenient mode accepted number and literal keys, so a whole
  parse of `{9:1}` rode the fast path and emitted `{9 -> 1}` while a
  chunked feed fell under the small-span gate to the general kernel,
  which correctly reads `9:1` as one plain scalar (`{"9:1" -> null}`):
  the same bytes produced two different streams depending on how the
  input was split. The engine's pass-1 walk, the DOM fused builder,
  and the ytape probe all walk strict now — non-JSON spans fall back
  to the general kernel's colon rules in every mode (nightly run
  38513849602, catch 89).
- **parse: nested explicit-key pairs flush their nulls at their own
  levels** — `? ? k : a` stacked two pending pair values over a single
  depth slot: the inner pair clobbered the outer's level, the outer
  null flushed inside the wrong map, and a following anchored value
  lost its anchor — the trailing alias emitted a dangling reference
  and the emitted document failed to re-parse. The pending levels are
  a stack now; every `key: value` completion funnels through one
  wrapper that resolves the innermost pending, and each null flushes
  directly inside its own `?` map (matching the libyaml oracle
  `{{{"k" => "a"} => nil} => nil}`; nightly run 38387614146, catch 88).
- **parse: a second props line cannot displace a pending anchor** —
  a props-only line followed by another props line carrying its own
  anchor ("&a\n&a1\nF: *a", or the '? &a' explicit-key twin) let the
  same-line anchor win the merge, so the displaced anchor stayed
  defined in the nametab while never appearing in the output: the
  trailing alias emitted a dangling reference and the emitted document
  failed to re-parse. Every nesting variant is rejected now, matching
  libyaml ("did not find expected <document start>" / "did not find
  expected key"); a TAG-only continuation ("&a\n!!str v") stays legal
  (nightly run 38271222289, catch 78).
- **parse: a props-only line at a pair position rejects** — a line
  carrying only properties (anchor, tag, or both) where the engine
  expected a mapping key ("k: 1\n!!t: &b\n  n: m") was consumed as
  node props: the pair's key event vanished, the key's tag leaked onto
  the value node, and a long-tag value then lost its anchor at emit —
  the trailing alias emitted a dangling reference. libyaml rejects
  every such shape ("could not find expected ':' while scanning a
  simple key"); node-position props lines stay legal, including the
  first pair of a fresh map (nightly run 38275208009, catch 79).
- **parse: the flow floor covers the closer line on every route** —
  the JSON fast path's floor scan stopped at the flow's close, so the
  closer's own line escaped the suite-strict 9C9N floor whenever the
  span took the fast path, while short spans (chunk cuts, small
  documents) took the general kernel and rejected: whole vs chunked
  feeds disagreed over the same bytes ("k: {\n\n} \n---\nk2: v"
  parsed whole and failed chunked). The fast scan now covers the
  closer line. The key pre-scan also restores its line state with the
  cursor, so a multi-line flow's open event marks at the '{' on the
  slow route too (it marked at the pre-scan's far line, diverging
  from the fast path's mark) (nightly run 38280372447, catch 80).
- **parse: the '?' / ':' pair state resets at document boundaries** —
  doc 4's flow set q_key_pending and the '---' boundary left it set,
  so doc 5's bare ':' skipped its empty-key event: the whole-buffer
  stream lost a record the chunked feed (which rebuilds per cut)
  emitted — transport changed the event stream. The boundary clears
  the pair state now, and the bare ':' key event carries the ':' mark
  instead of a zeroed one (nightly run 38287390219, catch 81).
- **feed: the step scan holds block-scalar state across cuts** — the
  scan had no '>' / '|' notion: a cut inside an open fold changed the
  scalar's value or split it into phantom documents (">\n---\n"
  whole vs chunked), and fold content like "'* bullet" poisoned the
  scanner's quote tracking so later cuts landed mid-fold. The scan now
  consumes a fold's lines wholesale — they cannot poison quote, flow,
  or comment state — and cuts only after the reader's own fold-end
  rule: a dedented line that is not blank, not a comment, not a
  marker; truncated marker fragments wait for the next chunk (nightly
  run 38291177166, catch 82).
- **feed: doc-inline block scalars hold the cut too** — the scan's
  header check only fired at line starts, so "--- |" left the fold
  untracked: the fold's flow-looking content poisoned the scanner's
  quote/flow state and the split diverged. The marker arm opens the
  state now, and doc-inline folds follow the root rule — only a later
  marker ends them; col-0 lines are content (nightly run 38352718743,
  catch 83).
- **parse: node props cannot precede a second '?' on the same line** —
  "? &a ? b" opened the nested key map with the props dropped: the
  displaced anchor stayed defined in the nametab while never emitting,
  so a later alias emitted a dangling reference and the round-trip
  failed. libyaml: "mapping keys are not allowed in this context";
  the cross-line twin ("? &a\n  ? b") composes, and the props-less
  "? ? b" stays a legal nested key (nightly run 38355778230,
  catch 84).
- **feed: the step scan tracks quotes at word starts** — a quote byte
  after any indicator char opened phantom quote state, and plain
  content like "A!'- %YAML" carried it across the "--- |" fold
  marker: the fold went untracked and a later cut orphaned the
  "%YAML" directive from its "---", shifting the document-start mark
  by a line. Quotes now open only at a word start; a word that began
  with an indicator still admits a quoted node after it (nightly run
  38359507422, catch 85).
- **feed: flow separators re-arm the word-start quote rule** — the
  quote in "{\"O\" ..." sits adjacent to the '{' with no space, so
  the word-start rule (catch 85) left it untracked: the scan counted
  the ']' inside the quoted scalar as closing the flow and cut at a
  '---' the engine never saw outside the quote — the chunked feed died
  on an unterminated quoted scalar the whole parse had closed. ','
  ':' '[' ']' '{' '}' re-arm tok_start immediately (nightly run
  38366643713, catch 86).
- **emit: the explicit pair's value keeps its anchor** — the block
  map's explicit-key arm wrote "? key" then ": " and emitted the value
  directly, skipping the simple arm's props-tail + fresh-line path for
  a nested block collection: an anchored map value lost its anchor (or
  glued it to the first key, "&basename"), and a later alias emitted a
  dangling reference the re-parse rejected. The explicit form now
  mirrors the simple arm exactly (nightly run 38371134997, catch 87).
- **emit: the core shorthand's suffix must re-scan** — a core-URI
  tag whose suffix carries a byte outside the parse side's tag charset
  (ours terminates on ',' — the lenience vs libyaml's URI set)
  re-parsed truncated when emitted `!!<suffix>`, killing the
  round-trip (nightly run 38132505901, the verbatim tag with an
  interior ','). Those fall back to the verbatim `!<...>` wrap; clean
  suffixes keep the `!!` form.
- **emit: the core namespace rides the !! secondary handle** —
  `tag:yaml.org,2002:X` tags emit as `!!X` at every tag site (scalar,
  collection line-tail, root props), matching libyaml's default-handle
  behavior instead of the `!<tag:yaml.org,2002:X>` verbatim wrap. A
  node tagged with the verbatim shorthand "!!str" collides with a
  LOCAL tag of the same bytes and rode the catch-27 %21 escape — the
  binding's chevron-key dump emitted `!%21str '<<': *1` (a local tag)
  and the key re-loaded as a merge, breaking the round-trip that
  v0.6.32's `!!str '<<'` form carried. Hosts pass the full core URI;
  local bang-bearing tags keep the %21 escape.
- **parse: the flat-run's key honors plain-first** (#513) — the
  line-head bail listed '[' but not the plain-first law; a ']'
  sibling key parsed while the marshal rejected.
- **parse: \\U escapes stay under U+10FFFF** (#514) — an 8-digit
  escape can name unencodable code points; the nightly's
  "\\UA66DA66D" parsed and never re-parsed. Psych rejects above the
  boundary.
- **emit: flow plains carrying quotes take the quoted route** — a
  plain scalar with '"' or '\\' rode plain inside a flow map, where
  the bytes flip the scanner into quoted/escape mode ("unterminated
  quoted scalar"; nightly run 38107485738, `? !str a"\u1: -…`). The
  catch-25 flow gate covers them.
- **emit: a null flow-seq item rides '~'** — the item position
  emitted as nothing, so "[, x]" read as a leading comma and neither
  libyaml nor Psych re-parse it (nightly run 38106246351,
  `? - \n  - 2002-07-*2Yon`). Flow-map values keep the bare form
  (`{k: }` is valid).
- **parse: the repeat-alias memo resets per document** — anchors are
  per-document, but the alias memo (the merge-key fast path)
  answered doc 2's identical alias name from doc 1's memoized
  ordinal across the boundary: the whole-buffer parse resolved the
  alias while the chunked feed (which rebuilds the engine per cut)
  correctly rejected it — transport changed semantics. The memo
  invalidates at every anchor-table reset (nightly run 38266487549,
  catch 77).
- **parse: a sibling pair flushes the buffered explicit key** — a
  same-column plain-start pair line under an explicit key was
  consumed as the key's content, stranding the buffered "? &a" key
  inside it: the trailing alias emitted a dangling reference to the
  dropped anchor and the output never re-parsed. The continuation
  stops at such pairs (dashes keep the old path — a block seq is the
  key's content); the null value flushes, the pair proceeds, and the
  anchor emits (issue #570, nightly run 38261610465, catch 75).
- **emit: a leading BOM content key rides quoted** — the plain-safety
  predicate let a LEADING U+FEFF ride raw (a UTF-16 double-BOM
  transcodes to one as CONTENT); the re-parse consumed it as a
  document BOM and the key became empty, every round shedding it.
  Only its escape ("\uFEFF") is byte-stable; mid-token BOMs ride
  plain (nightly run 38258674337, catch 74).
- **emit: flow plains quote the separators** — a plain carrying ": "
  rode plain inside a flow-rendered pair, and the second ':' died at
  the completed-pair law ("{a: b : c}"); the gate gains ':' and its
  explicit-key twin '?' (nightly run 38255134508, catch 73).
- **emit: flow plains quote the anchor indicators** — a plain
  carrying '&' (or its alias twin '*') rode plain inside a
  flow-rendered explicit key, the '&' opened an anchor scan and the
  re-parse died ("did not find expected alphabetic or numeric
  character while scanning an anchor"). The flow-context DQ gate
  gains both bytes; the suite's M2N8 parse is untouched (nightly run
  38250474732, catch 72).
- **parse: a props-only line under an explicit key rejects** —
  continuing "? &a\n&F" absorbed the new props INTO the key and
  DROPPED the key's own anchor, so an alias referencing it emitted a
  dangling reference that never re-parsed. libyaml rejects the shape
  ("could not find expected ':' while scanning a simple key"); the
  props-only-line law now covers the explicit-key context too, and
  the legal explicit-key shapes ride (nightly run 38207965687,
  catch 71).
- **emit: a key's anchor counts toward the written-token law** — the
  estimate counted the key's tag (catch 63) but not its anchor;
  anchor names carry interior colons (Y2GN), so a ~1700-byte name
  rode a 2-byte key past the re-parse's 1024 law and the emission
  never re-parsed. Over-limit anchored keys now ride the explicit
  form (nightly run 38206305189, catch 70).
- **parse: a mid-token '#' is content in prop lines** — the props-line
  classifier ended the scan at any '#', so "&a ?#:" read as
  props-plus-comment (only_props) and failed, while libyaml parses it
  — and the emitter's own re-spelling of such keys was the form our
  parser rejected. '#' opens a comment only after a blank; mid-token
  it rides the plain (nightly run 38204470906, catch 69).
- **parse: a deep anchored pending rides the engine** — the fused
  runner's anchored-pending deferral only flushes its records at
  depth 1; a pending resolved into an anchored map BELOW the root
  frame dropped its pairs at EOF ("4:\n4: r\n5:\n &5\n 5: e" lost
  the anchored map and every round shed bytes). Deeper anchored
  pendings now bail to the engine, which resolves them; the depth-1
  anchored shape keeps the fused path (nightly run 38202382251,
  catch 68).
- **emit: a tagged EMPTY key counts its tag** — the written-token
  estimate early-returned on an empty key value before the tag arm,
  so a tagged empty key wrote ~2300 bytes of escaped tag as a simple
  key and died at the re-parse's 1024 law. The tag arm now runs for
  empty keys too; over-limit tagged keys ride the explicit form
  (nightly run 38200627503, catch 67).
- **emit: only single-line tagged block keys take the explicit form**
  — the clip-chomp twin of catch 64's key (":\n! |\n r\n:") keeps its
  trailing break, so the style chooser demotes it to quoted even in
  the explicit path (as_key): promoting it made round one explicit
  and round two simple — every round differed. Multiline tagged block
  keys now ride the simple form in both rounds, stably; single-line
  tagged ones keep the catch-64 explicit form (nightly run
  38181819580, catch 66).
- **parse: a dedented pair closes the child map's column** — after a
  pending key resolved into its child map, the frame kept the CHILD's
  content column: a dedented root pair closed the child's phantom map
  but the next more-indented line rode the stale frame as a phantom
  sibling — the runner accepted what the engine and libyaml reject
  ("mapping values are not allowed in this context") and parse and
  marshal disagreed. Off-column pairs on resolved frames now bail to
  the engine, which owns both the error and the legit dedent; the
  common `k:\n  nested: v` / dedent / same-column shapes ride the
  fused path unchanged (nightly run 38179552341, catch 65).
- **emit: a tagged block key rides the explicit form; explicit-key
  props emit once** — a TAGGED literal/folded key keeps the block
  form in the style chooser (the #290 law), so as a map key the value
  separator glued onto the block's content line (colons are data
  there): the re-parse read a different tree and every round shed
  bytes. Such keys now take the `? key` form, which keeps the block
  form — both rounds agree. Untagged/empty block styles demote to
  quoted/plain and ride simple stably (the suite's 5WE3 unchanged).
  Along the way: the explicit-key writer pre-emitted the key's props
  AND let the node re-emit them ("? !" + "! |-...") — the catch-56
  law, explicit-key side: the DQ-promoted path keeps its pre-emit,
  everything else carries tag+anchor inline (nightly run 38177622733,
  catch 64).
- **emit: a key's tag counts toward the written-token law** — the
  simple-key length check estimated the quoted key's written size but
  ignored the tag riding the same token; wr_tag_uri escapes every
  byte outside its URI set as %XX (even `_`), so a ~975-raw-byte tag
  wrote ~1030 and the emission died at the re-parse's 1024 law
  ("simple key exceeds 1024"). Over-limit tagged keys now ride the
  explicit form; short-tagged keys stay simple (nightly run
  38176016091, catch 63).
- **step: fused flow openers carry the general kernel's marks** —
  the fused flow walk (spans >= 24 bytes) emitted collection opens
  one column right of the general kernel, so a tiny document cut at
  a `---` boundary (general kernel) and the same bytes parsed whole
  (fused) disagreed on the open's column. The fused pass-2 now
  mirrors the general kernel exactly (nightly run 38173499680,
  catch 62 — fuzz_feed's whole-vs-chunked invariant).
- **parse: the flow pre-scan reads tokens, not bytes** — a quote
  opened a quoted scalar wherever it appeared, so a tag suffix or a
  plain carrying one (`[!'str b]`, `[a'b]`) died as "unterminated
  quoted scalar" before the real scan ran, and the emitted form of
  such documents never re-parsed. Quotes now open only at token
  starts (libyaml accepts both shapes; nightly run 38171171360,
  catch 61).
- **parse: anchors and aliases have no quoting in flow** — a quote
  byte in a flow anchor/alias name rejects (Psych: "did not find
  expected alphabetic or numeric character"). Block names keep
  riding quotes — the suite's W5VH anchor `&:@*!$"<foo>:` is
  untouched.
- **parse: a completed flow pair owns its value slot** — `[s: 2:]`,
  `[a: b: c]`, `[s: : v]` opened a second single-pair wrapper inside
  the first pair's value, unbalancing the event stream (the DOM then
  silently dropped the `2`). Every completed-pair-plus-`:` shape now
  rejects, matching libyaml; the legal `[a: b, c: d]` / `[s: , x]`
  shapes are untouched (nightly run 38168927841, catch 60).
- **emit: a block-styled collection nested in a flow member renders
  flow** — the writer honored the node's parsed block form even
  inside brackets, writing `- ` where only node content is legal.
  The catch-55 law, collection arm: `w->in_flow` rides the dispatch.
- **emit: root props emit once** — the root-collection props rode
  their own leading line AND the empty collection re-emitted them
  inline (`! &a\n! &a {}`), so every round shed a line (nightly run
  38167237445, `! &a\n  ? &e...`). The catch-56 law, root side: an
  empty propertied collection takes the inline empty-form.
- **emit: the local shorthand's suffix must re-scan** — the catch-47
  law, on the local arm: a `!suffix` tag whose suffix carries a byte
  the parse side's charset terminates on (ours: ',') wrote it raw,
  re-parsed truncated, and the round-trip died (nightly run
  38165476712, `!<!<<b,,,,,-,,,,ar> :yam`). Non-scannable suffixes
  fall to the verbatim `!<...>` wrap.
- **parse: a leading blank floors the literal block** — a blank
  line with MORE indent than the block's first content line floors it
  (libyaml's auto-detect takes the blanks' max; the shallower content
  breaks the mapping — Psych: "did not find expected key"); the
  runner accepted what the engine rejected, so parseable input failed
  to marshal (nightly run 38163867815, `a: |\n    \n  b`). The
  runner bails the shape; deeper blanks AFTER content stay legal
  content.
- **emit: a seq item's props emit once** — the dash's props arm
  wrote "- " plus the props separator's own space (a DOUBLE space),
  and the empty propertied collection re-emitted the props
  `emit_node` had already written — every round duplicated
  (`- &a\n  &a {}`) until the shape stabilized wrong (nightly run
  38161738783, `- &a\n  ? &e...`). The props arm's dash carries no
  trailing space; an empty propertied collection rides the compact
  form (`- &a {}`).
- **emit: block scalars never ride flow keys** — a flow-rendered
  explicit key's subtree carried LITERAL members (`yaml: |` + content
  lines inside what reads as a flow context) and the emission could
  never re-parse (nightly run 38154662070, `? !\n- name: ...`).
  BLOCK SCALARS ARE ILLEGAL IN FLOW: in flow context the style
  chooser answers DQ for literal/folded styles.
- **emit: C1 controls escape on output** — `\x99` decodes to
  U+0099: valid UTF-8 but NON-PRINTABLE, so the raw pair in the
  emission failed the re-parse's own printability gate (nightly run
  38151866348, an escape-heavy doc). The whole C1 range
  (U+0080..U+009F) rides the `\xNN` escape on output — Psych's form;
  the escape re-enters the document the same way it left.
- **parse: the implicit-null mark stays in bounds** — after a flow
  close scan the cursor could stand one past EOF, and the deferred
  key's implicit-null position mark read one byte out of bounds
  (fuzz_parse ASan, nightly run 38147359781, a 23-byte exact buffer).
  The mark clamps to the buffer end first; zero-copy means every
  read is bounds-checked.
- **emit: the simple-key law counts the written token** — the key
  predicates estimated the RAW span; a ~1000-raw key whose quoted
  render expands past 1024 (escapes) rode as a simple key and the
  emission could never re-parse (nightly run 38145696566, the
  `? licit k:[…]` family). One conservative rendered-span estimate
  (factor-4 per expandable byte) now feeds all four sites — the flow
  member loop, the block member loop, the flat-map specialist, and
  the quoted-explicit-key emission — and the promoted scalar key
  rides QUOTED so its interior ": " sequences cannot re-parse as
  separators.
- **parse: the lenient flow walk enforces the simple-key law** — the
  DOM fast path's lenient walk carried no >1024 key check (the record
  walk's yt_flow_scan has one); a flow doc with a long key wrapped and
  its YAML serialization could never re-parse. The wrap now defers to
  the engine's clean error; the tape API keeps accepting (strict JSON
  has no key-length limit).
- **emit: a long flow key rides the explicit form** — JSON has no
  key-length limit, so the strict-JSON route parses >1024-char keys —
  but the YAML simple-key law caps one-line keys at 1024 and the
  emitted flow form could never re-parse (nightly run 38143486175, a
  JSON flow doc with >1000-char keys). The flow map member writer
  rides the explicit `? key : ` form past the limit, libyaml's escape
  for long keys.
- **emit: flow-rendered plains quote the tag and quote indicators** —
  the explicit-key entry's inner map renders FLOW, and a plain
  carrying '!' (a tag indicator everywhere) or a lone '\'' (opens
  flow single-quote mode) rode plain inside "{}" — the emitted form
  died in tag/quote scanning and never re-parsed (nightly run
  38139449828, `- &a\n ? : a!%!!a\n ? : a'\n-\t `). The flow gate
  gains both bytes; the suite's M2N8 ("- ? : x" must parse) keeps its
  parse-side form.
- **parse: the literal block's pool buffer is block-sized** — the
  clip lives in the span (`flen = clen - tb`) but the copy paths write
  every measured line, so allocating the clipped length wrote `tb`
  bytes past the pool block whenever a literal ended with
  stripped-empty lines (nightly run 38137087116, ASan
  heap-buffer-overflow WRITE in the re-walk). The block allocates the
  measured size; the span stays clipped.
- **parse: a pending map's root resolution tracks the root column** —
  the same missing root bookkeeping the seq arm gained, in the
  pending->map resolution: a dedent past a map-valued root's column
  popped to a content-column-0 root and the document accepted the
  second root the engine rejects (nightly run 38134498534,
  `  e:\n   f: g\nh: i`). The catch-39 law's map-valued variant;
  col-0 root siblings and same-column maps parse unchanged.
- **parse: a pending seq's root resolution tracks the root column** —
  the unanchored resolution set the opened map's content column (the
  catch-41 fix) but never the root's bookkeeping the anchored branch
  carries; a dedent past a seq-valued root map's column popped to a
  content-column-0 root and the document SPLIT INTO PHANTOM DOCS
  (`---\nb\n---\nv`) while the engine rejected (nightly run
  38128905644, ` american:\n  - ...\nnational:`). The catch-39
  second-root law, seq-valued variant; col-0 root siblings and nested
  dedents parse unchanged.
- **parse: a pending "key:" over a seq frame rejects** — the pending
  arm lacked the kind check the valued arm carries: a bare ` bar:`
  after an indentless seq item pushed the pending key UNDER the
  sequence and it materialized as an ITEM while the engine rejected
  (nightly run 38127584324, `~foo:\n- 42\n bar:\n  - 45 e.`).
  libyaml: "mapping values are not allowed in this context". Same-column
  siblings and legal containers parse unchanged.
- **parse: an alias node is complete** — props or a second node
  after an alias on the same line (`<<: *base &bar`, `*x *x`, the
  glued `*x#y`) error in libyaml ("did not find expected key");
  the DOM route absorbed the stray props while the value drain
  rejected — parseable input failed to marshal (nightly run
  38123150843, a merge-key churn family). One follower law at the
  engine's alias scan and the runner's alias-value branch; alias keys
  (`? *x`), flow aliases, comments, and plain merges parse unchanged.
- **parse: an anchor name ends at a blank or flow indicator** — a
  glued '#' is junk on the name (libyaml: "did not find expected
  alphabetic or numeric character"); the engine accepted it MANGLED —
  anchor kept, the bytes after '#' dropped — while the runner kept a
  different shape, so parseable input failed to marshal (nightly run
  38121449804, `ow0: &f#o   `). One boundary check at all three anchor
  captures; a separated '#' stays a comment and the suite charset
  (W5VH's `&:@*!$"<foo>:`) parses unchanged.
- **parse: a block-scalar header tail is blank-or-comment** — the
  runner's bare-literal arm took the block and silently dropped
  trailing header bytes (`ext: | c* `) while the engine rejected —
  parseable input failed to marshal (nightly run 38119775008, a
  multi-doc literal family). The engine's header scan also accepted
  params after a separator and repeated digits (`| 12`, `|12`).
  libyaml's law lands at both sites: the indentation digit and chomp
  sign ride the indicator directly, one each; after them only blanks
  and an optional comment ("did not find expected comment or line
  break"). All legal header forms parse unchanged.
- **parse: a pending key's seq resolution tracks the content column**
  — the unanchored resolution never recorded the opened map's content
  column (the anchored branch did), so the off-column guard could not
  fire and a key line BETWEEN the resolved seq's column and the
  parent's fell through to a SILENTLY TRUNCATED document — lines
  dropped with a successful parse while the engine rejected (nightly
  run 38118641722, `ue:\n  - one\n woc:\n  - one\n wt o-  : thrt`,
  Psych: "did not find expected key"). The opened map now carries its
  pairs' column; between-column keys reject everywhere.
- **parse: an indented seq owns its column** — a block sequence
  indented past its parent map's key column rejects a non-dash line at
  that column (libyaml: "did not find expected '-' indicator"); the
  runner had popped the seq and silently DROPPED the remaining lines
  while the engine rejected — parseable input truncated to a smaller
  tree (nightly run 38117287882, `amer:\n  - j\n  n:\n  - Bksl,} --
  }`). Only the indentless seq — parent keys at the same column —
  closes on a key line there; dedents to the parent column and dash
  continuations parse unchanged.
- **parse: a dedent below the root's own column is a second root**
  — a line left of a root map opened past column 0 ends the root
  node; content there is a second root without `---`, which libyaml
  and Psych's load_stream reject ("did not find expected <document
  start>") and the 1.2 grammar forbids (Psych.load only appears to
  accept because it stops reading at the first document end). The
  runner's root-floor exemption rode col-0 pair lines flat while the
  engine nested them, so both the catch-31 shape (run 38105178194,
  ` i1ak: ?o\nfr: ?ff`) and its three-line sibling (run
  38115641999, ` k2: v2\nk1: v\n k2: v2`) split parse from marshal;
  both routes now reject, matching the seq variant's existing
  verdict. Same-column siblings, post-root markers, and comments
  parse unchanged; the suite has no case pinning the shape.
- **parse: chained same-column pendings keep the tree** — resolving
  a pending "a:" via a same-column sibling key ("a:\na:") corrupted
  the runner's frame mapping when the container carried valued pairs:
  the OPEN'd map swallowed the second key as the first pair's value
  ("a: a", nightly run 38110714729). The runner bails the shape to
  the engine, whose tree is byte-identical to the input.
- **emit: plain-prefix document markers quote** — a plain whose
  value STARTS with "--- " or "... " (plus blank) emitted plain and
  re-read as a document marker at line start (nightly run 38109609214,
  `--- --- >\nlfoo ]…`). The bare-marker law now covers prefix forms;
  libyaml quotes both.
- **emit: flow-context anchor names that cannot ride raw are refused**
  — anchors have no quoting syntax, and an indicator byte in a name
  can never re-parse in flow context (the nightly's `&"`, bang, and
  question veins — runs 38103630412, 38108543435, 38112005844,
  38113259764). Parse keeps the full suite charset (W5VH wants
  `&:@*!$"<foo>:` to parse; block-context emission round-trips
  byte-stably); serialize returns NULL for the flow forms rather than
  emitting output that corrupts.

- **parse: `%TAG` allows only a comment after the prefix** — the
  directive's prefix scan ran to end-of-line, so trailing content rode
  the prefix and the resolved tag could never re-parse (nightly run
  38102517540, `%TAG !m! !mymy 3…`). libyaml rejects the family ("did
  not find expected comment or line break").
- **parse: the flat-run's value honors plain-first** — the
  specialist's value arm bailed on indicators but skipped the
  plain-first law, so a ']' first byte rode as a plain value and
  parseable input failed to marshal (nightly run 38101390216,
  `k1: --a &p{\n0: ]`). The engine rejects the family; the flat-run
  now bails to it — #489's law, fifth site.
- **emit: a shorthand suffix escapes every bang** — a suffix-leading
  `!` re-parses as the secondary handle (`!!bz` -> 
  `tag:yaml.org,2002:bz`, run 38099113018) and an interior one as a
  NAMED-handle boundary (`!(4!bz` -> undefined handle, run
  38100254427): the parse side only tolerates the byte when
  escape-hidden. The shorthand emitter writes `%21` for each `!` in
  the suffix, mirroring the escape grammar the parse side decodes;
  verbatim `!<...>` URIs keep the raw form.
- **emit: flow scalars quote the flow indicators** — the style
  chooser's plain predicate is block-context law, so ']' rode plain
  INSIDE "{}" where it terminates the scalar; the emitted form
  re-parsed as a stray close (nightly run 38097567398,
  `?  :\t555555ta]`). Flow-context scalars carrying ',', '[', ']',
  '{' or '}' now take the quoted routes. (The fix also initializes
  the new in-flow flag at every emitter site — the field-by-field
  stack init had left it reading garbage.)
- **parse: a lone CR after a literal is a 1-byte break** — the
  runner's literal-block line advance treated any '\r' as CRLF and
  skipped 2 bytes, so the byte after a lone CR vanished as a phantom
  line break and a trailing key-less line was silently dropped instead
  of rejected (nightly run 38096448601, `2: |\n  h!\ro`; parseable
  input then failed to marshal). Psych: "could not find expected ':'".
- **parse: tag URI escapes need two hex digits** — a stray '%' rode
  the tag raw while only the well-formed escape decoded; the stored
  tag re-encoded to a form that parsed differently (nightly run
  38095309044, `!e%2%21bz`). libyaml rejects the family at the tag
  scan ("did not find URI escaped octet") — truncated, non-hex, and
  verbatim forms included.
- **parse: tag suffixes scan the URI set** — `!>` and friends
  parsed as tags whose serialization could never re-parse (nightly
  run 38057695714, `!>\r?\r\r:\r`). libyaml scans tag suffixes over
  its URI charset and rejects the stray byte as the follower ("did
  not find expected whitespace or line break while scanning a tag").
- **parse: pend props stay off the first key** — a props-only line
  above a mapping attaches its tag/anchor to the mapping, but the
  quoted- and flow-key promotions emitted the key carrying the merged
  props; re-parse produced a tagged first key and the serialization
  never re-stabilized (nightly run 38092896626, `!\r!\r?\r?\r:\r\r:\r`).
  Same-line props stay on the key — the Psych law in both directions.
- **emit: root block props carry their separator** — a tagged,
  anchored root collection emitted `!<...>&anchor` with no space;
  libyaml requires blank or break after a tag, so the anchor-first
  9KAX documents could not re-parse.

## [0.6.32] - 2026-10-10
### Fixed
The nightly fuzz hunt's second wave — five more engine defects, each
Psych-parity-verified and pinned (crashers in the regression corpus).
- **emit: tag bytes re-encode as %XX** (#501) — the parse side decodes
  tag URI escapes per the spec; the emitter now re-encodes unsafe
  bytes (a decoded newline emitted raw drifted every round).
- **parse: an empty-key ':' line is never a pending value** (#502) —
  libyaml rejects the family; the following-lines arm now does too.
- **parse: the runner's seq item honors plain-first** (#503) — a
  plain item starting with ']' parsed but never marshaled.
- **emit: the U+FFFE/U+FFFF noncharacters ride their escapes** (#505)
  — raw, the printable validator rejects them on re-parse.
- **marshal: the value drains share the parse entry's encoding
  gate** (#506) — UTF-16/32 input marshaled raw bytes; parseable
  input failed to marshal. One fix, three consumers (marshal, visit,
  drain).
- **parse: a line-end dash is an indicator** (#508) — the runner's
  nested-dash detection required a following blank byte, so a
  line-ending '-' became the plain scalar "-" and the emitted form
  re-parsed differently than its input.
- **parse: the document-end marker owns its line** (#509) — the fused
  runners had no marker law; a '...' line carrying content was
  swallowed as a plain pair while the engine and Psych reject.

## [0.6.31] - 2026-10-09
### Fixed
The resurrected nightly fuzz harness's first full hunt — nine engine
defects, each Psych-parity-verified and pinned (every crasher joins
the `fuzz-parse-regression` corpus the suite re-executes).
- **parse: the flow-node read after props hit EOF** (#488) — an
  exact-buffer heap overread (`[!<<
!]`, 8 bytes).
- **parse: the fused runner rejects indicator-first plain scalars**
  (#489) — `u: }` parsed but never marshaled.
- **parse: an anchor needs a name** (#491) — the empty form was
  accepted at four sites; serialize then emitted an empty document.
- **parse: a verbatim tag closes `>` on the same line** (#492) —
  unterminated/empty/spaced forms accepted with drifting output.
- **emit: synthesized flow nulls are plain** (#493) — value-less flow
  entries round-tripped as `""`, drifting per serialize/re-parse round.
- **emit: U+0085 (NEL) escapes as `\N`** (#494, #495) — it is a YAML
  break; raw it folded on re-parse. The law matches the UTF-8 pair C2
  85 only — a lone 0x85 is a continuation byte.
- **emit: U+FEFF escapes as `\uFEFF`** (#496) — raw it IS the BOM and
  re-parse strips it.
- **emit: LS/PS escape as `\L`/`\P`** (#497) — the break set complete.
- **parse: the props/value-line recursion is capped at max_depth**
  (#498) — the frame-less chain stack-overflowed; it now rejects with
  `YEPTRIS_ERROR_DEPTH`.
### CI
- fuzz-nightly: the valgrind corpus path + the gtest compiler pin
  (#486); the fuzzer main links only into the harnesses (#487).

## [0.6.30] - 2026-10-07
### Performance
- **emit: the flat plain member specialist (#352)** — a strictly
  narrower fast path at the top of `emit_block_map`'s member loop
  (plain unpropertied key + plain unpropertied scalar value, proven by
  the exact predicates `sc_route` consults); anything else falls
  through untouched, so byte parity holds by construction. serialbench
  medium serialize-only -11.4%; gen table: wide-mapping +16.5%,
  scalar-heavy +5.3%, json-users +2.7%.
- **emit: fused flat-member write + lazy nametab + shrink guard
  (#352)** — the specialist's whole member writes with one capacity
  reservation and raw memcpy (plain-safe spans carry no breaks, so the
  col scan is arithmetic); the emitter's canonical-names table
  initializes only for canonical runs; the shrink-to-fit realloc is
  skipped when the waste is under cap/8. Cumulative on the gen table:
  wide-mapping +34%, scalar-heavy +20%, block-heavy +14%; serialbench
  serialize-only 1.38x.
### Fixed
- **cmake: install survives `YEPTRIS_BUILD_STATIC=OFF` — and installs
  the shared lib** — `install(TARGETS yeptris_static)` was
  unconditional while the target only exists under the flag (and
  nothing joined the export set with static off, so `install(EXPORT)`
  failed too; `yeptris_shared` was never installed at all). Found by
  yeptris-rs's CI.

## [0.6.29] - 2026-10-04
### Fixed
- **parse: a block-scalar header at the key's column is the pending
  value** — libyaml never validates a `|` / `>` header's own column
  (only the content must out-indent the parent block), but the
  engine's following-lines scan granted that same-column continuation
  only to flow nodes (`k:
[a]`). `prefatory-content:
|
  text` now
  parses as the pending key's literal value instead of failing with
  an unexpected-state error at the header — the metanorma-cli
  collection fixture shape (yeptris-ruby#258). A stray header after a
  completed pair still rejects, matching psych on both sides.

### Changed
- **release: gems belong to the binding's train** — the engine's
  release flow no longer pushes any gem variant (the retired
  trusted-publisher entry made every remaining gem step fail); it
  keeps the version cut, the tag + GitHub Release, the native asset,
  and now *reports* the lockstep binding tag instead of gating on it.

## [0.6.28] - 2026-09-30
### Fixed
- **parse: the fused runner bails on three anchored shapes it mishandled**
  — an anchored key whose "content" sits at the key's own column (the
  value is null; the line is a sibling) became a nested map; an
  indentless anchored sequence lost every following sibling; and an
  alias to an anchor whose container never opened (null-valued or
  still pending) corrupted the tape. All three now fall back to the
  engine route, byte-exact with it (the runner's own contract: any
  line the loop does not own bails). Found by the binding's Psych
  merge-key port (yeptris-ruby's CI gates the lockstep tag); the C
  suite had no coverage for these shapes.


## [0.6.27] - 2026-09-30
### Changed
- **parse: the fused block runner owns every block-family shape** —
  quoted values, literal block scalars, and anchored containers join
  the owned set, and chained same-column pendings no longer leak a
  frame per record (the 256-frame guard at ~254 documents was
  block-heavy's silent bail). block-heavy, scalar-heavy, and
  anchor-heavy now run the fused SWAR runner end-to-end
  (block-heavy ~2.4x, scalar-heavy ~4x, anchor-heavy 2.8x their eager
  routes); block-scalar content rides a per-run pool transferred to
  the document, and the first pair inside an anchored container bakes
  the engine's tag classification via the threaded resolver.
  Byte-exact tape differential over all five block-family corpora and
  the yaml-test-suite inputs.
- **parse: the fused block runner owns nested block YAML** — deep
  nesting, indentless/nested sequences, sibling maps after child
  closes, and null-resolved pending keys now run the fused SWAR
  runner instead of falling back to the engine. The frame model
  mirrors the engine's tape emission exactly (strict dedents,
  per-column phantom content maps, null scalars for same-column
  siblings, the repeat-alias memo). Deep-nesting parses 3.09x the
  eager route (1078 MB/s DOM); wide-mapping holds 2.06x; byte-exact
  tape differential over the yaml-test-suite inputs.
- **parse: the fused block runner owns monomorphic block documents**
  — one SWAR sweep per line with inline classification writes the
  packed tape directly, no engine pass; any line the loop does not
  own bails to the engine route unchanged (restart fallback).
  Wide-mapping parses at 2.06x (598 vs 290 MB/s); other shapes ride
  the fallback until the column model widens coverage (#79 session 3).

## [0.6.26] - 2026-09-27
### Fixed
- **parse: NEL/LS/PS documents no longer dangle** — the one-shot
  `yeptris_parse` normalized Unicode breaks (NEL, LS, PS) into an
  ENGINE-owned buffer that dies at engine teardown, so every document
  containing one returned views into freed memory (garbage values or
  segfault on access); no test had ever parsed a real NEL document.
  The copy now rides the document's `transcoded` ownership before the
  engine runs; the streaming feeds keep the engine's per-chunk copy.
### Changed
- **parse: the Unicode-break probe rides the SIMD stopset kernel** —
  the whole-document NEL/LS/PS pre-pass walked every byte with a
  two-prefix check (~6 compares/byte); it now zero-detects the two
  lead bytes per 8-byte word via `stopset_find` and byte-checks only
  candidates. Block-heavy +13%, scalar-heavy +31% (order-alternated
  DOM-lane medians). The lead-byte stopset literal is pinned by
  `Parse.NormBreakLeadsMatchRuntimeBuild`.

## [0.6.25] - 2026-09-26
### Fixed
- **recorder: the compat schema's engine runs floor-free** — the
  recorder's engine creation set only the resolver, so every consumer
  of `yeptris_recorder_new_ex(COMPAT)` + feed (the py safe_load path)
  kept the flow indent floor #431 had removed everywhere else and
  rejected the multi-line flows libyaml accepts; caught by the PyPI
  clean-venv verification of the 0.6.24.1 wheel (#431 follow-up).

## [0.6.24] - 2026-09-26
### Fixed
- **parse: the 11-COMPAT grammar accepts multi-line flows libyaml
  accepts** — the block-level flow indent floor (9C9N, VJP3) rejected
  real-world locale files whose flow mappings close at their parent's
  column ("k: {\n  a: b\n}"); libyaml/Psych parse those, the floor is
  a yaml-test-suite rule the strict engine keeps. The floor now stays
  on for the default 1.2 engine only; every 11-COMPAT parse entry
  (yeptris_parse_ex, the value drains, the marshal fast path) runs
  floor-free (metanorma-standoc CI breaker, isodoc i18n-en.yaml).

## [0.6.23] - 2026-09-26
### Fixed
- **emit: parsed scalars `---`/`...` emitted plain** — the '-'/'?' head
  case early-returned past the document-marker check; libyaml/Psych
  quote the bare markers, and the head now falls through to the rule
  that owns them (#352).

### Changed
- **json parse: the member cycle's compact fast paths + the SWAR digit
  run** — the walk's whitespace-skip loop topped the arm64 profile (~8
  instructions per member even when it exits on byte one; compact JSON
  makes every exit byte one), the per-byte digit run followed: the
  map/seq cycles now dispatch value `,` key / key `:` value / value `,`
  value directly through the byte LWS would land on (the general form
  remains for whitespace-bearing input), and the initial digit run
  scans SWAR words. Same-host calibrated reads: json-doc flat at
  parity, json-users 0.73x to 0.90x (#342).
- **engine: the pair path's value event assembles only on the
  fallback** — the whole-line fast path filled the ~100-byte event even
  when the sink took the block value and ignored it (every
  default-route consumer); the fallback rebuilds it field-for-field
  with the pair facts taken before the fold (item 79 seam).
- **emit: the plain-safety interior scan runs in SWAR words** — five
  zero-detects per 8 bytes plus shifted-mask adjacency with boundary
  carries replace the per-byte branch ladder (22-35% of emit);
  differential-pinned against a reference loop over adversarial
  alphabets — the pin caught two mask bugs and the marker bug before
  they shipped (#352).
## [0.6.22] - 2026-09-25
### Fixed
- **bench: the simdjson referee lane parses a pre-built padded string** —
  the raw-pointer parse overload's `realloc_if_needed` copies the whole
  input on every timed parse (a tax simdjson's own harness and the
  serialbench fixture runner don't pay), flattering every head-to-head
  ratio the table printed: the "parity band" was partly instrument tax.
  Calibrated local read: json-doc 1.07x, json-users 0.73x (#342, #420).
- **json parse: the strict number arm's digits-only flag clears only on a
  consumed byte** — the flag was cleared when *inspecting* the run's
  terminator (comma, bracket, newline), routing every delimiter-terminated
  integer into the strict single-pass validator (~15% of json-doc parse
  re-validating runs the cheap path already proved). CI referee: json-doc
  at the simdjson parity band (0.97x ubuntu / 1.05x macos interleaved
  medians, from 0.88x) (#342).

### Changed
- **json parse: the string arm settles inline to 32 bytes** — four clean
  SWAR words before the `quote_scan` kernel: the 9-31-byte clean strings
  (the email class; ~100k per parse on the users corpus) no longer pay
  the kernel call plus a redundant content re-sweep, and the strict
  number validator is always_inline (its out-of-line call cost more than
  the 3-8-byte spans it checks). Round 1 is untouched — sub-8-byte
  strings pay zero (#342).

## [0.6.21] - 2026-09-24
### Fixed
- **dom: the #377 child-index cache reads ride the mutex** — the hit path read
  the cache unlocked while a concurrent builder published it under midx.mu (a
  schedule-dependent TSAN race on read-only sharing, present since 0.6.17;
  caught by the Threads.ReadOnlySharing leg) (#414).

### Changed
- **json parse: the secondary passes are gone** — numbers validate in-walk at
  record time (the settle pass deleted; digits-only spans settle with a
  leading-zero check, dot/exp spans through a single-pass strict validator),
  the nametab pre-pass no longer runs in JSON mode, and the entry's
  printability/tab pre-passes are replaced by the walk's own byte guarantees
  (clean_only: tabs and non-ASCII reject in-walk; the strict sequence owns
  the pinned precedences and re-gates only when taken). CI referee:
  json-users at the simdjson parity band (0.95-1.01x); the lenient tape
  entry's deferred number contract is unchanged (#342, #410/#411/#412).

## [0.6.20] - 2026-09-24
### Fixed
- **emit: the writer's grow trigger left no terminator slot** — a serialized size landing
  exactly on the buffer capacity wrote the closing NUL one byte past the allocation
  (the next heap block's header). Corruption deterministic, detection dependent on
  allocation history: the multi-year "abort under load, position varies with GC timing"
  flake (yeptris-ruby#182). The writer now reserves the terminator slot; the
  boundary-sweep regression gate fires AddressSanitizer at the exact site when
  reverted (#405).
- lazy YAML-tape route: the parse-only document no longer allocates (and leaks) the
  eager sink's DOM; MSVC C4701 zero-init (#404 follow-up).

### Changed
- **the packed YAML record tape is the DEFAULT YAML parse route (#378 slice 2)**:
  `yeptris_parse` records the engine's committed ops into 8-byte words and the
  tree materializes on first access — parse-only workloads never build nodes.
  CI referee (parse-only, interleaved medians): tape/eager 1.06-1.52x on the four
  referee shapes, both ISA tables. The tree materialization is single-shot under
  a per-document mutex (the read-only-sharing contract holds); the eager form
  stays reachable internally for the A/B. Defects the flip exposed and fixed:
  the flow `max_depth` now rides the tape, a failed materialization answers 0
  from `yeptris_document_count` instead of dereferencing, and the marshal fast
  path routes through the lazy choke point.

### Added
- **the packed YAML record tape (#378 slice 1)**: a recording sink over the engine's
  seven committed-op callbacks packs the stream into 8-byte words; replay decodes
  into the same builder functions. Differential-gated (30-shape inline corpus +
  the 67-file psych-pure sweep + compat-11 + error parity), behind the internal
  `yeptris_parse_ytape_ex` seam — the default route is unchanged (#404).

### Changed
- **json parse: the fused lenient walk's map/seq member cycles** replace the
  per-token state machine (#402); the three string arms collapse into one
  SWAR/quote_scan fast path with the escape kernel as fallback, and pure-integer
  spans settle without the number kernel (#403). CI referee: json-doc 0.77x ->
  0.93x simdjson, json-users 0.64x -> 0.88x (#342).

## [0.6.19] - 2026-09-23
### Added
- libyaml-parity event end marks (#179): `yep_event`, `YeptrisEvent`, and
  the recorder's `YeptrisEventRecord` (36 -> 44 bytes) carry
  `end_line`/`end_col` with libyaml's exact end_mark semantics — every
  event family (stream, document with directive-inclusive starts and
  `---`/`...` spans, block and flow collections, plain/quoted/
  literal/folded/multiline scalars, keys, aliases, deferred nulls)
  matches stdlib psych's `event_location` marks byte-for-byte, pinned by
  the psych-frozen `EventMarks` gate over the 13-fixture corpus
  (`scripts/marks_oracle` regenerates it). The bindings mirror the
  record layout in this lockstep window.

## [0.6.18] - 2026-09-22
### Fixed
- The emit's 128-bit float tier (`yep_u128`) compiles for any compiler:
  the portable lo/hi-limbs fallback serves 32-bit GCC (the armv7
  platform gems), with the limb math pinned by the forced-tier
  `FloatLimbs.*` tests on 64-bit hosts.

## [0.6.17] - 2026-09-22
### Added
- The CBOR decode sink seam (#157): the grammar loop drives a
  `yep_cbor_sink` (text/int/float/str/bytes/open/close); the DOM
  builder is the default sink and the native bindings' VALUE builder
  rides the same grammar — decode to Ruby objects in one pass with no
  DOM intermediate and no per-number text round trip.
### Changed
- The emit front's double-quote and plain-scalar routing is
  run-batched (#352): the writer batches by run instead of
  re-deriving per scalar.
- CBOR map keys intern in the native walk (#157): repeated keys share
  one VALUE per document.
### Fixed
- The qemu platform-gem cells (#376): the per-cell setup now reaches
  the container, the debian cells install their packages, the s390x
  cell pins ruby:3.2-bookworm (3.3+ dropped the variant), and the
  setup string is parsed as shell (env expansion never re-parses
  metacharacters). ppc64le and s390x gems shipped in the 0.6.16.1
  binding release.
- The `yeptris_desc_node` layout gate is arch-relative
  (`sizeof(const char*) + 16`): 32-bit ARM builds again (armv7 gems
  follow the next binding release).

## [0.6.16] - 2026-09-21
### Added
- The lazy child-index (#377): `yeptris_node_seq_at` and
  `yeptris_node_map_at` are O(1) after the first call (the last
  indexed container's child ids cache under the dom's lazy-init
  mutex; invalidated by every link/unlink). The bulk drain
  (`yeptris_node_children`) fixed the bindings' loops; this fixes
  the accessor itself. Differential-tested against mutation.
- The qemu platform-gem matrix (#376): arm-linux, arm-linux-musl,
  ppc64le-linux, s390x-linux — the family's 14-gem target. The
  teptris-ruby recipe (binfmt + the arch ruby image, the same
  build-platform-gem.sh script the musl leg rides, smoke inside
  each container).
- The `index-rows` benchmark corpus (#179d): a top-level block
  sequence of symbol-keyed maps — the relaton index shape that hid
  the materialization cliff.
### Fixed
- The qemu cells use the right package manager per image (#376
  follow-up): the `||` fallback (apt-get || apk) broke on emulated
  Debian — each matrix cell now carries its own setup command.

## [0.6.15] - 2026-09-21
### Fixed
- `yeptris_marshal_node` bails (`YEPTRIS_ERROR_UNSUPPORTED`) when any
  node in the subtree carries an explicit tag: the value records hold
  the resolver's verdict, not the source's tag, so `!ruby/object`
  mappings marshaled as plain hashes with the tag silently dropped.
  Load-bearing for yeptris-ruby#178's `unsafe_load` fast path (the
  11 MB relaton index materializes in 0.71 s end to end — 4× stdlib).

## [0.6.14] - 2026-09-21
### Added
- Lazy DOM materialization (#342 slice 2): `yeptris_parse_json`'s
  gate-clean route rides the fused lenient walk (one pass, records
  only) with the deferred number grammar settled inline, and the tape
  travels on the document — `dom_from_tape` builds the tree on first
  access through the new internal `yep_doc_dom` choke point, so
  parse-only workloads never pay the DOM lane. Tab-carrying buffers
  and scalar roots keep the eager routes (pinned behavior).
- `yeptris_node_children` (yeptris-ruby#168): the bulk child drain —
  one walk fills a handle buffer (sequence elements in order; mapping
  key,value pairs interleaved), returning the total count. The
  per-index `seq_at`/`map_at` walks are O(i) sibling hops each; the
  bindings' per-element loops made an 80k-row sequence quadratic (the
  relaton index parsed in 238-328 s where stdlib takes ~3 s).

### Fixed
- `dom_from_tape`: restored the seven cleanup-eaten guard bodies
  (args, columns, depth cap, open/place, close underflow, arena tail)
  — load-bearing now that the materializer is the lazy path.
- Every document constructor now initializes the new `lazy_tape`
  field (build.c already memset; the three field-by-field ctors set
  it): a garbage word made `yeptris_document_free` free an
  indeterminate pointer — glibc's reused pages segfaulted every CBOR
  leg on Linux/Windows while macOS's zeroed fresh pages hid it.

## [0.6.13] - 2026-09-20
### Added
- The schema-descriptor API's declared headroom, executed (#238):
  `YEP_ST_ANY` materializes the resolver's verdict as one
  YeptrisValue record per value (kind/tag from the node, the
  borrowed span, converted numeric payloads; under compat the DOM's
  own resolver carries the schema — it types the nodes the verdict
  reads), and `YEP_SF_FIRST_WINS` bounds a duplicate key's column
  to its first match within one mapping instance (sequence
  elements still accumulate across theirs).
- `dom_from_tape` (#342 slice 1): builds the node tree from a
  parsed tape's records — the lazy-DOM seam. Reads the columns
  (the one representation valid on every route), classifies the
  lenient route's deferred number spans at materialization, and is
  differential-tested structurally identical to the direct fused
  parse.

## [0.6.12] - 2026-09-20
### Added
- The `!binary` contract (yeptris-ruby#168): the compat schema
  resolves psych's LOCAL `!binary` shorthand to the core BINARY tag
  id (stdlib to_ruby accepts both spellings; core12 keeps it
  CUSTOM); a tagged literal-styled scalar keeps the block form even
  single-line (`!binary |-` + one base64 line — psych's exact
  bytes); the bulk-build ABI admits exactly one trailing TAG on a
  closed scalar root; the marshal fast path returns UNSUPPORTED for
  binary-tagged scalars so hosts decode.
- The lazy document-open path (yeptris-ruby#157's fixed-cost cut):
  the pool's first block materializes on first use, and the handle
  pool defers to the first handle acquisition — every access rides
  the dom's lazy-init mutex (the map-index discipline; the first
  cut raced TSAN's read-only-sharing test).

## [0.6.11] - 2026-09-20
### Changed
- `yeptris_serialize` / `yeptris_serialize_ex` emit in ONE wet pass
  into a growable buffer (estimate: input size + 64, doubling on
  demand, shrink-to-fit at the end) — the exact-sizing dry pass
  measured 0.54-0.73x of the one-pass route on CI-fresh runners and
  is now exclusive to `yeptris_serialize_into` (whose contract needs
  the byte count before writing). serialize gained ~1.8x, judged by
  the emit-split table's within-run 2p/1p arm (#352).
- The dry pass records per-scalar emission routes (long scalars only,
  32B threshold) that the wet pass consumes — `serialize_into`'s
  remaining two-pass route halves its derive work.
- The benchmark artifact's emit table carries a third interleaved
  arm: the streaming route (one derive-while-writing pass) — the
  within-run referee for pass-count questions.

## [0.6.10] - 2026-09-19
### Changed
- The strict-JSON fused DOM builder resolves literal tags by first
  byte (the walker already validated the span is exactly
  true/false/null) instead of dispatching the resolver — no indirect
  call per literal token.
- The benchmark artifact carries two referee tables: the simdjson
  head-to-head now covers `json-users` (serialbench's medium.json
  shape — the attribution corpus for the DOM-build lane, #342), and
  an emit kernel split (serialize vs serialize_into, the allocation
  share — #352) rides every push.

## [0.6.9] - 2026-09-19
### Changed
- The lenient JSON tape's primary storage is ONE interleaved 8-byte
  record per token (off:u32 | len:u23 | kind:u8; spans over 16 MiB
  carry a CONT extension) instead of three column stores — item 07's
  slice 1 toward simdjson-class walk cost.
- The kinds/offs/lens columns stay as the compatibility ABI,
  materialized lazily: `yeptris_tape_columns` fills them on first
  touch; `yeptris_tape_convert`, `yeptris_tape_plan_walk`, tape-diff
  and the unit fixtures route through it. The strict route remains
  column-primary (unchanged).

## [0.6.8] - 2026-09-19
### Fixed
- The canonical CBOR encoder's sizing pass no longer holds a map
  pointer across child recursion: a nested map's canonical prep
  reallocates the map table, and the dangling pointer made the next
  pair read walk freed memory — a heap-use-after-free that segfaulted
  `yeptris_cbor_encode` on documents from 11 canonical-sorted map
  entries (the 16->32 growth crossing; yeptris-ruby#152). The map
  INDEX is held and the pointer re-derived for every read. ASAN-verified
  at 11/15/100/1000 items; insertion-order encoding was unaffected.

## [0.6.7] - 2026-09-18
### Fixed
- The marshal encoder's magnitude bookkeeping is width-safe: the
  byte-width and bignum limb-count loops read the magnitude through
  `unsigned long`, which is 32-bit on Windows (LLP64) — every integer
  with |v| >= 2^32 truncated to its low 32 bits through the Ruby
  binding's marshal load path (1234567890123456789 materialized as
  2112454933; caught by the windows-11-arm spec leg). The negative
  payload's borrow is now masked at k == 8, removing the `1UL << 64`
  shift UB reachable on LP64.

## [0.6.6] - 2026-09-18
### Added
- `yeptris_free` — the allocator-matching free for the serialize*
  family's owned buffers (#318's Windows root cause): the host's
  libc `free` is a different CRT instance than the DLL's on Windows,
  and FFI cannot resolve bare `free` against the DLL's exports at
  all. NULL is a no-op.

## [0.6.5] - 2026-09-18
### Changed
- The cycle fast path (TODO.restructure/79 carve 1a): lines that
  continue the top frame — a map key at the frame's column, a seq
  dash — with no pending state call the classified arm directly
  from the line loop, skipping the node prologue and dispatch tail
  on the monomorphic cycle lines. block-heavy parses run 8-12%
  faster (interleaved A/B, ledgered in benchmarks/PERF-LEDGER.md);
  the event streams are byte-identical (the differential and
  yaml-test-suite gates pin them).

## [0.6.4] - 2026-09-17
### Added
- The plan walk's segmented paths and the YAML/DOM leg (#293 /
  TODO.restructure/87, slice three): the spec's `path` accepts an
  ARRAY of strings (walked mapping by mapping to the rows container,
  both legs), and `yeptris_document_plan_walk` applies the same
  compiled plan to a `yeptris_parse` document — block YAML hydrates
  to typed columns with no intermediate tree. Typed extraction rides
  the parse-time tag ids (the typing SSOT) plus the number kernels;
  string columns expose `(ptr,len)` views via
  `yeptris_plan_result_strs` (the result borrows the document);
  aliases resolve to their targets; map-root containers yield their
  mapping values as rows.

## [0.6.3] - 2026-09-17
### Added
- The compiled plan walk (#293 / TODO.restructure/87, slice one):
  `yeptris_plan_compile` builds a plan from a strict-JSON descriptor
  spec (`{"kind":"seq|map","path":..., "children":[{"name":...,
  "kind":"int|float|str|bool"}...]}`), and `yeptris_tape_plan_walk`
  applies it to a parsed JSON tape in ONE C pass producing typed
  COLUMNAR output (per-column int64/double/span arrays + null
  bitmaps) — no host tree, no per-node dispatch. Slice one covers
  the lutaml-model shape (a root collection of scalar-leaf rows);
  nested paths, the YAML leg, and partial descriptors ride the
  board item. Works over both the strict and lenient tapes.

## [0.6.2] - 2026-09-17
### Added
- `YEPTRIS_BUILD_TAG` — the bulk build API can tag the last-placed
  node (tag bytes in the blob). The bulk ABI otherwise could not
  express tags; #300's nil mapping keys need Psych's explicit `!`
  form (`! ''`). Pinned by BulkBuild.TagAppliesToLastPlacedNode.

## [0.6.1] - 2026-09-17
### Fixed
- libyaml's null rendering and block-scalar indicators (#290, the C
  half of the six remaining Psych byte-parity families): an empty
  plain scalar value rides the key bare (`k:`), a null sequence item
  is a bare dash, an explicitly-empty block scalar stays `""` (a bare
  empty would re-read as null — corpus-pinned), and literal blocks
  emit the explicit indent only when the first body line starts with
  a space or is blank, with blank interior lines unpadded.

## [0.6.0] - 2026-09-16
### Added
- `yeptris_parse_json_tape_lenient` — the simdjson deferred contract,
  opt-in: structural checks stay at parse while number grammar defers
  to `yeptris_tape_convert` (charset runs record as `YEP_T_NUM`;
  "1.2.3" rejects at drain where simdjson's number parse would).
  Tab whitespace is legal (RFC 8259); non-ASCII takes the strict
  route; on everything strict accepts the tapes are structurally
  identical. json-doc: 528 -> 705-765 MB/s vs simdjson's ~1041
  (0.70-0.73x; PERF-LEDGER verdict eight records the endgame design
  for the last stretch). `yeptris_tape_convert` now requires
  full-span consumption — a latent partial-span acceptance the
  lenient differential caught (`1]`), pinned.
### Performance
- The lenient walk carries a SWAR close-quote settle (one u64 window
  for 1-8 byte strings) and colon/comma peek fusion on the byte walk.
  The bench matrix gains the tape_lenient leg on the simdjson
  head-to-head.

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
