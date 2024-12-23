require 'js'

module Rapp
  module Helper
    def selector(sel)
      sel.is_a?(Symbol) ? "##{sel}" : sel.to_s
    end
  end

  class ComponentContext
    include Helper

    def initialize(elem)
      @elem = elem
    end

    def method_missing(tag, sel, &block)
      if block
        elem = ComponentContext.new(@elem.querySelector("#{tag}#{selector sel}"))
        elem.instance_eval(&block)
      else
        puts "send #{tag} #{sel}"
        @elem.send(tag, sel)
      end
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

    def method_missing(tag, sel, &block)
      elem = Rapp::ComponentContext.new(JS.global[:document].querySelector("#{tag}#{selector sel}"))
      if block
        elem.instance_eval(&block)
      else
        elem
      end
    end

    def self.reactive_value(attr)
      attr_reader attr
      eval <<-RUBY
        class #{self.class}
          def #{attr}=(value)
            return if @#{attr} == value
            @#{attr} = value
            render
          end
        end
      RUBY
    end
  end

end
