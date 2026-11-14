# Changelog

All notable changes to this project are documented in this file. Versions
are bumped by `scripts/bump-version.sh` (CMakeLists.txt is the single
source of truth; this file, vcpkg.json are synced from it).

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Fixed

- **the comparator carries the nightly's post-tag soup finds (rounds r258–r260, r262, the #1310 family)** — the measured-unbounded soup family keeps producing fresh members against the comparator arm (zero engine divergence outstanding; the last engine defect remains r248, #1281):
  - **the soup repeated with counter variations (r258, the #1310 family)** — the diff leg's first post-exclusion find at scale (run 43359971370, 10732 bytes): the r255 shape-9 soup's member pattern repeated with counter variations, with the exotic-break tail members (`yam<LF><TAB>l.org`, `2002:binary<CR>YAM`, `?##<CR>L<LF>&m`), carried whole as the pinned shape (the r256/r257 full-artifact precedent). The byte walk resolves all 433 divergence hunks into the two documented laws: 227 glued-`?##` member deletions (yeptris keeps them flat and plain per the #949 law; libyaml's comment eats to the CR and rebinds the explicit key — including a `>`-prefixed member whose swallow eats `|,>` and a `0 `-prefixed twin), 78 `?##<CRLF>&m` rebinds (`&m` rides inside the yeptris plain scalar vs libyaml's anchored empty key), and 124 one-line break renderings (the U+2029/CR runs fold on the yeptris side while libyaml rides the raw bytes — the 1.2 break law), 124/124 verified raw-exact modulo break rendering; every other member byte-exact. Shape 41 joins the `yd_r244_flow_qhash_cr_gray` arm; the 50-mutant battery rides the carry (the full 20273+18836-byte dump pair is already pinned byte-exact by the shape's strcmp, the r256/r257 call); corpus walk exit-0, leaks clean, 792/792 ctest, 176/176 fuzz-diff pins with the `flow-qhash-cr-r258.bin` seed pin (PR #1322).
  - **the shape-2 twin, digit-swapped plus-spliced (r259, the #1310 family)** — the diff leg back at the family's minimum scale (run 43369491072, 25 bytes: `[1,?#,"\\rg+",?g:m?#,"\\rg",]`): the shape-2 composition with a `0`-to-`1` digit swap and a `+` spliced into the first quoted member. The three documented laws compose exactly as pinned since r244: the glued `?#` rides the comment-to-CR law (libyaml's `#` eats `,"` and the CR and reads the post-CR `g+"` as the empty-valued key) and `?g:m?#` rebinds identically, yeptris keeps the members flat and plain (the #949 law), and the quoted CRs fold to spaces on the yeptris side (the 1.2 break law; the surviving `"\\rg"` member rides DQ on both sides per the #1268 family, its fold byte-exact). Shape 42 joins the `yd_r244_flow_qhash_cr_gray` arm; corpus walk exit-0, leaks clean, 792/792 ctest on the plain build and 975/975 on the libyaml build, 177/177 fuzz-diff pins with the `flow-qhash-cr-r259.bin` seed pin (PR #1324).
  - **the three-mutation flow-map permutation (r260, the #1310 family)** — the diff leg back on the r256 flow-map soup (run 43380562434, 66 bytes): a three-byte permutation (the leading `?#?#` loses one `?` to `?##`, `*%L` loses its `%` to `*L`, and a `:` is spliced at the last member's `\n` fold point). The documented laws compose exactly as pinned since m39/m40: the glued `?##` members stay flat and plain (the #949 law) while libyaml comment-eats to CR/LF and rebinds the explicit keys with empty values, the TAB/LF folds ride the 1.2 break law (the spliced `:` formalizes the `` ` `` member's value slot and moves neither dump), and the alias member rides `=ALI *L` on yeptris vs libyaml's empty-key collapse (the m39/m40 alias precedent) — both dumps byte-identical to the pinned m39/m40 pair modulo the deleted bytes. Shape 43 joins the `yd_r244_flow_qhash_cr_gray` arm; corpus walk exit-0, leaks clean, 792/792 ctest on the plain build and 976/976 on the libyaml build, 178/178 fuzz-diff pins with the `flow-qhash-cr-r260.bin` seed pin (PR #1325).
  - **the head-grown soup twin, fold colon dropped (r262, the #1310 family)** — the diff leg back on the r260 member itself (run 43405687610, 77 bytes): a twelve-byte head growth plus a one-byte deletion (the leading `?#` takes a spliced `?` after it and eleven glued `?` before the closing `|+`, and the m43-spliced `:` at the last member's `\n` fold point is gone). The documented laws compose exactly as pinned since m39/m40: the grown head rides the #949 plain-? law — every spliced `?` is glued, so yeptris keeps one flat plain scalar with the same SP terminator and only the head key's text moves — while the fold-point colon's deletion rides m43's own finding that the spliced `:` moves neither dump (its absence moves neither either), libyaml's dump stays byte-identical to the pinned l43 (its head region is comment-eaten to empty keys either way), and the alias member rides `=ALI *L` on yeptris vs libyaml's empty-key collapse (the m39/m40 alias precedent). Shape 44 joins the `yd_r244_flow_qhash_cr_gray` arm; corpus walk exit-0, leaks clean, 792/792 ctest on the plain build and 978/978 on the libyaml build, 180/180 fuzz-diff pins with the `flow-qhash-cr-r262.bin` seed pin (PR #1333).
  - **the head-shrunk soup twin, alias `%` restored (r263, the #1310 family)** — the diff leg back on the r262 member itself (run 43423327515, 67 bytes): a twelve-byte head shrink plus two one-byte moves (the head key's spliced `?` and eleven glued `?` leave, so `?#?#|#|+` is the whole head key again; the alias regains m43's `%` to `*%L`; and the tail's pre-`}` SP becomes a `%`). The documented laws compose exactly as pinned since m39/m40: the glued `?#`/`?##` members stay flat and plain (the #949 law) while libyaml comment-eats each to an empty key and rebinds the explicit keys (the comment-to-CR law), the alias rides `=ALI *%L` on yeptris vs libyaml's empty-key collapse (the m39/m40 alias precedent, the m43 spelling), and the tail member's `%` rides the TAB/LF fold law identically on both sides (`=VAL : < %` byte-exact on both; the greedy minimizer pins the 10-byte core `{?#,*%.,<}` riding the same three laws). Shape 45 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (49/49 identical verdicts, the artifact the single pre-abort → post-pass), six corpus walks exit-0, leaks clean, 792/792 ctest on the plain build and 979/979 on the libyaml build, 181/181 fuzz-diff pins with the `flow-qhash-cr-r263.bin` seed pin (PR #1336).
  - **the bare-`?` x alias-ride soup — the family's first member with no `#` and no CR (r264, the #1310 family)** — the diff leg off the `?#` comment family onto its bare-`?` sibling (run 43452769394, 24 bytes: `{\n?\t \n ,?:-,?*M:-,?*M,}\n`, a flow map over the members `?\t \n `, `?:-`, `?*M:-`, `?*M`). The laws compose exactly as the r199/r209 arms pin them: member 1's `?` rides a TAB blank, so the explicit-key + empty pair matches byte-exact on BOTH engines (no divergence); members 2-4 ride the bare-`?`-glue x alias-ride composition — yeptris's suite-pinned plain-`?` law (HM87/652Z) takes `?:-` / `?*M:-` / `?*M` as ONE plain key each, the colon riding inside where a non-blank `-` follows it, values empty, while libyaml's 1.1 unconditional flow KEY-token law (the qflow quirk carried since r58) reads each `?` as the explicit-key indicator, its IS_ALPHA alias cut (the r142/r76 law) surfaces `=ALI *M` as the members 3-4 keys, and the members 2-3 `:` re-lexes as the VALUE token with the `-` as the plain value. Minimality: the libFuzzer minimizer converges on the 14-byte twin core `{?*M:-,?*M,}` and the hand-minimum inside the composition is `{?*M:-}` (7 bytes) — greedy deletion drifts into the arm-covered siblings (`{?*}` plain-`?`, `{*M:-}` bare alias-cut), exactly why the libFuzzer walk stops at the twin. Shape 46 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (14 pass / 36 abort-both, the artifact the single pre-abort → post-pass), six corpus walks exit-0 (parse 418 / feed 65 / roundtrip 7 / marshal 18 / cbor 102 / validate 5), leaks clean, 792/792 ctest on the plain build and 980/980 on the libyaml build, 182/182 fuzz-diff pins with the `flow-qhash-cr-r264.bin` seed pin (PR #1338).
  - **the qhash dict-ride soup twin (r265, the #1310 family)** — the diff leg back on the r263 soup with the auto-dictionary's `=VAL : ` spliced into member 5 (run 43482683441, 74 bytes: the nightly mutation `DE: "=VAL : "` makes the member `?#|#|=VAL : `, and the tail's post-fold `%` is back to a blank, member 8 riding `<` plain on both sides per the m44 spelling). The documented laws compose exactly as pinned since m39/m40: the glued `?#`/`?##` members stay flat and plain (the #949 law) while libyaml comment-eats member 1 and member 2 to the CR — member 2's eat swallowing members 3-5 and member 6's head — and rebinds the explicit keys with empty values (the comment-to-CR law), the alias member rides `=ALI *%L` on yeptris vs libyaml's empty-key collapse (the m39/m40 alias precedent, the r58 qflow KEY-token and r142/r76 IS_ALPHA-cut laws), and the spliced ` : ` rides the colon-inside law on yeptris only: the plain key grows to `?#|#|=VAL` with the colon re-lexing as the implicit VALUE token (the r46/HM87/652Z colon ride, the r230 residual-colon family) while on libyaml the whole member sits inside member 2's comment region and moves nothing. Minimality: no single member deletion keeps the divergence — every soup segment is load-bearing, so the artifact bytes are their own hand-minimum; the libFuzzer greedy floor is the 10-byte same-law twin `{?#,*%\n,<}` (drifted dump shape, the r264 lesson), and the surgical reductions all land in both-engines-agree territory. Shape 47 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (6 pass / 44 abort-both, the artifact the single pre-abort → post-pass), six corpus walks exit-0 (parse 419 / feed 64 / roundtrip 6 / marshal 17 / cbor 101 / validate 5), leaks clean, 792/792 ctest on the plain build and 981/981 on the libyaml build, 183/183 fuzz-diff pins with the `flow-qhash-cr-r265.bin` seed pin (PR #1340).
  - **the qhash non-ASCII-splice soup twin (r266, the #1310 family)** — the diff leg's fourth consecutive fresh member, back on the r263 soup with member 7's key bytes `#|#` mutated into a two-byte non-ASCII char (run 43509871735, 67 bytes: U+0783, the family's first non-ASCII splice, and the tail's post-fold `%` back to a blank, member 8 riding `<` plain on both sides per the m44 spelling). The documented laws compose exactly as pinned since m39/m40: the glued `?#`/`?##` members stay flat and plain (the #949 law) while libyaml comment-eats member 1 and member 2 to the line break — member 2's eat swallowing members 3-5 and member 6's head — and rebinds the explicit keys with empty values (the comment-to-CR law), the alias member rides `=ALI *%L` on yeptris vs libyaml's empty-key collapse (the m39/m40 alias precedent, the r58 qflow KEY-token and r142/r76 IS_ALPHA-cut laws), and the U+0783 splice rides the plain-scalar content law on yeptris only: the printable non-ASCII char sits in the 1.2 ns-printable range and rides inside member 7's plain key while on libyaml the whole member sits inside member 2's comment region and moves nothing (the m47 splice precedent). Minimality: no single member deletion keeps the divergence — every soup segment is load-bearing, so the artifact bytes are their own hand-minimum; the libFuzzer greedy floor is the 10-byte same-law twin `{?#,*%\n,<}` (drifted dump shape, the r264/r265 lesson), and the ASCII `#|#` spelling of member 7 already verifies under the qgh-differ walk — the unclaimed residue is precisely the non-ASCII splice. Shape 48 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (33 pass / 17 abort-both, the artifact the single pre-abort → post-pass), six corpus walks exit-0 (parse 419 / feed 64 / roundtrip 6 / marshal 17 / cbor 101 / validate 5), leaks clean, 792/792 ctest on the plain build and 982/982 on the libyaml build, 184/184 fuzz-diff pins with the `flow-qhash-cr-r266.bin` seed pin (PR #1342).
  - **the qhash quoted-tail-splice seq twin (r267, the #1310 family)** — the diff leg's fifth consecutive fresh member, back on the r259 shape-42 seq composition with the final quoted member's tail expanded by the auto-dictionary splice (run 43531063474, 31 bytes: the nightly mutation `DE: "-MAP\012-S"` turns the tail `"\rg"` into `"\rg-MAP\n-S"`, the head digit riding `1`). The documented laws compose exactly as pinned since shape 1: the glued `?#` stays flat and plain (the #949 law) while libyaml comment-eats `,"` to the CR and reads the post-CR `g"` as the empty-valued key, and `?g:m?#` rebinds identically (key `g:m?#`, empty value — the comment-to-CR law), and the expanded quoted tail rides the 1.2 break law byte-exact on BOTH engines — the CR folds to the leading space and the LF folds to the middle space, `=VAL :  g-MAP -S` on both (the r251 shape-5 precedent for quoted-tail expansions whose breaks fold identically), so the divergence stays members 2-4. Minimality: the libFuzzer greedy floor is the 5-byte `[?::]` (drifted dump shape, the r264 lesson — it exits the composition into the plain-`?` colon family, an uncarried same-law sibling flagged for the #1310 classifier), and the hand-minimum inside the composition is the 16-byte `[?#,"\rg",?g:m?#]` — the tail member is not load-bearing, the two law members carry the divergence alone. Shape 49 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (43 pass / 7 abort-both, the artifact the single pre-abort → post-pass), six corpus walks exit-0 (parse 420 / feed 64 / roundtrip 6 / marshal 17 / cbor 101 / validate 5), leaks clean, 792/792 ctest on the plain build and 983/983 on the libyaml build, 185/185 fuzz-diff pins with the `flow-qhash-cr-r267.bin` seed pin (PR #1344).
  - **the nested `?{` qhash carrier — the first post-classifier escape (r269, the #1310 family)** — the diff leg's sixth fresh member and the FIRST past the #1310 classifier since PR #1346 landed (run 43554293420, job 132323530554, 44 bytes: `[=,?=,?{?#?#|#| \t ,` ` ,?\t*L:\t ,?#|#pp\n\t }\n,8]`, the auto-dictionary composition `?#?#|#|` riding the log's DE). The classifier declines the shape CORRECTLY — the divergence rides a genuinely nested structure (seq > single-pair map > nested flow map; the explicit-key wrapper `?{` carries a `{...}` collection), the models are flat-member only by design, and the mutant firewall pins that a nesting splice must never classify (the documented conservatism decline ii) — so the arm stays the second line per the escalation design, no classifier extension. The laws compose exactly as pinned since shape 1, now at nesting depth 2: members 1 (`=`) and 4 (`8`) agree; member 2 `?=` rides the plain-? law on yeptris (ONE glued plain) while libyaml's 1.1 unconditional flow KEY-token law (the qflow quirk carried since r58) opens the single-pair map around `=` with the empty value; the `?{` wrapper agrees byte-exact on both (every flow `?` is the KEY token); inside the nested map libyaml content-start comment-eats from the glued `?#` to the LF (the #949 law; the eat swallows `#?#|#| \t ,` + the backtick + ` ,?\t*L:\t ,?#|#pp`, the post-LF TAB riding the flow-blank law) and rebinds the explicit key with the empty value (the KEY-without-node empty-scalar insertion), while yeptris keeps four flat members — the `?#?#|#|` glued plain key, the backtick member plain (the m39-m48 backtick precedent), the `?`+TAB-split explicit key riding `*L` as the alias (the IS_ALPHA cut, the r142/r76 law; the `:` re-lexes as the VALUE token) and the `?#|#pp` glued plain key with the LF folding at the tail (the 1.2 break law). Minimality: two independent 100s libFuzzer minimize walks converge on the 11-byte floor `[?{?#,*|\n}]` — the `*|` alias member and the LF are both load-bearing (the LF carries the comment-eat terminator that leaves `}]` for libyaml), every smaller neighbor is both-error or arm-claimed flat. Shapes 50-51 join the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant paired battery 0 regressions (9 pass / 41 abort-both, the artifact the single pre-abort → post-pass), the classifier self-test stays 52-seed green (0 failures, tamper/mutant declines intact), six corpus walks exit-0 (snapshots 405 ledgered / parse 422 / feed 64 / roundtrip 6 / marshal 17 / cbor 101 / validate 5), leaks clean, 792/792 ctest on the plain build and 985/985 on the libyaml build, 186/186 fuzz-diff pins with the `flow-qhash-nest-r269.bin` seed pin — whose prefix deliberately stays out of the classifier's seed set and joins the nightly comparator-snapshot exclusion (PR #1348).
  - **the bare alias member joins the classifier's yeptris model — the first flat escape, retired by model-extension (r270, the #1310 family)** — the diff leg's seventh fresh member and the FIRST the classifier declined on a flat shape (run 43567360733, job 132364005031, 37 bytes: `{?#?| \t ,` ` ,?\t*L/:\t \n( ` ?,\t*L:\t \n( }`; libFuzzer floor 10 bytes `{?#,*/\n,(}`, reproducible on macOS arm64). Where the r269 escape rode genuine nesting (correctly declined, arm-pinned), this one is flat: member 1's `?#` rides the #949 laws on both sides (yeptris keeps ONE glued plain key — the `#` rides content where no blank precedes it, the `,` ends the member — while libyaml's flow `?` is the KEY token whose `scan_to_next_token` comment-eats `#,*/` to the LF and rebinds the empty-key pair, the alias never existing there), and the divergence is the yeptris-side member 2: `*/` rides the e_alias name cut as an alias event (`=ALI */` then `=ALI *L` for the `*L:` member whose `:` re-lexes as the VALUE token, `L/` ending the first name) — a member shape the classifier's yeptris model gated as structural (`*` at a member start declined). Model-extension, not an arm: the shape is genuinely flat-derivable — `fc_alias_name` already encodes the engine walk (the Y2GN interior-`:` law) and the libyaml model has carried alias tokenization + rendering since PR #1346 — so `y_member` grows the bare alias member conservatively: a `:`-blank/break boundary opens the VALUE slot (MAP only, the value node riding the existing `y_value_node` gates), a separator/closer after the blank/break run leaves the parser's empty-scalar insertion (SEQ: the alias is a bare node), and SEQ alias keys with values, `:` glued to a flow indicator, and non-boundary bytes still decline. No `src/` change — both engines obey the documented laws; the gap was the comparator's model alone. The classifier self-test stays green at 53 seeds (2835 dump-tamper declines, 94 consulted mutants all declined, 0 failures; the `flow-qhash-cr-r270.bin` seed joins the seed set — the artifact IS the seed); 50-mutant paired battery 0 regressions (16 pass / 24 pre-abort → post-classified / 6 abort-both, the artifact the single flip); six corpus walks exit-0 (parse 423 / feed 64 / roundtrip 6 / marshal 17 / cbor 101 / validate 5); leaks clean; 792/792 ctest on the plain build and 986/986 on the libyaml build, 187/187 fuzz-diff pins with the `fuzz-diff-r270-qhash-alias-soup` seed pin (#1350, PR #1349).
  - **the r261 arm regression, repaired (two-phase arm discipline, PR #1333)** — the r237 commit's NEL-site surplus eat was devouring the r173 pairing material: `yd_block_nel_walk_mult`'s eat gate is repaired and `yd_block_nel_walk` re-sequences to the r177 two-phase discipline; the pins stay 180/180.

### Changed

- **the #1310 law-verifying classifier arm lands — law derivation replaces member enumeration (PR #1346)** — the glued-`?`/#/breaks flow-soup family is measured unbounded (the r256 de-novo proof; five consecutive nightly carries r263-r267, shapes 45-49), and per-member EXACT-BYTES pins cannot keep the cadence. `yd_family_classify` (test/fuzz/family_classify.c/h) DERIVES the expected event dump per side — the yeptris model (glued-`?` members one flat plain, `?`+blank splits, the 1.2 plain fold law) and the libyaml model (flow KEY/VALUE tokens with simple-key insertion, comment-eat to the break, READ_LINE plains, empty-scalar insertion, the single-pair wrapper) — and requires both actual dumps byte-exact; one byte either model cannot place stays DIFFER (the escalation clause unchanged; block context, tags, unresolved anchors, nesting, single quotes, escapes, >16 KB all decline). The misclassification firewall rides a ctest self-test (`fuzz-diff-family-classify`): 52 soup seeds classify, 2784 one-byte dump-tamper cases decline, all 93 consulted law-violating mutants decline; the 48 existing EXACT-BYTES pins stay as the regression net. The nightly is the final referee — the next rounds either classify cleanly or name the next model gap. 984/984 ctest, six corpus walks exit-0, leaks 0, clang-format 18 clean.

### Performance

- **the six long-horizon competition fronts are CLOSED with measured verdicts** (issues #474/#739/#742/#331/#352 + the binding faces #862/#857; every attempt ledgered in `benchmarks/PERF-LEDGER.md`):
  - **perf(#352): the DQ-preservation cell rides the recorded style (PR #1328)** — parse-time plain-safety fact bits (`plain_ok`/`json_clean`) set at the `dom_set_value` SSOT; the emitter's `sc_route` answers routed styles immediately instead of re-deriving scalar safety per node. DQ cells +21.6% / +17.7%; mutation-built scalars stay conservative; byte-identical outputs.
  - **perf(parse): the per-line memo lane behind one key (PR #1329, TODO.max-perf/02)** — the three line-keyed caches (facts/li/shape) fuse into one lazy memo; `e_facts_here` dissolves. The anchor-heavy shape reads **1.34x vs rapidyaml** (the last DOM shape that trailed).
  - **perf(#739): the JSON write leg — skip_col, the json_clean fact, the fused pair walk (PR #1331)** — the per-span fold-column scan no JSON lane reads (wr_put 25.6% → 1.9%), the parse-time `json_clean` fact rides a fused quote+memcpy+quote write, a pointer walk replaces id re-resolution, and clean pair/item specialists assemble as one token (declines resolve before any byte). Write **+23%** (referee form) / **+84-88%** (campaign form); generation +19%; the parse leg verified at its ledgered floor (95.8% one fused symbol).
  - **perf(#742): the CBOR decode lane pays only for the bytes it reads (PR #1330, on main via the #1332 stack)** — the 24 KB frame-stack memset per decode call is gone, `cbor_arg` inlines with a fixed-width big-endian compose, and a byte-DFA UTF-8 validator (SWAR ASCII runs) replaces the per-string checks, verdict-identical over 2.06M differential cases. SAX decode reads **1.29-1.41x past tinycbor** on every serialbench fixture.
  - **perf(#474): the strict tape stops carrying the record region it never touches (PR #1332)** — `tape_carve(with_recs)` stops column-built routes from allocating the never-touched record region (strict tape 17 → 9 bytes/input byte) and scopes the escape scan out of the LSTR fast path. The arm64 json-users verdict is recorded structural: the per-instruction profile is a FLAT walk (max lane 4.21%), NEON engaged on both CI legs — no targeted kernel rewrite has a hot target.

## [0.6.65] - 2026-11-13

### Fixed

- **the DOM replay pairs the sibling map as a key after an anchored block-seq value (#1303, PR #1318)** — the fused writer's dash-resolves-pending branch has an anchored arm (`remediable_types: &vt` + indented dash items) that emits the root MAP_START but never arms `st[0].content_col` — the non-anchored twin arms it (the run 38128905644 bookkeeping, "the root bookkeeping the anchored branch carries") and the anchored MAP twin arms it ("the root now has content"), so the next sibling key re-fired the contentless-root law in `yt_f_open_form`, a second OPEN landed in the root's spent-key slot, and the replay paired the fresh map as a KEY with a synthesized empty scalar ([mapping, scalar ""]) while the drain walk parsed the same bytes correctly (the replay-equals-event-walk law). Surfaced through yeptris-ruby#378: `safe_load(aliases: true)` reaches the DOM tree whenever the source carries anchors, so metanorma/svg_conform's anchored configuration shape mis-nested and `dig` missed. The differential corpus now carries the family — the existing `list_anchor` entry never fired this because its root-level alias pair hits the anchored_first bail (engine restart fallback), so the repro defers the alias past the root frame — and `YTape.AnchoredSeqSiblingPairKeepsTheRootPairForm` pins the `[scalar, seq], [scalar, mapping]` root pairs structurally on both routes, verified RED pre-fix on a pristine build. 792/792 ctest, leaks 0. yeptris-ruby#378's four capability-gated DOM pins activate once this ships in a released engine tag.

## [0.6.64] - 2026-11-12

### Fixed

- **the emitter's folded-line walk refuses TAB-adjacent break points (#1280, PR #1281)** — the fold-point selection guarded space-runs on both sides but not TABs, so a fold could land with a TAB at the folded line's end (stripped as trailing s-white on re-parse) and a TAB-led run at the continuation's start (stripped as leading s-white): `serialize∘parse∘serialize` lost three bytes per pass (149 → 146) on the nightly's 154-byte artifact. Both the #1268 single-quoted walk and the plain twin take `p[i-1]/p[i+1] != '\t'`; with no safe fold point the scalar rides an unfolded line. The libyaml emission goldens stay byte-unchanged; the paired 50-mutant battery vs the pre-fix binary reads 7 FIXED / 0 regressions; 934/934 ctest with the `tab-fold-r248.bin` seed pin (nightly run 43151917346).

- **the comparator carries the nightly's residual divergence families (round r246, #1275)** — the glued-`?` × LS/PS-break flow-seq soup (the r244/r245 family's fresh permutation): yeptris keeps the glued-`?` members (`?D`+LS, `?+`+PS) one plain scalar each (the suite-pinned plain-`?` law, the #949/r244/r245 arms) and folds the exotic breaks to spaces (the 1.2 break law, the #638 family), while libyaml rebinds the glued `?` as an explicit-key indicator (the `?D` map with an empty value) and rides the raw PS byte. Every other member matches byte-exact — the divergence is exactly the two documented drift laws composing. The EXACT-BYTES `yd_r246_flow_qsoup2_gray` arm (the r121/r137 precedent — the soup permutations are not soundly generalizable): two pinned shapes (the 16-byte libFuzzer minimum `[?D<LS>,0<LF>?+<PS>]` and the 193-byte artifact), both dumps walked byte-exact. Fresh-worktree build at the pre-arm parent confirms the find persists there; 50-mutant PAIRED battery with matched standalone harness binaries on both sides reads 0 regressions, 0 arm-caught (pre verdicts differ 1 / pass 9 / abort 40 — every mutation's verdict identical on the pre-arm binary); corpus walks exit-0; 932/932 ctest; leaks clean (PR #1276).
  - **the seq-head `?#` comment-to-CR permutation (r247, #1278)** — the r244 family's escalation clause firing: the composition minus the leading `0,` prefix, so the first `?#` sits AT seq-head; libyaml reads it as the explicit-key indicator whose `#` comment eats to the CR and the post-CR tail `g"` becomes the empty-valued key, the second `?g:m?#` rebinds identically, while yeptris keeps all four members flat and plain. Shape 4 joins the `yd_r244_flow_qhash_cr_gray` arm (PR #1279).
  - **the LS-dollar seq permutation (r249, #1283)** — the soup family's escalation clause again, landing in the r246 arm itself: `[)<LS>?t,?$]` (the 11-byte libFuzzer minimum) — yeptris keeps both members flat and plain (the glued `?$` per the suite-pinned plain-`?` law) with the LS folded to a space (the 1.2 break law), while libyaml rebinds `?$` as an explicit-key indicator (`+MAP{$ : ""}` with an empty value) and rides the raw LS bytes verbatim inside the first scalar. Shape 3 joins the `yd_r246_flow_qsoup2_gray` arm; 50-mutant exactness battery (38 diverging mutants still abort, 12 genuine agreements pass); 935/935 ctest with the `flow-qsoup2-ls-r249.bin` seed pin (PR #1284).
  - **the 11-member CR/LS/PS soup permutation (r250, #1286)** — three documented laws composing across one 122-byte artifact: the r244 comment-to-CR swallow chains (libyaml renders 7 members where yeptris keeps all 11 — the `?#` comments eat to the CR, swallowing `?|` and the next member's `?` in one chain), the #949 explicit-key rebinding with differing key reads (`?2`→key `2`, `?`+CR+LS→`0 ?`/`1 ?`), and the raw-PS ride splitting the break law inside keys and the `s =<PS>1` value. Shape 3 joins the `yd_r245_flow_qsoup_gray` arm; 50-mutant exactness battery (45 diverging mutants still abort, 5 genuine agreements pass); 936/936 ctest with the `flow-qsoup3-r250.bin` seed pin (PR #1287).
  - **the r247 seed's comment-to-CR mutation (r251, #1290)** — our own pinned seed, fuzzer-mutated: a `0,` prefix plus the final quoted-CR member expanded to a long double-quoted string with `\n`/`\t` escapes (whose breaks fold identically on both sides). The divergence stays members 2-4 — libyaml reads the glued `?#` as an explicit-key indicator whose `#` comment eats `,"` to the CR (the post-CR tail `g"` becomes the empty-valued key) and `?g:m?#` rebinds identically, while yeptris keeps all three members flat and plain (the #949 law) with the quoted CR folding to a space. Shape 5 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant exactness battery (41 diverging mutants still abort, 9 genuine agreements pass); 937/937 ctest with the `flow-qhash-cr-r251.bin` seed pin (PR #1291).
  - **the colon-dropped `?gm?#` permutation (r252, #1293)** — the fuzzer mutating our own pinned seed again (21 bytes: the r247 shape minus the colon). The two documented laws again: libyaml's `?#` comment eats `,"` to the CR (`g"` rides as the empty-valued key) and `?gm?#` rebinds with key `gm?#`, while yeptris keeps all four members flat and plain with the quoted CRs folding to spaces. Shape 6 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant exactness battery (26 diverging mutants still abort, 24 genuine agreements pass); 938/938 ctest with the `flow-qhash-cr-r252.bin` seed pin (PR #1294).
  - **the brace-mutated `"}g"` tail (r253, #1296)** — the fuzzer mutating our own pinned seeds again (24 bytes: the r251 shape with the final quoted-CR member's CR turned into a `}`). The two documented laws again — libyaml's `?#` comment eats `,"` to the CR (`g"` rides as the empty-valued key) and `?g:m?#` rebinds with key `g:m?#`, while yeptris keeps all five members flat and plain; the final `"}g"` member matches byte-exact on both sides. Shape 7 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant exactness battery (30 diverging mutants still abort, 20 genuine agreements pass); 939/939 ctest with the `flow-qhash-cr-r253.bin` seed pin (PR #1297).
  - **the truncated quoted tail (r254, #1299) and the corpus treadmill fix** — the fuzzer mutating our own pinned seeds a fifth time (75 bytes: the r251 artifact with the quoted tail truncated; the two documented laws again). Shape 8 joins the `yd_r244_flow_qhash_cr_gray` arm; 50-mutant exactness battery (42 diverging mutants still abort, 8 genuine agreements pass); 940/940 ctest with the `flow-qhash-cr-r254.bin` seed pin (PR #1300). The MECHANISM is fixed too: the nightly's parser legs were writing their coverage mutations into the shared corpus the comparator leg then executed during its seed read — each red since r250 was a fresh mutation of our own soup seeds fed to `fuzz_diff` by `fuzz_parse`. The comparator leg now fuzzes its own corpus snapshot taken right after seeding (PR #1301).
  - **the mining harvest: 30 soup members + the corpus exclusion (r255, #1306)** — the first genuine diff-leg find under the isolation (`[1,?#,"\\rg",?g:"?",?g:m?#,]`, 22 bytes) revealed the family's density: a local mining loop harvested 30 byte-unique members in 30 consecutive 20s runs. All 30 ride the two documented laws and ship as shapes 9-38 in the `yd_r244_flow_qhash_cr_gray` arm, each seeded and pinned (970/940 ctest pins; PR #1307). The structural answer: the soup seeds leave the nightly's comparator corpus — the pins keep every member's exact bytes under regression, while the diff leg explores fresh space and its find rate returns to the historical equilibrium (PR #1308).
  - **the flow-map soup, de-novo (r256, #1310)** — the corpus exclusion verified (1510 files, soup seeds gone) and the family STILL produced a fresh member within 11s of the diff leg's own fuzzing: the glued-`?`/#/breaks composition is dense in input space itself, not just around the seeds. The same documented laws in flow-MAP form; shape 39 joins the `yd_r244_flow_qhash_cr_gray` arm (971/940 ctest pins; PR #1311). The gate question is named on the issue: the family is measured unbounded — the owner call is a law-verifying classifier arm or treating adjudicated family reds as non-blocking for the wave.
  - **the one-byte permutation (r257, #1313)** — the r256 flow-map soup with a single tab-for-space mutation (67 bytes): the exact-bytes neighborhood is one-mutation dense, confirming the family is unbounded. Shape 40 joins the `yd_r244_flow_qhash_cr_gray` arm (972/940 ctest pins; PR #1314). The wave posture fires: zero engine divergence is outstanding (the last engine defect was r248, #1281) — every red since r249 has been an adjudicated documented-family comparator find, and the 0.6.64 wave cuts with the carry merged.

## [0.6.63] - 2026-11-11

### Fixed

- **the emitter carries libyaml's single_quoted_allowed law (#1264's family A, PR #1268)** — a plain-unsafe scalar whose only problem is a line break rode DOUBLE-quoted where libyaml/psych single-quote it: libyaml's analyze keeps `single_quoted_allowed` through plain breaks (only special_characters, space_break — a space immediately before a break — and break_space — a break immediately followed by a space — kill it, emitter.c:1655–1693), and `block_allowed` dies on space_break/trailing_blank too (`"x \ny"` stays DQ, not `|-` with a trailing-space line). The route consults the new `writer_sq_allowed` predicate for multiline ANY/SQ-requested values (keys and flow bodies stay DQ), the blockable walk honors the block_allowed law, and the single-quoted folded walk renders content breaks per `write_single_quoted` — the first `\n` of a run writes the clip's extra break, a break run re-indents at the fold column, a trailing break re-indents before the closing quote — so `'x y z\n\n  '` rides byte-exact against psych and re-parses/re-serializes byte-stable. Latent since before the fold flip (0.6.61.1 diverges identically). Unit pin `Emit.TrailingBreakScalarRidesSingleQuoted` (the trailing-break form + the space_break control + roundtrip stability); the 94-shape dump battery reads 0/94 divergences vs psych 5.2.2 and 5.5.0 with the binding half (yeptris-ruby#374: keys ride psych's ONE visit_String law — the original #1264 premise had the psych side inverted; stdlib quotes the reshape words in key position on every version since 4.0.2).
- **the comparator carries the nightly's residual divergence families (#1265, round r244)** — the wave passes one hundred and twenty fuzz artifacts (rounds r116–r244; the [0.6.62] entry above carries r116–r243), seventeen of them genuine engine fixes, the rest comparator-side (no engine bug found; yeptris correct per the suite-pinned laws — the carry documents the exact shape):
  - **the glued-`?#` × quoted-CR flow-seq composition (r244, #1265)** — libyaml reads the glued `?#` as an explicit-key indicator whose `#` opens a comment running to the CR (the line break), swallowing `,"` plus the following quoted member's opening quote; the post-CR tail (leading spaces stripped, closing quote riding as content) becomes the map's plain key with an empty value — two `+MAP` groups where yeptris keeps `?#` one plain scalar and every member flat, the suite-pinned shape (the #949 `?#`-composition arms, r132). The EXACT-BYTES `yd_r244_flow_qhash_cr_gray` arm (the r121/r137 precedent — comment-to-CR permutations are not soundly generalizable into a structural predicate): three pinned shapes (the 18-byte libFuzzer minimum, the 24-byte artifact, the 16-byte short-tail), both dumps walked byte-exact. Fresh-worktree A/B at the pre-arm parent confirms a fresh find; 50-mutant paired battery 0 regressions (both-pass 30, both-abort 20 — every abort identical on the pre-arm binary); six corpus walks exit-0; 927/927 ctest; leaks clean.
  - **the glued-`?` × `?#` × CR/LS/PS-break soup (r245, #1269)** — the flow-? explicit-key composition soup: yeptris keeps the `?#`/`?2`/`?|` members one plain scalar each (the suite-pinned plain-`?` law, the #949/r244 arms) and folds the multiline plain keys' exotic breaks to spaces (the 1.2 break law, the #638 family), while libyaml rebinds the glued `?` as an explicit-key indicator (the `?2` map) and kills the glued-`?#` members via the comment-to-CR law; its multiline keys ride the raw PS bytes. Every other map, key, and value matches byte-exact — the divergence is exactly the two documented drift laws composing. The EXACT-BYTES `yd_r245_flow_qsoup_gray` arm (the r121/r137 precedent — the soup permutations are not soundly generalizable): two pinned shapes (the 122-byte libFuzzer minimum, the 1391-byte artifact), both dumps walked byte-exact. Fresh-worktree A/B at the post-#1268 parent confirms the find persists there; 50-mutant paired battery with matched standalone harness shapes reads 0 regressions, 0 arm-caught (both-pass 6, both-abort 44 — every abort identical on the pre-arm binary); six corpus walks exit-0; 930/930 ctest; leaks clean.

## [0.6.62] - 2026-11-11

### Changed

- **block output folds at 80 by default - psych/libyaml stored-file parity (#1257, PR #1259)** — plain and single-quoted block scalars fold at `best_width` (default 80) exactly the way libyaml's emitter does: an interior single space past the width becomes the break, the continuation re-indents to parent_col + best_indent, keys and flow members never fold, canonical/JSON keep their fixed forms; the libyaml emission goldens pass byte-exact with the flip on. `best_width` adopts libyaml's emitter-init clamp (`< 0` never wraps — Psych's `line_width: -1` unwrap; `0 <= w <= best_indent*2` snaps to 80; 0/absent = 80) and an explicit `block_fold = 0` unwraps; all option reads stay size-versioned, so pre-existing callers keep their old bytes. The #352 flat-map fused arm stays armed for fold-inert members (no interior space, or the member line ends within the width — json-users emit fully recovered); prose-heavy shapes pay the real fold work (the fold-aware fused arm is the recorded follow-up lever on TODO.max-perf/11).
- perf(scan): the line-facts sweep carries the value-side stop (TODO.max-perf/03, #1258) - the pair arm's plain value reads the fused stop2/has-high facts instead of a second stopset sweep; the fast path rides only spans above the walking scan's tiny-gate threshold, and the terminating law is re-checked per the walking scan's own rules.

## [0.6.61] - 2026-11-10

### Added

- **the stream iterator exposes its framing contract (#927, #921)** — the tolerant per-document iterator's slice offsets index the POST-GATE buffer space (a BOM strip, a UTF-16/32 transcode, or NEL/LS/PS normalization move the bytes into the iterator's owned copy), so hosts that materialize through their own value route need the pair together: `yeptris_stream_iter_slice` (added with the tolerant iterator) reports the last successful document's offset+length, and the two new accessors complete the contract — `yeptris_stream_iter_buffer` hands out the ACTIVE buffer those offsets index into (borrowed, valid until free; the caller's bytes when no gate rewrote them), and the slice accessor's contract is spelled on the header. The Python/Rust/TS drains copy frame bytes out of the buffer and re-parse them through their own drain: scanner-accurate frames, one typing SSOT, no DOM walk.

### Fixed

- **perf: the fused walk re-enters chained same-column pendings** — the
  95b4dc3 correctness bail ("chained same-column pendings keep the
  tree") dropped the ENTIRE fused route on every chained pending, and
  the deep-nesting bench shape is exactly a chained pending per level:
  DOM fell 1066 → 480 MB/s (2.2x) when it shipped in 0.6.33. The
  genuinely-unsafe sub-case was only the SAME-COLUMN resolution — the
  OPEN'd container swallowed the sibling key as its first pair's value
  because the pending's null pair never landed. The walk now resolves
  the outer pending in place: the OPEN/SCALAR form decision is
  unchanged, a same-column sibling emits the null pair immediately
  (inside the opened container under the OPEN form, completing the
  key's pair in the parent's map under the key-SCALAR form), a deeper
  line opens the pending's container and the chain nests, and anchored
  pendings keep the bail (their anchor rides the event path —
  `yt_f_open` drops the name). Same-box interleaved bench_matrix
  medians (M1 Max NEON): deep-nesting DOM 478 → 1071 MB/s (2.24x, back
  in the pre-bail ~3.2x-vs-ryml class), tape lane 447 → 968 MB/s;
  block-heavy/flow-single/wide-mapping/scalar-heavy/json-doc/
  json-users/anchor-heavy all flat (±1.3%). Gates: the 95b4dc3
  regression pins stay green, the new `ChainedPendingFamilyRidesTheFusedWalk`
  route+tree parity pin (the route assertion is load-bearing — output
  parity cannot see a route-identity break), 39-input + 60k-input
  randomized eager-vs-lazy route differentials (0 splits), 3 new
  corpus seeds, 785/785 ctest, `leaks --atExit` clean.
- **serialize_json spells the resolved VALUE (#937)** — the JSON writer printed scalars from the DOM's tag_id + source text, but the DOM stores no decoded value, so legal inputs emitted invalid JSON or wrong values: core `0x1F`/`0o7` rode their raw radix prefixes (invalid JSON), `.inf`/`.nan` rode raw (invalid JSON), and compat's 1/3/2-char true-words (`y`/`yes`/`on`, any case) all decoded `false` because the BOOL arm guessed the value from the text LENGTH. Scalar decode now lives in SSOT tables the DOM route and the writer share (`yep_tag_bool_value`'s case-insensitive y/yes/true/on vs n/no/false/off table and `yep_tag_int64`'s separators/sexagesimal/radix walk in resolve/tags.c — compat11 rides the same bool table), and the emitter carries the document's resolver so int spelling stays family-correct (leading-0 octal in compat, decimal in core; `0o` core-only, `0b` compat-only) with FLOAT words quoted in JSON mode. The 31-row typed-value JSON-validity pin plus the canonical-YAML spelling pin (the YAML face's bytes unchanged) ride the emit tests.

- **the break normalizer's phantom verbatim closes at plain continuations (#951/#952, nightly r133)** — `tok_start`, armed after every folded multi-byte break (NEL/LS/PS) with no memory of the before-break byte, let a plain scalar CONTINUATION line whose content begins with `!<` open a PHANTOM verbatim tag: everything until the next `>` rode raw (un-normalized NELs), and the plain fold emitted raw lead-stripped `0x85` bytes into the scalar value where libyaml folds the same input to newlines. The verbatim open now keys on `yep_norm_ctx.bb` — the byte immediately before the last folded break (indicator-armed token positions clear it): the r133 continuation's `!<` is content, while the legal props form (`key:` + break + `!<t> x`) opens unchanged and quotes at line starts keep their arming. The 47-byte artifact and 8-byte minimal ride as corpus seeds + ctest pins; a 14-probe matrix, byte-identical corpus failure sets, and the full ctest battery verified the fix.
- **the document owns the engine's normalized bytes (#953/#954, nightly r134)** — ASan heap-use-after-free: DOM and packed-tape scalar slices pointed INTO the engine's `norm_buf`, which `yep_engine_destroy` freed at parse teardown while the document outlives it. The finish-pool law lands on the last engine-owned buffer: `yep_engine_detach_norm_buf` parks the copy into the document's `transcoded` slot, with the lazy tape's span base rebased onto the doc-owned copy (the tape stored engine event views ABSOLUTE). ASan repro 134 → 0, leaks 0, 759/759 release units.
- **the plain-value fact closes the phantom verbatim's third arming shape (#981/#982, nightly r147)** — the #836/#951 phantom-verbatim class again, past r133's `bb` gate: a plain's `-  ` line ending `LF NEL` armed `bb='\n'` (the break run's own tail, in the indicator set), so the continuation's `!<` opened a PHANTOM verbatim and every later NEL rode unnormalized into the plain fold — the mixed render masked behind the yen-sign lead byte (`C2 A5`) shielding `yep_scan_plain`'s first-lead-only piece check. The fact that distinguishes continuation from props lands as `plain_open` in `yep_norm_ctx` (a plain scalar VALUE word is in progress: set by non-indicator content bytes, preserved across breaks, cleared at indicator-armed slots and document markers); the verbatim open additionally declines when it is set, with `bb` staying as the single-byte slice. The 103-byte artifact + 43-byte minimal seed as `nel-phantom-verbatim-r147`; paired pre/post mutation battery (0 unclassified DIFFERs), byte-identical corpus walk, legal props/stream forms normalize byte-identically.
- **the plain-value fact survives the block-context bracket closers (#997/#998, nightly r155)** — the same class's fourth arming shape: the `]`/`}` branch of the break normalizer cleared `plain_open` unconditionally while arming the token slot, but in BLOCK context a `]`/`}` byte inside an open plain VALUE word is interior CONTENT (a plain cannot start with a c-indicator — catch-85's indicator twin); only a FLOW closer genuinely ends the node. The clear is now flow-scoped (`in_flow > 0`), so the fact survives breaks and the r147 verbatim gate declines the continuation's `!<` as content; legal flow forms keep their clears byte-identically and the block props forms keep their opens. Seed `nel-phantom-verbatim-r155`; 49-pin fuzz-diff battery, 759/759 unit, paired mutation battery 0 regressions, `leaks --atExit` clean.
- **the plain scalar ends at every break lead in its continuation pieces (#1034/#1036, nightly r169)** — `yep_scan_plain`'s multibyte-break lead check tested only the FIRST C2/E2 lead in a continuation piece: a non-break first lead (the yen `C2 A5`) declined the check and NEVER examined later leads, so a NEL break run rode into the plain as raw content — violating the r74 law (commit 2164c56: plains end at NEL/LS/PS). The check now loops over every lead in the piece (a non-break lead is skipped and the next re-tested); the yen stays legal plain content (verified byte-identical).
- **the phantom-flow state no longer defeats the bracket-closer plain_open law (#1034/#1036, nightly r169)** — the #836-accepted phantom flow (`:{[[{` glued inside a block plain) inflated `in_flow` to 4 and never decremented, so the `]` of `!<1]` hit the r155 `]`/`}` arm's flow branch and cleared `plain_open` — the r155 law (block-context closers are interior content) defeated through the inflated counter, arming a phantom verbatim that swallowed the input's remaining breaks. `yep_norm_ctx.phantom_flow` counts the phantom openers alone and the clear keys on `in_flow > phantom_flow` — real closers clear byte-identically, phantom-inflated block-context closers keep the fact. The r169 artifact seeds `plain-nel-yen-ride-r169` (pin fuzz-diff-r169-plain-nel-yen-ride); 23/23 legal-form pins byte-identical, 48-mutant paired battery 0 regressions, corpus walk byte-identical, 845/845 ctest, 759/759 unit, leaks clean.
- **the block header's comment skip ends at every fold break (#1040/#1042, nightly r171)** — the normalizer's one-shot header arm (and its doc-inline and streaming twins) ended the header token at `\n` only, so on `> #<` + NEL + ` t` (breakless EOF) the NEL rode unnormalized to the engine, whose `\n`/`\r`-contract comment skip then ate ` #<`+NEL+` t` whole — an EMPTY value where libyaml (header comment stops at IS_BREAKZ, SKIP_LINE consumes the NEL) yields content `t`. The fix ends the header token at `norm_next_fold_break` (LF, lone CR/CRLF, NEL, LS/PS — folded into the token, `at_break` armed so the content line rides the bs branch); the streaming line-start gate now honors `at_break` and the space arm clears it — a gate widening this exposed (an indented `  ---` reading as a col-0 marker) was caught by the unit battery and re-pinned. Decisive controls: the ASCII twins `> #x\n t` agree byte-exact pre/post (nothing arms without the mb break); the r160 content-NEL law untouched (the NEL is on the header line, outside the content area). Seed `block-header-comment-nel-r171` (pin fuzz-diff-r171-block-header-comment-nel); 53-mutant paired battery 0 regressions, corpus walk byte-identical, 65/65 fuzz-diff pins, 760/760 unit, leaks clean.
- **the borrowed-view law bounds the input window (r171 CI carry)** — the CI legs exposed a latent heap-address-based classification in `dom_str_in`: `(v->p >= input_base)` alone decided zero-copy-vs-arena, so a view into any engine-owned buffer sitting above the input (the normalizer's `norm_buf` — which r171's header-token fold now returns for every block-scalar header carrying an exotic lead byte) was misclassified per ALLOCATOR LAYOUT, and the pair fast path's borrowed views landed in different spaces per build — the route-parity differentials (G992/6FWR under gcc-x86, T26H under TSan, MYW6 under MSVC-arm) resolved one side's views against the other's arena. The decision is now a pure function of the view's location class (input-relative only inside `[input_base, input_base+input_len)`; everything else copies — healing the UTF-16 `enc_buf` variant too), and the differential comparator decodes each side in its own DOM's space (proven in-silico: forcing the verdicts to disagree reproduces the failure class under the old comparator and zero failures under the new). Counts byte-identical pre/post on clang arm64 + gcc-16 arm64; the previously-red ubuntu-gcc/windows-11-arm/TSan/UBSan legs arbitrate green.
- **the break normalizer's phantom CR pair no longer halves the plain fold's break count (#1090/#1091, nightly r188)** — the NEL/LS/PS → `\n` normalization walk (issue #638) rewrote each exotic break to one `\n` without looking at the preceding byte, so a lone raw CR copied just before a rewrite fused with the replacement into a PHANTOM CRLF pair: the byte readers count CR LF as ONE break (SKIP_LINE's pair law) while the input's CR + NEL were TWO, and the plain fold's (n-1) newline tally silently halved — the 8-byte `;!<CR><NEL><CR>;<CR>` folded to `;!\n;` where libyaml (a verbatim reader) and the spec fold `;!\n\n;` (the minimal form `a<CR><NEL>b` shows it bare; the r47 space-vs-`\n` comparator arm had covered the single-break residue, so the family hid until the input grew a THIRD break around the pair). The escalation clause fired — a genuine engine bug in the break normalizer's copy, not a comparator carry (the A/B against the parent 4865289 aborts identically, exonerating the fresh r187 arm). The fix rewrites the WHOLE trailing run of raw CRs to `\n` before the replacement lands — one CR is one break, LFs never pair with each other, and a single-byte rewrite would just move the phantom pair one byte left (`a<CR><CR><NEL>` → `a\n\n\n` re-pairs on the first CR at the shallower rewrite; the run keeps breaks-in-breaks-out) — so the break count survives everywhere the fold law counts breaks. Seed `norm-cr-exotic-phantom-pair-r188` (pin fuzz-diff-r188-cr-exotic-phantom-pair) plus the `Parse.CrBeforeExoticBreakKeepsBothBreaks` unit pin (fails pre-fix, passes post-fix); 50-mutant paired battery 0 regressions (20 newly agreeing), corpus walk exit-0 pre/post with byte-identical outputs (all six harness corpora + schema_compile), 81/81 fuzz-diff pins, 761/761 unit, leaks clean.
- **the break normalizer's fold-line indent keeps the block arm honest (#1107/#1108, nightly r194)** — the token-space arm consumed a fold line's leading indent spaces byte-by-byte (leaving `at_break` armed), so the block-scalar content arm's indent walk counted from the FIRST NON-SPACE byte: the one-space-indented line read ind=0 and the dedent rule (ind ≤ bs_indent) ended the fold mid-content — the next byte lexed as a token and the line's `!<` armed a phantom verbatim whose breaks-ride-raw law left the NEL tail unnormalized, and the framing walk spawned a phantom empty PLAIN second document libyaml never emits (which emitted zero bytes, re-parsed as style-ANY, and re-emitted as `""` on the next pass — the 26→29-byte roundtrip instability fuzz_roundtrip crashed on). The fix is the one-arm reorder in `norm_step`: the block-scalar content arm reads BEFORE the token-space arm, so a fold line consumes wholesale FROM the leading space with its true indent — the fold survives to the col-0 marker, the NEL tail folds (libyaml parity: one document), and the roundtrip is byte-stable; the r160 content-NEL law is untouched (interior content pairs still ride the value verbatim — the content-NEL pin passes unchanged; its upstream pass rode the same phantom). Seed `fold-bang-verbatim-nel-tail-r194` (pin fuzz-diff-r194-fold-bang-verbatim-nel-tail); 52-mutant paired battery 0 regressions (16 newly claimed), corpus walks exit-0 pre/post on the six lf corpora, 87/87 fuzz-diff pins, leaks clean.
- **the value drains normalize breaks like the parse front-end (#1241/#1242, nightly r239)** — the fuzz_roundtrip marshal-contract trap (run 42786114675, main e75c3949, crash-05d604c94124e5d64489ca5700dc3879fbf139ff; 20-byte minimum `<NEL>...<NEL> >\n <NEL>...<NEL>\nt`, seed-corpus replay at MS: 0): the document PARSED (compat schema, OK) and SERIALIZED cleanly (the NEL marker composition split into two documents, the emitted YAML re-parsed), but `yeptris_marshal` returned ERROR_PARSE — neither OK nor UNSUPPORTED. ROOT CAUSE: the value drains (`yep_values_from_input`, shared by marshal, visit, and the value drain) skipped the parse entry's break normalizer — `parse_frontend` normalizes NEL/LS/PS → LF BEFORE the engine (the #638 quote-aware walk), the drain only did BOM sniffing + UTF-16 transcoding, so the raw NEL rode to the SAME compat-grammar engine, the `...` doc-marker line-start gate fired on the raw byte, and the engine errored — a state the parse path can never reach. The fix threads the same `yep_engine_normalize_breaks` step into the drain's engine branch (JSON routes stay above it, the parse law: the JSON reader keeps U+0085 raw inside strings); ownership rides the existing `transcoded` plumbing. ENGINE round #17. Unit pin `Marshal.ParseableNelMarkerDocMarshalParity` (fails pre-fix, passes post-fix); seed `nel-docmarker-drain-r239.bin` (pin fuzz-roundtrip-r239-nel-docmarker-drain-normalize); 484-mutant paired battery vs parent e75c3949 116 fixed / 0 regressions / 368 both-pass, six corpus walks green, full ctest 919/919, `leaks --atExit` clean, clang-format 18 clean.
- **the fold comment line ends at every break spelling (#1110/#1111, nightly r195)** — the r194 reorder's own regression (the artifact exits 0 on the pristine pre-r194 build and a revert-only experiment — the token-space arm moved back above the block-scalar content arm — restores exit 0, so the reorder is causal): post-reorder the content arm runs FIRST at the space byte, and its blank/comment-line branch's line-end walk looked for LF ONLY — the r19-family indent transform's verify sub indents the col-0 comment line of the 8-byte `>\n#ML<NEL>\n` to `>\n #ML<NEL>\n` and the walk consumed it wholesale with `fold_off=-1`, riding the raw NEL into the engine's verbatim block-scalar reader (an empty scalar where libyaml folds; the documented col-0-content carry lost its claim and the harness aborted on the raw DIFFER). The fix ends the blank/comment branch's line at ANY break spelling: a multibyte break (NEL/LS/PS) folds at the token tail (fold_off/fold_len, the header arm's splice pattern) exactly like the comment arm, a lone CR ends the line like the comment arm's CR case, CRLF still consumes as one break, and a breakless tail rides to EOF as before; the r160 content-NEL law is untouched (the CONTENT branch keeps its verbatim ride) and the r194 fold-line indent law is untouched (the content arm still reads before the token-space arm; the r194 seed exits 0). Seed `fold-comment-nel-tail-r195` (pin fuzz-diff-r195-fold-comment-nel-tail); 67-mutant paired battery 0 regressions (28 newly claimed; every both-abort verified aborting on the pristine pre-r194 build too — pre-existing unclaimed shapes, not r194 damage), corpus walks exit-0 pre/post on the six lf corpora, 88/88 fuzz-diff pins, leaks clean.
- **the document-root empty scalar rides the quoted form (#1127/#1128, nightly r201)** — seed-corpus replay of the 26-byte `>\n! \n !<aaa: bbb\n...\n<NEL><NEL>\n` byte-instability'd in fuzz_roundtrip ([ri] l1=29 l2=32); the A/B against the pre-r200 parent 0b67393 aborts identically — a fresh find, the r200 differ arm exonerated, and an ENGINE round (the sixth) so the escalation clause fired. An empty-root document after the `...` doc-end followed by a break-only NEL tail makes the NELs the second document's plain content — a PLAIN-styled empty root whose render rode the #290 value law (an empty plain value emits NOTHING): pass 1 serialized a bare `---` while every other empty-root spelling records style ANY and renders `""`, so pass 2 gained `""` and the canonical fixpoint serialize(parse(serialize(x))) diverged (29 → 32 bytes). The fix is emit-side, root-only, in `emit_node` (src/yeptris/emit/writer.c): a document-root empty scalar (the root sentinel, scalar kind, style ANY-or-PLAIN, zero length, non-canonical/JSON) rides the quoted form like libyaml's own `--- ''` rendering; the `k:` value law is untouched (value emission never passes the root sentinel) and both empty-root styles converge on the same fixpoint bytes. Seed `empty-root-nel-doc-r201` (the TRUE 26-byte artifact, the round-39 corpus convention; the greedy deletion minimizer's 15-byte `>\n! !<\n...\n<NEL><NEL>` changes class on every further deletion) pinned by the new `fuzz-roundtrip-r201-empty-root-nel-doc` ctest; 107-mutant paired battery 0 regressions (the 26 pre-fix-aborting family mutants all clean post-fix), the six corpus walks byte-identical pre/post at -runs=0 on the build-lf harnesses, fresh full ctest green, leaks clean.
- **the pending sibling-reopen rides the stale-frame law (#1137/#1138, nightly r204)** — the ac088c0 perf slice's in-place pending resolution walked past the old bail into the sibling-reopen shape (`b:\n c: d\nq:\n b:\n  c: d\n`): the PENDSEC phantom close (the guard that retires a spent content map when a `key:` line arrives off its column) cleared the `phantom` bit but left the frame's `content_col` standing — after `q`'s in-place key-SCALAR resolution the spent key-SCALAR slot is reused for the next pending, the stale column rode `yt_f_open_form`'s sibling test at the reopened pending's pair resolution, and the container misfiled as a key-SCALAR (`q: b` + `? {c: d}` instead of `q: {b: {c: d}}`), so serialize's bytes re-parsed into a different tree and serialize(parse(serialize(x))) diverged on round two (the nightly's run 42488986434, fuzz_roundtrip seed-corpus replay; the seventh ENGINE round — the escalation clause fired). The fix is the stale-frame law the sibling path already carries (run 38179552341): the phantom close resets `content_col` — one store; every wrong-accept the stale column could greet becomes a bail, so the change is conservative (the fused walk hands the shape to the engine rather than misreading it) and the slice numbers hold (deep-nesting DOM at the slice's 1075 MB/s level, all seven other shapes flat). The 85-byte artifact aborts on the post-slice a3f9b1e build only — the pre-slice 9c32c73 and the r203-arm-only 955df7a builds both pass, so the r203 comparator arm is innocent. Seed + ctest pin `fuzz-roundtrip-r204-pend-sibling-reopen` (the TRUE artifact; the greedy minimizer drifts to the 23-byte first-parse twin, which passes too); 100-mutant paired battery vs the pre-slice build 0 regressions + 0 fingerprint drift, all six corpus walks clean on rebuilt _lf harnesses (parse 325, feed 65, roundtrip 3, marshal 18, cbor 102, validate 5, -runs=0) with identical verdicts on the pre-fix build, fresh ctest 879/882 (the 3 failures missing-download environmental, identical on the pre-fix build), the perf pins green (`ChainedPendingFamilyRidesTheFusedWalk` + the 9-test pending family), `leaks --atExit` clean.
- **the riding flow-property run re-lexes across the line break (#1158, nightly r211)** — the flow node parse (`e_flow_node` in src/yeptris/parse/engine.c) ran `e_props` at the anchor's inline position — its loop skips INLINE space only — so on the 100-byte `[-123122kdy: &an<O*87>chor\n !!312    3123]` the cursor stopped at the break, the following `e_flow_ws` crossed it, and the `!`-led line fell into the plain branch (`yep_plain_first_ok` allows `!` as plain content): the node lost its tag entirely and glued `!!312    3123` into the scalar value where libyaml resolves the tag (`tag:yaml.org,2002:312`) with value `3123` — YAML 1.2 lets node properties ride `s-separate-in-flow` (breaks allowed) and a plain scalar must not START with the c-indicator `!`; the eighth ENGINE round, the escalation clause fired (the A/B at the pre-r210 parent 6e225e1 aborts identically — exit 134, the same DIFFER dump pair — exonerating the r210 harness change). The fix re-lexes a riding property run when the cursor lands at `&`/`!` after the ws crossing, with the duplicate-property state threaded across the calls (the 4JVG law holds across the gap: `&a\n&b x` now errors like libyaml instead of gluing `&b x`); the tag-led order (`!t\n&a x`) and the comment-separated gap (`&a\n# c\n!!t v`) ride the same loop, and same-line props, block-context props (the pend mechanism), quoted/alias/collection followers, and mid-plain `!` (`x\n!!t y` stays one folded plain in BOTH engines) are untouched. Minimality: the DIFFER-constrained greedy deletion pins the 14-byte `[y: &r\n!!312 ]` with the same dump pair; the seed `flow-props-nextline-r211` is the TRUE 100-byte artifact (pin fuzz-diff-r211-flow-props-nextline). Battery: artifact pre-fix exit 134 / post-fix exit 0; 53-mutant paired battery on the artifact and 55 on the minimum — pre-DIFFER 33/17 all fixed, 0 regressions (the p1/p2 second-anchor twins flip from tolerated YEPTRIS-ONLY-ACCEPT to BOTH-ERROR, never an abort class); corpus walks exit-0 pre/post on the six lf corpora; full ctest at the pristine baseline; `leaks --atExit` clean.
- **the anchored chained-pending keeps the engine (#1165/#1166, nightly r213)** — the nightly's round 213 catch (run 42533789224, main c73e1f0, crash-d8e3b3f392c37a62257576ce20d5a19b351f66ad) is a fuzz_roundtrip byte-instability ([ri] l1=128 l2=127), not a comparator case: the two-arm triage aborts IDENTICALLY (exit 133, the same dump pair) on cb168e1 (pre-r212-arm), 344c37f (the anchor slice), and c73e1f0 — a fresh find predating the slice (fused_run ownership needs a non-NULL resolver, so the slice's memo/placed work never runs here); the ninth ENGINE round, the escalation clause fired. Mechanism (the LAZY route only — push events, the eager DOM, and Psych all parse the shape correctly): in `yt_f_run` (src/yeptris/dom/ytape.c), for a completed root pair + a pending + a deeper ANCHORED pending + its deeper content (`2: e` / `4:` / `5: &5` / `5: e` / `7: s`), the chained-pending arm resolved the outer pending through `yt_f_open_form`'s sibling key-SCALAR form (col+1 == content_col), the spent-frame slot-reuse kept depth, and the anchored pending's event-path resolution emitted its KEY SCALAR BEFORE the anchored MAP_START — the replay paired that key with the stranded outer key and stranded the anchored map as a sibling key of the parent ({2:e, 4:"5", {5:e}&5:"7", s:null} instead of {2:e, 4:{5:{5:e}&5}, 7:s}); the OPEN-form chain is untouched (the YTP_OPEN record materializes container+key in one and pairs correctly) — only the SCALAR-form outer starves the anchored arm's assumption. The fix is the anchored-pendings-keep-the-engine law: the chained-pending arm bails when the outer pending resolves SCALAR-form and the incoming pending is anchored — every bail is a strict subset of what the engine would decide, so parity holds by construction. Minimality: greedy deletion pins the 28-byte `2: \n e\n4:\n  5: &5\n   5: e\n7: s` (eager parse correct, lazy parse corrupt — the exact route pair); the seed `anchored-pending-chain-roundtrip-r213` is the TRUE 147-byte artifact (pin `fuzz-roundtrip-r213-anchored-pending-chain`). Battery: artifact pre-fix exit 133 / post-fix 0; 55-mutant paired battery (pre-fix 344c37f vs post-fix) 24 fixed / 0 regressions / 0 mixed; corpus walks exit-0 pre/post on all six lf corpora; fresh ctest green (892); the anchor slice's pins (AliasAfterRedefinitionBindsLastDefinition / AliasMemoSurvivesAnchorAlternation) green; deep-nesting DOM bench 1075 MB/s (the >=1000 gate holds); `leaks --atExit` clean.
- **the break normalizer's phantom flow opens a phantom verbatim (#1177, nightly r217)** — the nightly's round 217 catch (run 42555057049, main 0eb3b0f, crash 13cb50cd9b9756179bbe0eb250c30a10e5f68a04) is a fuzz_diff DIFFER, and the A/B against the pre-r216 parent 27c92f1 aborts IDENTICALLY (exit 134, the same dump pair) — a fresh finding, the r216 round exonerated; the tenth ENGINE round, the escalation clause fired. For the 35-byte artifact `;!&a:\to&a-:{*,,,!<!<[NEL^3]NNa::ao\t:` (the 15-byte DIFFER-constrained minimum `;:\t:{!<[NEL^3]::`) yeptris emits five scalars in one map against libyaml's four — an empty key minted from the raw-NEL run the engine received because the break normalizer kept the whole tail unnormalized; the pure-LF twins of both inputs parse EQUAL. Mechanism (norm_step's context walk, src/yeptris/parse/engine.c): the glued `:`-twin `{` of `o&a-:{` opened a PHANTOM flow (the #836-accepted prev==`:` clause) which (1) cleared `plain_open` unconditionally and (2) inflated `in_flow`, so the plain's interior `,`s armed a props slot under the comma arm's in-flow gate; the `!<!<` at that slot passed both r147 gate inputs (`bb=0`, `plain_open=0`) and opened a PHANTOM verbatim that swallowed the rest of the input — every NEL rode raw into the engine and the raw-NEL block walk minted the extra key. The minimal variant reaches the same verbatim without any comma: the separating `:`+tab left `plain_open` dead and the value plain's own LEAD `:` (a declined `:` — the #833 predicate) never re-armed it. Three gates, all in norm_step: the phantom opener preserves `plain_open` (its bytes are interior to the open plain VALUE word — the r155/r169 law's opener twin); the comma arm keys on REAL flow depth (`in_flow > phantom_flow` — the r169 gate's comma twin); the DECLINED `:` arms `plain_open` (a `:` before a non-blank byte is interior to a plain or the plain's own lead). Seed `norm-phantom-verbatim-lead-r217` (pin fuzz-diff-r217-norm-phantom-verbatim-lead); 142-mutant paired battery (pre-fix parent 27c92f1 vs post-fix) 71 fixed / 0 regressions / 9 carried both-abort / 62 both-pass; the six corpus walks byte-identical pre/post with zero aborts on the full walks; full ctest 893/896 (the three failures the unfetched yaml-test-suite/json-suite corpora); `leaks --atExit` clean (issue #1177, PR #1178).
- **the props-led follower rides the r29 un-consume law (#1209, nightly r228)** — the nightly's round 228 catch (run 42645063770, main 77b1709, crash-8100227a1abca67d3bebc6afaa1c301090471b08) is the PROPS-LED member of the r29 flow-follower colon family: for the 11-byte `&k2: ! {\n}\n` the run between the key separator and the flow collection is the VALUE's own properties (the tag `<!>` belongs to the flow map), so the r29/r31 un-consume law — which fired only when the follower was a flow collection or block header DIRECTLY — never fired: yeptris rode the colon into the anchor name (`k2:`) and stamped name + tag on the map itself (`+MAP &k2: <!>`, no pair) where libyaml reads the anchored null KEY plus the tagged flow map. The eleventh ENGINE round, the escalation clause fired. The fix (e_props_ex, src/yeptris/parse/engine.c) extends the follower scan to cross ONE props run (`!tag`/`&anch`, verbatim `!<!..>` included — scan_prop_end ends at a blank/break like the name scan) before the indicator check, and a crossed run un-consumes REGARDLESS of what follows: the tag-led PLAIN member (`&k2: ! t`, pre-fix an anchored `k2:` scalar vs libyaml's pair) and the bare-tag/EOF tail (`&k2: !`) heal to byte-exact, and the anchor-led twin (`&k2: &v {`, pre-fix a REJECT — duplicate anchor — vs libyaml's accept) now parses the anchored pair (dumpboth byte-exact). Plain-led followers never cross a props run: 2SXE/W5VH (`&a: key`) keep riding, and the r227 root-alias family's alias follower is untouched. Three r44/r156-era pins moved with the law: the composite `&va2: &node2` (catch 156's negative) now parses libyaml's anchored pair byte-exact — 4JVG is two anchors on ONE node and still errors — and the round-44 form `&:\n! |\n:\n` now REJECTS like libyaml's empty-anchor-name law, dissolving the unserializable-block concern. Residual (documented, out of scope): the artifact itself lands as a SPLIT post-fix — yeptris rejects the multi-line flow `{\n}` whose closer sits at column 0 under the block pair (BAD_INDENT) where libyaml's span is lenient; that layer is PRE-EXISTING on main (anchor-free `a: {\n}` errors identically pre/post) and awaits its own adjudication. Seed `anchored-props-led-flow-r228.bin` (the TRUE artifact, corpus-side — the lf gate does not abort splits) + `anchored-props-led-plain-r228.bin` (the pin fuzz-diff-r228-anchored-props-led-flow replays the EQUAL tag-led-plain member; on any revert of the law it is the riding DIFFER, so regression detection is exact). Battery: 128-mutant paired battery vs parent 77b1709 — 17 admissions abort→pass, 0 regressions, 52 still-fires ALL splits under the lf DIFFER-only gate (0 aborts); six corpus walks clean pre/post; full ctest 907/907; `leaks --atExit` clean (issue #1209, PR #1210).
- **the root lead line rides the anchor spell law (#1230, nightly r235)** — the nightly's round 235 catch (run 42739927952, main 52cc501, crash-5de6f9ff) was the **fuzz_roundtrip harness's first ENGINE find**: the 19-byte `&k:": #-]<PS> ? &in~` (minimized 11-byte `&: #<PS>? &~`) parses to a root map anchored by a RIDING-COLON name — the spec's ns-anchor-char set rides `:` (and `"`) to the blank, so the map's anchor is `k:":` / `:` — and the root-props LEAD LINE spell (the catch-56 root-side law: root block-collection props ride their own leading line) wrote it UNQUOTED as `&<name>\n<content>`; that path never consulted `anchor_name_unspellable` (only emit_anchor_ex and emit_props_tail do). The re-parse mis-attaches the riding name: the minimal's `&:` line re-parses as an anchored EMPTY KEY of the follow-on collection — the map's anchor lost, a phantom pair added, s1 != s2 with byte growth (the serialize never converges). The fix (the r235 law, the sixteenth ENGINE round) gates the root lead line's anchor write on the SAME spell law as every other anchor write path: a colon-tailed (or break-named) anchor on a root block collection has NO safe lead-line spelling, so the serialize FAILS — the r228 `&: |` law's root-side twin. Normal anchored root maps (`&a\nb: 1`, `&a:b\nk: v`) keep the lead-line spell — the law fires only on names that cannot re-parse. Regression triage CLEAN: the pre-r235 parent 03526bb aborts identically. Seed `roundtrip-root-anchor-colon-lead-r235.bin` (pin fuzz-roundtrip-r235-root-anchor-colon-lead); 360-mutant paired battery vs parent 103 fixed / 0 regressions / 257 both-pass, six corpus walks clean, full ctest 915/915, `leaks --atExit` clean, clang-format 18 clean.

- **perf: the lenient tape's clean-document fast path drops the whole-pass residuals (#1077)** — the ledgered 0.49x was a regression, not the item's floor: two whole input passes had landed on the lenient route since the 0.66x measurement. The col-0 `\n\t` continuation sweep (the Y79Y#4 defer, b97f147) rode a two-compare byte loop on every document and now rides memchr, skipped on the clean_only lane entirely (that lane's byte guarantees prove the pair cannot exist on an accepted span — the defer preserved by construction); the >1024 simple-key verdict settles at ldone when no key crossed the raw cap (chars ≤ bytes closes the char budget), ending the #739 long-key re-walk on every clean document. Same-run interleaved bench_matrix medians (M1 Max NEON): tape_lenient json-doc 490 → 798 MB/s (0.49x → **0.78x** simdjson, 1.63x), parse_json DOM 503 → 865 MB/s (**1.72x**), tape_lenient json-users 614 → 1094 MB/s; parse_json_tape flat (sanity). Mask consumption (TODO.max-perf/07 cut 2) is closed DEAD BY BOUND and ledgered, not built — stage-1 masks measure 3907 MB/s on json-doc, and the content-bound walk floor plus the gap-verification re-scan already straddles the parity budget.
- **the comparator carries the nightly's residual divergence families (#929–#935 + #939–#947 + #950 + #956/#958/#960/#964/#966 + #977/#978 + #979/#980 + #983/#984 + #985/#986 + #987/#988 + #989/#990 + #991/#992 + #993/#994 + #995/#996 + #999/#1000 + #1002/#1003 + #1004/#1005 + #1006/#1007 + #1008/#1009 + #1010/#1012 + #1014/#1015 + #1016/#1018 + #1019/#1021 + #1022/#1024 + #1025/#1027 + #1028/#1030 + #1031/#1033 + #1037/#1039 + #1043/#1045 + #1046/#1048 + #1049/#1051 + #1052/#1054 + #1055/#1057 + #1059/#1060 + #1062 + #1206 + #1212 + #1215 + #1218 + #1221 + #1224 + #1227 + #1230 + #1233 + #1236 + #1239 + #1246 + #1249 + #1252 + #1255, rounds r116–r243)** — one hundred and nineteen fuzz artifacts across the family shapes; r133/r134, r147/r155, r169, r171, r188, r194, r195, r201, r204, r211, r213, r217, r228, and r235 were the sixteen ENGINE fixes above, the rest comparator-side (no engine bug found; yeptris correct per the suite-pinned plain-'?' law and the 1.2 break law):
  - **the glued-`#` qflow family (r116–r120, r124–r127, r130–r131; #929/#930/#933/#940/#941/#942/#946/#947)** — libyaml's comment-kill at a glued `#` empties flow keys or splits values where yeptris keeps the run as content; `yd_qflow_glued_hash_differ` grew, round by round, to the content-agnostic emptied-key pair, the suffix-verified SPLIT pair, the L-only val skip, the yeptris-only closer skip, the mid-stream killed-content skip, the ALIAS realign, the content-agnostic kill, and finally the STRUCTURAL-REALIGN arm (#945: libyaml's kill re-pairs the continuation under a +MAP wrapper — skip yeptris's killed VAL and libyaml's opener, bank one owed closer). The r121 permuted-member and r129 mutation-soup inputs ride as EXACT-BYTES claims (#934, #944) — pinned verbatim because the permutations are not soundly generalizable. The family's skeleton algebra is complete.
  - **the glued-`?` x glued-`#` composition (r132, #949/#950)** — the two laws composed in new positions: libyaml comment-kills the `?#`-led seq entry (the SHARED-OPENER arm skips only yeptris's killed VAL when yeptris's next line IS libyaml's current opener — the r130 consume-both semantics would eat a member both walks carry) and wraps the tail's `&m-` in an explicit-key map under its anchored-empty key (the structural arm claims it unchanged); the 8-byte minimal `[?##\n&m-]` pins the wrapper shape.
  - **the flow-`?` indicator compositions (r135/r136, #955/#956 + #957/#958)** — libyaml's flow-`?` KEY token fires with ANY follower: the r135 TAG-RIDING key twin (`?!`-led keys whose glued `!` becomes libyaml's invisible non-specific tag — yeptris keeps the run as content; the twin arm bounds the gap to `!`s/spaces/break-escapes with a suffix verify) and the r136 `?!` TAIL WRAPPER (libyaml's glued-`?` opens the explicit-key map under the invisible tag — the pass-1 marker now scores glued `?!` beside `?#` and the existing structural arm claims it unchanged).
  - **the soup siblings (r137–r140, #959/#960 + #961/#962 + #963/#964 + #965/#966)** — four flow-map soups where libyaml's comment-glue closes the map many pairs before yeptris (yeptris rides the plain-'?' law to the closer, often through an `=ALI` key libyaml swallows whole): member-count skew is the GENUINE member-drop-bug signature, ruled unsound to generalize (the r121/r137 precedents), so each rides as an EXACT-BYTES gray claim (`yd_r137/r138/r139/r140_soup_gray` — four static seeds, 997/66/35/33 bytes, prefix-minimal by bisect).
  - **the soup neighborhood closes (r141–r144, #963/#964 + #965/#966 + #967 + #968/#969 + #970/#971 + #972/#973 + #974/#975)** — the nightly's byte-flip walk through the soup family exposed that exact-bytes claiming can never converge (one mutation = one red round), so the family graduated from gray claims to two BOUNDED ARMS: the TERMINAL-TAIL arm (#971 — fires only when libyaml's remainder is closers-only and yeptris's tail is 1..64 kept-content member lines, one-shot exhausted; a mid-stream drop negative still aborts, and 20/20 mutated r137-artifact variants classify) and the WRAPPER RIDE (#973 — libyaml's `?+#` salvage block rides whole when its KEY suffixes yeptris's killed member; the r130 suffix-pair and r132 comment-glue paths untouched), plus the ALIAS-SWALLOW RESIDUE arm (#975 — an `=ALI` may ride against libyaml content only when the IDENTICAL alias survives elsewhere in libyaml's dump, re-paired by the KEY-token law and never dropped; the r138–r141 no-libyaml-alias gray claims keep their exact paths).
  - **the plain-NEL fold family (r118/r119/r122/r128, #931/#932/#935/#943)** — raw U+0085 breaks: libyaml (1.1) folds a single NEL to a space and keeps k≥2 runs as k-1 literal breaks where yeptris (1.2) folds all; `yd_dump_eq_mod_nel_fold` walks the input's NEL runs and aligns all the fold boundaries — the mid-content space, the run's k-1 escaped breaks, the EOF break that vanishes instead of folding, and (r128) the LONE raw 0x85 byte spelling with its lead-byte guard. The r130/r131 structural twins would have ridden exact-bytes claims forever; the structural arm retired the pattern.
  - **the wrapper-ride family converges (r145/r146/r148/r149/r152/r153, #977/#978 + #979/#980 + #983/#984 + #985/#986 + #991/#992 + #993/#994)** — libFuzzer's real mutation operators walked the soup family round by round and the arms grew to cover each new shape: the pass-1 family marker learned the PLAIN-RUN law (a quote opens only where a node can start — r145's mid-plain `"` was phantom-swallowing the whole input and starving the entire arm chain behind a dead gate), the #971 terminal-tail ride cap now rides the classifier's own input bound instead of a fixed 64 (r146's 730-byte splice doubled the kept tail to 96 members past the old cap), the wrapper KEY's trailing render blank strips when the line is props-only (r148's `&m`-anchored wrapper whose KEY rides PROPS-ONLY), RE-PAIRED keys and values ride when libyaml's comment-glue re-pairs the pending KEY token onto a LATER member (r149: the next member's plain tail as a byte-exact KEY suffix; r152: the member's nested flow map — yeptris's kept residue VALs step aside to their own byte-exact copy of libyaml's opener under a 16-line run cap), and inside the structural branch the byte-exact-verified rides attempt FIRST with the bare shared-opener tell as their FALLBACK (r153's three-wrapper stack died on claim PRIORITY, not absence — the ride's four-line block is strictly stronger evidence than the tell's one compared line). The r151 one-byte-deletion sibling of the r138 seed rides as an EXACT-BYTES gray claim (#989/#990 — the deletion operator is outside the gray claims' flip-only batteries; generalization stays ruled out per the r121/r137 precedents).
  - **the LS/PS-fold pair arm (r150/r162, #987/#988 + #1014/#1015)** — plain fold runs mixing LS/PS (U+2029) into the LF/CR breaks: when both lines are VALs and libyaml's content is yeptris's with exactly one raw LS/PS triple inserted byte-exact (+3 length pins the count, two memcmps pin the position — the #638 forced-ADDITIVE law), the pair consumes as ONE paired diff; per-fold-site cost drops from 4 generic content-agnostic skips to 0. r162 adds the SUBSTITUTED pair form (a CR+PS break run folding to one yeptris blank where libyaml's READ_LINE eats the CR and joins the PS raw — delta 2, position-wise walk, pointers never rewind) beside the additive predicate, family-gated by the pass-1 `?#` marker.
  - **the BOM residual family (r154/r156/r159, #995/#996 + #999/#1000 + #1006/#1007)** — two UTF-8 BOMs then `&U:`: yeptris keeps the second BOM as key CONTENT (catch-121: one mark stripped, content BOMs ride) while libyaml skips a column-0 BOM too (its encoding-mark law is not stream-position-bound), leaving an ANCHORED EMPTY SCALAR whose residual is itself the r76 anchor-keymap shape; the twin inside `yd_bom_content_divergence` files the stripped residual into libyaml's prop form — r156 generalized the insertion point from the line's end to after the maximal run of 1.1 anchor-set bytes (the separator blank lands after the name, the glued tail byte-for-byte preserved), so the r154 bare form is the tail-empty special case the same law degenerates to. r159 rides the UTF-16-transcoded composition (BOM + NEL + mid-stream FEFF + NEL + plain): libyaml's column-0 skip consumes the transcode-residual BOM where yeptris rides it as the plain's first line, folding the second NEL to one fold space — the new fold-space-head twin strips the `EF BB BF 20` pair, encoding-symmetric (the LE twin classifies through the same relation).
  - **the break-law compositions in block scalars (r158/r160/r161, #1004/#1005 + #1008/#1009 + #1010/#1012)** — r158: a literal block (`|`, clip) whose blank lines surround an LS-only line and whose final line ends breakless at EOF — the COMPOSITION of two already-adjudicated carries (libyaml's READ_LINE joins the LS triple verbatim, #638; the clip's trailing break survives breakless EOF per suite L24T#2, r140), each law-clean alone but neither arm absorbing the other's artifact; the composed-transform arm appends the break libyaml drops and re-walks libyaml's dump through the existing LS-fold relation. r160: the folded-block content law — inside block-scalar content the 1.2 break set is LF/CR/CRLF only, so yeptris keeps a content NEL as its full 2-byte pair (byte-verified: the nightly's `!<^E` glyph was the renderer's view of C2 85, NOT the phantom-verbatim signature) where libyaml's 1.1 IS_BREAK splits inside the block scalar and folds; the block-content arm aligns k NEL pairs against libyaml's (k-1) escaped breaks plus exactly one realignment byte, byte-bounded beyond. r161: the arm's terminal-tail branch — libyaml's escaped `\n` behind the consumed space is its CLIP break (not a fold), so yeptris's own escaped clip break strands; the bound branch accepts `SP+\n(esc)` vs `\n(esc)` only when the remainders behind both escapes are byte-identical. r173: the run's-exit re-alignment generalizes to a bounded CONTINUATION — up to k space↔break pairs (one-for-one) plus the surplus spaces (yeptris's kept trailing + fold spaces where libyaml ate the indent), budget capped at the run's own length k, every iteration advancing at least one pointer, the r161 end-anchor still binding — carrying the NEL-run-at-line-end-with-fold-continuing composition (` !<`+NEL²+` ` lines where yeptris's phantom-verbatim-denied content rides raw). r175: the fourth continuation flavor — yeptris's escaped CLIP break with no b counterpart (the ` ! `-prec-line composition: the (k−1) collapse consumed libyaml's single break and yeptris's clip break faced end-of-value); the surplus is always ≤ k (clip keeps exactly one break), the r161 end-anchor untouched. r176: the family walk extracted verbatim as `yd_block_nel_walk` and the arm gains the r158-style composed transform — on a breakless final byte (the clip precondition), the one break libyaml drops is appended and libyaml re-dumped through the same walk; the LS/PS composition stays with r158 (whose triple-gate was the proven miss: the PS/LS twins of the exact shape were already green). r177: the fold-suppression flavor — a line ending in a NEL break followed by a blank-ish continuation line makes libyaml's 1.1 fold drop the fold space entirely (the space writes only when the boundary carries no blank-line breaks, scanner.c's `*trailing_breaks.start == '\0'` gate) where yeptris keeps the NEL pair as content plus both the line's trailing space and its fold space; the walk goes two-phase — the original budget-k walk first (every member that rode before r177 keeps its exact path), then on a decline the budget-2k retry whose surplus steps absorb the suppressed-fold space (a plain budget bump was rejected: it flipped the r173 seed's more-indented sibling from ridden to aborting).
  - **the block-NEL walk's LS/PS raw-keep head delta rides the r160 family (r197, #1116, nightly)** — the r160 content law composed with the #638/r158 raw-keep law at the walk's head (the 23-byte `> #<11<CR><PS> 700:<LF>  m<NEL> t`): libyaml's block reader keeps the leading PS triple RAW where yeptris spells it LF (the documented normalization, uniform since r171), so `yd_block_nel_walk_mult`'s byte-bound main loop died on the PS head delta before the NEL run was ever reached (libyaml eats the mid-content NEL as a 1.1 break where yeptris keeps it raw per spec 8.1 — the r158 walk owns the spelled-vs-raw polarity but has no arm for the NEL polarity); the carry is the LS/PS raw-keep tolerance inside the main loop (a 2-vs-3-byte window firing only on the documented delta), after which the existing NEL run branch plus the r176 append-LF transform own the whole relation — the A/B at the pre-r196 6341eb4 build aborts byte-identically, a fresh find, not a regression. Not an engine bug — yeptris is suite-correct on every delta (the 1.2 content NEL, the documented LS/PS normalization, the L24T#2 clip pin).
  - **the block-NEL family's LS/PS carriers open the gate (r202, #1130, nightly)** — a 23-byte literal block (`|` + blank-space content lines + two raw U+2028, breakless EOF) carries LS only and no NEL byte anywhere, so `yd_block_nel_content_divergence`'s NEL-presence gate declined before the walk could speak while libyaml keeps the raw LS triples mid-content (scanner.c READ_LINE's LS|PS branch) with no trailing break at breakless EOF against yeptris's all-LF normalization; the gate now also admits the raw LS/PS triple (E2 80 A8/A9, the tail's 2-byte NEL coverage untouched) and the byte-bound walk rides the r197 raw-keep tolerance composed with the r176 trailing-break transform and the r140 breakless-EOF clip. Not an engine bug — the A/B at the pre-r201 parent aborts identically, and YAML 1.2 permits both readings (production b-break vs b-as-space).
  - **the block-NEL family's aligned LS/PS-triple indent surplus rides the walk (r205, #1140, nightly)** — a folded block scalar whose content lines carry a MID-LINE PS and mid-line NELs (the 39-byte `> #<11<CR><PS> 70<PS> 700:\n  m<NEL> t0:\n  m<NEL> t`, run 42492751265, main a0f2ff6): yeptris's normalizer rides each bs content line wholesale to its `\n` (the r194 law), so the mid-line exotic breaks stay raw content and the continuation's leading space is kept, while libyaml breaks the line at EVERY NEL/LS/PS (IS_BREAK), keeps LS/PS verbatim through READ_LINE, normalizes NEL to `\n`, and eats the continuation's indent per split — three adjudicated deltas compose: the r197 spelled-`\n`-vs-raw-triple head, the NEW aligned-triple surplus (both dumps carry the raw PS at the same offset; yeptris keeps the space libyaml ate), and the r140 breakless clip tail (the r176 composed transform already owns it); the carry in `yd_block_nel_walk_mult` is the aligned branch (both dumps' raw triples coincide) skipping both triples and eating yeptris's SURPLUS spaces — only while the dumps disagree, capped at 64 (the block's indent column), so a space both dumps carry stays byte-bound and the claim cannot outrun the delta. Not an engine bug — the A/B at the parent a3f9b1e aborts on BOTH builds, a fresh find (the r204 engine fix is innocent). Greedy deletion pins the 34-byte `>\r<PS> 70<PS> 700:\n  m<NEL> t0:\n  m<NEL> t` with the byte-identical dump pair; the seeded pin is the TRUE 39-byte artifact (`block-nel-ps-aligned-indent-r205`, pin fuzz-diff-r205-block-nel-ps-aligned-indent); 52-mutant paired battery 0 regressions (30 carried, 22 gained — the family's own mutants), corpus walks pre/post byte-identical, `leaks --atExit` clean.
  - **the block-NEL family's surplus-break exit rides the walk (r208, #1149, nightly)** — a 25-byte root folded block scalar whose content carries raw NELs (run 42507908569, main 0398dc6): yeptris keeps each NEL as 1.2 content (the block-scalar break set is LF/CR only, spec 8.1) where libyaml's 1.1 reader breaks on it too (its IS_BREAK) — the r160 block-NEL family with a delta its walk arms all decline: at the run's exit libyaml's collapsed escaped-break run is exactly k LONGER than yeptris's (its collapse counts the NEL-turned-breaks while yeptris's counts only the real breaks around the kept bytes — the artifact's first site: yeptris NEL + 1 escaped break from the CR CR pair, libyaml 2 escaped breaks from NEL CR CR), with the content behind both runs coinciding (the r161 terminal tail is the disjoint identical-remainders shape, the (k−1) collapse eats only k−1 of b's escapes, and the r173 continuation never eats b escapes — the surplus rode no branch). The carry is the surplus-break arm in `yd_block_nel_walk_mult`, consulted after the r161 terminal-tail arm declines: eat both escaped-break runs, require the surplus == k and the alignment byte behind the runs to coincide (and not be an escape), then rejoin the byte-bound walk — strictly claim-additive (pointers never rewind, the surplus bound by the run, the site pinned by the alignment byte, so b-surplus escapes beyond the collapse can appear in no carried pin). Not an engine bug — the A/B at the parent 20e01be aborts on BOTH builds, a fresh find (the input has no `?`, the r207 arm untouched). The greedy minimizer drifts to the 9-byte `>\n<NEL SP NEL NEL` split twin (a DIFFERENT delta — yeptris leads with an escaped break where libyaml folds — documented, still open); the DIFFER-constrained rerun pins the 14-byte `>\n <NEL CR CR <NEL NEL`; the seeded pin is the TRUE 25-byte artifact (`block-nel-surplus-break-r208`, pin fuzz-diff-r208-block-nel-surplus-break); 110-mutant paired battery 0 regressions (23 gained — every one a NEL carrier with the artifact's shape; 60 carried both-abort, the ledgered split zoo unchanged), the six corpus walks byte-identical pre/post, `leaks --atExit` clean.
  - **the block-NEL family's doc-end terminator rides the walk (r214, #1168, nightly)** — a 14-byte root folded block scalar whose col-2 content is `<` + raw NEL + `...` + NEL + SP + LF (run 42538075383, main dc73734): at the FIRST NEL libyaml's 1.1 reader breaks the line, sees the next line — the `...` document-end marker — at column 0 under the col-2 content indent, and ENDS the scalar there (its value `<` plus one kept break; the marker line leaves the value entirely), while yeptris never broke at the NEL (the documented 1.2 LF/CR-only break set inside block-scalar content) and keeps the NEL run AND the whole terminator line as content; no arm owned the shape (r208's surplus law needs the alignment byte to coincide, the r19 col-0 indent transform cannot fix a break that never happened, r176's composed transform declines on the input's trailing break). The carry is the terminator law in `yd_block_nel_walk_mult`, fired LAST so every existing member keeps its exact path: yeptris's remainder is a SINGLE-LINE surplus (plain bytes plus kept raw NEL/LS/PS) then its ONE trailing escape against exactly one libyaml escape, the remainders behind BOTH escapes byte-identical to the very end (the r161 complete binding) — every step advances the yeptris pointer, so the walk stays bounded and nothing past the scalar can ride. Not an engine bug — the A/B at the pre-r213 parent c21173c aborts identically (exit 134, the same dump pair; the probe's byte-identical engine dumps exonerate the r213 anchored-chained-pending bail and the escalation clause does not fire), a fresh find. Greedy deletion pins the 11-byte `> SP <NEL...NEL` (the same terminator delta — yeptris's trailing escape there is the suite-pinned breakless-EOF clip break, the same branch rides it); the seeded pin is the TRUE 14-byte artifact (`block-nel-terminator-docend-r214`, pin fuzz-diff-r214-block-nel-terminator-docend); 80-mutant paired battery (parent c21173c vs post-arm) 14 fixed / 0 regressions / 49 carried both-abort, corpus walks exit-0 pre/post on all six lf corpora, `leaks --atExit` clean.
  - **the block-NEL family's fold-space and fold-break surplus sites ride the walk (r215, #1171, nightly)** — a 21-byte root folded block scalar whose two content lines carry raw NEL² runs mid-line with content chars around them (run 42543960307, main d380532): yeptris keeps the NELs as 1.2 content (the documented LF/CR-only block-scalar break set) with the fold spaces written at the line boundaries and the trailing kept space riding, while libyaml's 1.1 reader breaks AT the NELs, its blank-line accounting suppresses the fold space, and the break run opens with the boundary break; no arm owned the shape — the SPACE-vs-escape delta in front of the kept NEL run stopped the byte-bound loop before the NEL branch's gate was ever reached (r161/r208/the (k−1) collapse/r173/r214 all declined, and r176's composed transform declines on the input's trailing break). The carry is two arms in the mismatch handler of `yd_block_nel_walk_mult`, fired LAST so every existing member keeps its exact path: the fold-space arm eats only the one space (gated on the NEL pair immediately behind it and an escape on libyaml's side) and rejoins the loop top where the branch's own laws reconcile the rest under their existing budgets; the fold-break-surplus arm owns the drift sibling's site — yeptris rides x escapes then the k-NEL run against libyaml's y, and behind the run yeptris carries exactly (x − y) surplus escapes then libyaml's remainder byte-identical to the very end (the r161 complete binding; the empty-value drift twins need (x − y) escapes yeptris does not carry and keep aborting — a different mechanism, documented not carried). Not an engine bug — the A/B at the pre-r214 parent 4ac09a3 aborts identically (exit 134, byte-identical dump pair on both builds), a fresh find. Greedy deletion pins the 14-byte minimum `>\n <\u{85}² \u{85}²` (reachable from the seed by two deletions); the seeded pin is the TRUE 21-byte artifact (`block-nel-foldsurplus-r215`, pin fuzz-diff-r215-block-nel-foldsurplus); 83-mutant paired battery (parent 4ac09a3 vs post-arm) 24 fixed / 0 regressions / 21 carried both-ride / 38 both-abort, the six corpus walks byte-identical pre/post (full parse walk clean at 245 inputs incl. the new seed), `leaks --atExit` clean.
  - **the block-NEL family's fold-boundary surplus rides the walk (r216, #1174, nightly)** — a 35-byte root folded block scalar whose first content line is a bare NEL, with a blank line and two `<NELNEL SP` members on top of the r215 shape (run 42549204693, main 27c92f1): yeptris keeps the NEL as 1.2 content (the documented LF/CR-only block-scalar break set) and writes the fold space at the line boundary, while libyaml's 1.1 reader breaks AT the NEL and keeps the boundary break too — the r160 family's MIRROR polarity (libyaml's break run one escape longer at a plain content byte): the walk enters the NEL branch (k == 1), the (k−1) collapse has nothing to eat, and the r173 continuation spends its whole budget pairing yeptris's fold space against ONE of libyaml's two breaks — pointers never rewind, so libyaml's boundary break survives with no yeptris counterpart and the byte-bound loop stops at escape-vs-content, where every existing arm declined (the r215 arms own only the a-side surpluses; r197 owns a's spelled break). The carry is the fold-boundary arm in `yd_block_nel_walk_mult`'s mismatch handler, fired LAST (the branch's previous unconditional decline — every existing member keeps its exact path): it eats libyaml's ONE surplus escape, gated hard — the run is exactly one escape (a wider surplus keeps aborting — its own round if it ever surfaces), the alignment byte a PLAIN content byte (not a space — the r173/r215 sites — not an escape, not a raw NEL/LS/PS lead byte the NEL branch owns), and the byte behind libyaml's escape EQUALING it, so the walk rejoins byte-bound at an agreement byte and the relation behind the site stays owned by the loop and the NEL branch's budgeted laws — nothing past the agreement byte rides. Comparator carry only — the 1.2-keeps-NEL reading is the adjudicated suite-side polarity. Not an engine bug — the A/B at the pre-r215 parent d380532 aborts identically (exit 134, the same dump pair on both the artifact and the minimum), a fresh find. The DIFFER-constrained greedy deletion pins the 8-byte `>\n [NEL][LF] <`; the seeded pin is the TRUE 35-byte artifact (`block-nel-foldboundary-r216`, pin fuzz-diff-r216-block-nel-foldboundary); 268-mutant paired battery (parent d380532 vs post-arm) 41 fixed / 0 regressions / 160 carried both-abort / 67 both-pass, the six corpus walks byte-identical pre/post (full parse walk clean at 336 inputs incl. the new seed), `leaks --atExit` clean.
  - **the root-alias collapse (r157/r170, #1002/#1003 + #1037/#1039)** — `&a:o:` + indented `*a:`: yeptris's suite-pinned riding laws (2SXE/W5VH — colons inside anchor and alias NAMES) consume both lines whole — line 1 is a props-only anchor declaration (the anchor pends), line 2's riding alias name swallows the colon so no value indicator ever opens a map, and the document collapses to ONE root alias carrying the pended anchor as props (raw-event probe verified, re-verified on post-r169 main; the undefined alias rides at the event route by design, libyaml's parser likewise never validates, the suite pins no error). libyaml's 1.1 charset cuts compose its anchored-key + nested-alias-key map. 1 event vs 9 with an event-KIND divergence — exactly the "yeptris =ALI vs libyaml anything" mask the r137-r139 verdicts ruled unsound for twin arms, so the composition rides EXACT-BYTES gray claims; r170's no-indent sibling (`&a:o:\n*a:` — the deletion operator again outside the flip-only batteries) takes its own static seed, whose trailing break is NOT load-bearing (prefix 9 diverges identically, documented in the pin; the breakless twin stays unclaimed beside the pinned cut). r174's doubled-head form (`&a:o&a:o::` + `*a:`) takes a THIRD static seed (the interior `&` riding the anchor name is `yep_scan_prop_char`'s own reading — law-clean, probe-verified; libyaml's IS_ALPHA cut re-lexes the residual as plain content one `&` run richer); the `::` tail is not load-bearing (siblings named in the pin for their own entries). r182's indented colon-head twin (`&a::` + an indented `*a:` — the r157 seed with the head's `o` deleted, the r151/r170 deletion-sibling precedent again) takes a FOURTH static seed as the exact-bytes gray arm `yd_r182_root_alias_colonhead_gray`: with `o` gone the head is props-only — yeptris's suite-pinned laws (2SXE/W5VH — colons ride anchor and alias NAMES) pend the anchor `a::` and let line 2's alias name `a:` ride its colon, so no value indicator opens a map and the document stays the ONE root alias (`=ALI *a:`) — while libyaml's 1.1 IS_ALPHA cut re-lexes the residual `::` as plain `:` plus the value-token colon, opening a root map whose nested map keys on the alias (1 event vs 9, both accept); the prefix chain diverges from the head-only `&a::` member on, so the pin is byte-exact to the artifact (the A/B at the r181 parent aborts identically — a fresh finding).
  - **the alias rides the kept-content tail (r163, #1016/#1018)** — the soup family's alias-swallow composition in the REVERSE orientation: libyaml's comment-glue ate `?##` through its flow-KEY token and re-paired `*5` as an ALIAS (`=ALI *5`) where yeptris keeps the run as folded plain content (the plain-'?' law; `*` mid-plain is content for BOTH engines — verified by agreeing probes). The r144 survival arm demands the alias survive elsewhere (it does not); r125 alias-realign demands yeptris carry its own =ALI (it never does). The paired-consume arm — oriented so libyaml's alias is never dropped (the sound reverse of the r137-r139 event-kind mask) — requires libyaml's full `=ALI <name>` rendering byte-exact as a suffix of yeptris's kept content, a `?#`/`?!` glue fingerprint in the pre-spelling prefix, the pass-1 family gate, and both lines advancing (bounded by construction; the claim stands only on full exhaustion).
  - **the LS/PS fold-count pair form (r164, #1019/#1021)** — the soup pair arm's third relation: at break runs like CR LF CR CR PS SP, yeptris renders three escaped breaks where libyaml's READ_LINE joins the PS raw and folds only the CR/LF count (yeptris's 2-byte escape vs libyaml's 3-byte triple at the site, delta +1 — neither the r150 additive pin (+3) nor the r162 substituted walk (blank-vs-triple) accepts it). The position-wise `yd_qgh_lsps_escbreak` walk consumes both contents byte-pinned at the sites (backslash+n vs the raw triple), sites > 0, bounded by construction. The r164 mega-artifact (1198 bytes — the family's largest) died on the shared 64-skip cap at skips=65 with every difference law-explained; the pair form burns 0 skips per site and the walk exhausts.
  - **the BOM-fold blank-line break run (r165, #1022/#1024)** — the r159 twin's second fold form: a THIRD NEL after the FEFF content creates a blank line, so yeptris's stripped residual opens with the `EF BB BF` + two-byte `\\n` ESCAPE (the k≥2 run → k-1 escapes law, r118/r119/r122/r128) where the space gate demanded exactly one fold space. The break-run head twin added BESIDE the space gate: a maximal escape run followed by non-space content strips with the BOM, exact strcmp against libyaml — byte-pinned (a run is never a value's tail: trailing breaks strip, r122), encoding/break-symmetric (the LE twin, LS/PS substitutions, mixed NEL+LS, k=3/k=4 all classify).
  - **the UTF-16 NEL line-start BOM rides the r100 arm (r226, #1203, nightly)** — the 14-byte UTF-16BE `mark<NEL><BOM><NEL><BOM><NEL>a` (run 42622039396, main 892510b, crash-c76eee55): libyaml's scan_to_next_token drops a BOM at mark column 0 whatever break opened the line (the r154 law, byte-probed across LF/CR/NEL/LS/PS), while yeptris keeps every content BOM (the catch-121 law) — yeptris's plain reads `<BOM> <BOM> a` (the interior NELs folding) against libyaml's bare `a`; the r100 arm's line-start gate tracked LF only, so both NEL-line-start BOMs slipped past it (the feature never fired), and the r159 single-(BOM+fold) strip reaches only one pair, so the two-BOM composition stayed unknown. The carry is the gate widening alone in `yd_bom_utf16_line_start_divergence`: the UTF-16 unit arm opens a line after LF, CR, NEL, LS, or PS, and the UTF-8 byte arm's removal/replacement probes treat C2 85 and E2 80 A8/A9 as line starts in lockstep (the twin and skeleton-equality gates untouched). Not an engine bug — the A/B at the pre-r225 parent fe760cd aborts identically, a fresh find. Seed `bom-utf16-nel-line-start-r226.bin` (pin fuzz-diff-r226-bom-utf16-nel-line-start); 60-mutant paired battery vs main 892510b 0 regressions (27 admissions abort→pass), the six corpus walks byte-identical pre/post, fresh ctest fuzz-diff 116/116, `leaks --atExit` clean; minimality pinned to the 10-byte `mark<NEL><BOM><NEL><BOM>a` twin.
  - **the root-alias direct adjacency rides the r218 skeleton (r227, #1206, nightly)** — the 9-byte `&a:o:\n*a:` (run 42630534433, main 0b72c24, crash-03fcab71): the DIRECT-ADJACENCY member of the r218 family — the mutator deleted the r218 seed's 19-LF blank run down to the bare break. libyaml's 1.1 IS_ALPHA anchor/alias charset cuts both names at `:` and reads the flat pair map (key `o` anchor `a` + empty value, then the alias key `a` + empty value), while yeptris's suite-pinned riding law (2SXE: `&a: key` anchors `a:` and `*a:` names `a:`; W5VH keeps colons inside names — the spec's ns-anchor-char includes `:`) keeps `a:o:` pending at the root and names the alias `a:`, so no value indicator opens a map and the document collapses to ONE riding-name alias event. Not a regression: the r225/r226 fixes (#1198, #1204) are comparator-only and the artifact's stream pair is the r218 family verbatim. The carry widens the r218 arm in place (the blank run was always inert on the collapse): the separator law goes ZERO-OR-MORE blank lines (claiming the direct form, its lone-CR spell, and the zero-blank indented alias), leading blank lines claim as an inert prefix, the exact-byte name pins widen to a conservative riding-name set (`[A-Za-z0-9_.-]` + the riding `:`, each run required to END on its `:` — the plain-name endings are separate shapes), and a line-start `...` doc-end tail claims as an inert suffix — all dumpboth-verified same pairs. Seed `root-alias-direct-adjacency-r227.bin` (pin fuzz-diff-r227-root-alias-direct-adjacency); 97-mutant battery 96 clean with 1 designed decline (the no-tab gate's tab member), six corpus walks clean (parse 347, feed 65, marshal 18, cbor 102, validate 5, roundtrip 5), full ctest 907/907, `leaks --atExit` clean; still unclaimed per shape: tab members, props-restart names, non-ASCII names, colon-glued `...`.
  - **the UTF-16BE mid-stream BOM composite rides the FEFF-strip law (r243, #1255, nightly)** — the 5206-byte artifact (run 42942836432, main f04fd40-era, crash-1a7cef2a; already single-byte-deletion minimal yet mutation-fragile AS A WHOLE — every probe hits a different rung of the relation): a UTF-16BE stream opening with THREE `feff 0085` (BOM + NEL) code-unit pairs, where (1) yeptris's r144 double-BOM-vanish fix consumes only the STREAM-INITIAL BOM — every mid-stream `feff` code unit rides as U+FEFF (ZWNBSP) CONTENT and the normalized NEL following each folds to ONE space — while libyaml's UTF-16 reader eats the mid-stream `feff` code units as BOMs too, so both its FEFF characters and their fold spaces vanish, and (2) the relation completes with a TAIL allowance: libyaml's non-validating UTF-16 writer carries a bounded residue at the scalar-content end (up to two complete characters plus one TRUNCATED UTF-8 sequence — the partial `E0 A8` lead) that yeptris's validating front-end declines; libyaml emitting an incomplete sequence is a libyaml-side defect, yeptris's output is well-formed by construction. THE A/B IS FREE: the nightly ran on pre-arm main and its failure IS the pre-arm abort. The carry is `yd_r243_utf16_midstream_bom_gray` (the `yd_ps_space_divergence` dump-shape pattern): gate (the UTF-16BE BOM leads + a second `fe ff` code unit mid-stream), the exact strip transform (every U+FEFF triple removed together with the single fold space that follows each), and the bounded truncated-char tail law; any byte the relation cannot verify declines (the escalation clause stays live). Comparator carry only — no engine change. Seed `utf16-midstream-bom-r243.bin` (pin fuzz-diff-r243-utf16-midstream-bom, test #924); 42-mutant paired battery vs the pre-arm binary 39 arm-caught / 0 regressions / 3 both-pass / 0 both-abort, six corpus walks exit-0 on rebuilt _lf harnesses (parse 365 incl. the seed, feed 64, roundtrip 5, marshal 17, cbor 101, validate 5, -runs=0), full ctest 924/924, `leaks --atExit` clean, clang-format 18 clean.
  - **the flow explicit-key family's glued-? MIDDLE member rides the member grammar (r242, #1252, nightly)** — the 13-byte `[?,,g,?g?,\"\"]` (run 42930337137, main dc6d2e2, crash-be5b7d7bc6a1d1db754c32fe8e146caa473e85a3; seed-corpus replay at MS: 0): the r240 carry pinned the glued-'?' tail behind a PS-folded head — this member is the grammar itself, with NO exotic bytes. FOUR member kinds appear and exactly one diverges: the LONE '?' member opens the explicit-key map (empty key, empty value) on BOTH sides (they agree — the suite-pinned plain-'?' law protects only the GLUED '?'); the empty member (`,,`) renders nothing on both; plain and quoted members render `=VAL : <content>` on both (the harness dump normalizes scalar styles); the GLUED-'?' member (`?g?`) is the delta — yeptris reads it as ONE plain scalar while libyaml's 1.1 grammar opens `? g? : <empty>`, the key content INCLUDING the trailing '?' (a non-leading '?' is content on both engines — the artifact's own shape disproved the first exclusion cut). THE A/B IS FREE: the nightly ran on pre-arm main and its failure IS the pre-arm abort. The carry is `yd_r242_flow_glued_q_members_gray`: gate (breakless, no nested flow, members split on ','), classify each member (lone-? / empty / quoted-simple / plain / glued-?), BUILD BOTH expected dumps from the input bytes, and require exact equality on both sides; any byte the grammar cannot classify declines (the escalation clause stays live). Comparator carry only — no engine change. Seed `flow-glued-q-members-r242.bin` (pin fuzz-diff-r242-flow-glued-q-members, test #923); 63-mutant paired battery vs the pre-arm binary 11 arm-caught / 0 regressions / 50 both-pass / 1 both-abort (aborting identically pre-arm — the r195/r238 precedent), six corpus walks exit-0 on rebuilt _lf harnesses (parse 364 incl. the seed, feed 64, roundtrip 5, marshal 17, cbor 101, validate 5, -runs=0), full ctest 923/923, `leaks --atExit` clean, clang-format 18 clean.
  - **the root-alias collapse's TAB + folded-header member rides the family skeleton (r241, #1249, nightly)** — the 9-byte `&a:<TAB>>\n*a:` (run 42919365127, main f6111d8, crash-aa326ab184531278a80ecf9d719d6ef1cfc26c17; the artifact IS the libFuzzer minimum, seed-corpus replay at MS: 0): the r218/r227/r238 collapse family's members so far carried blank-line runs, direct adjacency, breakless and quoted followers — never a TAB separator, and never a BLOCK-SCALAR HEADER as the head's value. THREE agreements and the family's own delta: BOTH engines render the head scalar as `&` + name + ONE space (the TAB blank folds like any blank run), BOTH take the `>` as an EMPTY folded-scalar value (the `*a:` line sits at column 0), and at the second key yeptris's suite-pinned riding laws (2SXE/W5VH) let the colon RIDE the alias name (`=ALI *a:` as an unvalued key) while libyaml's 1.1 IS_ALPHA cut splits the alias at `a` and emits its empty value (the r142/r76 law). The r218 arm declines this shape twice (its no-tab gate, its blank-run separator law), so the claim is its own byte-shape gate (TAB separator, bare `>` header — the +/-/digit suffixes escalate, trailing breaks escalate) + an EXACT both-dump walk. REGRESSION TRIAGE CLEAN: a fresh worktree build at the pre-r238-arm parent ad84a13a aborts identically — a fresh find, no arm regression, no engine bug, the escalation clause does not fire. The declined compositions (`|` instead of `>`, space runs instead of the TAB, header suffixes, trailing breaks) are the family's next members if the nightly resolves them. Comparator carry only — no engine change. Seed `root-alias-tabfold-r241.bin` (pin fuzz-diff-r241-alias-tab-fold, test #922); 43-mutant paired battery vs the pre-arm binary 3 arm-caught / 0 regressions / 34 both-pass / 6 both-abort (all six aborting identically on the pre-arm binary — the declined `|`-header and space-separator compositions, the r195/r238 precedent), six corpus walks exit-0 on rebuilt _lf harnesses (parse 363 incl. the seed, feed 64, roundtrip 5, marshal 17, cbor 101, validate 5, -runs=0), full ctest 922/922, `leaks --atExit` clean, clang-format 18 clean.
  - **the plain-? tail rides the raw-PS plain fold (r240, #1246, nightly)** — the 12-byte `[)<PS>(?t,?$]` (run 42801813638, main bf8342c era, crash-63cbaa8cea06537b9ba5ded6a935c75e9c34174c; the 27-byte artifact `[11<CR>)#<PS>7.(,<CR>g?at##|A,?0$,]`, seed-corpus replay at MS: 0): a flow sequence of two plain members where TWO suite-over-libyaml laws compose and neither alone equalizes the pair — (1) the plain-LS/PS fold (the r56 law): yeptris (1.2) normalizes the head's interior PS and folds it to ONE space in the flow plain while libyaml's READ_LINE joins the break RAW into the content, and (2) the suite-pinned plain-'?' law (2SXE): yeptris reads the glued-'?' tail (`?$` — '?' followed by a non-space is content) as ONE plain scalar while libyaml's 1.1 grammar takes the bare '?' as the explicit-key indicator and opens `? $ : <empty>` for the tail. libyaml's dump CLOSES everything — this family CLIPS nothing; the nightly's stack frame pointing at `yd_clip_breakless_divergence` was the last consulted arm, not the family (its LF-suffix replay cannot equalize the pair — both deltas survive the break). REGRESSION TRIAGE CLEAN: a fresh worktree build at the pre-arm parent b41870e aborts identically — a fresh find, no arm regression, no engine bug, the escalation clause does not fire. The carry is the `yd_r240_flow_qtail_clip_gray` arm beside the flow-explicit-key claims: a byte-shape gate ('[' first / ']' last / breakless outside the one interior PS triple / exactly one ',' / the head not starting with '?' / the tail a glued-'?' printable run free of the indicator set) plus a BOTH-dump walk against the input bytes (yeptris: +SEQ, the head with the PS folded to one space, the tail plain, the closes; libyaml: the head with the PS RAW, +MAP, the tail-after-'?' as key, an empty value, the closes intact); anything the gates cannot verify declines, so the escalation clause stays live. The artifact's richer outer shape (a CR inside the head plain, `##` comment runs, `|` interior) is a superset the arm deliberately declines — it escalates on a future round if it resolves to a new composition. Comparator carry only — no engine change. Seed `flow-qtail-psfold-r240.bin` (pin fuzz-diff-r240-flow-qtail-psfold, test #921); 59-mutant paired battery vs the pre-arm binary 9 arm-caught / 0 regressions / 39 both-pass / 4 both-abort (all four aborting identically on the pre-arm binary — pre-existing unclaimed shapes, the r195 precedent), six corpus walks exit-0 on rebuilt _lf harnesses (parse 362 incl. the seed, feed 64, roundtrip 5, marshal 17, cbor 101, validate 5, -runs=0), full ctest 921/921, `leaks --atExit` clean, clang-format 18 clean.
  - **the anchor-quoted-follower collapse rides the r238 gray arm body walks (r238, #1239, nightly)** — the 10-byte `&2: '<LS>\r'` (run 42770301286, main ad84a13a, crash-3d571437a4027d3b46c234964191c77cd4febdf4; 18-byte artifact `&k2: 'j:(<LS> \r|\n '`, seed-corpus replay at MS: 0): the r218/r227 anchor-riding collapse family's QUOTED-FOLLOWER member — the anchor-riding colon composed with a single-quoted follower on the SAME line. yeptris's suite-pinned riding laws (2SXE/W5VH, ns-anchor-char includes `:`) keep the name's colon and collapse the line to ONE scalar whose content embeds the anchor spelling plus the quoted body through the PLAIN fold; libyaml's 1.1 IS_ALPHA charset cuts the name at the colon (the r142/r76 law), the freed colon becomes the VALUE indicator, and the quoted scalar parses as the map value under its own fold. MEASURED COMPOSITION-ONLY: the standalone quoted probes (`'a<LF>'`, `'a<CR>'`, `'a<LS>'`, `'a<LS><CR>'`, `'<LS><CR>'`, `'a<CR><LF>'`) ALL AGREE — the fold delta exists only under the collapse. REGRESSION TRIAGE CLEAN: the pre-r238 parent fdda224 aborts identically — a fresh find, the r237 carry exonerated. The carry is a gray arm beside the r218 claim walking BOTH dumps against the input bytes: printables and literal spaces byte-equal, raw NEL/LS/PS verbatim (the r231 law), a space run immediately before a break folding with it (yeptris to ONE space; libyaml to an escaped `\n` when a raw triple precedes — its blank-state quark, the r231 READ_LINE class — else one space), a standalone break folding the same way, and a trailing space run at the body end merging with a just-folded break on BOTH engines (the one fold space was already emitted); the escaped and no-fold spellings stay unmeasured and decline. Comparator carry only — no engine change. Seed `root-anchor-quoted-follow-r238.bin` (pin fuzz-diff-r238-root-anchor-quoted-follow, test #918); 230-mutant paired battery vs parent fdda224 36 fixed / 0 regressions / 181 both-pass / 13 both-abort, six corpus walks green, full ctest 917/917, `leaks --atExit` clean, clang-format 18 clean.
  - **the block-NEL post-run fold boundary rides the surplus eat and the fold-space mirror (r237, #1236, nightly)** — the 18-byte `> #<PS> :\n  <NEL> 0\n t` (run 42768368201 lineage, main fdda224, crash-7be6d7634714addf17298a04006b77a3a8644572; 28-byte artifact `> #<1<PS> 700:\n  m<NEL> t :0\n mt`): the literal block scalar's fold boundary behind a kept raw NEL. Both engines keep the NEL itself verbatim in the value, but behind it the dumps diverge twice: (1) the NEL-site indent surplus — libyaml's 1.1 reader breaks AT the NEL, collapses it into its escaped-break run, and eats the continuation line's leading space as that line's indent (`\n0`), while yeptris's 1.2 break set (LF/CR only) keeps the NEL raw and the space after it as content (`[NEL] SP 0`); (2) the fold-space mirror — behind the run yeptris's 1.2 fold emits the next line boundary as an escaped break (`esc t`) where libyaml kept the fold space and escaped its break (`SP t`). The r208 surplus-break law declined (its alignment byte `pa[0]==pb[0]` fails on yeptris's kept space vs libyaml's `0`) and the r161 terminal-tail law declined (the byte behind the space is `0`, not the escape). The carry is two arms in `yd_block_nel_walk_mult`, both at sites that previously declined unconditionally (r197 wants LS/PS on b, the r215 arm wants the space on a), so every existing member keeps its exact path: (1) the r208 surplus scan now EATS yeptris's NEL-site indent surplus before its escape-run scan — gated on b leading with an escape (the dumps disagree behind), capped at 64, the run law's surplus==k + alignment-byte bindings still arbitrating; (2) a fold-space mirror arm in the walk's main loop rides ONE escape<->space pair, gated on the alignment byte behind the pair coinciding. Not an engine bug — the 1.2 break set is normative (NEL is not a break character), the documented 1.1 delta the r160/r208/r214/r215/r176 family already carries. Seed `block-nel-nel-foldspace-r237.bin` (pin fuzz-diff-r237-block-nel-foldspace-mirror, test #917); 129-mutant paired battery vs parent 931d7d7 12 fixed / 0 regressions / 117 both-pass, six corpus walks clean (parse 359, re-walk 360 with the seed), full ctest 916/916, `leaks --atExit` clean, clang-format 18 clean.
  - **the r237 surplus eat no longer strands the split-pair rides (r261, nightly)** — the 23-byte `> #<PS> 700:\n  m<NEL> <NEL> t` (PS = raw U+2029; run 43388810632, main a86f67fe, crash-ae385ab41d3dfeef0aa6a3a1ddcb8cdce94789a0): the block-NEL family's SPLIT-PAIR composition — the folded scalar's kept raw NEL pairs arrive separated by single kept spaces (`<pair> SP <pair> SP t`, one more-indented content line in yeptris's 1.2 model; spec 5.4's b-char is LF|CR only so NEL is content, and the trailing break rides the suite's L24T#2 clip law at breakless EOF) where libyaml's 1.1 reader breaks AT every NEL and renders the region's whole break surplus as one escaped run. A ride that stood since r160 — the bisect pins the break to the r237 commit itself (the pre-r237 parent fdda224 rides the artifact byte-identically): the r237 NEL-site surplus eat consumed the leading SP that is exactly the r173 continuation's SP<->break pairing material, stranding the walk at the second pair with no pairing step left (the eat and the pairing are mutually exclusive at one site; the regression went unseen because no corpus seed carried the shape). The repair is the family's two-phase discipline (the r177 precedent — a greedy eat must never fire in the pass where an existing member's byte-walk needs the space): `yd_block_nel_walk_mult` gains an `eat` gate and `yd_block_nel_walk` sequences the eras (the pre-r237 mult 1/2 no-eat pair first, then the r237 mult 1/2 eat pair), so every pre-r237 member keeps its exact path while the r237 flavor still rides its own law (its seed re-verified). Not an engine bug — every yeptris delta stays suite/spec-correct (the 1.2 content NEL, the L24T#2 clip break); the find is a test-arm regression repair plus the seed pinning the shape from now on. Seed `nel-fold-r261.bin` (pin fuzz-diff-r261-nel-fold); 50-mutant paired battery vs the pre-repair binary 15 arm-caught (the restored rides) / 0 regressions / 14 both-pass / 21 both-abort (all byte-identical); probe census k=1 split, k=2 adjacent, PS/LS twins, trailing-LF and the k=3 split all ride; six corpus walks green (parse 416 incl. the seed, feed 66, roundtrip 8, marshal 19, cbor 103, validate 7); full ctest 792/792 plain + 977/977 _lf (the pin is test #977); `leaks --atExit` clean; clang-format 18 clean.
  - **the exotic-UTF-8 content member rides the r203 gate widenings (r236, #1233, nightly)** — the 38-byte `[?D<LS><PS><PS><U+2A29><PS><PS> U\<CR> <PS><PS><LF><LS><PS><PS>"]` (run 42752605314, main 5632789, crash-6a632d98; 32-byte minimal: the r231 shape with one U+2A29): the exotic bytes are INERT content both engines keep raw — the surrounding break runs ride the adjudicated r231 laws (libyaml keeps LS/PS raw verbatim, yeptris normalizes and collapses) — but three conservative gates in the r203 arm declined the member: the run scan's content gate (`d >= 0x80`), the DQ bake's escape law (`b > 0x7E`), and the scan's outright `\` decline. REGRESSION TRIAGE CLEAN: the pre-r236 parent 5e2141e aborts identically (byte-identical dumps) — the r235 engine round exonerated. The carry widens three gates, all work bounds the byte-strict verify arbitrates: (1) the run scan admits a COMPLETE VALID UTF-8 sequence that is not a break unit (fragments and overruns decline); (2) the DQ bake rides bytes ≥ 0x80 raw (valid UTF-8 carries raw in both engines' double-quoted scalars); (3) the `\` decline lifts — a plain backslash is content in both engines and the bake escapes it as `\\`. Comparator carry only — no engine change. Seed `qflow-lsps-utf8-content-r236.bin` (pin fuzz-diff-r236-qflow-lsps-utf8-content); 840-mutant paired battery vs parent 5e2141e 79 fixed / 0 regressions / 761 both-pass, six corpus walks clean (parse 358), full ctest 916/916, `leaks --atExit` clean, clang-format 18 clean.
  - **the PS-terminated doc marker rides the r142 gate's break-set law (r234, #1227, nightly)** — the 18-byte `---<PS> ? &--02:in~` (PS = raw U+2029; run 42706321967, main f856286, crash-4262eff1): the divergence itself is the r142 anchor-charset law in the BLOCK explicit-key position (yeptris's ns-char set rides the colon — anchor `--02:in~` with an EMPTY value; libyaml's IS_ALPHA set cuts at `:` — anchor `--02` + plain `:in~`), already carried for the bare form — but the PS-terminated `---` prefix broke the arm's GATE: the `-x` plain clause armed a phantom plain on the FIRST dash (`-` followed by the non-blank `-` matches the `-x` guard), and the doc-marker clause that resets it (`coll_col=-1`, `plain_open=0`) required the terminator ∈ {space, tab, nl, cr} — the raw PS is none, so the reset never fired, `node_pos` rode 0 forever, and the gate declined its own family (with an LF terminator the doc-marker clause fires on every dash — that's why the bare and LF forms carried). REGRESSION TRIAGE CLEAN: the pre-r234 parent f759d1f aborts identically (byte-identical dumps). The carry is the r234 law in BOTH the gate and copy walks: the doc-marker clause's terminator set admits the raw multibyte breaks (`yd_mb_break_len(data, size, i+3) > 0` — the r87 break model). Comparator carry only — no engine change. Seed `anchor-charset-ps-docstart-r234.bin` (pin fuzz-diff-r234-anchor-charset-ps-docstart); 198-mutant paired battery vs parent f759d1f 55 fixed / 0 regressions / 5 both-abort / 138 both-pass, the bare controls stay green, six corpus walks clean (parse 357), full ctest 914/914, `leaks --atExit` clean, clang-format 18 clean.
  - **the downstream-LS/PS plain member rides the r203 two-phase verify (r233, #1224, nightly)** — the 132-byte artifact `[LF ?+D<PS> LF ?+D<LS> 'c,ag:yamd.org,2002:m*a+D<LS> 'c<PS> LF . CR | CR LF ...]` (run 42697464143, main e2b7464, crash-72a25efe): the r203 finder FIRED and the bake SUCCEEDED (the '?'-led key baked byte-exact `+D\P\n\L'c`) — but the EXACT verify declined on a member the rewrite never touches: member 3 carries its own raw LS/PS bytes, libyaml keeping them RAW verbatim in the plain value (the r231 verbatim law) where yeptris normalizes and folds (`'c\n.` vs `'c<PS>\n.`) — the composition of two adjudicated carries (the r203 rewrite plus a downstream plain member riding the c2-control LS/PS law) died on the exact compare. REGRESSION TRIAGE CLEAN: the pre-r233 parent 9e78756 aborts identically (byte-identical dumps) — the r232 skeleton round exonerated. The carry is the r233 TWO-PHASE VERIFY in the r203 arm: phase 1 the exact compare (every previously-exact member keeps its exact path); phase 2 on decline — the rewrite must stay exact for libyaml (`ldump2 == ldump` byte-exact) and the yeptris leg re-verifies through the adjudicated LS/PS fold walk (`yd_dump_eq_mod_lsps_fold`) — two trusted carries composed, the r166 lesson applied to the verify. Comparator carry only — no engine change. Seed `qflow-lsps-downstream-plain-r233.bin` (pin fuzz-diff-r233-qflow-lsps-downstream-plain); 1320-mutant paired battery vs parent 9e78756 1028 fixed / 0 regressions / 16 both-abort / 276 both-pass (the walk admits the whole downstream-LS/PS neighborhood), six corpus walks clean (parse 356), full ctest 913/913, `leaks --atExit` clean, clang-format 18 clean.
  - **the doubled quote-comment-glue member rides the widened skeleton (r232, #1221, nightly)** — the 18-byte `[1,?#,"\r g",?g?,""]` (byte 8 a RAW CR) and the TRUE 24-byte `[1,?#,"\rg",?g:m?#,"\rg",]` (run 42688765844, main e8ba885, crash-1020a8db): the r229 law composed one member further — the glue member unchanged (libyaml's comment eats `#,"` to the RAW CR, the `g"` tail pairs with the orphaned KEY), the flow-`?` member's plain tail rides the GLUED `?#` (mid-plain content in BOTH engines — libyaml's KEY fired only at the leading `?`, its second salvage map's key is the byte-exact `g:m?#`), and the trailing quoted member `"\rg"` parses WHOLE as a seq member on both sides (libyaml's map closed EMPTY at the comma — the r229 single form closed at `]` — so the tail members AGREE, folded to ` g` in both engines). REGRESSION TRIAGE CLEAN: the pre-r231 parent edcece1 aborts identically (byte-identical dumps) — the r231 cap round exonerated. The r229 skeleton declined on its exact `?g:m]` tail. The carry widens the skeleton IN PLACE: `?g` exact, then the riding plain tail (the measured set `[a-z0-9:?#]`, non-empty, `:`/`#` never last — a trailing `:` faces the `,`/`]` as a pair separator, a trailing `#` an unmeasured end-form), then comma-separated inert members (`""` and the CR-folded quoted form, the head's raw-break spell set) with a trailing comma allowed — the exact `?g:m]` path is the empty-suffix member unchanged (the r229 seeds stay green); the TRUE artifact rides length+FNV-1a-64 (24 bytes, 0xd8de3b598a9d5dd9). Comparator carry only — no engine change. Seeds `qflow-quote-comment-glue-doubled-min-r232.bin` (pin fuzz-diff-r232-qflow-quote-comment-glue-doubled, the skeleton leg's exact revert detector) + `qflow-quote-comment-glue-doubled-r232.bin` (the TRUE artifact, its FNV leg); 504-mutant paired battery vs parent edcece1 37 fixed / 0 regressions / 97 both-abort (the gray cadence) / 370 both-pass, six corpus walks clean (parse 355), full ctest 912/912, `leaks --atExit` clean, clang-format 18 clean.
  - **the flow-'?' x LS/PS all-blank-run member rides the raised bake caps (r231, #1218, nightly)** — the 42-byte `[?D<LS><PS><PS>³<LF><LS><PS><PS>"]` (13 breaks, 12 trailing) and the TRUE 112-byte artifact (run 42677255629, main edcece1, crash-238624b1): the r203 family's laws composed into the member whose break run is ALL BLANK LINES — libyaml wraps the seq entry in `+MAP` (the flow-KEY law, the `?` stripped from the key, empty value) and keeps a break run containing LS/PS **verbatim** in the plain value (raw LS/PS bytes, the interior LF a real LF byte, NO fold space, NO trailing strip) where yeptris normalizes every break to LF and collapses per the k-1 law with the trailing strip. REGRESSION TRIAGE CLEAN: the pre-r230 main 73c447e aborts identically (byte-identical dump pair). Controls `[?Dx"]` (wrapper law alone) and `[D<LS><PS><PS>D"]` (fold law alone) both pass — each delta alone rides an existing arm, the catch needs both composed on one scalar; a quote glyph after the break run rides as plain content in BOTH engines (the r191 law; the run scan already admits it). The r203 arm declined on TWO bake obstructions, both work bounds the byte-strict verify arbitrates: the `trailing[24]` buffer (the minimal's run stores 34 bytes of trailing breaks after the raw LS/PS lead — `wok=0` declined the hit) and the 16-break cap (the artifact rides 23 folds; the r225 round raised the cap 8→16 but left the byte buffer at 24). The carry: buffer 24→192 (64×3 bytes — every admitted break stores at most 3) and cap 16→64; the verify still certifies every claim. Comparator carry only — no engine change. Seeds `qflow-lsps-allblank-min-r231.bin` + `qflow-lsps-allblank-r231.bin` (pin fuzz-diff-r231-qflow-lsps-allblank-caps replays the TRUE artifact — it needs BOTH widenings, so revert detection is exact on both legs); 1540-mutant paired battery vs parent edcece1 579 fixed / 0 regressions / 41 both-abort / 920 both-pass, six corpus walks clean (parse 353), full ctest 911/911, `leaks --atExit` clean, clang-format 18 clean.
  - **the double-split anchor charset member rides the residual-colon law (r230, #1215, nightly)** — the 59-byte `&I:919999k: &ab?cd v\n--- !!m~~top4~~~...` (run 42665259808, main 73c447e, crash-b841bb3a): TWO riding anchor names on ONE line, separated by the key colon — `&I:919999k:` at the key (yeptris's ns-char set rides the interior colon, name `I:919999k` + empty key; libyaml's IS_ALPHA set cuts at `:`, anchor `I` + plain key `:919999k`) and `&ab?cd v` at the value (yeptris rides the `?`, name `ab?cd` + plain `v`; libyaml cuts at `?`, anchor `ab` + plain `?cd v`); the trailing `--- !!m~~top4~~~...` document parses identically in both engines. REGRESSION TRIAGE CLEAN: the pre-r229 main 97bd9df aborts identically (the same dump pair) — the r229 quote-glue round (comparator-only) exonerated. The r142 arm's split transform declined its own family: the residual blind copy ran NO colon law, so `node_pos2` stayed dead after it and the SECOND `&ab` split was skipped — the copy kept the riding name and the copy-agrees leg died (both verify legs — yeptris(split)==libyaml(split) AND libyaml invariant between original and split — hold by hand on the fully-split copy, dumpboth-verified). The carry is the r230 residual-colon law in `yd_anchor_name_charset_divergence`: the gate's colon law runs through the residual blind copy itself (a colon with a blank follower ENDS the riding plain and arms `node_pos2` — the key colon; a colon with a non-blank follower RE-ARMS the colon-led plain law), and the trailing `plain_residual` arms only when the residual had NO colon of its own (the first attempt clobbered `plain_open2` after the key colon reopened the plain over the node position — the gate never clobbers). Not an engine bug — yeptris is suite-correct on the ns-anchor-char riding law (2SXE/W5VH). Seed `anchor-charset-residual-colon-r230.bin` (pin fuzz-diff-r230-anchor-charset-residual-colon); 359-mutant census 0 aborts (the arm is general — the whole name-variant neighborhood carries), six corpus walks clean (parse 352), full ctest 910/910, `leaks --atExit` clean, clang-format 18 clean.
  - **the quote-comment-glue soup member (r229, #1212, nightly)** — the 16-byte `[1,?#,\"<CR>g\",?g:m]` (byte 8 a RAW CR; run 42655755989, main 97bd9df, crash-603d262) and the TRUE 10,732-byte artifact over the family's repeated members: libyaml's comment-glue (the r178 law — it eats from the glued `#` to the raw break WITHOUT tracking quotes) swallows the `#,` AND THE OPENING QUOTE and resumes mid-scalar at the baked break — the `g\"` tail is plain content, the `,` separates the orphaned `?` KEY token's map (key `g\"`, empty), and the trailing `?g:m]` opens the SECOND map (key `g:m`, empty); yeptris composes the other way (the plain-'?' law keeps `?#` as content, the quoted scalar parses WHOLE with the raw break folded inside the flow quotes, `?g:m` rides as one plain) — 4 scalars vs 2 maps, an event-KIND divergence the r137/r138 verdicts rule unsound to generalize, and no spell transform reconciles the pair. REGRESSION TRIAGE CLEAN: the artifact aborts IDENTICALLY (exit 134, the same dump pair) on the pre-r228 parent 77b1709 — the r228 props-led ENGINE round exonerated. The carried arms decline (r224's HIT A comment scan is not quote-aware; the r137–r141/r163 walks assume comment ranges without baked breaks; the r203 escape-baked arm is a different spell). The carry rides the r129/r139 EXACT-BYTES gray discipline: the skeleton walk `yd_r229_flow_quote_comment_glue_gray` (ws* [ ws* 1 ws* `,?#,\"` RAWBREAK `g\"` ws* `,` ws* `?g:m]` ws*) claims the minimum plus the six MEASURED inert-whitespace positions (the r228 law), and the artifact rides length+FNV-1a-64 against its committed corpus seed. 118-mutant census: 72 claimed, 46 interior variants stay per the cadence; six walks clean (parse 351), full ctest 909/909, `leaks --atExit` clean.
  - **the interleaved NEL x LS plain-fold walk (r166, #1025/#1027)** — both break families in ONE dump: a 490-byte root plain with nine LS raw-join sites and two NEL k=2 sites — the LS walk dies at the first NEL site, the NEL walk dies at the first LS site (the r158 composition lesson, applied to WALKS instead of transforms). The gated `yd_nel_lsps_walk` — fired only when both byte families are present, ordered behind the single-family arms — pins each family to its adjudicated law (the LS triple rides the fold-walk run law with r90 backtracking; NEL runs ride the r118 k-law with the r128 lone-byte guard and r122 trailing vanish), equality sits before the triple law (the seed's raw U+20A9 won spell), pointers never rewind, additive-only.
  - **the mid-stream `?!` empty-key wrapper ride (r167, #1028/#1030)** — the r136 tail-wrapper shape MID-STREAM: each `?!` member becomes libyaml's four-line empty-key salvage block (`+MAP`/`=VAL : `/`=VAL : `/`-MAP` — the glued `!` rode the key away as the invisible non-specific tag), and after the first wrapper grazes members through the emptied-key arm, the SECOND wrapper's `+MAP` faces yeptris's `-SEQ` closer with no arm. The structural arm's wrapper-claim gates extend to mid-stream: the whole block rides when byte-pinned (empty key, empty value, bare closer) AND yeptris's killed member is glue-LED (`?`+`#`/`!` at the content lead, the r163 pin in lead form); skips += 3/diffs += 5 per claim, shared 64-cap, full-exhaustion law.
  - **the distance-2 rekey ride (r168, #1031/#1033)** — libyaml's comment-glue ate `##|,:|,:binary` through TWO members to the CR and re-paired the pending KEY with the CR-folded continuation, so the wrapper's KEY is the byte-exact suffix of yeptris's member at DISTANCE 2 (r149 covered distance 1; r143 distance 0). The sibling ride inside the rides-first region: killed member glue-led (r167 lead pin), one content VAL between, the distance-2 member's content ending with the r148-stripped wrapper KEY byte-exact, well-formed four-line block, both pointers advance, shared 64-skip cap, claim only on full exhaustion. (The CR folds themselves AGREE byte-exact in both engines — the CR-fold-arm hypothesis was ruled out by controls.)
  - **the distance-k rekey ride (r172, #1043/#1045)** — the re-pairing family generalizes from the distance ladder (0/1/2) to DISTANCE k: libyaml's comment-glue ate the glued `?##` run through FIVE members to the CR and re-paired the pending KEY with `bnaary`, the byte-exact suffix of yeptris's distance-5 member `:bYMA y bnaary`. The ride consults only after the 0/1/2 forms decline (MECE — the ladder stays theirs): killed member glue-led (the r167 lead pin), every member between a kept-content plain VAL, the distance-k member ending byte-exact with the r148-stripped wrapper KEY, the well-formed four-line block; cap charges 4+dist, monotone in k. Soundness: the byte-exact KEY suffix at distance k is the observable that the comment ran past k−1 members — the eaten bytes are comment-invisible (the r124/r127 precedent the ladder already trusts at 0/1/2). Comma-separated k=56/61 negatives still abort past the cap by design.
  - **the plain-'?' alias tail + the killed-member frame ride (r178, #1062)** — the flow soup whose `?` is glued to `#`: libyaml's flow-KEY token fires on the `?` unconditionally and its scanner treats the glued `#` as comment start between tokens (no separation required), eating the run whole — yeptris keeps it as content per the suite-pinned plain-'?' law (`?` + an ns-plain-safe char is a legal plain FIRST char; the reserved backtick rides as content). The r163 alias-as-content-tail arm declined — its glue-fingerprint gate is local to the pre-spelling prefix (here exactly the one `?` byte; the marker lives in the other member) — so the gate gains the plain-first-'?' form byte-tight: prefix length pinned to 1, byte pinned to `?`, libyaml's `=ALI <name>` spelling still the content's byte-exact suffix, libyaml's alias PRESENT (the r138–r141 alias-drop masking cannot apply), family-gated by pass 1. The 58-byte artifact strands the walk one member off (the comment ate the marker member `?#?#|##|#|` whole, the r124/r127 y-only skip leaves its EMPTY value line to mis-pair, the death is yeptris's empty vs libyaml's `=ALI *L`): the KILLED-MEMBER FRAME skip consumes the glue-marked content VAL + its exact `=VAL : ` partner — one skip count per frame (the r146 soup rides 96 kept-content members past the shared 64) — keeping libyaml's line for the downstream r117 SPLIT pairing. The family entry goes two-phase (the r177 pattern): the original walk first — every member that rode before r178 keeps its exact path (the frame route strands a backtick-comma flip the single-line route finishes) — then on a decline the frame-enabled walk. The 9-byte minimal `{?#\n,?*L}` pins the alias-tail form — every single-byte deletion changes the verdict.
  - **the alias charset-cut ride (r179, #1065)** — the r142/r76 IS_ALPHA charset law composed into the glued-hash family: libyaml's `scan_anchor` cuts the anchor name at the first byte outside its charset — here the `:` gluing the member's own value indicator — freeing that byte for its VALUE token and the tail for a plain value, while yeptris follows the spec's `ns-anchor-char` (c-flow indicators and white excluded only), so one run `*L:( }` renders yeptris `=ALI *L:(` with the value eaten EMPTY vs libyaml `=ALI *L` + `=VAL : (`. The byte-exact `yd_qgh_alias_cut_pair` arm: libyaml's name a proper PREFIX of yeptris's past the `*` marker, the cut byte exactly `:`, every kept name byte inside libyaml's charset, libyaml's next line the exact eaten suffix, yeptris's next line the exact empty val — both pointers advance two lines (bounded by construction). The r116 l-only-empty gate widens to accept a yeptris `=ALI ` line: the family's salvaged null sits directly before the re-positioned alias, which must still pair byte-exact downstream or the claim dies. The 19-byte minimal (every single-byte deletion changes the verdict) leaves the colon-less `(`-cut sibling aborting BY DESIGN — its cut byte carries no VALUE token, so libyaml re-lexes the tail as an extra key and the member counts diverge. Not an engine bug — yeptris is suite-correct on both laws.
  - **the comment-run neighbors ride the killed-member frames (r180, #1068)** — the nightly's 1728-byte artifact is a fresh qflow glued-hash shape, not an r179 regression (the A/B against the pre-r179 tree aborts with the byte-identical differ): libyaml's flow-`?` KEY token fires on every flow-context `?` and the glued `#` comments to the NEXT BREAK, so `"?#,0,<CR>"` kills the marker member AND its same-line neighbors whole where yeptris's suite-pinned plain-'?' law keeps every member. The r178 frame skip covered only the marker itself (its glue-fingerprint pin is local), so every comment-swallowed neighbor cost TWO shared-cap skips (the y-only content skip plus the L-only partner skip) and the bare-'?' member's `{null: null}` frame ate libyaml's SURVIVORS through the earlier L-only arm — the artifact's ~47-member killed runs outran the shared 64 in BOTH walk passes (the 89-byte and 193-byte minimals reproduce the two failure modes at arm scale). The carry in `yd_qflow_glued_hash_differ` is four-part: the MARKER FRAME RUN GRANT (`frame_run`) — every marker frame grants up to 16 neighbor frames (content VAL with the exact `=VAL : ` partner, libyaml's line KEPT for the downstream survivor pairing) at ONE shared skip each, the run LINE-LOCAL by construction (the comment cannot cross a break); the DOUBLE-EMPTY frame — the bare-'?' explicit-key member libyaml's comment ate whole renders yeptris two empty lines, and the frame consumes both and keeps libyaml's, granted only by the run (the r116 y-only-empty skip alone would leave the pair one member off); MARKER-FRAME PRIORITY over the r117 SPLIT reading — libyaml's surviving later member may suffix-match the killed plain's tail ("?#g" vs the survivor "g"), where the split reading strands yeptris's own survivor and burns the cap one-off (a false priority can only leave the DIFFER visible — the claim still dies on non-exhaustion); and the shared skip cap SCALES for the frame pass (64 + size/8 — every skip consumes dump lines, so the walk stays line-bounded by construction; frame 0 keeps the classic 64, its path predates r178). Not an engine bug — yeptris is suite-correct on the plain-'?' law.
  - **the comment-run straddle rides the shared block (r181, #1071)** — the soup family's comment-run kill STRADDLING a collection boundary: libyaml's post-`?`-token comment-glue law (the flow-`?` KEY token + the glued `#` comments to the NEXT BREAK) ate the glue-led member's tail together with its same-line followers while yeptris's suite-pinned plain-first-'?' law keeps every glued run as content — yeptris carries kept members libyaml has none of, and libyaml's line is the OPENER of a collection BOTH skeletons carry downstream (the wrapper rides read the block as a killed member's salvage and consume it, the r132 shared-opener tell demands yeptris's immediate next line BE the opener, and the bare structural skip then ate libyaml's opener of the SHARED block while yeptris's own copy was still pending — the walk died at yeptris's opener against libyaml's body VAL with no arm in both walk passes; the A/B at the r181 parent aborts byte-identically, a fresh finding). The carry in `yd_qflow_glued_hash_differ` is the SHARED-BLOCK ride — the r152 re-paired-value pattern on the content side, pinned to the WHOLE byte-exact four-line block (opener + KEY + salvaged null + bare closer): yeptris's glue-led member plus its kept residue (=VAL lines only, the r152 per-site cap of 16) step aside until yeptris's own copy of the block, libyaml's opener not consumed and pairing as an equal line on the next step (where yeptris carries no such block the reading stays with the rides below; an invented twin declines). Frame pass only (the r178 routing); the glue-led lead pin is the r167 family's own; bounded by the run cap and the shared skip cap. Not an engine bug — yeptris is suite-correct on the plain-'?' law. The 12-byte minimal `[2,?#,?|,?\r]` pins the shape.
  - **the flow-sequence glued-follower pair rides its own arm (r183, #1078)** — the qflow glued-follower family's SEQUENCE orientation (`[-1.0<U+2029> 7.tag<CR> ,?.]`, 20 bytes): libyaml's 1.1 unconditional flow-KEY law reads the glued `?` as a KEY, so `.` becomes the key of a single-pair mapping the enclosing flow sequence wraps as a nested member, while yeptris's suite-pinned plain reading (HM87/652Z) keeps `?.` one plain scalar member — the r71 drop-`?` spell is map-only (in a sequence the dropped `?` degrades to a plain member), which is why every earlier arm's verify declined the orientation; the fresh arm `yd_r183_flow_seq_question_divergence` spells the pair explicitly (`{run: }` — both engines read the nested pair libyaml produced) with an LS/PS-normalized verify fallback (libyaml keeps the mid-plain raw U+2029 while yeptris folds it). r206's glued-`#` member joins the run (the 25-byte `[11n<CR><PS> 7.tag<CR> ,?d#  m,]`, run 42498570153, #1143, nightly; the A/B at the parent b23d553 aborts on BOTH builds — a fresh find, the r205 aligned-triple arm innocent): libyaml's 1.1 flow-KEY law reads `?d#  m` as the explicit key `{d#  m: <empty>}` while the plain-'?' law keeps it ONE plain member — the `#` glued after `d` is mid-plain content in BOTH engines (libyaml's `scan_plain` comment check only fires at a blank boundary), so the arm's run scanner was the ONLY thing declining; the widen admits a GLUED `#` (the preceding byte a non-blank, non-exotic-break tail) so the `{run: }` spell stays reproducible, the artifact's head plain rides the existing LS/PS fallback (the CR+PS double break renders as yeptris's spelled-LF vs libyaml's raw kept PS — its fold law only folds leading `\n` breaks), and a `#` after a blank or byte-glued to an exotic-break tail (NEL C2 85, LS/PS E2 80 A8/A9 continuations) stays the comment boundary and declines (the r185 glue remains its own arm). Greedy deletion pins the 11-byte `[n<PS>g,?d#]` with both deltas alive byte-checked; the seeded pin is the TRUE 25-byte artifact (pin `fuzz-diff-r206-flow-question-glued-hash`); 101-mutant paired battery 0 regressions (48 gained — the family's own mutants; 35 carried), the six corpus walks byte-identical pre/post, `leaks --atExit` clean. Not an engine bug — yeptris is suite-correct.
  - **the comment-swallow's third orientation rides the sequence-frame pair (r185, #1081)** — the qflow glued-follower family's third orientation (`[11.0e ,1.0e ,?<U+0503>,:<CR>&binary<CR>, ...]`): libyaml's 1.1 law makes the flow `?` a KEY token whatever follows, and the scanner's next-token loop treats the glued `#` as a boundary comment swallowing `?…,:` through the CR, exposing the next line's node as the keyless pair's key (plain run → `{i:∅}`, anchor → `{&binary∅:∅}`), while yeptris keeps `?#…` one plain member and binds the bare-`:` pair to the post-break run as its VALUE, the map persisting across the `,`; the fresh arm `yd_r185_glued_hash_comment_cr_divergence` is a SEQUENCE-frame `?#` finder that spells `?` + comment-through-break + follower → `{follower: }` (the anchor form `{ &binary : }` rides the r142/r179 wider anchor-name charset) with a byte-strict double verify and a CR/LS/PS/NEL-normalized triple fallback. Not an engine bug — yeptris is suite-correct.
  - **the soup family's size gate scales with its arm's allocation freedom (r186, #1084)** — the nightly's `len_control` (no `-max_len` on the nightly legs) grew a glued-hash soup to 8984 bytes of the exact composition the frame-1 walk already carries byte-exact (`,?##|` members + CR/LS/PS break runs + anchor riders; the killed-member frames riding the r132 shared-opener tell, the SPLIT pairs, and the r150/r162/r164 LS/PS fold pairs on the multi-line plain key riding libyaml's `'`-KEY + glued-comment law) — past `yd_qflow_glued_hash_differ`'s shared 8192-byte size gate, so the walk never ran and the soup landed as a bare artifact. The carry is the gate alone: that arm is allocation-free end to end (pass-1 marker scan + line-wise walk on the ORIGINAL dumps — the walk's own line budget does the bounding), so its gate moves above the seed corpus's own ~205k `-max_len` pin to 262144, while the family's transform arms keep the shared 8192 where their transforms allocate; with the gate open the frame-1 walk exhausts BOTH skeletons byte-exact (frame0 declines, frame1 claims). The minimizer drifted to a documented 20-byte split-family twin. Not an engine bug — yeptris is suite-correct.
  - **the glued-hash folded-plain pair rides its own arm (r187, #1087)** — the qflow glued-hash family's multi-line-plain orientation (`[11n.0e ,?##|,:|,:binary<CR>b's'<LF>        bravo: 0x788,]`, 52 bytes), two composed laws: libyaml's flow-`?` KEY token + the glued-`#` boundary comment swallow `##|,:|,:binary` through the CR, and the PENDING explicit-KEY context then admits what the simple-key law forbids elsewhere — the post-break multi-line plain follower folds (`b's'` + break + indent + `bravo` → `b's' bravo`, the apostrophes mid-plain content) and binds `: 0x788` as its value — while yeptris's suite-pinned HM87 law (`#` mid-plain after a non-blank is content) keeps `?##|`/`:|` as members and reads `:binary b's' bravo` as one folded plain; the fresh arm `yd_r187_glued_hash_plain_pair_divergence` spells `?#…<break>` → `? ` — deleting the swallowed span and RESTORING the explicit `?` so libyaml keeps its KEY context (the `{k: v}` brace rewrite cannot carry the multi-line key: libyaml's simple-key law rejects it outright) — with a byte-strict double verify against libyaml's original dump and the CR/LS/PS/NEL-normalized triple fallback; the deletion mutants' 11-byte EMPTY-COLON twin `[?#,|,:y<CR>:]` rides the same spell. Not an engine bug — yeptris is suite-correct.
  - **the root-block-content arm's line walk models the NEL/LS/PS header breaks (r189, #1093)** — the r19 family's exotic-terminator spelling (the 7-byte `> <NEL># t`): yeptris folds the header's NEL to LF (the #638 walk) and rides the col-0 `# t` line as block content exactly like its LF twin (`# t\n`) where libyaml's 1.1 reader ends the header at the raw NEL into a floor-1 empty scalar (scanner.c floors the auto-detected indent at 1); the arm's line walk modeled LF/CR terminators only, saw ONE unterminated line and never entered block mode — the walk now spells the NEL/LS/PS terminators (NEL 2 bytes, LS/PS 3, a dangling multibyte lead riding as content per the scan's mb-break law), the LS twin declining identically pre-fix, acceptance self-verifying through the indented copy (it re-classifies only when the divergence actually vanishes), composing with the r140 breakless-EOF clip residual exactly as the LF twin's members do. Not an engine bug — yeptris is suite-correct (the A/B at the r189 parent aborts identically, and the input's raw NEL keeps the fresh r188 phantom-CR engine fix untouched).
  - **the glued-hash multiline follower rides plain continuation (r190, #1095)** — the qflow glued-hash family's multi-line follower across the break: libyaml's flow-`?` KEY token + the glued-`#` boundary comment admit the POST-BREAK plain follower as the keyless pair's key, and that follower rides PLAIN CONTINUATION across the break — folding it and absorbing the next line's quote as mid-plain content (`g<CR>"` reads `g "` in both engines), where r187's token-start tracker opened a PHANTOM quoted scalar that hid the family's LS/PS-terminated empty-tail member and the artifact's break-riding `?g` r183 shape; the fresh arm `yd_r190_glued_hash_multiline_follower_divergence` is r187's finder plus the NEL/LS/PS terminators, plain-continuation state, and a second hit kind for the break-riding `?<plain>` runs (a new-shape gate keeps the exotic-terminator / quote-absorb / break-ride hits out of r183/r185/r187 turf), spelling the explicit `?` back with the tail riding untouched — the brace `{? }` serves only the empty-tail member (libyaml's single-line simple-key law forbids multi-line brace keys, the r187 law) — with the byte-strict double verify against libyaml's original dump and the break-normalized triple fallback. Not an engine bug — yeptris is suite-correct (the A/B at the r189 parent aborts identically). The 33-byte minimal `[1<LS/PS x4>,?#,:\rg\r",?<LS/PS x2>]` pins the shape; the 32772-byte artifact seeds intact.
  - **the qflow quote-riding head rides the r190 fold law (r191, #1099)** — the qflow family's quote-riding head (`[-1.0<PS> '.tag<CR> ,?.]`): the head plain run folds across the PS break and absorbs the next line's quote as mid-plain content (the r190 fold law), where r183's unconditional quote-open opened a PHANTOM quoted scalar hiding the tail `?.` divergence (libyaml's KEY-token nested pair vs yeptris's plain member); the carry is r190's KIND 2 gate counting the `rode_head_quote` break-riding run beside the break-riding plain run, the `? ` spell and the break-normalized triple verify carrying it (libyaml keeps the mid-plain PS raw where yeptris folds). Not an engine bug — yeptris is suite-correct.
  - **the root-alias collapse rides the alpha head (r192, #1102)** — the r174 doubled-head member's ALPHA-HEAD sibling (16 bytes: `&al:o&a:o::` + `*a:`; the head length is this member's whole delta): yeptris's suite-pinned riding laws (2SXE/W5VH) read line 1 as a props-only anchor declaration — the anchor name rides BOTH colon runs and the interior `&` to the break (the trailing `::` passes the riding-colon un-consume gate) and pends, so line 2's `*a:` never opens a map and the document collapses to ONE root alias — while libyaml's IS_ALPHA cuts the anchor at TWO alpha chars (`al`), the residual `:o&a:o::` re-lexes as plain `:o&a:o:` (the interior `&` mid-plain is content) with the final colon riding as the value token, and line 1 becomes the anchored KEY of a root map with line 2's alias a second flat pair. Not an engine bug — the A/B at the r191 parent aborts identically; the exact-bytes gray arm `yd_r192_root_alias_alpha_head_gray` carries it beside the r157/r170/r174/r182 family claims.
  - **the root-alias collapse's breakless member rides the r182 family (r198, #1119, nightly)** — r182's seed with the trailing break deleted (the 10-byte `&a::\n  *a:`, libFuzzer-minimal by prefix — seed-corpus replay, no mutation chain), the member the r182 family list itself names, taking its own EXACT-BYTES gray claim `yd_r198_root_alias_breakless_gray` beside the r157/r170/r174/r182/r192 entries (the r151/r170 deletion-sibling precedent): yeptris's suite-pinned riding laws (2SXE/W5VH) make line 1 a props-only anchor declaration whose name rides both colons (the anchor PENDS) and line 2's colon-riding alias collapse the document to ONE root alias carrying the pended anchor as props (3 dump lines), while libyaml's 1.1 IS_ALPHA cut splits the anchor at `a`, the residual `::` re-lexes as plain `:` with the final colon riding as the value token (the anchored key of a root map), and line 2's cut alias plus its EOL colon opens the nested alias-key map — the 9-event form, r182's dumps byte-for-byte. Not an engine bug — the A/B at the pre-r197 parent b2fdddd aborts byte-identically, a fresh find.
  - **the glued question-quote member swallows the soup interior (r193, #1105)** — the qflow soup's `,"?"#|` member: yeptris's suite-pinned plain-first-'?' law (HM87/652Z) keeps `?"#|` ONE plain (the quote glued to the indicator-led run is content, the r145 word-start law), while libyaml's flow-'?' KEY-token law consumes the `?` and frees the glued quote to open a quoted scalar at the token start — the span rides to the NEXT member-final quote (those both engines read as mid-plain content, the fold law's absorption), swallowing the members between as the KEY's folded value (the 755-byte `=VAL : #|,?2002:binary ,...` dump line) and closing with the implicit empty value; the r183/r190 arms decline by construction (their follower gates list quote chars among the indicators left to the other qflow arms) and no walk route carries the swallow — the walk died at the span's trailing residue (the interior members' structure with no libyaml counterpart). The fresh transform arm `yd_r193_glued_question_quote_divergence` beside the qflow cascade: the preserving spell inserts ONE blank (glued → separated), making the quote a token start for BOTH engines — yeptris's separated-'?' law opens the explicit-key map with the quoted scalar as the KEY (the r151 explicit form) while libyaml's token stream stays unchanged (pinned by the ldump2 == ldump byte compare); the compound soup's remaining residue (the `?#`-comment kills, the LS/PS fold pairs) is then claimed by the family's walk (the r115 quote-out classifier's form extended with the walk). Not an engine bug — yeptris is suite-correct (the A/B at the r192 parent aborts identically). The 14284-byte artifact seeds `glued-question-quote-r193` (pin fuzz-diff-r193-glued-question-quote).
  - **the alias charset-cut's value orientation rides the r125 realign (r196, #1113, nightly)** — the r179 cut law composed with the r132 glued-`?#` plain law in the VALUE orientation (the 16-byte `{?#??\t*L=:\n*L:(}`, byte-minimal): libyaml's flow-`?` KEY token eats the `?` and its comment-glue law swallows `#??\t*L=:` whole to the LF, so line 2's `*L` becomes the KEY's content — `scan_anchor` cuts the name at the `:` (the r142/r76 law), freeing `:` for the VALUE token and `(` for a plain — while yeptris keeps `?#??\t*L=` ONE plain (the suite-pinned plain-'?' law; the `#` mid-plain is content) as the KEY and the alias as its VALUE with the spec's `ns-anchor-char` name `L:(`; the walk died at the r125 realign's byte-exact demand after its skip (`=ALI *L:(` vs `=ALI *L`) because the r179 helper's pin demands yeptris's next line be the eaten-empty val — HERE the alias was the member's own VALUE, so nothing follows but the closer. The carry adds the valued-orientation helper `yd_qgh_alias_cut_valued_pair` (the same cut pins — proper-prefix name, `:` cut byte, libyaml-charset kept bytes, libyaml's next line the exact eaten suffix — plus yeptris's next line a flow closer and libyaml's NEXT line that same closer) consulted from the r125 failure path; the A/B at the pre-r195 89aaf7c build aborts byte-identically, a fresh find, not a regression. Not an engine bug — yeptris is suite-correct on both laws.
  - **the bare-`?` plain-key head rides the alias-colon charset split (r199, #1121, nightly)** — the two suite-pinned riding laws composed against libyaml's 1.1 quirks in ONE 15-byte flow map (`{?*L:\n( `,*L:(}`, libFuzzer-minimal — prefixes 1–14 all both-error): the head `?` with its non-space follower rides as ONE plain key `?*L` under the HM87/652Z law where libyaml's unconditional flow-KEY token takes the `?` and re-lexes `*L` as an ALIAS key (the r142/r76 cut), and the tail anchor `*L:(` rides the `:` into the name per catch-142/2SXE/W5VH where libyaml's IS_ALPHA cut stops at `L` and re-lexes the `:` as the VALUE token with `(` the plain; the r71 drop-`?` spell declines (its rewrite makes yeptris reject the sub) and the charset-split twin declines both orientations, so the composition carries as the EXACT-BYTES gray claim `yd_r199_qflow_bareq_alias_ride_gray` (the r198 precedent — documented by input, not mutation). Not an engine bug — the A/B at the pre-r198 parent 8ef3eb6 aborts identically, a fresh find; the r198 arm exonerated.
  - **the alias restart rides the folded plain (r200, #1124, nightly)** — libyaml's `scan_to_next_token` eats the glued `#` unconditionally after the flow-`?` KEY token, killing the run to the line break, so the next line's `*` becomes a node start re-lexed as an ALIAS KEY (`=ALI *L` + `=VAL : (`) where yeptris's suite-pinned HM87 law keeps the run ONE plain folded across the break, absorbing the alias spelling; the carry is the comparator arm `yd_qgh_alias_restart_fold_pair` — byte-exact pins, a paired two-line consume, the claim standing only on full exhaustion. Not an engine bug — yeptris is suite-correct on the plain-'?' law.
  - **the flow-`?` multiline plain rides its own spell (r203, #1134, nightly)** — a glued flow-`?` whose plain run rides LS/PS/CR breaks (the 37-byte `[\n?+D<LS> 'c<PS>.\n--&k:j:(<PS> \r|\r\n.oa"]`, run 42484238632, main 9c32c73): libyaml's flow KEY-token law nests the run as the key of a single-pair map the flow sequence wraps while yeptris's suite-pinned plain reading (HM87/652Z) keeps one `?`-led scalar member — AND the key's VALUE bytes diverge underneath, libyaml's READ_LINE keeping the mid-plain LS/PS RAW while yeptris's spec fold folds them, so neither prior spell can carry it (r183's brace form demands a same-line plain-safe run; r190's `? ` form lets both engines fold the rewrite, and the folds disagree byte-for-byte, so its verify can never hold); the fresh arm `yd_r203_flow_seq_question_multiline_divergence` simulates the fold with libyaml's own law (raw LS/PS lead + LF-normalized trailing breaks; same-line spaces ride, break-adjacent spaces drop) and bakes the exact bytes into a double-quoted key with `\L`/`\P`/`\n`/`\t`/`\"` escapes so NEITHER engine folds the rewrite, the byte-strict verify pinning libyaml's original dump from both sides. Not an engine bug — the A/B at the pre-r202 parent 4581c78 aborts identically, a fresh find (r202 touched only the block-NEL gate), and both readings are suite-permitted. The unconstrained minimizer drifts to the 5-byte `[:\r|]` yeptris-only split twin (ledger-class), the DIFFER-constrained rerun pins the 15-byte `[?D<LS><PS>:(<PS>]`, the seeded pin is the true 37-byte artifact (`qflow-multiline-lsps-r203`, pin fuzz-diff-r203-qflow-multiline-lsps).
  - **the question-gap-colon pair rides verbatim (r207, #1146, nightly)** — the 24-byte flow map whose three `?` entries split both ways (run 42503100451, main 20e01be): yeptris's suite-pinned plain-'?' law (HM87) reads `?:-` and `?*M` as plain keys where libyaml's unconditional flow KEY-token law opens an empty key with `-` the plain and takes the ALIAS `*M` as a key — the qflow arm already owned both glued hits (`?:-` the r58 pair spell, `?*M` the r71 drop-`?`), but the artifact LEADS with an explicit-key pair whose `:` follows a blank/break gap (`?<TAB><SP><LF><TAB><TAB><SP><TAB><TAB>:`), and the r85 boundary spell doubled that pair's own separator — the `"":` spelling left a stray `:` both engines reject, so the byte-strict verify died before the glued hits were read; the widened `yd_qflow_question_colon_divergence` rides the form VERBATIM — a `?` whose follower is a blank/break gap (SP/TAB/LF/CR, NEL C2 85, LS/PS E2 80 A8/A9) up to the entry's OWN `:` is identical in both engines, so no spell is needed, a gap ending at anything else keeping the r85 spell — strictly claim-additive (the old spell always errored such rewrites, so no carried pin can contain the form). Not an engine bug — the A/B at the parent 7d5483e aborts on BOTH builds, a fresh find. The greedy minimizer drifts to the 3-byte `{:}` split twin (the r186–r202 drift law); the DIFFER-constrained rerun pins the 11-byte `{?\t:,?,?*M}`; the seeded pin is the TRUE 24-byte artifact (`qflow-question-gap-colon-r207`, pin fuzz-diff-r207-qflow-question-gap-colon); 430-mutant paired battery 78 gained / 147 carried both-red / 0 regressions, the six corpus walks byte-identical pre/post, `leaks --atExit` clean.
  - **the bare-`?` glue x alias-ride composition's empty-value sibling rides a spelled arm (r209, #1152, nightly)** — the r199 exact-bytes gray's family sibling with the head's value bound EMPTY (the 12-byte flow map `{?*L:\n,*L:(}`, run 42512673554, main 939c2f1; deleting the r199 seed's two multiline-plain bytes yields exactly this artifact), both splits live: the head `?*L:` reads ONE plain key under the suite-pinned plain-'?' law (HM87/652Z) where libyaml's 1.1 unconditional flow-KEY law takes the `?` as the explicit-key indicator and re-lexes `*L` as an ALIAS key (the r142/r76 cut), and the tail `,*L:(` rides the `:` into the alias name per 2SXE/W5VH/catch-142 (a plain-safe `(` follows) so the alias key is `L:(` with an empty value where libyaml's IS_ALPHA cut stops at `L`, re-lexes the `:` as the VALUE token, and reads `(` as the plain — so the family stops resurfacing one exact member per nightly, the carry is the structural spelled arm `yd_r209_qflow_bareq_alias_spelled_divergence`: the preserving spell inserts one blank at each glue site (`?*L` → `? *L`, the separated explicit-key form; every in-name alias colon spelled `L :(` — the ride law is context-sensitive by measurement, in the explicit-key head yeptris rides the colon even before a break, so every site is spelled except a `#` follower), then requires the r193 certificate's two pins — libyaml(rewrite) == libyaml(original) byte-exact and yeptris(rewrite) == libyaml(rewrite); a spell that moves a reading or the divergence elsewhere leaves a pin unsatisfied and the arm declines. The r199 15-byte sibling declines this arm BY MEASUREMENT (its spell makes yeptris reject the rewrite) and stays exactly where r199 put it. Not an engine bug — the A/B at the pre-r208 parent 0398dc6 aborts identically (the r208 surplus-break arm exonerated, the escalation clause does not fire), and the r208 documented drift twin stays documented and open (the 9-byte `>\n<NEL SP NEL NEL` block-scalar form — a different delta in a different family). Minimality for once: the shape-constrained and any-constrained greedy deletions BOTH converge on the 10-byte `{?*L,*L:(}` with the dump pair byte-identical to the artifact's (the seeded pin is the TRUE 12-byte artifact, `qflow-bareq-alias-spelled-r209`, pin fuzz-diff-r209-qflow-bareq-alias-spelled). Battery: artifact 134 → 0 (standalone and -lf); 50-mutant paired battery 0 regressions; the six corpus walks clean pre and post (`-runs=0`); ctest `-R fuzz-diff` 100/100; `leaks --atExit` clean.
  - **the harness's dump appends ride amortized capacity (r210, #1155, nightly — the first TIMEOUT-class finding, harness-only)** — the 81936-byte flow-`?` soup (13401 events) carries NO divergence (both engines accept, the dump pair byte-identical), but the ~50 full-input dumps it rode through the arm chain each grew their string one strlen+realloc per append (`yd_sem_out` in test/fuzz/sem_dump.h, `yd_libyaml_dump` in test/fuzz/diff_core.c), and the O(events²) cost crossed the CI Debug+ASan harness's wall-clock alarm on long dumps before a verdict could form; the carry is capacity-doubling appends in both dumpers — the emitted bytes identical, proven byte-identical pre/post by the mutant battery and the six-corpus walks — and the artifact pins as `fuzz-diff-r210-timeout-flow-soup-dump-append`. Not an engine bug and not a comparator arm — harness infrastructure.
  - **the root-alias collapse's breakless doubled-head member rides the r174 family (r212, #1162, nightly)** — the r174 doubled-head gray seed with the trailing break deleted (the 14-byte `&a:o&a:o::\n*a:`, run 42527856270, main cb168e1), the twin r174's own minimality list NAMES and left unclaimed beside the pinned cut (the r151/r170 deletion-sibling precedent): the mechanism unchanged since r157 — yeptris's suite-pinned riding laws (2SXE/W5VH plus the r142 anchor-charset carry) read line 1 as a props-only anchor declaration whose name rides both colon runs and the interior `&` to the break (the anchor pends) and line 2's `*a:` as an alias whose name rides its colon, so no value indicator opens a map and the document collapses to ONE root alias (`=ALI *a:`) where libyaml's 1.1 IS_ALPHA cut composes the anchored plain `:o&a:o:` as the root map's first key with the line-2 alias a second flat pair; the EXACT-BYTES gray arm `yd_r212_root_alias_double_head_breakless_gray` carries it beside the r157/r170/r174/r182/r192/r198 family claims (seed `root-alias-double-head-breakless-r212.bin`, pin fuzz-diff-r212-root-alias-double-head-breakless). Not an engine bug — the A/B at the pre-r211 parent 55d1ab5 aborts identically (exit 134, the same DIFFER pair), the r211 riding re-lex exonerated, a fresh find; 50-mutant paired battery 0 regressions (the survivors the r174 list's named family siblings), corpus walks exit-0 pre/post, `leaks --atExit` clean, no engine change.
  - **the root-alias collapse's blank-run member rides the r170 family (r218, #1180, nightly)** — the r170 no-indent seed with a BLANK-LINE RUN inserted between the anchored head and the alias line (the 28-byte `&a:o:` + 19 LF + `*a:`, run 42562016233, main ceea4ae, seed-corpus replay at MS: 0 — libFuzzer-minimal, no mutation chain), the family's named missing member (r157 pins the indented glued twin, r170 the glued twin, r182 the `&a::` indented twin, r198 the breakless indented twin — none carries the blank-line separator), taking its own byte-shape gray arm `yd_r218_root_alias_blank_run_gray` beside the r157/r170/r174/r182/r192/r198/r212 family claims (the r151/r170 deletion-sibling precedent): yeptris's suite-pinned riding laws (2SXE/W5VH — colons ride anchor and alias NAMES) make line 1 a props-only anchor declaration whose name rides the colon (the anchor PENDS across the INERT blank run — the pended anchor walks past pure-break lines) and leave line 2's colon-riding alias name without a value indicator to open a map, so the document collapses to ONE root alias carrying the pended anchor as props (3 dump lines), while libyaml's 1.1 IS_ALPHA cut splits the anchor at `a` (the r142/r76 law), the residual `:o:` re-lexes as plain `:o` plus the value-token colon (the anchored key with an empty value), and line 2's cut alias plus its EOL colon opens the second flat pair — the r170 shape behind an inert blank run; the arm walks the BYTE SHAPE (head line + ONE-OR-MORE blank lines + indent + `*a:` + blank-run tail), consulted in probe_one, the collapse blank-run-length INDEPENDENT (the 18-blank run shrinks to the 11-byte `&a:o:\n\n*a:\n` with identical dumps; the k=2/3 runs, breakless tail, trailing runs, space-only blanks, CRLF/lone-CR spells, head trailing-space, and indented members all ride the same skeleton), the TAB blanks declining by the no-tab gate (a tab flips libyaml's reading to a reject split — the differ-only routing declines splits, the r170 law). Not an engine bug — the A/B at the pre-r217 parent 0eb3b0f aborts identically (exit 134, the same dump pair), the r217 engine round exonerated, the escalation clause does not fire, a fresh find; 29-case blast-radius battery 0 mismatches, 50-mutant paired battery 13 arm-caught / 0 regressions, the six corpus walks byte-identical pre/post (parse +2 — the r217 and r218 seeds), all 108 fuzz-diff pins green, `leaks --atExit` clean (seed `root-alias-blankrun-r218.bin`, pin fuzz-diff-r218-root-alias-blankrun).
  - **the flow-SEQ question family's embedded-`?` member rides the r203 arm (r219, #1183, nightly)** — the 18-byte `[\n?+D<U+2028>.\nF:-?- \n]` (run 42567242070, main 3b7b3f8, minimized to the 15-byte `[?+D<U+2028>.\nF:-?-]`): libyaml's 1.1 flow KEY-token law reads the glued `?` as the key of a nested single-pair map (null value) while yeptris's suite-pinned plain reading (HM87/652Z) keeps `?+D…` ONE multi-line plain member — and the key bytes diverge underneath, libyaml keeping the mid-plain U+2028 RAW where yeptris folds one break to a space (the r203 pair exactly); the member's tail `F:-?-` carries a SECOND `?` mid-run, and mid-plain `?` is content in both engines' originals (libyaml's scan_plain never breaks at it mid-token), so the only refusal was r203's finder charset — the widen admits the embedded `?` and the double-quoted bake quotes it away, the byte-strict verify still arbitrating every hit. Not an engine bug — the A/B at the pre-r218 parent ceea4ae aborts identically (exit 134, the same dump pair), the r218 arm exonerated (the artifact matches none of #1180's documented alias siblings), a fresh find. The seeded pin is the minimized 15-byte artifact (`qflow-seq-question-embedded-r219.bin`, pin fuzz-diff-r219-qflow-seq-question-embedded); 54-mutant paired battery 0 regressions (18 intended abort-to-pass family flips), the six corpus walks byte-identical pre/post (full walk clean incl. the new seed), fresh ctest fuzz-diff 109/109, `leaks --atExit` clean.
  - **the flow-SEQ question family's glued-hash comment swallow (r222, #1191, nightly)** — the DIFFER-constrained minimum `[1,?##|,?<CR>:m]` (run 42586898367, main f45cc5b, crash-a651e854): libyaml's unconditional flow KEY-token law reads the glued `?` as a KEY and the `#` glued to that token sits at the next token position — `scan_to_next_token`'s unconditional `#` eat swallows `##|,?` through the break (the entry separator AND the following `?` ride the comment) and the next line's multiline plain re-lexes as the KEY's own scalar, while yeptris's suite-pinned plain reading (HM87/652Z) keeps `?##|` one plain member (`#` after a non-blank is mid-plain content, the r206 law) with the following `?` its own explicit key; the r183 spell cannot ride the shape (its follower gate declines a glued `#`, and the `{run: }` wrap cannot reproduce a comment that eats the entry separator), so the preserving spell DELETES the glued `#` through its comment's end and KEEPS the `?` — separated from the next line by the break it re-lexes as the explicit KEY in BOTH engines (the explicit pair carries no single-line simple-key law; a bare delete demotes the multiline plain to an implicit key and libyaml rejects); the innermost frame must be a SEQUENCE (the mapping orientation stays the qflow glued-hash walk's turf), the comment ends at CR/LF, the quote tracker is the r145 plain-run-aware one (the artifact's three `"` all ride glued mid-plain), and the byte-strict verify against libyaml's ORIGINAL dump arbitrates with the CR/LS/PS/NEL-normalized triple as fallback (the artifact's multiline key keeps the raw PS in libyaml's dump where yeptris folds). Not an engine bug — the A/B at the pre-r221 parent 5826206 aborts identically, a fresh site (the r220/r221 arms exonerated). Seed `qflow-seq-glued-hash-comment-r222.bin` (pin fuzz-diff-r222-qflow-seq-glued-hash-comment); 50-mutant paired battery 0 regressions, the six corpus walks byte-identical pre/post, fresh ctest fuzz-diff 112/112, `leaks --atExit` clean.
  - **the root-alias collapse's block-anchored glued-colon member rides the r157 family (r223, #1194, nightly)** — the 11-byte `&a::\n  *e:\n` (run 42595582912, main 6330309, crash-3f0335158b94b9260b77f1d5e80fb63cda358df8), taking its own gray arm `yd_r223_block_anchored_glued_colon_alias_divergence` beside the r157/r170/r174/r182/r192/r198/r212/r218 family claims: libyaml's 0.2 plain law frees a block-context `:` not followed by a blank as a plain-FIRST char — `&a::` scans as the anchor `a` on the plain `:` with the second glued colon blankz-followed and re-lexing as the implicit key's value indicator (the anchored key `:` owning the nested `{e: ""}` map) — while yeptris reads the head as ONE plain scalar and the indented `*e:` line's colon rides into the alias name (the r142/r76 charset carry), so no value indicator opens a map and the document collapses to the bare alias (`=ALI *e:`); the preserving spell rewrites the head into the explicit anchored key `&a ':' :` (a quoted `:` — the semantic dump is style-blind — plus the separated value indicator re-lexes as exactly libyaml's original KEY) and heals the glued alias separator `*e:` to `*e :`, the byte-strict verify against libyaml's ORIGINAL dump arbitrating with the CR/LS/PS/NEL-normalized triple as fallback (the r183/r221/r222 pattern). Not an engine bug — the A/B at the pre-r222 parent 3730a36 aborts identically (exit 134, the same dump pair), a fresh composition the r221/r222 arms exonerated. Seed `block-anchored-glued-colon-alias-r223.bin` (pin fuzz-diff-r223-block-anchored-glued-colon-alias); 60-mutant paired battery 0 regressions (9 family flips abort→pass), the six corpus walks exit 0 post-fix, fresh ctest fuzz-diff 113/113, `leaks --atExit` clean.
  - **the flow-SEQ question family's CR-crossing comment-swallow key binding rides its own arm (r224, #1197, nightly)** — the 345-byte flow soup (run 42604872810, main 988ba3e, minimized to the 21-byte `[Q,?##|,:<CR>bnaaryM,?g]`): libyaml's 1.1 KEY-token law makes the flow `?` a KEY token whatever follows and the scanner's next-token loop then treats the GLUED `#` as a boundary comment — the comment swallows to the NEXT LINE BREAK (a lone CR is that break) and the post-break plain binds as the keyless pair's key (`{bnaaryM: <empty>}`), while yeptris's suite-pinned plain-first-'?' law (HM87) reads the glued run as flat scalars (`?##|`, `:|`) with the CR folded INSIDE the plain. No single prior arm claims the shape — r185's comment-CR spell heals one `?#`+follower hit but leaves the tail's bare glued-`?` runs unhealed, and r183's spell declines a `:` inside the run (the artifact's tail rides `?tag:yaml.org`), so the fresh arm `yd_r224_seq_question_glued_hash_cr_run_divergence` composes the two healing spells in one walk: HIT A keeps the `?` and deletes the `#` through the comment's end (the r222 keep-`?` law), HIT B rewrites `?run` → `{run: }` (the r183 spell) with the widening that a `:` GLUED inside the run rides as mid-plain content (a `:` after a blank declines), the byte-strict verify against libyaml's ORIGINAL dump arbitrating with the CR/LS/PS/NEL-normalized triple as fallback (the r183/r185/r222 pattern). Not an engine bug — the A/B at the pre-r223 parent 6330309 aborts identically (exit 134, the same dump pair), the r223 arm exonerated, a fresh find. Seed `qflow-seq-glued-hash-cr-key-r224.bin` (pin fuzz-diff-r224-qflow-seq-glued-hash-cr-key); 60-mutant paired battery vs main d5778d8 0 regressions (19 family flips abort→pass), the six corpus walks byte-identical pre/post, fresh ctest fuzz-diff 114/114, `leaks --atExit` clean.
  - **the flow-SEQ question family's fold cap widens 8 → 16 (r225, #1200, nightly)** — the 112-byte direct descendant of the r203 seed (run 42613510600, main 355568f, crash-277015f0): the plain-first-`?` run now rides 11 folds (LS/PS/LF/CR/CRLF spellings) with glued `:`s and an embedded second `?` — libyaml's 1.1 KEY-token law nests the run as a single-pair map's key (empty value) where yeptris's suite-pinned plain reading (HM87/652Z) keeps ONE plain member, the key bytes diverging underneath (libyaml keeps the mid-plain LS/PS raw, yeptris folds k−1; both readings suite-permitted) — and every r203 admission already held (the embedded `?` rides r219, the mid-run quote r191, the glued `:`s the pair-separator law, the run's glued-`#` count zero): the arm's fold cap rode 8 and declined the run scan, so the byte-strict bake verify never ran. The widen is the cap alone — the trailing[24] bake buffer still declines gracefully past its caps (wok=0 rides the hit as content), so the widening only reaches folds the verify then arbitrates. Not an engine bug — the A/B at the pre-r224 parent d5778d8 aborts identically (exit 134, the same dump pair), a fresh find. The unconstrained minimizer drifts to a 1-byte split twin; the DIFFER-constrained rerun pins the 26-byte `[?D<LS><PS><PS>\n<LS><PS><PS>\r\r"]`; the seeded pin is the TRUE 112-byte artifact (`qflow-multiline-fold-run-r225.bin`, pin fuzz-diff-r225-qflow-multiline-fold-run); 60-mutant paired battery vs d5778d8 0 regressions (29 admissions abort→pass), the six corpus walks byte-identical pre/post, fresh ctest fuzz-diff 115/115, `leaks --atExit` clean.
  - Each carry: bounded classifier rule + the decoded artifact (from the run log's Base64 line or the `fuzz-crashers` artifact) and its minimal as corpus seeds + a ctest pin — r116–r243 all green (129 fuzz-diff pins + the r201/r204/r213/r235/r239 roundtrip seed pins), prior pins un-un-classified, the historical standalone-mode aborts documented as out-of-scope.

## [0.6.60] - 2026-11-03

### Added

- **schema SOTA 2020-12 complete: reference resolution + the unevaluated* model (#912, #901)** — compile-time $id resource crawl with $anchor/$dynamicAnchor fragments and RFC 3986 resolution (percent-decoded pointer fragments, ~0/~1), $dynamicRef's runtime dynamic scope (the OUTERMOST schema resource defining the anchor, core spec 8.2.3.2 + appendix C), and the unevaluated* annotation model (mark/release windows, upward-only collection from successful branches, `not` opaque outward, unevaluated* evaluated last). The regex kernel grew to spec: \c control escapes, a position-aware Pike VM with whole-code-point classes, Unicode \s and \D\W\S complements, generated Unicode 13.0.0 \p{Letter}/\p{digit} tables. Suite verdict: **1274/1298 pass, 24 carried (was 1158/140), 0 failed** — every residual carry is one documented policy family (targets outside the schema's own identifier space: no fetch-at-validate-time; MATRIX.md states the stance). Optional ecmascript-regex suite: 74/74, zero carries. Diag codes 34/35 appended; the ABI rail extended.
- **the event-route validator (#913, #902)** — `yeptris_validate_stream` rides the DOM route's exact parse (front-end, guards, statuses) into a streaming state machine: no node tree is ever built. The family checks are fact cores both walks drive — the routes cannot diverge by construction; the differential gate compares status, count, and every diagnostic field (code/path/detail/line/col/offset) across ~120 shapes x both families, 294 runs, 0 mismatches. Genuine second-root streams carry libyaml's diagnosed words via the new `YEP_ERR_DOC_BOUNDARY` code.
- **tag-driven schemas: the registry (#916, #906 slice 4)** — `yeptris_schema_registry` binds user tags to compiled schemas (byte-exact resolved-tag match, re-register replaces, registry borrows its schemas — free the registry first); `yeptris_validate_ex`/`yeptris_validate_document_ex` (size-versioned options, the emit-options pattern) walk a bound node's subtree against its schema with tag-site path attribution. Unregistered tags pass untouched; nested bindings compose.
- **the structured output face (#916, #906 slice 5)** — `yeptris_validate_report_write` renders the report as the published Recommended Output meta-schema's BASIC shape (one root unit + one unit per diagnostic), a pure deterministic allocation-free transform pinned byte-for-byte against the spec's §12.4.4 example; the vendored meta-schema self-validates the transform through our own validator. DETAILED/VERBOSE are schema-structured and deliberately not faked from the flat enumerated-misses report.
- **the FULL YAML 1.2 family lane (#918, #906 slice 3)** — `YEPTRIS_SCHEMA_12_FULL`: core 1.2 productions verbatim (spec 10.4's base-schema law) plus the yaml.org carried-forward set — RFC3339 timestamps on untagged plains across spec example 2.22's four shapes (canonical/iso8601/spaced/date), classified by PyYAML 6.0.2's timestamp_regexp as the practice oracle (a bounded 51-entry differential agrees on every verdict where the scanners deliver the same bytes; the deliberate divergences — 1.2-over-1.1 numeric laws, byte-level canonical keys, `!!binary` riding — documented), the merge/value words, and explicit `!!binary` passthrough (base64 shaping is construction, not resolution). The validator asks a `timestamped` capability bit on the family resolver, so the DOM walk and the #913 event walk family-check explicit !!timestamp content through one classifier. The 1.1 quirks (underscores, sexagesimal, y/n bools) stay in compat11 — full is spec-refereed, not psych-refereed.
- **the IETF WG draft is tracked (#914, #906 slice 2)** — `draft-ietf-jsonschema-json-schema-00` diffed against bhutton-01 (14 normative deltas, committed at docs/schema/ietf-wg-draft-delta.md): no keyword renamed/added/removed, the determinism/no-third-factors and rejection-voids-annotations requirements already held, the no-auto-fetch posture now explicit and matching our DIAG_REF stance. Zero evaluator changes demanded; four WG pins added (§4.2.2, §11.2, Appendix C dynamic scope, §10.4.1).
- **emit: block-side width folding + the indent option (#909, #903)** — `block_fold` (default 0) folds plain and single-quoted block scalars at best_width with libyaml's exact walk (pending-run column pricing, break-replaces-space, continuation re-indent at parent+indent; keys never fold, flow members unchanged, canonical/JSON fixed), and `best_indent` (default 2, clamped 2..9) drives nested map columns, seq-item pads, and literal indentation (ind=4 spells `-   a: 1` byte-identical to libyaml/PyYAML). Both fields appended size-versioned — old-struct callers keep today's bytes exactly. Ground truth from a raw libyaml 0.2.5 probe + PyYAML CDumper; documented divergences (byte vs char columns, DQ never folds, folded-plain re-emits literal) in the PR.
- **the value drains carry collection tag text (#910)** — an explicit tag on a sequence or mapping rides the value/columnar drains' span fields (an arena slice; untagged containers pay nothing, no stride change): a host constructor re-tags containers (`!!set` → set, `!!omap` → pairs) from one drain with no second pass. Route parity pinned (transform + linearizer byte-identical).
- **fuzz_validate rides the nightly (#907, #905)** — the validation instance walker gets its libFuzzer round over the checked-in seeds, and the nightly's budget divisor matches the harness count (was /6 for what are now 9 harnesses).
- **the tolerant per-document stream iterator (#923, #921)** — `yeptris_stream_iter_new/_next/_status/_free` frames documents engine-side with iterparse's abort-at-DOCUMENT_END sink (scanner-accurate: a quoted scalar spanning a marker-looking line never splits), reports failures per document (the failed run's line/col ARE that document's own — each run starts at the frame), and resyncs to the next line-start marker with a raw scan so the tail's valid documents still surface. Parse-class failures consume an index and continue; the stream-fatal classes (memory, encoding) end the walk; an unrecoverable tail ends it without hanging. The encoding front-end gates once (iterparse's law), and schema/compat/max_depth options ride every frame via the shared `resolver_for_schema`. Each document parses twice (boundary run + slice parse) — the cost note is the file header's contract; the Ruby `each_document` face rides it in leptris/yeptris-ruby.

### Fixed

- **the JSON family follows spec example 10.8 (#908)** — the 12_JSON walk enforced 10.2.2's `* -> Error` table row, so every block-style YAML document with plain word keys failed with one NON_JSON_TYPE per key: spec 10.2 is self-contradictory, and example 10.8 — the spec's own JSON Tag Resolution demonstration — resolves no-production plains as strings (keys AND the Invalid list). Example 10.8 wins: implicit no-production plains are strings on both routes; the family's teeth are the string-key demand and explicit non-JSON tags; the divergence from core is purely resolutional (`0o7` is a string under 12_JSON, an int under core). Six pins rewritten including the example document verbatim.
- **the marker line opens no block collection (#915, #900)** — after a `---`/`...` document indicator the parser's state machine hit an internal default arm ("unexpected parser state") on block-collection opens, and accepted `--- a: 1` documents libyaml/Psych/PyYAML all reject at 1:6. One law at the two collection-open choke points (shared by whole-buffer and chunked runs, all sinks, all encodings — the fused tape runner bails to the engine on marker lines): marker-line collections are rejected with libyaml's exact message and mark, genuine second-root documents carry "did not find expected <document start>" via `YEP_ERR_DOC_BOUNDARY`, and bare content after a break-form explicit document is absorbed into the open root collection (pinned). The impossible inline_doc frame state is removed. The oracle triple (libyaml 0.2.5 @ the nightly pin, Psych 5.5.0, PyYAML 6.0.2) agrees on every row; the suite's two suite-over-oracle carries (7Z25, HWV9) stand, documented. Every row pinned over tape/eager/push routes with byte-by-byte chunked-feed identity.
- **the comparator's tag-run split guard (#917, nightly r108)** — the r108 artifact's abort was a comparator-carry defect, not an engine bug: the all-splits heuristic cut inside a glued tag URI (`!!i&a` → `!!i&a :`), making the copy unparseable and skipping the verify retries. Bytes reached while a glued props run has carried `!` are tag-URI content in both walks of the diff harness; the artifact + the 12-byte minimal + a same-law drift ride as corpus seeds. Suite 2SXE ("Anchors With Colon in Name") pins yeptris's alias-name law correct; libyaml's IS_ALPHA charset lags (the already-carried r54 family).
- **the r107 harness gate (#897)** — the fuzz_diff composed-twin buffer gated on the TAIL while memcpy'ing the PREFIX: a late content BOM in a big dump wrote 25 KB into 8 KB of stack (the nightly's crash-7fda24ee). The gate now covers the whole input.
- **hygiene batch (#911, #904)** — MJ_DEBUG reads getenv once per process (cached atomic); the marshal reader's ~610 lines move pure to `marshal/to_json.c` (one writer engine, two consumers); step-state + pending-buffer and emitter scratch allocations ride the document/system allocators (fail-injection reachable, routing-asserted pins); a push-callback abort sets the TLS error channel at the aborting event's mark (events.h's promised contract, pinned); node-kind enum mixing contained at the boundary accessor. sc_route's per-byte flow switch was raced against the stopset kernel and measured under the bar — kept, with the numbers and recipe in the perf ledger.
- **the CBOR semantic-tag chain cap is one law on both ends (#920)** — the nightly's fuzz_cbor caught a deadly signal on 23 nested tags: the DECODER capped the tag chain at 64 links while the ENCODER's four split sites (and the encode key-scratch) used a different bound — encode_into built a chain the decode-side tables could not index and the plain build's SIGILL trap masked the site. `YEP_CBOR_TAGBUF`/`YEP_CBOR_TAG_MAX_LINKS` now govern decode (the cbor_note_tag link count) and encode (every split site + key scratch) from one header law; the 23-tag seed rides the corpus, and the ASan step-driver repro is in the PR.
- **the Pike VM's mark dedup gates on the initialized field (#922)** — Valgrind flagged mark_gen/mark_pos (and the dedup ring's gen/pos) read before the memset-ensured tag fields were written: the dedup conjunctions tested gen/pos FIRST, so an uninitialized gen could match by accident and skip writing the tag — the read order is now tag-first at all three sites (closure, ring push, consumer scan), which is sound by construction (tag==2 means gen/pos were written together).
- **every emit option field reads behind its own offsetof gate (#924)** — the #909 append grew `yeptris_emit_options` to 24 bytes, and the `canonical`/`best_width` readers still gated on the whole struct's sizeof: callers compiled against the 16-byte struct (the Ruby binding) silently lost both — `serialize(best_width: 30)` emitted at the 80 default. Per-field `offsetof` gates now, one law with the other three readers; the 16-byte-opts fold pin is red on the sizeof gate and green on the fix.
- **the comparator classifies the glued-hash family's accept/accept verdict (#925, nightly r115)** — fuzz_diff aborted on a SEED corpus input (crash-01361e26): the glued-`?#` family's other verdict — libyaml does not reject this shape, it EMPTIES the key at the glued `#` (the pair rides with a null key) while yeptris's suite-pinned plain-'?' law keeps the run as the key text; both accept with identical event skeletons. The quote-out classifier only consulted on YEPTRIS_ONLY verdicts, and cannot verify this variant anyway (quoting freezes the fold, breaking libyaml's line structure). The carry: the consult rides DIFFER too, a structural emptied-key classifier claims the exact signature, and the seed rides with a ctest pin alongside the r104-r108 pins (all 9 green, no un-classification).

## [0.6.59] - 2026-11-02

### Added

- **the compiled-schema economy (#885 item 3): compile once, validate many** — the leptris-XSD shape lands: `yeptris_schema_compile` parses a JSON Schema document ONCE into an opaque retained handle (the schema bytes, its DOM, and the pattern kernel's compiled NFA programs — a bounded 256-program cache, invalid patterns not retained), `yeptris_schema_validate` validates any number of instance streams against it, `yeptris_schema_free` frees everything in one call (document-scoped ownership; read-only sharing safe). `yeptris_json_schema_validate` keeps its exact one-shot contract and rides compile+validate+free internally — the JSON-Schema-Test-Suite's 1298 verdicts are byte-for-byte unchanged.
- **the JSON-Schema-Test-Suite port: the validate bar (#885 item 2)** — the official draft2020-12 suite vendored as fixtures (46 files, 1298 cases): 1158 pass, 140 documented carries, ZERO failures; every covered keyword passes 100% of its cases. Implemented this sweep: multipleOf (libm-free, inf-safe), min/maxProperties, patternProperties (the pattern kernel over key strings), propertyNames, contains+min/maxContains, dependentRequired, dependentSchemas, if/then/else; additionalProperties now excludes patternProperties matches. Carries (documented in MATRIX.md, UPASS-guarded): remote $ref/base URIs, $anchor/$dynamicRef/$id, unevaluated* annotation plumbing, the bounded regex subset's Unicode classes, metaschema vocabulary negotiation. The port FOUND AND FIXED four engine bugs: uniqueItems' mapping hash was key-order dependent (duplicate objects read as unique); const/enum compared numbers tag-strictly (`const 0` rejected `0.0`; `1` and `1.0` are one duplicate-key canonical form); unknown letter escapes were SyntaxErrors where ECMA-262 Annex B reads them as identity escapes; dependentRequired's detail printf read past a DOM allocation (caught by the port's own ASan CI leg).

### Fixed

- **a NULL/0-byte stream parses through a non-null base** — parse_impl's BOM arithmetic computed `buf + bom` on the caller's pointer: a NULL buffer with len 0 (the validate face's empty-schema inputs) made that NULL+0, undefined behavior clang's UBSan flags and the macOS arm64 sanitizer leg aborts on (gcc's UBSan stays silent, which is why ubuntu CI never saw it). Three validate-face tests aborted on it before the fix.

## [0.6.58] - 2026-11-02

### Added

- **the validation face (#882 slice 1): the schema-family walks + the JSON Schema 2020-12 subset** — `yeptris_validate` parses and validates in one pass against the YAML 1.2 Core/JSON families (explicit-tag consistency, mapping-key uniqueness by canonical form — `0o13` and `0xB` are ONE key — and the JSON family's string-key + JSON-production rules), and `yeptris_json_schema_validate` walks a JSON Schema draft 2020-12 subset (type with the zero-fraction integer rule, enum/const, pattern, min/maxLength in code points, min/maxItems, uniqueItems, numeric bounds, properties/required/additionalProperties, items + prefixItems, allOf/anyOf/oneOf/not, local `#/` $ref with a 64-hop chain guard, boolean schemas). Reports cap at 256 enumerated diagnostics (never first-fail), each with a JSON-pointer path and line:col. The `pattern` kernel is a bounded Thompson-NFA regex (Pike VM, O(subject x program), no backtracking blowup; unsupported constructs are SyntaxErrors that ignore the keyword — 2020-12's posture). The fuzz harness pins the status contract over untrusted bytes on both walks. The campaign continues on #885 (official-suite keyword sweep, compiled-schema economy, streaming validation, binding faces).

### Fixed

- **the validation harness's own invariant trap cannot fire on argument order, and its trap sites are named on every compiler** — `drain(validate(..., &r), r)` read the report argument in unspecified order; gcc-x86_64's right-to-left evaluation handed the harness NULL on every completed validation and tripped its own silent `__builtin_trap` (the trap naming was MSVC-only, so ubuntu CI showed a bare SIGILL while arm/clang/MSVC evaluated left-to-right and passed). The calls are sequenced and every trap site prints its name on all compilers; the standalone harness names the faulting PC for future digs.
- **the JSON Schema integer test reads the parsed value before its int64 range guard** — the guard once read `dv` before the parse (saw 0, folded constant-true) and left the narrowing cast undefined on out-of-range/NaN doubles; the range test now rides the parsed value (outside int64 range every finite double is already integral; NaN is not).
- **the regex closure stack bounds its worst-case depth** — the Pike VM's seeds plus one net push per marked instruction reach 2m+2 entries, one past the old 2m array at the full program budget.

## [0.6.57] - 2026-11-02

### Added

- **engine: the encoding front-end reaches every route (#877)** — `yep_engine_run` gated BOM/UTF-16/32 input, but `yep_engine_step`, the recorder, and the iterparse resume fed raw bytes to the engine: UTF-16 input silently produced garbage events (the round-122 bug class), and three resume-offset drifts reproduced on main. One gate now fronts every route (UTF-16/32 rejects as ENCODING at 1:1; the UTF-8 BOM strips across split feeds), iterparse resumes past the stripped prefix, and push events carry `end_line`/`end_col` so the push model's marks match pull/recorder (cross-model identity). Additive: `yeptris_recorder_status` reports the last feed's status with its 1-based error position (mirror of the pull/iterparse status calls).
- **engine: the marshal wire readers get a fuzzer (#879)** — both readers (`emit/writer.c` and `cbor/from_marshal.c`) are exercised by a new `fuzz_marshal` harness over a 17-seed corpus, wired into ctest and the fuzz nightly. The release workflow now builds the native assets BEFORE tagging, so a failed asset build can no longer strand a bare tag.
- **engine: the #863 plain-hex-scalar pin lands on main (#876)** — `Parse.PlainHexScalarStaysScalar863` pins the issue's exact document (the fontist build's scalar-kind contract) across the build matrix; the guard had been stranded on a branch.

### Fixed

- **engine: allocation failure reads as MEMORY on every route, and the DOM arena cannot corrupt silently (#878)** — the round-2 "clean error channel" discriminator provably never fired (`yep_engine_error` always returns the slot), so a fold-cap OOM reported PARSE on four routes; OOM is now discriminated by error code with route pins, iterparse carries a sink-OOM bit so a capture failure cannot masquerade as stream end, and the recorder's verdict is copied out before the engine dies. `yep_dom_str_put` OOM propagates through a sticky `dom->oom` + `dom_place` choke (a failed arena write can no longer read as silent success — `AllocFail.OomNeverReadsAsParseOrSilentSuccess`), uint32 capacity doubling is wrap-checked, and the arena/pool fit tests take the promised subtraction form (MSVC-clean sink init throughout).
- **engine: the nightly r104 composition classifies known (#880)** — anchor-charset x PS-fold, 16 PS-led break runs: the fold walk's forced-additive arm was depth-capped at 8 and the artifact outran it; the walk is now a linear loop, mutation-checked (unfixed input aborts, fixed input classifies), with the artifact as a corpus seed and the fold algebra pinned.

## [0.6.56] - 2026-11-02

### Fixed

- **engine: a raw NEL break run folds by the trailing-breaks law (#870)** — a line-start `!<` arms the break normalizer's verbatim region (the #836 phantom family), so raw U+0085 breaks reached the plain fold walk unnormalized; the walk slotted the scanner's empty span as a fold piece and reset the break tally per piece, emitting one fold-space per break where libyaml keeps N-1 literal newlines. The walk skips the empty piece and lets the run tally (the flow walk's law). Pinned across the artifact bytes, the LF control, the 1/2/3-break tally, the mixed LF+NEL run, and the recorder route (`Parse.RawNelBreakRunFoldsToNewlineTally`).

### Added

- **engine: the EncodingTwin pins (#871)** — the #867 dig proved yeptris reads the r101 artifact's content byte-identically in UTF-16BE and UTF-8 on every route (the engine half of that issue was verified NOT a bug; the one-mark law explains the "6 vs 7" comparison). `Parse.EncodingTwinBomOnlyFirstLineReadsTheSame` + `Parse.BomOnlyFirstLineBomIsContent` pin the law so the comparator's UTF-16 carry (#872) cannot mask a future engine regression.

## [0.6.55] - 2026-11-01

### Fixed

- **engine: the scanner/composer alias split (#847)** — libyaml's scanner emits the ALIAS event for ANY name; its composer rejects what it cannot resolve ("found undefined alias"). yeptris rejected inside `e_alias` unconditionally, so event streams (recorder/pull/push) diverged from libyaml's for every dangling alias — the nightly's round-101 finding. The rejection now rides a `compose` bit on the internal sink: the DOM and record-tape routes keep the parse-time error (yeptris_parse still returns NULL with the position and message — `Parse.Errors`, `AliasMemoResetPerDocument`, `AliasToUnplacedAnchorRejects` hold unchanged, and `YTape.ComposerRejectionOnBothRoutes` pins both routes), while event consumers carry the name for their host's resolution (`Parse.RecorderCarriesDanglingAlias`). The bindings own the carry now: the py loaders raise `found undefined alias` (PyYAML ComposerError parity) and reset anchors per document, the ruby materializer clears anchors at `DOCUMENT_START`.

## [0.6.54] - 2026-11-01

### Fixed

- **simd: the AVX2 `copy_count3` tail guards its scalar callee** — count-only callers hand len 0 with a NULL dst; forming `dst + 0` is UB (the nonnull attribute) even though nothing is dereferenced. NEON always had the guard; AVX2 mirrors it now. Pinned by `SimdText.CopyCount3RejectsNullTail`.
- **engine: OOM paths are honest and hostile sizes cannot wrap** — (1) the recorder arena's `arena_put` returned 0 on OOM while the caller still set `value_len`: materialize read whatever string lived at the arena head, or past a NULL arena on first-alloc failure — OOM now unwinds the record. (2) a failed SINK was mapped to `YEPTRIS_ERROR_PARSE` because the engine's rc is shared; the discriminator is the error channel (a parse error always sets it, a sink failure leaves it clean) — sink failures report `YEPTRIS_ERROR_MEMORY` in the recorder and iterparse now. (3) a dangling final UTF-32 unit (`len % 4 != 0`) was silently truncated while the UTF-16 twin rejects its own — rejects at the offending byte (`Transcode.Utf32DanglingUnitRejectsLikeUtf16`). (4) a size near SIZE_MAX wrapped `start + size` in the arena/pool fit tests — both allocators reject above `(SIZE_MAX - align) / 2` where every downstream sum stays wrap-free (`Memory.HostileSizesFailTheFitTest`).

### Added

- **ci: the NEON kernels get sanitizer exposure** — the Linux sanitizer jobs dispatch the AVX2 shapes only; `simd_text_neon.c` had zero coverage. A macOS arm64 (macos-15) job runs the full suite under ASAN+UBSAN with the 32 MB stack law.

## [0.6.53] - 2026-10-31

### Fixed

- **engine: the `0X` hex prefix rides a string in both schemas** — YAML 1.2 core's int regexp is lowercase `0x` only, and psych keeps `"0X1A"` a String in its 1.1 compat mode too; yeptris typed it INT in both. Both hex gates accept the lowercase prefix now (digits stay `[0-9a-fA-F]`). Pinned in `Core12.SpecTables` + `Compat11.PsychTables`. The audit's companion sexagesimal finding did not survive verification: the engine already matches psych on every row (3-group `190:20:30` -> Integer 685230 both; `1:2:3:4`/`1:75:30`/`1:60` -> String both).
- **engine: `serialize_json` spells no node properties** — JSON output carried YAML properties verbatim (`k: !` serialized as `{"k": ! ""}`, anchors rode inline, aliases wrote `*x` — every family invalid JSON, caught by the ts bridge's YAML-to-JSON export). The projection now drops tags and anchors and writes aliases as `null` (bounded: a dereference would loop on cycles); the json key rule stops letting properties select the explicit `? key :` form; the hostile-anchor-name laws still run on the json route, so a document with no safe spelling keeps failing the serialize. Pinned by `EmitJson.PropertylessProjection` (8 cases x default + compact).
- **engine: the marshal reader shares the parser's depth law** — `yeptris_json_from_marshal` recursed per nesting level with no budget: hostile bytes (header + N nested `[`, no Ruby needed) walked the stack to a SIGSEGV. Both recursive walkers (`mj_value`, `mj_discard_value`) now ride a shared depth budget capped at 1000, the parser's `max_depth` law; deeper streams reject with `YEPTRIS_ERROR_UNSUPPORTED`. Pinned by `MarshalJson.NestingRejectsBeyondTheDepthBudget` (999/1000 parse, 1001/100000 reject; a 200000-deep probe rejects cleanly).
- **engine: marshal bignum limb integrity — no silent truncation, no UB shift** — `e_bignum_text` used a fixed 128-limb array (~616 digits) while its entry gate admitted 1299-digit text: every value in between emitted a silently WRONG number (carries past limb 128 were dropped). The arrays now size from the gate bound (272 limbs); 10^700 (146 limbs) and the 1299-digit boundary (270 limbs) emit byte-identical to Ruby `Marshal.dump`, pinned with Ruby's true bytes. The reader's bignum decode shifted into a signed int64 — at limb 3 the 0xFFFF limb lands in the sign bit (UB) — the shift moves to the unsigned domain.
- **engine: line indents are 32-bit** — a 65536-space line wrapped the uint16 `yep_line_info.indent` to 0: the whole-document route kept the line verbatim (libyaml parity) while the step/feed route read it as a dedent and errored at the cut — whole-vs-chunked feed identity, the #839 family. `indent` widens to `uint32_t` across the scan struct, `yep_scan_line_f`, the step scan's inline builder, the quoted-floor parameter and the fold-walk comparisons. Pinned by `Parse.DeepIndentLineKeepsItsIndentPastUint16` (the scalar keeps the deep line verbatim and five split points replay identically).
- **engine: the JSON number scans bounds-check the entry index** — `yep_json_number_scan` and `yep_json_number_shape` read `p[*i]` before any bounds check: a caller handing `*i == len` (a number token ending exactly at the buffer end) read one byte past. Both exported primitives reject at entry now. Pinned by `JsonNumberScan.IndexAtEndRejectsWithoutTouchingPastTheBuffer`.

## [0.6.52] - 2026-10-31

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
