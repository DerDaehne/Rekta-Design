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
EXAMPLE_MAIL='[[:alnum:]._%+-]+@(example\.(com|org|net)|users\.noreply\.github\.com)|(^|[^[:alnum:]._%+-])noreply@[[:alnum:].-]+'
NOREPLY='(@users\.noreply\.github\.com|^noreply@[[:alnum:].-]+)$'
# ponytail: GitHub's issue numbers here have three digits, a private tracker's four or five (six would read as a
# colour, #049896); once GitHub's reach four digits or the tracker's six, tell them apart another way. Outside
# Markdown only in comments, so a number in a demo's text stays.
TICKET_ID='(^|[^&[:alnum:]])#[0-9]{4,5}([^[:alnum:]]|$)'
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

# Prints "<file>:<line>:<text>" for $2 in tracked and untracked, not ignored files (pathspec $3), vendored code left out.
# $1 is -a to read binary files too, or -I to leave them out.
grep_files() {
	git -c core.quotePath=false grep -n "$1" --untracked -E -e "$2" -- "${3:-.}" ':!core/third_party' 2>/dev/null
}

# Prints the lines commit $1 adds to the files of the pathspecs after $2, vendored code left out; $2 --text reads
# binary files too, "" leaves them out.
added_lines() {
	commit=$1 text=$2
	shift 2
	git -c core.quotePath=false show --format= --unified=0 --no-color --no-ext-diff --no-textconv ${text:+"$text"} \
		"$commit" -- "$@" ':!core/third_party' | LC_ALL=C awk '/^diff /{ hunk = 0 } /^@@/{ hunk = 1; next } hunk && /^\+/{ print substr($0, 2) }'
}

# Checks commit $1: its message and what it adds, its author and committer.
check_commit() {
	short=$(git rev-parse --short "$1")
	message=$(git log -1 --format=%B "$1")
	where="$short, in its message or what it adds"
	rewrite="Reword or rewrite the commit"
	{ printf '%s\n' "$message"; added_lines "$1" --text .; } | sed -E "s#$EXAMPLE_HOME##g" | grep -qaE "$HOME_PATH" &&
		report "Local home path" "$where" "$rewrite"
	{ printf '%s\n' "$message"; added_lines "$1" "" .; } | sed -E "s#$EXAMPLE_MAIL##g" | grep -qE "$MAIL" &&
		report "Mail address" "$where" "$rewrite"
	{ printf '%s\n' "$message" | grep -E "$TICKET_ID"; added_lines "$1" "" '*.md' | grep -E "$TICKET_ID"
		added_lines "$1" "" . | grep -E "$IN_COMMENT$TICKET_ID"; } | grep -q . &&
		report "Ticket id of a private tracker" "$where" "$rewrite"
	git log -1 --format='%ae%n%ce' "$1" | grep -qvE "$NOREPLY" &&
		report "Author or committer without noreply" "$short" "Set the repository's noreply identity, then git commit --amend --reset-author"
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
	gitleaks_ git $scope --verbose --redact --no-banner . ||
		report "gitleaks" "a secret or an error${scope:+ ($scope)}" "Rotate a real secret and rewrite the commit before pushing"
done

echo "== Home paths, mail addresses, tracker ticket ids, identities =="
report "Local home path" "$(grep_files -a "$HOME_PATH" | sed -E "s#$EXAMPLE_HOME##g" | grep -aE "$HOME_PATH" | cut -d: -f1,2)" \
	"Write ~ or a placeholder instead"
report "Mail address" "$(grep_files -I "$MAIL" | sed -E "s#$EXAMPLE_MAIL##g" | grep -E "$MAIL" | cut -d: -f1,2)" \
	"Use name@example.com or a noreply address"
report "Ticket id of a private tracker" "$({ grep_files -I "$TICKET_ID" '*.md'; grep_files -I "$IN_COMMENT$TICKET_ID"; } |
	cut -d: -f1,2 | sort -u)" "Refer to a GitHub issue, or say it in words"
if $has_base; then
	for sha in $commits; do check_commit "$sha"; done
else
	echo "origin/main is unknown (git fetch): commits not checked."
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
	changed=$({ git -c core.quotePath=false diff --name-only origin/main
		git -c core.quotePath=false ls-files --others --exclude-standard; } | sort -u)
	n=0
	while IFS= read -r pattern || [ -n "$pattern" ]; do
		pattern=$(printf '%s' "$pattern" | tr -d '\r')
		case "$pattern" in "" | "#"*) continue ;; esac
		n=$((n + 1))
		: | grep -E -- "$pattern" >/dev/null 2>&1
		[ $? -eq 2 ] && { report "Private pattern #$n" "not a valid extended regex" "Fix it in .privacy-patterns"; continue; }
		report "Private pattern #$n" "$(printf '%s\n' "$changed" | grep -i -E -- "$pattern" 2>/dev/null | sed 's/^/the file name /')" "$hint"
		[ -n "$changed" ] && report "Private pattern #$n" "$(printf '%s\n' "$changed" | tr '\n' '\0' |
			xargs -0 git -c core.quotePath=false grep -n -a -i --untracked -E -e "$pattern" -- 2>/dev/null | cut -d: -f1,2)" "$hint"
		for sha in $commits; do
			short=$(git rev-parse --short "$sha")
			files=$(git -c core.quotePath=false diff-tree --no-commit-id --name-only -r --diff-filter=d "$sha")
			printf '%s\n' "$files" | grep -q -i -E -- "$pattern" 2>/dev/null && report "Private pattern #$n" "a file name in $short" "$hint"
			[ -n "$files" ] && report "Private pattern #$n" "$(printf '%s\n' "$files" | tr '\n' '\0' |
				xargs -0 git -c core.quotePath=false grep -n -a -i -E -e "$pattern" "$sha" -- 2>/dev/null | cut -d: -f2,3 | sed "s/^/$short, /")" "$hint"
			git log -1 --format='%an <%ae>%n%cn <%ce>%n%B' "$sha" | grep -q -i -E -- "$pattern" 2>/dev/null &&
				report "Private pattern #$n" "the message, author or committer of $short" "$hint"
		done
	done <"$patterns"
fi

[ "$status" -eq 0 ] && echo "No findings."
exit "$status"
