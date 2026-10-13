#!/bin/sh
# chooser.sh: asks for files under Xvfb, first through a stand-in desktop portal on a private session bus, then with
# no portal, through a stand-in zenity. Needs Xvfb, dbus-daemon and the X11 headers. Run from core/: sh test/dnd/chooser.sh
set -e
out=${TMPDIR:-/tmp}/rekta-chooser.$$
mkdir -p "$out/bin"
cc -std=c99 -O1 -Iinclude test/dnd/chooser_test.c src/rekta.c -o "$out/ask" -DRK_NO_WAYLAND -lX11 -lXext -lm -lpthread -ldl
cc -std=c99 -O1 test/dnd/fake_portal.c -o "$out/portal" -ldl
printf '#!/bin/sh\necho "$@" > "%s/zenity.args"\nprintf "/home/ada/one.png\\n"\n' "$out" > "$out/bin/zenity"
chmod +x "$out/bin/zenity"
cat > "$out/bus.conf" <<CONF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN" "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig><type>session</type><listen>unix:path=$out/bus</listen><auth>EXTERNAL</auth>
<policy context="default"><allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/></policy></busconfig>
CONF
Xvfb :76 -screen 0 800x600x24 >/dev/null 2>&1 & xvfb=$!
dbus-daemon --config-file="$out/bus.conf" --nofork >/dev/null 2>&1 & bus=$!
trap 'kill $xvfb $bus 2>/dev/null; rm -rf "$out"' EXIT
for i in 1 2 3 4 5 6 7 8 9 10; do test -S "$out/bus" && break; sleep 0.3; done
sleep 0.5
export DISPLAY=:76 RK_PLATFORM=x11 PATH="$out/bin:$PATH"
DBUS_SESSION_BUS_ADDRESS="unix:path=$out/bus" "$out/portal" "file:///home/ada/portal%20pick.png" > "$out/portal.out" & portal=$!
sleep 0.5
DBUS_SESSION_BUS_ADDRESS="unix:path=$out/bus" "$out/ask" > "$out/asked"
wait $portal
grep -q "^file /home/ada/portal pick.png$" "$out/asked" || { echo "portal: wrong answer"; cat "$out/asked"; exit 1; }
grep -q "^filters Accepted files \*.png \*.jpg" "$out/portal.out" && grep -q "^multiple 1$" "$out/portal.out" || { echo "portal: wrong options"; cat "$out/portal.out"; exit 1; }
test ! -f "$out/zenity.args" || { echo "portal: zenity was asked too"; exit 1; }
DBUS_SESSION_BUS_ADDRESS="unix:path=$out/none" "$out/ask" > "$out/asked2"
grep -q "^file /home/ada/one.png$" "$out/asked2" && grep -q -- "--file-filter=Accepted files | \*.png" "$out/zenity.args" || { echo "zenity: wrong"; cat "$out/asked2" "$out/zenity.args"; exit 1; }
echo "chooser: the portal's answer taken with its filter; without a portal, zenity's"
