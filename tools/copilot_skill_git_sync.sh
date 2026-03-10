#!/usr/bin/env bash
set -euo pipefail

MAX_BACKUPS=5
BACKUP_PREFIX="copilot-backup-v"
REMOTE="${GIT_REMOTE:-origin}"

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[ERROR] Not inside a git repository."
  exit 1
fi

if ! git remote get-url "$REMOTE" >/dev/null 2>&1; then
  echo "[ERROR] Remote '$REMOTE' not found."
  exit 1
fi

BRANCH="$(git rev-parse --abbrev-ref HEAD)"
if [ "$BRANCH" = "HEAD" ]; then
  echo "[ERROR] Detached HEAD is not supported."
  exit 1
fi

REQ_MSG="${1:-requirement done}"
TIMESTAMP="$(date '+%Y-%m-%d %H:%M:%S')"
COMMIT_MSG="copilot: ${REQ_MSG} (${TIMESTAMP})"

git add -A

if git diff --cached --quiet; then
  echo "[INFO] No file changes detected. Skip commit and backup rotation."
  SHOULD_ROTATE=0
else
  git commit -m "$COMMIT_MSG"
  SHOULD_ROTATE=1
fi

if [ "$SHOULD_ROTATE" -eq 1 ]; then
  if git rev-parse --verify HEAD~1 >/dev/null 2>&1; then
    BACKUP_COMMIT="$(git rev-parse HEAD~1)"
  else
    BACKUP_COMMIT="$(git rev-parse HEAD)"
  fi

  for i in $(seq "$MAX_BACKUPS" -1 2); do
    PREV=$((i - 1))
    PREV_TAG="${BACKUP_PREFIX}${PREV}"
    CURR_TAG="${BACKUP_PREFIX}${i}"

    if git rev-parse -q --verify "refs/tags/${PREV_TAG}" >/dev/null; then
      git tag -f "$CURR_TAG" "$PREV_TAG" >/dev/null
    else
      if git rev-parse -q --verify "refs/tags/${CURR_TAG}" >/dev/null; then
        git tag -d "$CURR_TAG" >/dev/null
      fi
    fi
  done

  git tag -f "${BACKUP_PREFIX}1" "$BACKUP_COMMIT" >/dev/null
fi

git push "$REMOTE" "$BRANCH"

if [ "$SHOULD_ROTATE" -eq 1 ]; then
  for i in $(seq 1 "$MAX_BACKUPS"); do
    TAG="${BACKUP_PREFIX}${i}"
    if git rev-parse -q --verify "refs/tags/${TAG}" >/dev/null; then
      git push "$REMOTE" "refs/tags/${TAG}" --force
    fi
  done
fi

echo "[OK] Synced branch '${BRANCH}' to '${REMOTE}'."
if [ "$SHOULD_ROTATE" -eq 1 ]; then
  echo "[OK] Rotating backups updated: ${BACKUP_PREFIX}1 ... ${BACKUP_PREFIX}${MAX_BACKUPS}"
fi
