# MATRIX.md — the JSON-Schema-Test-Suite port (draft2020-12), #885 item 2 + the 2020-12 sota sweep

The conformance bar for the `yeptris_json_schema_validate` face, exactly as
yaml-test-suite is the bar for the parse face. The fixtures are vendored from
the official JSON-Schema-Test-Suite at upstream commit
`3f05891cac5d51ffa068d7076d1372130e00c829` (`tests/draft2020-12/`):

- `tests/` — the 46 main test files (committed verbatim);
- `optional/ecmascript-regex.json` — run as its own honest section;
- `optional/format/` and the other optional directories are NOT vendored
  (format-assertion, cross-draft, non-bmp-regex and friends are out of scope
  for this face and are recorded here as skips), and neither is the upstream
  `remotes/` tree (see divergence family 1 — it is the reason the remote
  carries below are what they are).

The suite is MIT (Copyright (c) 2012 Julian Berman) — this notice is the
attribution the license requires for the vendored fixtures.

## How it runs

`test_jsonschema_suite conformance/jsonschema/carries.txt
conformance/jsonschema/tests` (and `... optional`): the driver
(`conformance/jsonschema/js_suite.c`, ctest names `jsonschema-suite` and
`jsonschema-suite-optional`) extracts the raw bytes of every `schema`/`data`
value with a bounded JSON span scanner — no re-serialization, so the suite's
number shapes (`1.0`, `1e308`) reach the validator unchanged — and expects
`YEPTRIS_OK` where the suite says `valid`, `YEPTRIS_ERROR_SCHEMA` where it
says `invalid`. Every other status is a hard failure. `carries.txt` names the
documented carries (file-level or group+test substrings); a carried case that
starts passing prints `UPASS` so the stale carry gets dropped. Any uncarried
failure fails the run.

## Headline

| section  | files | cases  | pass  | carried | FAILED | UPASS |
| -------- | ----- | ------ | ----- | ------- | ------ | ----- |
| tests/   | 46    | 1298   | 1274  | 24      | 0      | 0     |
| optional | 1     | 74     | 74    | 0       | 0      | 0     |

**Every covered keyword passes 100% of its suite cases; the optional
ecmascript-regex section passes 74/74 uncarried.** The sota sweep took the
verdict from 1158/1298 (140 carries) to 1274/1298 (24 carries) by landing the
machinery the draft's heart is made of: 2020-12 reference resolution, the
unevaluated* annotation model, and the pattern kernel's code-point classes
(`\p{Letter}`/`\p{digit}` property escapes, Unicode `\s`, the `\D`/`\W`/`\S`
complements, whole-code-point brackets and dot — a position-aware Pike VM).
The 24 remaining carried cases ride ONE documented policy family (below); no
case fails for an unexplained reason, and no carry is a fake pass.

## Per-file results (tests/)

| file                          | pass | carried | note                                          |
| ----------------------------- | ---- | ------- | --------------------------------------------- |
| additionalProperties.json     | 21   | 0       |                                               |
| allOf.json                    | 30   | 0       |                                               |
| anchor.json                   | 8    | 0       | `$anchor` + `$id` base composition resolved   |
| anyOf.json                    | 18   | 0       |                                               |
| boolean_schema.json           | 18   | 0       |                                               |
| const.json                    | 54   | 0       |                                               |
| content.json                  | 18   | 0       | content keywords are annotation-only in 2020-12 — every case expects valid |
| default.json                  | 7    | 0       | annotation-only                               |
| defs.json                     | 1    | 1       | the metaschema is an external resource        |
| dependentRequired.json        | 20   | 0       |                                               |
| dependentSchemas.json         | 20   | 0       |                                               |
| dynamicRef.json               | 39   | 5       | the five groups referencing the suite's HTTP fixtures |
| enum.json                     | 53   | 0       |                                               |
| exclusiveMaximum.json         | 4    | 0       |                                               |
| exclusiveMinimum.json         | 4    | 0       |                                               |
| format.json                   | 133  | 0       | format is annotation-only (no assertion) — every case expects valid |
| if-then-else.json             | 30   | 0       |                                               |
| infinite-loop-detection.json  | 2    | 0       |                                               |
| items.json                    | 29   | 0       |                                               |
| maxContains.json              | 14   | 0       |                                               |
| maxItems.json                 | 6    | 0       |                                               |
| maxLength.json                | 7    | 0       |                                               |
| maxProperties.json            | 10   | 0       |                                               |
| maximum.json                  | 8    | 0       |                                               |
| minContains.json              | 28   | 0       |                                               |
| minItems.json                 | 6    | 0       |                                               |
| minLength.json                | 7    | 0       |                                               |
| minProperties.json            | 10   | 0       |                                               |
| minimum.json                  | 11   | 0       |                                               |
| multipleOf.json               | 11   | 0       |                                               |
| not.json                      | 40   | 0       | annotations inside `not` collect (and never escape) |
| oneOf.json                    | 27   | 0       |                                               |
| pattern.json                  | 12   | 0       | `\p{Letter}` resolved by the code-point classes |
| patternProperties.json        | 27   | 0       | `\p{Letter}` resolved by the code-point classes |
| prefixItems.json              | 11   | 0       |                                               |
| properties.json               | 28   | 0       |                                               |
| propertyNames.json            | 22   | 0       |                                               |
| ref.json                      | 78   | 1       | the metaschema target (family 1)              |
| refRemote.json                | 15   | 16      | whole file carried (family 1)                 |
| required.json                 | 18   | 0       |                                               |
| type.json                     | 80   | 0       |                                               |
| uniqueItems.json              | 65   | 0       |                                               |
| unevaluatedItems.json         | 71   | 0       | the annotation model                          |
| unevaluatedProperties.json    | 129  | 0       | the annotation model                          |
| vocabulary.json               | 3    | 1       | vocabulary negotiation needs an external metaschema |

optional: `ecmascript-regex.json` — 74 pass, 0 carried, 0 failed.

## Keyword matrix

| keyword                    | state       | pass % | notes                                                    |
| -------------------------- | ----------- | ------ | -------------------------------------------------------- |
| type                       | slice 2     | 100%   |                                                          |
| enum                       | slice 2     | 100%   |                                                          |
| const                      | slice 2     | 100%   |                                                          |
| pattern                    | sota sweep  | 100%   | `\p{Letter}` rides the kernel's property tables           |
| minimum / maximum          | slice 2     | 100%   |                                                          |
| exclusiveMinimum / Maximum | slice 2     | 100%   |                                                          |
| multipleOf                 | this sweep  | 100%   |                                                          |
| minLength / maxLength      | slice 2     | 100%   |                                                          |
| minItems / maxItems        | slice 2     | 100%   |                                                          |
| uniqueItems                | slice 2     | 100%   |                                                          |
| contains / minContains / maxContains | this sweep | 100% | contains annotates the matching indices (the unevaluated model) |
| prefixItems / items        | slice 2     | 100%   |                                                          |
| properties                 | slice 2     | 100%   |                                                          |
| required                   | slice 2     | 100%   |                                                          |
| patternProperties          | this sweep  | 100%   | (was 96%: the `\p{...}` group now passes)                 |
| propertyNames              | this sweep  | 100%   | annotates nothing (a different instance location)         |
| minProperties / maxProperties | this sweep | 100% |                                                          |
| additionalProperties       | slice 2     | 100%   | excludes patternProperties matches, per 2020-12; its applied keys annotate |
| dependentRequired          | this sweep  | 100%   |                                                          |
| dependentSchemas           | this sweep  | 100%   | triggered schemas annotate their location                |
| if / then / else           | this sweep  | 100%   | a holding if annotates; a failed if's annotations drop   |
| allOf / anyOf / oneOf / not | slice 2    | 100%   | successful branches annotate; `not` never annotates outward |
| unevaluatedProperties      | sota sweep  | 100%   | the annotation model (`validate/jsonschema.c`)           |
| unevaluatedItems           | sota sweep  | 100%   | the annotation model                                     |
| $ref                       | sota sweep  | 99%    | full URI-reference resolution through the identifier map |
| $id / base-URI composition | sota sweep  | 100%   | RFC 3986 path merge over the crawl's resource map         |
| $anchor                    | sota sweep  | 100%   | plain-name fragments scoped to the nearest $id resource   |
| $dynamicRef / $dynamicAnchor | sota sweep | 89%    | the runtime dynamic scope (outermost matching resource); the HTTP-fixture groups carry (family 1) |
| boolean schemas            | slice 2     | 100%   |                                                          |
| contentMediaType / contentSchema | carried (annotation-only; ignoring them passes the whole file) | 100% | — |
| format                     | carried (annotation-only in 2020-12) | 100% | — |
| default                    | carried (annotation-only) | 100% | — |
| vocabulary negotiation     | carried     | 75%    | needs the external metaschema's `$vocabulary` (family 1) |

Skipped optional directories (not vendored): `optional/format/`
(format-assertion support), `bignum.json`, `non-bmp-regex.json`,
`cross-draft.json`, `float-overflow.json`, `dynamicRef.json` (optional),
`refOfUnknownKeyword.json`, `unknownKeyword.json`, `no-schema.json`,
`id.json`, `dependencies-compatibility.json` (draft-07 shim).

## Documented divergence families (the carries)

1. **The no-network-corpus stance (24 main cases).** The validator's
   reference contract resolves a reference only against the SCHEMA'S OWN
   identifier space — the resource map the compile-time crawl builds from
   the schema's `$id`/`$anchor`/`$dynamicAnchor` keywords. A reference whose
   canonical URI is not in that space is `DIAG_REF` at the instance, never a
   fetch: the face does no I/O, embeds no corpus, and cannot be made to
   dereference a URL by writing a schema. The suite's remaining
   remote-shaped cases all point OUTSIDE the schema document:
   - `refRemote.json` whole-file (16) — every schema references a document
     under `http://localhost:1234/...` served from the upstream `remotes/`
     tree, which this port does not vendor (a second corpus, not a test of
     the resolution machinery itself);
   - `dynamicRef.json` (5) — the `strict-tree`/`extendible`/`detached`
     groups `$ref` the remote documents `tree.json`,
     `extendible-dynamic-ref.json`, `detached-dynamicref.json` from the same
     tree. Their expected-INVALID siblings pass on the unresolvable-ref
     verdict and are NOT carried;
   - `ref.json` (1), `defs.json` (1) — the draft 2020-12 metaschema target
     (`https://json-schema.org/draft/2020-12/schema`, whose resolution needs
     the metaschema plus its six `meta/*` sibling resources);
   - `vocabulary.json` (1) — vocabulary negotiation requires reading the
     `$vocabulary` of the referenced metaschema, which is itself an external
     document.
   These are the only carries where the suite tests a capability (fetching an
   external resource) the face deliberately does not have. Vendoring the
   remote corpus would test it only against fixtures keyed to one host —
   the honest boundary is the validator's, not the corpus's.

2. **(Landed in this sweep — recorded for the ledger.)** The pattern
   kernel previously stepped BYTES with ASCII-only classes, which carried
   3 main + 18 optional cases: `\p{...}` property escapes read as Annex B
   identity escapes, and the named classes could not consume a whole
   multi-byte code point (so `$` never landed after one). The kernel now
   runs a position-aware Pike VM: code points are decoded once per step,
   bracket classes and dot match WHOLE code points (every byte of the
   point must sit in the byte bitmap — exactly the ECMA verdict for both
   positive and negated classes), `\s` is ECMA WhiteSpace plus
   LineTerminator, `\D`/`\W`/`\S` are code-point complements, and
   `\p{Letter}`/`\p{digit}` ride generated Unicode 13.0.0 range tables
   (regexp.c; the two canonical spellings only — any other `\p{...}` name
   stays the Annex B identity escape, pinned). Thread positions live in a
   five-slot ring (a code point consumes at most 4 bytes), deduped per
   (pc, position) with separate closure and ring marks; the budget
   constants are unchanged and every array stays fixed-size. Zero optional
   carries remain.

## Bugs the suite exposed in covered keywords (all fixed + pinned)

1. **uniqueItems was key-order dependent** — the canonical mapping hash
   folded pairs in document order, so `{"a":1,"b":2}` and `{"b":2,"a":1}`
   hashed apart and never reached the equality probe: the suite's
   `property order of array of objects is ignored` cases reported unique
   content that is duplicate. The mapping hash fold is now commutative
   (validate.c `yep_validate_node_hash`). Pinned in
   `test_validate.cpp:UniqueItemsIgnoresKeyOrder`.
2. **const/enum compared numbers tag-strictly** — INT vs FLOAT never matched,
   so `const: 0` rejected `0.0`, `const: 1.0` rejected `1`, and enum members
   `[1]` rejected `[1.0]` (suite const/enum groups `const with 0 does not
   match other zero-like types` etc.). Numbers now compare by value across
   the int/float tags (`yep_validate_node_equal`), which also makes the
   duplicate-key canonical form treat `1` and `1.0` as one key (spec
   3.2.1.3). Pinned in
   `test_validate.cpp:ConstEnumCompareNumbersAcrossIntAndFloatTags`.
3. **Unknown letter escapes were SyntaxError** — `\p` (and every
   unsupported letter escape) made the pattern invalid, and an invalid
   pattern is silently ignored, mis-validating. ECMA-262 Annex B (the
   non-unicode mode JSON Schema actually references) reads unknown letter
   escapes as identity escapes: `\p{Digit}` outside unicode mode is the
   literal text. Digits stay SyntaxError (backreferences stay out of the
   subset — an ignored pattern beats a misread one). Pinned in
   `test_validate.cpp:ValidateRegex.SearchSemantics`.
4. **dependentRequired's detail text read past the DOM allocation** —
   caught by the ASan CI leg ON this suite's run: the emitter composes
   details with printf, and the schema-side name view is not
   NUL-terminated, so `%s` walked past the buffer. Every emit in the sweep
   copies into a bounded buffer before composing; the suite and the pins
   run ASan-clean (`YEPTRIS_ENABLE_ASAN` build, same matrix numbers).

## The structured output face (#906 slice 5)

`output/schema.json` is the published JSON Schema 2020-12 output
meta-schema, fetched verbatim from
https://json-schema.org/draft/2020-12/output/schema (2026-11-03). It is
the referee for `yeptris_validate_report_write`'s bytes: the output
test validates the transform's output against it through our own
validator.

`output/spec-12-4-4-basic.json` is the byte-for-byte expectation derived
from the core specification's section 12.4.4 (Verbose) example: the
same schema and instance, flattened to the published BASIC shape the
flat report can faithfully produce — one root output unit plus one
error unit per diagnostic. The walk's `additionalProperties` law reports
the object's instance path and names the property in the error text
(the spec example's inner unit carries `/disallowedProp` directly); the
error wording is our own, as section 12.3.4 allows. DETAILED/VERBOSE
(the schema-structured hierarchies) are not derivable from a flat
report and are deliberately not faked.
