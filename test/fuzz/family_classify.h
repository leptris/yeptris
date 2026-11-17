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
 *  (d) the plain-`?` law (suite HM87/652Z + r199/r209/r264): a `?`-led
 *      member with a non-blank follower keeps the whole text as one
 *      plain key (colon rides inside when a non-blank follows), values
 *      empty; libyaml splits the explicit key and its IS_ALPHA cut
 *      surfaces `=ALI *M`-style keys;
 *  (e) the r230 double-split residual-colon law and the m43/m44
 *      finding: a spliced/dropped `:` at a fold point moves neither
 *      dump (the fold-point `:` re-lexes as the VALUE token on both).
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
 *      state); past 32 deep stays UNKNOWN.
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
 *        SEQUENCES, member-start collections, single quotes, escapes)
 *        -> UNKNOWN;
 *  (l)   a mismatch inside a quoted span (beyond the law-b folds) ->
 *        UNKNOWN;
 *  (m)   the classifier consults no arm pins -- it stands alone;
 *  (n)   total budget bounded (refuses inputs over 16 KB -> UNKNOWN).
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
