# The IETF jsonschema WG draft — delta against the published 2020-12

Campaign issue #906 slice 2. This is the diff record for
`draft-ietf-jsonschema-json-schema-00` (2026-05-12) against the published
2020-12 edition (`draft-bhutton-json-schema-01` + `draft-bhutton-json-schema-validation-01`,
June 2022). Everything below quotes the canonical text: the IETF archive
renders (`https://www.ietf.org/archive/id/draft-...-NN.txt`), fetched
2026-11-03 — per the spec-doc-not-memory law, nothing here is from memory
or from a summary.

## 1. What is compared

| document | bytes/lines | role |
| --- | --- | --- |
| `draft-ietf-jsonschema-json-schema-00` | 5544 lines, 99 pages | the WG's consolidated next-edition draft |
| `draft-dusseault-json-schema-00` | 5488 lines | the pre-adoption I-D (same authors); see §7 |
| `draft-bhutton-json-schema-01` | 4368 lines | the published 2020-12 core |
| `draft-bhutton-json-schema-validation-01` | 1680 lines | the published 2020-12 validation vocabulary |
| `https://json-schema.org/draft/2020-12/output/schema` | 97 lines | the published 2020-12 output metaschema (for §13 below) |

The WG draft folds the published core, validation, and output documents
into ONE document: §4 Core Keywords (bhutton core §8), §5 Subschema
keywords (bhutton core §10), §6 Unevaluated (bhutton core §11), §7
Structural Validation (bhutton validation §6), §8 format (bhutton
validation §7), §9 content (bhutton validation §8), §10 meta-data
(bhutton validation §9), §11 Loading (bhutton core §9), §12 Keyword
Behaviors (bhutton core §7), §13 Output Formatting (the 2020-12 output
document, which the WG no longer references as a separate I-D),
§14 Extensibility (bhutton core §8.3/8.4).

## 2. The WG draft's own change log (the primary evidence)

Appendix G.1, quoted verbatim:

> Compared to the "2020-12" version of JSON Schema, this draft makes
> the following changes.
>
> *  Consolidate the 2020-12 core and validation documents
> *  Delevel headers for a more readable Table of Contents
> *  Shift terminology to a terminology section
> *  Reorder conceptually: intro, keywords, processing and output,
>    extensibility.
> *  Define input and instance as different things.

None of the five listed changes is a keyword-semantics change. The
mechanical checks below agree.

## 3. Keyword inventory: nothing renamed, added, or removed

Mechanical extraction of the keyword names from the WG draft's keyword
sections against the bhutton pair, plus a section-by-section body diff
(the bhutton validation text renders two keyword names — `minLength`,
`uniqueItems` — unquoted in body prose, so a name-token scan alone
under-reports it; §6.3.2/§6.4.3 carry the same definitions as WG
§7.3.2/§7.4.3), shows the same 55-keyword inventory this engine tracks:
the same definitions section by section, and the same vocabulary URIs.

- Vocabulary URIs are UNCHANGED (all still `.../draft/2020-12/vocab/...`
  and `.../draft/2020-12/meta/...`; WG §4 quotes the Core URI
  `<https://json-schema.org/draft/2020-12/vocab/core>` — identical to
  bhutton core §8).
- The default dialect URI is UNCHANGED (WG §14.4: "The current URI for
  the default JSON Schema dialect meta-schema is
  https://json-schema.org/draft/2020-12/schema").
- No keyword is renamed, added, or removed. `contains`'s definition is
  rewritten (D4 below) with the same semantics; every other keyword's
  definition text carries over with `instance` → `input` substitutions.

**Impact on our evaluator: none.** The 1298-case suite verdict
(1274 pass / 24 carried) is untouched by this slice, and the WG draft
pins the same reading everywhere the suite pins one.

## 4. Normative deltas, one by one

Each entry: the bhutton text → the WG text → the impact on
`yeptris_json_schema_validate` / `yeptris_schema_compile`.

### D1 — "input" vs "instance": an instance is now only an accepted input

bhutton core §4.2.1: "A JSON value interpreted according to this data
model is called an \"instance\"." (every validated document is an
instance, valid or not).

WG §1.2.1: "A _validator_ (also known as an _acceptor_) is a process
that tests if a particular input is described by the schema, by
evaluating it against the requirements.  An accepted input is called an
_instance_ of the schema, and a rejected input is called a _violation_
of the schema."

**Impact: none.** Vocabulary of the prose, not of the evaluator. Our
reports and docs keep the 2020-12 "instance" reading (a diagnostic path
names an instance location even when the document is a violation);
renaming our public language would be a binding-facing churn with zero
behavior delta — carried, not implemented.

### D2 — NEW determinism requirement: no third factors

WG §1.2.1 (no bhutton counterpart; this paragraph is new in the WG
revision — it is absent from draft-dusseault-json-schema-00 too):

> In all validator implementations, equivalent JSON values MUST return
> the same validation result and annotations.  For example, selection
> between equivalent character escapes, or use of whitespace, does not
> affect the result.  This also means that a validation keyword MUST
> NOT accept or reject based on third factors (those factors besides
> the schema and the input itself).  A separate "outside verification"
> scheme that queries external data sources is possible, but outside
> the scope of this document.

**Impact: already held.** Our equality is the data model's
(mathematical value for numbers, code points for strings — pinned by
`test_validate.cpp:ConstEnumCompareNumbersAcrossIntAndFloatTags` and the
suite's const/enum zero-like groups), and no keyword consults anything
but the schema and the input. No code change; the fixture set pins the
WG-draft shape of the same machinery (§6).

### D3 — NEW: rejection voids collected annotations

WG §1.2.2 (new in the WG revision):

> Annotations may be presented as a set, or as a stream of events,
> however if the input is rejected during processing, this voids all
> annotations previously emitted from that input.

**Impact: already held.** Our annotation collection (the unevaluated*
model) is internal: evaluations collect upward through the in-place
applicators, a failed branch annotates nothing, and a failed overall
validation yields diagnostics only — there is no annotation stream a
consumer could have half-received. No code change.

### D4 — `contains` rewritten: the count-range form becomes the definition

bhutton core §10.3.1.3:

> An array instance is valid against "contains" if at least one of its
> elements is valid against the given schema, except when "minContains"
> is present and has a value of 0, in which case an array instance MUST
> be considered valid against the "contains" keyword, even if none of
> its elements is valid against the given schema.

WG §5.3.3:

> An array input is valid against "contains" if the number of elements
> that are valid against its subschema is within the inclusive range of
> the minimum and (if any) maximum number of occurrences.
>
> The minimum and maximum numbers of occurrences are provided by the
> "minContains" and "maxContains" keywords, respectively, within the
> same schema object as "contains".  If "minContains" is absent, the
> minimum MUST be 1.  If "maxContains" is absent, the maximum MUST be
> unbounded.

**Impact: none — the WG form is what we already implement.** The
evaluator counts matches and compares against `[minc, maxc]` with the
same absent-keyword defaults (jsonschema.c: `double minc = 1.0; double
maxc = -1.0 /* unset */`), which satisfies both texts (bhutton's
"at least one, except minContains=0" is the range [1,∞] with the 0
special case; the WG's range form covers [0,∞] uniformly). The suite's
minContains.json (28) + maxContains.json (14) + contains.json files pin
this reading at 100%. No change.

### D5 — `$id`: the empty-fragment allowance is withdrawn

bhutton core §8.2.1:

> If present, the value for this keyword MUST be a string, and MUST
> represent a valid URI-reference [RFC3986].  This URI-reference SHOULD
> be normalized, and MUST resolve to an absolute-URI [RFC3986] (without
> a fragment), or to a URI with an empty fragment.
>
> Therefore, "$id" MUST NOT contain a non-empty fragment, and SHOULD
> NOT contain an empty fragment.

WG §4.1.3:

> Requirements on $id value:
>
> *  MUST be a string
> *  MUST be a valid URI reference ([RFC3986], Section 4.1)
> *  SHOULD be normalized
> *  MUST NOT have a fragment ([RFC3986], Section 3.5)

The WG upgrades the empty-fragment form (`"$id": "https://x/#"`) from
"SHOULD NOT" to "MUST NOT". This is a SCHEMA-SYNTAX constraint — the
kind of assertion the metaschema check makes, not the evaluator. Our
face does not police schema syntax (2020-12 posture: a schema's own
validity is a separate check; keyword values we cannot use are ignored,
not rejected), so an empty-fragment `$id` base resolves exactly as
before under both texts. **Impact: none** (permissive superset,
documented). A future slice that adds schema-syntax diagnostics would
carry this as a syntax-level error, never an evaluation one.

### D6 — `$schema` value clause lost a word (editorial defect)

bhutton core §8.1.1: "The value of this keyword MUST be a URI [RFC3986]
(containing a scheme) and this URI MUST be normalized."

WG §4.1.1: "The value of this keyword MUST be a (containing a scheme,
per [RFC3986], Section 3) and this URI MUST be normalized."

The word "URI" dropped out of the WG text (the carried intent is
identical — a URI with a scheme, normalized). **Impact: none.** Listed
for the WG's issue tracker; we do not file to third-party projects.

### D7 — media-type name typo in the Core section

WG §4: "Core keywords MUST be implemented by any processor indicating
support for the "application/jsonschema+json" media type." — every
other occurrence in both drafts (WG §1.2.3, §3.4, §11.x, IANA §17)
reads `application/schema+json`. Editorial defect, no normative reading
supports a new media type. **Impact: none.**

### D8 — loading referenced schemas: the no-fetch posture is now explicit UX guidance

bhutton core §9.1.2: "The use of URIs to identify remote schemas does
not necessarily mean anything is downloaded, but instead JSON Schema
implementations SHOULD understand ahead of time which schemas they will
be using, and the URIs that identify them."

WG §11.1.2:

> Although it's impossible to cover all use cases, we start by assuming
> that an implementation given a schema with references to other
> schemas not in the same document can be given instructions about
> those other documents, and the implementation therefore SHOULD NOT
> automatically dereference network locations or search the network for
> schemas not already loaded in.
>
> What should implementations do when the referenced schema is not
> known?  The implementation could have error messages, flags or UX to
> explicitly get instructions to fetch a schema or to signal that it
> should be configured differently.

**Impact: already held — this is our documented stance.** The face does
no I/O; a reference outside the schema's own identifier space is
`DIAG_REF` at the instance (the suite's 24 carries are exactly this
boundary, MATRIX.md divergence family 1). The WG's "error messages"
sentence describes our diagnostic. No change.

### D9 — applicator annotation-aggregation sentence reworded

bhutton core §7.5: "In addition to possibly defining annotation results
of their own, applicator keywords aggregate the annotations collected in
their subschema(s) or referenced schema(s)."

WG §12.6: "Applicator keywords do not play a direct role in this
preservation." — with preservation itself still specified by WG §12.8.1
(the Collecting Annotations bullets are carried verbatim from bhutton
§7.7.1: keyword name, instance location, schema location path, absolute
schema location, values).

**Impact: none.** The rewording moves the aggregation duty from the
applicator section to the annotation-collection section; the observable
contract (annotations survive through `allOf`/`anyOf`/`$ref` branches
and feed `unevaluated*`) is unchanged and suite-pinned (allOf.json 30,
anyOf.json 18, oneOf.json 27, not.json 40,
unevaluatedProperties.json 129, unevaluatedItems.json 71 — all pass).

### D10 — unevaluated* preamble gains the car example (informative)

WG §6 adds: "For example if a subschema represents a car, which
requires between 2-4 wheels, and the value of "wheels" is 6, the input
object is not \"evaluated\" to be a car, and the \"wheels\" property is
considered \"unevaluated (successfully as a known thing)\", and does
not retain any annotations."

**Impact: none** — it restates the model we implement (only successful
evaluations annotate; failed branches annotate nothing). Suite-pinned.

### D11 — readOnly/writeOnly gains an annotated-output example (output face)

WG §10.4.1 is new vs bhutton validation §9.4: it walks the
username/password example to two annotation output units
(`keywordLocation`/`absoluteKeywordLocation`/`instanceLocation`/
`annotation`). This is a demonstration of the OUTPUT format (WG §13),
not a new keyword or a new assertion: readOnly/writeOnly remain
annotation-only in the WG text ("An instance document that is marked as
"readOnly" ... MAY be ignored ... at the authority's discretion").

**Impact: annotation keywords never assert — pinned** (§6 pins the
example's schema validating its example instance). The output-units
face itself is #906 slice 5 and is not built here.

### D12 — Output formatting folded into the document (§13)

The WG draft carries the 2020-12 output face inline as §13 with NO
normative change:

- The four formats are the same four: flag / basic / detailed / verbose
  (WG §13.2), with the same SHOULD/MUST lattice ("An implementation
  SHOULD provide at least one of the "flag", "basic", or "detailed"
  format and MAY provide the "verbose" format.").
- The minimum outputUnit fields are the same five-plus-two
  (WG §13.3: keywordLocation, absoluteKeywordLocation,
  instanceLocation, error/annotation, nested errors/annotations) —
  byte-for-byte the published 2020-12 output metaschema's `outputUnit`
  definition (`https://json-schema.org/draft/2020-12/output/schema`:
  `required: [valid, keywordLocation, instanceLocation]`, the
  valid:false → error-or-errors implication, the $ref/$dynamicRef in
  keywordLocation → absoluteKeywordLocation implication).
- The output validation schema URI is the same single URI (WG §13.4.5:
  `https://json-schema.org/draft/2020-12/output/schema`).

The delta is structural: the WG no longer names a separate output
I-D (the references carry only `[I-D.bhutton-json-schema-validation]`
and `[I-D.handrews-json-schema-hyperschema]` as informative). **Impact:
none for this slice** — the structured-output face is #906 slice 5;
when it lands, its reference spec is WG §13, and its contract is
already the published metaschema quoted above.

### D13 — reference updates (editorial)

- `relative-json-pointer` now normatively cites
  `draft-hha-relative-json-pointer-00` (bhutton cited the handrews
  personal draft). Same ABNF content; the format stays annotation-only
  for us (out of the assertion subset).
- The `regex` format's section becomes "8.3.8. Expressions" with the
  regex attribute as its sole subsection — the text is carried ("A
  regular expression, which SHOULD be valid according to the [ECMA262]
  regular expression dialect.").
- `[I-D.bhutton-json-schema-validation]` moved to the INFORMATIVE
  references (its content is folded into the WG body).

**Impact: none.**

### D14 — JSON Pointer fragments across resource boundaries: the MAY-not-support clause carries

WG §11.2.2 keeps bhutton core §9.2.1's clause verbatim: "An
implementation MAY choose not to support addressing schema resource
contents by URIs using a base other than the resource's canonical URI,
plus a JSON Pointer fragment relative to that base." — the WG only adds
a CREF noting the WG may yet forbid the cross-resource form outright
(issues #937/#1183 cited in the CREF).

**Impact: none.** Our resolver addresses embedded resources by their
canonical `$id` URIs (and the enclosing-resource pointer form only where
bhutton allowed it); no change while the text says SHOULD NOT / MAY.

## 5. Editorial defects observed in the WG text (for the WG's tracker, not filed by us)

Third-party I-D: we record, we do not file. All have no evaluator
impact:

1. WG §4.1.1 `$schema`: "MUST be a (containing a scheme" — "URI" lost
   (D6).
2. WG §4: "application/jsonschema+json" for the media type (D7).
3. WG §3: "skip toSection 12" (missing space); §5.2.5 "Iputs that fail"
   for "Inputs"; §8.2.2 "this specificaion"; §11.4.2 heading carries a
   stray closing quote: `References to Possible Non-Schemas"`.
4. WG §3.2: "JSON Schema Section 1.2.5 that wish to work with such
   differences" — garbled reference (bhutton: "JSON Schema
   vocabularies (Section 8.1) that wish to work ...").

## 6. What this slice shipped

No evaluator change was demanded: the WG draft's keyword set, its
vocabulary URIs, its dialect URI, and every implemented keyword's
semantics are the 2020-12 semantics the engine already holds (§2's own
change log says the same). The suite verdict MUST NOT and DOES NOT move:
`tests/` 1298 cases — 1274 pass, 24 carried (the no-network family),
`optional/` 74/74 — identical before and after this slice.

Pinned in `test/unit/test_validate.cpp` (`ValidateJsonSchemaWgDraft`
group) — the WG draft's own examples, verbatim, because they exercise
the machinery the WG text showcases and our subset covers:

1. **§4.2.2 `$defs`/`positiveInteger`** — the WG's canonical reference
   example (`items` → `$ref` → `exclusiveMinimum`); valid for [1,2,3],
   `EXCLUSIVE_MINIMUM` at `/1` for [1,-2].
2. **§11.2 root.json dereferencing** — `$id` base composition +
   `$anchor` plain-name fragment (`#item`) + the unresolved external
   (`other.json`) surfacing as `DIAG_REF` at the property the
   `additionalProperties` schema applies to — the WG's own walkthrough
   of the no-fetch boundary (D8).
3. **Appendix C `tree`/`strict-tree`** — the WG's flagship
   `$dynamicAnchor`/`$dynamicRef` cooperative-extension example with
   the `daat` misspelling: outermost dynamic-scope resolution +
   `unevaluatedProperties` rejecting the misspelled key, the clean
   instance passing.
4. **§10.4.1 readOnly/writeOnly** — annotation keywords never assert:
   the example's instance validates (D11).

No conformance-fixture change (`test/conformance/jsonschema/`): the
upstream suite is keyed to 2020-12 and pins bhutton readings that the
WG draft does not alter (§3), so there is nothing to re-vendor and no
suite case conflicts with a WG reading. The suite remains the bar.

## 7. draft-dusseault-json-schema-00 divergence

The WG draft is the adopted revision of the personal draft: same three
authors, same body. Against `draft-dusseault-json-schema-00` (2026-02-26)
the WG text (2026-05-12) differs in boilerplate (draft name, dates, the
Discussion Venues block removed) and in exactly one substantive move:
the personal draft's §2 "Overview" became §1.2 "Functionality", gaining
the two paragraphs quoted at D2 and D3 (determinism/no-third-factors;
annotations voided on rejection) — `grep` confirms neither paragraph
exists in the personal draft. Section numbers shift by one from §3 on
(personal draft §4 "Overall Definitions" → WG §3). All keyword
definitions, the reference framework, and the change log are identical
paragraph-for-paragraph modulo renumbering. Nothing in the personal
draft diverges from the WG text on any implemented keyword.

## 8. Re-check cadence

Slice 2 tracks the WG: each WG publication (-01, -02, ...) gets this
same treatment — fetch the canonical text, re-run the mechanical
inventory + paragraph diff, extend this document, implement what the
deltas demand. The published 2020-12 suite stays the conformance bar
throughout; a WG change that would move a suite-pinned reading is
documented here with the suite case names BEFORE any evaluator change
is considered.
