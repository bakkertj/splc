#!/bin/sh
# expect_fail.sh SPLC PLAY [FLAGS...]; succeeds only if splc rejects the play
splc="$1"; play="$2"; shift 2
if "$splc" -fsyntax-only "$@" "$play" 2>/dev/null; then
  echo "expected compilation to fail"; exit 1
fi
