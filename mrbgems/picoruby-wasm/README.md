# picoruby-wasm

PicoRuby for WebAssembly - Ruby runtime in the browser powered by mruby VM.

## Overview

This mrbgem provides WebAssembly bindings for PicoRuby, enabling Ruby code to run in web browsers with seamless JavaScript interoperability.

```ruby
require 'js'

# Access DOM
button = JS.document.getElementById('myButton')

# Add event listener
button.addEventListener('click') do |event|
  puts "Button clicked at #{event[:clientX]}, #{event[:clientY]}"
end

# Schedule delayed execution
JS.global.setTimeout(2000) do
  puts "Timer fired!"
end

# Make HTTP requests
JS.global.fetch('https://api.example.com/data') do |response|
  puts "Status: #{response[:status]}"
end
```

## Quick Start

### Using in HTML

```html
<!DOCTYPE html>
<html>
<head>
  <title>PicoRuby.wasm Example</title>
</head>
<body>
  <button id="myButton">Click me!</button>

  <script type="text/ruby">
    require 'js'

    button = JS.document.getElementById('myButton')
    button.addEventListener('click') do |event|
      event.target[:textContent] = 'Clicked!'
    end
  </script>

  <script src="init.iife.js"></script>
</body>
</html>
```

### Using as NPM Package

```bash
npm install picoruby-wasm
```

```javascript
import init from 'picoruby-wasm';

init().then(picoruby => {
  picoruby.evalRubyCode(`
    puts "Hello from Ruby!"
  `);
});
```

## Key Features

### Task-Based Execution
- Cooperative multitasking integrated with JavaScript event loop
- Multiple Ruby tasks can run concurrently
- No ASYNCIFY overhead (50% smaller WASM file)

### Seamless JavaScript Interop
- Call JavaScript functions from Ruby
- Access DOM APIs naturally
- Automatic type conversion between Ruby and JavaScript

### Exception Safety
- Ruby exceptions work correctly across async boundaries
- No need for ASYNCIFY=1 flag
- Comprehensive test suite validates exception handling

## Documentation

### Core Concepts
- **[Architecture](docs/architecture.md)** - Task-based execution model and exception safety
- **[Async Operations](docs/async_operations.md)** - addEventListener, setTimeout, fetch patterns
- **[Callbacks](docs/callback.md)** - How callbacks work in PicoRuby.wasm
- **[JS-Ruby Interoperability](docs/interoperability_between_js_and_ruby.md)** - Data conversion and type handling

### API Reference
- **[JS::Object Guide](docs/js_object_guide.md)** - Complete guide to JS::Object API (TODO)

### Examples
See `demo/www/` for working examples:
- `dom.html` - DOM manipulation
- `timeout.html` - setTimeout/clearTimeout
- `websocket.html` - WebSocket client
- `multitasks.html` - Concurrent tasks
- And many more...

## Building from Source

### Prerequisites
- Emscripten SDK (emsdk)
- Ruby
- Rake

### Build Commands

```bash
# Build WASM (debug mode)
rake wasm:debug

# Build WASM (release mode)
rake wasm:release

# Clean build artifacts
rake wasm:clean

# Run development server
rake wasm:server
# Open http://localhost:8080/www/
```

## Testing

Run the comprehensive test suite to validate exception handling:

```bash
rake wasm:debug
rake wasm:server
# Open http://localhost:8080/www/test_index.html
```

All tests must pass to ensure async boundary safety.

## Build Configuration

Key Emscripten flags (see `mrbgem.rake`):

```ruby
-s WASM=1                      # WebAssembly output
-s EXPORT_ES6=1                # ES6 module export
-s MODULARIZE=1                # Wrap in module
-s INITIAL_MEMORY=16MB         # Initial heap size
-s ALLOW_MEMORY_GROWTH=1       # Dynamic memory
-s ENVIRONMENT=web             # Browser target
```

**Note**: `ASYNCIFY=1` is NOT used. Our task suspension model eliminates the need for it, saving 50%+ in code size.

## Project Structure

```
picoruby-wasm/
├── README.md                # This file
├── docs/                    # Detailed documentation
│   ├── architecture.md
│   ├── async_operations.md
│   ├── callback.md
│   └── interoperability_between_js_and_ruby.md
├── mrbgem.rake             # Build configuration
├── src/
│   └── mruby/
│       ├── js.c            # JavaScript interop
│       └── wasm.c          # WASM initialization
├── mrblib/                 # Ruby code
│   └── js.rb              # JS module
├── demo/
│   └── www/               # Examples and tests
└── npm/                   # NPM package
```

## Contributing

Contributions are welcome! When modifying async/exception handling code:

1. Read [docs/architecture.md](docs/architecture.md) for design principles
2. Make your changes
3. Run the full test suite
4. Ensure all tests pass

## License

MIT

## See Also

- [PicoRuby](https://github.com/picoruby/picoruby) - The main PicoRuby project
- [mruby](https://github.com/mruby/mruby) - The Ruby VM used by PicoRuby
- [Emscripten](https://emscripten.org/) - Compiler toolchain for WebAssembly
