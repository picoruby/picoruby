module Rubino
  class Components
    def initialize
      @children = {}
    end

    def add(name, comp)
      @children[name] = comp
    end

    def [](name)
      @children[name]
    end

    def method_missing(name, *args, &block)
      @children[name]
    end
  end

  Comps = Rubino::Components.new
end

