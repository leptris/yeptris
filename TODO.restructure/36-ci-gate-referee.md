# 36 — The CI gate: the referee enforces the win

Status: complete

## What shipped

- benchmark/json_profile.rb fails (exit 1) when GATE is set and the
  mean ratio exceeds it.
- ci.yml json-profile job: the per-arch DEFAULT combination runs
  GATED at 1.05 on ubuntu + macos; off-default combos run ungated as
  evidence; every run prints the full distribution and the
  gc-footprint (pages/minors) per parser.
- Round 3 verdict (gated): ubuntu 0.961×, macos 0.718× (h2h 173/200)
  — both green.

## Gotchas recorded

- A matrix with BOTH top-level keys and include entries MERGES the
  entries into base combos instead of adding rows (2 of 5 jobs ran)
  — include-only matrices, ruby rides every row.
- ENV["GATE"]="" is truthy in Ruby — empty means unset.
