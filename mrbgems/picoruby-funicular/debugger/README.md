# Funicular DevTools Extension

A prototype Chrome/Edge extension for debugging Funicular applications in the browser.

## How to Install

1.  Open `chrome://extensions/` in Chrome/Edge.
2.  Enable "Developer mode".
3.  Click "Load unpacked".
4.  Select this directory (`debugger/`).

## How to Use

1.  Open a Funicular application.
2.  Open DevTools (F12).
3.  The "Funicular" tab will appear.
4.  Check debug information and execute code in the REPL.

## Architecture

```
DevTools Extension (JS)
    ↕ chrome.devtools.inspectedWindow.eval()
Web Page (HTML + JS)
    ↕ WASM Function Calls
PicoRuby VM (C + WASM)
```
