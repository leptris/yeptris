/* family_classify.h — the law-verifying classifier for the glued-`?`/#/
 * breaks flow-soup family (issue #1310).
 *
 * The nightly's fuzz_diff comparator leg reds on fresh members of this
 * family faster than per-member EXACT-BYTES pins can be adjudicated (the
 * family is measured unbounded in the input space, the r256 de-novo
 * proof). The owner call in #1310: generalize the adjudicated laws into
 * a verdict function so an input whose divergence resolves EXACTLY into
 * them classifies as documented without a per-member arm, while anything
 * the classifier cannot fully explain stays DIFFER.
 *
 * The laws (each owner-verified by byte walks, r244-r266):
 *  (a) the #949 law: a member whose text begins with a GLUED `?` or
 *      `?##` (`?` directly followed by non-blank) is ONE flat plain
 *      scalar on yeptris (explicit-key indicator NOT split out), while
 *      libyaml treats the `?` as an explicit-key indicator (the 1.1
 *      unconditional flow KEY-token quirk), comment-eats from any `#`
 *      to the line break, and rebinds the explicit key with an empty
 *      value (the post-break bytes become the key);
 *  (b) the 1.2 break law: exotic breaks (NEL U+0085, LS U+2028,
 *      PS U+2029, CR, TAB-LF folds) fold to SPACE on the yeptris side
 *      while libyaml rides the raw bytes verbatim inside plains;
 *  (c) the alias law: `&anchor`/`*alias` reached after the comment-eat
 *      collapses to an empty key on libyaml (the anchor prop renders on
 *      the empty scalar; the IS_ALPHA alias cut), while yeptris keeps
 *      the bytes as plain content or the alias node (the m39/m40
 *      precedent); the r301 widening (run 44006575967): the split
 *      explicit-key slot's alias (`? *L ,`) rides the same blank/break
 *      run to the separator/closer the bare-alias member walks — the
 *      map value slot stays the parser's empty scalar; a ':' across
 *      the run (a VALUE boundary the probes never cover) stays out;
 *      the r302 widening (run 44034382404): an ALIAS at a `:`-boundary
 *      VALUE slot (the floor's `?#:<TAB>*% ,` — the fold-colon value
 *      followed by the TAB-riding alias, the plain-key twin
 *      `L:<TAB>*% <sp>,`) rides the engine's name cut verbatim
 *      (`=ALI *<name>` in the value position) with the same
 *      blank/break run to the terminator the bare-alias member walks;
 *      a ':' after the alias and any other byte stay out;
 *      the r326 widening (run 44624659366): the split explicit-key
 *      slot's alias ride reaches a blank/break-preceded `#` — the
 *      engine's comment condition fires, the run eats inertly to its
 *      break (the r279/r309 boundary matrix, now on the alias slot:
 *      the floor's `?\t*L #\n,` site and the artifact's
 *      `?\t*%L #|...` one), and after the ride either the
 *      separator/closer derives as before or a ':' + blank/break
 *      re-lexes as the VALUE boundary (the pair's empty value opens —
 *      `=ALI *<name>` + the inserted empty, identical rendering).
 *      libyaml never scans such an alias: the law-a KEY + comment-eat
 *      swallows the whole member tail including the name, so the
 *      engine's wider alias-name cut (the floor's `*%L` — libyaml's
 *      scan_anchor stops at the non-IS_ALPHA `%`) never surfaces
 *      against it. A ':' across the PLAIN ride (no comment), the same
 *      comment ride on the bare-alias member, an eat to EOF, and any
 *      other byte stay declined;
 *      the r342 widening (run 44891943987): the alias-name cut rides a
 *      colon CLOSED BY A BREAK (the artifact's `*00:` + LF is ONE name
 *      `00:` — the engine's prop walk has no break exception on the
 *      colon; probed `? *00:<LF>,` -> `=ALI *00:`, the bare-alias
 *      member `[*00:<LF>,]` twin, and the anchor twin `&a:`; a blank or
 *      a flow indicator still closes the name — probed `*00: x` /
 *      `*00:,` cut before the colon). With the ride, the split
 *      explicit-key slot's run after the name may reach CONTENT: the
 *      engine's flow-map parser binds that node as the pair's VALUE
 *      with no VALUE token ever firing (the floor `? *0:<LF>m` ->
 *      pair(*0:, m); probed `? *a<LF>  m,`, `? *a:<LF>  m, n`, the
 *      separator's empty value included). libyaml never scans the ride
 *      — its IS_ALPHA cut hands the glued ':' to its flow VALUE token
 *      and the same post-run plain lands in ITS value slot, so the
 *      dumps agree but for the NAME bytes (the r311/r318 charset
 *      divergence); libfyaml 04e0b58 corroborates the riding name
 *      event and declines the binding itself (the documented third
 *      reading). The SEQ single-pair wrapper renders the alias key
 *      event exactly as the MAP branch does. An alias, an anchor, a
 *      nested map, or any ':' reached across the run stay declined;
 *      the r315 widening (run 44411162279): the ANCHOR slots take the
 *      same run — the bare-anchor member, the anchored-empty value
 *      slot, and the open-pair fold-in member ride a blank/break run
 *      between the name and the terminator (the same prop walk; the
 *      engine skips the run before the flow indicator and nothing
 *      folds into the empty node — the artifact's `&i<CR>,` /
 *      `&binary<CR>,` sites render `=VAL : &<name> ` byte-exact on
 *      both engines); a `#` inside the run still declines (the engine
 *      comment condition fires there), and the open-pair fold-in's
 *      GLUED-`?` plain member rides P7's content law while the SPLIT
 *      explicit-key spell stays out;
 *      the r318 widening (run 44458062455): the MAP key slot's
 *      charset-divergent ANCHOR member joins. When the engine's yep_scan_prop_end cut
 *      and libyaml's scan_anchor cut DISAGREE and libyaml's cut lands
 *      on a GLUED ':' (the artifact's `&m:(}` — the engine's prop
 *      charset keeps `:(` inside the name per the suite (2SXE/W5VH;
 *      libyaml is behind the suite, the r311 law), libyaml re-lexes
 *      the tail: the 1.1 flow VALUE token fires on the glued ':'
 *      (key = anchored-empty `&m`, value = the plain `(`), while
 *      yeptris keeps the whole name so NO VALUE token fires and the
 *      parser inserts the empty value at the closer (`=VAL : &m:( ` +
 *      `=VAL : `). libfyaml 04e0b58 reads yeptris's stream byte-exact
 *      on every probe (`{&m:(}` / `{*L,&m:}` / the 12-byte artifact) —
 *      comparator carry only. Gates: the ':' glued AT the libyaml cut
 *      (a blank/break between cut and ':' re-opens the value-slot
 *      compositions the probes never cover), the prop name terminated
 *      by a flow indicator, the divergence real (an agreeing name is
 *      the r76-arm territory, declined), and the MAP level (a SEQ
 *      anchor member with a divergent name errors libyaml's parser —
 *      not a DIFFER shape); the libyaml walk derives the cut tail
 *      independently and the entry's dual byte-equality is the
 *      firewall; the r357 widening (run 45033480083): the BARE-ALIAS
 *      member at the flow MAP key slot takes the GLUED-':' VALUE
 *      boundary when a flow indicator follows the colon (the artifact
 *      `{*L:,&-:}`'s first member — the r342 cut law's own face at the
 *      key slot: a flow indicator closes the engine's alias-name walk
 *      BEFORE the colon, probed `*00:,`, and the artifact's own y-dump
 *      `=ALI *L` + `=VAL : ` pins it; the glued ':' fires the VALUE
 *      token on BOTH engines — libyaml's flow fetch_value on the
 *      alias's saved simple key; probed `{*L:,}` EQUAL). The member
 *      derives as the alias key + the parser's inserted empty at the
 *      terminator; a content follower (`{*L:x}` — EQUAL, never a
 *      DIFFER, no claim exists) and the SEQ slot stay out. libfyaml
 *      04e0b58 reads the artifact's divergent member byte-exact
 *      yeptris's way in BOTH passes (the key node's anchor is `<-:` —
 *      the glued-colon prop ride, the r318 law; its alias-member third
 *      reading documented), the charset law suite-pinned (2SXE/W5VH;
 *      libyaml behind the suite, the r311 law);
 *  (d) the plain-`?` law (suite HM87/652Z + r199/r209/r264): a `?`-led
 *      member with a non-blank follower keeps the whole text as one
 *      plain key (colon rides inside when a non-blank follows), values
 *      empty; libyaml splits the explicit key and its IS_ALPHA cut
 *      surfaces `=ALI *M`-style keys; the r339 widening (run
 *      44864014989): when the follower is an ALIAS (`?*L` — the r318
 *      composition's comma-glued-colon face) the member's colon +
 *      flow-indicator boundary derives as the VALUE boundary on both
 *      engines even GLUED — libyaml's flow `?` fires the KEY token and
 *      the `*` fires the ALIAS token (fetch_key removes the simple key
 *      and `simple_key_allowed = (!flow_level)` = 0 saves no key; the
 *      complex-key fetch_value path is check-free in flow), so NO plain
 *      is ever open at the colon and the r291 decline's still-open-plain
 *      premise is false (libfyaml 04e0b58 reads the artifact byte-exact
 *      yeptris's way; a plain follower keeps the decline — there the
 *      libyaml plain opens and the boundary shape is the
 *      yd_r242/yd_qflow family, not the classifier's);
 *  (e) the r230 double-split residual-colon law and the m43/m44
 *      finding: a spliced/dropped `:` at a fold point moves neither
 *      dump (the fold-point `:` re-lexes as the VALUE token on both).
 *  (e2) the r309 value-slot comment ride (run 44178312079): a
 *      whole-line comment reached ACROSS the blank/break run INSIDE a
 *      pair's value slot (after the `:`-boundary) rides — the `#` is
 *      blank/break-preceded (the engine's comment condition), so both
 *      engines eat it to its break and the value resumes after the run
 *      (the r309 artifact's `?<CR>/<LF><CR><CR><LS> 50:<CR>##|,?<CR>
 *      2002:binary` member pairs (/\n\n\n50, 2002:binary) byte-exact
 *      on both engines while the un-ridden model read `##|,?` as the
 *      value plain). The ride loops (the r307 multi-comment law, one
 *      slot deeper); a GLUED colon-`#` is the documented
 *      yeptris-rejects/libyaml-accepts split, never a comment, and
 *      stays out; an eat to EOF stays declined.
 *  (f) the r277 MAP-steal laws (run 43657665584): an explicit-`?`
 *      member whose key-position scan hits a blank-preceded `#`
 *      comment ride — with ZERO separators inside the run on a MAP
 *      root, the post-break content is the member's key slot, and a
 *      GLUED `:` leading it (the 9-byte floor `{?TAB#LF:#LF}`) is
 *      plain content on yeptris (the 1.2 ns-plain-first rule) while
 *      libyaml's flow scan splits ANY colon as the VALUE token and the
 *      line's tail comment-eats (yeptris keeps the plain key, libyaml
 *      renders the empty-key pair). And the whole-line comment run
 *      between a member's plain tail and its separator: inert on both
 *      engines, the plain ends at the run and terminates at the next
 *      flow indicator.
 *      the r319 widenings (run 44470194441): (1) the comment run's
 *      separator count is INERT comment text — the artifact's
 *      `?<CR>#, 12?1#|,?<CR>/...` member carries two commas inside the
 *      eat and both engines render the identical SEQ-rooted steal
 *      (the probes `[a,?\r#x,y,b\nc]` / `[a,?\r#x,y,z,b\nc]` render
 *      the r245 `+MAP(c,)` byte-exact); the root matrix keeps its
 *      shape (a comma-carrying run on the MAP root and a clean eat on
 *      the SEQ root stay untested-declined; an eat to EOF declines);
 *      (2) the stolen key rides the SAME plain walk the split
 *      plain-key slot takes — SEQ-rooted, so the r272 KEY-implicit
 *      colon cut applies (the artifact's stolen key
 *      `/<LF><CR><CR><PS> 51:<CR><CR>1.0e17.tag` cuts at the
 *      colon+break boundary: key = `/\n\n\n51`, the common colon path
 *      opens the value `1.0e17.tag`); the MAP-rooted clean eat keeps
 *      the plain law; flat stolen keys never see the difference; (3)
 *      the INDICATOR-form `?` member's silent comma ride on the SEQ
 *      root: the engines consume ONE stray ',' past the member
 *      terminator byte-exact (the floor's `?,,g` member —
 *      `{e,e}` map then the plain `g`; `[1,?,,g,h]` -> (1,{e,e},g,h),
 *      `[1,?,,]` -> (1,{e,e})); one comma only — the triple form and
 *      the break-between form are BOTH-ERROR shapes and the MAP root
 *      errors the ride; the libyaml model's member loop already
 *      skipped the leading FLOW-ENTRY. The A/B: libfyaml 04e0b58 joins
 *      the shared reading on the steal probes byte-exact and rejects
 *      the ride as a third reading — comparator carry, no engine
 *      change.
 *      the r377 widening (the nightly's round 377, run 45317828972,
 *      main 7171f226 — the 23-byte giant floor's `?\r#\n5 ?#` member):
 *      the CLEAN eat (zero separators inside the run) on the SEQ ROOT
 *      joins — the parser inserts the empty value at the closer and
 *      the stolen key rides the same plain walk as the separator form
 *      (probed byte-equal `[1,?\r##\na,2]` -> `+SEQ/1/+MAP/a//-SEQ`,
 *      the floor's own member; the engines AGREE — the member is an
 *      agreeing composition riding the dual byte-equality firewall,
 *      like the r351 nested-SEQ slot). The MAP-rooted clean eat keeps
 *      the r277 law; the comma-carrying run on the MAP root stays
 *      declined. The giant (73,133 bytes, the nightly's own red)
 *      decomposed to this 23-byte floor (`5b312c3f230d0d31e280a96e2c
 *      3f0d230a35203f230a5d`): the glued-`?#` member's CR-run/LS fold
 *      faces (law b/d, the r361 neighborhood — claimed by the standing
 *      arms) + the clean-eat SEQ member (this widening) — NOT the
 *      soak finds' conjunctions (the alias/doc-lead faces are absent
 *      from the floor; three independent faces this round).
 *  (g) the r278 quoted-tail law (run 43675241311): inside a glued-`?`
 *      member the quoted-span bytes ride yeptris's flat plain as
 *      content (the plain fold law joins the piece breaks), while
 *      libyaml scans a real double-quoted scalar whose break runs join
 *      per the leading_break/trailing_breaks dance: a run led by
 *      CR/LF/NEL folds to ONE space when it is the only break, else
 *      the leading break discards and the rest join (CR/LF/NEL ->
 *      newline, LS/PS raw); a run led by LS/PS joins every break
 *      (LS/PS raw) — the probes `?"2<CR>"` -> `2 `,
 *      `?"a<CR><PS>b"` -> `a<PS>b`, `?"a<PS><CR>b"` -> `a<PS><LF>b`,
 *      the break-only `?"<CR>"` -> ` ` twin of the r274 law. Pure
 *      trailing BLANKS stay UNKNOWN (libyaml joins them raw, yeptris
 *      trims — unmodeled split).
 *  (g2) the r308 quoted-span exotic-fold law (run 44160636260): the
 *      member-led double-quoted span is no longer declined for its
 *      break bytes — an EXOTIC break (NEL/LS/PS) inside the span is a
 *      break like any other on the yeptris side (the 1.2 fold law
 *      counts the whole run: 1 break -> one space, n -> n-1 newlines;
 *      the r308 artifact's `",?g??...` member renders its
 *      `/<LF><CR><CR><LS>` site as three newlines with the LS folded,
 *      zero raw LS bytes in either actual dump), and the pending
 *      ONE-break run joins the close as the ONE fold space (the r278
 *      `?"2<CR>"` -> `2 ` law, the artifact's `.tag<CR>"` tail). A
 *      multi-break tail at the close and trailing blanks (pure or
 *      riding the tail run) stay UNKNOWN; a multi-break-LED span stays
 *      UNKNOWN. The libyaml side of a member-led span rides the same
 *      fc_quoted_value twin gates; when libyaml never OPENS the span
 *      (the law-a comment-eat swallows the glued-`#` member's tail
 *      including the quote) the bytes are comment content and the
 *      post-break rebinding walks on.
 *  (h) the r279 plain-tail comment law (run 43688038100): a
 *      blank-preceded `#` INSIDE a member's plain (not at the key
 *      position, not at a line start) ends the plain on BOTH engines —
 *      the run eats inertly to the break — and the member boundary
 *      after the break must derive: a flow indicator (the plain
 *      terminates there) or a `:`+blank/break VALUE boundary (the
 *      colon re-lexes as the VALUE token, the floor's
 *      `{?# #\n:\t}`: the glued-`?#` plain `?#` rides the key with the
 *      run's `,`/`\`` bytes eaten, the post-break `:`+TAB opens an
 *      empty value; libyaml fires the KEY token for the flow `?`,
 *      comment-eats the rest of the line, and pairs the KEY/VALUE with
 *      inserted empties). Content resuming after the run, a second
 *      comment line, and eats to EOF stay UNKNOWN.
 *  (i) the r281 nested-flow-map law (run 43705128492): a `{` at a
 *      member's VALUE slot (after a `:`-blank/break boundary) opens a
 *      NESTED flow map on both engines and the same member laws derive
 *      its content recursively. yeptris: the colon boundary opens the
 *      value, the nested map renders +MAP/-MAP in the value slot, and
 *      the blank/break run after the close skips before the member
 *      terminator; a `:`-led member (the floor's second member — its
 *      `:` rode the libyaml comment) derives the EMPTY key with the
 *      nested value. libyaml: the flow `?` fires the KEY token, the
 *      comment eats the colon line, and the parser reaches the nested
 *      `{` as the explicit-key member's node (its simple-key slots are
 *      PER FLOW LEVEL — the nested level's saved key dies on the line
 *      cross). Maps only: a nested `[` stays UNKNOWN; a colon-led
 *      member is reachable exactly when libyaml comment-ate its colon
 *      (a raw flow `:` after a separator errors the parser's node
 *      state); past 32 deep stays UNKNOWN. The r310 widening (run
 *      44191067135): the nested `{` may hang off the split-explicit-key
 *      member's KEY SLOT itself (`?\t{...}` — the `?` drops, the `{`
 *      opens the key node, and the blank/break run after the close
 *      rides to the `:` VALUE boundary the same slot walk takes; the
 *      floor's `{?\t{},?-,?*M}` keeps the empty collection key and the
 *      two glued-`?` plains of the law-d split behind it, 13 of 48
 *      bytes under the dual bar). A glued colon (not a VALUE boundary)
 *      after the collection key and any other non-boundary byte stay
 *      out. The r377 widening (the r376b soak find 1, run
 *      45317828972's session): the collection follower GLUED to the
 *      `?` (no blank/break between) rides the same split machinery —
 *      the engine's `?` is the explicit-key indicator with the
 *      collection as the key node on BOTH engines, and the plain
 *      cannot start there (a flow indicator is not a plain-safe char;
 *      probed byte-equal `[?{a}]`, `[?[a]]`, `{?{a}}`, `[? {a}]` all
 *      EQUAL — the SEQ single-pair wrapper + the collection key node
 *      + the parser's inserted empty value). The find-1 floor's
 *      `[=,?{?#,`,*L\n}]` composes the glued wrapper with the
 *      nested-map law-a comment-eat (the KEY token's glued `#` eats
 *      `#,\`,*L` to the fold, the nested pair empties) and the law-c
 *      alias ride (`*L` renders `=ALI *L` on yeptris, never lexes on
 *      libyaml) — the 15-byte floor carries it under the dual bar.
 *  (l2) the r349 document-marker LEAD law (run 44941446715): a run of
 *      document-marker lines ahead of the flow root is INERT and
 *      byte-equal on both engines, each marker BUT THE LAST opening an
 *      EMPTY first document (one `=VAL : \n` per prior marker:
 *      `---\n[...` -> none, `---\n---\n[...]` -> one, three -> two; the
 *      r349 artifact's `---\n---\n[11 ,?##|,...]` lead renders
 *      `+STR\n=VAL : \n+SEQ...` on BOTH engines). Marker units: `---`
 *      at a LINE START (the first at input pos 0 exactly — an indented
 *      ` ---` is plain CONTENT on both engines, the probes' `=VAL :
 *      ---` doc, and stays OUT), padded with blanks (the tab-padded
 *      form dump-verified beside the spaces form), then LF, CR, or
 *      CRLF, or the INLINE root spelling (`--- [...]`, zero empties;
 *      a GLUED `---[` has no blankz after the triple — never a
 *      marker). Between units: spaces, LF/CR/CRLF breaks, and
 *      whole-line `#` comments at a line start (inert on both
 *      engines). Anything unverified — a tab or exotic break in the
 *      intervening run, a doc-end `...` line, the same-line `--- ---`
 *      pair, a comment behind marker padding — ends the lead at the
 *      last committed marker and the entry gate declines the tail
 *      exactly as before. libfyaml 04e0b58 corroborates the lead on
 *      the floor with BOTH passes (`+DOC ---` + `=VAL :` empty +
 *      `+DOC ---` + the seq on 1.2 and 1.1 alike) while its body
 *      reading joins yeptris's flat glued-`?` plain — the documented
 *      third reading; comparator carry only, no engine change.
 *      the r377 widening (the r376b soak find 2, run 45317828972's
 *      session): a blank/break RUN may precede the FIRST marker line
 *      (the floor's `\n---\n[?/ ?,?/]`) — both engines scan-to-token
 *      before the doc start, byte-equal across the probe matrix
 *      (`\n---\n[1]`, ` \n---\n[1]`, `\n \n---\n[1]`, `\r---\n[1]` and
 *      the exotic-break heads `NEL---\n[1]`/`LS---\n[1]`/
 *      `PS---\n[1]`/`\nLS---\n[1]` all EQUAL; the marker-count law
 *      composes unchanged behind the run, the `\n---\n---\n[1]` twin
 *      renders the one empty first document on both engines). The run
 *      admits spaces (each space run must face a break — a space run
 *      DIRECTLY before the triple is the indented-`---` content law,
 *      stays out) and every fc_break_len break kind; a TAB before the
 *      triple is BOTH-ERROR on both engines (probed) — the walk's
 *      decline matches. The find-2 artifact's `----\n` lead
 *      (a FOUR-dash line is plain CONTENT — both engines open the
 *      stream with the multi-line block plain riding the break runs
 *      into the `[`...) is the documented residual: a BLOCK-context
 *      face (law j), the flow family stays out — the 16-byte floor
 *      carries the law.
 *
 * DERIVE, don't pattern-match: the classifier independently segments
 * the input into flow members (parsing the member structure itself:
 * glued-`?` detection, quoted spans, alias/anchor positions,
 * comment-to-break regions), derives the EXPECTED yeptris dump AND the
 * EXPECTED libyaml dump per the laws, and requires both actual dumps
 * byte-equal to their expectations. Any byte it cannot derive -> UNKNOWN.
 *
 * Conservatism laws:
 *  (j)   block context -> UNKNOWN (the family is flow-only);
 *  (k)   any member outside the family's flat-member shape (anchors it
 *        cannot resolve, tags, nesting it cannot place — nested flow
 *        SEQUENCES, member-start collections, single quotes) ->
 *        UNKNOWN; ESCAPES stay outside except the two admitted
 *        followers: the r337 `\n` (decodes to the LF byte, the dump
 *        re-escapes it) and the r374 `\uXXXX` (see (g3)) — every
 *        other escape follower declines;
 *  (l)   a mismatch inside a quoted span (beyond the law-b folds) ->
 *        UNKNOWN;
 *  (m)   the classifier consults no arm pins -- it stands alone;
 *  (n)   total budget bounded (refuses inputs over 256 KB, derived
 *        dumps over 1 MB, collections past 16,384 members, libyaml
 *        token walks past 65,536 tokens -> UNKNOWN; the r362 raise
 *        carried the 73,133-byte r361 floor (8,243 members) and the
 *        148,798-byte r361 artifact (~25K tokens) by law — the dig
 *        measured the ENGINES size-independent on this family (the
 *        glued-`?` face is byte-identical at 8 bytes and 73KB), so
 *        the old exact-byte pins were carrying only the classifier's
 *        budget gates, not a reading dependence).
 *  (g3)  the r374 `\uXXXX` law (run 45259544033, main 84150a76): a
 *        well-formed `\uXXXX` inside a member-led double-quoted span
 *        derives on BOTH engines byte-identically — the 21-row
 *        matrix measured yeptris's and libyaml's decode EQUAL
 *        mid-content, chained, at the lead, at the close edge, and
 *        across the fold compositions; the decoded codepoint rides
 *        the dump escape (backslash/LF/CR/TAB/\xNN, non-ASCII raw —
 *        probed `A`, the decoded backslash, LF, C0, DEL, the C1 NEL
 *        byte, NBSP, the decoded LS, U+FFFD, U+FFFE). The decline
 *        domain is BOTH-ERROR, never a consult: surrogate halves and
 *        the malformed spellings (short hex, a bad digit) error on
 *        both engines. The escape flushes the pending fold run /
 *        blank run exactly as a content byte does; a surrogate
 *        declines (cheap — it can only be a both-error). The r374
 *        artifact's member-led span `"\r g61クリス"` (the CR lead
 *        folds, three chained BMP escapes) and the 19-byte floor
 *        `[1,"ク",?g?,""]` DIFFER only at the SIBLING `?g?` member
 *        (the pinned plain-`?` split) — the escape member itself
 *        renders EQUAL on both engines, but the old blanket escape
 *        decline left any DIFFER sharing a seq with it red.
 *  (l3)  the r375 tag-property LEAD law (run 45281713143, main
 *        4e6f0e78): a bare non-specific `!` tag at input offset 0
 *        followed by a blank/break run and then the ROOT bracket is an
 *        INERT lead for the member walks — the property rides the ROOT
 *        collection header on both engines (`+SEQ <!>`/`+MAP <!>`,
 *        byte-equal across the probe matrix: LF, blank, CRLF, blank+LF
 *        and LF+LF runs all agree; the collection's tag never
 *        "resolves by style" — only the scalar's empty render drops
 *        it, the r335 law). Gates, probe-backed: the bare `!` EXACTLY
 *        (a `!!` handle lead is a YEPTRIS-ONLY-ACCEPT split — yeptris
 *        renders `<tag:yaml.org,2002:>`, libyaml 90a56d4 ERRORS the
 *        parse; `!name`/`!<verbatim>` leads carry other resolved tags
 *        — the probes' `<!x>`/`<x>` renders agree but stay out),
 *        the run NON-EMPTY (a GLUED `![` is a YEPTRIS-ONLY-ACCEPT
 *        split — libyaml errors the tag-end), no comment inside the
 *        run (`! # c\n[1]` agrees on both engines but the comment
 *        form is a separate composition, stays out), and the bracket
 *        DIRECTLY after the run. The r375 artifact `!\n[?+<LS>:#,?]`
 *        (13 bytes) DIFFERs at the SIBLING member — the law-d
 *        plain-`?` map-count split (libyaml's 1.1 unconditional flow
 *        KEY-token law wraps `+<LS>:#` in the implicit single-pair
 *        map while yeptris keeps the flat glued plain with the LS
 *        folded to the fold space, the r374 matrix law) — the lead
 *        itself was the underivable byte: the flow-rooted gate
 *        declined every non-bracket head, so the whole tag-led
 *        composition stayed red. THE A/B: libfyaml 04e0b58 reads the
 *        member BYTE-EXACT yeptris's way (`?+\L:#` one plain scalar,
 *        the LS kept raw) in BOTH strict-1.2 and 1.1 passes — the
 *        comparators split along the libyaml-compat bar, engine
 *        defect REFUTED (the r318/r330/.../r371/r374 precedent).
 *        Comparator carry only, no engine change.
 *
 * Memory: no allocation escapes the call; all scratch is heap and freed
 * before return. Pure over its four arguments: same inputs, same
 * verdict, no globals, no I/O.
 */
#ifndef YD_FAMILY_CLASSIFY_H
#define YD_FAMILY_CLASSIFY_H

#include <stddef.h>
#include <stdint.h>

/* 1 = CLASSIFIED: the (data, ydump, ldump) divergence resolves exactly
 * into the documented laws above. 0 = UNKNOWN: the classifier declines
 * (and the comparator keeps the input red). When in doubt, UNKNOWN. */
int yd_family_classify(const uint8_t* data, size_t size, const char* ydump, const char* ldump);

#endif /* YD_FAMILY_CLASSIFY_H */
