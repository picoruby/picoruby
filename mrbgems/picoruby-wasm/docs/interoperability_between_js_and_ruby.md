# JavaScript-Ruby Interoperability in PicoRuby.wasm

This document explains how PicoRuby.wasm handles interoperability between JavaScript and Ruby code, focusing on a design that prioritizes explicitness and predictability.

## Table of Contents

1. [Overview](#overview)
2. [Guiding Principles](#guiding-principles)
3. [Accessing Data from JavaScript (JS to Ruby)](#accessing-data-from-javascript-js-to-ruby)
4. [Explicit Conversion Methods](#explicit-conversion-methods)
5. [Passing Data to JavaScript (Ruby to JS)](#passing-data-to-javascript-ruby-to-js)
6. [Best Practices](#best-practices)
7. [Common Pitfalls](#common-pitfalls)
8. [Summary](#summary)

## Overview

PicoRuby.wasm provides seamless interoperability between JavaScript and Ruby through the `JS` module. This allows you to manipulate the DOM and interact with JavaScript libraries using idiomatic Ruby code.

The core of this interoperability lies in two main concepts:
1.  **Accessing and Reading** JavaScript objects in Ruby via a wrapper class, `JS::Object`.
2.  **Passing and Converting** Ruby data structures for use in JavaScript, which is handled automatically by setters.

```ruby
require 'js'

# Access JavaScript's global object (window)
global = JS.global

# Access the document object
doc = JS.document

# Get a DOM element, which is always wrapped in a JS::Object
element = doc.getElementById('myElement')
```

## Guiding Principles

The API is designed around two principles:

1.  **Explicitness over Implicitness (for Getters):** When retrieving data from the JavaScript world, ambiguity is avoided. Instead of automatically converting JS objects to their potential Ruby equivalents, we almost always return a `JS::Object` wrapper. This forces the developer to be explicit about the desired Ruby type (e.g., String, Array), preventing unexpected behavior.

2.  **Convenience over Strictness (for Setters):** When passing data into the JavaScript world, the API prioritizes convenience. Ruby primitives like `String`, `Integer`, `Array`, and `Hash` are automatically converted to their JavaScript counterparts, allowing for natural and intuitive assignment.

## Accessing Data from JavaScript (JS to Ruby)

When you access JavaScript objects from Ruby, they are wrapped in a `JS::Object`, which acts as a reference to the live JavaScript object.

### Type Conversion on Read (Getters)

To ensure predictability, automatic type conversion is kept to a minimum.

| JavaScript Type | Ruby Type | Example |
|-----------------|-----------|---------|
| `null` | `nil` | `element.getAttribute('missing')` -> `nil` |
| `undefined` | `nil` | `obj[:nonExistent]` -> `nil` |
| `boolean` | `JS::Object` | `checkbox[:checked]` -> `#<JS::Object>` |
| `number` | `JS::Object` | `element[:offsetWidth]` -> `#<JS::Object>` |
| `string` | `JS::Object` | `element.getAttribute('id')` -> `#<JS::Object>` |
| Array-like | `JS::Object` | `element[:children]` -> `#<JS::Object>` |
| Other objects | `JS::Object` | `element[:style]` -> `#<JS::Object>` |

As you can see, only `null` and `undefined` are automatically converted to `nil`. All other values, including booleans, are returned as a `JS::Object`. To use these values as standard Ruby types, you must use the explicit conversion methods described below.

### Accessing `JS::Object` Properties and Methods

You can interact with `JS::Object` instances as if they were Ruby objects.

**Property Access:**
Use the `[]` operator for reading and `[]=` for writing.
```ruby
# Read property (returns a JS::Object)
text_content_obj = element[:textContent]

# Assign property (accepts a Ruby String)
element[:className] = "highlighted"
```

**Method Calls:**
JavaScript methods can be called directly. The return value will be a `JS::Object` (or `nil` for null/undefined values).
```ruby
# No arguments
element.focus

# With arguments (accepts Ruby primitives)
element.setAttribute('class', 'active')
```

## Explicit Conversion Methods

To convert a `JS::Object` wrapper into a native Ruby type, use one of the `to_*` methods.

### `.to_s` -> String
Converts a JS `string` object to a Ruby `String`.
```ruby
# Get the text content of an element
text_obj = element[:textContent]  #=> <JS::Object>
text_str = text_obj.to_s         #=> "Some text"
```

### `.to_i` and `.to_f` -> Integer and Float
Converts a JS `number` object to a Ruby `Integer` or `Float`.
```ruby
# Get the width of an element
width_obj = element[:offsetWidth] #=> <JS::Object>
width_int = width_obj.to_i        #=> 100
width_float = width_obj.to_f      #=> 100.0
```

### `.to_a` -> Array
Converts a JS `Array` or array-like object (e.g., `NodeList`, `HTMLCollection`) to a Ruby `Array` of `JS::Object` instances. This allows you to use powerful `Enumerable` methods.
```ruby
# querySelectorAll returns a JS::Object (representing a NodeList)
buttons_obj = doc.querySelectorAll('.button')

# Convert to a Ruby Array to use .each, .map, etc.
buttons_array = buttons_obj.to_a
buttons_array.each do |button|
  button[:textContent] = "Click me" # Setter accepts a Ruby String
end

button_count = buttons_array.length
```
**Note:** `.to_a` performs a conversion. Modifying the returned Ruby `Array` (e.g., `buttons_array.clear`) will **not** affect the original JavaScript object or the DOM.

### Comparison Operators

`JS::Object` supports direct comparison with Ruby native types, eliminating the need for explicit conversion in many cases.

#### `==` Equality Comparison
Compares a `JS::Object` with Ruby primitives based on the underlying JavaScript type:

```ruby
# String comparison
name_obj = element[:name]         #=> <JS::Object> (JS string)
if name_obj == "test"             # Direct comparison with Ruby String
  puts "Name matches!"
end

# Number comparison
width_obj = element[:offsetWidth] #=> <JS::Object> (JS number)
if width_obj == 100               # Direct comparison with Ruby Integer
  puts "Width is 100"
end

# Boolean comparison
checked_obj = checkbox[:checked]  #=> <JS::Object> (JS boolean)
if checked_obj == true            # Direct comparison with Ruby true/false
  puts "Checkbox is checked"
end
```

#### Numeric Comparison Operators (`>`, `>=`, `<`, `<=`, `<=>`)
For `JS::Object` instances that wrap JavaScript numbers, you can use comparison operators directly:

```ruby
width = element[:offsetWidth]     #=> <JS::Object> (JS number)

if width > 100
  puts "Element is wide"
end

if width >= 50 && width <= 200
  puts "Width is in range"
end

# Spaceship operator returns -1, 0, 1, or nil
result = width <=> 100
case result
when -1 then puts "Less than 100"
when 0  then puts "Equal to 100"
when 1  then puts "Greater than 100"
end
```

**Note:** Comparison operators only work when the `JS::Object` wraps a JavaScript `number` or `bigint`. Using them on other types (string, object, etc.) will raise a `TypeError`.

## Passing Data to JavaScript (Ruby to JS)

When calling a `setter` method (e.g., `element[:className] = "active"` or `element.value = "text"`) or passing arguments to a JS method (e.g., `element.setAttribute('class', 'active')`), PicoRuby automatically and recursively converts Ruby objects to their JavaScript counterparts.

| Ruby Type | JavaScript Type |
|-----------|-----------------|
| `nil` | `null` |
| `true`/`false` | `boolean` |
| `Integer`/`Float`| `number` |
| `String` | `string` |
| `Array` | `Array` |
| `Hash` | `Object` |
| `JS::Object` | The original JS object it refers to |

This design allows for intuitive and convenient code when writing to the JavaScript world.
```ruby
# Automatic conversion on assignment
element[:textContent] = "Hello, World!"
element[:value] = 123
element[:hidden] = false

# Automatic conversion for method arguments
doc.body.appendChild(another_element) # another_element is a JS::Object
```

For creating new, complex JavaScript objects from Ruby Hashes or Arrays to pass to JS library constructors (e.g., for charts or maps), you can use `JS::Bridge.to_js`.

```ruby
# A complex Ruby Hash
chart_config_ruby = {
  type: 'bar',
  data: {
    labels: ['Jan', 'Feb', 'Mar'],
    datasets: [{ data: [10, 20, 30] }]
  }
}

# Convert the entire structure into a new JavaScript object
chart_config_js = JS::Bridge.to_js(chart_config_ruby)

# Pass the new JS object to a JavaScript library
chart = JS.global.Chart.new(canvas, chart_config_js)
```

## Best Practices

### 1. Be Explicit with Getters, Be Direct with Setters
This is the core philosophy. When reading, expect a `JS::Object` and convert it. When writing, use Ruby types directly.
```ruby
# Good: Explicit getter conversion, direct setter assignment
text = element[:textContent].to_s
element[:textContent] = text + " (updated)"
```

### 2. Use `.to_a` to Iterate
To use `Enumerable` methods like `.each` or `.map` on a `NodeList`, convert it to a Ruby `Array` first.
```ruby
# Good: Convert to Array before iterating
nodes = doc.querySelectorAll('.item').to_a
nodes.each do |node|
  node[:className] = 'processed'
end
```

### 3. Chain Method Calls
Since getters return a `JS::Object`, you can chain calls that exist on the underlying JavaScript object.
```ruby
# Get the first child's tag name
tag_name = doc.body.firstChild.tagName.to_s
```

## Common Pitfalls

### 1. Calling Ruby Methods on `JS::Object`
**Problem:** `JS::Object` only proxies methods that exist on the underlying JavaScript object.
```ruby
# WRONG
nodes = doc.querySelectorAll('.item')
nodes.each { |n| ... } #=> NoMethodError: undefined method `each' for #<JS::Object>

# CORRECT
nodes = doc.querySelectorAll('.item').to_a
nodes.each { |n| ... }
```

### 2. Modifying the Array from `.to_a`
**Problem:** Expecting changes to the converted Ruby `Array` to affect the DOM.
```ruby
# Get a converted Ruby Array
children = element[:children].to_a

# This only clears the Ruby array, NOT the actual DOM children
children.clear

# To modify the actual DOM, you must call methods on the JS::Object
element.innerHTML = ""
```

## Summary

PicoRuby.wasm provides powerful and predictable JavaScript-Ruby interoperability. Key points to remember:

1.  **Getters are strict:** When reading from JS, you get a `JS::Object` for all values except `null` and `undefined` (which become `nil`).
2.  **Setters are convenient:** When writing to JS, you can use native Ruby types, which are converted automatically.
3.  **Be explicit:** Use `.to_s`, `.to_i`, `.to_f`, and `.to_a` to convert `JS::Object` wrappers into the Ruby types you need.
4.  **Compare directly:** Use `==` to compare `JS::Object` with Ruby primitives (String, Integer, Float, true/false). Use `>`, `>=`, `<`, `<=`, `<=>` for numeric comparisons.

By following these guidelines, you can write clean, robust, and predictable Ruby code that seamlessly interacts with JavaScript libraries and the DOM.
