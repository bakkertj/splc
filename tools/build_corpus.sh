#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
# Build shakespeare/plays/<play>_verse.txt and <play>_prose.txt from a
# shakespeare.mit.edu download, and shakespeare/poems/*.txt from its Poetry pages.
# usage: tools/build_corpus.sh shakespeare/shakespeare.mit.edu
set -e
src="${1:-shakespeare/shakespeare.mit.edu}"
here="$(dirname "$0")"
out="$here/../shakespeare/plays"
mkdir -p "$out"
printf '%6s %6s  %s\n' verse prose play
for d in "$src"/*/; do
  play=$(basename "$d")
  [ -f "$d/full.html" ] || continue
  python3 "$here/play_to_lines.py" --verse "$d" > "$out/${play}_verse.txt" 2>/dev/null
  python3 "$here/play_to_lines.py" --prose "$d" > "$out/${play}_prose.txt" 2>/dev/null
  printf '%6d %6d  %s\n' "$(wc -l < "$out/${play}_verse.txt")" "$(wc -l < "$out/${play}_prose.txt")" "$play"
done
poems="$here/../shakespeare/poems"
if [ -d "$src/Poetry" ]; then
  mkdir -p "$poems"
  python3 "$here/poem_to_stanzas.py" "$src/Poetry/VenusAndAdonis.html" > "$poems/venus_and_adonis.txt"
  python3 "$here/poem_to_stanzas.py" "$src/Poetry/RapeOfLucrece.html" > "$poems/lucrece.txt"
  python3 "$here/poem_to_stanzas.py" "$src/Poetry/LoversComplaint.html" > "$poems/lovers_complaint.txt"
fi
