# Advanced Features

This guide covers advanced Funicular features including Error Boundaries, CSS Transitions, and JavaScript integration.

## Table of Contents

- [Error Boundary](#error-boundary)
- [CSS Transition Helpers](#css-transition-helpers)
- [JS Integration via Delegation Model](#js-integration-via-delegation-model)

## Error Boundary

Funicular provides an `ErrorBoundary` component that catches errors from child components and displays a fallback UI instead of crashing the entire application. This is similar to React's Error Boundaries pattern.

### Basic Usage

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

### Custom Fallback UI

Provide a custom fallback using the `fallback` prop:

```ruby
component(Funicular::ErrorBoundary,
  fallback: ->(error) {
    div(style: 'background: #fee; padding: 20px; border: 2px solid #f88;') do
      h3 { "Oops! Something went wrong" }
      p { "Error: #{error.message}" }
      button(onclick: -> { Funicular.router.navigate('/') }) { "Go Home" }
    end
  }
) do
  component(RiskyComponent)
end
```

The fallback receives the error object as an argument.

### Error Callback

Use `on_error` to log or report errors to an external service:

```ruby
component(Funicular::ErrorBoundary,
  on_error: ->(error, info) {
    puts "Error in #{info[:component_class]}: #{error.message}"

    # Send to error tracking service
    ErrorTracker.report({
      message: error.message,
      component: info[:component_class],
      props: info[:props],
      stack_trace: error.backtrace.join("\n")
    })
  }
) do
  component(RiskyComponent)
end
```

The `on_error` callback receives:
- `error`: The error object
- `info`: Hash with `{ component_class:, props: }`

### Isolating Failures

Use multiple ErrorBoundaries to isolate failures. One component's error won't affect others:

```ruby
class Dashboard < Funicular::Component
  def render
    div(style: 'display: flex; gap: 20px;') do
      # Each section is isolated
      component(Funicular::ErrorBoundary,
        fallback: ->(e) { div(class: "error") { "Widget A failed" } }
      ) do
        component(WidgetA)
      end

      component(Funicular::ErrorBoundary,
        fallback: ->(e) { div(class: "error") { "Widget B failed" } }
      ) do
        component(WidgetB)  # If this fails, WidgetA and WidgetC still work
      end

      component(Funicular::ErrorBoundary,
        fallback: ->(e) { div(class: "error") { "Widget C failed" } }
      ) do
        component(WidgetC)
      end
    end
  end
end
```

### Default Fallback UI

When no custom fallback is provided, ErrorBoundary displays a styled error message:

```
┌─────────────────────────────────────┐
│ ⚠ Error                             │
│                                     │
│ RuntimeError: Something went wrong  │
│                                     │
│ Component: RiskyComponent           │
└─────────────────────────────────────┘
```

### Best Practices

**1. Place Boundaries Strategically**

```ruby
# ✅ Good: Boundary around independent features
component(ErrorBoundary) { component(UserProfile) }
component(ErrorBoundary) { component(CommentList) }

# ❌ Avoid: Boundary around entire app (too broad)
component(ErrorBoundary) do
  component(EntireApplication)
end
```

**2. Provide Recovery Actions**

```ruby
component(ErrorBoundary,
  fallback: ->(error) {
    div do
      p { "Failed to load data" }
      button(onclick: -> { reload_component }) { "Try Again" }
      button(onclick: -> { Funicular.router.navigate('/') }) { "Go Home" }
    end
  }
)
```

**3. Log Errors**

```ruby
component(ErrorBoundary,
  on_error: ->(error, info) {
    # Always log errors for debugging
    puts "[ERROR] #{info[:component_class]}: #{error.message}"
    puts error.backtrace.join("\n")
  }
)
```

## CSS Transition Helpers

Funicular provides built-in CSS transition helpers to add smooth animations when elements enter or leave the DOM. These helpers leverage native CSS transitions for GPU-accelerated performance.

### Adding Elements with Animation

Use `add_via` to smoothly fade in new elements:

```ruby
class MessageComponent < Funicular::Component
  def component_mounted
    add_via(
      "message-#{props[:message]['id']}",  # Element ID
      "opacity-0 scale-95",                # from: invisible, slightly smaller
      "opacity-100 scale-100",             # to: visible, normal size
      duration: 300                        # Animation duration in ms
    )
  end

  def render
    div(class: "opacity-0 scale-95 transition-all", id: "message-#{props[:message]['id']}") do
      p { props[:message]["content"] }
    end
  end
end
```

### Removing Elements with Animation

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

  def render
    state.messages.map do |message|
      div(
        id: "message-#{message['id']}",
        class: "opacity-100 max-h-screen transition-all"
      ) do
        span { message["content"] }
        button(onclick: -> { handle_message_delete(message["id"]) }) { "Delete" }
      end
    end
  end
end
```

### How It Works

1. **String-based Class Specification**: CSS classes are specified as space-separated strings (e.g., `"opacity-0 scale-95"`)
2. **From-To Transitions**: The first string is the starting state, the second is the ending state
3. **Callback Support**: Optional blocks execute after animations complete
4. **Native CSS**: Uses browser CSS transitions for smooth, GPU-accelerated animations

### CSS Setup

Define your transitions using Funicular's Styles DSL or regular CSS:

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

Or with plain CSS:

```css
.message {
  transition: opacity 500ms ease-out,
              max-height 500ms ease-out,
              transform 500ms ease-out;
  max-height: 100vh;
}
```

### Common Animation Patterns

**Fade in from below**:
```ruby
add_via(element_id, "opacity-0 translate-y-4", "opacity-100 translate-y-0", duration: 300)
```

**Slide out to the right**:
```ruby
remove_via(element_id, "translate-x-0 opacity-100", "translate-x-full opacity-0", duration: 400)
```

**Scale and fade**:
```ruby
add_via(element_id, "opacity-0 scale-50", "opacity-100 scale-100", duration: 200)
```

**Collapse height**:
```ruby
remove_via(element_id, "max-h-screen opacity-100", "max-h-0 opacity-0", duration: 500)
```

### Timing Functions

Specify different timing functions via CSS:

```ruby
styles do
  # Ease out (default)
  fade_out "transition-opacity duration-300 ease-out"

  # Spring effect
  bounce_in "transition-transform duration-500 ease-[cubic-bezier(0.68,-0.55,0.265,1.55)]"

  # Linear
  slide "transition-transform duration-400 linear"
end
```

### Complete Example

```ruby
class NotificationList < Funicular::Component
  styles do
    notification "transition-all duration-500 ease-out max-h-24 overflow-hidden"
    notification_enter "opacity-0 translate-x-full"
    notification_visible "opacity-100 translate-x-0"
    notification_exit "opacity-0 max-h-0 translate-x-full"
  end

  def initialize_state
    { notifications: [] }
  end

  def add_notification(message)
    notification = {
      id: Time.now.to_i,
      message: message,
      timestamp: Time.now
    }

    patch(notifications: state.notifications + [notification])

    # Animate in after a brief delay (allows element to render)
    JS.global.setTimeout(50) do
      add_via(
        "notification-#{notification[:id]}",
        s.notification_enter,
        s.notification_visible,
        duration: 500
      )
    end

    # Auto-remove after 5 seconds
    JS.global.setTimeout(5000) do
      remove_notification(notification[:id])
    end
  end

  def remove_notification(id)
    remove_via(
      "notification-#{id}",
      s.notification_visible,
      s.notification_exit,
      duration: 500
    ) do
      patch(notifications: state.notifications.reject { |n| n[:id] == id })
    end
  end

  def render
    div(class: "notification-container fixed top-4 right-4 space-y-2") do
      state.notifications.map do |notification|
        div(
          id: "notification-#{notification[:id]}",
          key: notification[:id],
          class: "#{s.notification} #{s.notification_enter} bg-blue-600 text-white p-4 rounded shadow-lg"
        ) do
          p { notification[:message] }
          button(
            onclick: -> { remove_notification(notification[:id]) },
            class: "text-white underline text-sm"
          ) { "Dismiss" }
        end
      end
    end
  end
end
```

## JS Integration via Delegation Model

For complex UI interactions or data visualizations requiring existing JavaScript libraries (e.g., Chart.js, D3.js), Funicular adopts a "delegation model."

### The Delegation Model

This strategy:
1. **Define container elements** in Ruby components using `ref` attributes
2. **Delegate control** to external JavaScript during lifecycle hooks (like `component_mounted`)
3. **Maintain separation** between Ruby (component structure, data flow) and JavaScript (DOM manipulation, library-specific logic)

### Basic Example: Chart.js Integration

```ruby
class ChartComponent < Funicular::Component
  def component_mounted
    # Get the canvas element via ref
    canvas = refs[:chart_canvas]

    # Prepare chart data
    chart_data = {
      labels: state.labels,
      datasets: [{
        label: 'Sales',
        data: state.data,
        backgroundColor: 'rgba(54, 162, 235, 0.2)',
        borderColor: 'rgba(54, 162, 235, 1)',
        borderWidth: 1
      }]
    }

    # Delegate to Chart.js
    @chart = JS.global.Chart.new(canvas, {
      type: 'bar',
      data: chart_data,
      options: {
        responsive: true,
        maintainAspectRatio: false
      }
    })
  end

  def component_unmounted
    # Clean up: destroy the chart
    @chart&.destroy()
  end

  def render
    div(class: "chart-container", style: "height: 400px") do
      canvas(ref: :chart_canvas)
    end
  end
end
```

### File Upload Integration

Use `Funicular::FileUpload` for file uploads:

```ruby
class ProfilePictureUpload < Funicular::Component
  def initialize_state
    { uploading: false, image_url: nil, error: nil }
  end

  def component_mounted
    # Mount the file upload helper (only once globally)
    Funicular::FileUpload.mount
  end

  def handle_file_select(event)
    file = event.target[:files][0]
    return unless file

    patch(uploading: true, error: nil)

    # Use the FileUpload helper
    Funicular::FileUpload.upload(file) do |result|
      if result[:error]
        patch(uploading: false, error: result[:error])
      else
        patch(
          uploading: false,
          image_url: result[:url],
          error: nil
        )
      end
    end
  end

  def render
    div do
      input(
        type: "file",
        accept: "image/*",
        onchange: :handle_file_select
      )

      if state.uploading
        p { "Uploading..." }
      elsif state.image_url
        img(src: state.image_url, class: "w-32 h-32")
      elsif state.error
        p(class: "text-red-600") { "Error: #{state.error}" }
      end
    end
  end
end
```

### D3.js Integration

```ruby
class D3GraphComponent < Funicular::Component
  def component_mounted
    render_graph
  end

  def component_updated(prev_state)
    # Re-render graph when data changes
    if prev_state[:data] != state.data
      render_graph
    end
  end

  def render_graph
    container = refs[:graph_container]

    # Clear existing SVG
    container.innerHTML = ""

    # Get D3 from global scope
    d3 = JS.global.d3

    # Create SVG
    svg = d3.select(container)
      .append("svg")
      .attr("width", 600)
      .attr("height", 400)

    # Bind data and create elements
    circles = svg.selectAll("circle")
      .data(state.data)
      .enter()
      .append("circle")
      .attr("cx", ->(d, i) { i * 50 + 25 })
      .attr("cy", 200)
      .attr("r", ->(d) { d * 10 })
      .attr("fill", "steelblue")

    # Add transitions
    circles.transition()
      .duration(1000)
      .attr("r", ->(d) { d * 15 })
  end

  def render
    div(class: "graph-container") do
      h3 { "D3 Visualization" }
      div(ref: :graph_container)
    end
  end
end
```

### Custom JavaScript Initialization

For custom JavaScript code:

```ruby
class MapComponent < Funicular::Component
  def component_mounted
    init_map
  end

  def init_map
    container = refs[:map_container]

    # Call custom JavaScript function
    JS.global.initializeMap(container, {
      center: { lat: props[:lat], lng: props[:lng] },
      zoom: props[:zoom] || 10,
      markers: props[:markers] || []
    })
  end

  def component_unmounted
    # Clean up map instance
    JS.global.destroyMap(refs[:map_container])
  end

  def render
    div(ref: :map_container, style: "width: 100%; height: 500px;")
  end
end
```

### Best Practices

**1. Use Refs for Delegation**

```ruby
# ✅ Good: Use refs to identify delegation targets
canvas(ref: :chart_canvas)
div(ref: :map_container)

# ❌ Avoid: Relying on class/id selectors
canvas(id: "my-chart")  # Fragile, might conflict
```

**2. Clean Up in component_unmounted**

```ruby
def component_mounted
  @chart = JS.global.Chart.new(...)
  @interval = JS.global.setInterval(1000) { update_data }
end

def component_unmounted
  @chart&.destroy()          # Destroy chart
  JS.global.clearInterval(@interval)  # Clear timers
end
```

**3. Keep JS Logic Minimal**

```ruby
# ✅ Good: Ruby handles data, JS handles rendering
def render_chart
  @chart.data.datasets[0].data = state.chart_data  # Update from Ruby
  @chart.update()
end

# ❌ Avoid: Complex logic in JS
# Keep business logic in Ruby, use JS only for rendering
```

**4. Handle Updates Properly**

```ruby
def component_updated(prev_state)
  # Only update JS library when relevant data changes
  if prev_state[:chart_data] != state.chart_data
    update_chart(state.chart_data)
  end
end

def update_chart(new_data)
  @chart.data.datasets[0].data = new_data
  @chart.update()
end
```

### See Also

For more details on JavaScript interoperability:
- [picoruby-wasm/docs/callback.md](../../picoruby-wasm/docs/callback.md) - Callback system
- [picoruby-wasm/docs/interoperability_between_js_and_ruby.md](../../picoruby-wasm/docs/interoperability_between_js_and_ruby.md) - JS::Bridge for converting Ruby objects to JS
