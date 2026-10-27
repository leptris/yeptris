# 01 — Ruby input typing: no respond_to? at the input boundary

## Why
Five `yaml.read if yaml.respond_to?(:read)` duck-type probes
(document.rb ×2, materializer.rb, valueml.rb ×2) violate the typing
law: respond_to? hides type errors until the wrong object reaches
the parser. Input coercion is ONE concern and belongs in ONE place.

## Plan
- `Yeptris.read_input(yaml)` module_function in yeptris.rb (the
  parent namespace's file — no autoload needed): IO/StringIO read,
  String passes, anything else must be a String and gets to_s'd
  exactly as today.
- All five sites call it (DRY); the valueml twin pair shares it too.

## Acceptance
- grep finds zero respond_to?/send/instance_variable_ in lib/
- 139/139 specs; a spec pins IO, StringIO, String, Pathname-ish
  (to_s fallback) inputs
