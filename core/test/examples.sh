#!/bin/sh
# The first app appears three times: examples/hello.c (built and run by make test), in rekta(3) and in README.md.
# Fails when a copy no longer matches the file.
set -e
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
# rekta(3): the literal block under EXAMPLES
sed -n '/^\.Sh EXAMPLES/,/^\.Sh /p' man/rekta.3 | sed -n '/^\.Bd -literal$/,/^\.Ed$/p' | sed '1d;$d' > "$tmp/man.c"
# README.md: the C block under "A first app"
sed -n '/^## A first app/,/^## /p' README.md | awk '/^```$/ && on { exit } on { print } /^```c$/ { on = 1 }' > "$tmp/readme.c"
status=0
for copy in man readme; do
  if ! diff -u examples/hello.c "$tmp/$copy.c" > "$tmp/$copy.diff"; then
    echo "examples/hello.c and its copy in $copy differ:"; cat "$tmp/$copy.diff"; status=1
  fi
done
[ $status -eq 0 ] && echo "the first app reads the same in hello.c, rekta(3) and README.md"
exit $status
