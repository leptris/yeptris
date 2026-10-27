# 19 — Python: the README's examples execute in CI (docs cannot rot)

## Why
The mirror of item 14: the Python README's snippets were verified
manually once — nothing keeps them runnable.

## Plan
- tests/test_readme.py: extract every ```python block, exec it in a
  namespace with nothing pre-defined (the README is the SSOT; the
  blocks must be self-sufficient)

## Acceptance
- breaking an example fails the suite; 56+N green

## Status: COMPLETE
Anti-rot proven by poisoning the extraction (TypeError caught);
58 tests green.
