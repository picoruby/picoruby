# PicoRuby.wasm Architecture

This document explains the core architecture of PicoRuby.wasm, focusing on the task-based execution model and how asynchronous operations integrate with exception handling.

## Task-Based Execution Model

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

## Asynchronous Operations and Exception Safety

A critical design aspect is how **asynchronous JavaScript operations** (like `fetch()`, `setTimeout()`, `addEventListener()`) integrate with Ruby's **exception handling** (raise/rescue).

### The Challenge

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

### Our Solution: Task Suspension Model

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

## Why ASYNCIFY=1 is Not Needed

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
│       ├── js.c             # JavaScript interop, callbacks, timers
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
  - `js_set_timeout()`: Timer scheduling
  - `call_ruby_callback()`: Generic callback invocation

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

## Memory Management

### JavaScript Reference Management

JavaScript object references are stored in `globalThis.picorubyRefs` array:

```javascript
// Every JS method call adds a new reference:
const newRefId = globalThis.picorubyRefs.push(element) - 1;
```

**Current Limitation**: References are never removed, leading to potential memory leaks in long-running applications. See Future Considerations section in main README.

### Task Contexts

Task contexts (TCB) are properly managed:
- Allocated in heap via `mrb_malloc()`
- Freed when task completes or is terminated
- Each task's `mrb_context` includes its own Ruby stack

No known issues with task memory management.

## See Also

- [async_operations.md](async_operations.md) - Guide to using async APIs
- [callback.md](callback.md) - Callback system details
- [interoperability_between_js_and_ruby.md](interoperability_between_js_and_ruby.md) - Data conversion
