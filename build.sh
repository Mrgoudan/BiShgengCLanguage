#!/usr/bin/env bash
#
# Build the BiSheng C user manual site (4 editions) into ./site.
#
# Designed to run BOTH in GitHub Actions and locally. It clones the book source
# fresh from the public gitee repo, so it needs no pre-existing local clone and
# no secrets.
#
#   zh release  (gitee bishengc/15.0.4)          -> site/
#   zh preview  (gitee bishengc/15.0.4-preview)  -> site/preview/
#   en release  (overlay/en/src)                 -> site/en/
#   en preview  (overlay/en-preview/src)         -> site/en/preview/
#
# Layout assumed: this script + overlay/ live together on the `book-ci` branch.
set -euo pipefail

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
OVERLAY="$HERE/overlay"
GITEE_BOOK="https://gitee.com/bisheng_c_language_dep/book.git"
GITEE_WEB="https://gitee.com/bisheng_c_language_dep/book"
SITE="$HERE/site"
SRC_CACHE=$(mktemp -d)
TODAY=$(date +%Y/%m/%d)

rm -rf "$SITE"; mkdir -p "$SITE"

# Clone the two source branches from gitee (shallow — we only need the tip tree).
git clone --quiet --depth 1 --branch bishengc/15.0.4 \
    "$GITEE_BOOK" "$SRC_CACHE/zh-main"
git clone --quiet --depth 1 --branch bishengc/15.0.4-preview \
    "$GITEE_BOOK" "$SRC_CACHE/zh-prev"

apply_overlay() {  # $1 work dir, $2 toml name
  cp "$OVERLAY/$2" "$1/book.toml"
  mkdir -p "$1/theme"
  cp "$OVERLAY/custom.css"     "$1/theme/custom.css"
  cp "$OVERLAY/lang-switch.js" "$1/theme/lang-switch.js"
}

attach_version_info() {  # $1 work, $2 src-clone (for git rev), $3 lang
  local work="$1" clone="$2" lang="$3" version short_version latest_date readme
  version=$(git -C "$clone" rev-parse HEAD)
  short_version=$(printf '%s' "$version" | cut -c1-7)
  latest_date=$(git -C "$clone" log -1 --date=format:%Y/%m/%d --format=%cd)
  readme="$work/src/README.md"
  local branch; branch=$(git -C "$clone" rev-parse --abbrev-ref HEAD)
  {
    echo ""
    if [ "$lang" = en ]; then
      echo "> Version: [$branch]($GITEE_WEB/tree/$branch/) - [$short_version]($GITEE_WEB/commit/$version)"
      echo ">"; echo "> Updated: $latest_date"; echo ">"; echo "> Released: $TODAY"
    else
      echo "> 版本说明：[$branch]($GITEE_WEB/tree/$branch/) - [$short_version]($GITEE_WEB/commit/$version)"
      echo ">"; echo "> 更新日期：$latest_date"; echo ">"; echo "> 发布日期：$TODAY"
    fi
  } >> "$readme"
}

attach_preview_link() {  # $1 work, $2 url, $3 lang, $4 dir(toprev|torel)
  local work="$1" url="$2" lang="$3" dir="$4" readme line body
  readme="$work/src/README.md"; body=$(cat "$readme")
  if [ "$lang" = en ]; then
    [ "$dir" = toprev ] \
      && line="> 📖 You are reading the **release** edition. A [**preview edition**]($url) documents upcoming, not-yet-released features." \
      || line="> 📖 You are reading the **preview** edition (upcoming, unreleased features). Go to the [**release edition**]($url)."
  else
    [ "$dir" = toprev ] \
      && line="> 📖 当前为**正式版**。[**预览版**]($url)包含即将发布、尚未正式发布的特性。" \
      || line="> 📖 当前为**预览版**（即将发布、尚未正式发布的特性）。前往[**正式版**]($url)。"
  fi
  printf '%s\n\n%s\n' "$line" "$body" > "$readme"
}

build_edition() {  # $1 src dir, $2 toml, $3 lang, $4 dest sub, $5 version-clone
  local src="$1" toml="$2" lang="$3" sub="$4" clone="$5" work
  work=$(mktemp -d)
  # search assets (assets/js/*) come from the zh release clone
  ( cd "$SRC_CACHE/zh-main" && git archive HEAD assets ) | tar -x -C "$work"
  cp -r "$src" "$work/src"
  apply_overlay "$work" "$toml"
  attach_version_info "$work" "$clone" "$lang"
  case "$sub" in
    ""|"en")               attach_preview_link "$work" "./preview/" "$lang" toprev ;;
    "preview"|"en/preview") attach_preview_link "$work" "../"        "$lang" torel ;;
  esac
  echo "[INFO] building $lang -> ${sub:-<root>}"
  mdbook build "$work" --dest-dir "$SITE${sub:+/$sub}"
  rm -rf "$work"
}

build_edition "$SRC_CACHE/zh-main/src" book.zh.toml zh ""           "$SRC_CACHE/zh-main"
build_edition "$SRC_CACHE/zh-prev/src" book.zh.toml zh "preview"    "$SRC_CACHE/zh-prev"
build_edition "$OVERLAY/en/src"         book.en.toml en "en"         "$SRC_CACHE/zh-main"
build_edition "$OVERLAY/en-preview/src" book.en.toml en "en/preview" "$SRC_CACHE/zh-prev"

touch "$SITE/.nojekyll"
rm -rf "$SRC_CACHE"
echo "[INFO] site built at $SITE"
