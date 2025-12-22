// Entrypoint of DevTools extension
// PicoRuby tab creation

chrome.devtools.panels.create(
  "PicoRuby",
  "",  // No icon
  "panel.html",
  (panel) => {
    console.log("PicoRuby DevTools panel created");
  }
);
