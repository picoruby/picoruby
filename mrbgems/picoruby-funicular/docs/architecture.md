# Funicular Architecture

This document provides a comprehensive analysis of Funicular's architectural design decisions and how they compare to other modern SPA frameworks.

## Table of Contents

- [Overview](#overview)
- [Architecture Design Decisions](#architecture-design-decisions)
  - [1. Data Flow](#1-data-flow)
  - [2. Rendering Strategy](#2-rendering-strategy)
  - [3. Reactivity Model](#3-reactivity-model)
  - [4. Component Model](#4-component-model)
  - [5. State Management](#5-state-management)
  - [6. Routing](#6-routing)
  - [7. Rendering Mode](#7-rendering-mode)
  - [8. Template System](#8-template-system)
  - [9. Build Strategy](#9-build-strategy)
  - [10. Concurrency](#10-concurrency)
  - [11. Event System](#11-event-system)
  - [12. Error Handling](#12-error-handling)
  - [13. Data Fetching](#13-data-fetching)
- [Comparison with Other Frameworks](#comparison-with-other-frameworks)
- [Design Philosophy](#design-philosophy)
- [Trade-offs](#trade-offs)
- [Best Use Cases](#best-use-cases)

## Overview

Funicular is a **unidirectional, Virtual DOM-based SPA framework** for PicoRuby.wasm. It adopts design patterns similar to React while embracing Ruby's expressiveness and integrating seamlessly with Rails.

**Core Principle**: Pure Ruby browser applications with explicit, predictable data flow.

## Architecture Design Decisions

### 1. Data Flow

**Choice**: Unidirectional (One-way) Data Binding

```ruby
# State flows DOWN to UI
state -> VDOM -> DOM rendering

# Events flow UP to trigger state updates
DOM events -> event handlers -> patch() -> state update -> re-render
```

**Evidence**:
- State is immutable, updatable only via `patch()`
- Direct state mutation is blocked by `StateAccessor`
- Form inputs require explicit event handlers + `patch()` calls

**Comparison**:

| Framework | Binding Type | Update Mechanism |
|-----------|--------------|------------------|
| **Funicular** | Unidirectional | `patch()` + VDOM diffing |
| React | Unidirectional | `setState()` + VDOM diffing |
| Vue 2 | Bidirectional (v-model) | Reactivity proxy + VDOM |
| Angular | Bidirectional | Two-way data binding |
| Svelte | Unidirectional | Compile-time reactivity |

### 2. Rendering Strategy

**Choice**: Virtual DOM with Diffing Algorithm

**Implementation**:
- Custom VDOM implementation (`vdom.rb`)
- `Differ.diff()` calculates minimal changes
- `Patcher.apply()` applies patches to real DOM
- **Key-based reconciliation** for efficient list updates

**Code Reference**:
```ruby
# Component re-renders when state changes
def re_render
  new_vdom = build_vdom
  patches = VDOM::Differ.diff(@vdom, new_vdom)
  @dom_element = VDOM::Patcher.apply(@dom_element, patches)
  @vdom = new_vdom
end
```

**Benefits**:
- Efficient DOM updates (minimal mutations)
- Declarative UI (describe what, not how)
- Cross-platform potential (VDOM is abstraction layer)

### 3. Reactivity Model

**Choice**: Explicit Updates (Manual Trigger)

| Approach | Mechanism | Examples |
|----------|-----------|----------|
| **Explicit Updates** | Manual `setState()`/`patch()` | React, **Funicular** |
| Auto-tracking | Proxy/Getter dependency tracking | Vue 3, Solid.js |
| Compile-time | Compiler analyzes dependencies | Svelte |

**Example**:
```ruby
def handle_input(event)
  patch(username: event.target[:value])  # Explicit call
end
```

**Benefits**:
- Simple, predictable
- Low runtime overhead
- Easy to debug (explicit control flow)

### 4. Component Model

**Choice**: Class-based Components

```ruby
class ChatComponent < Funicular::Component
  def initialize_state
    { messages: [] }
  end

  def component_mounted
    # Lifecycle hook
  end

  def render
    div { "Chat UI" }
  end
end
```

**Features**:
- Lifecycle hooks: `component_mounted`, `component_unmounted`
- Instance variables for component-local data
- Suspense support for async data loading
- Similar to React Class Components

### 5. State Management

**Choice**: Local State + Props Drilling + Model Layer

| Approach | Description | Example |
|----------|-------------|---------|
| **Local State** | Component-scoped `@state` | **Funicular** |
| Global Store | Centralized state tree | Redux, Vuex |
| Context/DI | Share state within tree | React Context |

**Architecture**:
- Components manage their own state (`@state`)
- Parent-to-child communication via `props`
- Server state managed by `Model` layer (ActiveRecord-style API)

**No Global Store**: Funicular intentionally omits Redux-style global state to keep things simple. For shared state, use:
- Props drilling
- Model layer for server data
- Component composition

### 6. Routing

**Choice**: Client-Side Routing (History API)

```ruby
Funicular.start(container: 'app') do |router|
  router.get('/chat/:channel_id', to: ChatComponent, as: 'chat_channel')
end
```

**Features**:
- Rails-style DSL
- URL parameter extraction (`/chat/:id` -> `{ id: "123" }`)
- Auto-generated URL helpers: `chat_channel_path(id)`
- History API for SPA navigation

### 7. Rendering Mode

**Choice**: Pure Client-Side Rendering (CSR)

| Mode | Description | Funicular Support |
|------|-------------|-------------------|
| **CSR** | Render in browser | ✅ Yes |
| SSR | Server-side rendering | ❌ No (for now...) |
| SSG | Static site generation | ❌ No |
| ISR | Incremental static regen | ❌ No |

**Reason**: PicoRuby.wasm runs only in browsers. Rails serves JSON APIs + assets.

### 8. Template System

**Choice**: Ruby DSL (Not JSX or Template Strings)

```ruby
def render
  div(class: 'container') do
    h1 { 'Welcome' }
    input(value: state.username, oninput: :handle_input)
  end
end
```

**Features**:
- HTML tag names are Ruby methods
- Blocks for child elements
- Event handlers as symbols or Procs
- Full Ruby expressiveness (loops, conditionals, etc.)

**Comparison**:

| Framework | Template Syntax |
|-----------|-----------------|
| React | JSX (XML-like) |
| Vue | Template strings with directives |
| Svelte | HTML-like template syntax |
| **Funicular** | **Ruby DSL** |

### 9. Build Strategy

**Choice**: Runtime Execution (No Build Step)

| Approach | Description | Examples |
|----------|-------------|----------|
| **Runtime** | Code runs directly in browser | Vue (CDN), **Funicular** |
| Compile-time | Pre-build required | Svelte, Angular |
| Hybrid | JSX compiled, runtime VDOM | React |

**Funicular's Approach**:
- `app/funicular/**/*.rb` files are compiled to mruby bytecode (`.mrb`) via `picorbc`
- Compilation output: `app/assets/builds/app.mrb`
- Rails autoloading is explicitly disabled for `app/funicular/` directory
- Asset pipeline automatically handles compilation (hooks into `assets:precompile`)
- Developers don't need to manually compile (transparent automation)

**Build Process**:
```bash
# Automatic in production
rake assets:precompile  # -> calls funicular:compile

# Manual compilation (if needed)
rake funicular:compile
```

**Benefits**:
- Automated compilation via asset pipeline
- Developers work with plain Ruby files
- Production serves optimized bytecode
- No separate build tool required (uses existing Rails toolchain)

### 10. Concurrency

**Choice**: Synchronous Rendering + Async Data Fetching

- `patch()` triggers immediate `re_render()`
- No batching or scheduling
- Suspense feature for async data with loading states
- `min_delay` option prevents spinner flickering

```ruby
use_suspense :user,
  ->(resolve, reject) { User.find(id, &resolve) },
  min_delay: 300  # Minimum loading spinner duration
```

### 11. Event System

**Choice**: Native DOM Events (Not Synthetic Events)

- Direct `addEventListener` usage
- Event listeners re-bound on each re-render
- `cleanup_events()` prevents memory leaks
- No event pooling or synthetic event objects

**Unified Callback Handling**:

All event handlers accept `Symbol | Method | Proc`:

```ruby
# All valid:
button(onclick: :handle_click)                    # Symbol (recommended)
button(onclick: method(:handle_click))            # Method (explicit)
button(onclick: -> { handle_click })              # Proc (inline)
button(onclick: -> { patch(count: count + 1) })  # Proc (inline logic)
```

**Recommendation**:
- Use `:symbol` for method references (concise)
- Use `-> { }` for inline logic
- Use `method(:name)` when passing callbacks to child components

### 12. Error Handling

**Choice**: Error Boundary Pattern (React-style)

```ruby
component(Funicular::ErrorBoundary,
  fallback: ->(error) { div { "Error: #{error.message}" } }
) do
  component(RiskyComponent)
end
```

- Catches errors from child components
- Displays fallback UI
- Prevents entire app crash
- Stack-based boundary resolution

### 13. Data Fetching

**Choice**: Manual Fetch + Model Abstraction Layer

**Low-level**: `HTTP.get`, `HTTP.post`
**High-level**: `Model.all`, `Model.find`, `Model.create`

```ruby
# ActiveRecord-style API
User.all do |users, error|
  patch(users: users)
end
```

**Features**:
- Callback-based (not Promise-based)
- Automatic CSRF token handling
- Rails API integration
- No built-in caching (like SWR/React Query)

## Comparison with Other Frameworks

### Funicular vs React

| Aspect | Funicular | React |
|--------|-----------|-------|
| Language | Ruby | JavaScript/JSX |
| Components | Class-based | Function (Hooks) or Class |
| State Update | `patch()` | `setState()` / `useState()` |
| VDOM | Custom implementation | Custom implementation |
| Data Fetching | Model layer | Manual / libraries |
| Routing | Built-in | Separate library |
| Build Step | Asset Pipeline integration | Required (Babel/JSX) |

### Funicular vs Vue

| Aspect | Funicular | Vue |
|--------|-----------|-----|
| Data Binding | Unidirectional | Bidirectional (v-model) |
| Reactivity | Explicit `patch()` | Auto-tracking (Proxy) |
| Templates | Ruby DSL | HTML-like templates |
| SSR | No | Yes |

### Funicular vs Svelte

| Aspect | Funicular | Svelte |
|--------|-----------|--------|
| VDOM | Yes | No (compiles to imperative code) |
| Reactivity | Explicit | Compile-time analysis |
| Build Step | Asset Pipeline integration | Required (compiler) |

## Design Philosophy

Funicular embodies **"PicoRuby.wasm-based React"** with these principles:

1. **Ruby First**: Leverage Ruby's expressiveness for frontend development
2. **Explicit Over Magic**: Predictable, explicit state updates
3. **Rails Integration**: Seamless Rails API + ActionCable + Asset Pipeline integration
4. **Simple by Default**: No global state, no complex build tools
5. **Progressive Enhancement**: Start simple, add complexity when needed

## Trade-offs

### Strengths

✅ **Ruby Expressiveness**: Full Ruby syntax in templates
```ruby
state.messages.map { |msg| div(key: msg.id) { msg.content } }
```

✅ **No Build Step**: Instant development feedback

✅ **Rails Integration**: CSRF tokens, ActiveRecord-style APIs

✅ **Lightweight Dependencies**: Minimal gem dependencies

### Limitations

❌ **No SSR**: PicoRuby.wasm is browser-only (for now...)

❌ **No npm Ecosystem**: Limited to Ruby gems

❌ **State Management**: No global store (can be complex at scale)

❌ **Performance Overhead**: WebAssembly Ruby execution slower than native JS

## Best Use Cases

### ✅ Well-Suited For:

- **Rails SPA Features**: Adding interactive UI to Rails apps
- **Small to Medium SPAs**: Dashboard, admin panels, chat apps
- **Ruby Teams**: Frontend work by Ruby developers
- **Rapid Prototyping**: Quick interactive UI development

### ❌ Not Ideal For:

- **SEO-Critical Apps**: Needs SSR (use Rails views or Next.js)
- **Large-Scale SPAs**: Complex state management requirements
- **Mobile Apps**: Use React Native or native solutions
- **Performance-Critical**: Real-time games, high-frequency updates

## Conclusion

Funicular adopts proven patterns from React (unidirectional flow, VDOM) while providing unique value through **Ruby ecosystem integration**. It's not trying to replace JavaScript frameworks for all use cases, but rather offers a compelling alternative for Ruby teams building interactive web applications.

The architecture prioritizes **simplicity, predictability, and Rails integration** over features like maximum performance. This makes it an excellent choice for its target use cases: Ruby teams building SPAs as part of Rails applications.
