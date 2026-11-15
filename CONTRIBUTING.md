# Contributing

This repository is public. People and agents contribute the same way.

## Before the first commit

In a fresh clone, set the repository's noreply identity first, so no
private address reaches the history:

```sh
git config user.name "<GitHub name>"
git config user.email "<id>+<GitHub name>@users.noreply.github.com"
```

The scan below rejects any other author or committer address.

## Before every push

- `node scripts/build.mjs --check` passes.
- A change to the public surface has its line in CHANGELOG.md under
  "Unreleased".
- `sh scripts/scan-secrets.sh` passes. It needs git and gitleaks (with
  Nix, it fetches gitleaks itself). CI runs the same scan. After a change
  to the scan, `sh scripts/scan-secrets.test.sh` passes too.

The scan looks at the working tree and at every commit not yet on `main`
(what it adds, its message, author and committer), and fails on:

- a secret, in the history or in staged and unstaged changes (gitleaks);
- a local home path (`/home/…`, `/Users/…`), binary files included:
  write `~` or a placeholder;
- a mail address other than an `example.com` or a noreply one;
- the ticket id of a private tracker (`#` and four or five digits) in
  Markdown, a code comment or a commit message: refer to a GitHub issue
  instead;
- an author or committer address other than a noreply one.

No public file names local paths, other repositories on your machine,
people, hosts or machines, or what a private tracker holds (its tickets,
notes and columns).

### Private patterns

What only you know to keep out (names, hosts, words of your own tracker)
goes into `.privacy-patterns` at the root of your checkout: one extended
regular expression per line, `#` starts a comment. Git ignores the file;
never commit, copy or share it. The scan matches the patterns, regardless
of case, against the files that differ from `main` and against each
commit not yet on it: its files, message, author and committer. A hit
names the file and line or the commit and the pattern's number, never the
pattern. A git worktree uses its main checkout's file; a separate clone
has none of its own, and CI runs without one.

## Pull requests and review

- One topic per branch, from the current `main`, as a pull request.
- A reviewer does not push to the author's branch. Findings go into the
  review as comments; the author fixes them and pushes, and the reviewer
  looks again.
- Once CI is green and the review approves, the pull request is merged on
  GitHub with a merge commit (no squash, no rebase), and the branch is
  deleted.
