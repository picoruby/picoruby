# PicoRuby.wasm

PicoRuby.wasm runs Ruby in the browser. It uses the mruby VM compiled to WebAssembly.

## PicoRuby Version Support

|Tag or version      |Package                        |Build                 |
|:------------------:|:-----------------------------:|:--------------------:|
|latest (recommended)|@picoruby/wasm-wasi@latest     |production            |
|X.Y.Z               |@picoruby/wasm-wasi@X.Y.Z      |production            |
|X.Y.Z-debug         |@picoruby/wasm-wasi@X.Y.Z-debug|debug                 |
|debug               |@picoruby/wasm-wasi@debug      |latest versioned debug|
|head                |@picoruby/wasm-wasi@head       |production HEAD       |
|head-debug          |@picoruby/wasm-wasi@head-debug |debug HEAD            |

Use `@picoruby/wasm-wasi@latest` to get the most recent stable release. You can also omit the tag.
To debug your application with the Chrome extension PicoRuby Debugger, use a debug build such as `@picoruby/wasm-wasi@X.Y.Z-debug`.
The `@debug` tag points to the latest versioned debug build. The `@head-debug` tag points to the latest HEAD debug build.

All published packages expose the runtime from the `dist/` path. For `@latest`, `@head`, and versioned production packages, `dist/` contains the production build. For `@X.Y.Z-debug`, `@debug`, and `@head-debug`, `dist/` contains the debug build.

Maintainers build local artifacts with `rake wasm:prod` and `rake wasm:debug`. These tasks write to `npm/picoruby/dist` and `npm/picoruby/debug`. Use `rake wasm:npm:publish` to publish the versioned production and debug packages. Use `rake wasm:npm:publish_head` to publish the HEAD production and debug packages.

## Quick Start

Add the loader `<script>` tag. Then run Ruby in one of the three ways below:

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

If you use a bundler, install the package with npm:

```bash
npm install @picoruby/wasm-wasi
```

## JavaScript Interoperability

`JS.global` (window) and `JS.document` are the two entry points into the JavaScript world.

### Reading properties

```ruby
require 'js'

title  = JS.document[:title]                          #=> String
width  = JS.document.getElementById('box')[:offsetWidth] #=> Integer
hidden = JS.document.getElementById('box')[:hidden]   #=> true / false
items  = JS.document.querySelectorAll('.item').to_a   #=> Array of JS::Element
nav    = JS.global[:navigator]                        #=> JS::Object
```

The bridge converts JavaScript primitives to Ruby values automatically. You do not need to call `.to_s` or `.to_i`.

| JS value                 | Ruby value            |
|--------------------------|-----------------------|
| `string`                 | `String`              |
| `number`                 | `Integer` or `Float`  |
| `boolean`                | `true` / `false`      |
| `null` / `undefined`     | `nil`                 |
| object, array, function  | `JS::Object` wrapper  |

The bridge wraps a composite value as `JS::Object` or as one of its subclasses.
The subclass depends on the runtime type of the JavaScript value:

- `JS::Array` includes `Enumerable`.
- `JS::Function` has `#call`.
- `JS::Promise` has `#await` and `#then`.
- `JS::Element` wraps DOM elements and `document`.
- `JS::Event` has `#preventDefault` and `#stopPropagation`.
- `JS::Response` wraps a `Response` from `fetch`.

`NodeList` and `HTMLCollection` are not real arrays. The bridge wraps them as `JS::Object`. Use `#to_a` to convert them to a Ruby `Array`.

`JS::Object` inherits from `BasicObject`, not from `Object`. Because of this, the bridge forwards almost every method name to JavaScript. A method name becomes a property read, a method call, or a property write (for `name=`).
Only a small set of Ruby-side names is reserved:

- Conversion: `to_s`, `to_i`, `to_f`, `to_a`, `inspect`
- Operators: `==`, `[]`, `[]=`
- Predicates: `nil?`, `is_a?`, `kind_of?`, `instance_of?`, `respond_to?`
- Bridge helpers: `addEventListener`, `fetch`, `setTimeout`, `to_binary`, and others

Names such as `hash`, `send`, and `open` go to the JavaScript side. Any other name that ends in `?` or `!` raises `NoMethodError`. The bridge does not forward these names.
The [interoperability guide](https://github.com/picoruby/picoruby/blob/master/mrbgems/picoruby-wasm/docs/interoperability_between_js_and_ruby.md) gives the complete list.

### Writing properties and calling methods

```ruby
element = JS.document.getElementById('output')
element[:textContent] = "updated"
element.setAttribute('class', 'active')
element.focus
```

The bridge converts Ruby values to their JavaScript equivalents automatically. This applies to `String`, `Integer`, `Float`, `true`, `false`, `nil`, `Array`, and `Hash`.

Use `JS::Bridge.to_js` to pass a deeply nested structure to a JavaScript library:

```ruby
config = JS::Bridge.to_js({
  type: 'bar',
  data: { labels: ['Jan', 'Feb'], datasets: [{ data: [10, 20] }] }
})
JS.global[:Chart].new(canvas, config)
```

### Async operations

Each callback runs as a cooperative task. Multiple async operations can run at the same time. They do not block the browser.

```ruby
# setTimeout
JS.global.setTimeout(1000) do
  puts "one second later"
end

# fetch
JS.global.fetch('https://api.example.com/data') do |response|
  puts response[:status]  #=> Integer
end

# addEventListener
button = JS.document.getElementById('btn')
button.addEventListener('click') do |event|
  puts "clicked at #{event[:clientX]}, #{event[:clientY]}"
end
```

`rescue` and `ensure` catch a Ruby exception that occurs inside a callback.

### Promises

A JavaScript method that returns a Promise gives you a `JS::Promise`.
Call `await` to suspend the current Ruby task until the Promise resolves. `await` returns the resolved value.
Call `then` to pass the resolved value to a block.
If the Promise rejects, the bridge raises a Ruby exception.

```ruby
port = JS.global[:navigator][:serial].requestPort.await

JS.global[:navigator][:serial].requestPort.then do |port|
  # use port
end
```

`fetch` in the section above is a shorthand. It does the request and the await in one call.
`to_binary` reads the body of a `Blob`, a `File`, or a `Response`. It returns a binary Ruby `String` without UTF-8 conversion.

### Synchronous listeners

An event handler usually runs as a task after the browser has dispatched the event.
Pass `sync: true` to run the handler during dispatch instead. This is necessary in two cases:

- The handler calls `preventDefault` or `stopPropagation`.
- The handler uses an API that requires transient user activation. Examples are Web Serial, Web Bluetooth, `AudioContext#resume`, fullscreen, and clipboard.

```ruby
form.addEventListener('submit', sync: true) do |event|
  event.preventDefault
end
```

`addEventListener` also accepts `capture:`, `once:`, and `passive:`.
A synchronous handler cannot suspend. `fetch`, `await`, and a blocking queue read raise an exception inside it. The handler also blocks the main thread of the browser while it runs.
The handler can schedule work for later. `Task.new`, `setTimeout`, and an async listener are permitted.

## Debugging

Use the **PicoRuby Debugger** Chrome extension to inspect a running application. The extension gives you an interactive Ruby REPL, `binding.irb` breakpoints, a step debugger, and an inspector for local variables and the call stack.

The debugger requires a debug build:

```html
<!-- Use this during development instead of @latest -->
<script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@X.Y.Z-debug/dist/init.iife.js"></script>
```

The path still uses `dist/`. npm package entrypoints always come from `dist/`, and a debug package puts the debug artifacts there. Use `@head-debug` if you need the latest HEAD debug build.

The [Debugging guide](https://github.com/picoruby/picoruby/blob/master/mrbgems/picoruby-wasm/docs/debugging.md) gives the full setup instructions.

## License

MIT

Copyright © 2026 HASUMI Hitoshi.

## Links

- [Source and documentation](https://github.com/picoruby/picoruby/tree/master/mrbgems/picoruby-wasm)
- [PicoRuby](https://github.com/picoruby/picoruby)
