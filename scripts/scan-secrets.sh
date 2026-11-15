#!/bin/sh
# scan-secrets.sh: run before every push (CONTRIBUTING.md). Looks at the working tree and at every commit not yet on
# origin/main for secrets (gitleaks), local home paths, mail addresses, tracker ticket ids, author and committer
# addresses other than noreply ones, and the patterns of a local, gitignored .privacy-patterns, if there is one.
# Exits non-zero on any finding. Run from anywhere in the checkout: sh scripts/scan-secrets.sh
set -u
cd "$(git rev-parse --show-toplevel)" || exit 2

HOME_PATH='(^|[^[:alnum:]_.~-])/(home|Users)/[[:alnum:]_]'
EXAMPLE_HOME='/(home|Users)/ada/' # the tests' stand-in user
MAIL='[[:alnum:]._%+-]+@[[:alnum:].-]+\.[[:alpha:]]{2,}'
EXAMPLE_MAIL='[[:alnum:]._%+-]+@(example\.(com|org|net)|users\.noreply\.github\.com)|noreply@[[:alnum:].-]+'
NOREPLY='(@users\.noreply\.github\.com|^noreply@[[:alnum:].-]+)$'
# ponytail: GitHub's issue numbers here have three digits and a private tracker's four; once the issues reach four
# digits, tell them apart another way. Outside Markdown only in comments, so a number in a demo's text stays.
TICKET_ID='(^|[^&[:alnum:]])#[0-9]{4}([^[:alnum:]]|$)'
IN_COMMENT='(//|/\*|^[[:space:]]*[*#]|<!--|[[:space:]]#[[:space:]]).*'

status=0
has_base=false
git rev-parse --verify -q origin/main >/dev/null && has_base=true
commits=""
$has_base && commits=$(git rev-list --no-merges origin/main..HEAD)

# Prints each line of $2 as a finding of kind $1 with the hint $3, and marks the scan failed.
report() {
	[ -n "$2" ] || return 0
	printf '%s\n' "$2" | while IFS= read -r hit; do echo "$1: $hit. $3." >&2; done
	status=1
}

# Prints "<file>:<line>:<text>" for $1 in tracked and untracked, not ignored files (pathspec $2), vendored code left out.
grep_files() {
	git grep -n -I --untracked -E -e "$1" -- "${2:-.}" ':!core/third_party' 2>/dev/null
}

gitleaks_() {
	if command -v gitleaks >/dev/null 2>&1; then
		gitleaks "$@"
	elif command -v nix >/dev/null 2>&1; then
		nix shell nixpkgs#gitleaks --command gitleaks "$@"
	else
		echo "gitleaks is missing: https://github.com/gitleaks/gitleaks/releases" >&2
		return 1
	fi
}

echo "== gitleaks: history, staged and unstaged changes =="
for scope in "" --staged --pre-commit; do
	# shellcheck disable=SC2086 # an empty scope is no argument
	gitleaks_ git $scope --redact --no-banner . ||
		report "gitleaks" "a secret or an error${scope:+ ($scope)}" "Rotate a real secret and rewrite the commit before pushing"
done

echo "== Home paths, mail addresses, tracker ticket ids, identities =="
report "Local home path" "$(grep_files "$HOME_PATH" | sed -E "s#$EXAMPLE_HOME##g" | grep -E "$HOME_PATH" | cut -d: -f1,2)" \
	"Write ~ or a placeholder instead"
report "Mail address" "$(grep_files "$MAIL" | sed -E "s#$EXAMPLE_MAIL##g" | grep -E "$MAIL" | cut -d: -f1,2)" \
	"Use name@example.com or a noreply address"
report "Ticket id of a private tracker" "$({ grep_files "$TICKET_ID" '*.md'; grep_files "$IN_COMMENT$TICKET_ID"; } | cut -d: -f1,2 | sort -u)" \
	"Refer to a GitHub issue, or say it in words"
if $has_base; then
	for sha in $commits; do
		short=$(git rev-parse --short "$sha")
		message=$(git log -1 --format=%B "$sha")
		printf '%s\n' "$message" | grep -qE "$HOME_PATH" && report "Local home path" "message of $short" "Reword the commit"
		printf '%s\n' "$message" | sed -E "s#$EXAMPLE_MAIL##g" | grep -qE "$MAIL" && report "Mail address" "message of $short" "Reword the commit"
		printf '%s\n' "$message" | grep -qE "$TICKET_ID" && report "Ticket id of a private tracker" "message of $short" "Reword the commit"
		git log -1 --format='%ae%n%ce' "$sha" | grep -qvE "$NOREPLY" &&
			report "Author or committer without noreply" "$short" "Set the repository's noreply identity, then git commit --amend --reset-author"
	done
else
	echo "origin/main is unknown (git fetch): commit messages and identities not checked."
fi

echo "== Private patterns =="
patterns=.privacy-patterns
# A git worktree reads its main checkout's file in place; a separate clone has none of its own.
[ -f "$patterns" ] || patterns="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")/.privacy-patterns"
if [ ! -f "$patterns" ]; then
	echo "No .privacy-patterns in this checkout: skipped."
elif ! $has_base; then
	echo "origin/main is unknown (git fetch): skipped."
else
	{ set +x; } 2>/dev/null # xtrace would print the patterns
	hint="Remove it, or rewrite the commit that brought it"
	changed=$({ git diff --name-only origin/main; git ls-files --others --exclude-standard; } | sort -u)
	n=0
	while IFS= read -r pattern || [ -n "$pattern" ]; do
		pattern=$(printf '%s' "$pattern" | tr -d '\r')
		case "$pattern" in "" | "#"*) continue ;; esac
		n=$((n + 1))
		: | grep -E -- "$pattern" >/dev/null 2>&1
		[ $? -eq 2 ] && { report "Private pattern #$n" "not a valid extended regex" "Fix it in .privacy-patterns"; continue; }
		report "Private pattern #$n" "$(printf '%s\n' "$changed" | grep -i -E -- "$pattern" | sed 's/^/the file name /')" "$hint"
		[ -n "$changed" ] && report "Private pattern #$n" "$(printf '%s\n' "$changed" | tr '\n' '\0' |
			xargs -0 git grep -n -I -i --untracked -E -e "$pattern" -- 2>/dev/null | cut -d: -f1,2)" "$hint"
		for sha in $commits; do
			short=$(git rev-parse --short "$sha")
			files=$(git diff-tree --no-commit-id --name-only -r --diff-filter=d "$sha")
			printf '%s\n' "$files" | grep -q -i -E -- "$pattern" && report "Private pattern #$n" "a file name in $short" "$hint"
			[ -n "$files" ] && report "Private pattern #$n" "$(printf '%s\n' "$files" | tr '\n' '\0' |
				xargs -0 git grep -n -I -i -E -e "$pattern" "$sha" -- 2>/dev/null | cut -d: -f2,3 | sed "s/^/$short, /")" "$hint"
			git log -1 --format='%an <%ae>%n%cn <%ce>%n%B' "$sha" | grep -q -i -E -- "$pattern" &&
				report "Private pattern #$n" "the message, author or committer of $short" "$hint"
		done
	done <"$patterns"
fi

[ "$status" -eq 0 ] && echo "No findings."
exit "$status"
