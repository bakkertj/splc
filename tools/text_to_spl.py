#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
"""Write a Shakespeare Programming Language play that prints a given text.

usage: text_to_spl.py [--title T] [--speaker NAME] [--listener NAME] text.txt > play.spl

One character (the listener) holds the current output byte; the other (the speaker)
tells them what to become and to speak. Each byte is reached from the previous one by
adding or subtracting a constant, spelled as a sum of powers of two (`a big big cat` is
4), whichever is shorter; a run of the same byte is a single assignment followed by
several `Speak thy mind!`. Nouns are drawn from a small rotating list so the play reads
less mechanically, and every adjective is `big`, so the arithmetic is easy to check by
eye. The result is prose, and the play says so with a `[Prose]` stage direction, so
`splc` does not warn about the metre.
"""
import sys

NOUNS = ["cat", "hat", "bird", "flower", "rose", "star", "ship", "song", "tree", "kiss", "jewel", "angel"]
INSULTS = ["pig", "toad", "worm", "devil", "hog", "wolf"]

def powers(n, nouns, idx):
    """Spell |n| as `the sum of ... and ...` over powers of two; returns (text, new idx)."""
    parts = []
    for k in range(n.bit_length()):
        if n >> k & 1:
            noun = nouns[idx % len(nouns)]; idx += 1
            parts.append(("a " if noun[0] not in "aeiou" else "an ") + "big " * k + noun)
    parts.reverse()
    expr = parts[-1]
    for p in reversed(parts[:-1]):
        expr = f"the sum of {p} and {expr}"
    return expr, idx

def main(argv):
    title, speaker, listener = "A Text, Recited.", "Juliet", "Romeo"
    files = []
    i = 0
    while i < len(argv):
        if argv[i] == "--title": title = argv[i + 1]; i += 2
        elif argv[i] == "--speaker": speaker = argv[i + 1]; i += 2
        elif argv[i] == "--listener": listener = argv[i + 1]; i += 2
        else: files.append(argv[i]); i += 1
    if not files:
        print(__doc__); return 2
    text = open(files[0], encoding="utf-8").read()
    out = []
    out.append(f"{title}\n")
    out.append(f"{listener}, who says what he is told.")
    out.append(f"{speaker}, who tells him.\n")
    out.append("                    Act I: The recitation.\n")
    out.append("                    Scene I: Every letter in its turn.\n")
    out.append(f"[Enter {listener} and {speaker}]")
    out.append("[Prose]\n")
    out.append(f"{speaker}:")
    cur, idx, first = 0, 0, True
    bytes_ = text.encode("utf-8")
    j = 0
    while j < len(bytes_):
        b = bytes_[j]
        run = 1
        while j + run < len(bytes_) and bytes_[j + run] == b: run += 1
        d = b - cur
        if d != 0:
            direct, idx1 = powers(b, NOUNS, idx)
            if d > 0:
                delta, idx2 = powers(d, NOUNS, idx)
                delta = f"the sum of thyself and {delta}"
            else:
                delta, idx2 = powers(-d, NOUNS, idx)
                delta = f"the difference between thyself and {delta}"
            if first or len(direct) <= len(delta):
                out.append(f" Thou art {direct}."); idx = idx1
            else:
                out.append(f" Thou art {delta}."); idx = idx2
            cur = b
        out.append(" Speak thy mind!" * run)
        first = False
        j += run
    out.append("\n[Exeunt]")
    print("\n".join(out))
    return 0

if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
