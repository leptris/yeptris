# FFI binding notes (TODO.impl/15)

How the Ruby binding talks to libyeptris — the rules that keep the
binding thin, safe, and fast. Written for the next binding (the C++
header consumers, or a hypothetical Rust one).

## The binding's one contract: O(chunks), not O(calls)

The FFI tax is the crossing, not the work. The binding's hot paths
are bulk-shaped by design:

- **Loading** has an adoption ladder; take the highest rung the
  loaded library offers (feature-detect the symbol, fall back):
  1. **Recorder** (always present): two reads (record array + string
     arena), then a host-side walk. Never call per-event/per-node
     accessors in a load loop — measured 2-3 FFI calls per node on
     the phase-A DOM path.
  2. **Value drain** (`yeptris_value_drain`, v0.1.1+): typed
     24-byte records — the C side pre-converts scalars through the
     number kernels; the host never re-parses text. `is_key` carries
     the pair alternation; anchors decorate the following value.
  3. **Columnar drain** (`yeptris_value_drain_columns`, v0.1.2+):
     the same stream as parallel typed buffers (payloads/offs/lens/
     kinds/tags/is_keys/bools) carved from ONE allocation — each
     column unpacks in a single host call, and int/float scalars
     arrive pre-converted. The Ruby and Python bindings' fast path;
     column i is byte-faithful to record i (equivalence-pinned).
- **Typing is C's verdict**: `YeptrisEventRecord.tag_id` (the pad
  byte; `sizeof` stays 36) carries the resolver's answer. The host
  converts (`Kernel#Integer`/`Float`) but never re-derives grammar.
  Psych-quirk overrides live in ONE place (`Materializer.scan_by_tag`)
  with a comment each.
- **Dumping** = `yeptris_document_build` (v0.1.1): a flat entry
  array (12 bytes/op-style-off-len, ABI-pinned) plus one string
  blob — ONE call raises the tree, one `serialize` emits. The host
  decides a string's plain-safety (the reading schema is the host's
  contract; a "12" that must stay a string takes a quoted style —
  round-trip by construction). The per-node builder
  (`node_new_*`, `map_add`/`seq_add`, `set_root`) remains the
  MUTATION-flow tool, never the bulk dump path.

## Identity and lifetime

- **Query handles are transient**; node IDS are stable
  (`yeptris_node_id`). Key every wrapper cache on the id — an
  address-keyed cache silently never hits (each query allocates a
  fresh handle from the thread-safe handle arena).
- **The document is the sole owner.** One `yeptris_document_free`
  releases everything ever handed out from it. Bindings layer a
  liveness flag (explicit free + GC finalizer over the SAME flag —
  two flags is a double free) and raise on use-after-free instead of
  ever touching freed memory.
- **Read-only sharing across threads is safe** (mutex-guarded handle
  arena; parse pools are single-threaded by contract): one document
  parsed, many threads querying.

## Owned pointers and errors

- `yeptris_serialize*` return malloc'd buffers — read once, free
  once (libc `free`). One seam function per binding, never per call
  site.
- Failures carry detail on the thread-local error channel
  (`yeptris_last_error`): message + line/column. Attach it to every
  exception the binding raises.
- Status enums are pinned (see ABI.md): the binding hard-codes the
  constants it needs — no C headers at runtime.

## Psych compatibility notes

The bar is Psych's observed behavior (verified case-by-case, see
`spec/psych/` and the Materializer): `yes`/`no`/`on`/`off` are
booleans but single-char `y`/`n` are NOT; floats need a dot and a
signed exponent (`1e3` is a String); octal is `017`-shaped (`0o17`
is YAML 1.2 only); sexagesimal follows Psych's `(e-2).abs` formula,
not yaml.org's; space-form timestamps normalize to iso8601 before
`Time.xmlschema`.
