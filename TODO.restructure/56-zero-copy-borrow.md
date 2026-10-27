# 56 — the zero-copy borrow fix: input_base BEFORE the run

Status: LANDED (this wave)

## Why

parse.c set `dom->input_base` AFTER `yep_engine_run`: during the
whole parse, dom_str_in's borrow branch was dead and EVERY scalar
value, key, and anchor name was arena-copied — while the
differential gates (which set input_base before running) tested the
borrowed configuration all along. The document already keeps
`transcoded` alive precisely so input views stay valid; the ordering
simply contradicted the design.

## What

Two lines move above the run (input_len + input_base). Local h2h
sanity moved every shape: flow-json 1.33x, flow-single +50%
absolute (the giant span's strings no longer copy), scalar-heavy
0.68 → ~0.8, wide ~0.85. Folded/escaped/finish-pool strings still
copy (necessary); borrowed ones are input views.

## Gates

279/279 ctest, both differentials, validate, format — unchanged
semantics, only the string ENCODING during parse (the gates already
compared under borrowing).
