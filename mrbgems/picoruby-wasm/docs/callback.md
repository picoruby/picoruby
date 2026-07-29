# Callbacks in PicoRuby.wasm

Three callback types exist, with fundamentally different execution models:

| Type | How Called | Blocks JS? | Can sleep_ms? |
|------|-----------|-----------|---------------|
| **Async** (addEventListener, setTimeout, await/then) | Via scheduler as a new task | No | Yes |
| **Sync listener** (addEventListener with `sync: true`) | Synchronously from JS, on the dispatch stack | Yes | No |
| **Generic** (register_callback) | Synchronously from JS | Yes | No |

## Async Callbacks

Used for all event-driven and Promise-based patterns. Each fires in a new Ruby task; the JavaScript event loop is never blocked.

```ruby
# DOM event
button.addEventListener('click') do |event|
  puts event[:type].to_s
end

# Timer
JS.global.setTimeout(2000) { puts "fired" }

# Promise (await / then)
JS.global[:navigator][:serial].requestPort().then do |port|
  # use port
end
```

Async callbacks may use `sleep_ms` and other task-switching operations.

To remove an event listener:

```ruby
callback_id = button.addEventListener('click') { ... }
JS::Object.removeEventListener(callback_id)
```

For convenience, an asynchronous listener automatically calls `preventDefault()` on
`submit` events and on `click` events targeting an `<a>` tag, so Ruby handlers can act
as SPA navigation handlers. This does not apply to synchronous listeners, which call
`preventDefault` themselves.

## Synchronous Listeners

`addEventListener` accepts `sync: true`, plus the DOM listener options `capture:`,
`once:` and `passive:`. A synchronous listener runs the block *on the JavaScript event
dispatch stack*, before `dispatchEvent` returns.

```ruby
input.addEventListener('keydown', sync: true) do |event|
  event.preventDefault if event[:key].to_s == 'ArrowUp'
end

# passive is browser-default unless given; touch and wheel listeners need it
# explicitly disabled to be able to preventDefault
canvas.addEventListener('touchstart', sync: true, passive: false) do |event|
  event.preventDefault
end
```

Use it when the handler needs any of:

- `preventDefault` / `stopPropagation` — an async handler runs after dispatch has
  finished, so both are no-ops there
- transient user activation — `navigator.serial.requestPort`,
  `navigator.bluetooth.requestDevice`, `AudioContext#resume`, `requestFullscreen`,
  clipboard writes, `window.open`
- `event.currentTarget` / `event.composedPath()`, which the DOM invalidates once
  dispatch ends

**Constraints.** The block cannot suspend, so anything that would raises
`RuntimeError` inside it:

- `fetch`, `Promise#await` / `#then`, `JS::Response#to_binary`
- blocking `Task::Queue#pop`
- `Task#join` / `#suspend` / `#resume` / `#terminate`

Merely *scheduling* work is fine: `Task.new`, `setTimeout` and registering an
asynchronous `addEventListener` all work from inside a synchronous handler; the
spawned block simply runs later, on the scheduler.

`sleep` / `sleep_ms` do not raise, but block the browser's main thread in real time.
Keep synchronous handlers short: they run inside the JavaScript event loop, so any
work done there delays rendering and input, exactly as a JS handler would.

The `JS::Event` passed to the block is released when the handler returns; holding on
to it and reading it later yields `nil`.

## Generic Callbacks

Registered once; callable from JS synchronously. Used when a JS library expects a direct return value (e.g., Chart.js formatters).

```ruby
JS::Object.register_callback('formatYen') do |value|
  "Y#{value.to_i}"
end
```

```javascript
// JS side - blocks until Ruby returns
const label = globalThis.picorubyGenericCallbacks['formatYen'](1234);
```

Generic callbacks run through the same synchronous dispatch as `sync: true` listeners,
so the same constraints apply: **no `fetch` / `await` / blocking `Task::Queue#pop`**
(they raise `RuntimeError`), and `sleep` blocks the browser's main thread in real time.

### Avoid High-Frequency Generic Callbacks

Some libraries call callbacks on every render frame. Prefer precomputed values:

```ruby
# BAD: called on every render
chart_data = { datasets: [{ backgroundColor: JS.generic_callbacks[:barColor] }] }

# GOOD: compute once, pass as static array
colors = data.map { |v| color_for(v) }
chart_data = { datasets: [{ backgroundColor: colors }] }
```

## See Also

- [async_operations.md](async_operations.md) - await, then, fetch, timers
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) - Type conversion
