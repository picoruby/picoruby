# Asynchronous Operations in PicoRuby.wasm

| Pattern | Use Case | Blocks Task? | Repeatable? | Cancelable? |
|---------|----------|-------------|-------------|-------------|
| `addEventListener` | DOM events | No | Yes | Yes |
| `setTimeout` | Delayed execution | No | No | Yes |
| `await` / `then` | Any JS Promise | Yes | No | No |

When a task blocks, other Ruby tasks continue running. The JavaScript event loop is never blocked.

## Event Listeners

```ruby
callback_id = button.addEventListener('click') do |event|
  puts event[:clientX].to_i
end

# Remove when done
JS::Object.removeEventListener(callback_id)
```

## Timers

`setTimeout` returns a `callback_id` usable with `clearTimeout`.

```ruby
timer_id = JS.global.setTimeout(2000) { puts "fired" }
JS.global.clearTimeout(timer_id)
```

Debounce pattern:

```ruby
$timer = nil
input.addEventListener('input') do |e|
  JS.global.clearTimeout($timer) if $timer
  $timer = JS.global.setTimeout(300) { search(e.target[:value].to_s) }
end
```

## Promises: await and then

Any JS method that returns a Promise can be awaited. The current Ruby task suspends until the Promise resolves.

### `await` — returns the resolved value

```ruby
port     = JS.global[:navigator][:serial].requestPort().await
response = JS.global[:navigator][:bluetooth].requestDevice(options).await
```

### `then` — yields the resolved value to a block

```ruby
JS.global[:navigator][:serial].requestPort().then do |port|
  # use port
end
```

Both `await` and `then` propagate exceptions correctly across the async boundary:

```ruby
begin
  device = JS.global[:navigator][:bluetooth].requestDevice(options).await
rescue => e
  puts "User cancelled: #{e.message}"
end
```

## HTTP Requests: fetch

`JS.global.fetch` is a dedicated shorthand for HTTP. It combines the request and await:

```ruby
JS.global.fetch('https://api.example.com/data') do |response|
  puts response[:status].to_i
end

# With options
JS.global.fetch('https://api.example.com', { method: 'POST', body: json }) do |response|
  # ...
end
```

Note: `JS.global.fetch` is a special Ruby method. For all other Promise-returning JS APIs, use `.await` or `.then`.

## See Also

- [callback.md](callback.md) - Generic and async callback system
- [architecture.md](architecture.md) - Task-based execution model
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) - Type conversion
