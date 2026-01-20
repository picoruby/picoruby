# Components and State Management

This guide covers how to create components, manage state, and handle component lifecycle in Funicular.

## Table of Contents

- [Creating Components](#creating-components)
- [Event Handlers](#event-handlers)
- [State Management](#state-management)
- [Props](#props)
- [Component Lifecycle](#component-lifecycle)
- [Component Communication](#component-communication)
- [Refs](#refs)

## Creating Components

Components are Ruby classes that inherit from `Funicular::Component`. Each component must implement a `render` method that returns a VDOM representation of the UI.

### Basic Component

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
    end
  end
end
```

### Mounting a Component

```ruby
# Mount to a container element
Funicular.start(Counter, container: "app")

# Or use the router (recommended for SPAs)
Funicular.start(container: 'app') do |router|
  router.get('/counter', to: Counter, as: 'counter')
end
```

## Event Handlers

Funicular provides a unified callback system for handling events. All event handlers accept **Symbol**, **Method**, or **Proc**.

### Three Ways to Handle Events

```ruby
class ExampleComponent < Funicular::Component
  def handle_click(event)
    puts "Button clicked!"
    patch(clicked: true)
  end

  def render
    div do
      # 1. Symbol (recommended for method references)
      button(onclick: :handle_click) { "Click Me" }

      # 2. Method object (useful for passing to child components)
      button(onclick: method(:handle_click)) { "Click Me" }

      # 3. Proc/Lambda (for inline logic)
      button(onclick: -> { patch(count: state.count + 1) }) { "Increment" }
      button(onclick: ->(e) { puts e.target[:value] }) { "With Event" }
    end
  end
end
```

### When to Use Each

**Use Symbol (`:method_name`)** - Most common:
- Clean and concise
- Best for method references
- Works for all event types

```ruby
button(onclick: :handle_submit) { "Submit" }
input(oninput: :handle_input, onchange: :handle_change)
form_for(:user, on_submit: :handle_submit)
```

**Use Method (`method(:name)`)** - For passing to child components:
- Explicitly creates a Method object
- Preserves `self` binding
- Useful when passing callbacks as props

```ruby
component(ChildComponent, {
  on_delete: method(:handle_delete),
  on_update: method(:handle_update)
})
```

**Use Proc/Lambda** - For inline logic:
- Short, one-off handlers
- Direct state updates
- Complex logic that doesn't warrant a separate method

```ruby
button(onclick: -> { patch(count: state.count + 1) }) { "+" }
button(onclick: -> { Funicular.router.navigate('/home') }) { "Home" }
```

### Event Object

When using Symbol or Method, the event object is automatically passed:

```ruby
def handle_input(event)
  value = event.target[:value]
  patch(search_query: value)
end

# Event passed automatically
input(oninput: :handle_input)
```

With Proc/Lambda, you can optionally receive the event:

```ruby
# With event
input(oninput: ->(e) { patch(value: e.target[:value]) })

# Without event (arity = 0)
button(onclick: -> { patch(clicked: true) })
```

### Form Submissions

The `form_for` helper also accepts all three types:

```ruby
# Symbol (recommended)
form_for(:user, on_submit: :handle_submit) do |f|
  # ...
end

# Method object
form_for(:user, on_submit: method(:handle_submit)) do |f|
  # ...
end

# Proc
form_for(:user, on_submit: ->(data) { User.create(data) }) do |f|
  # ...
end
```

## State Management

State is component-local data that determines what gets rendered. When state changes, the component re-renders.

### Initializing State

Override `initialize_state` to set initial state:

```ruby
class TodoList < Funicular::Component
  def initialize_state
    {
      todos: [],
      input: "",
      filter: :all
    }
  end
end
```

### Reading State

Access state via the `state` accessor (read-only):

```ruby
def render
  div do
    p { "Total: #{state.todos.length}" }
    p { "Input: #{state.input}" }
  end
end
```

### Updating State

Use `patch()` to update state. The component will automatically re-render:

```ruby
def handle_input(event)
  patch(input: event.target[:value])
end

def add_todo
  new_todo = { id: Time.now.to_i, text: state.input, done: false }
  patch(
    todos: state.todos + [new_todo],
    input: ""
  )
end
```

**Important**: `patch()` merges the new state with existing state (shallow merge). Direct state mutation is blocked:

```ruby
# ❌ This will raise an error
state.count = 5

# ✅ Use patch() instead
patch(count: 5)
```

### Updating Nested State

For deeply nested state, manually construct the new structure:

```ruby
def initialize_state
  {
    user: {
      profile: { name: "Alice", age: 30 },
      settings: { theme: "dark" }
    }
  }
end

def update_name(new_name)
  patch(
    user: state.user.merge(
      profile: state.user[:profile].merge(name: new_name)
    )
  )
end
```

## Props

Props are data passed from parent to child components. They are immutable within the child component.

### Passing Props

```ruby
# Parent component
class App < Funicular::Component
  def render
    div do
      component(UserCard, {
        name: "Alice",
        email: "alice@example.com",
        avatar_url: "/images/alice.png"
      })
    end
  end
end
```

### Receiving Props

Access props via the `props` accessor:

```ruby
class UserCard < Funicular::Component
  def render
    div(class: "card") do
      img(src: props[:avatar_url])
      h3 { props[:name] }
      p { props[:email] }
    end
  end
end
```

### Passing Callbacks

Props can include Proc/Lambda callbacks for child-to-parent communication:

```ruby
# Parent
class TodoApp < Funicular::Component
  def handle_delete(id)
    patch(todos: state.todos.reject { |t| t[:id] == id })
  end

  def render
    state.todos.map do |todo|
      component(TodoItem, {
        todo: todo,
        on_delete: method(:handle_delete)
      })
    end
  end
end

# Child
class TodoItem < Funicular::Component
  def handle_delete_click
    props[:on_delete].call(props[:todo][:id])
  end

  def render
    div do
      span { props[:todo][:text] }
      button(onclick: :handle_delete_click) { "Delete" }
    end
  end
end
```

## Component Lifecycle

Funicular provides lifecycle hooks for component initialization and cleanup.

### component_mounted

Called after the component is mounted to the DOM:

```ruby
class ChatComponent < Funicular::Component
  def component_mounted
    # Subscribe to WebSocket
    @subscription = Cable.subscribe("ChatChannel") do |message|
      patch(messages: state.messages + [message])
    end

    # Load initial data
    load_suspense_data if self.class.suspense_definitions.any?
  end
end
```

### component_unmounted

Called before the component is removed from the DOM:

```ruby
class ChatComponent < Funicular::Component
  def component_unmounted
    # Cleanup: unsubscribe from WebSocket
    @subscription&.unsubscribe

    # Clear timers
    @timer_ids.each { |id| JS.global.clearTimeout(id) }
  end
end
```

### Lifecycle Order

```
1. new(props)           # Constructor
2. initialize_state     # Define initial state
3. render               # First render
4. component_mounted    # After DOM insertion
5. [state changes...]   # User interactions
6. render (re-renders)  # On each state update
7. component_unmounted  # Before removal
```

## Component Communication

### Parent to Child: Props

Pass data down via props (see [Props](#props) section above).

### Child to Parent: Callbacks

Pass callback Procs via props:

```ruby
# Parent defines handler
def handle_change(new_value)
  patch(value: new_value)
end

# Parent passes callback
component(InputField, {
  value: state.value,
  on_change: ->(new_value) { handle_change(new_value) }
})

# Child calls callback
props[:on_change].call(new_value)
```

### Sibling Communication

Lift state to common parent:

```ruby
class Dashboard < Funicular::Component
  def initialize_state
    { selected_item: nil }
  end

  def render
    div do
      component(ItemList, {
        items: state.items,
        on_select: ->(item) { patch(selected_item: item) }
      })

      component(ItemDetails, {
        item: state.selected_item
      })
    end
  end
end
```

## Refs

Refs provide direct access to DOM elements, useful for:
- Focus management
- Text selection
- Integrating with JavaScript libraries

### Creating Refs

Use the `ref` prop and access via `refs` accessor:

```ruby
class SearchBox < Funicular::Component
  def component_mounted
    # Focus input on mount
    refs[:search_input]&.focus()
  end

  def render
    div do
      input(ref: :search_input, type: "text", placeholder: "Search...")
      button(onclick: -> { refs[:search_input].focus() }) { "Focus" }
    end
  end
end
```

### Refs with JavaScript Libraries

Refs are essential for integrating JS libraries:

```ruby
class ChartComponent < Funicular::Component
  def component_mounted
    # Delegate to Chart.js
    canvas = refs[:chart_canvas]
    @chart = JS.global.Chart.new(canvas, chart_config)
  end

  def component_unmounted
    @chart&.destroy()
  end

  def render
    div do
      canvas(ref: :chart_canvas, width: 400, height: 300)
    end
  end
end
```

## Best Practices

### Keep Components Small

Break large components into smaller, focused ones:

```ruby
# ❌ Too large
class UserDashboard < Funicular::Component
  def render
    div do
      # 500 lines of mixed UI code
    end
  end
end

# ✅ Decomposed
class UserDashboard < Funicular::Component
  def render
    div do
      component(UserProfile)
      component(ActivityFeed)
      component(SettingsPanel)
    end
  end
end
```

### Avoid Deep Props Drilling

If passing props through many levels, consider:
- Component composition
- Lifting state higher
- Model layer for shared server data

### Use Descriptive State Keys

```ruby
# ❌ Unclear
{ x: true, y: 5, z: [] }

# ✅ Descriptive
{ is_loading: true, page_number: 5, search_results: [] }
```

### Normalize State Shape

Prefer flat structures over deep nesting:

```ruby
# ❌ Deeply nested
{
  users: [
    { id: 1, posts: [{ id: 10, comments: [...] }] }
  ]
}

# ✅ Normalized
{
  users: { 1 => { id: 1, name: "Alice" } },
  posts: { 10 => { id: 10, user_id: 1 } },
  comments: { 100 => { id: 100, post_id: 10 } }
}
```
