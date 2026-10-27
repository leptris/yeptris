#!/usr/bin/env python3
"""gen-fuzz-corpus.py — deterministic fuzz seed corpus (the teptris
gen_bench_corpus pattern). Fixed seed: identical bytes on every run.
Shapes teach libFuzzer the grammar cheaply — block/flow structure,
anchors, every scalar style, the multibyte breaks and BOMs the engine
normalizes, and the document markers the chunk-cut scanner keys on —
so the nightly's mutations start from well-formed-ish YAML instead of
discovering brackets one byte at a time. Output is NOT checked into
the repo; the workflow regenerates it before each round."""
import os
import random
import sys

random.seed(42)
OUT = sys.argv[1] if len(sys.argv) > 1 else "fuzz-corpus"
os.makedirs(OUT, exist_ok=True)

WORDS = ("alpha bravo charlie delta echo foxtrot golf hotel india juliet "
         "kilo lima mike november oscar papa quebec romeo sierra tango").split()
UNI = ("héllo wörld 日本語テキスト emoji ↔ mix tabs   spaces "
       "dash-dash-dot   colon: ")


def write(name, text):
    with open(os.path.join(OUT, name), "w", encoding="utf-8") as f:
        f.write(text)


def rnd_scalar(r):
    return r.choice([
        WORDS[r.randrange(len(WORDS))],
        str(r.randrange(2**32)),
        "%g" % (r.random() * 10 ** r.randrange(-3, 6)),
        "0x%08x" % r.randrange(2**32),
        "true" if r.random() < 0.5 else "null",
        '"%s"' % WORDS[r.randrange(len(WORDS))],
        "'it''s'",
        UNI[: r.randrange(4, 20)],
    ])


def block_map(r, depth, budget):
    lines = []
    for k in range(r.randrange(1, 5)):
        if budget[0] <= 0 or depth > 4:
            lines.append("%s: %s" % (WORDS[k % len(WORDS)], rnd_scalar(r)))
            budget[0] -= 1
            continue
        roll = r.random()
        if roll < 0.35:
            lines.append("%s: %s" % (WORDS[k % len(WORDS)], rnd_scalar(r)))
        elif roll < 0.6:
            lines.append("%s:" % WORDS[k % len(WORDS)])
            for l in block_map(r, depth + 1, budget):
                lines.append("  " + l)
        elif roll < 0.8:
            lines.append("%s:" % WORDS[k % len(WORDS)])
            for l in block_seq(r, depth + 1, budget):
                lines.append("  " + l)
        else:
            lines.append("? [%s, %s]" % (rnd_scalar(r), rnd_scalar(r)))
            lines.append(": %s" % rnd_scalar(r))
        budget[0] -= 1
    return lines


def block_seq(r, depth, budget):
    lines = []
    for k in range(r.randrange(1, 5)):
        if budget[0] <= 0 or depth > 4 or r.random() < 0.5:
            lines.append("- %s" % rnd_scalar(r))
        else:
            lines.append("-")
            for l in block_map(r, depth + 1, budget):
                lines.append("  " + l)
        budget[0] -= 1
    return lines


def flow(r, depth):
    if depth > 3 or r.random() < 0.4:
        return rnd_scalar(r)
    n = r.randrange(1, 5)
    if r.random() < 0.5:
        return "[" + ", ".join(flow(r, depth + 1) for _ in range(n)) + "]"
    return "{" + ", ".join("%s: %s" % (rnd_scalar(r), flow(r, depth + 1))
                           for _ in range(n)) + "}"


def scalars(r):
    style = r.randrange(6)
    body = "\n".join(rnd_scalar(r) for _ in range(r.randrange(1, 4)))
    if style == 0:
        return "|-\n" + body
    if style == 1:
        return ">\n" + "\n".join("  " + l for l in body.split("\n"))
    if style == 2:
        return '"%s"' % body.replace("\\", "\\\\").replace('"', '\\"')[:40]
    return body


def anchors(r):
    a = "a%d" % r.randrange(9)
    roll = r.random()
    if roll < 0.4:
        return "%s: &%s %s\nnext: *%s" % (WORDS[0], a, rnd_scalar(r), a)
    if roll < 0.7:
        return "base: &%s\n  k: %s\nuse: *%s" % (a, rnd_scalar(r), a)
    return "list: &%s [%s]\ncopy: *%s" % (a, rnd_scalar(r), a)


shapes = {
    "block": lambda r: "\n".join(block_map(r, 0, [12])) + "\n",
    "seq": lambda r: "\n".join(block_seq(r, 0, [10])) + "\n",
    "flow": lambda r: "doc: %s\n" % flow(r, 0),
    "deep": lambda r: "".join("  " * d + "k%d:\n" % d for d in range(12))
                      + "  " * 12 + "leaf: 1\n",
    "scalars": lambda r: "lit: %s\n" % scalars(r),
    "anchors": lambda r: anchors(r) + "\n",
    "docs": lambda r: "---\n%s\n---\n%s\n...\n" % (
        "\n".join(block_map(r, 0, [4])), rnd_scalar(r)),
    "directives": lambda r: "%%YAML 1.2\n---\n%s\n" % rnd_scalar(r),
    "unicode": lambda r: "k: %s\nother: \U0001F600\n" % UNI,
    "json": lambda r: '{"k": [1, 2.5, "s", true, null], "n": {"m": 0}}\n',
    "cr": lambda r: ("a: 1\r\nb:\r\n  - x\r\n---\r\nc: 2\r\n"),
    "edge": lambda r: r.choice([
        "\xef\xbb\xbfk: v\n",
        ": v\nk:\n",
        "- \n-\n",
        "a: |\n---\n",
        "? k\n: {x: [y]}\n",
        "&a k: v\nb: *a\n",
        "!<tag> v\n",
        "k: >-\n  folded\n  text\n",
    ]),
}

for name, gen in shapes.items():
    r = random.Random(42 + list(shapes).index(name))
    for i in range(6):
        write("%s-%02d" % (name, i), gen(r))

print("gen-fuzz-corpus: %d files in %s" % (len(os.listdir(OUT)), OUT))
