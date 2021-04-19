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

"$C2FFI" "test1.hpp" -m -T -D clib -o "out/clib-test1" -I "."
g++ out/clib-test1.cpp -c -o out/clib-test1.o -I "."
# no definition for test1 is available, so just check syntax in C
clang -fsyntax-only -x c out/clib-test1.h

# TEST 2
g++ "test2.cpp" -c -o "out/test2.o"
"$C2FFI" "test2.hpp" -m -T -D clib -o "out/clib-test2" -I "."
g++ out/clib-test2.cpp -c -o out/clib-test2.o -I "."
gcc main2.c out/clib-test2.o out/test2.o -o out/test2 -I "out"
./out/test2
