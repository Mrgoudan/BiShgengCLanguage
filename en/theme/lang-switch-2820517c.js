// Adds a 中文 / EN language toggle to the mdBook menu bar.
//
// Site layout on GitHub Pages:
//   <base>/...          -> Chinese book (root)
//   <base>/en/...       -> English book
//   <base>/preview/...  -> Chinese preview variant (toggle hidden there)
//
// The toggle keeps you on the same page when switching, when that page exists
// in the other language (same relative path); otherwise it lands on the other
// book's home page.
(function () {
  "use strict";

  // path_to_root is emitted by mdBook for every page (relative path back to the
  // book root, e.g. "" or "../"). We use it to locate this book's root URL.
  function bookRoot() {
    var p2r = (typeof path_to_root === "string") ? path_to_root : "";
    var url = new URL(p2r || ".", window.location.href);
    var path = url.pathname;
    if (!path.endsWith("/")) path += "/";
    return path; // e.g. /BiShgengCLanguage/  or  /BiShgengCLanguage/en/
  }

  function compute() {
    var root = bookRoot();
    var rel = window.location.pathname.slice(root.length); // page path within this book

    // Determine current language from the book root's last path segment.
    var isEn = /\/en\/$/.test(root);
    var isPreview = /\/preview\/$/.test(root);
    if (isPreview) return null; // no toggle on the preview build

    var otherRoot, label;
    if (isEn) {
      otherRoot = root.replace(/\/en\/$/, "/"); // en -> zh (drop /en)
      label = "中文";
    } else {
      otherRoot = root + "en/";                 // zh -> en
      label = "EN";
    }
    return { href: otherRoot + rel, home: otherRoot, label: label };
  }

  window.addEventListener("DOMContentLoaded", function () {
    var info = compute();
    if (!info) return;
    var rb = document.querySelector(".right-buttons");
    if (!rb) return;
    var a = document.createElement("a");
    a.href = info.href;
    a.className = "icon-button lang-switch";
    a.title = "Switch language / 切换语言";
    a.setAttribute("aria-label", "Switch language");
    a.textContent = info.label;
    // If the twin page 404s, fall back to the other book's home page.
    a.addEventListener("click", function (e) {
      e.preventDefault();
      fetch(info.href, { method: "HEAD" })
        .then(function (r) { window.location.href = r.ok ? info.href : info.home; })
        .catch(function () { window.location.href = info.home; });
    });
    rb.appendChild(a);
  });
})();
