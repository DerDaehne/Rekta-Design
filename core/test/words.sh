#!/bin/sh
# words.sh: the words the library says come from one table, the English rk_strings in src/app.c (between
# rk-strings-begin and rk-strings-end), so an app can give its own with rk_app_set_strings. Fails when one of those
# words is written anywhere else in src/ (an icon of the same name aside), when a call that puts words on the screen,
# in a toast or in what is read aloud is handed a literal, or when a count or a figure is written with printf instead
# of the table's plural forms and the app's digits. Run by make test: sh test/words.sh
set -e
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
sed -n '/rk-strings-begin/,/rk-strings-end/p' src/app.c | grep -o '"[^"]*"' > "$tmp/words"
[ -s "$tmp/words" ] || { echo "words: no table between rk-strings-begin and rk-strings-end in src/app.c"; exit 1; }
# every source line but the table's, numbered
for f in src/*.c src/*.m; do
  awk -v f="$f" '/rk-strings-begin/ { skip = 1 } !skip { print f ":" FNR ": " $0 } /rk-strings-end/ { skip = 0 }' "$f"
done > "$tmp/lines"
status=0
while read -r word; do
  grep -F -- "$word" "$tmp/lines" | grep -v -E 'rk_canvas_icon\(|rk_icon(_set)?\(|rk__icon_check\(|\*const icons\[|rk__(error|warn|log)\(' > "$tmp/hits" || true
  if [ -s "$tmp/hits" ]; then echo "words: $word is the table's, written here instead of taken from rk__words():"; cat "$tmp/hits"; status=1; fi
done < "$tmp/words"
grep -E '(rk_window_toast|rk_button|rk_label|rk__text_draw|rk__text_width|rk_canvas_text|rk__toast_create|rk_menu_add|rk_menu_add_label|rk__say|rk_view_set_access(_value)?)\([^;]*"[A-Za-z]|access_(label|value), *"[A-Za-z]' "$tmp/lines" | grep -v -E 'RK__EXPECT|rk__expect\(' > "$tmp/calls" || true
if [ -s "$tmp/calls" ]; then echo "words: a literal handed to a call that shows it or reads it aloud (put the word in rk_strings):"; cat "$tmp/calls"; status=1; fi
# "%d items", "%.1f": a count says the table's forms (rk__count), a figure the app's digits (rk_format_number, rk__figure:
# app.c, where figures are made, is the one place that prints digits)
grep -E 'snprintf\([^;]*"([^"]*%[-0-9.*]*l?[diu] [A-Za-z][^"]*|%[-0-9.*]*[fg])"' "$tmp/lines" | grep -v '^src/app\.c:' > "$tmp/figures" || true
if [ -s "$tmp/figures" ]; then echo "words: a count or a figure written with printf (use rk__count, rk_format_number or rk__figure):"; cat "$tmp/figures"; status=1; fi
[ $status -eq 0 ] && echo "words: the $(wc -l < "$tmp/words" | tr -d ' ') built-in words come from rk_strings, none written elsewhere; counts and figures follow them"
exit $status
