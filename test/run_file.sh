#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Trevor Bakker
# run_file.sh SPLC PLAY EXPECTED_FILE: compile PLAY, run it, and require its output to be
# byte-identical to EXPECTED_FILE.
set -e
splc="$1"; play="$2"; expected="$3"
tmp=$(mktemp -d)
"$splc" -fpentameter=off -o "$tmp/prog" "$play"
"$tmp/prog" > "$tmp/out" < /dev/null
if cmp -s "$tmp/out" "$expected"; then rm -rf "$tmp"; exit 0; fi
echo "output differs from $expected:"; diff "$tmp/out" "$expected" | head -20; rm -rf "$tmp"; exit 1
