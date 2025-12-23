// Entrypoint of DevTools extension
// PicoRuby tab creation

chrome.devtools.panels.create(
  "Funicular",
  "icons/icon16.png",
  "panel.html",
  (panel) => {
    console.log("Funicular DevTools panel created");
  }
);
