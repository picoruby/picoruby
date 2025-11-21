# MicroRuby.wasm

MicroRuby WebAssembly - Ruby for the browser powered by mruby VM

## What is MicroRuby?

MicroRuby is a WebAssembly build of [PicoRuby](https://github.com/picoruby/picoruby) using the full **mruby VM** (as opposed to PicoRuby.wasm which uses mruby/c).

### Key Features

- **Full mruby VM**: Complete Ruby implementation with rich features
- **Task Scheduler**: Built-in cooperative multitasking for concurrent Ruby code
- **JavaScript Interop**: Seamless integration with JavaScript APIs
- **Browser Compatible**: Runs directly in modern web browsers
- **Same API as PicoRuby**: Consistent interface across VM variants

## Installation

```bash
npm install @microruby/wasm
```

## Usage

### HTML (IIFE)

```html
<!DOCTYPE html>
<html>
<head>
  <script type="module" src="node_modules/@microruby/wasm/dist/init.iife.js"></script>
</head>
<body>
  <script type="text/ruby">
    puts "Hello from MicroRuby!"

    # JavaScript interop
    js_global = JS.global
    document = js_global[:document]
    document.getElementById("output").innerText = "Hello from Ruby!"
  </script>

  <div id="output"></div>
</body>
</html>
```

### Loading External Ruby Files

```html
<script type="text/ruby" src="app.rb"></script>
```

### JavaScript API

```javascript
// Initialize manually
await window.initMicroRuby();

// Access the module
const Module = window.Module;

// Execute Ruby code
Module.ccall('picorb_create_task', 'number', ['string'], ['puts "Hello!"']);
```

## Differences from PicoRuby.wasm

| Feature | PicoRuby (mruby/c) | MicroRuby (mruby) |
|---------|-------------------|-------------------|
| VM | mruby/c (compact) | mruby (full-featured) |
| Size | ~780KB | ~1.6MB |
| Performance | Faster startup | Richer features |
| Memory | Lower footprint | More memory needed |
| API | Same | Same |

## Architecture

MicroRuby uses **explicit execution loop model**:

```
JavaScript (60fps)
  ├─ mrb_tick_wasm()  → Timer processing & task wakeup
  └─ mrb_run_step()   → Execute one task step
```

This architecture:
- ✅ Provides fine-grained control from JavaScript
- ✅ Enables easy debugging and profiling
- ✅ Consistent with PicoRuby.wasm interface
- ✅ Cooperative multitasking via Task Scheduler

## Development

### Building

```bash
# Build debug version
rake wasm:microruby:debug

# Build production version
rake wasm:microruby:release

# Start local server
rake wasm:microruby:server
```

### Testing

Visit http://localhost:8080 after starting the server.

## License

MIT

## Links

- [PicoRuby](https://github.com/picoruby/picoruby)
- [mruby](https://github.com/mruby/mruby)
