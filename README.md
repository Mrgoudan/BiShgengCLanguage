# book-ci — manual build & publish

This branch builds the **BiSheng C user manual** site and deploys it to GitHub Pages
via GitHub Actions. It is independent of the compiler branch (`bishengc/15.0.4`) and the
generated site branch (`gh-pages`).

## Contents

- `build.sh` — clones the book source from the public gitee repo, applies the overlay,
  and builds 4 mdBook editions into `./site`:
  | Edition | Source | URL path |
  |---|---|---|
  | 中文 release | gitee `bishengc/15.0.4` | `/` |
  | 中文 preview | gitee `bishengc/15.0.4-preview` | `/preview/` |
  | English release | `overlay/en/src` | `/en/` |
  | English preview | `overlay/en-preview/src` | `/en/preview/` |
- `overlay/` — GitHub-only additions kept out of the pristine gitee repo:
  - `book.zh.toml` / `book.en.toml` — themed mdBook config (Rust theme, dark ayu, MathJax, search)
  - `custom.css`, `lang-switch.js` — visual polish + 中文/EN toggle
  - `en/`, `en-preview/` — the English translation
  - `TRANSLATION_GUIDE.md` — glossary/rules used for the translation
- `.github/workflows/publish-manual.yml` — daily schedule + manual trigger; builds and
  deploys to Pages.

## Source of truth

The Chinese manual lives in gitee `bisheng_c_language_dep/book.git`. This branch never
pushes there; it only consumes it. To update the Chinese text, commit to gitee. To update
the English text or styling, edit `overlay/` here.

## Run locally

```sh
cargo install mdbook        # or use a prebuilt binary
./build.sh                  # outputs ./site
```
