# JavaScript-Ruby Interoperability

`JS.global` (= `window`) and `JS.document` are the two entry points into the JavaScript world.

**Reading** auto-converts JS primitives (string, number, boolean, null, undefined) into Ruby native values. Composite JS values (object, array, function) are returned as `JS::Object` wrappers. **Writing** auto-converts Ruby primitives to JS equivalents.

## Reading from JS

Use `[]` for property access, or call methods directly.

```ruby
element  = JS.document.getElementById('myElement')  # JS::Object (DOM element)
nav      = JS.global[:navigator]                    # JS::Object
width    = element[:offsetWidth]                    #=> Integer
text     = element[:textContent]                    #=> String
hidden   = element[:hidden]                         #=> true / false
tag      = element[:tagName]                        #=> String
```

### Type mapping (JS to Ruby, auto-converted)

| JS type     | Ruby type |
|-------------|-----------|
| `string`    | `String`   |
| `number` (integer) | `Integer` |
| `number` (float)   | `Float`   |
| `boolean`   | `true` / `false` |
| `null` / `undefined` | `nil` |
| `object`    | `JS::Object` |
| `array`     | `JS::Object` (use `#to_a` to iterate) |
| `function`  | `JS::Object` |
| `symbol`    | `JS::Object` |
| `bigint`    | `JS::Object` |

### Iterating array-like values

`querySelectorAll`, `HTMLCollection`, `NodeList` come back as `JS::Object`. Convert with `#to_a`:

```ruby
items = JS.document.querySelectorAll('.item').to_a
items.each { |el| puts el[:textContent] }  # el is a JS::Object, [:textContent] is String
```

`#to_a` recursively auto-converts primitives too, so an array of numbers becomes `Array[Integer]`.

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

Primitives come back as Ruby native values, so `==` and numeric operators just work:

```ruby
checkbox[:checked] == true          # boolean == boolean
element[:offsetWidth] > 100         # Integer > Integer
element[:id] == "myElement"         # String == String
```

Between two `JS::Object` wrappers, `==` compares the underlying ref_id (identity). A `JS::Object` is never `==` to a Ruby primitive.

## Ruby methods on JS::Object

`JS::Object` inherits from `BasicObject`, not `Object`. This keeps the Ruby
method namespace on JS wrappers as small as possible, so that almost every
method name is forwarded to JavaScript via `method_missing`. In particular,
names that used to be shadowed by `Kernel` now reach the JS side:

```ruby
JS.global[:location].hash       # location.hash (was Kernel#hash)
websocket.send(data)            # ws.send(...)  (was Kernel#send)
port.open({ baudRate: 115200 }) # port.open(...) (was Kernel#open, private)
promise.then { |v| ... }        # JS::Promise#then (defined in Ruby)
```

The complete set of Ruby-side methods on a `JS::Object` instance is:

- From `BasicObject`: `==`, `!=`, `!`, `__id__`, `__send__`, `equal?`,
  `instance_eval`, `instance_exec`, `method_missing`
- Defined in C: `[]`, `[]=`, `==`, `to_s`, `to_i`, `to_f`, `to_a`, `inspect`,
  `typeof`, `refcount`, `create_object`, `create_array`
- Ruby protocol predicates defined in C: `nil?` (always `false`), `is_a?`,
  `kind_of?`, `instance_of?`, `respond_to?`
- Defined in Ruby: `addEventListener`, `fetch`, `setTimeout`, `clearTimeout`,
  and subclass methods such as `JS::Promise#await` / `#then`

Every other method name is forwarded to JavaScript as a property read, method
call, or (for `name=`) property write.

Notes:

- The predicate names end in `?`, which is illegal in a JS identifier, so they
  can never shadow a JS property. `respond_to?` performs a real method-table
  lookup only; names that would be forwarded to JS do not count.
- Any other method name ending in `?` or `!` raises `NoMethodError` instead of
  being forwarded (it could never be a JS property anyway), so typos like
  `js_obj.frozen?` fail loudly rather than silently returning `nil`.
- `case x when JS::Object` and `rescue` matching work as usual (they use
  `Module#===` on the class, not methods on the instance).
- Known limitation: `JS::Array` includes `Enumerable` and its `each`-based
  methods (`map`, `select`, `include?`, ...) work, but `Enumerable#hash`
  raises `NoMethodError` because it relies on `Kernel#__method_recursive?`.

## Debugging

`#inspect` returns a readable preview that reflects the JS constructor and key properties:

```ruby
p JS.document.getElementById('app')
#=> #<JS::Object ref:87 HTMLDivElement id="app" class="root">

p some_fetch_response
#=> #<JS::Object ref:42 Response status=200 url="https://...">
```

## See Also

- [async_operations.md](async_operations.md) - await, then, fetch, timers, events
- [callback.md](callback.md) - Generic and async callbacks
