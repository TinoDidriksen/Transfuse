#!/usr/bin/env bash
set -e
set -o pipefail

rm -rf "$5/clean-$3-$4" "clean-$3-$4.out" "clean-$3-$4.err"
"$1" -v -m clean -K -d "$5/clean-$3-$4" -s "$4" "$2/test.$3" "clean-$3-$4.out" 2>"clean-$3-$4.err"
rm -rf "$5/clean-$3-$4"
diff "$2/clean-$3-$4.expect" "clean-$3-$4.out"
