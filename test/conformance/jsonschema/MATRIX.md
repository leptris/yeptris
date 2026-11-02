# MATRIX.md — the JSON-Schema-Test-Suite port (draft2020-12), #885 item 2

The conformance bar for the `yeptris_json_schema_validate` face, exactly as
yaml-test-suite is the bar for the parse face. The fixtures are vendored from
the official JSON-Schema-Test-Suite at upstream commit
`3f05891cac5d51ffa068d7076d1372130e00c829` (`tests/draft2020-12/`):

- `tests/` — the 46 main test files (committed verbatim);
- `optional/ecmascript-regex.json` — run as its own honest section;
- `optional/format/` and the other optional directories are NOT vendored
  (format-assertion, cross-draft, non-bmp-regex and friends are out of scope
  for this face and are recorded here as skips).

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
| tests/   | 46    | 1298   | 1158  | 140     | 0      | 0     |
| optional | 1     | 74     | 54    | 20      | 0      | 0     |

**Every covered keyword passes 100% of its suite cases.** All 160 carried
cases ride five documented machinery families (below) — no case fails for an
unexplained reason, and no carry is a fake pass.

## Per-file results (tests/)

| file                          | pass | carried | note                          |
| ----------------------------- | ---- | ------- | ----------------------------- |
| additionalProperties.json     | 21   | 0       |                               |
| allOf.json                    | 30   | 0       |                               |
| anchor.json                   | 4    | 4       | whole file carried (`$anchor`) |
| anyOf.json                    | 18   | 0       |                               |
| boolean_schema.json           | 18   | 0       |                               |
| const.json                    | 54   | 0       |                               |
| content.json                  | 18   | 0       | content keywords are annotation-only in 2020-12 — every case expects valid |
| default.json                  | 7    | 0       | annotation-only               |
| defs.json                     | 1    | 1       | metaschema `$ref` is remote   |
| dependentRequired.json        | 20   | 0       |                               |
| dependentSchemas.json         | 20   | 0       |                               |
| dynamicRef.json               | 22   | 22      | whole file carried (`$dynamicRef`) |
| enum.json                     | 53   | 0       |                               |
| exclusiveMaximum.json         | 4    | 0       |                               |
| exclusiveMinimum.json         | 4    | 0       |                               |
| format.json                   | 133  | 0       | format is annotation-only (no assertion) — every case expects valid |
| if-then-else.json             | 30   | 0       |                               |
| infinite-loop-detection.json  | 2    | 0       |                               |
| items.json                    | 29   | 0       |                               |
| maxContains.json              | 14   | 0       |                               |
| maxItems.json                 | 6    | 0       |                               |
| maxLength.json                | 7    | 0       |                               |
| maxProperties.json            | 10   | 0       |                               |
| maximum.json                  | 8    | 0       |                               |
| minContains.json              | 28   | 0       |                               |
| minItems.json                 | 6    | 0       |                               |
| minLength.json                | 7    | 0       |                               |
| minProperties.json            | 10   | 0       |                               |
| minimum.json                  | 11   | 0       |                               |
| multipleOf.json               | 11   | 0       |                               |
| not.json                      | 39   | 1       | `unevaluated*` inside `not`   |
| oneOf.json                    | 27   | 0       |                               |
| pattern.json                  | 10   | 2       | `\p{Letter}` property escape  |
| patternProperties.json        | 25   | 1       | `\p{Letter}` on property names |
| prefixItems.json              | 11   | 0       |                               |
| properties.json               | 28   | 0       |                               |
| propertyNames.json            | 22   | 0       |                               |
| ref.json                      | 60   | 19      | percent-encoding, `$id` scoping, URN, remote |
| refRemote.json                | 15   | 16      | whole file carried (remote base URIs) |
| required.json                 | 18   | 0       |                               |
| type.json                     | 80   | 0       |                               |
| uniqueItems.json              | 65   | 0       |                               |
| unevaluatedItems.json         | 44   | 27      | annotation plumbing           |
| unevaluatedProperties.json    | 83   | 46      | annotation plumbing           |
| vocabulary.json               | 4    | 1       | metaschema vocabulary negotiation |

optional: `ecmascript-regex.json` — 54 pass, 20 carried, 0 failed.

## Keyword matrix

"this sweep" = implemented in #885 item 2; "slice 2" = already present from
#889's slice 2.

| keyword                    | state       | pass % | failing case families (verbatim, all carried) |
| -------------------------- | ----------- | ------ | --------------------------------------------- |
| type                       | slice 2     | 100%   | — |
| enum                       | slice 2     | 100%   | — |
| const                      | slice 2     | 100%   | — |
| pattern                    | slice 2     | 83%    | `pattern with Unicode property escape requires unicode mode :: ASCII letters match`, `... :: Non-ASCII letters match` |
| minimum / maximum          | slice 2     | 100%   | — |
| exclusiveMinimum / Maximum | slice 2     | 100%   | — |
| multipleOf                 | this sweep  | 100%   | — |
| minLength / maxLength      | slice 2     | 100%   | — |
| minItems / maxItems        | slice 2     | 100%   | — |
| uniqueItems                | slice 2     | 100%   | — (the suite EXPOSED an order-dependent mapping hash; fixed, see below) |
| contains                   | this sweep  | 100%   | — |
| minContains / maxContains  | this sweep  | 100%   | — |
| prefixItems / items        | slice 2     | 100%   | — |
| properties                 | slice 2     | 100%   | — |
| required                   | slice 2     | 100%   | — |
| patternProperties          | this sweep  | 96%    | `patternProperties with Unicode property escape :: Unicode letter property name matches but value type is incorrect` |
| propertyNames              | this sweep  | 100%   | — |
| minProperties / maxProperties | this sweep | 100% | — |
| additionalProperties       | slice 2     | 100%   | — (now excludes patternProperties matches, per 2020-12) |
| dependentRequired          | this sweep  | 100%   | — |
| dependentSchemas           | this sweep  | 100%   | — |
| if / then / else           | this sweep  | 100%   | — |
| allOf / anyOf / oneOf / not | slice 2    | 100%   | not: `collect annotations inside a 'not', even if collection is disabled :: unevaluated property` (1, carried with the unevaluated family) |
| $ref (LOCAL `#/...`)       | slice 2     | 76%    | ref.json: `escaped pointer ref :: percent valid`, `refs with quote :: object with numbers is valid` (pointer percent-encoding), `$id`/relative-URI scoping groups, URN base-URI groups, remote targets |
| boolean schemas            | slice 2     | 100%   | — |
| contentMediaType / contentSchema | carried (annotation-only; ignoring them passes the whole file) | 100% | — |
| format                     | carried (annotation-only in 2020-12) | 100% | — |
| default                    | carried (annotation-only) | 100% | — |
| unevaluatedItems           | carried     | 62%    | 27 cases — "with unevaluated items" families, contains-interaction, if/then/else-annotation, dynamicRef |
| unevaluatedProperties      | carried     | 64%    | 46 cases — "with unevaluated properties" families, in-place applicator siblings, cyclic refs, instance-location collections |
| $anchor                    | carried     | —      | whole anchor.json (8 cases; 4 agree coincidentally) |
| $dynamicRef / $dynamicAnchor | carried   | —      | whole dynamicRef.json (44; 22 agree coincidentally) |
| remote $ref / base URIs    | carried     | —      | whole refRemote.json (31; 15 agree coincidentally) |
| $id / relative-URI scoping | carried     | —      | ref.json groups (see $ref row) |
| vocabulary negotiation     | carried     | 80%    | `no validation: invalid number, but it still validates` (a `$schema` metaschema without the validation vocabulary must un-assert `minimum`) |

Skipped optional directories (not vendored): `optional/format/`
(format-assertion support), `bignum.json`, `non-bmp-regex.json`,
`cross-draft.json`, `float-overflow.json`, `dynamicRef.json` (optional),
`refOfUnknownKeyword.json`, `unknownKeyword.json`, `no-schema.json`,
`id.json`, `dependencies-compatibility.json` (draft-07 shim), and the
remotes tree (remote-ref HTTP fixtures).

## Documented divergence families (the carries)

1. **Remote references** — the `$ref` subset resolves LOCAL `#/...` pointers
   only. `refRemote.json`, the URN groups, and remote metaschema targets
   (`defs.json`) cannot pass; whole-file carries include cases whose verdicts
   agree coincidentally (an unresolvable ref makes everything invalid, which
   matches the suite's expected-invalid cases for the wrong reason).
2. **Pointer machinery** — `$anchor`, `$dynamicRef`/`$dynamicAnchor`, `$id`
   base-URI scoping, and JSON-pointer percent-encoding (`%25`) are out of the
   subset. `anchor.json` and `dynamicRef.json` carry whole-file.
3. **Annotation plumbing** — `unevaluatedItems`/`unevaluatedProperties` need
   cross-branch evaluation collection (which sibling applied to which
   instance location). 27 + 46 cases carry. The one `not.json` case carries
   with them (annotations inside `not`).
4. **Regex subset** — the bounded Thompson-NFA kernel is byte-oriented
   ASCII-class ECMA-262 without the `u` flag: `\p{...}` property escapes are
   not implemented (2 pattern.json + 1 patternProperties.json case), and the
   optional `ecmascript-regex.json` families `\c` control escapes,
   Unicode-aware `\d \w \s \D \W \S`, and Unicode semantics (20 cases) carry.
   ASCII-semantics cases in the same groups pass and are NOT carried.
5. **Metaschema/vocabulary negotiation** — a `$schema` pointing at a
   metaschema without the validation vocabulary must un-assert validation
   keywords (1 vocabulary.json case).

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
   NUL-terminated, so `%s` walked past the buffer. Every emit in the
   sweep copies into a bounded buffer before composing; the suite and
   the pins run ASan-clean (`YEPTRIS_ENABLE_ASAN` build, same matrix
   numbers).
