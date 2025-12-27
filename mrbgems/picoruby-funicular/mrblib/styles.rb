module Funicular
  class StyleValue
    attr_reader :value

    def initialize(value)
      @value = value.to_s
    end

    def |(other)
      case other
      when StyleValue
        # @type var other: StyleValue
        StyleValue.new("#{@value} #{other.value}".strip)
      when String
        StyleValue.new("#{@value} #{other}".strip)
      when nil
        self
      else
        StyleValue.new("#{@value} #{other.to_s}".strip)
      end
    end

    def to_s
      @value
    end
  end

  class StyleAccessor
    def initialize(definitions)
      @definitions = definitions
    end

    def method_missing(name, *args)
      style = @definitions[name]
      return StyleValue.new("") unless style

      if args.empty?
        # No arguments: return base or value
        StyleValue.new(style[:base] || style[:value] || "")
      elsif args[0] == true || args[0] == false || (args[0].is_a?(JS::Object) && args[0].type == :boolean)
        # Boolean argument: base + active (if true)
        base = style[:base] || ""
        active_class = (args[0] == true || (args[0].is_a?(JS::Object) && args[0].true?)) ? (style[:active] || "") : ""
        StyleValue.new("#{base} #{active_class}".strip)
      elsif args[0].is_a?(Symbol)
        # Symbol argument: base + variants[symbol]
        base = style[:base] || ""
        variant_class = style[:variants] ? (style[:variants][args[0]] || "") : ""
        StyleValue.new("#{base} #{variant_class}".strip)
      else
        # Other types: just return base
        StyleValue.new(style[:base] || style[:value] || "")
      end
    end

    def respond_to_missing?(name, include_private = false)
      @definitions.key?(name) || super
    end
  end

  class StyleBuilder
    def initialize
      @definitions = {}
    end

    def method_missing(name, *args)
      if args.size == 1 && args[0].is_a?(String)
        # Simple style: name "class-string"
        @definitions[name] = { value: args[0] }
      elsif args.size == 1 && args[0].is_a?(Hash)
        # Complex style with base/active/variants
        @definitions[name] = args[0]
      else
        raise ArgumentError, "Invalid style definition for #{name}"
      end
    end

    def to_definitions
      @definitions
    end
  end
end
