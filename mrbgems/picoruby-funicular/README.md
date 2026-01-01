# Funicular

> 🎵Funicu-lì, Funicu-là!🚊🚊🚊
>
> 🎵Funicu-lì, Funicu-là!🚞🚞🚞


**Funicular** is a single-page application (SPA) framework powered by PicoRuby.wasm.
Named after a cable-driven railway, it lets you build apps using pure Ruby, with no JavaScript or HTML required.[^1]

[^1]: 子曰*知止而后有定* / Confucius, *When one knows where to stop, one can be steady.*

## Features

- [Pure Ruby browser app](#pure-ruby-browser-app)
- [Object-REST Mapper (O**R**M)](#object-rest-mapper-orm)
- [ActionCable-compatible WebSocket](#actioncable-compatible-websocket)
- [Rails integration](#rails-integration)
- [CSS-in-Ruby with Styles DSL](#css-in-ruby-with-styles-dsl)
- [Rails-style Form Builder](#rails-style-form-builder)
- [Rails-style link_to Helper](#rails-style-link_to-helper)
- [CSS Transition Helpers](#css-transition-helpers)
- [Error Boundary](#error-boundary)
- [Suspense / Loading State](#suspense--loading-state)
- [JS Integration via Delegation Model](#js-integration-via-delegation-model)

### Pure Ruby browser app

Funicular is a component-based frontend framework that allows you to build browser applications entirely in Ruby, powered by PicoRuby.wasm. It embraces a "Zero JS, Zero HTML" philosophy, meaning you, as an application developer, can focus on writing Ruby code.

It uses a Virtual DOM (VDOM) to efficiently update the UI. You define your application's UI by creating components, which are just Ruby classes. When a component's state changes, Funicular intelligently re-renders only what's necessary.

Here's a simple counter example:

```ruby
class Counter < Funicular::Component
  def initialize_state
    { count: 0 }
  end

  def render
    div do
      h1 { "Counter" }
      p { "Current count: #{state.count}" }
      button(onclick: -> { update(count: state.count + 1) }) { "Increment" }
      button(onclick: -> { update(count: state.count - 1) }) { "Decrement" }
    end
  end
end

Funicular.start(Counter, container: "app")
```

### Object-REST Mapper (O**R**M)

Funicular includes a built-in Object-REST Mapper (O-R-M) that seamlessly bridges the gap between your frontend Ruby objects and your backend REST API. Define your models by inheriting from `Funicular::Model`, and you can interact with your server-side resources through a familiar, ActiveRecord-like interface.

Forget about manual `fetch` calls and JSON parsing. The O-R-M handles all the HTTP communication for you.

```ruby
# Define your model (attributes are loaded from a schema API)
class Post < Funicular::Model
end

# --- In your component ---

# Fetch all posts
Post.all do |posts|
  update(posts: posts)
end

# Create a new post
Post.create(title: "Hello Funicular", content: "This is my first post!") do |new_post|
  puts "Post created with ID: #{new_post.id}"
end

# Find a post and update it
Post.find(1) do |post|
  post.update(content: "Updated content.")
end
```

### ActionCable-compatible WebSocket

Bring real-time features to your application with Funicular's built-in, ActionCable-compatible WebSocket client. It allows your Pure Ruby frontend to communicate seamlessly with your Rails backend for live updates, notifications, chat, and more.

The `Funicular::Cable` module provides a simple API to create consumers and subscribe to channels, handling all the complexities of the ActionCable protocol, including automatic reconnection.

```ruby
# --- In your component's component_mounted lifecycle hook ---

# Create a consumer for your Rails app's cable endpoint
consumer = Funicular::Cable.create_consumer("/cable")

# Subscribe to a channel
@subscription = consumer.subscriptions.create(channel: "ChatChannel", room: "Funicular Users") do |message|
  # This block is called whenever a message is received from the server
  puts "New message: #{message}"
  add_message_to_state(message)
end

# --- Later, to send a message to the server ---
@subscription.perform("speak", message: "Hello from a Funicular app!")
```

### Rails integration

Funicular is designed to be a perfect companion for your Ruby on Rails applications. The integration is smooth and natural, enabling a true full-stack Ruby development experience.

- **Real-time with ActionCable:** As shown above, the WebSocket client is fully compatible with ActionCable, making real-time features a breeze.
- **Effortless API Communication:** The Object-REST Mapper works beautifully with Rails APIs. You can even set up a simple `schema` endpoint in your Rails app to dynamically define your models' attributes on the frontend.
- **"Zero JS" Workflow:** By combining Funicular on the frontend with Rails on the backend, you can build modern, interactive web applications without ever leaving the comfort of the Ruby ecosystem.

### CSS-in-Ruby with Styles DSL

Funicular provides a powerful CSS-in-Ruby DSL that keeps your styles organized and scoped within each component. This eliminates the need to scatter Tailwind classes throughout your render methods, improving maintainability and readability.

#### Basic Usage

Define styles using the `styles` block at the top of your component:

```ruby
class LoginComponent < Funicular::Component
  styles do
    container "min-h-screen flex items-center justify-center bg-gradient-to-br from-blue-500 to-purple-600"
    card "bg-white p-8 rounded-lg shadow-2xl w-96"
    title "text-3xl font-bold text-center mb-8 text-gray-800"
    input "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
  end

  def render
    div(class: s.container) do
      div(class: s.card) do
        h1(class: s.title) { "Welcome" }
        input(class: s.input, type: "text", placeholder: "Username")
      end
    end
  end
end
```

#### Conditional Styles with Boolean Arguments

For styles that toggle based on state (like active/inactive), use the `active:` key:

```ruby
styles do
  channel_item base: "p-4 hover:bg-gray-700 cursor-pointer",
               active: "bg-gray-700"
end

def render
  div(class: s.channel_item(is_active)) { "Channel Name" }
end
```

When `is_active` is `true`, the result is `"p-4 hover:bg-gray-700 cursor-pointer bg-gray-700"`. When `false`, only the base classes are applied.

#### Multiple State Variants with Symbols

For styles with multiple states (e.g., primary/danger buttons), use `variants:`:

```ruby
styles do
  button base: "px-4 py-2 rounded font-semibold",
         variants: {
           primary: "bg-blue-600 text-white hover:bg-blue-700",
           danger: "bg-red-600 text-white hover:bg-red-700",
           secondary: "bg-gray-600 text-white hover:bg-gray-700"
         }
end

def render
  button(class: s.button(:primary)) { "Submit" }
  button(class: s.button(:danger)) { "Delete" }
end
```

#### Combining Styles with the `|` Operator

Chain multiple styles together using the `|` operator:

```ruby
styles do
  flex "flex"
  items_center "items-center"
  gap_2 "gap-2"
  sidebar "w-64 bg-gray-800 text-white"
end

def render
  # Combine utility classes
  div(class: s.flex | s.items_center | s.gap_2) { "Content" }

  # Mix with custom classes
  div(class: s.sidebar | "custom-shadow") { "Sidebar" }

  # Combine with conditional styles
  div(class: s.channel_item(is_active) | "mb-2") { "Channel" }
end
```

The `|` operator automatically filters out `nil` values, making it perfect for conditional styling.

#### Key Benefits

- **Scoped Styles**: Styles are defined within components, avoiding global namespace pollution
- **Readability**: `s.button(:primary)` is more semantic than raw Tailwind classes
- **Maintainability**: Change styles in one place rather than hunting through render methods
- **Type Safety**: The DSL ensures style names are consistent throughout your component
- **Flexibility**: Mix DSL styles with raw strings when needed

### Rails-style Form Builder

Funicular provides a powerful `form_for` helper that brings Rails-style form building to your Pure Ruby frontend. The FormBuilder automatically manages form state, handles two-way data binding, and displays validation errors.

#### Basic Usage

```ruby
class SignupComponent < Funicular::Component
  def initialize_state
    {
      user: { username: "", email: "", password: "" },
      errors: {}
    }
  end

  def handle_submit(form_data)
    # Create user via O-R-M or API call
    User.create(form_data) do |user, errors|
      if errors
        update(errors: errors)
      else
        # Success: redirect or show success message
      end
    end
  end

  def render
    form_for(:user, on_submit: method(:handle_submit)) do |f|
      f.label(:username)
      f.text_field(:username, class: "form-input")

      f.label(:email)
      f.email_field(:email, class: "form-input")

      f.label(:password)
      f.password_field(:password, class: "form-input")

      f.submit("Sign Up", class: "btn btn-primary")
    end
  end
end
```

#### Available Field Types

The FormBuilder supports all standard HTML input types:

- `text_field` - Text input
- `password_field` - Password input
- `email_field` - Email input
- `number_field` - Number input
- `textarea` - Multi-line text area
- `checkbox` - Checkbox input
- `select` - Dropdown select (pass array of choices)
- `file_field` - File upload input
- `label` - Field label (auto-generates from field name)
- `submit` - Submit button

All fields support standard HTML boolean attributes (`autofocus`, `disabled`, `checked`, `readonly`, `required`, etc.) which can be set to `true` or `false`:

```ruby
f.text_field(:username, autofocus: true)
f.submit("Send", disabled: state.message.empty?)
```

#### Automatic Error Display

The FormBuilder automatically displays validation errors when they exist in `state.errors`. Errors are displayed below their respective fields with customizable styling:

```ruby
def initialize_state
  {
    user: { email: "" },
    errors: { email: "Email is invalid" }
  }
end

def render
  form_for(:user) do |f|
    f.email_field(:email, class: "w-full p-2 border rounded")
    # Error message automatically displayed in red below the field
    # Field automatically gets border-red-500 class
  end
end
```

#### Nested Field Support

The FormBuilder supports nested fields using dot notation:

```ruby
def initialize_state
  {
    user: {
      profile: { bio: "", location: "" }
    }
  }
end

def render
  form_for(:user) do |f|
    f.text_field("profile.bio")
    f.text_field("profile.location")
  end
end
```

#### Customizing Error Styles

Configure error display styles globally:

```ruby
Funicular.configure_forms do |config|
  config[:error_class] = "text-red-600 text-sm mt-1"
  config[:field_error_class] = "border-red-500"
end
```

Or per-form:

```ruby
form_for(:user, error_class: "error-text", field_error_class: "error-border") do |f|
  # ...
end
```

### Rails-style link_to Helper

Funicular provides a `link_to` helper that brings Rails-style navigation and actions to your Pure Ruby frontend. It intelligently chooses between navigation (using `<a>` tags) and actions (using `<div>` tags) based on the context.

#### Basic Usage

**Navigation Links** (use `navigate: true` for page transitions):

```ruby
# Navigate to a different page (uses <a> tag with real href)
link_to settings_path, navigate: true, class: "nav-link" do
  span { "Settings" }
end

# Normal click: SPA navigation via History API
# Right-click -> New Tab: Opens /settings in new tab
```

**Action Links** (default behavior for server actions):

```ruby
# Delete a resource (uses <div> tag, Fetch API)
link_to message_path(message), method: :delete, class: "delete-btn" do
  span { "Delete" }
end

# Supports all HTTP methods: :get, :post, :put, :patch, :delete
link_to post_path(post), method: :patch, class: "btn" do
  span { "Update" }
end
```

#### Why Different Elements?

Funicular uses semantic HTML to distinguish between navigation and actions:

- **Navigation** (`navigate: true`): Generates `<a href="/path">` tags
  - Browser features work: right-click menu, link preview, open in new tab
  - SPA navigation on normal click (uses `preventDefault()` + History API)
  - Semantically correct for page transitions

- **Actions** (default): Generates `<div>` tags
  - Fetch API for server communication
  - Semantically correct for operations (create, update, delete)
  - No `href` attribute needed

#### Integration with Rails Route Helpers

When using Funicular with Rails, route helpers are automatically available:

```ruby
# Route helpers work seamlessly
link_to user_path(user), navigate: true do
  span { user.name }
end

link_to post_path(post), method: :delete do
  span { "Delete Post" }
end

# Generated routes (from router.get '/settings', as: 'settings')
link_to settings_path, navigate: true do
  span { "Go to Settings" }
end
```

#### Advanced: Handling Responses

Override `handle_link_response` to customize action response handling:

```ruby
class MyComponent < Funicular::Component
  def handle_link_response(response, path, method)
    if response.error?
      puts "Action failed: #{response.error_message}"
      update(error: response.error_message)
    else
      puts "Action succeeded!"
      # Update component state based on response
    end
  end
end
```

### CSS Transition Helpers

Funicular provides built-in CSS transition helpers to add smooth animations when elements enter or leave the DOM. These helpers make it easy to create polished user experiences with minimal code, leveraging native CSS transitions for GPU-accelerated performance.

#### Adding Elements with Animation

Use `add_via` to smoothly fade in new elements:

```ruby
class MessageComponent < Funicular::Component
  def component_mounted
    add_via(
      "message-#{props[:message]['id']}",
      "opacity-0 scale-95",      # from: invisible, slightly smaller
      "opacity-100 scale-100",   # to: visible, normal size
      duration: 300
    )
  end

  def render
    div(class: "opacity-0 scale-95", id: "message-#{props[:message]['id']}") do
      p { props[:message]["content"] }
    end
  end
end
```

#### Removing Elements with Animation

Use `remove_via` to smoothly fade out elements before removing them from state:

```ruby
class ChatComponent < Funicular::Component
  def handle_message_delete(message_id)
    remove_via(
      "message-#{message_id}",
      "opacity-100 max-h-screen",  # from: visible, full height
      "opacity-0 max-h-0",         # to: invisible, collapsed
      duration: 500
    ) do
      # Callback: update state after animation completes
      updated_messages = state.messages.reject { |m| m["id"] == message_id }
      patch(messages: updated_messages)
    end
  end
end
```

#### How It Works

1. **String-based Class Specification**: CSS classes are specified as space-separated strings (e.g., `"opacity-0 scale-95"`), matching standard CSS syntax
2. **From-To Transitions**: The first string is the starting state, the second is the ending state
3. **Callback Support**: Optional blocks execute after animations complete, perfect for state updates
4. **Native CSS**: Uses browser CSS transitions for smooth, GPU-accelerated animations

#### CSS Setup

Define your transitions using Funicular's Styles DSL:

```ruby
class MessageComponent < Funicular::Component
  styles do
    message "transition-[opacity,max-height,transform] duration-500 ease-out max-h-screen"
  end

  def render
    div(class: "#{s.message} opacity-0 scale-95", id: "message-#{props[:message]['id']}") do
      # Message content
    end
  end
end
```

The Styles DSL keeps your transition definitions organized alongside other component styles, making them easy to maintain and reuse.

#### Common Animation Patterns

**Fade in from below**:
```ruby
add_via(element_id, "opacity-0 translate-y-4", "opacity-100 translate-y-0")
```

**Slide out to the right**:
```ruby
remove_via(element_id, "translate-x-0", "translate-x-full")
```

**Scale and fade**:
```ruby
add_via(element_id, "opacity-0 scale-50", "opacity-100 scale-100")
```

### Error Boundary

Funicular provides an `ErrorBoundary` component that catches errors from child components and displays a fallback UI instead of crashing the entire application. This is similar to React's Error Boundaries pattern.

#### Basic Usage

Wrap components that might throw errors with `ErrorBoundary`:

```ruby
class MyApp < Funicular::Component
  def render
    div do
      h1 { "My Application" }

      # If RiskyComponent throws, only this section shows error UI
      component(Funicular::ErrorBoundary) do
        component(RiskyComponent)
      end

      # This component continues to work normally
      component(SafeComponent)
    end
  end
end
```

#### Custom Fallback UI

Provide a custom fallback using the `fallback` prop:

```ruby
component(Funicular::ErrorBoundary,
  fallback: ->(error) {
    div(style: 'background: #fee; padding: 20px;') do
      h3 { "Oops! Something went wrong" }
      p { "Error: #{error.message}" }
    end
  }
) do
  component(RiskyComponent)
end
```

#### Error Callback

Use `on_error` to log or report errors:

```ruby
component(Funicular::ErrorBoundary,
  on_error: ->(error, info) {
    puts "Error in #{info[:component_class]}: #{error.message}"
    # Send to error tracking service
  }
) do
  component(RiskyComponent)
end
```

#### Isolating Failures

Use multiple ErrorBoundaries to isolate failures. One component's error won't affect others:

```ruby
div(style: 'display: flex; gap: 20px;') do
  # Each section is isolated
  component(Funicular::ErrorBoundary) do
    component(WidgetA)
  end

  component(Funicular::ErrorBoundary) do
    component(WidgetB)  # If this fails, WidgetA still works
  end

  component(Funicular::ErrorBoundary) do
    component(WidgetC)
  end
end
```

#### Default Fallback UI

When no custom fallback is provided, ErrorBoundary displays a styled error message showing the error class and message. In development mode, it also shows the component class that threw the error.

### Suspense / Loading State

Funicular provides a Suspense pattern for handling asynchronous data fetching with declarative loading states. This eliminates boilerplate code for managing loading spinners and error states.

#### Basic Usage

Declare async data loaders at the class level using `use_suspense`, then use the `suspense` helper in your render method:

```ruby
class UserProfile < Funicular::Component
  use_suspense :user,
    ->(resolve, reject) {
      User.find(props[:id]) do |user, error|
        error ? reject.call(error) : resolve.call(user)
      end
    }

  def render
    div do
      h1 { "Profile" }

      suspense(
        fallback: -> { div { "Loading..." } }
      ) do
        div { "Name: #{user.name}" }
        div { "Email: #{user.email}" }
      end
    end
  end
end
```

The suspense data (`user` in this example) is automatically accessible as a method within the component.

#### With Error Handling

Provide an `error` callback to display custom error UI:

```ruby
suspense(
  fallback: -> { div(class: "spinner") },
  error: ->(e) {
    div(class: "error") do
      span { "Failed to load: #{e}" }
      button(onclick: -> { reload_suspense(:user) }) { "Retry" }
    end
  }
) do
  div { user.name }
end
```

#### Syncing Form State with on_resolve

When using suspense data with forms, use the `on_resolve` callback to sync the loaded data with form state:

```ruby
class SettingsComponent < Funicular::Component
  use_suspense :current_user,
    ->(resolve, reject) {
      Session.current_user do |user, error|
        error ? reject.call(error) : resolve.call(user)
      end
    },
    on_resolve: ->(user) {
      # Sync form state when data loads
      patch(
        form: {
          username: user.username,
          display_name: user.display_name
        }
      )
    }

  def initialize_state
    { form: { username: "", display_name: "" } }
  end

  def render
    suspense(fallback: -> { div { "Loading..." } }) do
      form_for(:form, on_submit: :handle_save) do |f|
        f.text_field(:username, disabled: true)
        f.text_field(:display_name)
        f.submit("Save")
      end
    end
  end
end
```

#### Preventing Flickering with min_delay

For fast-loading data, use `min_delay` to ensure the loading state is visible for a minimum duration. This prevents UI flickering:

```ruby
use_suspense :data,
  ->(resolve, reject) {
    API.fetch_data do |data, error|
      error ? reject.call(error) : resolve.call(data)
    end
  },
  min_delay: 300  # Show loading spinner for at least 300ms
```

#### Multiple Suspense Sources

You can declare multiple suspense data sources. The `suspense` helper waits for all of them:

```ruby
class Dashboard < Funicular::Component
  use_suspense :user, ->(resolve, reject) { ... }
  use_suspense :stats, ->(resolve, reject) { ... }
  use_suspense :notifications, ->(resolve, reject) { ... }

  def render
    suspense(fallback: -> { div { "Loading dashboard..." } }) do
      # All three are loaded before this block executes
      div { "Welcome, #{user.name}" }
      div { "Stats: #{stats.count}" }
      div { "Notifications: #{notifications.length}" }
    end
  end
end
```

#### Reloading Data

Use `reload_suspense` to refresh data (useful for retry buttons or refresh actions):

```ruby
def handle_refresh
  reload_suspense(:user)
end

def render
  suspense(
    fallback: -> { div { "Loading..." } },
    error: ->(e) {
      button(onclick: -> { reload_suspense(:user) }) { "Retry" }
    }
  ) do
    div { user.name }
    button(onclick: -> { handle_refresh }) { "Refresh" }
  end
end
```

#### Helper Methods

- `suspense_loading?` - Check if any suspense data is still loading
- `suspense_loading?(:name)` - Check if specific suspense data is loading
- `suspense_error?(:name)` - Check if suspense data failed to load
- `suspense_error(:name)` - Get the error for a specific suspense data
- `reload_suspense(:name)` - Reload specific suspense data

### JS Integration via Delegation Model

For complex UI interactions or data visualizations requiring existing JavaScript libraries (e.g., Chart.js, D3.js), Funicular adopts a "delegation model."
This strategy allows you to define container DOM elements within your Ruby components using `ref` attributes.
During lifecycle hooks (like `component_mounted`), control of these specific DOM elements is "delegated" to external JavaScript code.
This ensures you can leverage the full power and rich ecosystems of existing JS libraries, maintain a clear separation of concerns between Ruby (component structure, data flow) and JavaScript (DOM manipulation, library-specific logic), and keep the Funicular core lean and focused on Ruby-first development.

### See also:
- [picoruby-wasm/docs/callback.md](../picoruby-wasm/docs/callback.md) ... Callback system
- [picoruby-wasm/docs/interoperability_between_js_and_ruby.md](../picoruby-wasm/docs/interoperability_between_js_and_ruby.md) ... JS::Bridge helps you with converting Ruby object to JS

### License

MIT
