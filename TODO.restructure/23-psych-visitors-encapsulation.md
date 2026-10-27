# 23 — Psych visitors: purge respond_to? / instance_variable_* 

Status: complete

## Why

Global law: never `respond_to?`, never `instance_variable_get` /
`instance_variable_set`, never `send` to private methods. The Psych
compat visitors (`lib/yeptris/psych/visitors.rb`) still use all three
patterns for the `encode_with` / generic-object dump path — copied
from Psych's own reflection style, which our law forbids.

## Plan

1. Replace `respond_to?(:encode_with)` with a typed protocol:
   `Yeptris::Psych::Encodable` module. Objects that implement the
   Psych coder protocol `include` it (or we detect via
   `obj.is_a?(Encodable)` after an explicit registry). For the
   default dump of unknown objects that Psych would ivar-reflect,
   require they either be Encodable or fall through to a
   `Dumper.for(obj.class)` registry (OCP: extension = registration).

2. Replace `instance_variable_get/set` loops with the Encodable
   coder path only — generic ivar reflection goes away. The
   `psych_objects_spec` round-trips already use `encode_with` /
   `init_with`; plain ivar objects get a thin `Encodable` adapter
   in the spec helper if needed, not a law violation in library
   code.

3. No `send`. Public methods only.

## Acceptance

- `grep -R 'respond_to?\|instance_variable_\|send(' lib/` empty
  of violations (comments may mention the law).
- `psych_objects_spec` + full suite green.
- Encodable protocol documented in the Psych README section.
