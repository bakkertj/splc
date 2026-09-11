#!/bin/sh
# run.sh SPLC PLAY EXPECTED_OUTPUT [STDIN]
set -e
splc="$1"; play="$2"; expected="$3"; input="${4:-}"
tmp=$(mktemp -d)
"$splc" -fpentameter=off -o "$tmp/prog" "$play"
actual=$(printf '%s\n' "$input" | "$tmp/prog")
rm -rf "$tmp"
if [ "$actual" != "$expected" ]; then
  echo "expected: [$expected]"; echo "actual:   [$actual]"; exit 1
fi
