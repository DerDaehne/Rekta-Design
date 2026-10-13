#!/bin/sh
# scale.sh: changes Xft.dpi under Xvfb while a Rekta window is open, as a desktop's display settings would, and checks
# the window keeps its logical size in twice the pixels. Needs Xvfb and the X11 headers. Run from core/: sh test/platform/scale.sh
set -e
out=${TMPDIR:-/tmp}/rekta-scale.$$
mkdir -p "$out"
cc -std=c99 -O1 -Iinclude test/platform/scale_test.c src/rekta.c -o "$out/scale" -DRK_NO_WAYLAND -lX11 -lXext -lm -lpthread -ldl
Xvfb :74 -screen 0 800x600x24 >/dev/null 2>&1 & xvfb=$!
trap 'kill $xvfb 2>/dev/null; rm -rf "$out"' EXIT
sleep 1
DISPLAY=:74 RK_PLATFORM=x11 "$out/scale" > "$out/seen"
grep -q "^before scale 1 size 300x200 pixels 300$" "$out/seen" && grep -q "^after scale 2 size 300x200 pixels 600$" "$out/seen" ||
  { echo "x11 scale: wrong"; cat "$out/seen"; exit 1; }
echo "x11 scale: Xft.dpi 96 to 192 while open, the window 300x200 in 600 pixels wide"
