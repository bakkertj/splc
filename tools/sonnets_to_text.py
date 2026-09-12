#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
"""Convert the shakespeare.mit.edu sonnet pages to plain text.

usage: sonnets_to_text.py <dir with sonnet.*.html> <outdir>
writes  <outdir>/sonnets.txt        all 154 sonnets, numbered, blank-line separated
        <outdir>/sonnets_lines.txt  verse lines only, one per line (for `splc --scan-text`)
"""
import glob, html, os, re, sys

def roman_to_int(s):
    v = {"I": 1, "V": 5, "X": 10, "L": 50, "C": 100, "D": 500, "M": 1000}
    total, prev = 0, 0
    for ch in reversed(s):
        n = v[ch]
        total += -n if n < prev else n
        prev = max(prev, n)
    return total

def main(src, out):
    sonnets = []
    for path in glob.glob(os.path.join(src, "sonnet.*.html")):
        numeral = os.path.basename(path).split(".")[1]
        if not re.fullmatch(r"[IVXLCDM]+", numeral): continue
        text = open(path, encoding="latin-1").read()
        m = re.search(r"<BLOCKQUOTE>(.*?)(</BLOCKQUOTE>|$)", text, re.S | re.I)
        if not m: continue
        if not m.group(2): print(f"warning: {os.path.basename(path)} is truncated (no </BLOCKQUOTE>)", file=sys.stderr)
        body = re.sub(r"<[^>]+>", "", m.group(1))
        lines = []
        for raw in body.splitlines():
            l = html.unescape(raw)
            if not l.strip(): continue
            if "\t" in l[:4] and lines:          # the site wraps an over-long line onto a tab-indented continuation
                lines[-1] += " " + l.strip()
            else:
                lines.append(l.strip())
        sonnets.append((roman_to_int(numeral), numeral, lines))
    sonnets.sort()
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, "sonnets.txt"), "w") as f, open(os.path.join(out, "sonnets_lines.txt"), "w") as g:
        for n, numeral, lines in sonnets:
            f.write(f"Sonnet {n} ({numeral})\n")
            for l in lines:
                f.write(l + "\n")
                g.write(l + "\n")
            f.write("\n")
    print(f"{len(sonnets)} sonnets, {sum(len(l) for _, _, l in sonnets)} lines -> {out}")

if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
