# TODO.impl/12 — Event delivery: pull, recorder, iterparse, libyaml-compat push

Status: phases A+B COMPLETE (push/pull/recorder/iterparse, cross-model 405/405); yaml_compat adapter + streaming feed remain · Depends: 07, 11 · Layer: `src/yeptris/events` · PLAN.md phase: 1

## Goal

The other consumption models over the one engine, each solving a different
payment problem — and the FFI lesson from leptris applied verbatim: bulk
recorder beats per-event callbacks by orders of magnitude through bindings.

## Deliverables

- `events/pull.{h,c}` — StAX-style: `yeptris_pull_next(pull) → const
  YepEvent*` (valid until the next call), `yeptris_pull_new{,_file}`;
  zero C→host callbacks; memory bounded by the input slice, not the
  document (leptris pull contract).
- `events/recorder.{h,c}` — fixed-size `YepEventRecord` ring +
  string arena: `yeptris_recorder_feed(rec, chunk, len, final)` then
  `yeptris_recorder_records(rec, &n)` + `yeptris_recorder_arena(rec, &len)`.
  Strings slice the arena by record offset+len — bulk drain per chunk,
  O(chunks) host crossings (leptris measured 20 calls for 41,806 events).
- `events/push.{h,c}` — callback push over the engine sink (the C-user
  convenience API); documented to be ~1 µs/event through FFI — bindings
  use the recorder instead.
- `events/iterparse.{h,c}` — bounded-memory iteration: per-document in a
  multi-doc stream, or per-top-level-node in a single doc (v1 contract:
  nodes are yielded complete with their subtree, released on advance —
  the leptris iterparse discipline; full-document post-order mode is a
  v2 extension if a consumer demands it).
- `events/yaml_compat.{h,c}` — the libyaml-shaped adapter: `yaml_event_t`
  /`yaml_event_type_t` mirroring libyaml's public `api.h` surface, driven
  by our engine (aliases/anchor props, style + plain_implicit/quoted_implicit
  flags, start/end marks with line/col). Scope decision (open question in
  PLAN.md §9): porting shim first; promote to public compat API only if
  17's drivers prove the shape complete.

## Design decisions

- All four are **sinks** over `parse/sink.h` — the engine has zero
  knowledge of them (OCP/MECE). Recorder and pull share the streaming
  feed machinery (`parse/feed.c`); iterparse rides pull; DOM (11) is a
  fifth sibling, not a parent.
- Event validity windows are contracts, stated per API (pull: until next
  call; recorder: until next feed; push: the call itself) — bindings
  depend on these; ABI-pinned.
- The compat adapter maps types 1:1; where libyaml distinguishes
  token-level facts we synthesize them from events (scanner-driver
  coverage in 17 documents the lossy spots).

## Phases

A. pull + push; contract tests (validity windows, error surfacing).
B. recorder + chunked drain tests (the 41,806-event benchmark shape,
  ≤ O(chunks) calls asserted).
C. iterparse + multi-doc streams; compat adapter skeleton.

## Acceptance

- Streaming a 100 MB multi-doc stream via iterparse stays under 2× the
  largest single document's RSS (the bounded-memory claim, measured).
- Recorder: 64 KiB chunks on a 10 MB document → < 200 host calls (asserted
  ceiling, not vibe).
- All four models produce identical event streams on the corpus
  (cross-model differential test — one engine, provably).

## References

- `~/src/leptris/leptris/src/leptris/sax/` (recorder + chunked delivery,
  issue #585 write-up in the README), `~/src/leptris/leptris-ruby/lib/leptris/xml/{pull,iterparse}.rb`,
  `~/src/external/libyaml/src/api.c` (event surface being adapted).
