#!/usr/bin/env python3
"""Turn a shakespeare.mit.edu long-poem page (Venus and Adonis, The Rape of Lucrece,
A Lover's Complaint) into plain-text stanzas separated by blank lines, ready for
`splc --scan-text -frhyme-scheme=...`.

usage: poem_to_stanzas.py POEM.html [--lines N] > stanzas.txt

Each <BLOCKQUOTE> on the page is a stanza; blocks with fewer than N lines (default 6,
which drops the Latin epigraph and the dedication) are skipped.
"""
import html, re, sys

def main(argv):
    n = 6
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--lines": n = int(argv[i + 1]); i += 2
        else: files.append(argv[i]); i += 1
    if not files:
        print(__doc__); return 2
    blocks = []
    for path in files:
        text = open(path, encoding="latin-1").read()
        for m in re.finditer(r"<BLOCKQUOTE>(.*?)</BLOCKQUOTE>", text, re.S | re.I):
            lines = [html.unescape(re.sub(r"<[^>]+>", "", l)).strip() for l in m.group(1).splitlines()]
            lines = [l for l in lines if l]
            if len(lines) >= n: blocks.append(lines)
    # The site occasionally runs two stanzas together in one block: split any block
    # that is an exact multiple of the poem's usual stanza length.
    usual = max(set(len(b) for b in blocks), key=lambda k: sum(1 for b in blocks if len(b) == k))
    stanzas = 0
    for b in blocks:
        parts = [b[i:i + usual] for i in range(0, len(b), usual)] if len(b) % usual == 0 else [b]
        for st in parts:
            print("\n".join(st) + "\n")
            stanzas += 1
    print(f"{stanzas} stanzas of {usual} lines", file=sys.stderr)
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
