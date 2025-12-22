# PicoRuby DevTools Extension

A prototype Chrome/Edge extension for debugging PicoRuby applications in the browser.

## How to Install

1.  Open `chrome://extensions/` in Chrome/Edge.
2.  Enable "Developer mode".
3.  Click "Load unpacked".
4.  Select this directory (`debugger/`).

## How to Use

1.  Open a PicoRuby WASM application.
2.  Open DevTools (F12).
3.  The "PicoRuby" tab will appear.
4.  Check debug information and execute code in the REPL.

## Implementation Status

### ✅ Implemented
- Basic UI for the DevTools tab
- REPL interface
- Call stack display (mock data)
- Variable display (mock data)

### 🚧 Implementation Required (WASM C side)

The following C functions need to be added to `mrbgems/picoruby-wasm/src/mruby/wasm.c`:

```c
// Get backtrace
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_backtrace_json(void);

// Get local variables
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_local_variables_json(void);

// Get global variables
EMSCRIPTEN_KEEPALIVE
const char* mrb_get_globals_json(void);

// Execute code
EMSCRIPTEN_KEEPALIVE
const char* mrb_eval_string(const char* code);

// Debug control
EMSCRIPTEN_KEEPALIVE
void mrb_debug_pause(void);

EMSCRIPTEN_KEEPALIVE
void mrb_debug_resume(void);

// Set breakpoint
EMSCRIPTEN_KEEPALIVE
void mrb_set_breakpoint(const char* file, int line);
```

### 📋 Next Steps

1.  **Phase 1**: Basic inspection features
    - Implement `mrb_get_globals_json()`
    - Implement `mrb_eval_string()`

2.  **Phase 2**: Call stack and local variables
    - Implement `mrb_get_backtrace_json()`
    - Implement `mrb_get_local_variables_json()`

3.  **Phase 3**: Breakpoints
    - Enable `MRB_USE_DEBUG_HOOK`
    - Implement `code_fetch_hook`
    - Notification from WASM to DevTools

## Architecture

```
DevTools Extension (JS)
    ↕ chrome.devtools.inspectedWindow.eval()
Web Page (HTML + JS)
    ↕ WASM Function Calls
PicoRuby VM (C + WASM)
```

## File Structure

```
debugger/
├── manifest.json      # Extension definition
├── devtools.html      # DevTools entry point
├── devtools.js        # Tab creation
├── panel.html         # Debugger UI
├── panel.js           # Debugger logic
└── README.md          # This file
```

## Development Notes

- The `evalInPage()` method in `panel.js` is used to call WASM functions.
- Currently, it returns mock data, so C-side implementation is required.
- `chrome.devtools.inspectedWindow.eval()` is executed in the page context.
- Notifications from WASM to DevTools can be achieved with `postMessage()` + `chrome.runtime.sendMessage()`.