# 11 — Python: PEP 561 py.typed; Ruby spec hygiene

## Why
The package ships return-type hints on its publics but no py.typed
marker — type checkers ignore the hints without it (PEP 561). And
one spec file (yaml_spec.rb) lacks the frozen_string_literal magic
comment (lib/ was swept; spec/ was not).

## Plan
- yeptris/py.typed (empty marker), included in the wheel via
  package-data; verify the built wheel contains it
- the magic comment on spec/yaml_spec.rb

## Acceptance
- unzip -l the wheel shows yeptris/py.typed
- 56/56; 143/143

## Status: COMPLETE (wheel 0.1.10.1; the spec fix merged without a gem release — gems ship lib/ only)
py.typed in the wheel (verified by unzip); 56/56; 143/143.
