# Pure Ruby Callbacks in PicoRuby.wasm

This document explains how to write callbacks in Pure Ruby when working with JavaScript from PicoRuby.wasm.

## Overview

PicoRuby.wasm allows you to write callbacks in Pure Ruby that can be called from JavaScript. There are two main types of callbacks, each with a different execution model:

1.  **Event Listeners (Asynchronous)** - Callbacks triggered by DOM events. These are executed asynchronously via the PicoRuby scheduler and do not block the JavaScript event loop.
2.  **Generic Callbacks (Synchronous)** - General-purpose callbacks that can be registered and called directly from JavaScript. These are executed synchronously, blocking JavaScript execution until the Ruby code completes.

Understanding the difference is key to building responsive applications.

## Event Listeners

Event listeners allow you to respond to DOM events using Ruby code in a non-blocking way.

### Basic Usage

```ruby
require 'js'

button = JS.document.getElementById('myButton')
button.addEventListener('click') do |event|
  puts "Button clicked!"
  puts "Event type: #{event[:type]}"
end
```

### How It Works

From a JavaScript perspective, event listeners work asynchronously:

1.  When you call `addEventListener`, PicoRuby stores your Ruby block in a callback registry.
2.  It then attaches a JavaScript event handler to the DOM element.
3.  When the DOM event fires (e.g., a 'click'), the JavaScript handler is invoked by the browser's event loop.
4.  This handler creates a new **Ruby Task** and adds it to the PicoRuby scheduler's queue.
5.  The PicoRuby scheduler executes the Ruby block in the background, without blocking the browser's UI thread.

### Event Object

The event object passed to your block is a `JS::Object` that wraps the JavaScript event. You can access its properties using symbol keys:

```ruby
element.addEventListener('click') do |event|
  target = event[:target]
  x = event[:clientX]
  y = event[:clientY]

  # Prevent default behavior
  event.preventDefault

  # Stop propagation
  event.stopPropagation
end
```

### Removing Event Listeners

```ruby
# addEventListener returns a callback ID
callback_id = button.addEventListener('click') do |event|
  puts "Clicked!"
end

# Remove the listener later
JS::Object.removeEventListener(callback_id)
```

## Generic Callbacks

Generic callbacks allow you to register Ruby functions that JavaScript can call directly in a **synchronous, blocking** manner. This is useful for integrating with JavaScript libraries that expect a direct return value from a callback, such as a formatter function in Chart.js.

### Basic Usage

```ruby
require 'js'

# Register a callback
JS::Object.register_callback('myCallback') do |arg1, arg2|
  "Result: #{arg1 + arg2}"
end
```

### Calling from JavaScript

When called from JavaScript, the Ruby code executes immediately, and the return value is passed back to JavaScript.

```javascript
// This call blocks until the Ruby code finishes
const result = globalThis.picorubyGenericCallbacks['myCallback'](10, 20);
console.log(result); // "Result: 30"
```

### Type Conversion

When calling callbacks, PicoRuby automatically converts arguments from JavaScript to Ruby types and return values from Ruby back to JavaScript.

For a detailed explanation of the type conversion rules and how to work with objects from both languages, please see [interoperability_between_js_and_ruby.md](./interoperability_between_js_and_ruby.md).

### Example: Chart.js Integration

```ruby
# Register a formatter callback (synchronous)
JS::Object.register_callback('formatYen') do |value|
  num = value.to_i
  "¥#{num}"
end

# Use it in Chart.js configuration
chart_config = {
  options: {
    scales: {
      y: {
        ticks: {
          callback: JS.generic_callbacks[:formatYen]
        }
      }
    }
  }
}
```

## Working with JS::Object

### Accessing Properties

```ruby
obj = JS.global[:someObject]

# Get property
value = obj[:propertyName]

# Set property
obj[:propertyName] = "new value"
```

### Calling Methods

When you call a method on a `JS::Object`:

1. If the property is a function, it's called automatically (no arguments needed)
2. If you need to pass arguments, provide them as usual

```ruby
# No arguments - function is called automatically
chart.update

# With arguments
element.setAttribute('class', 'active')

# Chaining
JS.document.getElementById('myId').focus
```

### Type Detection

The `method_missing` implementation checks if a property is a function:
- If it's a function and called with no arguments, it executes the function
- Otherwise, it returns the property value

This makes the API feel natural - you don't need to distinguish between property access and method calls in most cases.

## Caveats and Limitations

### 1. Execution Context and Scheduling

It is crucial to understand the execution model of each callback type.

-   **Generic Callbacks (Synchronous):**
    These run on the main JavaScript thread and will block rendering and other user interactions until they complete. Avoid operations that possibly try to trigger switching tasks like `Kernel#sleep`, they are raising an error.

    ```ruby
    # BAD - `sleep_ms` blocks the JavaScript UI thread
    JS::Object.register_callback('bad') do
      puts "Blocking..."
      sleep_ms 1000 // Don't do this
      puts "Done."
    end
    ```

-   **Event Listeners (Asynchronous):**
    These are executed as tasks by the PicoRuby scheduler. While they don't block the JavaScript UI, a long-running task can block the PicoRuby scheduler itself, preventing other Ruby tasks from running. Keep them fast and efficient.

-   In both callback types, avoid complex string interpolation or heavy object allocation that could trigger the Garbage Collector (GC) and cause performance issues.

### 2. Memory Management

When converting nested structures (arrays, hashes), temporary JavaScript references are created and cleaned up automatically. However:

- Avoid creating deeply nested structures in high-frequency callbacks
- The conversion creates temporary refs that are deleted after use

### 3. Callback Frequency

Some JavaScript libraries (like Chart.js) call callbacks very frequently (e.g., on every mouse move for hover effects). For such cases:

- Avoid using Ruby callbacks for high-frequency events like `backgroundColor` functions
- Prefer static arrays or less frequent updates
- Use callbacks for lower-frequency operations like formatters and tooltips

Example of what to avoid:

```ruby
# BAD - called many times per render
chart_data = {
  datasets: [{
    backgroundColor: JS.generic_callbacks[:barColor]  # Don't do this
  }]
}

# GOOD - calculate colors once in Ruby
colors = data.map { |v| get_color(v) }
chart_data = {
  datasets: [{
    backgroundColor: colors  # Static array
  }]
}

# GOOD - update colors when data changes
button.addEventListener('click') do
  new_data = generate_data()
  new_colors = new_data.map { |v| get_color(v) }

  chart[:data][:datasets][0][:data][:length] = 0
  chart[:data][:datasets][0][:backgroundColor][:length] = 0

  new_data.each { |val| chart[:data][:datasets][0][:data].push(val) }
  new_colors.each { |color| chart[:data][:datasets][0][:backgroundColor].push(color) }

  chart.update
end
```

### 4. Debugging

When callbacks fail silently:

1. Check the browser console for JavaScript errors
2. Add `puts` statements (but not with complex string interpolation)
3. Verify that the callback is registered: `console.log(globalThis.picorubyGenericCallbacks)`

### 5. Context and `this`

Ruby callbacks don't have JavaScript's `this` context. If you need access to the object:

```ruby
# The callback receives the context as an argument
JS::Object.register_callback('tooltipCallback') do |context|
  # context is a JS::Object wrapping the Chart.js tooltip context
  value = context[:parsed][:y]
  label = context[:label]
  "#{label}: #{value}"
end
```

## Best Practices

1.  **Prefer Event Listeners for UI events:** Use `addEventListener` for user interactions like clicks to keep your application responsive.
2.  **Use Generic Callbacks for synchronous needs:** Use `register_callback` only when a JavaScript library requires an immediate, direct return value.
3.  **Keep callbacks simple:** Complex logic should be in separate Ruby methods.
4.  **Avoid GC triggers:** Minimize object allocation in high-frequency callbacks.
5.  **Test in the browser:** Callback behavior can differ from regular Ruby code.
6.  **Clean up listeners:** Remove event listeners when they're no longer needed.

## Examples

See the demo files for complete examples:
- `demo/www/test_generic_callback.html` - Basic generic callback examples
- `demo/www/funicular/test_chartjs_callbacks.html` - Chart.js integration with callbacks
- `demo/www/test_event_listener.html` - Event listener examples
