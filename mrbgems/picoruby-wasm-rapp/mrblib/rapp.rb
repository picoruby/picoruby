require 'js'

module Rapp
  module Helper
    def s(selector)
      selector.is_a?(Symbol) ? "##{selector}" : selector.to_s
    end
  end

  class ComponentContext
    include Helper

    def initialize(elem)
      @elem = elem
    end

    def method_missing(tag, selector, &block)
      if block
        elem = ComponentContext.new(@elem.querySelector("#{tag}#{s selector}"))
        elem.instance_eval(&block)
      else
        puts "send #{tag} #{selector}"
        @elem.send(tag, selector)
      end
    end

    def style
      @elem[:style]
    end

    def text(content)
      @elem.text = content
    end

    def innerHTML(content)
      @elem.innerHTML = content
    end
  end

  class Component
    include Helper

    def add(name, comp)
      @children[name] = comp
    end

    def initialize(selector = 'div')
      @children = {}
      @selector = s(selector)
      @element = JS.global[:document].querySelector(@selector)
    end

    def method_missing(tag, selector = nil, &block)
      if selector.nil? && block.nil?
        return @children[tag]
      end
      elem = Rapp::ComponentContext.new(JS.global[:document].querySelector("#{tag}#{s selector.to_s}"))
      if block
        elem.instance_eval(&block)
      else
        elem
      end
    end

    def on(event, selector = @selector, &block)
      JS.global[:document].querySelector(selector.to_s).addEventListener(event.to_s, &block)
    end

    def self.attr_reactive(*attrs)
      attr_reader *attrs
      script = "class #{self.class}\n"
      attrs.each do |attr|
        script += <<-RUBY
          def #{attr}=(value)
            return if @#{attr} == value
            @#{attr} = value
            render
          end
        RUBY
      end
      script += "\nend"
      eval script
    end
  end

end
