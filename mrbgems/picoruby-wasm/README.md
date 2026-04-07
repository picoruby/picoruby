# picoruby-wasm

This mrbgem builds PicoRuby as WebAssembly, producing the [`@picoruby/wasm-wasi`](npm/picoruby/) npm package.
It uses the mruby VM to run Ruby in the browser with full JavaScript interoperability.

If you want to **use** PicoRuby.wasm rather than develop it, see the [npm package README](npm/picoruby/README.md).

## Architecture

PicoRuby.wasm uses a task suspension model instead of Emscripten's ASYNCIFY:

- Ruby tasks cooperatively yield to the JavaScript event loop (60fps via `requestAnimationFrame`)
- Async operations (setTimeout, fetch, addEventListener) suspend the current task, cleanly exiting the `setjmp` scope
- When the Promise resolves, the task resumes on a fresh C stack with a new `setjmp`
- Ruby exceptions therefore work correctly across async boundaries without ASYNCIFY overhead

See [docs/architecture.md](docs/architecture.md) for the full design including invariants that must be preserved.

## Documentation

- [Architecture](docs/architecture.md) - Task suspension model, exception safety, design invariants
- [Async Operations](docs/async_operations.md) - addEventListener, setTimeout, fetch patterns
- [Callbacks](docs/callback.md) - Callback system internals
- [JS-Ruby Interoperability](docs/interoperability_between_js_and_ruby.md) - Type conversion reference

## Prerequisites

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
- Ruby
- Rake

## Build

```bash
# Debug build
rake wasm:debug

# Release build
rake wasm:release

# Clean build artifacts
rake wasm:clean

# Start local dev server
rake wasm:server
# Open http://localhost:8080/www/
```

## Testing

Run the exception-handling test suite after any change to async/task/exception code:

```bash
rake wasm:debug
rake wasm:server
# Open http://localhost:8080/www/test_index.html
```

All tests must pass. If any test fails, an async/exception boundary invariant has been broken.
See [docs/architecture.md](docs/architecture.md) for what each invariant means.

## Project Structure

```
picoruby-wasm/
├── docs/                     # Design documentation
│   ├── architecture.md
│   ├── async_operations.md
│   ├── callback.md
│   └── interoperability_between_js_and_ruby.md
├── mrbgem.rake               # Build configuration (emcc flags)
├── src/mruby/
│   ├── js.c                  # JS interop: callbacks, timers, DOM events
│   └── wasm.c                # WASM init, main loop
├── mrblib/
│   └── js.rb                 # JS module (Ruby side)
├── demo/www/                 # Examples and test cases
└── npm/picoruby/             # NPM package
```

### Key Emscripten Flags

```
-s WASM=1                 # WebAssembly output
-s EXPORT_ES6=1           # ES6 module export
-s MODULARIZE=1           # Wrap in module
-s INITIAL_MEMORY=16MB    # Initial heap size
-s ALLOW_MEMORY_GROWTH=1  # Dynamic memory growth
-s ENVIRONMENT=web        # Browser target
```

`ASYNCIFY=1` is NOT used. The task suspension model eliminates the need for it,
keeping the WASM binary ~50% smaller than an ASYNCIFY build.

## Contributing

Before modifying async/exception handling code:

1. Read [docs/architecture.md](docs/architecture.md)
2. Make your changes
3. Run the full test suite (see Testing above)
4. Ensure all tests pass

## License

MIT

## See Also

- [PicoRuby](https://github.com/picoruby/picoruby)
- [mruby](https://github.com/mruby/mruby)
- [Emscripten](https://emscripten.org/)
