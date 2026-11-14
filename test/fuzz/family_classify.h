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
 *      precedent);
 *  (d) the plain-`?` law (suite HM87/652Z + r199/r209/r264): a `?`-led
 *      member with a non-blank follower keeps the whole text as one
 *      plain key (colon rides inside when a non-blank follows), values
 *      empty; libyaml splits the explicit key and its IS_ALPHA cut
 *      surfaces `=ALI *M`-style keys;
 *  (e) the r230 double-split residual-colon law and the m43/m44
 *      finding: a spliced/dropped `:` at a fold point moves neither
 *      dump (the fold-point `:` re-lexes as the VALUE token on both).
 *
 * DERIVE, don't pattern-match: the classifier independently segments
 * the input into flow members (parsing the member structure itself:
 * glued-`?` detection, quoted spans, alias/anchor positions,
 * comment-to-break regions), derives the EXPECTED yeptris dump AND the
 * EXPECTED libyaml dump per the laws, and requires both actual dumps
 * byte-equal to their expectations. Any byte it cannot derive -> UNKNOWN.
 *
 * Conservatism laws:
 *  (i)   block context -> UNKNOWN (the family is flow-only);
 *  (ii)  any member outside the family's flat-member shape (anchors it
 *        cannot resolve, tags, nesting, single quotes, escapes) ->
 *        UNKNOWN;
 *  (iii) a mismatch inside a quoted span (beyond the law-b folds) ->
 *        UNKNOWN;
 *  (iv)  the classifier consults no arm pins -- it stands alone;
 *  (v)   total budget bounded (refuses inputs over 16 KB -> UNKNOWN).
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
