# 08 — Wave-3 audit: the send/ivar-get residue

## Findings
1. `Object.send(:remove_const, :Psych)` — FIXED: `class_eval`
   reaches Module-private methods without send (remove_const has no
   public form; the rebind is the namespace's purpose).
2. `instance_variable_get` on USER OBJECTS (visitors, 2 sites) —
   DOCUMENTED EXCEPTION: Ruby's serialization seam (Marshal itself
   reads/writes ivars this way); the read side of the init_with
   restore exception. There is no public route.
3. `define_method` in handler.rb (the Recorder) — clean
   metaprogramming (20 event methods, DRY); not on the banned list.
4. Python: only the ctypes hasattr capability-detect (documented).

## Status: COMPLETE (gem 0.1.9.2)
143/143. The lib/ is now clean of every banned construct except the
four documented protocol exceptions (encode_with/init_with probes,
user-object ivar get/restore, ctypes capability detect) — each
mirroring the reference implementation's own public mechanics.
