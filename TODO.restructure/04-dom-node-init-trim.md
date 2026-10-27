# 04 — C: DOM sink node-init trim (ledgered DOM-side lever)

## Why
Milestone 45 bounded the DOM side at 12-25% of parse (recorder-vs-
DOM gap): dom_new_node's full memset + field writes per node and
unbatched arena copies. The dispatch itself measured within noise.

## Plan
- Profile dom_new_node/dom_place/yep_dom_str_put on scalar-heavy;
  trim the init to the fields each kind actually needs (measured,
  not assumed); batch arena appends where a run allows.

## Acceptance
- Same-binary bench, min-of-25, every shape no worse
- Full sanitizer gates; node-size check unchanged

## Status: ASSESSED — ceiling ~1-2%, closed without churn
After the ordinals + node-hint-reserve work, dom_new_node is a
64-byte memset + 5 field writes; the trimmable slice (per-kind
field subsets) is worth ~1% on scalar-heavy, and str_put batching
only touches non-borrowed strings (absent from the plain-heavy
shapes). The unit is closed by measurement; revisit only if the
direct-from-index DOM project (the real DOM-side lever) changes
the builder's shape.
