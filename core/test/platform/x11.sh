#!/bin/sh
# x11.sh: drags files onto a Rekta window under Xvfb, as a file manager would (XDND 5), and checks what it took.
# Needs Xvfb and the X11 headers. Run from core/: sh test/platform/x11.sh
set -e
out=${TMPDIR:-/tmp}/rekta-dnd.$$
mkdir -p "$out"
cc -std=c99 -O1 -Iinclude test/platform/drop_target.c src/rekta.c -o "$out/target" -DRK_NO_WAYLAND -lX11 -lXext -lm -lpthread -ldl
cc -std=c99 -O1 test/platform/xdnd_send.c -o "$out/send" -lX11
Xvfb :73 -screen 0 800x600x24 >/dev/null 2>&1 & xvfb=$!
trap 'kill $xvfb 2>/dev/null; rm -rf "$out"' EXIT
sleep 1
DISPLAY=:73 RK_PLATFORM=x11 "$out/target" > "$out/taken" & target=$!
sleep 1
DISPLAY=:73 "$out/send" drop-target 200 150 "file:///tmp/a%20b.png
file:///tmp/c.txt
" > "$out/sent"
wait $target
grep -q "status accepted" "$out/sent" && grep -q "finished taken" "$out/sent" || { echo "x11 dnd: the source was not told it was taken"; cat "$out/sent"; exit 1; }
grep -q "^taken /tmp/a b.png$" "$out/taken" && grep -q "^refused /tmp/c.txt$" "$out/taken" || { echo "x11 dnd: wrong files"; cat "$out/taken"; exit 1; }
echo "x11 dnd: a b.png taken, c.txt refused, the source told"
