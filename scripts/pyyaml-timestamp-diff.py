#!/usr/bin/env python3
"""pyyaml-timestamp-diff.py — regenerate the FULL-family timestamp
differential corpus (#906 slice 3).

The FULL 1.2 family's timestamp surface is classified by PyYAML's
timestamp regex (the de-facto "full schema" behavior: libyaml has no
timestamp type, PyYAML's SafeLoader resolves them). The unit pins in
test/unit/test_resolve.cpp (TEST(Full12, PyYamlTimestampDifferential))
carry this corpus's verdicts; re-run this script with any PyYAML 6.x to
re-derive them after a corpus edit.

Usage: python3 scripts/pyyaml-timestamp-diff.py [--full]

    --full   include entries whose plain-scalar SHAPE a YAML scanner
             changes or rejects before resolution (leading/trailing
             spaces, tabs, a trailing ':' that starts a mapping); the
             pinned corpus excludes those and documents the edges.

Not a CI dependency: PyYAML is only the regeneration oracle.
"""

import sys

import yaml

# The bounded corpus: the spec 2.22 forms, every production edge of
# PyYAML's timestamp_regexp, the range questions, and near-miss rejects.
CORPUS = [
    # spec 2.22
    "2001-12-15T02:59:43.1Z",
    "2001-12-14t21:59:43.10-05:00",
    "2001-12-14 21:59:43.10 -5",
    "2002-12-14",
    # date-only width
    "2001-1-1",
    "2001-01-1",
    "2001-1-01",
    "2001-12-14x",
    # time separator / hour width
    "2001-01-01T1:00:00",
    "2001-01-01t1:00:00",
    "2001-01-01  1:00:00",
    "2001-01-01 1:00:00",
    "2001-01-01T01:00:00",
    "2001-01-01T001:00:00",
    "2001-01-01T01:0:00",
    "2001-01-01T01:00:0",
    "2001-01-01T01:00",
    "2001-01-01T01:00:00:00",
    # fraction
    "2001-01-01T01:00:00.",
    "2001-01-01T01:00:00.5",
    "2001-01-01T01:00:00.123456789",
    "2001-01-01T01:00:00,5",
    "2001-01-01T01:00:00.5.5",
    # timezone
    "2001-01-01T01:00:00Z",
    "2001-01-01T01:00:00z",
    "2001-01-01T01:00:00+5",
    "2001-01-01T01:00:00-5",
    "2001-01-01T01:00:00+05",
    "2001-01-01T01:00:00+05:30",
    "2001-01-01T01:00:00-05:00",
    "2001-01-01T01:00:00+0530",
    "2001-01-01T01:00:00 +5",
    "2001-01-01T01:00:00    Z",
    "2001-01-01T01:00:00 Z",
    "2001-01-01T01:00:00+05:30x",
    # year shape / sign
    "-2001-12-14",
    "+2001-12-14",
    "20011-12-14",
    "201-12-14",
    # range: resolution is not calendar validation (the oracle's own
    # resolver tags these; only its constructor raises)
    "2001-13-14",
    "2001-00-00",
    "2001-01-01T25:99:99",
    "2001-01-32",
    "2001-12-14 24:00:00",
    # in-range shapes
    "2001-12-14 21:59:43",
    "2001-12-14 21:59:43.0 -05:00",
    "0000-01-01",
    "9999-12-31",
    "2001-01-01T00:00:00-00:00",
    "2001-01-01T00:00:00+00:00",
    "2002-12-14T00:00:00Z extra",
]

# Shapes a YAML scanner normalizes or rejects before resolution ever
# sees the bytes (kept out of the C pins, reported under --full).
SCANNER_EDGES = [
    "2001-12-14 ",  # trailing space: the scanner strips it
    " 2001-12-14",  # leading space: stripped
    "2001-01-01\t1:00:00",  # PyYAML's scanner rejects the tab
    "2001-01-01T01:00:00+05:",  # the trailing ':' starts a mapping
]


def verdict(text):
    """The de-facto full verdict: the composed scalar's tag."""
    try:
        node = yaml.compose(text)
    except yaml.YAMLError:
        return "PARSE_ERROR"
    if node is None:
        return "null"
    return node.tag.rsplit(":", 1)[-1]


def main(argv):
    show_edges = "--full" in argv
    corpus = CORPUS + (SCANNER_EDGES if show_edges else [])
    for text in corpus:
        print('{"%s", YEPTRIS_TAG_%s},' % (text.replace("\\", "\\\\").replace("\t", "\\t"),
                                           verdict(text).upper()))


if __name__ == "__main__":
    main(sys.argv[1:])
