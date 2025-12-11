# JavaScript-Ruby Interoperability in PicoRuby.wasm

This document explains how PicoRuby.wasm handles interoperability between JavaScript and Ruby code, including automatic type conversion, property access, and best practices.

## Table of Contents

1. [Overview](#overview)
2. [Accessing Data from JavaScript (JS to Ruby)](#accessing-data-from-javascript-js-to-ruby)
3. [Passing Data to JavaScript (Ruby to JS) with `JS::Bridge`](#passing-data-to-javascript-ruby-to-js-with-jsbridge)
4. [Best Practices](#best-practices)
5. [Common Pitfalls](#common-pitfalls)
6. [Advanced Topics](#advanced-topics)
7. [Summary](#summary)

## Overview

PicoRuby.wasm provides seamless interoperability between JavaScript and Ruby through the `JS` module. This allows you to manipulate the DOM and interact with JavaScript libraries using idiomatic Ruby code.

The core of this interoperability lies in two main concepts:
1.  **Accessing and Reading** JavaScript objects in Ruby (`JS::Object`).
2.  **Passing and Converting** Ruby data structures for use in JavaScript (`JS::Bridge`).

```ruby
require 'js'

# Access JavaScript's global object (window)
global = JS.global

# Access the document object
doc = JS.document

# Get a DOM element, which becomes a JS::Object in Ruby
element = doc.getElementById('myElement')
```

## Accessing Data from JavaScript (JS to Ruby)

When you access JavaScript objects from Ruby, they are typically wrapped in a `JS::Object`, which acts as a reference or pointer to the live JavaScript object. PicoRuby automatically converts primitive values and array-like objects to their Ruby equivalents for convenience.

### Automatic Type Conversion (JS to Ruby)

When you read properties or get return values from methods, automatic conversion occurs:

| JavaScript Type | Ruby Type | Example |
|-----------------|-----------|---------|
| `null` | `nil` | `element.getAttribute('missing')` -> `nil` |
| `undefined` | `nil` | `obj[:nonExistent]` -> `nil` |
| `boolean` | `true`/`false` | `checkbox[:checked]` -> `true` |
| `number` | `Float` or `Integer` | `element[:offsetWidth]` -> `100.0` |
| `string` | `String` | `element.getAttribute('id')` -> `"myId"` |
| Array-like | `Array` | `element[:children]` -> `[...]` |
| Other objects | `JS::Object` | `element[:style]` -> `#<JS::Object>` |

### Accessing `JS::Object` Properties and Methods

You can interact with `JS::Object` instances as if they were Ruby objects.

**Property Access with `[]`:**
Use the `[]` operator with Symbols (recommended) or Strings.
```ruby
# Read property
element[:textContent]

# Assign property
element[:className] = "highlighted"
```

**Method Calls:**
JavaScript methods can be called directly.
```ruby
# No arguments
element.focus

# With arguments
element.setAttribute('class', 'active')
```

### Array-like Objects

JavaScript objects that are "array-like" (e.g., `NodeList`, `HTMLCollection`) are automatically converted into a Ruby `Array` of `JS::Object` instances. This allows you to use powerful Ruby `Enumerable` methods.

```ruby
# querySelectorAll returns a Ruby Array of JS::Objects
buttons = doc.querySelectorAll('.button')

# Now you can use standard Ruby methods
buttons.each do |button|
  button[:textContent] = "Click me"
end

button_count = buttons.length
active_buttons = buttons.select { |btn| btn[:classList].contains('active') }
```
**Note:** Because this is a conversion, modifying the Ruby `Array` (e.g., `children.clear`) will **not** affect the original JavaScript object or the DOM. You must operate on the parent `JS::Object` to make changes.


## Passing Data to JavaScript (Ruby to JS) with `JS::Bridge`

While `JS::Object` is for referencing existing JavaScript objects, `JS::Bridge.to_js` is for **creating new JavaScript objects from Ruby data structures**. This is a crucial concept when you need to pass complex data like nested Hashes and Arrays to JavaScript libraries, which often expect plain JavaScript objects.

### Deep Conversion of Ruby Objects

`JS::Bridge.to_js` performs a "deep" or "recursive" conversion:
- A Ruby `Hash` becomes a JavaScript `Object`.
- A Ruby `Array` becomes a JavaScript `Array`.
- This process is applied to all nested elements.

**Example: Converting a Complex Hash**
Imagine you are configuring a chart library.
```ruby
# A complex Ruby Hash with nested data
chart_config_ruby = {
  type: 'bar',
  data: {
    labels: ['Jan', 'Feb', 'Mar'],
    datasets: [{
      label: 'Sales',
      data: [10, 20, 30],
      backgroundColor: 'rgba(54, 162, 235, 0.6)'
    }]
  },
  options: {
    responsive: true
  }
}

# Convert the entire structure into a plain JavaScript object
chart_config_js = JS::Bridge.to_js(chart_config_ruby)

# Now, you can pass this new JS object to a JavaScript library
canvas = doc.getElementById('myChart')
chart = JS.global[:Chart].new(canvas, chart_config_js)
```

### `JS::Object` vs. `JS::Bridge.to_js`

It's important to understand the difference:

-   **`JS::Object`**: A **reference** to an *existing* JavaScript object. It's a live link. Changes made in Ruby are reflected in JavaScript and vice-versa.
-   **`JS::Bridge.to_js(ruby_obj)`**: Creates a **new, plain** JavaScript object by **copying** data from a Ruby object. It's a one-time conversion, not a live link.

### When to Use `JS::Bridge.to_js`

Use it whenever you construct data in Ruby that needs to be handed over to a JavaScript function or property.

1.  **Assigning to JavaScript object properties:**
    ```ruby
    chart[:data][:datasets][0][:data] = JS::Bridge.to_js([10, 20, 30])
    ```

2.  **Passing complex configurations to JS libraries:**
    ```ruby
    # The Chart.js constructor expects a plain JS object for its config
    chart = JS.global[:Chart].new(canvas, JS::Bridge.to_js(ruby_config))
    ```

## Best Practices

### 1. Use Symbol Keys for Property Access

```ruby
# Good - clear and concise
element[:textContent]
element[:className]

# Avoid - string keys are less idiomatic
element['textContent']
```

### 2. Trust Automatic Conversion

Don't manually check types when automatic conversion handles it:

```ruby
# Good - automatic conversion
value = element[:value]
if value.empty?
  # ...
end

# Unnecessary - avoid manual type checking
value = element[:value]
if value.is_a?(String) && value.empty?
  # ...
end
```

### 3. Use Ruby Array Methods with Array-like Objects

```ruby
# Good - leverage Ruby's powerful Array methods
buttons = doc.querySelectorAll('.button')
active_buttons = buttons.select { |btn| btn[:classList].contains('active') }

# Good - use each for iteration
buttons.each do |button|
  button[:disabled] = true
end
```

### 4. Convert Ruby Data Before Assigning to JavaScript

```ruby
# Good - explicit conversion
new_data = [10, 20, 30, 40]
chart[:data][:datasets][0][:data] = JS::Bridge.to_js(new_data)

# Bad - assigning Ruby array directly may not work as expected
chart[:data][:datasets][0][:data] = [10, 20, 30, 40]
```

### 5. Handle Nil Values Appropriately

```ruby
# Good - check for nil
attr = element.getAttribute('data-value')
if attr
  process(attr)
else
  # Handle missing attribute
end

# Good - use safe navigation
element.getAttribute('data-value')&.upcase
```

## Common Pitfalls

### 1. Array-like Objects are Converted to Ruby Arrays

**Problem:** Trying to access JavaScript properties on converted arrays

```ruby
# WRONG - files is now a Ruby Array
files = input[:files]
files[:length] = 0  # TypeError: Symbol cannot be converted to Integer

# CORRECT - use Ruby array methods
files = input[:files]
files.clear
# Or get the original JS object if needed for specific JS operations
```

### 2. Forgetting to Convert Ruby Arrays

**Problem:** Assigning Ruby arrays directly to JavaScript object properties

```ruby
# May not work correctly
chart[:data][:datasets][0][:data] = [1, 2, 3]

# CORRECT - explicit conversion
chart[:data][:datasets][0][:data] = JS::Bridge.to_js([1, 2, 3])
```

### 3. Checking for Undefined vs Nil

**Problem:** JavaScript `undefined` is converted to Ruby `nil`

```ruby
# JavaScript: returns undefined
value = obj[:nonExistentProperty]

# In Ruby, this becomes nil
puts value.nil?  # => true

# Cannot distinguish between null and undefined
# Both become nil in Ruby
```

### 4. String Conversion Assumptions

**Problem:** Assuming `.to_s` is needed

```ruby
# WRONG - unnecessary conversion
element = doc.getElementById('myElement')
text = element[:textContent].to_s  # Already a String!

# CORRECT - automatic conversion handles it
text = element[:textContent]  # Returns Ruby String
```

### 5. Modifying Converted Arrays

**Problem:** Expecting changes to affect the original JavaScript array

```ruby
# Get array-like object (converted to Ruby Array)
children = element[:children]

# Modifying the Ruby array doesn't affect the DOM
children.clear  # Only clears the Ruby array copy

# To modify the actual DOM:
while element[:childNodes].length > 0
  element.removeChild(element[:childNodes][0])
end

# Or work with the JS object directly if needed
child_nodes = element[:childNodes]  # Ruby Array of JS::Objects
child_nodes.each do |child|
  element.removeChild(child)
end
```

## Advanced Topics

### Working with Events

Event handlers receive event objects that are automatically converted:

```ruby
button.addEventListener('click') do |event|
  # event is a JS::Object
  event.preventDefault

  # Properties are automatically converted
  target = event[:target]
  value = target[:value]  # Ruby String

  puts "Button clicked: #{value}"
end
```

### Callbacks to JavaScript Libraries

When registering callbacks with JavaScript libraries, use `JS::Object.register_callback`:

```ruby
# Register a Ruby callback that JavaScript can call
JS::Object.register_callback('formatValue') do |value|
  # value is automatically converted from JavaScript
  "¥#{value.to_i.to_s.reverse.scan(/.{1,3}/).join(',').reverse}"
end

# Use in JavaScript library
chart_config = {
  scales: {
    y: {
      ticks: {
        callback: JS.global[:picorubyGenericCallbacks][:formatValue]
      }
    }
  }
}
```

### Direct JavaScript Execution

For advanced cases, you can execute JavaScript directly:

```ruby
# Create and execute script
script = doc.createElement('script')
script[:textContent] = "console.log('Hello from JavaScript');"
doc.body.appendChild(script)
doc.body.removeChild(script)
```

## Summary

PicoRuby WASM provides powerful and convenient JavaScript-Ruby interoperability with automatic type conversion. Key points to remember:

1. **Automatic conversion** handles most type conversions transparently
2. **Array-like objects** become Ruby Arrays automatically
3. **Use `JS::Bridge.to_js`** when passing Ruby data to JavaScript
4. **Trust the conversion** - don't manually check types unnecessarily
5. **Leverage Ruby idioms** - use Ruby array methods, safe navigation, etc.

By following these guidelines, you can write clean, idiomatic Ruby code that seamlessly interacts with JavaScript libraries and the DOM.
