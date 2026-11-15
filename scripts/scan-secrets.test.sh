#!/bin/sh
# scan-secrets.test.sh: plants each kind of finding in a throwaway repository and checks that scan-secrets.sh fails on
# it with that finding and never shows the private pattern, and that it passes on the clean repository. gitleaks is a
# stand-in that finds nothing here; CI runs the real one on this repository. Run: sh scripts/scan-secrets.test.sh
set -u
scan="$(cd "$(dirname "$0")" && pwd)/scan-secrets.sh"
tmp=$(mktemp -d) || exit 2
trap 'rm -rf "$tmp"' EXIT
mkdir "$tmp/bin"
printf '#!/bin/sh\nexit 0\n' >"$tmp/bin/gitleaks"
chmod +x "$tmp/bin/gitleaks"
PATH="$tmp/bin:$PATH"
# The user's git config (signing, hooks, another identity) stays out of the throwaway repository.
export GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1 GIT_AUTHOR_NAME=ada GIT_COMMITTER_NAME=ada
export GIT_AUTHOR_EMAIL=1+ada@users.noreply.github.com GIT_COMMITTER_EMAIL=1+ada@users.noreply.github.com
# Put together here, so this file holds none of what it plants.
home_path="/home""/quokka/notes.txt"
address="quokka@""mail.test"
ticket="#""1234"
pattern=wombat

# A repository whose main is on origin, with one commit of its own and a stand-in .privacy-patterns.
fresh_repo() {
	cd "$tmp" && rm -rf repo && git init -q -b main repo && cd repo || exit 2
	echo .privacy-patterns >.git/info/exclude
	echo "$pattern" >.privacy-patterns
	echo "Stand-ins pass: /home/ada/, ada@example.com." >README.md
	git add . && git commit -qm "A start" && git update-ref refs/remotes/origin/main HEAD
	echo "A GitHub issue passes: (#160)." >>README.md && git commit -qam "Work of its own (#160)"
}

failures=0
# Scans the repository: $1 says what it holds, $2 is the finding the scan must report, or "" when it must pass.
expect() {
	out=$(sh "$scan" 2>&1)
	code=$?
	if printf '%s\n' "$out" | grep -q "$pattern"; then
		verdict="shows the private pattern"
	elif [ -z "$2" ]; then
		[ $code -eq 0 ] && verdict=ok || verdict="exit $code, expected no finding"
	elif [ $code -ne 1 ] || ! printf '%s\n' "$out" | grep -qF "$2: "; then
		verdict="exit $code, expected \"$2\""
	else
		verdict=ok
	fi
	[ "$verdict" = ok ] && { echo "ok: $1"; return; }
	echo "FAILED: $1 ($verdict):"
	printf '%s\n' "$out" | sed 's/^/    /'
	failures=$((failures + 1))
}

fresh_repo
expect "a clean repository with stand-ins and a GitHub issue passes" ""

fresh_repo
echo "$home_path" >>README.md
expect "a home path in a changed file fails" "Local home path"

fresh_repo
printf 'ELF\0%s\0' "$home_path" >tool.o
expect "a home path inside a binary file fails" "Local home path"

fresh_repo
echo "$address" >>README.md
expect "a mail address other than an example or noreply one fails" "Mail address"

fresh_repo
git commit -q --allow-empty -m "A fix ($ticket)"
expect "a four-digit ticket id in a commit message fails" "Ticket id of a private tracker"

fresh_repo
echo "Done in ${ticket}5." >>README.md
expect "a five-digit ticket id in Markdown fails" "Ticket id of a private tracker"

fresh_repo
GIT_COMMITTER_EMAIL=$address git commit -q --allow-empty -m "A change"
expect "a committer address other than a noreply one fails" "Author or committer without noreply"

fresh_repo
echo "$pattern" >"$(printf '\303\234bersicht.md')"
git add . && git commit -qm "An overview"
expect "a private pattern in a file with a non-ASCII name fails" "Private pattern #1"

fresh_repo
echo "$home_path" >notes.txt
git add notes.txt && git commit -qm "Notes" && git rm -q notes.txt && git commit -qm "No notes"
expect "a home path one commit adds and the next removes fails" "Local home path"

[ $failures -eq 0 ] && echo "scan-secrets: every case behaves" && exit 0
echo "scan-secrets: $failures case(s) failed"
exit 1
