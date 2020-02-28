#!/usr/bin/env bash

set -euo pipefail

GIT_REMOTE=https://github.com/advancedtelematic/aktualizr
DOX_DOCS=${DOX_DOCS:-$TEST_BUILD_DIR/docs/doxygen/html}
WORKTREE=${WORKTREE:-$TEST_BUILD_DIR/pages}
DRY_RUN=${DRY_RUN:-0}
DESCRIBE=$(git describe)
LAST_TAG=$(git describe --abbrev=0)

set -x

git remote add github_rls "$GIT_REMOTE" || true
git fetch github_rls
if ! [ -d "$WORKTREE" ]; then
    mkdir -p "$WORKTREE"
    git worktree add "$WORKTREE" github_rls/gh-pages
fi

gitcommit() (
    export GIT_AUTHOR_NAME="HERE OTA Gitlab CI"
    export GIT_AUTHOR_EMAIL="gitlab@example.org"
    export GIT_COMMITTER_NAME=$GIT_AUTHOR_NAME
    export GIT_COMMITTER_EMAIL=$GIT_AUTHOR_EMAIL
    git commit "$@"
)

DOX_DOCS=$(realpath "$DOX_DOCS")
WORKTREE=$(realpath "$WORKTREE")

(
cd "$WORKTREE"

git reset --hard github_rls/gh-pages

# create release directory
if [ -d "$WORKTREE/$LAST_TAG" ]; then
    echo "Docs for $LAST_TAG already published, skipping..."
else
    cp -r "$DOX_DOCS" "$WORKTREE/$LAST_TAG"
    git add "$WORKTREE/$LAST_TAG"
    gitcommit -m "$LAST_TAG release"
fi

# create last snapshot

# cleanup old snapshot
find . \( -regex './[^/]*' -and -type f -and -not -path ./.git \) -or \( -path './search/*' \) -exec git rm -r {} +
cp -ar "$DOX_DOCS/." .
git add .
if git diff --cached --quiet; then
    echo "Docs already updated to the latest version, skipping..."
else
    gitcommit -m "Update docs to latest ($DESCRIBE)"
fi

if [ "$DRY_RUN" != 1 ]; then
    git config credential.${GIT_REMOTE}.username "$GITHUB_API_USER"
    git config credential.${GIT_REMOTE}.helper '!f() { echo "password=$(echo $GITHUB_API_TOKEN)"; }; f'
    git push github_rls HEAD:gh-pages
fi
)
