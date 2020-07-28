#!/usr/bin/env bash
set -e
set -o pipefail
rm -rf "extract-$3" "extract-$3.tmp" "extract-$3.out"
"$1" -m extract -K -d "extract-$3" "$2/test.$3" "extract-$3.tmp"
cat "extract-$3.tmp" | grep -aEv '^\[transfuse:' > "extract-$3.out"
rm -rf "extract-$3" "extract-$3.tmp"
diff "$2/extract-$3.expect" "extract-$3.out"
