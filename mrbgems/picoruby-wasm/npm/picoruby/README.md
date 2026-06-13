# PicoRuby.wasm

Run Ruby in the browser. Powered by the mruby VM compiled to WebAssembly.

## PicoRuby Version Support

|Tag or version      |Package                        |Build                 |
|:------------------:|:-----------------------------:|:--------------------:|
|latest (recommended)|@picoruby/wasm-wasi@latest     |production            |
|X.Y.Z               |@picoruby/wasm-wasi@X.Y.Z      |production            |
|X.Y.Z-debug         |@picoruby/wasm-wasi@X.Y.Z-debug|debug                 |
|debug               |@picoruby/wasm-wasi@debug      |latest versioned debug|
|head                |@picoruby/wasm-wasi@head       |production HEAD       |
|head-debug          |@picoruby/wasm-wasi@head-debug |debug HEAD            |

Use `@picoruby/wasm-wasi@latest` (or omit the tag entirely) to always get the most recent stable release.
If you want to debug your application with Chrome extension PicoRuby Debugger, use a debug build such as `@picoruby/wasm-wasi@X.Y.Z-debug`. The `@debug` tag points to the latest versioned debug build, and `@head-debug` points to the latest HEAD debug build.

All published packages expose the runtime from the `dist/` path. For `@latest`, `@head`, and versioned production packages, `dist/` contains the production build. For `@X.Y.Z-debug`, `@debug`, and `@head-debug`, `dist/` contains the debug build.

Maintainers build local artifacts with `rake wasm:prod` and `rake wasm:debug`. These write to `npm/picoruby/dist` and `npm/picoruby/debug` respectively. Publishing is handled by `rake wasm:npm:publish` for versioned production+debug packages, or `rake wasm:npm:publish_head` for HEAD production+debug packages.

## Quick Start

Add the loader `<script>` tag, then choose one of the following ways to run Ruby:

```html
<script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@latest/dist/init.iife.js"></script>

<!-- Embedded Ruby Script -->
<script type="text/ruby">
  puts "Hello, World!"
</script>

<!-- Remote Ruby Script file (.rb) -->
<script type="text/ruby" src="hello.rb"></script>

<!-- Remote Precompiled Ruby VM Code file (.mrb) -->
<script type="application/x-mrb" src="hello.mrb"></script>
```

## Installation (npm)

For bundler-based workflows:

```bash
npm install @picoruby/wasm-wasi
```

## JavaScript Interoperability

`JS.global` (window) and `JS.document` are the two entry points into the JavaScript world.

### Reading properties

```ruby
require 'js'

title  = JS.document[:title].to_s
width  = JS.document.getElementById('box')[:offsetWidth].to_i
items  = JS.document.querySelectorAll('.item').to_a
nav    = JS.global[:navigator]
```

Property access returns a `JS::Object`. Convert with `.to_s`, `.to_i`, `.to_f`, or `.to_a`.
`null` and `undefined` become `nil` automatically.

### Writing properties and calling methods

```ruby
element = JS.document.getElementById('output')
element[:textContent] = "updated"
element.setAttribute('class', 'active')
element.focus
```

Ruby values (String, Integer, Float, true/false, nil, Array, Hash) are auto-converted
to their JavaScript equivalents.

For deeply nested structures passed to JS libraries, use `JS::Bridge.to_js`:

```ruby
config = JS::Bridge.to_js({
  type: 'bar',
  data: { labels: ['Jan', 'Feb'], datasets: [{ data: [10, 20] }] }
})
JS.global[:Chart].new(canvas, config)
```

### Async operations

Callbacks run as cooperative tasks. Multiple async operations can run concurrently
without blocking the browser.

```ruby
# setTimeout
JS.global.setTimeout(1000) do
  puts "one second later"
end

# fetch
JS.global.fetch('https://api.example.com/data') do |response|
  puts response[:status].to_i
end

# addEventListener
button = JS.document.getElementById('btn')
button.addEventListener('click') do |event|
  puts "clicked at #{event[:clientX].to_i}, #{event[:clientY].to_i}"
end
```

Ruby exceptions raised inside callbacks are caught correctly by `rescue`/`ensure`.

## Debugging

Use the **PicoRuby Debugger** Chrome extension to inspect running applications
with an interactive Ruby REPL, `binding.irb` breakpoints, a step debugger,
and a local-variable/call-stack inspector.

The debugger requires a debug build:

```html
<!-- Use this during development instead of @latest -->
<script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@X.Y.Z-debug/dist/init.iife.js"></script>
```

The path still uses `dist/` because npm package entrypoints are always published from `dist/`; debug packages put debug artifacts there. Use `@head-debug` when you need the latest HEAD debug build.

For full setup instructions see the
[Debugging guide](https://github.com/picoruby/picoruby/blob/master/mrbgems/picoruby-wasm/docs/debugging.md).

## License

MIT

Copyright © 2026 HASUMI Hitoshi.

## Links

- [Source and documentation](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-wasm)
- [PicoRuby](https://github.com/picoruby/picoruby)
