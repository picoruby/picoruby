# PicoRuby.wasm

Run Ruby in the browser. Powered by the mruby VM compiled to WebAssembly.

## PicoRuby Version Support

|Version|Package|
|:-----:|:-----:|
|debug|@picoruby/wasm-wasi@debug|
|head|@picoruby/wasm-wasi@head|
|3.4.5|@picoruby/wasm-wasi@3.4.5|
|0.9.6|@picoruby/wasm-wasi@0.9.6|

If you want to debug your application with Chrome extension PicoRuby Debugger, you need to use `@picoruby/wasm-wasi@debug`.

## Quick Start

Add one `<script>` tag and write Ruby directly in your HTML:

```html
<!DOCTYPE html>
<html>
<head>
  <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi/dist/init.iife.js"></script>
  <!-- If you want to specify a version: @picoruby/wasm-wasi@3.4.5/dist/init.iife.js -->
</head>
<body>
  <script type="text/ruby">
    require 'js'

    JS.document.getElementById('output')[:textContent] = "Hello from Ruby!"
  </script>

  <div id="output"></div>
</body>
</html>
```

Load an external Ruby file:

```html
<script type="text/ruby" src="app.rb"></script>
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

The debugger requires the `@debug` dist-tag build:

```html
<!-- Use this during development instead of @latest -->
<script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@debug/dist/init.iife.js"></script>
```

For full setup instructions see the
[Debugging guide](https://github.com/picoruby/picoruby/blob/master/mrbgems/picoruby-wasm/docs/debugging.md).

## License

MIT

Copyright © 2026 HASUMI Hitoshi.

## Links

- [Source and documentation](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-wasm)
- [PicoRuby](https://github.com/picoruby/picoruby)
