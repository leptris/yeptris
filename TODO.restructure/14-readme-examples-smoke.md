# 14 — Ruby: the README's examples become a spec (docs cannot rot)

## Why
Wave 8's README defects included a RUNNABLE-BROKEN example — found
only by manually executing the snippets. Inspection rots; execution
is the only trustworthy check.

## Plan
- spec/readme_spec.rb: extract every [source,ruby] block from
  README.adoc and eval it in a fixture context (config_yaml defined;
  output assertions only where the README states them)
- the example blocks stay the SSOT — the spec copies nothing

## Acceptance
- dropping a broken example into the README FAILS the suite
- 143 + N specs green

## Status: COMPLETE (merged to main)
Anti-rot PROVEN: a broken example fails the suite; restored, 144/144.
