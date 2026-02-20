// Entrypoint of DevTools extension
// PicoRuby tab creation

chrome.devtools.panels.create(
  "PicoRuby",
  "icons/icon16.png",
  "panel.html",
  (panel) => {
    console.log("PicoRuby DevTools panel created");
  }
);
