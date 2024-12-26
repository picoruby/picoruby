require 'js'

module Rubino
  class Component
    def initialize(selector = 'div')
      @selector = (selector.is_a?(Symbol) ? "##{selector}" : selector.to_s)
      @element = JS.document.querySelector(@selector)
    end

    def h(tag_name, props = {}, children = [])
      children = [children] unless children.is_a?(Array)
      Rubino::VNode.new(tag_name, props, children)
    end

    def on(event, selector = @selector, &block)
      JS.document.querySelector(selector.to_s).addEventListener(event.to_s, &block)
    end

    def update_vdom
      # Subclass should override this method
      h('div', {}, [h('p', {}, ['Hello, World!'])])
    end

    def render(new_vdom)
      patches = Rubino::Differ.diff(@current_vdom, new_vdom)
      new_element = Rubino::Patcher.apply(@element, patches)
      @element = new_element if new_element
      @current_vdom = new_vdom
    end

    def self.attr_reactive(*attrs)
      attr_reader *attrs
      script = "class #{self.class}\n"
      attrs.each do |attr|
        script += <<~RUBY
          def #{attr}=(value)
            return if @#{attr} == value
            @#{attr} = value
            render(update_vdom)
          end
        RUBY
      end
      script += "\nend"
      eval script
    end
  end
end
