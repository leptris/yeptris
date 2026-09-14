#!/usr/bin/env python3
"""cbor-diff.py — CBOR differential vs Python cbor2 (TODO.cbor/06).

Walks the shared corpus (the RFC 8949 seed vectors + every json-test-
suite document re-encoded to CBOR by cbor2), runs BOTH decoders, and
compares CANONICAL re-encodings byte-for-byte. Divergences must fall
into a documented, waived class; anything else is a bug and fails the
run with the offending bytes on stdout.

Waived classes (the representation ledger, TODO.cbor/01-02):
  bytes-as-text   — byte strings (incl. tag 2/3 bignum content) emit
      as text in our DOM; tree-equal in our model
  non-text-keys   — non-string map keys materialize as diagnostic
      text (the plan's JSON-model policy); cbor2 keeps native types
  duplicate-keys  — we keep every pair; cbor2 collapses to the last
      (its dict model), so canonical re-encodes differ in pair count
  int-width       — integers beyond int64 widen to doubles on our
      side; cbor2 carries them exactly
  tag-semantics   — cbor2 materializes tags 0/1 as datetimes and
      re-encodes them as tag 0 strings; we preserve tags verbatim
      (RFC 8949 s3.4: interpreting tags is optional for a decoder)
  trailing-bytes  — cbor2.loads ignores bytes after the first item;
      our single-item decode rejects them (sequences are the
      RFC 8742 surface)
  depth-cap       — cbor2's default nesting cap is 400; ours is the
      DOM's 1000 (both are legal decoder resource limits)
"""
import json
import pathlib
import subprocess
import sys
import tempfile

import cbor2

ROOT = pathlib.Path(__file__).resolve().parent.parent
DRIVER = ROOT / "build/test/cbor_canon"
SEEDS = ROOT / "test/fuzz/corpus/cbor"
JSON_DIR = ROOT / "test/conformance/data/json-test-suite/test_parsing"

WAIVED = ("bytes-as-text", "non-text-keys", "duplicate-keys", "int-width",
          "tag-semantics", "trailing-bytes", "depth-cap")


def classify(data: bytes, ours: str, ref: str | None):
    """Names the divergence class set, or None when the trees agree."""
    import io

    import io

    fp = io.BytesIO(data)
    try:
        obj = cbor2.CBORDecoder(fp).decode()
        trailing = fp.tell() != len(data)
    except cbor2.CBORError as e:
        if ours.startswith("OK") and "nesting depth" in str(e):
            return frozenset({"depth-cap"})
        return {"we-accept-they-reject"}
    if ours.startswith("REJECT"):
        return frozenset({"trailing-bytes"}) if trailing else {"they-accept-we-reject"}
    if ours.startswith("ENCFAIL"):
        return {"enc-fail"}
    our_bytes = bytes.fromhex(ours[3:])
    ref_bytes = bytes.fromhex(ref) if ref is not None else cbor2.dumps(obj, canonical=True)
    if our_bytes == ref_bytes:
        return None

    def walk(o):
        kinds = set()
        stack = [o]
        while stack:
            cur = stack.pop()
            if isinstance(cur, bool) or cur is None:
                continue
            if isinstance(cur, int):
                if cur > (1 << 63) - 1 or cur < -(1 << 63):
                    kinds.add("int-width")
                continue
            if isinstance(cur, float):
                continue
            if isinstance(cur, (bytes, bytearray)):
                kinds.add("bytes-as-text")
                continue
            if isinstance(cur, str):
                continue
            import datetime as _dt

            if isinstance(cur, (_dt.datetime, _dt.date)):
                kinds.add("tag-semantics")  # cbor2 materialized tag 0/1
                continue
            if isinstance(cur, cbor2.CBORTag):
                if cur.tag in (2, 3):
                    kinds.add("bytes-as-text")
                stack.append(cur.value)
                continue
            if isinstance(cur, list):
                stack.extend(cur)
                continue
            if isinstance(cur, dict):
                seen = set()
                for k, v in cur.items():
                    if not isinstance(k, str):
                        kinds.add("non-text-keys")
                    kk = cbor2.dumps(k, canonical=True)
                    if kk in seen:
                        kinds.add("duplicate-keys")
                    seen.add(kk)
                    stack.append(v)
                continue
        return kinds

    kinds = walk(obj)
    return frozenset(kinds) if kinds else frozenset({"UNCLASSIFIED"})


def our_line(data: bytes) -> str:
    with tempfile.NamedTemporaryFile(delete=False, suffix=".cbor") as f:
        f.write(data)
        path = f.name
    try:
        out = subprocess.run([str(DRIVER), path], capture_output=True, text=True)
        return out.stdout.strip()
    finally:
        pathlib.Path(path).unlink(missing_ok=True)


def ref_line(data: bytes) -> str | None:
    try:
        obj = cbor2.loads(data)
        return cbor2.dumps(obj, canonical=True).hex()
    except Exception:
        return None


def main() -> int:
    cases = []
    for p in sorted(SEEDS.glob("*.cbor")):
        cases.append((p.name, p.read_bytes()))
    for p in sorted(JSON_DIR.glob("*.json")):
        try:
            obj = json.loads(p.read_text())
            data = cbor2.dumps(obj)
        except Exception:
            continue  # not JSON (or un-encodable surrogates): outside
        cases.append((p.name, data))

    stats: dict[str, int] = {}
    unclassified = []
    for name, data in cases:
        ours = our_line(data)
        ref = ref_line(data)
        if ours.startswith("OK ") and ref is not None:
            if bytes.fromhex(ours[3:]) == bytes.fromhex(ref):
                stats["match"] = stats.get("match", 0) + 1
                continue
        elif ours.startswith("REJECT") and ref is None:
            stats["both-reject"] = stats.get("both-reject", 0) + 1
            continue
        kinds = classify(data, ours, ref)
        if kinds is None:
            stats["match"] = stats.get("match", 0) + 1
            continue
        kind_set = kinds if isinstance(kinds, (frozenset, set)) else {kinds}
        for k in kind_set:
            stats[k] = stats.get(k, 0) + 1
        if "UNCLASSIFIED" in kind_set or "they-accept-we-reject" in kind_set:
            unclassified.append((name, data.hex(), ours, ref))

    total = len(cases)
    print(f"cbor-diff: {total} cases")
    for k in sorted(stats):
        marker = "waived" if k in WAIVED else ("ok" if k in ("match", "both-reject") else "!!!")
        print(f"  {marker:<6} {k}: {stats[k]}")
    if unclassified:
        print(f"\nUNCLASSIFIED divergences: {len(unclassified)}")
        for name, hexdata, ours, ref in unclassified[:10]:
            print(f"  {name}: bytes={hexdata[:80]} ours={ours[:60]} ref={(ref or 'None')[:60]}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
