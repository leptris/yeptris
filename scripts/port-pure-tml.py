#!/usr/bin/env python3
"""port-pure-tml.py — psych-pure .tml corpora -> conformance fixtures.

Converts the parse-*.tml spec corpora from psych-pure (the YAML 1.2
pure-Ruby engine; see ~/src/external/psych-pure/spec) into the
yaml-test-suite frontmatter shape the conformance runner already
reads (16's machinery — one converter, no new harness).

.tml semantics handled:
  --- yaml            block value (common-indent dedent)
  --- yaml(<|-)      same block form (testml leading/trailing markers)
  --- yaml: value     inline YAML scalar (quoted forms decoded)
  --- events          the expected event stream (suite notation)
  --- ^events         inherit the previous case's events
  --- SKIP            case is not run upstream; skipped here too
The (<- ) marker (no final newline) is unrepresentable in the suite
format (its inputs always end with a newline) — those cases are
skipped and counted, not silently dropped.

Usage: port-pure-tml.py <psych-pure-spec-dir> <out-dir>
"""

import re
import sys
from pathlib import Path

# ids are 4 chars: the suite loader's shape (16's harness reads
# exactly <id>.yaml with a 4-char id — the same pin as upstream)
PREFIX = {
    "parse-block.tml": "pb",
    "parse-flow.tml": "pf",
    "parse-scalar.tml": "ps",
    "parse-props.tml": "pp",
    "parse-stream.tml": "pt",
}


def dedent_block(lines):
    indents = [len(l) - len(l.lstrip(" ")) for l in lines if l.strip()]
    if not indents:
        return ""
    cut = min(indents)
    out = "\n".join(l[cut:] if len(l) >= cut else "" for l in lines)
    return out


OPENS = ("+STR", "+DOC", "+SEQ", "+MAP")
CLOSES = ("-STR", "-DOC", "-SEQ", "-MAP")


def indent_events(lines):
    """The .tml events are flat; the suite tree format indents by
    nesting depth (openers print at depth then enter, closers leave
    then print, =VAL/=ALI are leaves at depth)."""
    out = []
    depth = 0
    for l in lines:
        if l.startswith(OPENS):
            out.append(" " * depth + l)
            depth += 1
        elif l.startswith(CLOSES):
            depth -= 1
            out.append(" " * depth + l)
        else:
            out.append(" " * depth + l)
    return out


def decode_inline(v):
    if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
        body = v[1:-1]
        return (
            body.replace("\\n", "\n")
            .replace("\\t", "\t")
            .replace('\\"', '"')
            .replace("\\\\", "\\")
        )
    if len(v) >= 2 and v[0] == "'" and v[-1] == "'":
        return v[1:-1].replace("''", "'")
    return v


def parse_tml(text):
    """Yields dicts: name, yaml (str|None), events (list|None), skip."""
    cases = []
    cur = None
    field = None  # (key, lines, inline)
    prev_events = None

    def close_field():
        nonlocal field, cur, prev_events
        if field is None or cur is None:
            field = None
            return
        key, lines, inline = field
        if key == "yaml":
            cur["yaml"] = decode_inline(inline) if inline is not None else dedent_block(lines)
        elif key == "events":
            ev = [l for l in dedent_block(lines).split("\n")]
            cur["events"] = [l for l in ev if l != ""]
            prev_events = cur["events"]
        elif key == "SKIP":
            cur["skip"] = True
        field = None

    for line in text.split("\n"):
        if line.startswith("=== "):
            close_field()
            if cur is not None:
                cases.append(cur)
            cur = {"name": line[4:].strip(), "yaml": None, "events": None, "skip": False}
            continue
        if line.startswith("--- ") and cur is not None:
            close_field()
            rest = line[4:]
            if rest.startswith("^"):
                field = None
                cur["events"] = prev_events
                continue
            m = re.match(r"^([A-Za-z-]+)(\([^)]*\))?\s*(?::(.*))?$", rest)
            if not m:
                continue
            key = m.group(1)
            inline = m.group(3).strip() if m.group(3) is not None else None
            if key == "SKIP":
                cur["skip"] = True
                field = None
                continue
            if inline is not None:
                cur[{"yaml": "yaml"}.get(key, key)] = decode_inline(inline)
                field = None
                continue
            field = (key, [], None)
            continue
        if line.startswith("%") or line.startswith("  '"):
            continue  # directives / assertion code
        if field is not None:
            field[1].append(line)
    close_field()
    if cur is not None:
        cases.append(cur)
    return cases


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    total = skipped = 0
    for fname, prefix in sorted(PREFIX.items()):
        path = src / fname
        if not path.exists():
            print(f"missing {path}", file=sys.stderr)
            continue
        text = path.read_text(encoding="utf-8")
        # (<-): no-final-newline inputs — unrepresentable upstream too
        no_final_nl = text.count("(<-)")
        n = 0
        for case in parse_tml(text):
            if case["skip"] or case["yaml"] is None or not case["events"]:
                skipped += 1
                continue
            n += 1
            cid = f"{prefix}{n:02d}"
            body = ["---", f"- name: {case['name']} (psych-pure {fname})",
                    "  yaml: |"]
            body += [f"    {l}" for l in case["yaml"].split("\n")]
            body += ["  tree: |"]
            body += [f"    {l}" for l in indent_events(case["events"])]
            (out / f"{cid}.yaml").write_text("\n".join(body) + "\n", encoding="utf-8")
            total += 1
        print(f"{fname}: {n} cases"
              + (f" ({no_final_nl} no-final-newline skipped)" if no_final_nl else ""))
    print(f"total: {total} cases, {skipped} skipped")
    return 0


if __name__ == "__main__":
    sys.exit(main())
