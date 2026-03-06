# JavaScript-Ruby Interoperability

`JS.global` (= `window`) and `JS.document` are the two entry points into the JavaScript world.

**Reading** always returns a `JS::Object` wrapper (except `null`/`undefined` which become `nil`). **Writing** auto-converts Ruby primitives to JS equivalents.

## Reading from JS

Use `[]` for property access, or call methods directly. Always returns `JS::Object`.

```ruby
element  = JS.document.getElementById('myElement')
nav      = JS.global[:navigator]
width    = element[:offsetWidth]   #=> JS::Object (JS number)
text     = element[:textContent]   #=> JS::Object (JS string)
```

Convert with explicit methods:

| Method | Ruby type |
|--------|-----------|
| `.to_s` | String |
| `.to_i` | Integer |
| `.to_f` | Float |
| `.to_a` | Array of JS::Object |

```ruby
text  = element[:textContent].to_s
width = element[:offsetWidth].to_i
items = doc.querySelectorAll('.item').to_a  # then .each, .map, etc.
```

Only `null` and `undefined` convert automatically to `nil`.

## Writing to JS

`[]=` and method arguments auto-convert Ruby types:

| Ruby | JS |
|------|----|
| `String` | string |
| `Integer` / `Float` | number |
| `true` / `false` | boolean |
| `nil` | null |
| `Array` | Array |
| `Hash` | Object |
| `JS::Object` | the same JS object |

```ruby
element[:textContent] = "Hello"
element[:hidden]      = false
element[:value]       = 42
```

## Calling JS Methods

Call any JS method directly. Arguments accept any combination of Ruby primitives and `JS::Object`:

```ruby
element.focus
element.setAttribute('class', 'active')    # (String, String)
canvas.getContext('2d')                     # String
timer_obj.makePromise("hello", 50)         # String + Integer
port.open({ baudRate: 115200 })            # Hash
```

For JS constructors, use `new`:

```ruby
chart = JS.global[:Chart].new(canvas, config_js)
```

For deeply nested Ruby structures passed to JS libraries, use `JS::Bridge.to_js`:

```ruby
config = JS::Bridge.to_js({
  type: 'bar',
  data: { labels: ['Jan', 'Feb'], datasets: [{ data: [10, 20] }] }
})
JS.global[:Chart].new(canvas, config)
```

## Comparison

`==` compares `JS::Object` with Ruby primitives directly. Numeric operators work on JS numbers:

```ruby
checkbox[:checked] == true          # boolean comparison
element[:offsetWidth] > 100         # numeric comparison
element[:id] == "myElement"         # string comparison
```

## See Also

- [async_operations.md](async_operations.md) - await, then, fetch, timers, events
- [callback.md](callback.md) - Generic and async callbacks
