#!/bin/sh
# Build shakespeare/plays/<play>_lines.txt from a shakespeare.mit.edu download.
# usage: tools/build_corpus.sh shakespeare/shakespeare.mit.edu
set -e
src="${1:-shakespeare/shakespeare.mit.edu}"
out="$(dirname "$0")/../shakespeare/plays"
mkdir -p "$out"
for d in "$src"/*/; do
  play=$(basename "$d")
  [ -f "$d/full.html" ] || continue
  python3 "$(dirname "$0")/play_to_lines.py" "$d" > "$out/${play}_lines.txt" 2>/dev/null
  printf '%6d %s\n' "$(wc -l < "$out/${play}_lines.txt")" "$play"
done
