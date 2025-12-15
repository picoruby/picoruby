# Funicular

> 🎵Funicu-lì, Funicu-là!🚊🚊🚊
>
> 🎵Funicu-lì, Funicu-là!🚞🚞🚞


**Funicular** is an SPA framework powered by PicoRuby.wasm, featuring:
- Pure Ruby with no JavaScript or HTML required[^1]
- Browser-native *Object-REST Mapper* (O**R**M)
- ActionCable-compatible WebSocket support
- Seamless Rails integration
- Flexible JavaScript Integration via Delegation Model

[^1]: 子曰*知止而后有定* / Confucius, *When one knows where to stop, one can be steady.*

## Features

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
