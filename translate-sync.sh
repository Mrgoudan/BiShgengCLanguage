#!/usr/bin/env bash
#
# Keep the English manual in sync with the Chinese source using AI translation.
#
# For each edition (release / preview):
#   1. read the baseline SHA the English was last translated from (overlay/<ed>/.translated-from)
#   2. clone the current gitee Chinese branch
#   3. git diff baseline..current -- src/  -> list of changed/added/deleted .md files
#   4. for each changed file, call the Claude API to translate zh -> en, following
#      overlay/TRANSLATION_GUIDE.md, and overwrite the matching overlay/<ed>/src file
#      (deletes are mirrored)
#   5. advance the baseline marker to the current SHA
#
# Outputs (for the workflow): writes "changed=N" to $GITHUB_OUTPUT if set.
# Requires: ANTHROPIC_API_KEY in the environment, curl, jq, git.
#
# Safe no-op when nothing changed. If ANTHROPIC_API_KEY is unset, exits 0 without
# doing anything (so the pipeline degrades gracefully until the secret is added).
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OVERLAY="$HERE/overlay"
GUIDE="$OVERLAY/TRANSLATION_GUIDE.md"
GITEE="https://gitee.com/bisheng_c_language_dep/book.git"
MODEL="${ANTHROPIC_MODEL:-claude-opus-4-8}"

# edition -> gitee branch
declare -A BRANCH=(
  [en]="bishengc/15.0.4"
  [en-preview]="bishengc/15.0.4-preview"
)

if [ -z "${ANTHROPIC_API_KEY:-}" ]; then
  echo "[skip] ANTHROPIC_API_KEY not set — leaving English unchanged."
  [ -n "${GITHUB_OUTPUT:-}" ] && echo "changed=0" >> "$GITHUB_OUTPUT"
  exit 0
fi

GUIDE_TEXT=$(cat "$GUIDE")
TOTAL_CHANGED=0
TMP=$(mktemp -d)

# translate one Chinese markdown file -> English, print result to stdout
translate_file() {
  local zh_path="$1"
  local sys="You are translating one page of the BiSheng C language user manual from \
Chinese to English. Follow this translation guide EXACTLY:

$GUIDE_TEXT

Output ONLY the translated Markdown for this single file — no preamble, no code fence \
around the whole thing, no commentary. Preserve all Markdown structure, heading levels \
and their section numbers, tables, links, and code blocks; translate only prose, \
headings, table text, and code comments. Keep all BiSheng C keywords, identifiers, and \
file names verbatim."
  local user; user=$(cat "$zh_path")
  # build request with jq to safely encode content
  jq -n --arg model "$MODEL" --arg sys "$sys" --arg user "$user" '{
    model: $model,
    max_tokens: 16000,
    system: $sys,
    messages: [ { role: "user", content: $user } ]
  }' > "$TMP/req.json"
  curl -sS https://api.anthropic.com/v1/messages \
    -H "x-api-key: $ANTHROPIC_API_KEY" \
    -H "anthropic-version: 2023-06-01" \
    -H "content-type: application/json" \
    --data @"$TMP/req.json" > "$TMP/resp.json"
  # surface API errors instead of writing garbage
  if jq -e '.error' "$TMP/resp.json" >/dev/null 2>&1; then
    echo "[error] API: $(jq -r '.error.message' "$TMP/resp.json")" >&2
    return 1
  fi
  jq -r '.content[] | select(.type=="text") | .text' "$TMP/resp.json"
}

for ED in en en-preview; do
  BR="${BRANCH[$ED]}"
  BASE_FILE="$OVERLAY/$ED/.translated-from"
  BASE=$(tr -d '[:space:]' < "$BASE_FILE")
  SRC="$TMP/$ED-src"
  git clone --quiet --branch "$BR" --single-branch "$GITEE" "$SRC"
  CUR=$(git -C "$SRC" rev-parse HEAD)

  if [ "$BASE" = "$CUR" ]; then
    echo "[$ED] up to date ($CUR) — nothing to translate."
    continue
  fi
  echo "[$ED] baseline $BASE -> current $CUR"

  # changed/added/deleted .md under src/ between baseline and current
  if git -C "$SRC" cat-file -e "$BASE" 2>/dev/null; then
    mapfile -t DIFF < <(git -C "$SRC" diff --name-status "$BASE" "$CUR" -- 'src/*.md')
  else
    # baseline not in this clone's history (shallow/unknown) — translate all src files
    echo "[$ED] baseline not found in history; retranslating all pages."
    mapfile -t DIFF < <(cd "$SRC" && find src -name '*.md' | sed 's/^/A\t/')
  fi

  for entry in "${DIFF[@]}"; do
    [ -z "$entry" ] && continue
    status=$(printf '%s' "$entry" | cut -f1)
    path=$(printf '%s' "$entry" | cut -f2)
    dest="$OVERLAY/$ED/$path"
    case "$status" in
      D*)
        rm -f "$dest"; echo "  [del] $path"; TOTAL_CHANGED=$((TOTAL_CHANGED+1)) ;;
      A*|M*|R*)
        echo "  [tr ] $path"
        mkdir -p "$(dirname "$dest")"
        if translate_file "$SRC/$path" > "$dest.new"; then
          mv "$dest.new" "$dest"; TOTAL_CHANGED=$((TOTAL_CHANGED+1))
        else
          rm -f "$dest.new"; echo "  [FAIL] $path (kept old)" >&2
        fi ;;
    esac
  done

  echo "$CUR" > "$BASE_FILE"   # advance baseline
done

rm -rf "$TMP"
echo "[done] files changed: $TOTAL_CHANGED"
[ -n "${GITHUB_OUTPUT:-}" ] && echo "changed=$TOTAL_CHANGED" >> "$GITHUB_OUTPUT"
exit 0
