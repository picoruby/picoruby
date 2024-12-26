module Rapp
  class VNode
    attr_reader :type, :props, :children

    def initialize(type, props = {}, children = [])
      @type = type
      @props = props
      @children = children
    end

    def to_s
      "#{@type}, #{@props}, #{@children}"
    end
  end
end
