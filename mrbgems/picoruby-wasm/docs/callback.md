# Callbacks in PicoRuby.wasm

Two callback types exist, with fundamentally different execution models:

| Type | How Called | Blocks JS? | Can sleep_ms? |
|------|-----------|-----------|---------------|
| **Async** (addEventListener, setTimeout, await/then) | Via scheduler as a new task | No | Yes |
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

**Do not use `sleep_ms` or any task-switching inside generic callbacks** - it will raise an error.

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
