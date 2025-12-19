# Asynchronous Operations in PicoRuby.wasm

This document explains the async operation patterns available in PicoRuby.wasm and when to use each one.

## Overview

PicoRuby.wasm provides three main patterns for asynchronous operations:

1. **Event Listeners** (`addEventListener`) - For DOM events
2. **Timers** (`setTimeout`, `clearTimeout`) - For delayed execution
3. **Promises** (`fetch`) - For synchronous-style async I/O

Each pattern serves different use cases and has different execution models.

## Event Listeners

Event listeners respond to DOM events asynchronously without blocking the UI.

### Basic Usage

```ruby
require 'js'

button = JS.document.getElementById('myButton')
callback_id = button.addEventListener('click') do |event|
  puts "Button clicked at #{event[:clientX]}, #{event[:clientY]}"
  event.preventDefault
end
```

### Execution Model

- **Asynchronous**: The callback executes in a separate task
- **Non-blocking**: Does not block JavaScript event loop
- **Repeatable**: Can fire multiple times for the same event

### Cleanup

```ruby
# Remove event listener when done
JS::Object.removeEventListener(callback_id)
```

### When to Use

- User interactions (click, input, submit)
- DOM events (load, resize, scroll)
- Any event that may fire multiple times

## Timers (setTimeout/clearTimeout)

Timers execute code after a specified delay, similar to JavaScript's `setTimeout`.

### Basic Usage

```ruby
require 'js'

# Schedule a callback to run after 2 seconds
callback_id = JS.global.setTimeout(2000) do
  puts "Timer fired at #{Time.now}"
  # Your code here
end
```

### Canceling Timers

```ruby
callback_id = JS.global.setTimeout(5000) do
  puts "This may not run"
end

# Cancel the timer before it fires
if JS.global.clearTimeout(callback_id)
  puts "Timer canceled successfully"
end
```

### Execution Model

- **Asynchronous**: The callback executes in a separate task
- **Non-blocking**: Does not block JavaScript event loop
- **One-shot**: Fires only once (unless you schedule another timer)
- **Cancelable**: Can be canceled before it fires

### Important Notes

- Returns a `callback_id` (not a timer_id) for use with `clearTimeout`
- The callback is removed automatically after firing or being canceled
- If canceled successfully, the callback will not execute

### When to Use

- Delayed actions (animations, delayed UI updates)
- Debouncing user input
- Scheduling background tasks
- Timeout handling for operations
- Periodic checks (by rescheduling)

### Example: Debouncing

```ruby
$search_timer = nil

search_input.addEventListener('input') do |event|
  # Cancel previous timer
  JS.global.clearTimeout($search_timer) if $search_timer

  # Schedule new search after 300ms of inactivity
  $search_timer = JS.global.setTimeout(300) do
    query = event.target[:value].to_s
    perform_search(query)
  end
end
```

### Example: Connection Suspension

```ruby
class Connection
  def initialize
    @suspend_timer = nil
  end

  def schedule_suspend
    @suspend_timer = JS.global.setTimeout(30000) do
      puts "Suspending connection after 30s of inactivity"
      disconnect
    end
  end

  def cancel_suspend
    if @suspend_timer
      JS.global.clearTimeout(@suspend_timer)
      @suspend_timer = nil
    end
  end
end
```

## Promises (fetch)

The `fetch` API provides a synchronous-style way to make HTTP requests using task suspension.

### Basic Usage

```ruby
require 'js'

JS.global.fetch('https://api.example.com/data') do |response|
  if response[:status].to_i == 200
    json = response.json
    # Process data
  else
    puts "Request failed: #{response[:status]}"
  end
end
```

### Execution Model

- **Appears synchronous**: Code after the block waits for the response
- **Task suspension**: Current task is suspended until Promise resolves
- **Blocking the task**: The task cannot continue until response arrives
- **Non-blocking UI**: JavaScript event loop continues running

### How It Works

1. `fetch` suspends the current Ruby task
2. JavaScript Promise is created and starts the HTTP request
3. Ruby task is put to sleep (other tasks can run)
4. When Promise resolves, the task resumes
5. The block executes with the response
6. Code after the block continues

### When to Use

- HTTP requests (GET, POST, etc.)
- Loading remote resources
- API calls
- Any operation that should wait for completion before proceeding

## Comparison Table

| Pattern | Use Case | Blocking Task? | Repeatable? | Cancelable? |
|---------|----------|----------------|-------------|-------------|
| `addEventListener` | DOM events | No | Yes | Yes |
| `setTimeout` | Delayed execution | No | No (one-shot) | Yes |
| `fetch` | HTTP requests | Yes | No (one-shot) | No |

## Choosing the Right Pattern

### Use addEventListener when:
- Responding to user interactions
- Handling DOM events
- Need to handle same event multiple times

### Use setTimeout when:
- Need to delay an action
- Want to debounce or throttle
- Need cancelable delayed execution
- Implementing timeouts

### Use fetch when:
- Making HTTP requests
- Loading data from APIs
- Need to wait for result before proceeding
- Want synchronous-style async code

## Implementation Details

All three patterns use the same underlying callback mechanism:

1. Ruby block is stored in `JS::Object::CALLBACKS` hash
2. JavaScript calls `call_ruby_callback` when event occurs
3. A new Ruby task is created to execute the callback
4. The callback runs without blocking the JavaScript event loop

The key difference is how they integrate with the task scheduler:

- **addEventListener/setTimeout**: Callbacks run in new tasks (non-blocking)
- **fetch**: Current task suspends and resumes (appears blocking)

## See Also

- [callback.md](callback.md) - Detailed explanation of callback system
- [architecture.md](architecture.md) - Task-based execution model
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) - Data conversion
