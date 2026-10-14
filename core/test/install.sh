#!/bin/sh
# install.sh: installs under a fresh prefix, then builds an app the way a user would, through pkg-config, against the
# shared library and against the static one, and runs both. Run from core/: sh test/install.sh
set -e
prefix=${TMPDIR:-/tmp}/rekta-install.$$
trap 'rm -rf "$prefix"' EXIT
make -s install PREFIX="$prefix" > /dev/null
export PKG_CONFIG_PATH="$prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
test "$(pkg-config --modversion rekta)" = "$(awk '/^#define RK_VERSION_(MAJOR|MINOR|PATCH) /{ v = v (v == "" ? "" : ".") $3 } END { print v }' include/rekta.h)"
cc -std=c99 -Wno-unused-parameter examples/hello.c $(pkg-config --cflags --libs rekta) -Wl,-rpath,"$prefix/lib" -o "$prefix/hello"
RK_HEADLESS=1 "$prefix/hello" > /dev/null
case $(uname -s) in Darwin) static_flags= ;; *) static_flags=-Wl,-Bstatic ;; esac
if [ -n "$static_flags" ]; then
  cc -std=c99 -Wno-unused-parameter examples/hello.c $(pkg-config --cflags rekta) -L"$prefix/lib" -Wl,-Bstatic -lrekta -Wl,-Bdynamic $(pkg-config --static --libs rekta | tr ' ' '\n' | grep -v '^-lrekta$' | tr '\n' ' ') -o "$prefix/hello-static"
  RK_HEADLESS=1 "$prefix/hello-static" > /dev/null
  ! ldd "$prefix/hello-static" | grep -q librekta
fi
test -f "$prefix/share/man/man3/rekta.3" && test -f "$prefix/include/rekta.h"
echo "install: rekta $(pkg-config --modversion rekta) found by pkg-config; hello built and run, shared and static"
