#!/usr/bin/env bash
set -e
set -o pipefail

rm -rf "extract-$3-$4" "extract-$3-$4.tmp" "extract-$3-$4.out" "extract-$3-$4.err"
"$1" -v -m extract -K -d "extract-$3-$4" -s "$4" "$2/test.$3" "extract-$3-$4.tmp" 2>"extract-$3-$4.err"
cat "extract-$3-$4.tmp" | grep -aEv '^\[transfuse:' | grep -aEv '^<STREAMCMD:TRANSFUSE:' > "extract-$3-$4.out"
rm -rf "extract-$3-$4" "extract-$3-$4.tmp"
diff "$2/extract-$3-$4.expect" "extract-$3-$4.out"
