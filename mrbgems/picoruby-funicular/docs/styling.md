# CSS-in-Ruby with Styles DSL

Funicular provides a powerful CSS-in-Ruby DSL that keeps your styles organized and scoped within each component.

## Table of Contents

- [Basic Usage](#basic-usage)
- [Conditional Styles](#conditional-styles)
- [Variant Styles](#variant-styles)
- [Combining Styles](#combining-styles)
- [Best Practices](#best-practices)

## Basic Usage

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

Access styles via the `s` helper in your render method.

## Conditional Styles

For styles that toggle based on state (like active/inactive), use the `active:` key:

```ruby
class ChannelList < Funicular::Component
  styles do
    channel_item base: "p-4 hover:bg-gray-700 cursor-pointer transition-colors",
                 active: "bg-gray-700 border-l-4 border-blue-500"
  end

  def render
    state.channels.map do |channel|
      is_active = state.current_channel_id == channel.id
      div(class: s.channel_item(is_active)) do
        span { channel.name }
      end
    end
  end
end
```

When `is_active` is `true`, the result is `"p-4 hover:bg-gray-700 cursor-pointer transition-colors bg-gray-700 border-l-4 border-blue-500"`.
When `false`, only the base classes are applied.

## Variant Styles

For styles with multiple states (e.g., primary/danger buttons), use `variants:`:

```ruby
class ButtonComponent < Funicular::Component
  styles do
    button base: "px-4 py-2 rounded font-semibold transition-colors",
           variants: {
             primary: "bg-blue-600 text-white hover:bg-blue-700",
             danger: "bg-red-600 text-white hover:bg-red-700",
             secondary: "bg-gray-600 text-white hover:bg-gray-700",
             ghost: "bg-transparent border border-gray-300 hover:bg-gray-100"
           }
  end

  def render
    div do
      button(class: s.button(:primary)) { "Submit" }
      button(class: s.button(:danger)) { "Delete" }
      button(class: s.button(:secondary)) { "Cancel" }
      button(class: s.button(:ghost)) { "Learn More" }
    end
  end
end
```

You can also combine variants with props:

```ruby
class ActionButton < Funicular::Component
  styles do
    button base: "px-4 py-2 rounded font-semibold",
           variants: {
             primary: "bg-blue-600 text-white hover:bg-blue-700",
             danger: "bg-red-600 text-white hover:bg-red-700"
           }
  end

  def render
    button(class: s.button(props[:variant] || :primary)) do
      props[:label]
    end
  end
end

# Usage
component(ActionButton, variant: :danger, label: "Delete")
```

## Combining Styles

Chain multiple styles together using the `|` operator:

```ruby
class Sidebar < Funicular::Component
  styles do
    flex "flex"
    items_center "items-center"
    gap_2 "gap-2"
    sidebar "w-64 bg-gray-800 text-white p-4"
    channel_item base: "p-2 rounded hover:bg-gray-700 cursor-pointer",
                 active: "bg-gray-700"
  end

  def render
    div(class: s.sidebar) do
      # Combine utility classes
      div(class: s.flex | s.items_center | s.gap_2) do
        span { "Channel List" }
      end

      # Mix DSL styles with custom classes
      div(class: s.channel_item(is_active) | "mb-2" | "custom-shadow") do
        span { "General" }
      end
    end
  end
end
```

The `|` operator:
- Automatically filters out `nil` values
- Perfect for conditional styling
- Allows mixing DSL styles with raw strings

### Conditional Combination

```ruby
def render
  div(class: s.card | (state.highlighted ? "ring-2 ring-blue-500" : nil)) do
    # Card content
  end
end
```

## Best Practices

### 1. Keep Styles Scoped

Define styles within components to avoid global namespace pollution:

```ruby
# ✅ Good: Scoped to component
class UserCard < Funicular::Component
  styles do
    card "bg-white rounded shadow p-4"
  end
end

# ❌ Avoid: Global CSS classes scattered in render
class UserCard < Funicular::Component
  def render
    div(class: "bg-white rounded shadow p-4")  # Less maintainable
  end
end
```

### 2. Use Semantic Names

```ruby
# ✅ Good: Semantic style names
styles do
  primary_button "bg-blue-600 text-white px-4 py-2 rounded"
  danger_button "bg-red-600 text-white px-4 py-2 rounded"
end

# ❌ Avoid: Generic or unclear names
styles do
  btn1 "bg-blue-600 text-white px-4 py-2 rounded"
  btn2 "bg-red-600 text-white px-4 py-2 rounded"
end
```

### 3. Group Related Styles

```ruby
styles do
  # Layout
  container "max-w-7xl mx-auto px-4"
  sidebar "w-64 bg-gray-800"
  main_content "flex-1 p-6"

  # Typography
  heading "text-2xl font-bold text-gray-900"
  subheading "text-lg font-semibold text-gray-700"
  body_text "text-base text-gray-600"

  # Buttons
  button base: "px-4 py-2 rounded",
         variants: {
           primary: "bg-blue-600 text-white",
           secondary: "bg-gray-600 text-white"
         }
end
```

### 4. Extract Common Styles

For styles used across multiple components, consider creating a shared module:

```ruby
module ButtonStyles
  def self.included(base)
    base.class_eval do
      styles do
        button base: "px-4 py-2 rounded font-semibold transition-colors",
               variants: {
                 primary: "bg-blue-600 text-white hover:bg-blue-700",
                 danger: "bg-red-600 text-white hover:bg-red-700"
               }
      end
    end
  end
end

class MyComponent < Funicular::Component
  include ButtonStyles

  def render
    button(class: s.button(:primary)) { "Click Me" }
  end
end
```

### 5. Use Tailwind (or Similar) for Utility Classes

Funicular's Styles DSL works great with Tailwind CSS utility classes:

```ruby
styles do
  card "bg-white dark:bg-gray-800 rounded-lg shadow-md p-6 hover:shadow-xl transition-shadow"
  input "w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
end
```

### 6. Prefer Composition Over Long Class Strings

```ruby
# ✅ Good: Composed styles
styles do
  base_input "w-full px-3 py-2 border rounded-md"
  focused_input "outline-none ring-2 ring-blue-500"
end

def render
  input(class: s.base_input | s.focused_input)
end

# ❌ Avoid: One massive style
styles do
  input "w-full px-3 py-2 border rounded-md outline-none ring-2 ring-blue-500"
end
```

## Key Benefits

- **Scoped Styles**: Styles are defined within components, avoiding global namespace pollution
- **Readability**: `s.button(:primary)` is more semantic than raw Tailwind classes scattered throughout render methods
- **Maintainability**: Change styles in one place rather than hunting through render methods
- **Type Safety**: The DSL ensures style names are consistent throughout your component
- **Flexibility**: Mix DSL styles with raw strings when needed using the `|` operator
