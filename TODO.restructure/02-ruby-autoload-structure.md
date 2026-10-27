# 02 — Ruby autoload structure: no internal requires

## Why
yeptris.rb uses require_relative for the eager error hierarchy (the
nested-constant autoload gap) and psych.rb REQUIRES its own children
(handler/parser/coder_shim/visitors) plus a redundant self-require
of the library root. The law: internal code loads via autoload
declared in the IMMEDIATE parent namespace's file.

## Plan
- Move the error hierarchy INTO yeptris.rb (it is the parent
  namespace's file; eager by construction, no require at all).
  Delete error.rb.
- psych.rb declares `autoload :Handler/:Parser/:CoderShim/:Visitors`
  for its own namespace; drop its five requires. yeptris.rb gains
  `autoload :Psych, "yeptris/psych"`.
- ffi.rb's eager require stays (external gem + fail-fast contract,
  documented ordering with the manifest).

## Acceptance
- grep: no require_relative, no `require "yeptris/..."` inside lib/
  (the gem root's own require entry is the user's business)
- 139/139; `require "yeptris/psych"` still works standalone
