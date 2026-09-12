#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
# calibration.sh SPLC: the scansion and rhyme checkers must still tell Shakespeare's verse
# from his prose. Thresholds sit a few points below the measured figures so that a
# regression in the lexicon or the cost model fails CI, while ordinary drift does not.
set -e
splc="$1"
here="$(cd "$(dirname "$0")/.." && pwd)"
rate() { "$splc" --scan-text "$@" 2>/dev/null | tail -1 | awk '{printf "%d", 100*$1/$3}'; }
check() { # name value op threshold [unit]
  if [ "$2" "$3" "$4" ]; then echo "ok    $1: $2$5 ($3 $4)"; else echo "FAIL  $1: $2$5 (wanted $3 $4)"; fail=1; fi
}
fail=0
check "sonnets scan"            "$(rate "$here/shakespeare/sonnets_lines.txt")"                -ge 90 %
check "Richard II verse scans"  "$(rate "$here/shakespeare/plays/richardii_verse.txt")"        -ge 88 %
check "Hamlet verse scans"      "$(rate "$here/shakespeare/plays/hamlet_verse.txt")"           -ge 82 %
check "Hamlet prose rejected"   "$(rate "$here/shakespeare/plays/hamlet_prose.txt")"           -le 40 %
check "Merry Wives prose rejected" "$(rate "$here/shakespeare/plays/merry_wives_prose.txt")"   -le 40 %
violations() { grep -v '^Sonnet' "$here/shakespeare/sonnets.txt" | "$splc" --scan-text "$@" /dev/stdin 2>/dev/null | tail -1 | awk '{print $1}'; }
v=$(violations "-frhyme-scheme=ABAB CDCD EFEF GG")
check "sonnet rhyme violations (of 1078 pairs)" "$v" -le 100
v=$(violations -fnear-rhymes "-frhyme-scheme=ABAB CDCD EFEF GG")
check "sonnet rhyme violations with -fnear-rhymes" "$v" -le 40
exit $fail
