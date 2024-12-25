module Rapp
  class VNode
    attr_reader :type, :props, :children

    def initialize(type, props = {}, children = [])
      @type = type
      @props = props
      @children = children
    end

    def to_s
      if @children.is_a?(Array)
        "#{@type}, #{@props}, #{@children.map{|c| c.to_s}}"
      else
        "#{@type}, #{@props}, #{@children}"
      end
    end
  end
end
