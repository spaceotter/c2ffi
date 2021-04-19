#!/usr/bin/env bash
set -e
set -x
bindir="$1"
testdir="$2"
C2FFI="$(realpath $bindir/c2ffi)"

rm -rf "$testdir/out"
mkdir -p "$testdir/out"
cd "$testdir"
"$C2FFI" "test1.hpp" -m -T -D json -o "out/test1.spec" -I "."
diff "out/test1.spec" "test1.spec"
