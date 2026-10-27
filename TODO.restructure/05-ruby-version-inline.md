# 05 — Ruby: the VERSION constant inlines into the parent's file

## Why
`require "yeptris/version"` in yeptris.rb is the library's LAST
internal require (the law: internal code loads via autoload or lives
in the parent namespace's file). VERSION is one line.

## Plan
- Define VERSION in yeptris.rb; retire version.rb via git rm.
- The gemspec loads it through the library require (verify).

## Acceptance
- Zero internal requires in lib/ (stdlib/gem requires only)
- 143/143; gem build unaffected

## Status: COMPLETE (gem 0.1.9.1)
VERSION inlined (version.rb retired); 143/143; gem builds clean.
