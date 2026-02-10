# Funicular

> 🎵Funicu-lì, Funicu-là!🚊🚊🚊
>
> 🎵Funicu-lì, Funicu-là!🚞🚞🚞

**Funicular** is a single-page application (SPA) framework powered by PicoRuby.wasm.
Named after a cable-driven railway, it lets you build apps using pure Ruby, with no JavaScript or HTML required.[^1]

[^1]: 子曰知止而后有定 / Confucius, *When one knows where to stop, one can be steady.*

## Architecture Overview

Funicular is a **unidirectional, Virtual DOM-based SPA framework** that adopts design patterns similar to React while embracing Ruby's expressiveness and integrating seamlessly with Rails.

### Core Design Decisions

| Aspect | Choice | Philosophy |
|--------|--------|------------|
| **Data Flow** | Unidirectional | Explicit state updates via `patch()` for predictability |
| **Rendering** | Virtual DOM + Diffing | Efficient DOM updates, declarative UI |
| **Components** | Class-based | Clear organization with lifecycle hooks |
| **State Management** | Local + Props | Simple by default, no global store |
| **Routing** | Client-side (History API) | SPA navigation with Rails-style DSL |
| **Templates** | Ruby DSL | Full Ruby expressiveness, no JSX |
| **Build Strategy** | Runtime (No build step) | Instant feedback during development |
| **Reactivity** | Explicit | Manual `patch()` calls, no auto-tracking magic |

### Framework Comparison

**Funicular vs React**:
- Similar: Unidirectional flow, VDOM, component-based
- Different: Ruby DSL instead of JSX, no build step, Rails integration

**Funicular vs Vue**:
- Similar: Component-based, developer-friendly
- Different: Unidirectional (not v-model), explicit updates (not reactive proxy)

**Key Differentiator**: Pure Ruby browser applications with zero JavaScript, powered by PicoRuby.wasm.

**Read more**: [Architecture Deep Dive](docs/architecture.md)

## Features

- **[Pure Ruby Browser App](#pure-ruby-browser-app)** - Write frontend code entirely in Ruby
- **[Object-REST Mapper](#object-rest-mapper-orm)** - ActiveRecord-style API for REST backends
- **[ActionCable WebSocket](#actioncable-compatible-websocket)** - Real-time features with Rails integration
- **[Rails Integration](#rails-integration)** - Seamless Rails API communication with CSRF handling
- **[CSS-in-Ruby](#css-in-ruby-with-styles-dsl)** - Scoped styles with conditional variants
- **[Form Builder](#rails-style-form-builder)** - Rails-style forms with validation errors
- **[Routing & Navigation](#routing-and-navigation)** - Client-side routing with URL helpers
- **[Error Boundary](#error-boundary)** - Graceful error handling for component failures
- **[Suspense](#suspense--loading-state)** - Declarative async data loading with loading states
- **[JS Integration](#js-integration)** - Delegation model for Chart.js, D3.js, etc.

## Quick Start

### Hello World

```ruby
class Counter < Funicular::Component
  def initialize_state
    { count: 0 }
  end

  def increment
    patch(count: state.count + 1)
  end

  def decrement
    patch(count: state.count - 1)
  end

  def render
    div do
      h1 { "Counter" }
      p { "Current count: #{state.count}" }
      button(onclick: :increment) { "Increment" }
      button(onclick: :decrement) { "Decrement" }
      # Or use inline lambda for simple logic
      # button(onclick: -> { patch(count: state.count + 1) }) { "Increment" }
    end
  end
end

Funicular.start(Counter, container: "app")
```

### With Router

```ruby
Funicular.start(container: 'app') do |router|
  router.get('/login', to: LoginComponent, as: 'login')
  router.get('/dashboard', to: DashboardComponent, as: 'dashboard')
  router.set_default('/login')
end
```

## Pure Ruby Browser App

Funicular is a component-based frontend framework that allows you to build browser applications entirely in Ruby, powered by PicoRuby.wasm. It uses a Virtual DOM (VDOM) to efficiently update the UI.

```ruby
class TodoApp < Funicular::Component
  def initialize_state
    { todos: [], input: "" }
  end

  def handle_input(event)
    patch(input: event.target[:value])
  end

  def handle_add
    new_todo = { id: Time.now.to_i, text: state.input, done: false }
    patch(todos: state.todos + [new_todo], input: "")
  end

  def render
    div do
      h1 { "Todo List" }
      input(
        value: state.input,
        oninput: :handle_input
      )
      button(onclick: :handle_add) { "Add" }

      state.todos.each do |todo|
        div(key: todo[:id]) { todo[:text] }
      end
    end
  end
end
```

**Learn more**: [Components and State Management](docs/components-and-state.md)

## Object-REST Mapper (O-R-M)

Funicular includes a built-in Object-REST Mapper that provides an ActiveRecord-like interface for interacting with REST APIs.

```ruby
class User < Funicular::Model
end

# Fetch all users
User.all do |users, error|
  patch(users: users)
end

# Find specific user
User.find(123) do |user, error|
  patch(user: user)
end

# Create user
User.create(name: "Alice", email: "alice@example.com") do |user, errors|
  if errors
    patch(errors: errors)
  else
    patch(user: user, success: "User created!")
  end
end

# Update user
user.update(email: "newemail@example.com") do |updated_user, errors|
  patch(user: updated_user)
end
```

**Learn more**: [Data Fetching](docs/data-fetching.md)

## ActionCable-compatible WebSocket

Real-time features powered by ActionCable-compatible WebSocket client.

```ruby
class ChatComponent < Funicular::Component
  def component_mounted
    consumer = Funicular::Cable.create_consumer("/cable")

    @subscription = consumer.subscriptions.create(
      channel: "ChatChannel",
      room: "lobby"
    ) do |message|
      patch(messages: state.messages + [message])
    end
  end

  def handle_input(event)
    patch(input: event.target[:value])
  end

  def handle_send
    @subscription.perform("speak", message: state.input)
    patch(input: "")
  end

  def render
    div do
      state.messages.each { |msg| div { msg["content"] } }
      input(value: state.input, oninput: :handle_input)
      button(onclick: :handle_send) { "Send" }
    end
  end
end
```

**Learn more**: [ActionCable Integration](docs/realtime.md)

## Rails Integration

- **CSRF Token Handling**: Automatic inclusion in POST/PATCH/PUT/DELETE requests
- **ActionCable Compatible**: Real-time features with Rails channels
- **Schema Loading**: Dynamic model attribute definitions from Rails API
- **Zero JS Workflow**: Full-stack Ruby development

**Learn more**: [Data Fetching](docs/data-fetching.md), [Real-time Features](docs/realtime.md)

## CSS-in-Ruby with Styles DSL

Keep your styles organized and scoped within each component.

```ruby
class LoginComponent < Funicular::Component
  styles do
    container "min-h-screen flex items-center justify-center bg-gradient-to-br from-blue-500 to-purple-600"
    card "bg-white p-8 rounded-lg shadow-2xl w-96"
    title "text-3xl font-bold text-center mb-8 text-gray-800"

    # Conditional styles
    button base: "px-4 py-2 rounded font-semibold",
           variants: {
             primary: "bg-blue-600 text-white hover:bg-blue-700",
             danger: "bg-red-600 text-white hover:bg-red-700"
           }
  end

  def render
    div(class: s.container) do
      div(class: s.card) do
        h1(class: s.title) { "Welcome" }
        button(class: s.button(:primary)) { "Login" }
      end
    end
  end
end
```

**Learn more**: [Styling Guide](docs/styling.md)

## Rails-style Form Builder

Automatic form state management and error display.

```ruby
class SignupComponent < Funicular::Component
  def initialize_state
    { user: { username: "", email: "" }, errors: {} }
  end

  def handle_submit(form_data)
    User.create(form_data) do |user, errors|
      if errors
        patch(errors: errors)
      else
        Funicular.router.navigate('/dashboard')
      end
    end
  end

  def render
    form_for(:user, on_submit: :handle_submit) do |f|
      f.label(:username)
      f.text_field(:username, autofocus: true)

      f.label(:email)
      f.email_field(:email)

      f.submit("Sign Up")
    end
  end
end
```

**Learn more**: [Forms Guide](docs/forms.md)

## Routing and Navigation

Client-side routing with Rails-style DSL and URL helpers.

```ruby
Funicular.start(container: 'app') do |router|
  router.get('/users/:id', to: UserProfileComponent, as: 'user')
  router.get('/settings', to: SettingsComponent, as: 'settings')
end

# Use URL helpers
include Funicular::RouteHelpers

link_to user_path(user), navigate: true do
  span { user.name }
end

link_to settings_path, navigate: true, class: "nav-link" do
  span { "Settings" }
end
```

**Learn more**: [Routing and Navigation](docs/routing-and-navigation.md)

## Error Boundary

Catch errors from child components and display fallback UI.

```ruby
component(Funicular::ErrorBoundary,
  fallback: ->(error) {
    div(class: "error") do
      h3 { "Oops! Something went wrong" }
      p { "Error: #{error.message}" }
    end
  }
) do
  component(RiskyComponent)
end
```

**Learn more**: [Advanced Features - Error Boundary](docs/advanced-features.md#error-boundary)

## Suspense / Loading State

Declarative async data loading with loading states.

```ruby
class UserProfile < Funicular::Component
  use_suspense :user,
    ->(resolve, reject) {
      User.find(props[:id]) do |user, error|
        error ? reject.call(error) : resolve.call(user)
      end
    }

  def render
    suspense(
      fallback: -> { div { "Loading..." } },
      error: ->(e) { div { "Failed: #{e}" } }
    ) do
      div do
        h1 { user.name }
        p { user.email }
      end
    end
  end
end
```

**Learn more**: [Data Fetching - Suspense](docs/data-fetching.md#suspense--loading-states)

## JS Integration

Delegation model for integrating JavaScript libraries (Chart.js, D3.js, etc.).

```ruby
class ChartComponent < Funicular::Component
  def component_mounted
    canvas = refs[:chart_canvas]
    @chart = JS.global.Chart.new(canvas, {
      type: 'bar',
      data: { labels: state.labels, datasets: [...] }
    })
  end

  def component_unmounted
    @chart&.destroy()
  end

  def render
    div { canvas(ref: :chart_canvas) }
  end
end
```

**Learn more**: [Advanced Features - JS Integration](docs/advanced-features.md#js-integration-via-delegation-model)

## Documentation

### Architecture
- [**Architecture Deep Dive**](docs/architecture.md) - Design decisions, comparisons, trade-offs

### Core Concepts
- [**Components and State**](docs/components-and-state.md) - Component lifecycle, state management, props
- [**Styling**](docs/styling.md) - CSS-in-Ruby Styles DSL, conditional styles, variants
- [**Forms**](docs/forms.md) - Form builder, validation, error handling

### Features
- [**Routing and Navigation**](docs/routing-and-navigation.md) - Router, URL helpers, link_to
- [**Data Fetching**](docs/data-fetching.md) - HTTP client, Model (O-R-M), Suspense
- [**Real-time**](docs/realtime.md) - ActionCable WebSocket integration

### Advanced
- [**Advanced Features**](docs/advanced-features.md) - Error Boundary, CSS Transitions, JS Integration

### Related Documentation
- [picoruby-wasm/docs/callback.md](../picoruby-wasm/docs/callback.md) - Callback system
- [picoruby-wasm/docs/interoperability_between_js_and_ruby.md](../picoruby-wasm/docs/interoperability_between_js_and_ruby.md) - JS::Bridge for Ruby/JS interop

## Best Use Cases

### Well-Suited For

- Rails applications with SPA features
- Small to medium SPAs (dashboards, admin panels, chat apps)
- Ruby teams doing frontend development
- Rapid prototyping of interactive UIs

### Not Ideal For

- SEO-critical applications (no SSR support for now, but planning...)
- Large-scale SPAs with complex state management needs
- Performance-critical applications (WebAssembly overhead)
- Mobile applications (use React Native or native solutions)

**Read more**: [Architecture - Trade-offs](docs/architecture.md#trade-offs)

## License

MIT
