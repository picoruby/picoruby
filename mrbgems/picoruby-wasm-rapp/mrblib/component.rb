require 'js'

module Rapp
  class Component
    def initialize(selector = 'div')
      @selector = (selector.is_a?(Symbol) ? "##{selector}" : selector.to_s)
      @element = JS.document.querySelector(@selector)
    end

    def h(tag_name, props = {}, children = [])
      VNode.new(tag_name, props, children)
    end

    def on(event, selector = @selector, &block)
      JS.document.querySelector(selector.to_s).addEventListener(event.to_s, &block)
    end

    def update_vdom
      # Subclass should override this method
      VNode.new('div',{},'dummy')
    end

    def render(new_vdom)
      patches = Differ.diff(@current_vdom, new_vdom)
      new_element = Patcher.apply(@element, patches)
      @element = new_element if new_element
      @current_vdom = new_vdom
      #puts "@current_vdon: #{@current_vdom.to_s}"
    end

    def self.attr_reactive(*attrs)
      attr_reader *attrs
      script = "class #{self.class}\n"
      attrs.each do |attr|
        script += <<-RUBY
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
