# Debugging PicoRuby.wasm Applications

PicoRuby.wasm ships with a Chrome DevTools extension that provides an
interactive Ruby REPL, `binding.irb` breakpoints, a step debugger, and a
local-variable/call-stack inspector.

## Requirements

- Chrome 102 or later
- A page loaded from the **`@debug` dist-tag** of `@picoruby/wasm-wasi`

The debug API (`mrb_debug_*`) is compiled only into debug builds.
Production builds (`@latest`) do not include it.
When the extension detects a release build it shows an error and the REPL
remains inactive.

## Installation

Install from the Chrome Web Store:
**[PicoRuby Debugger — Chrome Web Store](PLACEHOLDER)**

Or load unpacked for local development:

1. Open `chrome://extensions`
2. Enable **Developer mode**
3. Click **Load unpacked** and select `mrbgems/picoruby-wasm/debugger/`

## Using the debug package

Switch your page to the `@debug` dist-tag. No local build is required.

```html
<!-- CDN (development only) -->
<script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@debug/dist/init.iife.js"></script>
```

```js
// npm
import { loadPicoRuby } from '@picoruby/wasm-wasi@debug';
```

Switch back to `@latest` before deploying to production.

To publish a new debug build from source:

```bash
rake wasm:release_debug
```

## Breakpoints with `binding.irb`

Call `binding.irb` anywhere in Ruby code to pause execution at that point:

```ruby
def calculate(x, y)
  binding.irb   # execution suspends here
  x + y
end
```

When execution reaches `binding.irb` the task is suspended, the DevTools
panel gains focus, and the REPL prompt changes to `irb(debug):NNN>`.

## Debug commands

The following commands are accepted in the REPL while paused:

| Command | Shorthand | Description |
|---|---|---|
| `continue` | `c` | Resume execution |
| `step` | `s` | Step into the next expression |
| `next` | `n` | Step over to the next line |
| `help` | `h` | Show command list |

Keyboard shortcuts:

| Key | Action |
|---|---|
| F8 | Continue |
| F10 | Step over |
| F11 | Step into |

## REPL

The REPL is available at all times, not only when paused. Expressions are
evaluated in the top-level context normally, or in the current binding context
when paused at a `binding.irb`.

```
irb:001> 1 + 1
=> 2
irb:002> MyClass.instance_methods(false)
=> [:foo, :bar]
```

Input history is navigable with the up/down arrow keys.
The **Copy** button on each entry copies the input and output together.

## Local variables and call stack

When paused, the right-hand sidebar shows:

- **Local Variables** — all locals visible in the current scope, updated after
  each REPL evaluation
- **Call Stack** — the Ruby call stack from the pause point; the active frame
  is highlighted

## Funicular component inspector

When the page uses [Funicular](https://picoruby.org/funicular) with the debug
global enabled, a **Components** panel appears alongside the REPL:

```ruby
$__funicular_debug__ = true
```

The panel shows the live component tree. Clicking a component opens the
**Inspector** showing its `state` hash and instance variables. The tree
refreshes automatically every 500 ms.

## How the debug API works

The extension communicates with the page exclusively through
`chrome.devtools.inspectedWindow.eval`, which executes JavaScript in the
context of the inspected page. No background service worker or content script
is involved.

The JavaScript calls into the WASM module via `window.picorubyModule.ccall`:

| C function | Purpose |
|---|---|
| `mrb_debug_get_status` | Poll pause state (mode, file, line, pause_id) |
| `mrb_eval_string` | Evaluate Ruby in top-level context |
| `mrb_debug_eval_in_binding` | Evaluate Ruby in paused binding |
| `mrb_debug_get_locals` | Return local variables as JSON |
| `mrb_debug_get_callstack` | Return call stack frames as JSON |
| `mrb_debug_continue` | Resume execution |
| `mrb_debug_step` | Step into |
| `mrb_debug_next` | Step over |
| `mrb_get_component_debug_info` | Funicular component tree |
| `mrb_get_component_state_by_id` | Funicular component state |

The extension polls `mrb_debug_get_status` every 200 ms to detect pause events.
