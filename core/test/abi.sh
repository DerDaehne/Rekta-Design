#!/bin/sh
# abi.sh: the shared library shows exactly the headers' RK_API functions (rekta.h and rekta_test.h) (nothing internal leaks into the ABI), and a
# program links against it and runs. Run by make abi: sh test/abi.sh BUILD LIBRARY
set -e
build=$1 lib=$2
want=$build/abi-header.txt got=$build/abi-library.txt
cat include/rekta.h include/rekta_test.h | grep 'RK_API' | grep -v '#' | sed 's,/\*.*,,' | sed -n 's/.*[ *]\(rk_[a-z0-9_]*\)(.*/\1/p' | sort -u > "$want"
case $(uname -s) in
  Darwin) nm -gU "$build/$lib" | awk '{ print $3 }' | sed 's/^_//' ;;
  *) nm -D --defined-only "$build/$lib" | awk '$2 ~ /^[TDRB]$/ { print $3 }' ;;
esac | sort -u > "$got"
if ! cmp -s "$want" "$got"; then
  echo "abi: the library's exports differ from the header's RK_API functions (< header only, > library only):"
  diff "$want" "$got" | grep '^[<>]' | head -40
  exit 1
fi
cc -std=c99 -Wno-unused-parameter -Iinclude examples/hello.c -L"$build" -lrekta -Wl,-rpath,"$PWD/$build" -o "$build/hello-shared"
RK_HEADLESS=1 "$build/hello-shared" > /dev/null
echo "abi: $(wc -l < "$got" | tr -d ' ') functions, all the header's and nothing else; hello runs against $lib"
