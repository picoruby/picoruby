# picoruby-wasm

PicoRuby for WebAssembly - Ruby runtime in the browser powered by mruby VM.

## Overview

This mrbgem provides WebAssembly bindings for PicoRuby, enabling Ruby code to run in web browsers with seamless JavaScript interoperability.

## Architecture

### Task-Based Execution Model

PicoRuby WASM uses a **cooperative multitasking model** that integrates Ruby tasks with the JavaScript event loop:

```
JavaScript Event Loop (requestAnimationFrame, 60fps)
  |
  ├─> mrb_tick_wasm()    // Timer processing, wake sleeping tasks
  |
  └─> mrb_run_step()     // Execute one time slice of one task
       |
       └─> mrb_task_run_once()
            |
            └─> mrb_vm_exec()  // Execute Ruby bytecode
```

Each Ruby task is a separate execution context with its own:
- Call stack (stored in heap via `mrb_context`)
- Local variables
- Exception state
- Priority and scheduling parameters

### Asynchronous Operations and Exception Safety

A critical design aspect is how **asynchronous JavaScript operations** (like `fetch()`, `setTimeout()`) integrate with Ruby's **exception handling** (raise/rescue).

#### The Challenge

Ruby uses `setjmp/longjmp` for exception handling. Emscripten's default `longjmp` implementation uses JavaScript exceptions, which **cannot cross async boundaries**:

```
Ruby: fetch() do ... raise ... end
       |
       v
C: mrb_vm_exec()
   setjmp() established
       |
       v
JS: Promise.then()
       |  [ASYNC BOUNDARY - different call stack]
       v
   longjmp()  // ERROR: setjmp is on a different call stack!
```

#### Our Solution: Task Suspension Model

PicoRuby WASM **explicitly manages async boundaries** using task suspension:

```
1. Ruby callback registered
   JS.global.fetch('url') do |response|
     raise "error"  // This will work correctly!
   rescue => e
     puts e.message
   end

2. Suspend task BEFORE async operation
   mrb_suspend_task(current_task)
   // setjmp scope is EXITED here

3. JavaScript Promise resolves
   Promise.then(result => {
     // New C call stack starts here
     ccall('resume_promise_task', ...)
   })

4. Resume task with NEW setjmp
   mrb_resume_task(task)
     |
     v
   mrb_task_run_once()
     |
     v
   mrb_vm_exec()
     // NEW setjmp() established on current C stack
     |
     v
   Execute Ruby callback
     raise "error"  // longjmp() to NEW setjmp - SAFE!
```

**Key Points:**

1. **Task suspension exits setjmp scope**: When `mrb_suspend_task()` is called, we return from `mrb_vm_exec()`, completely exiting the setjmp scope.

2. **Promise callback starts fresh C stack**: JavaScript's `Promise.then()` callback invokes a new C function (`resume_promise_task`) with a clean call stack.

3. **New setjmp on task resume**: When `mrb_vm_exec()` is called again during task resumption, a **new setjmp** is established on the current C stack.

4. **longjmp never crosses boundaries**: Ruby exceptions (longjmp) only occur within a single `mrb_vm_exec()` call, always targeting the setjmp on the same C stack.

This design ensures that **ASYNCIFY=1 is not required**, avoiding the 50%+ code size increase and performance overhead.

### Why ASYNCIFY=1 is Not Needed

**ASYNCIFY=1** is an Emscripten feature that transforms WASM code to save/restore the entire call stack, enabling `longjmp` across async boundaries. However:

- **Cost**: 50%+ increase in WASM file size, runtime performance overhead
- **Unnecessary**: Our task suspension model already handles async boundaries correctly
- **Verified**: Comprehensive test suite (see `demo/www/test_*.html`) validates exception safety in:
  - Nested async operations (3 levels deep)
  - Multiple concurrent tasks
  - ensure clauses
  - Exception re-raising
  - Deep recursion + async
  - Custom exception classes
  - 100 parallel tasks (stress test)

All tests pass without ASYNCIFY=1, confirming the architecture is sound.

## Critical Design Principles

When modifying this codebase, **preserve these invariants** to maintain exception safety:

### 1. Always Suspend Tasks Before Async Operations

```c
// CORRECT
mrb_suspend_task(mrb, current_task);
setup_promise_handler(promise_id, ...);  // Async operation

// WRONG - DO NOT DO THIS
setup_promise_handler(promise_id, ...);  // Async without suspension
// Task still thinks it's running, setjmp is still active
```

### 2. Resume Tasks in Fresh Call Stack

```javascript
// CORRECT
promise.then((result) => {
  ccall('resume_promise_task', ...);  // Fresh C stack
});

// WRONG - DO NOT DO THIS
promise.then((result) => {
  // Direct execution without going through resume mechanism
});
```

### 3. Let mrb_vm_exec() Manage setjmp

```c
// CORRECT - Let mrb_vm_exec handle setjmp
mrb_value
mrb_task_run_once(mrb_state *mrb)
{
  mrb_tcb *tcb = q_ready_;
  mrb->c = &tcb->c;
  tcb->value = mrb_vm_exec(mrb, ...);  // setjmp happens here
  // ...
}

// WRONG - DO NOT DO THIS
// Never manually call setjmp outside mrb_vm_exec
if (setjmp(buf) == 0) {  // WRONG!
  mrb_vm_exec(mrb, ...);
}
```

### 4. Each Task Gets Independent Context

```c
// CORRECT - Each task has its own mrb_context
typedef struct RTcb {
  struct mrb_context c;  // Per-task context with own stack
  // ...
} mrb_tcb;

// When running task:
mrb->c = &tcb->c;  // Switch context

// WRONG - DO NOT DO THIS
// Never share mrb_context between tasks
```

### 5. Test Exception Handling After Changes

After any modification to task scheduling, async operations, or exception handling:

1. Build: `rake wasm:clean && rake wasm:debug`
2. Run test server: `rake wasm:server`
3. Open: http://localhost:8080/test_index.html
4. **Run ALL tests** and verify each one passes
5. Check browser console for errors

**If any test fails**, the async/exception boundary has been broken.

## File Structure

```
picoruby-wasm/
├── mrbgem.rake              # Build configuration (emcc flags)
├── src/
│   └── mruby/
│       ├── js.c             # JavaScript interop, Promise handling
│       └── wasm.c           # WASM initialization, main loop
├── demo/
│   └── www/
│       ├── test_index.html  # Exception handling test suite
│       ├── test_*.html      # Individual test cases
│       └── *.html           # Usage examples
└── npm/                     # NPM package for distribution
```

### Key Files for Async/Exception Handling

- **`src/mruby/js.c`**
  - `setup_promise_handler()`: Registers Promise.then callback
  - `resume_promise_task()`: Resumes task after Promise resolves
  - `js_add_event_listener()`: DOM event handling

- **`../../picoruby-mruby/src/task.c`**
  - `mrb_suspend_task()`: Suspends task, exits setjmp scope
  - `mrb_resume_task()`: Resumes task, will establish new setjmp
  - `mrb_task_run_once()`: Task execution wrapper, calls mrb_vm_exec

- **`../../picoruby-mruby/src/vm.c`**
  - `mrb_vm_exec()`: VM execution with setjmp/longjmp for exceptions

## Testing

### Exception Handling Test Suite

Located in `demo/www/`, the test suite validates async exception safety:

| Test | Purpose |
|------|---------|
| `raise_rescue.html` | Basic async exception handling |
| `test_nested_async.html` | 3-level nested async with exception |
| `test_multi_task.html` | Multiple independent tasks |
| `test_ensure.html` | ensure clause execution |
| `test_reraise.html` | Exception re-raising |
| `test_deep_recursion.html` | Deep call stack + async |
| `test_custom_exception.html` | Custom exception classes |
| `test_promise_chain.html` | Chained Promise callbacks |
| `test_stress.html` | 100 parallel tasks |

**All tests must pass** to ensure async boundary safety.

### Running Tests

```bash
cd lib/picoruby/mrbgems/picoruby-wasm
rake
cd demo
python3 -m http.server 8080
# Open http://localhost:8080/www/test_index.html
```

## Build Configuration

### Emscripten Flags (mrbgem.rake)

```ruby
emcc options:
  -s WASM=1                      # WebAssembly output
  -s EXPORT_ES6=1                # ES6 module export
  -s MODULARIZE=1                # Wrap in module
  -s INITIAL_MEMORY=16MB         # Initial heap size
  -s ALLOW_MEMORY_GROWTH=1       # Dynamic memory
  -s ENVIRONMENT=web             # Browser target
  -s WASM_ASYNC_COMPILATION=1    # Async compilation
```

**Note**: `ASYNCIFY=1` is **NOT** used. Our task suspension model eliminates the need for it.

### Why Not ASYNCIFY=1?

We explicitly avoid ASYNCIFY=1 because:

1. **Code Size**: Would increase WASM from ~1.6MB to ~2.4MB+ (50% increase)
2. **Performance**: Runtime overhead for stack save/restore operations
3. **Unnecessary**: Task suspension model already handles async correctly
4. **Proven**: All test cases pass without it

## Future Considerations

### Promise Rejection Handling

Currently, Promise rejections are **not handled**. The code in `src/mruby/js.c:193-205` shows commented-out `.catch()` handler:

```javascript
// .catch(
//   (error) => {
//     ccall('resume_promise_error_task', ...);
//   }
// )
```

**Recommendation**: Implement this to handle network errors and rejected Promises by raising Ruby exceptions in the task.

### Memory Management

#### Current Limitation: JavaScript Reference Leaks

JavaScript object references are stored in `globalThis.picorubyRefs` array and **never removed**:

```javascript
// js.c:38-43
EM_JS(void, init_js_refs, (), {
  globalThis.picorubyRefs = [];
  globalThis.picorubyRefs.push(window);
});

// Every JS method call adds a new reference:
const newRefId = globalThis.picorubyRefs.push(element) - 1;
```

**Problem**: When Ruby objects are garbage collected, their corresponding JavaScript references remain in the array.

**Example**:
```ruby
# This leaks memory
10000.times do
  element = JS.document.getElementById('button')
  # Ruby GC frees the wrapper, but JS reference stays
end
# picorubyRefs array grows to 10000+ entries
```

**Current Status**:
- Not a critical issue for typical web apps (page reload clears memory)
- Becomes problematic for long-running single-page applications
- Browser's GC cannot collect objects while they're in the array

#### Potential Solutions

**Option 1: Implement Finalizer (Recommended)**

Modify `picorb_js_obj_free()` to remove JS reference:

```c
static void
picorb_js_obj_free(mrb_state *mrb, void *ptr)
{
  picorb_js_obj *obj = (picorb_js_obj *)ptr;
  remove_js_ref(obj->ref_id);  // New function
  mrb_free(mrb, ptr);
}

EM_JS(void, remove_js_ref, (int ref_id), {
  // Option A: Set to null (keeps array indices stable)
  globalThis.picorubyRefs[ref_id] = null;

  // Option B: Use WeakRef (modern browsers only)
  // Requires redesigning to use WeakRef from the start
});
```

**Option 2: Periodic Compaction**

Periodically rebuild `picorubyRefs` array, removing null entries.

#### Task Contexts

Task contexts (TCB) are properly managed:
- Allocated in heap via `mrb_malloc()`
- Freed when task completes or is terminated
- Each task's `mrb_context` includes its own Ruby stack

No known issues with task memory management.

## JS::Object Programming Guide

### Overview

`JS::Object` provides Ruby-JavaScript interoperability. It wraps JavaScript objects and allows calling their methods from Ruby code.

### Supported Method Call Patterns

```ruby
# Get property
obj[:propertyName]

# Set property
obj[:propertyName] = value

# Call method with no arguments
obj.methodName()

# Call method with String argument
obj.methodName("string")

# Call method with Integer argument (added in latest version)
obj.methodName(42)

# Call method with JS::Object argument
obj.methodName(another_js_object)

# Call method with two String arguments
obj.methodName("arg1", "arg2")

# Call method with two JS::Object arguments
obj.methodName(obj1, obj2)
```

### Known Limitations and Workarounds

#### 1. FormData and "Illegal Invocation" Errors

**Problem**: Some JavaScript APIs like `FormData.append()` fail with "Illegal invocation" when called directly from Ruby.

```ruby
# DOES NOT WORK
form_data = JS.global[:FormData].new
form_data.append("key", "value")  # TypeError: Illegal invocation
```

**Root Cause**: JavaScript's `this` binding. Even though we use `func.call(obj, ...)` in the C code, some native APIs validate that they're called in the correct context.

**Workaround**: Define JavaScript helper functions in HTML:

```html
<script>
  window.createFormData = function(fieldsObj) {
    const formData = new FormData();
    for (const [key, value] of Object.entries(fieldsObj)) {
      formData.append(key, value);
    }
    return formData;
  };
</script>
```

```ruby
# Use the helper
fields_js = JS.global[:Object].new
fields.each { |k, v| fields_js[k.to_sym] = v }
form_data = JS.global[:createFormData].call(fields_js)
```

#### 2. Cannot Assign Proc to JavaScript Properties

**Problem**: JavaScript callback properties (like `onload`, `onclick`) cannot accept Ruby Proc objects.

```ruby
# DOES NOT WORK
reader = JS.global[:FileReader].new
reader[:onload] = -> (e) { puts "loaded" }  # TypeError: unsupported type
```

**Root Cause**: Ruby Proc objects cannot be directly converted to JavaScript functions.

**Workaround 1**: Use `addEventListener` for DOM elements:

```ruby
element.addEventListener("click") do |event|
  puts "Clicked!"
end
```

**Workaround 2**: Use alternative APIs that don't require callbacks:

```ruby
# Instead of FileReader with onload callback:
reader = JS.global[:FileReader].new
reader[:onload] = -> (e) { ... }  # DOESN'T WORK

# Use URL.createObjectURL instead:
url = JS.global[:URL].createObjectURL(file)  # Synchronous, no callback needed
```

#### 3. Promise Handling

**Built-in Support**: `fetch()` method handles Promises automatically:

```ruby
JS.global.fetch("https://api.example.com/data") do |response|
  json_text = response.to_binary
  data = JSON.parse(json_text)
  puts data
end
```

**Generic Promises**: Use the `then` method (polling-based):

```ruby
promise = some_js_function_that_returns_promise()
promise.then do |result|
  puts "Promise resolved: #{result.inspect}"
end
```

**Note**: The `then` method uses polling (`sleep 0.05`) internally. For better performance, prefer using the built-in `fetch()` method when possible.

#### 4. Integer Arguments

**Supported** (as of latest version):

```ruby
# Array/NodeList access
files = input[:files]
file = files.item(0)  # Integer argument works

# Indexed access
element = array[5]  # Integer argument works
```

### Best Practices

#### 1. Minimize JavaScript Object Creation

Ruby wrappers are created for each JavaScript object reference. In long-running apps, this can accumulate:

```ruby
# AVOID in loops
10000.times do
  element = JS.document.getElementById('button')
  # Creates 10000 Ruby wrappers (memory leak)
end

# BETTER
element = JS.document.getElementById('button')
10000.times do
  # Reuse the wrapper
  element[:textContent] = "Click #{i}"
end
```

#### 2. Use Native Ruby When Possible

```ruby
# JS values are automatically converted to Ruby types
items = js_array  # Already a Ruby-accessible array
items.each do |item|  # Process in Ruby
  process(item)
end

# INSTEAD OF
js_array[:forEach].call(-> (item) {  # Repeated JS calls
  # Processing
})
```

#### 3. Handle Errors Gracefully

```ruby
element = JS.document.getElementById('nonexistent')
if element
  # Element exists
else
  # Element not found - many JS methods return nil for errors
end
```

#### 4. Clean Up Event Listeners

```ruby
# Store callback ID
callback_id = element.addEventListener("click") do |e|
  # Handle click
end

# Later, remove listener
element.removeEventListener(callback_id)
```

### Common Patterns

#### File Upload with Preview

```ruby
# Get file from input
input = JS.document.getElementById('file-input')
files = input[:files]
file = files.item(0)

# Create preview URL (no callback needed!)
preview_url = JS.global[:URL].createObjectURL(file)
img[:src] = preview_url
```

#### FormData with File Upload

```ruby
# Define helper in HTML first (see limitation #1 above)

# Use from Ruby
Funicular::FileUpload.upload_with_formdata(
  "/upload",
  fields: { name: "John" },
  file_field: 'avatar',
  file: file_object
) do |result|
  puts "Upload complete: #{result}"
end
```

#### Fetch with JSON

```ruby
JS.global.fetch("/api/users", {
  method: "POST",
  headers: { "Content-Type" => "application/json" },
  body: JSON.generate({ name: "Alice" })
}) do |response|
  data = JSON.parse(response.to_binary)
  puts "Created user: #{data['id']}"
end
```

### Debugging Tips

1. **Check Browser Console**: JavaScript errors appear in the browser console, not Ruby output
2. **Inspect Objects**: Use `obj.inspect` to see JS::Object wrapper details
3. **Access Properties**: JavaScript primitive values are automatically converted to Ruby types when accessed via `[]`:
   ```ruby
   obj[:propertyName]  # Returns Ruby String, Integer, true/false, nil, etc.
   ```
4. **Enable Debug Output**: Use `puts` liberally to trace execution flow

## References

- [Emscripten setjmp/longjmp Support](https://emscripten.org/docs/porting/setjmp-longjmp.html)
- [Emscripten ASYNCIFY](https://emscripten.org/docs/porting/asyncify.html)
- [PicoRuby Task Scheduler](../../picoruby-mruby/include/task.h)

