# Picoruby-Funicular Analysis Report

**Date**: 2025-12-28
**Analyzed by**: Claude Opus 4.5

---

## Executive Summary

picoruby-funicular is a pure Ruby SPA framework powered by picoruby-wasm, enabling developers to write 100% Ruby client-side applications. This report analyzes the current state and recommends features for a robust SPA framework.

---

## Current Feature Set

### Core Framework (mrbgem: 2,993 lines)

| Feature | File | Lines | Status |
|---------|------|-------|--------|
| Component System | component.rb | 685 | Complete |
| Virtual DOM | vdom.rb, differ.rb, patcher.rb | 645 | Complete |
| Client-side Routing | router.rb | 235 | Complete |
| Form Builder | form_builder.rb | 284 | Complete |
| HTTP Client | http.rb | 89 | Complete |
| O-R-M (REST) | model.rb | 145 | Complete |
| ActionCable | cable.rb | 329 | Complete |
| CSS-in-Ruby | styles.rb | 82 | Complete |
| File Upload | file_upload.rb | 180 | Complete |
| Debug Tools | debug.rb | 143 | Complete |

### Rails Plugin (funicular gem: 385 lines)

- Compiler (Ruby to .mrb bytecode)
- Development middleware (auto-recompilation)
- Railtie integration
- Rake tasks (compile, routes, install)

---

## Recommended Features by Priority

### Critical (Robustness)

#### 1. Error Boundary

**Problem**: Child component errors crash the entire application. `component_raised` hook exists but lacks UI fallback mechanism.

**Proposed Implementation**:

```ruby
class ErrorBoundary < Component
  def initialize_state
    { has_error: false, error: nil }
  end

  def component_raised(error)
    patch(has_error: true, error: error)
  end

  def render
    if state[:has_error]
      props[:fallback]&.call(state[:error]) || div { "Something went wrong" }
    else
      props[:children]
    end
  end
end
```

#### 2. Suspense / Loading State

**Problem**: No standardized pattern for async data fetching with loading indicators.

**Proposed API**:

```ruby
class UserProfile < Component
  use_suspense :user, -> { User.find(props[:id]) }

  def render
    suspense(fallback: -> { div { "Loading..." } }) do
      div { user.name }
    end
  end
end
```

#### 3. ActionCable Reliability

**Problem**: Reconnection logic exists but lacks guaranteed delivery for queued messages.

**Recommendations**:
- Implement message acknowledgment protocol
- Add message deduplication
- Persist queue to localStorage during disconnection

#### 4. Component Unmount Cleanup

**Problem**: Event listeners and WebSocket subscriptions may leak.

**Recommendations**:
- Automatic subscription cleanup registry
- Warning in development mode for orphaned listeners

---

### High Priority (Developer Experience)

#### 1. Context API / Global State Management

**Problem**: Props drilling required for deep component trees.

**Proposed Implementation**:

```ruby
# context.rb
module Funicular
  class Context
    @@current_values = {}

    def initialize(name, default_value = nil)
      @name = name
      @default = default_value
    end

    def provide(value)
      @@current_values[@name] = value
      yield
    ensure
      @@current_values.delete(@name)
    end

    def consume
      @@current_values[@name] || @default
    end
  end
end

# Usage
AuthContext = Funicular::Context.new(:auth)

class App < Component
  def render
    AuthContext.provide(current_user) do
      child(Dashboard)
    end
  end
end

class DeepNestedComponent < Component
  def render
    user = AuthContext.consume
    div { "Hello, #{user.name}" }
  end
end
```

#### 2. Lazy Loading / Code Splitting

**Problem**: Large applications suffer from slow initial load times.

**Proposed API**:

```ruby
# Lazy component loading
LazySettings = Funicular.lazy { SettingsComponent }

router.get('/settings', to: LazySettings, as: 'settings')
```

**Implementation Notes**:
- Split .mrb files per route or component group
- Dynamic loading via fetch + WASM execution

#### 3. Router Guards / Middleware

**Problem**: No route transition interception for auth checks.

**Proposed Implementation**:

```ruby
# router.rb addition
class Router
  def before_each(&block)
    @guards << block
  end

  def after_each(&block)
    @after_hooks << block
  end
end

# Usage
Funicular.start(container: 'app') do |router|
  router.before_each do |to, from, next_fn|
    if to[:meta][:requires_auth] && !Session.current_user
      next_fn.call('/login')
    else
      next_fn.call
    end
  end

  router.get('/dashboard', to: Dashboard, as: 'dashboard', meta: { requires_auth: true })
end
```

#### 4. Reactive Route Parameters

**Problem**: URL parameter changes don't automatically trigger re-render.

**Current Behavior**:
```ruby
# User must manually watch for changes
props[:channel_id]  # Static at mount time
```

**Proposed Behavior**:
```ruby
# Automatic reactivity
watch(:route_params) do |new_params, old_params|
  load_channel(new_params[:channel_id])
end
```

#### 5. Computed Properties

**Problem**: Derived values from state lack caching/memoization.

**Proposed API**:

```ruby
class MessageList < Component
  computed :filtered_messages do
    state[:messages].select { |m| m.channel_id == state[:current_channel] }
  end

  computed :unread_count, depends_on: [:messages, :last_read_at] do
    state[:messages].count { |m| m.created_at > state[:last_read_at] }
  end
end
```

---

### Medium Priority (Differentiation)

#### 1. Server-Side Rendering (SSR)

**Benefits**:
- SEO optimization
- Faster initial paint
- Progressive enhancement

**Challenges**:
- PicoRuby needs to run in Node.js or Ruby backend
- Hydration strategy required

#### 2. DevTools Enhancement

**Current**: Component tree view, state inspection

**Proposed Additions**:
- Time-travel debugging (state history)
- Action replay
- Performance profiling
- Network request timeline

#### 3. Hot Module Replacement (HMR)

**Problem**: Full page reload loses application state during development.

**Proposed**:
- WebSocket connection to dev server
- Component-level hot swap
- State preservation across reloads

#### 4. Static Analysis Enhancement

**Current**: RBS type signatures exist

**Proposed**:
- Editor integration (LSP)
- Compile-time type checking
- Unused code detection
- Circular dependency warnings

#### 5. Testing Utilities

**Proposed API**:

```ruby
# test_helper.rb
class ComponentTest < Funicular::TestCase
  def test_counter_increment
    component = mount(Counter, initial_count: 0)

    component.find('button.increment').click

    assert_equal 1, component.state[:count]
    assert_text '1', component.find('.count')
  end
end
```

---

### Nice to Have (Convenience)

#### 1. Slot / Children Props

```ruby
class Card < Component
  def render
    div(class: 'card') do
      div(class: 'header') { slot(:header) }
      div(class: 'body') { slot(:default) }
      div(class: 'footer') { slot(:footer) }
    end
  end
end

# Usage
child(Card) do
  slot(:header) { h2 { "Title" } }
  slot(:footer) { button { "OK" } }
  p { "Card content" }  # default slot
end
```

#### 2. Transition Group

```ruby
class MessageList < Component
  def render
    transition_group(tag: 'ul', name: 'message') do
      state[:messages].map do |msg|
        li(key: msg.id) { msg.content }
      end
    end
  end
end
```

#### 3. Portal

```ruby
class Modal < Component
  def render
    portal(to: 'modal-root') do
      div(class: 'modal-backdrop') do
        div(class: 'modal-content') do
          props[:children]
        end
      end
    end
  end
end
```

#### 4. Form Validation DSL

```ruby
class SignupForm < Component
  validates :username, presence: true, length: { min: 3, max: 20 }
  validates :email, presence: true, format: :email
  validates :password, presence: true, length: { min: 8 }
  validates :password_confirmation, confirmation: :password

  def render
    form_for(:user, on_submit: method(:handle_submit), validate: true) do |f|
      f.text_field(:username)
      f.email_field(:email)
      f.password_field(:password)
      f.password_field(:password_confirmation)
      f.submit("Sign Up")
    end
  end
end
```

#### 5. i18n Support

```ruby
# config/locales/en.yml
# config/locales/ja.yml

class Greeting < Component
  def render
    h1 { t('greeting.hello', name: props[:name]) }
  end
end
```

---

## Recommended Action Plan

### Phase 1: Foundation (Immediate)

1. **Error Boundary** - Prevent cascade failures
2. **Suspense Pattern** - Standardize async loading
3. **Router Guards** - Enable auth middleware

### Phase 2: Developer Experience (Short-term)

4. **Context API** - Solve props drilling
5. **Computed Properties** - Optimize derived state
6. **Testing Utilities** - Enable TDD workflow

### Phase 3: Scale (Medium-term)

7. **Lazy Loading** - Reduce initial bundle
8. **HMR** - Improve dev iteration speed
9. **DevTools Enhancement** - Time-travel debugging

### Phase 4: Production (Long-term)

10. **SSR** - SEO and performance
11. **Static Analysis** - Catch errors early
12. **i18n** - Global market support

---

## Comparison with Other Frameworks

| Feature | Funicular | React | Vue | Svelte |
|---------|-----------|-------|-----|--------|
| Language | Ruby | JavaScript | JavaScript | JavaScript |
| VDOM | Yes | Yes | Yes | No (compile-time) |
| Component System | Yes | Yes | Yes | Yes |
| Routing | Built-in | react-router | vue-router | svelte-kit |
| State Management | Local + (Context TBD) | useState + Context | ref/reactive + Pinia | $state |
| Error Boundary | Partial | Yes | Yes | Yes |
| SSR | No | Yes | Yes | Yes |
| DevTools | Basic | Excellent | Excellent | Good |
| Bundle Size | ~100KB (WASM) | ~40KB | ~30KB | ~5KB |

---

## Conclusion

picoruby-funicular has a solid foundation with comprehensive features for building SPAs in pure Ruby. The key areas for improvement are:

1. **Reliability**: Error boundaries and cleanup mechanisms
2. **Scalability**: Global state management and lazy loading
3. **Developer Experience**: Router guards, testing utilities, and HMR

The framework's unique value proposition (100% Ruby, no JavaScript) positions it well for teams that want to leverage Ruby expertise across the full stack. Focus on the recommended priorities will make it a robust choice for production applications.
