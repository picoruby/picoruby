// Entrypoint of DevTools extension
// PicoRuby tab creation

chrome.devtools.panels.create(
  "PicoRuby",
  "icons/icon16.png",
  "panel.html",
  (panel) => {
    console.log("PicoRuby.WASM DevTools panel created");
  }
);
