# Rapp

Rapp is a lightweight frontend framework for PicoRuby.wasm that implements Virtual DOM and reactive component-based architecture.

## Features

- Virtual DOM-based rendering
- Component-based architecture
- Reactive state management
- Event handling support
- Written purely in Ruby (PicoRuby)

## Basic Usage

Here's a simple counter component example:

```html
<!DOCTYPE html>
<html>
  <head>
    <title>PicoRuby.wasm Rapp Demo</title>
    <meta charset="utf-8">
  </head>
  <body>
    <h1>PicoRuby.wasm Rapp Demo</h1>
    <button id="button">Up</button>
    <div id="container">
      <div id="counter">
        <b>0</b>
      </div>
    </div>
    <script type="text/ruby" src="rapp_demo.rb"></script>
    <script src="https://cdn.jsdelivr.net/npm/@picoruby/wasm-wasi@latest/dist/init.iife.js"></script>
  </body>
</html>
```

```ruby
# rapp_demo.rb
require 'rapp'

class Counter < Rapp::Component
  attr_reactive :num

  def initialize(selector)
    super(selector)
    @num = 0
  end

  def update_vdom
    if @num % 3 == 0 || @num.to_s.include?('3')
      prop, word = {style: "color: red;"}, "aho"
    else
      prop, word = {}, num
    end
    h('b', prop, [ h('#text', {}, [ word ]) ])
    # `h('b', prop, h('#text', {}, word))` does also work (not recommended)
  end
end

# Register component
Rapp::Comps.add(:counter, Counter.new('#container #counter'))

# Add click event handler
Rapp::Component.new(:button).on :click do
  Rapp::Comps[:counter].num += 1
end
```

## Component API

### Creating Components

Inherit from `Rapp::Component` to create a new component:

```ruby
class MyComponent < Rapp::Component
  def initialize(selector)
    super(selector)
  end

  def update_vdom
    h('div', {}, ['Hello, World!'])
  end
end
```

### Reactive Properties

Use `attr_reactive` to create properties that trigger re-rendering when changed:

```ruby
class MyComponent < Rapp::Component
  attr_reactive :title, :content

  def initialize(selector)
    super(selector)
    @title = "Initial Title"
    @content = "Initial Content"
  end
end
```

### Event Handling

Add event listeners using the `on` method:

```ruby
Rapp::Component.new('#my-button').on :click do
  # Handle click event
end
```

### Virtual DOM Creation

Use the `h` helper method to create virtual DOM nodes:

Syntax:

```
h(tag_name, properties, children)
```

```ruby
# Example
h('div', {class: 'container'}, [
  h('span', {}, ['Text content'])
])
```

## Component Registry

Use `Rapp::Comps` to register and access components globally:

```ruby
# Register component
Rapp::Comps.add(:my_component, MyComponent.new('#app'))

# Access component
Rapp::Comps[:my_component].title = "New Title"
```

## Architecture

Rapp uses a Virtual DOM implementation consisting of these main parts:

- `Rapp::Component`: Base class for all components
- `Rapp::VNode`: Virtual DOM node representation
- `Rapp::Differ`: Calculates differences between Virtual DOM trees
- `Rapp::Patcher`: Applies Virtual DOM differences to actual DOM

## Future Plans

- Bug fix for Virtual DOM diffing
- State management and lifecycle hooks for components
- Partial HTML templating support

## Contributing

Bug reports and pull requests are welcome on GitHub.

## License

MIT License.

2024 (c) HASUMI Hitoshi a.k.a. [@hasumikin](https://twitter.com/hasumikin)
